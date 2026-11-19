/*
  This file is part of the VSCP Blinky demo for the Modtronix SBC28PC-IR4
  (VSCP Level I over RS-485).

  The MIT License (MIT)
  Copyright (C) 2000-2026 Ake Hedman, the VSCP project <info@vscp.org>
*/

/*!
  @file test_node.c
  Host tests of the node: the real firmware (blinky.c, rs485-node.c,
  persist.c, vscp-rs485.c and vscp-firmware.c) against the simulated
  hardware in hw-stub.c. The test plays the master: it polls the node,
  sends it events and checks the frames the node answers with.
*/

#include <stdio.h>
#include <string.h>

#include "blinky.h"
#include "config.h"
#include "hw-stub.h"
#include "hw.h"
#include "persist.h"
#include "rs485-node.h"
#include "vscp-rs485.h"

#include <vscp-class.h>
#include <vscp-firmware.h>
#include <vscp-type.h>

static int g_failed = 0;
static int g_checks = 0;

#define CHECK(cond)                                                                                                    \
  do {                                                                                                                 \
    g_checks++;                                                                                                        \
    if (!(cond)) {                                                                                                     \
      g_failed++;                                                                                                      \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                           \
    }                                                                                                                  \
  } while (0)

#if RS485_NINE_BIT
#define FIRST_MARK 1
#else
#define FIRST_MARK -1
#endif

static const vscp_rs485_options_t g_opt = { RS485_CRC_ATM ? VSCP_RS485_CRC_ATM : VSCP_RS485_CRC_DOW,
                                            RS485_FIXED_LENGTH };

void
hw_tick_1ms(void)
{
  blinky_tick_1ms();
}

static void
work(int n)
{
  while (n--) {
    blinky_work();
    stub_advance_ms(1);
  }
}

// Send a frame on the bus (from the master or another node)
static void
bus_send(uint8_t dest, uint8_t src, uint8_t op, uint16_t cls, uint8_t type, const uint8_t *data, uint8_t size)
{
  vscp_rs485_frame_t f;
  uint8_t buf[VSCP_RS485_MAX_FRAME];
  memset(&f, 0, sizeof(f));
  f.dest       = dest;
  f.src        = src;
  f.op         = op;
  f.vscp_class = cls;
  f.vscp_type  = type;
  f.size       = size;
  if (size) {
    memcpy(f.data, data, size);
  }
  uint8_t len = (uint8_t) vscp_rs485_encode(&g_opt, &f, buf);
  stub_rx_push(buf, len, FIRST_MARK);
  stub_advance_ms(2); // The pause after a frame
}

// Poll an address. Returns 1 and the answer if the node answered.
static int
poll(uint8_t address, vscp_rs485_frame_t *answer)
{
  uint8_t out[64];
  stub_tx_take(out, sizeof(out)); // Discard anything old
  bus_send(address, 0, VSCP_RS485_OP_POLL, 0, 0, NULL, 0);
  work(3);
  uint8_t n = stub_tx_take(out, sizeof(out));
  if (0 == n) {
    return 0;
  }
  vscp_rs485_rx_t rx;
  vscp_rs485_rx_init(&rx, &g_opt);
  vscp_rs485_rx_result_t r = VSCP_RS485_RX_PENDING;
  for (uint8_t i = 0; i < n; i++) {
    r = vscp_rs485_rx_char(&rx, out[i], (0 == i) ? FIRST_MARK : (FIRST_MARK < 0 ? -1 : 0), answer);
  }
  return (VSCP_RS485_RX_FRAME == r) ? 1 : -1;
}

// Poll until NO_EVENTS; return how many events were collected into evs
static int
drain(uint8_t address, vscp_rs485_frame_t *evs, int max)
{
  int n = 0;
  vscp_rs485_frame_t f;
  for (int i = 0; i < 20; i++) {
    if (1 != poll(address, &f) || (VSCP_RS485_OP_NO_EVENTS == f.op)) {
      break;
    }
    if (n < max) {
      evs[n] = f;
    }
    n++;
  }
  return n;
}

static int
find(const vscp_rs485_frame_t *evs, int n, uint16_t cls, uint8_t type)
{
  for (int i = 0; i < n; i++) {
    if ((evs[i].vscp_class == cls) && (evs[i].vscp_type == type)) {
      return i;
    }
  }
  return -1;
}

int
main(void)
{
  vscp_rs485_frame_t f;
  vscp_rs485_frame_t evs[16];
  int n, i;

  // --- First start with an erased EEPROM --------------------------------------
  stub_eeprom_erase();
  hw_init();
  blinky_init();
  work(5);
  CHECK(VSCP_STATE_ACTIVE == vscp_node_state);
  CHECK(NODE_ADDRESS == vscp_nickname);

  // The first poll gives NEW_NODE_ONLINE with the address
  CHECK(1 == poll(NODE_ADDRESS, &f));
  CHECK((VSCP_RS485_OP_EVENT == f.op) && (0 == f.dest) && (NODE_ADDRESS == f.src));
  CHECK((VSCP_CLASS1_PROTOCOL == f.vscp_class) && (VSCP_TYPE_PROTOCOL_NEW_NODE_ONLINE == f.vscp_type));
  CHECK((1 == f.size) && (NODE_ADDRESS == f.data[0]));

  // Then NO_EVENTS
  CHECK(1 == poll(NODE_ADDRESS, &f));
  CHECK((VSCP_RS485_OP_NO_EVENTS == f.op) && (0 == f.dest) && (NODE_ADDRESS == f.src));

  // Polls for other addresses are not answered
  CHECK(0 == poll(NODE_ADDRESS + 1, &f));

  // --- Register read: control register (3), default 0xCE ------------------------
  uint8_t rd[2] = { NODE_ADDRESS, 3 };
  bus_send(NODE_ADDRESS, 0, VSCP_RS485_OP_EVENT, VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_READ_REGISTER, rd, 2);
  work(3);
  CHECK(1 == poll(NODE_ADDRESS, &f));
  CHECK((VSCP_CLASS1_PROTOCOL == f.vscp_class) && (VSCP_TYPE_PROTOCOL_RW_RESPONSE == f.vscp_type));
  CHECK((2 == f.size) && (3 == f.data[0]) && (0xCE == f.data[1]));

  // GUID register 0xDF is the address
  uint8_t rg[2] = { NODE_ADDRESS, 0xDF };
  bus_send(NODE_ADDRESS, 0, VSCP_RS485_OP_EVENT, VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_READ_REGISTER, rg, 2);
  work(3);
  CHECK((1 == poll(NODE_ADDRESS, &f)) && (0xDF == f.data[0]) && (NODE_ADDRESS == f.data[1]));

  // A read for another node is ignored
  uint8_t ro[2] = { NODE_ADDRESS + 1, 3 };
  bus_send(NODE_ADDRESS + 1, 0, VSCP_RS485_OP_EVENT, VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_READ_REGISTER, ro, 2);
  work(3);
  CHECK((1 == poll(NODE_ADDRESS, &f)) && (VSCP_RS485_OP_NO_EVENTS == f.op));

  // --- Write the status register: LED on, INFORMATION ON is sent ---------------
  // First stop the blinking (control bit 7) so the LED stays as set
  uint8_t wc[3] = { NODE_ADDRESS, 3, 0x4E };
  bus_send(NODE_ADDRESS, 0, VSCP_RS485_OP_EVENT, VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_WRITE_REGISTER, wc, 3);
  work(3);
  n = drain(NODE_ADDRESS, evs, 16);
  CHECK((1 == n) && (VSCP_TYPE_PROTOCOL_RW_RESPONSE == evs[0].vscp_type) && (0x4E == evs[0].data[1]));

  uint8_t ws[3] = { NODE_ADDRESS, 2, BLINKY_STATUS_LED_ON };
  bus_send(NODE_ADDRESS, 0, VSCP_RS485_OP_EVENT, VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_WRITE_REGISTER, ws, 3);
  work(3);
  CHECK(1 == stub_led());
  n = drain(NODE_ADDRESS, evs, 16);
  CHECK(find(evs, n, VSCP_CLASS1_INFORMATION, VSCP_TYPE_INFORMATION_ON) >= 0);
  CHECK(find(evs, n, VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_RW_RESPONSE) >= 0);

  // --- Button: TURN-ON and START on press, STOP on release ---------------------
  stub_set_button(1);
  stub_advance_ms(40);
  work(3);
  stub_set_button(0);
  stub_advance_ms(40);
  work(3);
  n = drain(NODE_ADDRESS, evs, 16);
  CHECK(3 == n);
  i = find(evs, n, VSCP_CLASS1_CONTROL, VSCP_TYPE_CONTROL_TURNON);
  CHECK((i >= 0) && (3 == evs[i].size) && (1 == evs[i].data[0]) && (1 == evs[i].data[1]) && (1 == evs[i].data[2]));
  CHECK(find(evs, n, VSCP_CLASS1_INFORMATION, VSCP_TYPE_INFORMATION_START) >= 0);
  CHECK(find(evs, n, VSCP_CLASS1_INFORMATION, VSCP_TYPE_INFORMATION_STOP) >= 0);

  // --- Decision matrix: row 0 turns the LED off on CLASS1.CONTROL TURN-OFF ----
  const uint8_t row[8] = {
    0, 0x80, 0xFF, VSCP_CLASS1_CONTROL, 0xFF, VSCP_TYPE_CONTROL_TURNOFF, BLINKY_ACTION_LED_OFF, 0
  };
  for (uint8_t b = 0; b < 8; b++) {
    uint8_t wd[3] = { NODE_ADDRESS, (uint8_t) (BLINKY_REG_DM_START + b), row[b] };
    bus_send(NODE_ADDRESS, 0, VSCP_RS485_OP_EVENT, VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_WRITE_REGISTER, wd, 3);
    work(3);
    drain(NODE_ADDRESS, evs, 16);
  }
  // TURN-OFF from another node (address 7), sent to this node
  uint8_t off[3] = { 0, 0, 0 };
  bus_send(NODE_ADDRESS, 7, VSCP_RS485_OP_EVENT, VSCP_CLASS1_CONTROL, VSCP_TYPE_CONTROL_TURNOFF, off, 3);
  work(3);
  CHECK(0 == stub_led());
  n = drain(NODE_ADDRESS, evs, 16);
  CHECK(find(evs, n, VSCP_CLASS1_INFORMATION, VSCP_TYPE_INFORMATION_OFF) >= 0);

  // Broadcast address 0xFF is accepted too: turn on with a status write
  // through the decision matrix is not set up, so use TURN-OFF again (no change)
  bus_send(VSCP_RS485_ADDR_BROADCAST, 0, VSCP_RS485_OP_EVENT, VSCP_CLASS1_CONTROL, VSCP_TYPE_CONTROL_TURNOFF, off, 3);
  work(3);
  CHECK(rs485_node_stats()->received >= 13);

  // --- Damaged frames are ignored ---------------------------------------------------
  uint16_t crc_before = rs485_node_stats()->crc_errors;
  uint8_t bad[VSCP_RS485_MAX_FRAME];
  vscp_rs485_frame_t pf;
  memset(&pf, 0, sizeof(pf));
  pf.dest     = NODE_ADDRESS;
  pf.op       = VSCP_RS485_OP_POLL;
  uint8_t len = (uint8_t) vscp_rs485_encode(&g_opt, &pf, bad);
  bad[len - 1] ^= 0x01;
  stub_rx_push(bad, len, FIRST_MARK);
  stub_advance_ms(2);
  uint16_t frames_before = stub_tx_frames();
  work(3);
  CHECK(stub_tx_frames() == frames_before); // No answer to a damaged poll
  CHECK(rs485_node_stats()->crc_errors == crc_before + 1);

  // --- Persistence: the settings are in the EEPROM a second later ----------------
  stub_advance_ms(1100);
  work(200); // One byte per call while the EEPROM is not busy
  for (i = 0; i < 300; i++) {
    stub_advance_ms(5);
    blinky_work();
  }
  CHECK(0 == persist_pending());
  CHECK(0x55 == stub_eeprom(0));
  CHECK(0xAA == stub_eeprom(1));
  CHECK(NODE_ADDRESS == stub_eeprom(2));
  CHECK(0x4E == stub_eeprom(5));      // Control register
  CHECK(0x80 == stub_eeprom(16 + 1)); // DM row 0 flags

  // A restart reads them back
  vscp_nickname = 0;
  blinky_init();
  work(3);
  CHECK(NODE_ADDRESS == vscp_nickname);
  CHECK(0x4E == g_persist.control);
  CHECK(VSCP_CLASS1_CONTROL == g_persist.dm[3]);
  drain(NODE_ADDRESS, evs, 16);

  // --- Nickname = address: invalid values are refused, valid ones move the node -
  uint8_t sn_bad[2] = { NODE_ADDRESS, 0xFF };
  bus_send(NODE_ADDRESS, 0, VSCP_RS485_OP_EVENT, VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_SET_NICKNAME, sn_bad, 2);
  work(3);
  CHECK(NODE_ADDRESS == vscp_nickname);
  drain(NODE_ADDRESS, evs, 16);

  uint8_t drop[1] = { NODE_ADDRESS };
  bus_send(NODE_ADDRESS, 0, VSCP_RS485_OP_EVENT, VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_DROP_NICKNAME, drop, 1);
  work(5);
  CHECK(NODE_ADDRESS == vscp_nickname); // Restarted with the same address
  n = drain(NODE_ADDRESS, evs, 16);
  CHECK(find(evs, n, VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_NEW_NODE_ONLINE) >= 0);

  uint8_t sn[2] = { NODE_ADDRESS, 42 };
  bus_send(NODE_ADDRESS, 0, VSCP_RS485_OP_EVENT, VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_SET_NICKNAME, sn, 2);
  work(3);
  CHECK(42 == vscp_nickname);
  CHECK(0 == poll(NODE_ADDRESS, &f));
  CHECK((1 == poll(42, &f)) && (VSCP_TYPE_PROTOCOL_NICKNAME_ACCEPTED == f.vscp_type) && (42 == f.src));
  CHECK(42 == g_persist.nickname);

  // --- Restore defaults (0x55 then 0xAA to register 0xA2) keeps the address ------
  uint8_t r1[3] = { 42, 0xA2, 0x55 }, r2[3] = { 42, 0xA2, 0xAA };
  bus_send(42, 0, VSCP_RS485_OP_EVENT, VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_WRITE_REGISTER, r1, 3);
  work(3);
  bus_send(42, 0, VSCP_RS485_OP_EVENT, VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_WRITE_REGISTER, r2, 3);
  work(3);
  CHECK(BLINKY_DEFAULT_CONTROL == g_persist.control);
  CHECK(0 == g_persist.dm[1]);
  CHECK(42 == vscp_nickname);

  // --- Heartbeat once a minute -----------------------------------------------------
  drain(42, evs, 16);
  for (i = 0; i < 61; i++) {
    stub_advance_ms(1000);
    work(1);
  }
  n = drain(42, evs, 16);
  CHECK(find(evs, n, VSCP_CLASS1_INFORMATION, VSCP_TYPE_INFORMATION_NODE_HEARTBEAT) >= 0);

  printf("%d checks, %d failed\n", g_checks, g_failed);
  return g_failed ? 1 : 0;
}
