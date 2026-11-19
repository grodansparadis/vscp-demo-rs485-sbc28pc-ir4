# Makefile - VSCP Blinky node on RS-485 for the Modtronix SBC28PC-IR4
#
# Builds firmware for a PIC18F25K22 (or PIC18F26K22) with Microchip XC8.
#
#   make                         build build/blinky.hex
#   make NODE_ADDRESS=5          node address 5
#   make CLOCK=XTAL16            16 MHz crystal (default: internal oscillator)
#   make CPU=18F26K22            other chip of the family
#   make program                 program with a PICkit 4/5 (MPLAB IPE)
#   make clean
#
# All options are described in config.h and docs/firmware.md.

CPU          ?= 18F25K22
XC8          ?= $(firstword $(wildcard /opt/microchip/xc8/v*/bin/xc8-cc) xc8-cc)
# Device family pack. Found in the MPLAB X pack folder when not given.
DFP          ?= $(lastword $(sort $(wildcard $(HOME)/.mchp_packs/Microchip/PIC18F-K_DFP/*/xc8) \
                                  $(wildcard /opt/microchip/mplabx/*/packs/Microchip/PIC18F-K_DFP/*/xc8)))
IPECMD       ?= $(firstword $(wildcard /opt/microchip/mplabx/*/mplab_platform/mplab_ipe/ipecmd.sh) ipecmd)
TOOL         ?= PPK4

NODE_ADDRESS ?= 1
BAUDRATE     ?= 115200
CLOCK        ?= INTOSC
NINE_BIT     ?= 1
FIXED_LENGTH ?= 0
CRC_ATM      ?= 0
DIR_PINS     ?= 0

VSCP_FW = third-party/vscp-firmware/src
BUILD   = build

SRC = main.c hw-pic18.c blinky.c rs485-node.c persist.c vscp-rs485.c $(VSCP_FW)/vscp-firmware.c

DEFS = -DNODE_ADDRESS=$(NODE_ADDRESS) -DRS485_BAUDRATE=$(BAUDRATE)UL -DCLOCK_$(CLOCK) \
       -DRS485_NINE_BIT=$(NINE_BIT) -DRS485_FIXED_LENGTH=$(FIXED_LENGTH) -DRS485_CRC_ATM=$(CRC_ATM) \
       -DRS485_DIR_PINS=$(DIR_PINS)

CFLAGS = -mcpu=$(CPU) $(if $(DFP),-mdfp=$(DFP)) -std=c99 -O2 \
         -mwarn=-3 -Wa,-a -msummary=-psect,-class,+mem,-hex,-file \
         -I. -I$(VSCP_FW) $(DEFS)

.PHONY: all clean program

all: $(BUILD)/blinky.hex

$(VSCP_FW)/vscp-firmware.c:
	@echo "vscp-firmware is missing: run 'git submodule update --init'" && false

$(BUILD)/blinky.hex: $(SRC) $(wildcard *.h)
	@mkdir -p $(BUILD)
	$(XC8) $(CFLAGS) -o $(BUILD)/blinky.elf $(SRC)
	@ls -l $(BUILD)/blinky.hex

program: $(BUILD)/blinky.hex
	$(IPECMD) -TP$(TOOL) -P$(CPU) -F$(BUILD)/blinky.hex -M -OL

clean:
	rm -rf $(BUILD)
