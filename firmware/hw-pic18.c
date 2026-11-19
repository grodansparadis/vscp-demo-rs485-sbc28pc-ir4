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
  @file hw-pic18.c
  The hardware of the node: PIC18F25K22 on the Modtronix SBC28PC-IR4.
  See hw.h for the pins.

  The UART (EUSART1) runs in 9-bit mode: the ninth bit is the address
  marker of the RS-485 protocol. Received characters are put in a ring
  buffer by the interrupt together with their ninth bit; the main loop
  reads them with hw_rs485_read().
*/

#include <xc.h>

#include "config.h"
#include "hw.h"

// --- Configuration bits ---------------------------------------------------------

#if defined(CLOCK_INTOSC)
#pragma config FOSC   = INTIO67 // Internal oscillator, RA6/RA7 are I/O
#pragma config PLLCFG = OFF     // PLL switched on by software (OSCTUNE.PLLEN)
#elif defined(CLOCK_XTAL16)
#pragma config FOSC   = HSHP // 16 MHz crystal
#pragma config PLLCFG = ON   // 4x PLL: 64 MHz
#else
#pragma config FOSC   = HSHP // 18.432 MHz crystal
#pragma config PLLCFG = OFF
#endif
#pragma config PRICLKEN = ON
#pragma config FCMEN    = ON // Fall back to the internal oscillator if the crystal fails
#pragma config IESO     = OFF
#pragma config PWRTEN   = ON
#pragma config BOREN    = SBORDIS // Brown-out reset at 2.85 V
#pragma config BORV     = 285
#pragma config WDTEN    = SWON // Watchdog switched on by software
#pragma config WDTPS    = 256  // About 1 s
#pragma config CCP2MX   = PORTC1
#pragma config PBADEN   = OFF // PORTB digital after reset
#pragma config CCP3MX   = PORTB5
#pragma config HFOFST   = ON
#pragma config T3CMX    = PORTC0
#pragma config P2BMX    = PORTB5
#pragma config MCLRE    = EXTMCLR // /MCLR is reset (needed for ICSP)
#pragma config STVREN   = ON
#pragma config LVP      = OFF // High voltage programming (PICkit, ICD); RB5 is I/O
#pragma config XINST    = OFF
#pragma config CP0 = OFF, CP1 = OFF, CP2 = OFF, CP3 = OFF, CPB = OFF, CPD = OFF
#pragma config WRT0 = OFF, WRT1 = OFF, WRT2 = OFF, WRT3 = OFF, WRTC = OFF, WRTB = OFF, WRTD = OFF
#pragma config EBTR0 = OFF, EBTR1 = OFF, EBTR2 = OFF, EBTR3 = OFF, EBTRB = OFF

// --- Derived values -------------------------------------------------------------

// UART with BRG16 = 1 and BRGH = 1: baud = FOSC / (4 * (SPBRG + 1))
#define UART_SPBRG ((FOSC_HZ + 2UL * RS485_BAUDRATE) / (4UL * RS485_BAUDRATE) - 1UL)

// Timer 2, 1 ms: FOSC/4 / prescaler / (PR2 + 1) / postscaler = 1000
#if (FOSC_HZ == 64000000UL)
#define T2_PR2     249                         // 16 MHz / 16 / 250 / 4
#define T2_CON_VAL ((3u << 3) | 0x04u | 0x02u) // Postscale 1:4, on, prescale 1:16
#else
#define T2_PR2     143                         // 4.608 MHz / 16 / 144 / 2
#define T2_CON_VAL ((1u << 3) | 0x04u | 0x02u) // Postscale 1:2, on, prescale 1:16
#endif

// --- RS-485 receive buffer ------------------------------------------------------

#define RX_SIZE 64u // Power of two

static volatile uint8_t s_rx_char[RX_SIZE];
static volatile uint8_t s_rx_mark[RX_SIZE / 8u]; // Ninth bits, one bit per character
static volatile uint8_t s_rx_head;
static volatile uint8_t s_rx_tail;
static volatile uint16_t s_overruns;
static volatile uint16_t s_millis;

// Direction: high = send
static void
dir_send(uint8_t on)
{
#if (RS485_DIR_PINS == 0) || (RS485_DIR_PINS == 1)
  LATAbits.LATA0 = on;
#endif
#if (RS485_DIR_PINS == 0) || (RS485_DIR_PINS == 2)
  LATBbits.LATB2 = on;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// hw_init
//

void
hw_init(void)
{
#if defined(CLOCK_INTOSC)
  OSCCON            = 0x70; // HFINTOSC 16 MHz
  OSCTUNEbits.PLLEN = 1;
  while (!OSCCON2bits.PLLRDY) {
    ;
  }
#endif

  // All pins digital
  ANSELA = 0;
  ANSELB = 0;
  ANSELC = 0;

  // RS-485 direction pins: outputs, receiving
  LATA  = 0;
  LATB  = 0;
  TRISA = 0xFE; // RA0 output
  // RB0 button (input), RB1 status LED, RB2 direction, RB4 LED; RB6/RB7 ICSP
  TRISB = 0xE9; // 1110 1001: RB1, RB2, RB4 outputs
  dir_send(0);

  // Button: weak pull-up on RB0
  WPUB              = 0x01;
  INTCON2bits.nRBPU = 0;

  // EUSART1, 9 bit, BRG16 high speed
  TRISCbits.TRISC6 = 1; // The EUSART drives TX itself
  TRISCbits.TRISC7 = 1;
  BAUDCON1         = 0x08; // BRG16
  SPBRGH1          = (uint8_t) (UART_SPBRG >> 8);
  SPBRG1           = (uint8_t) (UART_SPBRG & 0xFF);
#if RS485_NINE_BIT
  TXSTA1 = 0x64; // TX9, TXEN, BRGH
  RCSTA1 = 0xD0; // SPEN, RX9, CREN
#else
  TXSTA1 = 0x24; // TXEN, BRGH
  RCSTA1 = 0x90; // SPEN, CREN
#endif
  PIE1bits.RC1IE = 1;

  // Timer 2: 1 ms tick
  PR2             = T2_PR2;
  TMR2            = 0;
  T2CON           = T2_CON_VAL;
  PIE1bits.TMR2IE = 1;

  // EEPROM: data memory
  EECON1bits.EEPGD = 0;
  EECON1bits.CFGS  = 0;

  RCONbits.IPEN   = 0; // One interrupt level
  INTCONbits.PEIE = 1;
  INTCONbits.GIE  = 1;

  WDTCONbits.SWDTEN = 1;
}

///////////////////////////////////////////////////////////////////////////////
// Interrupts
//

void
__interrupt() isr(void)
{
  // Received characters
  while (PIR1bits.RC1IF) {
    uint8_t mark = RCSTA1bits.RX9D; // Read before RCREG1
    uint8_t c    = RCREG1;
    uint8_t next = (uint8_t) ((s_rx_head + 1u) & (RX_SIZE - 1u));
    if (next == s_rx_tail) {
      s_overruns++;
    }
    else {
      s_rx_char[s_rx_head] = c;
      if (mark) {
        s_rx_mark[s_rx_head >> 3] |= (uint8_t) (1u << (s_rx_head & 7u));
      }
      else {
        s_rx_mark[s_rx_head >> 3] &= (uint8_t) ~(1u << (s_rx_head & 7u));
      }
      s_rx_head = next;
    }
  }
  if (RCSTA1bits.OERR) {
    // UART overrun: restart the receiver
    RCSTA1bits.CREN = 0;
    RCSTA1bits.CREN = 1;
    s_overruns++;
  }

  // 1 ms tick
  if (PIR1bits.TMR2IF) {
    PIR1bits.TMR2IF = 0;
    s_millis++;
    hw_tick_1ms();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Timekeeping
//

uint16_t
hw_millis(void)
{
  uint16_t ms;
  INTCONbits.GIE = 0;
  ms             = s_millis;
  INTCONbits.GIE = 1;
  return ms;
}

void
hw_irq_disable(void)
{
  INTCONbits.GIE = 0;
}

void
hw_irq_enable(void)
{
  INTCONbits.GIE = 1;
}

void
hw_watchdog(void)
{
  CLRWDT();
}

void
hw_reset(void)
{
  RESET();
}

///////////////////////////////////////////////////////////////////////////////
// I/O
//

void
hw_led(uint8_t on)
{
  LATBbits.LATB4 = on ? 1 : 0;
}

void
hw_status_led(uint8_t on)
{
  LATBbits.LATB1 = on ? 1 : 0;
}

void
hw_status_led_toggle(void)
{
  LATBbits.LATB1 ^= 1;
}

uint8_t
hw_button(void)
{
  return PORTBbits.RB0 ? 0 : 1; // Pressed pulls the pin low
}

///////////////////////////////////////////////////////////////////////////////
// RS-485
//

uint8_t
hw_rs485_read(uint8_t *c, int8_t *mark)
{
  if (s_rx_tail == s_rx_head) {
    return 0;
  }
  *c = s_rx_char[s_rx_tail];
#if RS485_NINE_BIT
  *mark = (s_rx_mark[s_rx_tail >> 3] & (uint8_t) (1u << (s_rx_tail & 7u))) ? 1 : 0;
#else
  *mark = -1;
#endif
  s_rx_tail = (uint8_t) ((s_rx_tail + 1u) & (RX_SIZE - 1u));
  return 1;
}

void
hw_rs485_send(const uint8_t *data, uint8_t len)
{
  // The SP485E's DE and /RE are tied together: while it sends its receiver
  // output floats. Keep the UART receiver off meanwhile.
  RCSTA1bits.CREN = 0;
  dir_send(1);

  for (uint8_t i = 0; i < len; i++) {
    while (!PIR1bits.TX1IF) {
      ; // Wait until TXREG1 is free
    }
#if RS485_NINE_BIT
    TXSTA1bits.TX9D = (0 == i) ? 1 : 0;
#endif
    TXREG1 = data[i];
  }
  // Wait until the last stop bit has left the shift register
  while (!TXSTA1bits.TRMT) {
    ;
  }

  dir_send(0);
  RCSTA1bits.CREN = 1;
}

uint16_t
hw_rs485_overruns(void)
{
  uint16_t n;
  INTCONbits.GIE = 0;
  n              = s_overruns;
  INTCONbits.GIE = 1;
  return n;
}

///////////////////////////////////////////////////////////////////////////////
// EEPROM
//

uint8_t
hw_eeprom_read(uint8_t addr)
{
  EEADR            = addr;
  EECON1bits.EEPGD = 0;
  EECON1bits.CFGS  = 0;
  EECON1bits.RD    = 1;
  return EEDATA;
}

uint8_t
hw_eeprom_busy(void)
{
  return EECON1bits.WR;
}

void
hw_eeprom_write_start(uint8_t addr, uint8_t value)
{
  EEADR            = addr;
  EEDATA           = value;
  EECON1bits.EEPGD = 0;
  EECON1bits.CFGS  = 0;
  EECON1bits.WREN  = 1;
  INTCONbits.GIE   = 0; // The unlock sequence must not be interrupted
  EECON2           = 0x55;
  EECON2           = 0xAA;
  EECON1bits.WR    = 1;
  INTCONbits.GIE   = 1;
  EECON1bits.WREN  = 0; // The write goes on; WR clears when it is done
}
