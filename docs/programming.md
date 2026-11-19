# Programming the board

The PIC is programmed in circuit through the ICSP connector **CON3**, an
8-pin 2 mm header. The firmware uses high voltage programming (LVP is off),
so use a programmer that can drive /MCLR to programming voltage:

- **PICkit 4 or PICkit 5** with the Modtronix **PGM2KIT** adapter (recommended),
- **MPLAB ICD 4/5** with the Modtronix **PGM06** adapter.

An MPLAB Snap can only do low voltage programming and does not work with the
default configuration bits.

## 1. Install the tools

- [MPLAB XC8](https://www.microchip.com/xc8) (free version) — the compiler.
- [MPLAB X IDE](https://www.microchip.com/mplab-x-ide), which includes
  **MPLAB IPE** (the programming tool) and the device packs. On Linux it
  installs to `/opt/microchip/mplabx/`.

## 2. Build the firmware

```bash
git clone --recurse-submodules https://github.com/grodansparadis/vscp-demo-rs485-sbc28pc-ir4.git
cd vscp-demo-rs485-sbc28pc-ir4/firmware
make NODE_ADDRESS=1
```

Choose a different `NODE_ADDRESS` (1-254) for every node. The hex file is
`build/blinky.hex`. See [firmware.md](firmware.md) for the other options.

If `make` can not find the compiler or the device pack, give their places:

```bash
make XC8=/opt/microchip/xc8/v3.10/bin/xc8-cc \
     DFP=$HOME/.mchp_packs/Microchip/PIC18F-K_DFP/1.15.303/xc8
```

## 3. Connect the programmer

Put the PIC18F25K22 in the socket (pin 1 towards the notch on the socket).
Plug the adapter into CON3 and the programmer into the adapter. The ICSP
connector carries these signals:

| CON3 pin | Signal | | CON3 pin | Signal |
|----------|--------|-|----------|--------|
| 1 | RB7 / PGD (data) | | 2 | RB6 / PGC (clock) |
| 3 | GND | | 4 | /MCLR / VPP |
| 5 | — | | 6 | VDD of the PIC |
| 7 | — | | 8 | VCC |

Without an adapter, connect a PICkit with wires:

| PICkit pin | Signal | CON3 pin |
|------------|--------|----------|
| 1 (arrow) | /MCLR / VPP | 4 |
| 2 | VDD (target voltage sense) | 6 |
| 3 | GND | 3 |
| 4 | PGD / ICSPDAT | 1 |
| 5 | PGC / ICSPCLK | 2 |
| 6 | (LVP, not used) | — |

Check the pinout against Modtronix's page "Programming Modtronix PIC based
boards" before the first connection.

**Power the board from its own supply** (7-30 V on the power jack). Do not let
the programmer power it: the 7805 and the RS-485 transceiver draw more than a
PICkit wants to supply.

## 4a. Program with make

```bash
make program                 # PICkit 4 (default)
make program TOOL=PPK5       # PICkit 5
make program TOOL=ICD4       # MPLAB ICD 4
```

This runs MPLAB IPE's command line tool (`ipecmd.sh`): it erases the PIC,
programs and verifies `build/blinky.hex` and releases /MCLR so the node
starts.

## 4b. Program with MPLAB IPE

1. Start MPLAB IPE.
2. **Device**: PIC18F25K22, press **Apply**.
3. **Tool**: your PICkit or ICD, press **Connect**. IPE reports the device id
   it read; if it can not find the device, check the cable and the board's
   power.
4. **Hex File**: browse to `firmware/build/blinky.hex`.
5. Press **Program**, then **Verify**.
6. Under **Settings > Release from Reset** let the PIC run, or unplug the
   programmer.

## 5. Check that it runs

- The status LED (if fitted on RB1) lights up: the node is active.
- The blinky LED on RB4 blinks every 500 ms.
- Connect the bus and a master (see [testing.md](testing.md)): the node's
  first answer is NEW_NODE_ONLINE with its address, the status LED flickers
  every time it is polled.

## Building in MPLAB X

The repository builds with make. To work in the MPLAB X IDE instead:

1. **File > New Project > Microchip Embedded > Standalone Project**, device
   PIC18F25K22, compiler XC8.
2. Add the source files: `main.c`, `hw-pic18.c`, `blinky.c`, `rs485-node.c`,
   `persist.c`, `vscp-rs485.c` and `third-party/vscp-firmware/src/vscp-firmware.c`.
3. **Project Properties > XC8 Global Options > XC8 Compiler**: C standard
   C99, include directories `.` and `third-party/vscp-firmware/src`, and the
   macros you want to change (for example `NODE_ADDRESS=5`).
4. Build and program with the MPLAB X toolbar.

## Changing the address later

The address is stored in EEPROM when the node starts the first time.
Programming a new hex file with MPLAB IPE erases the EEPROM too (unless you
tell IPE to preserve it), so the new `NODE_ADDRESS` takes effect. Over the bus
the address can be changed with CLASS1.PROTOCOL SET_NICKNAME, see
[registers.md](registers.md).
