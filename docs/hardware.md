# Hardware

## The board

The [Modtronix SBC28PC-IR4](https://modtronix.com/product/sbc28pc-ir4/) is a
small (58 x 54 mm) single board computer for 28-pin DIP PICs with an RS-485
interface. It is the SBC28PC Rev 2 (the PCB says "SBC28PC Rev2") assembled
with the RS-485 transceiver; the SBC28PC-IR2 is the same board with RS-232.

On the board:

| Part | Use in the demo |
|------|-----------------|
| 28-pin DIP socket (IC4) | The PIC18F25K22 (not included with the board) |
| SP485E RS-485 transceiver (15 kV ESD protected) | The RS-485 bus. DI = RC6 (TX), RO = RC7 (RX), DE and /RE tied together to header J1 |
| 7805 regulator, 2.1 mm power jack | 7-30 V in, 5 V for the PIC. A diode protects against reverse polarity |
| 3-pin Molex connector CON6 | RS-485: 1 = A, 2 = ground, 3 = B |
| 5-pin 3.81 mm terminal block | RS-485 A/B when SJ4 and SJ5 are made (or CAN when the MCP2551 is fitted), ground, Vin |
| J3 | 120 Ω termination of the bus (made at delivery) |
| ICSP connector CON3 (8 pins, 2 mm) | Programming |
| Daughter board connectors CON1/CON2, frontend connectors BRD1/BRD2 | All free PIC pins: the LED and the button of the demo go here |
| Power LED | Shows that the board has power. There is no user LED |
| Crystal footprint | Empty by default; 16 MHz or 18.432 MHz can be ordered |

The board's documentation (datasheet with schematics, *sbc28pcr2_ir4.pdf*) is
linked from the [product page](https://modtronix.com/product/sbc28pc-ir4/).

## The PIC

The firmware is written for the **PIC18F25K22** (32 KB flash, 1536 bytes RAM,
256 bytes EEPROM, internal 64 MHz oscillator). The PIC18F26K22 (64 KB) works
with the same firmware (`make CPU=18F26K22`). The demo needs about 9.4 KB of
flash, so the 16 KB PIC18F24K22 would also fit, but is not tested.

Older PIC18s the board was designed for (PIC18F252, PIC18F2680, ...) need
other configuration bits and register names; the K22 is the current, cheap
choice.

## Clock

The board comes without a crystal. The firmware then runs the PIC from its
internal 16 MHz oscillator with the 4x PLL, at 64 MHz. The baud rate
generator gives 115200 baud within 0.1 %, but the internal oscillator itself
is only accurate to about 2 % over temperature, which is near the limit of
what a UART accepts. That is fine on a desk; for a node that must work over a
wide temperature range, order the board with a crystal and build with
`CLOCK=XTAL16` (16 MHz, 64 MHz with the PLL) or `CLOCK=XTAL18432` (exact
standard baud rates).

## RS-485

### Direction control: J1, SJ2 and SJ3

The SP485E's DE (driver enable) and /RE (receiver enable) pins are tied
together and go to the middle pin of the three pin header **J1**. From there
they can be connected to a PIC pin:

| Connection | Made with | PIC pin |
|------------|-----------|---------|
| J1 pins 1-2 | jumper on J1, or solder jumper SJ3 | RA0 |
| J1 pins 2-3 | jumper on J1, or solder jumper SJ2 | RB2 |

One of the two must be made, otherwise the transceiver never sends. The
datasheet does not say which one is made at delivery, so the firmware drives
**both RA0 and RB2** with the same signal: it works whichever is made. If
you need one of the two pins for something else, check your board and build
with `DIR_PINS=1` (RA0 only) or `DIR_PINS=2` (RB2 only).

While the node sends, the receiver of the SP485E is switched off (DE and /RE
are the same signal), so the node does not read back its own frames.

### The bus

Wire the nodes as a daisy chain (not a star):

```text
  PC with USB RS-485 adapter          SBC28PC-IR4 node 1         node 2 ...
  A  ─────────────────────────────── A (CON6 pin 1) ─────────── A
  B  ─────────────────────────────── B (CON6 pin 3) ─────────── B
  GND ────────────────────────────── GND (CON6 pin 2) ───────── GND
```

- **A goes to A and B to B.** Some adapters call them D+/D- or +/-; A is the
  non-inverting line (D+). If nothing works, swap A and B.
- **Termination**: the two ends of the cable need a 120 Ω resistor between A
  and B. J3 on the SBC28PC is that resistor and is made at delivery: remove
  the J3 jumper on every node that is not at an end of the cable. Most USB
  adapters have a switch or jumper for their own termination.
- **Ground**: connect the signal ground too; RS-485 tolerates a few volts of
  ground difference, not an unconnected ground.
- **Bias**: an idle bus needs fail-safe bias resistors (usually in the master's
  adapter). Without them the receivers may see noise between frames.
- Every node on the segment needs its **own address** and the **same baud
  rate**.

### Using the terminal block

To get A and B on the 5-pin terminal block (pin 2 = A, pin 3 = B, pin 1 =
ground, pin 5 = Vin), make solder jumpers **SJ4 and SJ5**. Then do not fit a
CAN driver chip in socket IC5. With SJ6 or J2 made, pin 5 carries the
unregulated supply, so a chain of boards can share one power supply.

## LED and button

The SBC28PC has no user LED or button. Connect them to the daughter board
connector (or a prototype daughter board such as the PT01TC) using the
signals printed on the board:

```text
  B4 ───[470 Ω]───►|─── GND      Blinky LED (on when RB4 is high)
  B1 ───[470 Ω]───►|─── GND      Status LED, optional
  B0 ────── button ──── GND      Push button (the PIC's pull-up is used)
```

| Signal | PIC pin | Daughter board connector |
|--------|---------|--------------------------|
| B0 | RB0 | CON2 |
| B1 | RB1 | CON2 |
| B4 | RB4 | CON2 |
| GND, +5V | | CON1 |

The status LED is on while the node is active and blinks off for a moment
every time the master polls the node, so you can see that the bus works.

Do not use RB6 and RB7 (programming), RC6 and RC7 (RS-485), RA0 and RB2
(RS-485 direction, see above), or RC3 and RC4 (they have the I2C pull-ups of
the Micro Match connector).

## Power

7-30 V DC on the 2.1 mm jack (center positive), or on terminal block pin 5
when SJ6/J2 are made. The board with the PIC takes about 20 mA.
