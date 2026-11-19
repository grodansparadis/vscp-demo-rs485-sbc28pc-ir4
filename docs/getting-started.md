# Getting started

From an unpacked board to a node that shows up on the bus, in six steps.

## 1. What you need

- A Modtronix **SBC28PC-IR4** and a **PIC18F25K22** (DIP-28).
- A 7-30 V DC power supply with a 2.1 mm plug (center positive).
- An LED with a 470 Ω resistor, a push button, and optionally a second LED
  with a resistor for the status (see [hardware](hardware.md#led-and-button)).
- A **PICkit 4 or 5** and the Modtronix **PGM2KIT** adapter (or wires).
- A **USB RS-485 adapter** and a twisted pair with ground.
- A PC with MPLAB XC8 and MPLAB X (for MPLAB IPE).

## 2. Prepare the board

1. Put the PIC in the socket, pin 1 to the notch.
2. Check that header **J1** connects the SP485E's DE//RE to the PIC: a jumper
   on pins 1-2 (RA0) or 2-3 (RB2), or solder jumper SJ3/SJ2. The default
   firmware works with either. See [hardware](hardware.md#direction-control-j1-sj2-and-sj3).
3. Connect the LED from **B4** to ground, the button from **B0** to ground and
   (optional) the status LED from **B1** to ground on daughter board
   connector CON2.
4. Keep **J3** (termination) when the node is at an end of the cable.

## 3. Build

```bash
git clone --recurse-submodules https://github.com/grodansparadis/vscp-demo-rs485-sbc28pc-ir4.git
cd vscp-demo-rs485-sbc28pc-ir4/firmware
make NODE_ADDRESS=1
```

## 4. Program

Power the board, connect the PICkit to the ICSP connector CON3 through the
PGM2KIT adapter, and run `make program`, or load `build/blinky.hex` with
MPLAB IPE. Details in [programming](programming.md).

The LED starts blinking every 500 ms.

## 5. Connect the bus

Connect A to A, B to B and ground to ground between the USB adapter and the
Molex connector CON6 (pin 1 A, pin 2 ground, pin 3 B).

## 6. Watch the node

```bash
vscp-dump -i canal --driver /var/lib/vscp/drivers/level1/vscpl1drv-rs485.so \
          --driver-config "/dev/ttyUSB0;115200;1-8"
```

Reset the node: NEW_NODE_ONLINE from node 1 appears. Press the button: TURN-ON
and START, then STOP when you let go. Continue with [testing](testing.md) to
configure the node from VSCP Works.
