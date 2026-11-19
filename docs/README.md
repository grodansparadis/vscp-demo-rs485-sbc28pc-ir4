# RS-485 SBC28PC-IR4 Blinky

This demo turns a Modtronix SBC28PC-IR4 with a PIC18F25K22 into a
[VSCP](https://www.vscp.org) Level I node on an RS-485 segment, as described
in [VSCP Level I over RS-485/RS-422](https://grodansparadis.github.io/vscp-doc-spec/#/./vscp_level_i_over_rs-485_rs-422).
It blinks an LED, sends button events, exposes registers and responds to
events through a four-row decision matrix. A master,
[vscpl1drv-rs485](https://github.com/grodansparadis/vscpl1drv-rs485) or
[vscpl2drv-rs485](https://github.com/grodansparadis/vscpl2drv-rs485), polls
it for events.

Start with [getting started](getting-started.md), then see the
[hardware and wiring](hardware.md), [programming](programming.md),
[firmware](firmware.md), [registers and events](registers.md), and
[testing](testing.md) pages.

The [source repository](https://github.com/grodansparadis/vscp-demo-rs485-sbc28pc-ir4)
contains the firmware and the host tests.

[Document version](./bottom-copyright.md ':include')
