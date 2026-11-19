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
  @file rs485-node.h
  The RS-485 transport of a VSCP Level I node ("VSCP Level I over
  RS-485/RS-422").

  A node only talks when the master polls it. Events the node wants to send
  (vscp-firmware.c calls sendVSCPFrame) wait in a small queue; when the
  master polls the node's address the first one is sent, or NO_EVENTS when
  the queue is empty. EVENT frames for the node's address (or the broadcast
  address 0xFF) go to a receive queue that vscp-firmware.c reads with
  getVSCPFrame.

  The node's address is its VSCP nickname (vscp_nickname).

  The frames, CRC and receiver are in vscp-rs485.c, the same file as in the
  masters vscpl1drv-rs485 and vscpl2drv-rs485.
*/

#ifndef RS485_NODE_H
#define RS485_NODE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RS485_NODE_TXQ_SIZE 8u // Events waiting to be sent when polled
#define RS485_NODE_RXQ_SIZE 4u // Received events waiting for vscp-firmware.c

/// Statistics
typedef struct {
  uint16_t polls;      ///< Polls for this node answered
  uint16_t sent;       ///< Events sent
  uint16_t received;   ///< Events received for this node
  uint16_t crc_errors; ///< Frames with a wrong CRC (any address)
  uint16_t frame_errors;
  uint16_t tx_overruns; ///< Events dropped because the transmit queue was full
  uint16_t rx_overruns; ///< Events dropped because the receive queue was full
} rs485_node_stats_t;

void
rs485_node_init(void);

/*!
  Read the received characters and answer a poll. Call as often as
  possible from the main loop: the master expects the answer within its
  poll time-out (20 ms by default).
*/
void
rs485_node_work(void);

const rs485_node_stats_t *
rs485_node_stats(void);

/// Events waiting to be sent
uint8_t
rs485_node_tx_pending(void);

/*
  The two transport callbacks of vscp-firmware.c (declared in
  vscp-firmware.h):

    int8_t getVSCPFrame(uint16_t *pvscpclass, uint8_t *pvscptype,
                        uint8_t *pNodeId, uint8_t *pPriority,
                        uint8_t *pSize, uint8_t *pData);
    int8_t sendVSCPFrame(uint16_t vscpclass, uint8_t vscptype,
                         uint8_t nodeid, uint8_t priority,
                         uint8_t size, uint8_t *pData);
*/

#ifdef __cplusplus
}
#endif

#endif // RS485_NODE_H
