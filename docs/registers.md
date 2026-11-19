# Registers, events and the decision matrix

The node follows the blinky module description file (MDF),
<https://eurosource.se/blinky0.json>. Tools such as VSCP Works read the MDF
from the URL the node reports in its standard registers and then show the
registers below with names and descriptions.

## Application registers (page 0)

| Register | Name | Access | Default | Stored | Description |
|----------|------|--------|---------|--------|-------------|
| 0 | Zone | rw | 0 | yes | Zone of the node |
| 1 | Subzone | rw | 0 | yes | Subzone of the node |
| 2 | Status | rw | | no | Bit 1: LED is on. Bit 0: button is pressed. Writing bit 1 turns the LED on or off |
| 3 | Control | rw | 0xCE | yes | See below |
| 4 | Blink interval MSB | rw | 0x01 | yes | Blink interval in milliseconds, high byte. Default 500 ms. Interval zero stops the blinking |
| 5 | Blink interval LSB | rw | 0xF4 | yes | Blink interval in milliseconds, low byte |
| 6 | Millisecond counter, byte 3 (MSB) | rw | | no | 32-bit counter that counts milliseconds when enabled in the control register |
| 7 | Millisecond counter, byte 2 | rw | | no | |
| 8 | Millisecond counter, byte 1 | rw | | no | |
| 9 | Millisecond counter, byte 0 (LSB) | rw | | no | Writing any value to any of registers 6 - 9 clears the counter |
| 10 | Button data byte 0 | rw | 1 | yes | Data byte 0 of the TURN-ON button event only; START/STOP reserve byte 0 and send zero |
| 11 | Button zone | rw | 1 | yes | Zone (data byte 1) of the button events |
| 12 | Button subzone | rw | 1 | yes | Subzone (data byte 2) of the button events |
| 0x60 - 0x7F | Decision matrix | rw | 0 | yes | Four rows of eight bytes |

"Stored" registers are saved in the PIC's EEPROM about a second after they
were last changed, and are restored when the node starts.

### Control register (3)

| Bit | Default | Meaning when set |
|-----|---------|------------------|
| 7 | 1 | Blink the LED |
| 6 | 1 | The button sends events |
| 3 | 1 | Send TURN-ON when the button is pressed |
| 2 | 1 | Send START when the button is pressed |
| 1 | 1 | Send STOP when the button is released |
| 0 | 0 | Run the millisecond counter |

### Restore the defaults

Write 0x55 and then, within one second, 0xAA to standard register 0xA2. The
application registers and the decision matrix return to their defaults. The
address (nickname) is kept.

## Standard registers

The standard registers (0x80 - 0xFF) are handled by `vscp-firmware.c`. The
ones that say something about this node:

| Register | Content |
|----------|---------|
| 0x84 - 0x88 | User id, five bytes you can use as you like (stored) |
| 0x91 | Nickname = the RS-485 address |
| 0x94 - 0x96 | Firmware version (0.1.0) |
| 0x97 | Bootloader algorithm: 0xFF, the node has no bootloader |
| 0x98 | Buffer size: 8 |
| 0x99 | Register pages used: 1 |
| 0xD0 - 0xDF | GUID, most significant byte first |
| 0xE0 - 0xFF | MDF URL: `eurosource.se/blinky0.json` |

### GUID

The PIC18F25K22 has no unique id, so the GUID is set when the firmware is
built: a fixed prefix and, as the last byte, the node's RS-485 address.

```text
FF:FF:FF:FF:FF:FF:FF:F5:52:53:34:38:35:00:00:<address>
```

`FF:FF:FF:FF:FF:FF:FF:F5` is the range VSCP reserves for demos and examples;
`52:53:34:38:35` is "RS485" in ASCII. Every node on a segment has its own
address and so its own GUID. For a product, use a GUID range of your own
(`NODE_GUID_PREFIX` in `config.h`).

## The address (nickname)

On CAN a node finds a free nickname by itself. On RS-485 the master polls the
addresses it was told to poll, so the nickname of the node **is its RS-485
address** and it never changes on its own:

- It is set when the firmware is built (`make NODE_ADDRESS=5`) and stored in
  EEPROM the first time the node starts.
- CLASS1.PROTOCOL **SET_NICKNAME** (type 6) changes it to another address
  (1-254). The node answers NICKNAME_ACCEPTED from the new address and keeps
  it after a restart. Remember to add the new address to the master's poll
  range.
- **DROP_NICKNAME** (type 8) restarts the node; it comes back with the same
  address and sends NEW_NODE_ONLINE.
- Address 0 is the master and 255 is broadcast; attempts to set them are
  ignored.

## Events the node sends

| Event | Class | Type | Data | When |
|-------|-------|------|------|------|
| New node online | 0 CLASS1.PROTOCOL | 2 | address | When the node starts, and on a long (two second) press of the button |
| Heartbeat | 20 CLASS1.INFORMATION | 9 | 0, zone, subzone | Every minute |
| ON | 20 CLASS1.INFORMATION | 3 | 0, zone, subzone | The LED was turned on by a register write or a decision matrix action |
| OFF | 20 CLASS1.INFORMATION | 4 | 0, zone, subzone | The LED was turned off in the same way |
| TURN-ON | 30 CLASS1.CONTROL | 5 | byte 0, button zone, button subzone | Button pressed (control bit 3) |
| START | 20 CLASS1.INFORMATION | 25 | 0, button zone, button subzone | Button pressed (control bit 2) |
| STOP | 20 CLASS1.INFORMATION | 24 | 0, button zone, button subzone | Button released (control bit 1) |

The periodic blink does not send ON and OFF events. With the default
interval that would be two events every second from every blinky node.

The events wait in the node until the master polls it. The node holds eight
events; if the master polls too seldom, newer events are dropped (the
statistics count them).

START and STOP are button-state notifications, not CLASS1.CONTROL commands. For both events, data byte 0 is reserved and must be zero; register 10 applies only to the TURN-ON event.

The node also answers the CLASS1.PROTOCOL events addressed to it: register
read and write, page read and write, who is there, decision matrix info,
set and drop nickname, and so on. That is all done by `vscp-firmware.c`.

## Decision matrix

The decision matrix lets events on the bus trigger actions in the node,
without any programming. It has four rows in registers 0x60 - 0x7F. A row is
eight bytes:

| Byte | Name | Meaning |
|------|------|---------|
| 0 | oaddr | Nickname of the sender, used if flag bit 6 is set |
| 1 | flags | See below |
| 2 | class mask | Bits of the class that must match the class filter |
| 3 | class filter | Class to match (low eight bits) |
| 4 | type mask | Bits of the type that must match the type filter |
| 5 | type filter | Type to match |
| 6 | action | What to do |
| 7 | action parameter | Argument for the action |

Flags:

| Bit | Meaning when set |
|-----|------------------|
| 7 | The row is enabled |
| 6 | The sender must be `oaddr` |
| 4 | The zone of the event (data byte 1) must be the zone of the node, or 255 |
| 3 | The subzone of the event (data byte 2) must be the subzone of the node, or 255 |
| 1 | Bit 8 of the class mask |
| 0 | Bit 8 of the class filter |

A mask bit of zero means "do not care". Mask 0xFF and filter 30 matches class
30 only; mask 0 matches every class.

Actions:

| Code | Action | Parameter |
|------|--------|-----------|
| 0 | No operation | |
| 1 | Set the LED | 0 = off, 1 = on, 2 = toggle |
| 2 | Turn the LED on | |
| 3 | Turn the LED off | |
| 4 | Toggle the LED | |
| 5 | Start blinking (sets control bit 7) | |
| 6 | Stop blinking (clears control bit 7) | |
| 7 | Clear the millisecond counter | |

### Example

Let TURN-ON (class 30, type 5) from any node turn the LED on, and TURN-OFF
(class 30, type 6) turn it off:

| Register | Row 0 | Row 1 (register + 8) |
|----------|-------|-------|
| 0x60 oaddr | 0 | 0 |
| 0x61 flags | 0x80 | 0x80 |
| 0x62 class mask | 0xFF | 0xFF |
| 0x63 class filter | 30 | 30 |
| 0x64 type mask | 0xFF | 0xFF |
| 0x65 type filter | 5 | 6 |
| 0x66 action | 2 | 3 |
| 0x67 parameter | 0 | 0 |

Stop the blinking first (clear bit 7 of the control register), or the LED
will be toggled again by the blink function.

On RS-485 the nodes do not hear each other: a node only receives what the
master sends to its address (or to broadcast address 255). For the button
of one node to control the LED of another, the master side (for example the
VSCP daemon) must forward the events.
