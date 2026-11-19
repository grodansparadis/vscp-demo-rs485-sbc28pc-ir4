# The firmware

## Source files

| File | Content |
|------|---------|
| `firmware/main.c` | Starts the hardware and the node, then runs the main loop |
| `firmware/config.h` | Build options: address, baud rate, frame format, clock, direction pins, GUID |
| `firmware/hw.h`, `hw-pic18.c` | Everything that touches the PIC: configuration bits, clock, UART, 1 ms timer, pins, EEPROM, interrupts |
| `firmware/rs485-node.h/.c` | The node side of VSCP over RS-485: answers polls, queues events, and the transport callbacks of `vscp-firmware.c` |
| `firmware/vscp-rs485.h/.c` | Frame encoder/decoder and CRC, shared with the master drivers [vscpl1drv-rs485](https://github.com/grodansparadis/vscpl1drv-rs485) and [vscpl2drv-rs485](https://github.com/grodansparadis/vscpl2drv-rs485) |
| `firmware/blinky.h/.c` | The application: LED, button, registers, decision matrix, and the callbacks of `vscp-firmware.c` |
| `firmware/persist.h/.c` | The settings in EEPROM |
| `firmware/vscp-projdefs.h`, `vscp-compiler.h` | Project settings for `vscp-firmware.c` |
| `firmware/third-party/vscp-firmware` | The VSCP firmware library (git submodule) |
| `tests/` | Host tests: the same code on a PC against simulated hardware |

`hw-pic18.c` is the only PIC-specific file. To move the node to another MCU,
write a new `hw-*.c` with the functions in `hw.h`.

## How it works

```text
          ┌──────────────────── PIC18F25K22 ───────────────────────┐
 RS-485   │  UART ISR        rs485-node.c         vscp-firmware.c   │
 A/B ─────┤  RX ring  ──►  frame decoder  ──► RXQ ──► vscp_getEvent │
 SP485E   │  (64 chars,     POLL to me?           │                 │
          │   9th bit)      └► answer_poll  ◄── TXQ ◄── vscp_sendEvent
          │                    (EVENT or                blinky.c     │
          │                     NO_EVENTS)      registers, DM, LED,  │
          │  Timer 2 1 ms  ──► blinky_tick_1ms  button, persist.c    │
          └─────────────────────────────────────────────────────────┘
```

**The bus.** The master (address 0) owns the segment. It polls each node in
turn with a POLL frame. A node only sends when it is polled: it answers with
the oldest event in its send queue, or with NO_EVENTS. The master sends
events to a node (register reads and writes, decision matrix events) as
EVENT frames addressed to it, or to broadcast address 255. The frame format
is in the
[specification](https://grodansparadis.github.io/vscp-doc-spec/#/./vscp_level_i_over_rs-485_rs-422):

```text
POLL       dest | src | 2 | CRC
NO_EVENTS  dest | src | 3 | CRC
EVENT      dest | src | 1 | flags | class | type | data[0-8] | CRC
```

`flags` holds the number of data bytes (bits 0-3) and bit 8 of the class
(bit 4). The CRC is CRC-8/MAXIM-DOW over all characters before it. With the
ninth bit enabled, the first character of a frame (the destination address)
has the ninth bit set, so every node finds the frame starts even after
noise.

**Receiving.** The UART interrupt puts every character, with its ninth bit,
into a 64-character ring. `rs485_node_work()` (called first in every turn of
the main loop) feeds them to the frame decoder in `vscp-rs485.c`. Frames with
a bad CRC are dropped and counted. In 8N1 mode, where there is no ninth bit,
a pause of 3 ms resets the decoder instead.

**Answering polls.** A POLL to the node's address is answered at once from the
main loop, well within the master's 20 ms poll time-out. The answer is sent
with interrupts on: the driver enable (RA0/RB2) is raised, the bytes are
written (the first with the ninth bit set), and after the last stop bit has
left the shift register (TRMT) the driver is released and the receiver turned
on again.

**Events.** `vscp-firmware.c` sends events through `sendVSCPFrame()`, which
puts them in an eight-entry queue that the polls empty. It receives events
through `getVSCPFrame()`, from a four-entry queue filled by the decoder.

**The nickname.** VSCP Level I nodes on CAN find a free nickname by
themselves. On RS-485 the nickname is the node's bus address, given when the
firmware is built and stored in EEPROM. The node goes straight to the active
state when it starts, and `blinky.c` restores the address if a protocol
event (DROP_NICKNAME) clears it. See [registers.md](registers.md).

**Time.** Timer 2 interrupts every millisecond. The tick drives the VSCP
timers, the blink interval, the millisecond counter, the one second work of
`vscp-firmware.c` (heartbeat) and the button debounce (20 ms).

**Settings.** `persist.c` keeps the settings in a RAM copy and writes the bytes
that changed to EEPROM a second after the last change, one byte at a time so
that an EEPROM write (about 4 ms) never delays the answer to a poll.

**Watchdog.** The watchdog is on (about one second) and is cleared in the main
loop. A stuck node restarts.

## Build options

Every option is a make variable that becomes a define of `config.h`:

| make variable | Default | Meaning |
|---------------|---------|---------|
| `NODE_ADDRESS` | 1 | RS-485 address (1-254) at the first start |
| `BAUDRATE` | 115200 | Baud rate. Must be the same on the master and all nodes |
| `NINE_BIT` | 1 | 1: address character with the ninth bit (the specification). 0: plain 8N1 |
| `FIXED_LENGTH` | 0 | 1: EVENT frames always carry eight data bytes |
| `CRC_ATM` | 0 | 1: CRC-8 polynomial 0x07 instead of the DOW CRC |
| `CLOCK` | INTOSC | `INTOSC` internal 64 MHz, `XTAL16` 16 MHz crystal with PLL, `XTAL18432` 18.432 MHz crystal |
| `DIR_PINS` | 0 | Direction pins: 0 = RA0 and RB2, 1 = RA0 only, 2 = RB2 only |
| `CPU` | 18F25K22 | Also 18F26K22 |
| `XC8`, `DFP` | found | Compiler and device pack, if `make` does not find them |
| `TOOL` | PPK4 | Programmer for `make program`: PPK4, PPK5, ICD4, ICD5 |

The frame options must match the master:

| Node | vscpl1drv-rs485 flags | vscpl2drv-rs485 |
|------|-----------------------|-----------------|
| `NINE_BIT=0` | bit 0 | `"nine-bit": false` |
| `FIXED_LENGTH=1` | bit 1 | `"fixed-length": true` |
| `CRC_ATM=1` | bit 2 | `"crc": "0x07"` |

The GUID prefix (`NODE_GUID_PREFIX`) can be changed in `config.h`.

## Resources

With XC8 v3.10 (free mode, `-O2`) for the PIC18F25K22:

| | Used | Of |
|-|------|----|
| Program memory | 9.4 KB | 32 KB |
| RAM | 537 bytes | 1536 bytes |
| EEPROM | 48 bytes | 256 bytes |

XC8 prints warning 520 ("function is never called") for callbacks that
`vscp-firmware.c` only uses with some options; they are harmless.

## Adding to the node

To make your own node from the demo:

1. Change `s_mdf_url` in `blinky.c` and write an MDF for your registers.
2. Replace the registers in `vscp_readAppReg()` / `vscp_writeAppReg()` and the
   fields of `persist_t` in `persist.h`.
3. Add your actions to `do_action()`.
4. Use a GUID range of your own (`NODE_GUID_PREFIX`).

Keep the main loop fast: the node must answer a poll within the master's
poll time-out (20 ms by default). Long work should be split into steps.
