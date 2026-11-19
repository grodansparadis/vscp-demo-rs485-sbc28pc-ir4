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
  @file persist.h
  Persistent settings in the data EEPROM of the PIC18F25K22.

  All persistent values are kept in a RAM copy (g_persist). A while after
  the last change the bytes that differ from the EEPROM are written, one
  at a time in the background, so the main loop never waits for the 4 ms
  an EEPROM write takes and polls are always answered in time.

  An erased EEPROM reads 0xFF. Then the VSCP control bytes are not
  0x55/0xAA and vscp-firmware.c calls vscp_init_pstorage(), which sets the
  defaults.
*/

#ifndef PERSIST_H
#define PERSIST_H

#include <stdint.h>

#include "blinky.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Time from the last change until the RAM copy is written (ms)
#define PERSIST_COMMIT_DELAY_MS 1000u

/// First EEPROM address used
#define PERSIST_EEPROM_BASE 0x00u

/// The persistent data (stored byte by byte from PERSIST_EEPROM_BASE)
typedef struct {
  uint8_t ctrl[2];            // VSCP control bytes (0x55/0xAA when initialized)
  uint8_t nickname;           // RS-485 address / nickname (1-254)
  uint8_t zone;               // Register 0
  uint8_t subzone;            // Register 1
  uint8_t control;            // Register 3
  uint8_t blink_interval_msb; // Register 4
  uint8_t blink_interval_lsb; // Register 5
  uint8_t button_byte0;       // Register 10
  uint8_t button_zone;        // Register 11
  uint8_t button_subzone;     // Register 12
  uint8_t userid[5];          // Standard registers 0x84-0x88
  uint8_t dm[BLINKY_DM_SIZE]; // Decision matrix, registers 0x60-0x7F
} persist_t;

/// The RAM copy. Change it, then call persist_changed().
extern persist_t g_persist;

/// Read the EEPROM into g_persist
void
persist_load(void);

/// Tell that g_persist has changed and needs to be saved
void
persist_changed(void);

/// Write changed bytes in the background. Call from the main loop.
void
persist_work(void);

/// 1 while there are changes that are not yet in the EEPROM
uint8_t
persist_pending(void);

#ifdef __cplusplus
}
#endif

#endif // PERSIST_H
