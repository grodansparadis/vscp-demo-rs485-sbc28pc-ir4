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
  @file persist.c
  Persistent settings in the data EEPROM. See persist.h.
*/

#include "persist.h"

#include "hw.h"

persist_t g_persist;

static uint8_t s_dirty;
static uint8_t s_cursor; // Next byte to compare
static uint16_t s_changed_ms;

void
persist_load(void)
{
  uint8_t *p = (uint8_t *) &g_persist;
  for (uint8_t i = 0; i < sizeof(g_persist); i++) {
    p[i] = hw_eeprom_read((uint8_t) (PERSIST_EEPROM_BASE + i));
  }
  s_dirty = 0;
}

void
persist_changed(void)
{
  s_dirty      = 1;
  s_cursor     = 0;
  s_changed_ms = hw_millis();
}

uint8_t
persist_pending(void)
{
  return s_dirty;
}

void
persist_work(void)
{
  if (!s_dirty || hw_eeprom_busy()) {
    return;
  }
  if ((uint16_t) (hw_millis() - s_changed_ms) < PERSIST_COMMIT_DELAY_MS) {
    return; // Wait for more changes (a burst of register writes)
  }

  // Write the next byte that differs; one per call
  const uint8_t *p = (const uint8_t *) &g_persist;
  while (s_cursor < sizeof(g_persist)) {
    const uint8_t addr  = (uint8_t) (PERSIST_EEPROM_BASE + s_cursor);
    const uint8_t value = p[s_cursor];
    s_cursor++;
    if (hw_eeprom_read(addr) != value) {
      hw_eeprom_write_start(addr, value);
      return;
    }
  }
  s_dirty = 0; // Everything is in the EEPROM
}
