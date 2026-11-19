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
  @file config.h
  Build options of the node. Every option can also be given on the make
  command line, for example

    make NODE_ADDRESS=5 CLOCK=XTAL16

  which passes -DNODE_ADDRESS=5 -DCLOCK_XTAL16 to the compiler (see the
  Makefile and docs/firmware.md).
*/

#ifndef CONFIG_H
#define CONFIG_H

/*
  RS-485 address of the node (1-254) the first time it starts. It is the
  node's VSCP nickname and is kept in EEPROM: it can later be changed over
  the bus with CLASS1.PROTOCOL SET_NICKNAME, and "restore defaults" does
  not change it. Every node on a segment needs its own address.
*/
#ifndef NODE_ADDRESS
#define NODE_ADDRESS 1
#endif

/*
  Baud rate of the RS-485 segment. The master (vscpl1drv-rs485 or
  vscpl2drv-rs485) and all nodes must use the same rate.
*/
#ifndef RS485_BAUDRATE
#define RS485_BAUDRATE 115200UL
#endif

/*
  1: frames start with an address character that has the ninth bit set, as
  the specification says (default). 0: plain 8N1, for a master configured
  without the ninth bit (vscpl1drv-rs485 flag bit 0, vscpl2drv-rs485
  "nine-bit": false).
*/
#ifndef RS485_NINE_BIT
#define RS485_NINE_BIT 1
#endif

/*
  1: EVENT frames always carry eight data bytes (15 characters). Must match
  the master (vscpl1drv-rs485 flag bit 1, vscpl2drv-rs485 "fixed-length").
*/
#ifndef RS485_FIXED_LENGTH
#define RS485_FIXED_LENGTH 0
#endif

/*
  1: CRC-8 with polynomial 0x07 instead of the DOW CRC. Must match the
  master (vscpl1drv-rs485 flag bit 2, vscpl2drv-rs485 "crc": "0x07").
*/
#ifndef RS485_CRC_ATM
#define RS485_CRC_ATM 0
#endif

/*
  The SP485E transceiver's DE/RE pins go to header J1 on the SBC28PC, from
  where solder jumper SJ3 (or J1 pins 1-2) connects them to RA0 and solder
  jumper SJ2 (or J1 pins 2-3) to RB2. By default the firmware drives both
  pins the same way, so it works whichever connection is made.

    RS485_DIR_PINS 0  RA0 and RB2 (default)
    RS485_DIR_PINS 1  RA0 only (RB2 is free)
    RS485_DIR_PINS 2  RB2 only (RA0 is free)
*/
#ifndef RS485_DIR_PINS
#define RS485_DIR_PINS 0
#endif

/*
  Clock. The SBC28PC comes without a crystal; the PIC18F25K22 then runs
  from its internal 16 MHz oscillator with the 4x PLL (64 MHz). The
  internal oscillator is accurate to about 2 %, enough for the UART at room
  temperature. For a node that must work over a wide temperature range,
  order the board with a crystal and build with:

    CLOCK_XTAL16     16 MHz crystal with the 4x PLL (64 MHz)
    CLOCK_XTAL18432  18.432 MHz crystal, no PLL (exact standard baud rates)
*/
#if defined(CLOCK_XTAL16)
#define FOSC_HZ 64000000UL
#elif defined(CLOCK_XTAL18432)
#define FOSC_HZ 18432000UL
#else
#ifndef CLOCK_INTOSC
#define CLOCK_INTOSC
#endif
#define FOSC_HZ 64000000UL
#endif

/*
  GUID of the node, most significant byte first, without the last byte,
  which is the RS-485 address. The default is in the range VSCP reserves
  for demos and examples (FF:FF:FF:FF:FF:FF:FF:F5), followed by "RS485" in
  ASCII: FF:FF:FF:FF:FF:FF:FF:F5:52:53:34:38:35:00:00:<address>. Give
  production nodes GUIDs from a range of your own.
*/
#ifndef NODE_GUID_PREFIX
#define NODE_GUID_PREFIX 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0x52, 0x53, 0x34, 0x38, 0x35, 0x00, 0x00
#endif

#endif // CONFIG_H
