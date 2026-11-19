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
  @file hw-stub.c
  The hardware layer (hw.h) for the host tests: a simulated UART, timer,
  LED, button and EEPROM.
*/

#include "hw.h"
#include "hw-stub.h"

#include <string.h>

static uint8_t s_rx[512];
static int8_t s_rxm[512];
static uint16_t s_rx_head, s_rx_tail;
static uint8_t s_tx[512];
static uint16_t s_tx_len;
static uint16_t s_tx_frames;
static uint16_t s_millis;
static uint8_t s_led, s_status, s_button;
static uint8_t s_eeprom[256];
static uint8_t s_eeprom_busy_ms;

void
hw_init(void)
{
}

void
stub_rx_push(const uint8_t *data, uint8_t len, int8_t first_mark)
{
  for (uint8_t i = 0; i < len; i++) {
    s_rx[s_rx_head]  = data[i];
    s_rxm[s_rx_head] = (first_mark < 0) ? -1 : ((0 == i) ? first_mark : 0);
    s_rx_head        = (uint16_t) ((s_rx_head + 1u) % sizeof(s_rx));
  }
}

uint8_t
stub_tx_take(uint8_t *out, uint8_t cap)
{
  uint8_t n = (uint8_t) ((s_tx_len < cap) ? s_tx_len : cap);
  memcpy(out, s_tx, n);
  s_tx_len = 0;
  return n;
}

uint16_t
stub_tx_frames(void)
{
  return s_tx_frames;
}

void
stub_advance_ms(uint16_t ms)
{
  while (ms--) {
    s_millis++;
    if (s_eeprom_busy_ms) {
      s_eeprom_busy_ms--;
    }
    hw_tick_1ms();
  }
}

void
stub_set_button(uint8_t pressed)
{
  s_button = pressed;
}

uint8_t
stub_led(void)
{
  return s_led;
}

uint8_t
stub_eeprom(uint8_t addr)
{
  return s_eeprom[addr];
}

void
stub_eeprom_erase(void)
{
  memset(s_eeprom, 0xFF, sizeof(s_eeprom));
}

uint16_t
hw_millis(void)
{
  return s_millis;
}

void
hw_irq_disable(void)
{
}

void
hw_irq_enable(void)
{
}

void
hw_watchdog(void)
{
}

void
hw_reset(void)
{
}

void
hw_led(uint8_t on)
{
  s_led = on ? 1 : 0;
}

void
hw_status_led(uint8_t on)
{
  s_status = on ? 1 : 0;
}

void
hw_status_led_toggle(void)
{
  s_status ^= 1;
}

uint8_t
hw_button(void)
{
  return s_button;
}

uint8_t
hw_rs485_read(uint8_t *c, int8_t *mark)
{
  if (s_rx_tail == s_rx_head) {
    return 0;
  }
  *c        = s_rx[s_rx_tail];
  *mark     = s_rxm[s_rx_tail];
  s_rx_tail = (uint16_t) ((s_rx_tail + 1u) % sizeof(s_rx));
  return 1;
}

void
hw_rs485_send(const uint8_t *data, uint8_t len)
{
  memcpy(s_tx + s_tx_len, data, len);
  s_tx_len = (uint16_t) (s_tx_len + len);
  s_tx_frames++;
}

uint16_t
hw_rs485_overruns(void)
{
  return 0;
}

uint8_t
hw_eeprom_read(uint8_t addr)
{
  return s_eeprom[addr];
}

uint8_t
hw_eeprom_busy(void)
{
  return s_eeprom_busy_ms ? 1 : 0;
}

void
hw_eeprom_write_start(uint8_t addr, uint8_t value)
{
  s_eeprom[addr]   = value;
  s_eeprom_busy_ms = 4;
}
