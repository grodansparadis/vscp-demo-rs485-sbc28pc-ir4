/*
  This file is part of the VSCP Blinky demo for the Modtronix SBC28PC-IR4
  (VSCP Level I over RS-485).

  The MIT License (MIT)
  Copyright (C) 2000-2026 Ake Hedman, the VSCP project <info@vscp.org>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

/*!
  @file blinky.c
  The VSCP blinky demo for a VSCP Level I node on RS-485.

  This file holds the node logic and all the callbacks vscp-firmware.c
  needs, except the two transport callbacks which are in rs485-node.c. It
  is the same blinky as on the other boards (same MDF, registers, decision
  matrix and events); the differences are the hardware and the nickname:

  On RS-485 the nickname is the node's bus address. It is set when the
  firmware is built (NODE_ADDRESS), stored in EEPROM and can be changed
  over the bus with SET_NICKNAME, but the node never searches for a
  nickname: the master finds the node by polling its address.

  What the node does
  ------------------
  - Blinks the LED with the interval in registers 4/5 when enabled.
  - Sends events when the button is pressed and released.
  - Runs a millisecond counter that can be read in registers 6-9.
  - Has a decision matrix with four rows that can control the LED.
  - Sends NEW_NODE_ONLINE at start and a heartbeat every minute.
  - A long press on the button (two seconds) sends NEW_NODE_ONLINE again,
    which shows the node's address in tools such as VSCP Works.
*/

#include "blinky.h"

#include "config.h"
#include "hw.h"
#include "persist.h"
#include "rs485-node.h"

#include "vscp-compiler.h"
#include "vscp-projdefs.h"

#include <string.h>
#include <vscp-class.h>
#include <vscp-firmware.h>
#include <vscp-type.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

// The MDF for the node is fetched from this URL (without "http://").
// Max 32 characters.
static const char s_mdf_url[] = "eurosource.se/blinky0.json";

// GUID prefix (15 bytes); the last byte is the RS-485 address
static const uint8_t s_guid_prefix[15] = { NODE_GUID_PREFIX };

// Status register (register 2). Not persistent.
static uint8_t s_status;

// Millisecond counter (registers 6-9). Not persistent.
static volatile uint32_t s_ms_counter;

// Milliseconds since the LED was last toggled by the blink function
static volatile uint16_t s_blink_cnt;

// Counts milliseconds up to one second
static volatile uint16_t s_ms_in_second;

// Seconds that have passed and not yet been handled by the main loop
static volatile uint8_t s_seconds_pending;

// Button handling. The button is sampled every millisecond.
#define BTN_EVENT_PRESSED  0x01u
#define BTN_EVENT_RELEASED 0x02u

static volatile uint8_t s_btn_events; // BTN_EVENT_xxx waiting for the main loop
static uint8_t s_btn_raw;             // Last sampled state (ISR only)
static uint8_t s_btn_stable;          // Debounced state (ISR only)
static uint8_t s_btn_debounce;        // Debounce counter (ISR only)
static uint8_t s_long_press;          // Set while a long press is in progress

///////////////////////////////////////////////////////////////////////////////
// valid_address
//

static uint8_t
valid_address(uint8_t address)
{
  return (address >= 1u) && (address <= 254u);
}

///////////////////////////////////////////////////////////////////////////////
// send_event
//
// Send an event with zone/subzone data from this node.
//

static void
send_event(uint16_t vscp_class, uint8_t vscp_type, uint8_t byte0, uint8_t zone, uint8_t subzone)
{
  vscp_omsg.flags      = VSCP_VALID_MSG + 3;
  vscp_omsg.priority   = VSCP_PRIORITY_NORMAL;
  vscp_omsg.vscp_class = vscp_class;
  vscp_omsg.vscp_type  = vscp_type;
  vscp_omsg.data[0]    = byte0;
  vscp_omsg.data[1]    = zone;
  vscp_omsg.data[2]    = subzone;
  vscp_sendEvent();
}

///////////////////////////////////////////////////////////////////////////////
// led_set
//
// Turn the LED on or off. With bEvent set a CLASS1.INFORMATION ON or OFF
// event is sent if the state changed. The periodic blink does not send
// events, it would flood the bus.
//

static void
led_set(uint8_t bOn, uint8_t bEvent)
{
  uint8_t bWasOn = (s_status & BLINKY_STATUS_LED_ON) ? 1 : 0;

  hw_led(bOn);

  if (bOn) {
    s_status |= BLINKY_STATUS_LED_ON;
  }
  else {
    s_status &= (uint8_t) ~BLINKY_STATUS_LED_ON;
  }

  if (bEvent && (bWasOn != (bOn ? 1 : 0)) && (VSCP_STATE_ACTIVE == vscp_node_state)) {
    send_event(VSCP_CLASS1_INFORMATION,
               bOn ? VSCP_TYPE_INFORMATION_ON : VSCP_TYPE_INFORMATION_OFF,
               0,
               g_persist.zone,
               g_persist.subzone);
  }
}

static void
led_toggle(uint8_t bEvent)
{
  led_set((s_status & BLINKY_STATUS_LED_ON) ? 0 : 1, bEvent);
}

static uint16_t
get_blink_interval(void)
{
  return (uint16_t) (((uint16_t) g_persist.blink_interval_msb << 8) | g_persist.blink_interval_lsb);
}

///////////////////////////////////////////////////////////////////////////////
// set_defaults
//
// Set all application registers to the defaults in the MDF. The address
// (nickname) is not touched.
//

static void
set_defaults(void)
{
  g_persist.zone               = BLINKY_DEFAULT_ZONE;
  g_persist.subzone            = BLINKY_DEFAULT_SUBZONE;
  g_persist.control            = BLINKY_DEFAULT_CONTROL;
  g_persist.blink_interval_msb = (uint8_t) (BLINKY_DEFAULT_BLINK_INTERVAL >> 8);
  g_persist.blink_interval_lsb = (uint8_t) (BLINKY_DEFAULT_BLINK_INTERVAL & 0xffu);
  g_persist.button_byte0       = BLINKY_DEFAULT_BUTTON_BYTE0;
  g_persist.button_zone        = BLINKY_DEFAULT_BUTTON_ZONE;
  g_persist.button_subzone     = BLINKY_DEFAULT_BUTTON_SUBZONE;
  memset(g_persist.dm, 0, sizeof(g_persist.dm));
  persist_changed();
}

///////////////////////////////////////////////////////////////////////////////
// do_action
//
// Execute a decision matrix action.
//

static void
do_action(uint8_t action, uint8_t param)
{
  switch (action) {

    case BLINKY_ACTION_LED_STATE:
      if (BLINKY_ARG_LED_OFF == param) {
        led_set(0, TRUE);
      }
      else if (BLINKY_ARG_LED_ON == param) {
        led_set(1, TRUE);
      }
      else if (BLINKY_ARG_LED_TOGGLE == param) {
        led_toggle(TRUE);
      }
      break;

    case BLINKY_ACTION_LED_ON:
      led_set(1, TRUE);
      break;

    case BLINKY_ACTION_LED_OFF:
      led_set(0, TRUE);
      break;

    case BLINKY_ACTION_LED_TOGGLE:
      led_toggle(TRUE);
      break;

    case BLINKY_ACTION_START_BLINK:
      if (!(g_persist.control & BLINKY_CTRL_ENABLE_LED)) {
        g_persist.control |= BLINKY_CTRL_ENABLE_LED;
        s_blink_cnt = 0;
        persist_changed();
      }
      break;

    case BLINKY_ACTION_STOP_BLINK:
      if (g_persist.control & BLINKY_CTRL_ENABLE_LED) {
        g_persist.control &= (uint8_t) ~BLINKY_CTRL_ENABLE_LED;
        persist_changed();
      }
      break;

    case BLINKY_ACTION_CLR_COUNTER:
      hw_irq_disable();
      s_ms_counter = 0;
      hw_irq_enable();
      break;

    case BLINKY_ACTION_NOOP:
    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// do_dm
//
// Feed the incoming event (vscp_imsg) through the decision matrix.
//

static void
do_dm(void)
{
  // Copy the event. An action may send an event, and that must not
  // disturb the matching of the rows that follow.
  const uint16_t vscp_class = vscp_imsg.vscp_class;
  const uint8_t vscp_type   = vscp_imsg.vscp_type;
  const uint8_t oaddr       = vscp_imsg.oaddr;
  const uint8_t size        = vscp_imsg.flags & 0x0f;
  const uint8_t zone        = vscp_imsg.data[1];
  const uint8_t subzone     = vscp_imsg.data[2];

  for (uint8_t row = 0; row < BLINKY_DM_ROWS; row++) {

    const uint8_t *pdm = &g_persist.dm[row * BLINKY_DM_ROW_SIZE];
    uint8_t flags      = pdm[VSCP_DM_POS_FLAGS];

    if (!(flags & VSCP_DM_FLAG_ENABLED)) {
      continue;
    }

    if ((flags & VSCP_DM_FLAG_CHECK_OADDR) && (oaddr != pdm[VSCP_DM_POS_OADDR])) {
      continue;
    }

    // Zone 255 and subzone 255 mean "all"
    if (flags & VSCP_DM_FLAG_CHECK_ZONE) {
      if ((size < 2) || ((255 != zone) && (zone != g_persist.zone))) {
        continue;
      }
    }

    if (flags & VSCP_DM_FLAG_CHECK_SUBZONE) {
      if ((size < 3) || ((255 != subzone) && (subzone != g_persist.subzone))) {
        continue;
      }
    }

    // The class is nine bits. Bit eight of mask and filter is in the flags.
    uint16_t class_mask = (uint16_t) (((flags & VSCP_DM_FLAG_CLASS_MASK) ? 0x100u : 0u) | pdm[VSCP_DM_POS_CLASSMASK]);
    uint16_t class_filter =
      (uint16_t) (((flags & VSCP_DM_FLAG_CLASS_FILTER) ? 0x100u : 0u) | pdm[VSCP_DM_POS_CLASSFILTER]);

    if ((class_filter ^ vscp_class) & class_mask) {
      continue;
    }

    if ((pdm[VSCP_DM_POS_TYPEFILTER] ^ vscp_type) & pdm[VSCP_DM_POS_TYPEMASK]) {
      continue;
    }

    do_action(pdm[VSCP_DM_POS_ACTION], pdm[VSCP_DM_POS_ACTIONPARAM]);
  }
}

///////////////////////////////////////////////////////////////////////////////
// do_status_led
//
// The status LED (optional, RB1) is on while the node is active and blinks
// fast for a moment every time the node answers a poll, so it shows that
// the master is talking to the node.
//

static void
do_status_led(void)
{
  static uint16_t s_polls;
  const uint16_t polls = rs485_node_stats()->polls;

  if (VSCP_STATE_ACTIVE != vscp_node_state) {
    hw_status_led(0);
    return;
  }
  if (polls != s_polls) {
    s_polls        = polls;
    vscp_statuscnt = 0;
    hw_status_led(0); // A short blink for every poll
  }
  else if (vscp_statuscnt > 20) {
    hw_status_led(1);
  }
}

///////////////////////////////////////////////////////////////////////////////
// do_button
//
// Handle button presses and releases reported by the millisecond tick.
//

static void
do_button(void)
{
  uint8_t events;

  hw_irq_disable();
  events       = s_btn_events;
  s_btn_events = 0;
  hw_irq_enable();

  if (events & BTN_EVENT_PRESSED) {

    s_status |= BLINKY_STATUS_BTN_ON;

    if ((VSCP_STATE_ACTIVE == vscp_node_state) && (g_persist.control & BLINKY_CTRL_ENABLE_BTN)) {

      if (g_persist.control & BLINKY_CTRL_ENABLE_BTN_TURNON) {
        send_event(VSCP_CLASS1_CONTROL,
                   VSCP_TYPE_CONTROL_TURNON,
                   g_persist.button_byte0,
                   g_persist.button_zone,
                   g_persist.button_subzone);
      }

      if (g_persist.control & BLINKY_CTRL_ENABLE_BTN_START) {
        // CLASS1.INFORMATION START reserves data[0]; zone and subzone are data[1:2].
        send_event(VSCP_CLASS1_INFORMATION,
                   VSCP_TYPE_INFORMATION_START,
                   0,
                   g_persist.button_zone,
                   g_persist.button_subzone);
      }
    }
  }

  if (events & BTN_EVENT_RELEASED) {

    s_status &= (uint8_t) ~BLINKY_STATUS_BTN_ON;

    if (s_long_press) {
      // The release that ends a long press is not reported
      s_long_press = 0;
    }
    else if ((VSCP_STATE_ACTIVE == vscp_node_state) && (g_persist.control & BLINKY_CTRL_ENABLE_BTN) &&
             (g_persist.control & BLINKY_CTRL_ENABLE_BTN_STOP)) {
      // CLASS1.INFORMATION STOP reserves data[0]; zone and subzone are data[1:2].
      send_event(VSCP_CLASS1_INFORMATION,
                 VSCP_TYPE_INFORMATION_STOP,
                 0,
                 g_persist.button_zone,
                 g_persist.button_subzone);
    }
  }

  // Long press: announce the node again (the address does not change)
  if (!s_long_press && (vscp_initbtncnt > BLINKY_INIT_BUTTON_HOLD_MS)) {
    s_long_press = 1;
    if (VSCP_STATE_ACTIVE == vscp_node_state) {
      vscp_goActiveState(); // Sends NEW_NODE_ONLINE
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// blinky_init
//

void
blinky_init(void)
{
  s_status = 0;
  led_set(0, FALSE);

  persist_load();
  rs485_node_init();

  // Initializes the storage with defaults on first start
  vscp_check_pstorage();
  vscp_init();
}

///////////////////////////////////////////////////////////////////////////////
// blinky_work
//

void
blinky_work(void)
{
  rs485_node_work(); // First: answer a poll as soon as possible
  persist_work();
  do_button();
  do_status_led();

  // The nickname is the RS-485 address and is never searched for. If a
  // DROP_NICKNAME or an invalid SET_NICKNAME cleared it, use the stored one.
  if (!valid_address(vscp_nickname)) {
    vscp_nickname = g_persist.nickname;
  }

  // Fetch an incoming event if there is one
  vscp_getEvent();

  switch (vscp_node_state) {

    case VSCP_STATE_STARTUP:
    case VSCP_STATE_INIT:
    case VSCP_STATE_PREACTIVE:
      // No nickname discovery on RS-485: go active with the address
      vscp_node_state = VSCP_STATE_ACTIVE;
      vscp_goActiveState();
      vscp_imsg.flags = 0;
      break;

    case VSCP_STATE_ACTIVE:
      if (vscp_imsg.flags & VSCP_VALID_MSG) {
        if (VSCP_CLASS1_PROTOCOL == vscp_imsg.vscp_class) {
          vscp_handleProtocolEvent();
        }
        do_dm();
        vscp_imsg.flags = 0;
      }
      break;

    case VSCP_STATE_ERROR:
    default:
      vscp_error();
      vscp_imsg.flags = 0;
      break;
  }

  // Once a second
  while (s_seconds_pending) {
    hw_irq_disable();
    s_seconds_pending--;
    hw_irq_enable();
    vscp_doOneSecondWork();
  }

  // Blink
  if (VSCP_STATE_ACTIVE == vscp_node_state) {
    uint16_t interval = get_blink_interval();
    uint16_t cnt;
    hw_irq_disable();
    cnt = s_blink_cnt;
    hw_irq_enable();
    if ((g_persist.control & BLINKY_CTRL_ENABLE_LED) && interval && (cnt >= interval)) {
      hw_irq_disable();
      s_blink_cnt = 0;
      hw_irq_enable();
      led_toggle(FALSE);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// blinky_tick_1ms
//
// Called from the timer interrupt.
//

void
blinky_tick_1ms(void)
{
  // Timers used by vscp-firmware.c
  vscp_timer++;
  vscp_configtimer++;
  vscp_statuscnt++;

  if (g_persist.control & BLINKY_CTRL_ENABLE_MS_COUNTER) {
    s_ms_counter++;
  }

  if (s_blink_cnt < 0xffffu) {
    s_blink_cnt++;
  }

  if (++s_ms_in_second >= 1000u) {
    s_ms_in_second = 0;
    if (s_seconds_pending < 0xffu) {
      s_seconds_pending++;
    }
  }

  // Debounce the button
  uint8_t raw = hw_button();

  if (raw != s_btn_raw) {
    s_btn_raw      = raw;
    s_btn_debounce = 0;
  }
  else if (s_btn_debounce < BLINKY_DEBOUNCE_MS) {
    s_btn_debounce++;
  }
  else if (raw != s_btn_stable) {
    s_btn_stable = raw;
    s_btn_events |= raw ? BTN_EVENT_PRESSED : BTN_EVENT_RELEASED;
  }

  // vscp_initbtncnt is the time the button has been held
  if (s_btn_stable) {
    if (vscp_initbtncnt < 0xffffu) {
      vscp_initbtncnt++;
    }
  }
  else {
    vscp_initbtncnt = 0;
  }
}

// ---------------------------------------------------------------------------
//                     Callbacks for vscp-firmware.c
// ---------------------------------------------------------------------------

uint8_t
vscp_getMajorVersion(void)
{
  return THIS_FIRMWARE_MAJOR_VERSION;
}

uint8_t
vscp_getMinorVersion(void)
{
  return THIS_FIRMWARE_MINOR_VERSION;
}

uint8_t
vscp_getSubMinorVersion(void)
{
  return THIS_FIRMWARE_RELEASE_VERSION;
}

///////////////////////////////////////////////////////////////////////////////
// GUID. Index 0 is the most significant byte (register 0xD0). The last
// byte is the RS-485 address, so every node on the segment has its own.
//

uint8_t
vscp_getGUID(uint8_t idx)
{
  if (idx < 15) {
    return s_guid_prefix[idx];
  }
  return (15 == idx) ? vscp_nickname : 0;
}

void
vscp_setGUID(uint8_t idx, uint8_t data)
{
  // The GUID is set when the firmware is built
  (void) idx;
  (void) data;
}

///////////////////////////////////////////////////////////////////////////////
// User id (standard registers 0x84-0x88)
//

uint8_t
vscp_getUserID(uint8_t idx)
{
  return (idx < sizeof(g_persist.userid)) ? g_persist.userid[idx] : 0;
}

void
vscp_setUserID(uint8_t idx, uint8_t data)
{
  if ((idx < sizeof(g_persist.userid)) && (g_persist.userid[idx] != data)) {
    g_persist.userid[idx] = data;
    persist_changed();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Manufacturer id (standard registers 0x89-0x90). Not used by this demo.
//

uint8_t
vscp_getManufacturerId(uint8_t idx)
{
  (void) idx;
  return 0;
}

void
vscp_setManufacturerId(uint8_t idx, uint8_t data)
{
  (void) idx;
  (void) data;
}

///////////////////////////////////////////////////////////////////////////////
// Misc. standard register content
//

uint8_t
vscp_getBootLoaderAlgorithm(void)
{
  return VSCP_BOOTLOADER_NONE;
}

uint8_t
vscp_getBufferSize(void)
{
  return 8; // An RS-485 event frame holds eight data bytes
}

uint8_t
vscp_getRegisterPagesUsed(void)
{
  return 1;
}

uint8_t
vscp_getMDF_URL(uint8_t idx)
{
  return (idx < (sizeof(s_mdf_url) - 1)) ? (uint8_t) s_mdf_url[idx] : 0;
}

uint32_t
vscp_getFamilyCode(void)
{
  return 0;
}

uint32_t
vscp_getFamilyType(void)
{
  return 0;
}

uint16_t
vscp_getFirmwareCode(void)
{
  return 0;
}

uint8_t
vscp_getZone(void)
{
  return g_persist.zone;
}

uint8_t
vscp_getSubzone(void)
{
  return g_persist.subzone;
}

///////////////////////////////////////////////////////////////////////////////
// Nickname = RS-485 address. Only valid addresses (1-254) are stored.
//

uint8_t
vscp_readNicknamePermanent(void)
{
  return valid_address(g_persist.nickname) ? g_persist.nickname : NODE_ADDRESS;
}

void
vscp_writeNicknamePermanent(uint8_t nickname)
{
  if (valid_address(nickname) && (g_persist.nickname != nickname)) {
    g_persist.nickname = nickname;
    persist_changed();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Control bytes. They tell if the persistent storage has been initialized.
//

uint8_t
vscp_getControlByte(uint8_t idx)
{
  return (idx < sizeof(g_persist.ctrl)) ? g_persist.ctrl[idx] : 0;
}

void
vscp_setControlByte(uint8_t idx, uint8_t ctrl)
{
  if ((idx < sizeof(g_persist.ctrl)) && (g_persist.ctrl[idx] != ctrl)) {
    g_persist.ctrl[idx] = ctrl;
    persist_changed();
  }
}

///////////////////////////////////////////////////////////////////////////////
// vscp_init_pstorage
//
// Called when the storage is used for the first time.
//

void
vscp_init_pstorage(void)
{
  memset(g_persist.userid, 0, sizeof(g_persist.userid));
  g_persist.nickname = NODE_ADDRESS;
  set_defaults();
}

///////////////////////////////////////////////////////////////////////////////
// vscp_restoreDefaults
//
// Called when 0x55 and then 0xAA is written to register 0xA2 within one
// second. The address is kept.
//

void
vscp_restoreDefaults(void)
{
  set_defaults();
}

///////////////////////////////////////////////////////////////////////////////
// Page select
//

uint8_t
vscp_getPageSelect(uint8_t idx)
{
  return (0 == idx) ? (uint8_t) (vscp_page_select >> 8) : (uint8_t) (vscp_page_select & 0xff);
}

void
vscp_setPageSelect(uint8_t idx, uint8_t data)
{
  if (0 == idx) {
    vscp_page_select = (uint16_t) ((vscp_page_select & 0x00ffu) | ((uint16_t) data << 8));
  }
  else {
    vscp_page_select = (uint16_t) ((vscp_page_select & 0xff00u) | data);
  }
}

///////////////////////////////////////////////////////////////////////////////
// vscp_readAppReg
//
// Read an application register (0x00-0x7F). All registers are on page 0.
//

uint8_t
vscp_readAppReg(uint8_t reg)
{
  uint32_t counter;

  if (0 != vscp_page_select) {
    return 0;
  }

  if ((reg >= BLINKY_REG_DM_START) && (reg < (BLINKY_REG_DM_START + BLINKY_DM_SIZE))) {
    return g_persist.dm[reg - BLINKY_REG_DM_START];
  }

  switch (reg) {

    case BLINKY_REG_ZONE:
      return g_persist.zone;

    case BLINKY_REG_SUBZONE:
      return g_persist.subzone;

    case BLINKY_REG_STATUS:
      return s_status;

    case BLINKY_REG_CONTROL:
      return g_persist.control;

    case BLINKY_REG_BLINK_INTERVAL_MSB:
      return g_persist.blink_interval_msb;

    case BLINKY_REG_BLINK_INTERVAL_LSB:
      return g_persist.blink_interval_lsb;

    case BLINKY_REG_COUNTER_0:
    case BLINKY_REG_COUNTER_1:
    case BLINKY_REG_COUNTER_2:
    case BLINKY_REG_COUNTER_3:
      hw_irq_disable();
      counter = s_ms_counter;
      hw_irq_enable();
      return (uint8_t) (counter >> (8u * (uint8_t) (BLINKY_REG_COUNTER_3 - reg)));

    case BLINKY_REG_BUTTON_BYTE0:
      return g_persist.button_byte0;

    case BLINKY_REG_BUTTON_ZONE:
      return g_persist.button_zone;

    case BLINKY_REG_BUTTON_SUBZONE:
      return g_persist.button_subzone;

    default:
      return 0;
  }
}

///////////////////////////////////////////////////////////////////////////////
// vscp_writeAppReg
//
// Write an application register (0x00-0x7F). Returns the content of the
// register after the write, which the sender compares with what it wrote.
//

uint8_t
vscp_writeAppReg(uint8_t reg, uint8_t value)
{
  uint8_t *ppersistent = NULL;

  if (0 != vscp_page_select) {
    return (uint8_t) ~value;
  }

  if ((reg >= BLINKY_REG_DM_START) && (reg < (BLINKY_REG_DM_START + BLINKY_DM_SIZE))) {
    ppersistent = &g_persist.dm[reg - BLINKY_REG_DM_START];
  }
  else {
    switch (reg) {

      case BLINKY_REG_ZONE:
        ppersistent = &g_persist.zone;
        break;

      case BLINKY_REG_SUBZONE:
        ppersistent = &g_persist.subzone;
        break;

      case BLINKY_REG_STATUS:
        // Only the LED bit can be written. It turns the LED on or off.
        led_set((value & BLINKY_STATUS_LED_ON) ? 1 : 0, TRUE);
        return s_status;

      case BLINKY_REG_CONTROL:
        ppersistent = &g_persist.control;
        hw_irq_disable();
        s_blink_cnt = 0;
        hw_irq_enable();
        break;

      case BLINKY_REG_BLINK_INTERVAL_MSB:
        ppersistent = &g_persist.blink_interval_msb;
        break;

      case BLINKY_REG_BLINK_INTERVAL_LSB:
        ppersistent = &g_persist.blink_interval_lsb;
        break;

      case BLINKY_REG_COUNTER_0:
      case BLINKY_REG_COUNTER_1:
      case BLINKY_REG_COUNTER_2:
      case BLINKY_REG_COUNTER_3:
        // Any write clears the counter
        hw_irq_disable();
        s_ms_counter = 0;
        hw_irq_enable();
        return 0;

      case BLINKY_REG_BUTTON_BYTE0:
        ppersistent = &g_persist.button_byte0;
        break;

      case BLINKY_REG_BUTTON_ZONE:
        ppersistent = &g_persist.button_zone;
        break;

      case BLINKY_REG_BUTTON_SUBZONE:
        ppersistent = &g_persist.button_subzone;
        break;

      default:
        // Not a register of this node
        return (uint8_t) ~value;
    }
  }

  if (*ppersistent != value) {
    *ppersistent = value;
    persist_changed();
  }

  return *ppersistent;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_getMatrixInfo
//
// Fill in the response to CLASS1.PROTOCOL, "get decision matrix info".
//

void
vscp_getMatrixInfo(char *pData)
{
  pData[0] = BLINKY_DM_ROWS;      // Number of rows
  pData[1] = BLINKY_REG_DM_START; // Register offset
  pData[2] = 0;                   // Start page MSB
  pData[3] = 0;                   // Start page LSB
  pData[4] = 0;                   // End page MSB
  pData[5] = 0;                   // End page LSB
  pData[6] = 0;                   // Level II row size, not used
}

///////////////////////////////////////////////////////////////////////////////
// vscp_getEmbeddedMdfInfo
//
// The MDF is on a server (see s_mdf_url), not in the node.
//

void
vscp_getEmbeddedMdfInfo(void)
{
}

///////////////////////////////////////////////////////////////////////////////
// vscp_goBootloaderMode
//
// The demo has no bootloader; it is programmed over ICSP.
//

void
vscp_goBootloaderMode(uint8_t algorithm)
{
  (void) algorithm;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_hardreset / vscp_wait_ms / vscp_wait_s
//
// Used by vscp-firmware.c for some protocol events.
//

void
vscp_hardreset(void)
{
  hw_reset();
}

void
vscp_wait_ms(uint16_t ms)
{
  uint16_t start = hw_millis();
  while ((uint16_t) (hw_millis() - start) < ms) {
    hw_watchdog();
  }
}

void
vscp_wait_s(uint16_t sec)
{
  while (sec--) {
    vscp_wait_ms(1000);
  }
}
