# Testing

## Host tests (no hardware)

The tests in `tests/` compile the node firmware (`blinky.c`, `rs485-node.c`,
`persist.c`, `vscp-rs485.c` and the real `vscp-firmware.c`) for the PC.
`hw-stub.c` replaces `hw-pic18.c` with a simulated UART, timer, button, LED
and EEPROM, and `test_node.c` plays the master: it polls the node, sends it
events and decodes the answers.

```bash
git submodule update --init
cmake -S tests -B build-tests
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

The test is built three times: with the default frame format, in 8N1 mode
(address 9), and with fixed-length frames and the 0x07 CRC (address 200). It
checks:

- NEW_NODE_ONLINE on the first poll, then NO_EVENTS; polls for other
  addresses are not answered.
- Register reads (control register, GUID) and writes, the LED and the
  INFORMATION ON/OFF events.
- Button events (TURN-ON, START, STOP).
- A decision matrix row that turns the LED off on CLASS1.CONTROL TURN-OFF.
- Frames to the broadcast address; a poll with a bad CRC is not answered.
- The settings are written to EEPROM and read back after a restart.
- SET_NICKNAME moves the node to a new address, invalid addresses are refused,
  DROP_NICKNAME keeps the address.
- Restore defaults keeps the address.
- The heartbeat every minute.

The workflow in `.github/workflows/build.yml` runs these tests and builds the
firmware with XC8 for every push.

## On the bus

You need:

- the SBC28PC-IR4 with the firmware, an LED on RB4 and a button on RB0
  (see [hardware.md](hardware.md));
- a USB RS-485 adapter (FTDI or CH340 based adapters work);
- the master: [vscpl1drv-rs485](https://github.com/grodansparadis/vscpl1drv-rs485)
  for CANAL tools and VSCP Works, or
  [vscpl2drv-rs485](https://github.com/grodansparadis/vscpl2drv-rs485) for the
  VSCP daemon.

Wire A to A, B to B and ground to ground. With only the PC and one node, keep
the node's J3 terminator, and turn on the adapter's terminator if it has one.

### With vscp-dump and the Level I driver

```bash
vscp-dump -i canal --driver /var/lib/vscp/drivers/level1/vscpl1drv-rs485.so \
          --driver-config "/dev/ttyUSB0;115200;1-8"
```

The driver polls addresses 1-8. Power the node (or reset it) and you see:

```text
CLASS1.PROTOCOL NEW_NODE_ONLINE   from node 1   data 01
```

Then a heartbeat (CLASS1.INFORMATION, type 9) every minute, CONTROL TURN-ON and
INFORMATION START when the button is pressed, INFORMATION STOP when it is
released. A two second press sends NEW_NODE_ONLINE again.

If the node was built with other frame options, give the driver the matching
flags (see [firmware.md](firmware.md#build-options)), for example for an 8N1
node:

```bash
vscp-dump -i canal --driver .../vscpl1drv-rs485.so \
          --driver-config "/dev/ttyUSB0;115200;1-8" --flags 1
```

### With VSCP Works

1. Add a CANAL connection with the driver `vscpl1drv-rs485.so`, the
   configuration `/dev/ttyUSB0;115200;1-8` and flags 0.
2. Open a session window: the node's events appear.
3. Open a configuration window for node 1. VSCP Works reads the GUID and the
   MDF URL (`eurosource.se/blinky0.json`), loads the MDF and shows the
   registers by name.
4. Try:
   - Clear bit 7 of the control register (register 3, 0xCE → 0x4E): the LED
     stops blinking.
   - Write 2 to the status register (2): the LED goes on and the node sends
     INFORMATION ON.
   - Write 0x00 0xC8 to registers 4/5: blink every 200 ms.
   - Set bit 0 of the control register and read registers 6-9: the
     millisecond counter.

### With the VSCP daemon

Add vscpl2drv-rs485 to the daemon's configuration:

```json
{
  "enable": true,
  "name": "rs485",
  "path-driver": "/var/lib/vscp/drivers/level2/libvscpl2drv-rs485.so",
  "path-config": "/etc/vscp/rs485.json",
  "guid": "FF:FF:FF:FF:FF:FF:FF:F5:01:00:00:00:00:00:00:00"
}
```

and in `/etc/vscp/rs485.json`:

```json
{
  "device": "/dev/ttyUSB0",
  "baudrate": 115200,
  "addresses": "1-8"
}
```

The node's events then reach every client of the daemon, and clients such as
VSCP Works (over the TCP/IP link) can configure the node.

### Decision matrix example

Make TURN-ON (class 30, type 5) turn the LED on and TURN-OFF (class 30,
type 6) turn it off. Write, to node 1:

| Register | Value | | Register | Value |
|----------|-------|-|----------|-------|
| 0x60 | 0 | | 0x68 | 0 |
| 0x61 | 0x80 | | 0x69 | 0x80 |
| 0x62 | 0xFF | | 0x6A | 0xFF |
| 0x63 | 30 | | 0x6B | 30 |
| 0x64 | 0xFF | | 0x6C | 0xFF |
| 0x65 | 5 | | 0x6D | 6 |
| 0x66 | 2 | | 0x6E | 3 |
| 0x67 | 0 | | 0x6F | 0 |

and 0x4E to register 3 (stop blinking). Now send CLASS1.CONTROL TURN-ON with
data `0,0,0` to node 1 from VSCP Works: the LED goes on.

## Troubleshooting

| Symptom | Check |
|---------|-------|
| The LED does not blink | Is the PIC programmed and running (release from reset in MPLAB IPE)? Is the LED the right way round on RB4? |
| No events at all | A and B swapped? Same baud rate and frame options on both sides? Is the node's address in the master's poll range? |
| The node receives but never answers | DE//RE is not connected to the PIC: make J1 (1-2 for RA0 or 2-3 for RB2), or build with the right `DIR_PINS` |
| CRC errors, events now and then | Termination at both ends only; ground connected; bias resistors on the bus; the internal oscillator is off by more than 2 % (fit a crystal) |
| Two nodes answer, garbage | Two nodes with the same address. Give each its own `NODE_ADDRESS` |
| Settings lost after programming | MPLAB IPE erases the EEPROM; enable "Preserve EEPROM" to keep it |
