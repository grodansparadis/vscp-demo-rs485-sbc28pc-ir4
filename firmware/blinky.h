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
  @file blinky.h
  The VSCP blinky demo for a VSCP Level I node on RS-485.

  Registers, actions and events follow the blinky MDF
  (https://eurosource.se/blinky0.json).
*/

#ifndef BLINKY_H
#define BLINKY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
//                          Application registers (page 0)
// ---------------------------------------------------------------------------

#define BLINKY_REG_ZONE               0  // Zone for the node [persistent]
#define BLINKY_REG_SUBZONE            1  // Subzone for the node [persistent]
#define BLINKY_REG_STATUS             2  // Status (LED and button state)
#define BLINKY_REG_CONTROL            3  // Control bits [persistent]
#define BLINKY_REG_BLINK_INTERVAL_MSB 4  // Blink interval in ms, MSB [persistent]
#define BLINKY_REG_BLINK_INTERVAL_LSB 5  // Blink interval in ms, LSB [persistent]
#define BLINKY_REG_COUNTER_0          6  // Millisecond counter MSB
#define BLINKY_REG_COUNTER_1          7  // Millisecond counter
#define BLINKY_REG_COUNTER_2          8  // Millisecond counter
#define BLINKY_REG_COUNTER_3          9  // Millisecond counter LSB
#define BLINKY_REG_BUTTON_BYTE0       10 // Data byte 0 of button events [persistent]
#define BLINKY_REG_BUTTON_ZONE        11 // Zone of button events [persistent]
#define BLINKY_REG_BUTTON_SUBZONE     12 // Subzone of button events [persistent]

#define BLINKY_REG_DM_START 0x60 // First register of the decision matrix
#define BLINKY_DM_ROWS      4    // Number of decision matrix rows
#define BLINKY_DM_ROW_SIZE  8    // Bytes in a Level I decision matrix row
#define BLINKY_DM_SIZE      (BLINKY_DM_ROWS * BLINKY_DM_ROW_SIZE)

// Status register bits
#define BLINKY_STATUS_LED_ON 0x02u // Set when the LED is on
#define BLINKY_STATUS_BTN_ON 0x01u // Set when the button is pressed

// Control register bits
#define BLINKY_CTRL_ENABLE_LED        0x80u // Blink the LED
#define BLINKY_CTRL_ENABLE_BTN        0x40u // Button sends events
#define BLINKY_CTRL_ENABLE_BTN_TURNON 0x08u // Send TURN-ON when the button is pressed
#define BLINKY_CTRL_ENABLE_BTN_START  0x04u // Send START when the button is pressed
#define BLINKY_CTRL_ENABLE_BTN_STOP   0x02u // Send STOP when the button is released
#define BLINKY_CTRL_ENABLE_MS_COUNTER 0x01u // Run the millisecond counter

// Register defaults (as in the MDF)
#define BLINKY_DEFAULT_ZONE           0u
#define BLINKY_DEFAULT_SUBZONE        0u
#define BLINKY_DEFAULT_CONTROL        0xCEu // 0b11001110
#define BLINKY_DEFAULT_BLINK_INTERVAL 500u  // 0x01F4 milliseconds
#define BLINKY_DEFAULT_BUTTON_BYTE0   1u
#define BLINKY_DEFAULT_BUTTON_ZONE    1u
#define BLINKY_DEFAULT_BUTTON_SUBZONE 1u

// ---------------------------------------------------------------------------
//                          Decision matrix actions
// ---------------------------------------------------------------------------

#define BLINKY_ACTION_NOOP        0 // No operation
#define BLINKY_ACTION_LED_STATE   1 // Set the LED. Parameter: 0=off, 1=on, 2=toggle
#define BLINKY_ACTION_LED_ON      2 // Turn the LED on
#define BLINKY_ACTION_LED_OFF     3 // Turn the LED off
#define BLINKY_ACTION_LED_TOGGLE  4 // Toggle the LED
#define BLINKY_ACTION_START_BLINK 5 // Start blinking
#define BLINKY_ACTION_STOP_BLINK  6 // Stop blinking
#define BLINKY_ACTION_CLR_COUNTER 7 // Clear the millisecond counter

#define BLINKY_ARG_LED_OFF    0
#define BLINKY_ARG_LED_ON     1
#define BLINKY_ARG_LED_TOGGLE 2

// ---------------------------------------------------------------------------
//                                  Timing
// ---------------------------------------------------------------------------

// Hold the button this long (ms) to drop the nickname and start a new
// nickname discovery.
#define BLINKY_INIT_BUTTON_HOLD_MS 2000u

// Button debounce time (ms)
#define BLINKY_DEBOUNCE_MS 20u

// ---------------------------------------------------------------------------
//                                    API
// ---------------------------------------------------------------------------

/*!
  Initialize storage, the RS-485 transport and the VSCP firmware.
  Call once after the peripherals are initialized.
*/
void
blinky_init(void);

/*!
  Do the work of the node. Call as often as possible from the main loop.
*/
void
blinky_work(void);

/*!
  Timekeeping. Call once every millisecond (from the timer interrupt).
*/
void
blinky_tick_1ms(void);

#ifdef __cplusplus
}
#endif

#endif // BLINKY_H
