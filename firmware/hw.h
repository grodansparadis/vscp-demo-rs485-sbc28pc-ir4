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
  @file hw.h
  The hardware of the node. Everything that touches PIC registers is in
  hw-pic18.c; the rest of the firmware only uses these functions, so it
  can be tested on a PC with tests/hw-stub.c.

  Pins (PIC18F25K22 on the SBC28PC-IR4)

    RC6  RS-485 TX (SP485E DI)
    RC7  RS-485 RX (SP485E RO)
    RA0  RS-485 direction (SP485E DE/RE) through SJ3 or J1 1-2
    RB2  RS-485 direction (SP485E DE/RE) through SJ2 or J1 2-3
    RB4  Blinky LED, to ground through a 470 ohm resistor (on when high)
    RB1  Status LED, to ground through a 470 ohm resistor (optional)
    RB0  Button to ground (internal pull-up)
    RB6, RB7, /MCLR  ICSP
*/

#ifndef HW_H
#define HW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Set up the clock, pins, timer, UART, EEPROM and interrupts
void
hw_init(void);

// --- Timekeeping --------------------------------------------------------------

/// Called from the 1 ms timer interrupt (implemented in main.c)
void
hw_tick_1ms(void);

/// Milliseconds since start (wraps after 65 s)
uint16_t
hw_millis(void);

/// Disable/enable interrupts around access to data shared with them
void
hw_irq_disable(void);
void
hw_irq_enable(void);

/// Feed the watchdog
void
hw_watchdog(void);

// --- I/O ------------------------------------------------------------------------

void
hw_led(uint8_t on);
void
hw_status_led(uint8_t on);
void
hw_status_led_toggle(void);

/// 1 when the button is pressed
uint8_t
hw_button(void);

// --- RS-485 ---------------------------------------------------------------------

/*!
  Next received character.
  @param c The character
  @param mark 1/0 for a character with the ninth bit set/clear, -1 when the
         port runs without the ninth bit
  @return 1 if a character was read, 0 if none is waiting
*/
uint8_t
hw_rs485_read(uint8_t *c, int8_t *mark);

/*!
  Send a frame: switch the transceiver to sending, send data[0] with the
  ninth bit set and the rest with it clear, wait until the last stop bit
  has left and switch back to receiving. Blocks for the frame time (at most
  15 characters, 1.4 ms at 115200 baud).
*/
void
hw_rs485_send(const uint8_t *data, uint8_t len);

/// Characters lost because the receive buffer or the UART overflowed
uint16_t
hw_rs485_overruns(void);

// --- EEPROM ---------------------------------------------------------------------

uint8_t
hw_eeprom_read(uint8_t addr);

/// 1 while an EEPROM write is in progress
uint8_t
hw_eeprom_busy(void);

/*!
  Start writing one byte. Returns at once; the write takes about 4 ms
  (hw_eeprom_busy). Only call when hw_eeprom_busy() is 0.
*/
void
hw_eeprom_write_start(uint8_t addr, uint8_t value);

/// Reset the MCU
void
hw_reset(void);

#ifdef __cplusplus
}
#endif

#endif // HW_H
