# vscp-demo-rs485-sbc28pc-ir4

[![Build](https://github.com/grodansparadis/vscp-demo-rs485-sbc28pc-ir4/actions/workflows/build.yml/badge.svg)](https://github.com/grodansparadis/vscp-demo-rs485-sbc28pc-ir4/actions/workflows/build.yml)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![Last commit](https://img.shields.io/github/last-commit/grodansparadis/vscp-demo-rs485-sbc28pc-ir4)

The [VSCP](https://www.vscp.org) blinky demo for the Modtronix
[SBC28PC-IR4](https://modtronix.com/product/sbc28pc-ir4/) as a **VSCP Level I
node on RS-485**, following
[VSCP Level I over RS-485/RS-422](https://grodansparadis.github.io/vscp-doc-spec/#/./vscp_level_i_over_rs-485_rs-422).

Blinky is the smallest useful VSCP node: an LED that blinks, a button that
sends events, a few registers and a small decision matrix. The same demo
exists for other boards and transports, for example
[CAN4VSCP on an STM32F103](https://github.com/grodansparadis/vscp-demo-can4vscp-stm32f103).
Use this project to learn how a node on an RS-485 segment works, or as the
starting point for your own RS-485 node.

| | |
|---|---|
| Board | Modtronix SBC28PC-IR4 (SBC28PC Rev 2 with RS-485), SP485E transceiver |
| MCU | PIC18F25K22 (also PIC18F26K22), 32 KB flash, 1.5 KB RAM, 256 bytes EEPROM |
| Transport | RS-485, 115200 baud, ninth (address) bit, polled by the master |
| Master | [vscpl1drv-rs485](https://github.com/grodansparadis/vscpl1drv-rs485) (CANAL) or [vscpl2drv-rs485](https://github.com/grodansparadis/vscpl2drv-rs485) (VSCP daemon) |
| VSCP stack | [vscp-firmware](https://github.com/grodansparadis/vscp-firmware) Level I (`vscp-firmware.c`), as a git submodule |
| Tools | Microchip XC8 (free), make; MPLAB IPE and a PICkit 4/5 to program |
| Size | About 9.4 KB flash, 540 bytes RAM |

More documentation:

- [Getting started](docs/getting-started.md)
- [Hardware: the board, jumpers, wiring](docs/hardware.md)
- [Programming the board](docs/programming.md)
- [How the firmware works and build options](docs/firmware.md)
- [Registers, events and the decision matrix](docs/registers.md)
- [Testing the node from a PC](docs/testing.md)

## What the node does

- Answers the master's polls with its events, or with "no events".
- Blinks an LED. The interval is a register (default 500 ms).
- Sends events when the button is pressed and released.
- Runs a millisecond counter that can be read from registers.
- Has a decision matrix with four rows, so events from the master can turn
  the LED on and off, start and stop the blinking and clear the counter.
- Keeps its RS-485 address (its VSCP nickname) and the settings in EEPROM.
- Sends NEW_NODE_ONLINE when it starts and a heartbeat every minute.

## Hardware in short

The SBC28PC-IR4 is delivered without a CPU: put a PIC18F25K22 in the socket.
It has no user LED or button; connect them to the daughter board connector
(details in [docs/hardware.md](docs/hardware.md)).

| Pin | Function |
|-----|----------|
| RC6 / RC7 | RS-485 TX / RX (SP485E DI / RO), on the board |
| RA0 and RB2 | RS-485 direction (SP485E DE//RE), on the board through J1/SJ2/SJ3 |
| RB4 | Blinky LED to ground through 470 Ω |
| RB0 | Button to ground (internal pull-up) |
| RB1 | Status LED to ground through 470 Ω (optional) |
| RB6, RB7, /MCLR | ICSP connector CON3 |

RS-485 A/B are on the 3-pin Molex connector (A, ground, B), or on the 5-pin
terminal block when solder jumpers SJ4 and SJ5 are made.

## Build

Install [MPLAB XC8](https://www.microchip.com/xc8) (the free version is
enough) and MPLAB X (for its device packs and MPLAB IPE). Then:

```bash
git clone --recurse-submodules https://github.com/grodansparadis/vscp-demo-rs485-sbc28pc-ir4.git
cd vscp-demo-rs485-sbc28pc-ir4/firmware
make NODE_ADDRESS=1
```

The result is `firmware/build/blinky.hex`. Every node on a segment needs its
own address. Other options (crystal, 8N1, frame format, direction pins) are
in [docs/firmware.md](docs/firmware.md).

## Program

Connect a PICkit 4 or 5 to the ICSP connector with the Modtronix PGM2KIT
adapter, power the board and run

```bash
make program
```

or load `build/blinky.hex` with MPLAB IPE. Step by step in
[docs/programming.md](docs/programming.md).

## Try it

Connect the node to an RS-485 adapter on a PC (A to A, B to B, ground) and
watch it with the Level I driver:

```bash
vscp-dump -i canal --driver /var/lib/vscp/drivers/level1/vscpl1drv-rs485.so \
          --driver-config "/dev/ttyUSB0;115200;1-8"
```

NEW_NODE_ONLINE appears, then a heartbeat every minute and an event for every
button press. With the VSCP daemon and vscpl2drv-rs485, VSCP Works can read
and write the node's registers and set up its decision matrix. See
[docs/testing.md](docs/testing.md).

## Tests

```bash
cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
```

The node firmware runs on the PC against simulated hardware, with the test
playing the master. See [docs/testing.md](docs/testing.md).

## License

MIT, see [LICENSE](LICENSE).
