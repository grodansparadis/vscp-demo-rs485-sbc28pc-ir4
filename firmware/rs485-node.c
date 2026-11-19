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
  @file rs485-node.c
  The RS-485 transport of the node. See rs485-node.h.
*/

#include "rs485-node.h"

#include "config.h"
#include "hw.h"
#include "vscp-rs485.h"

#include "vscp-compiler.h"
#include "vscp-projdefs.h"

#include <string.h>
#include <vscp-firmware.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

// Without the ninth bit a pause this long between characters ends a frame
#define RS485_GAP_MS 3u

// Priority of received events (the bus does not carry it)
#define RS485_RX_PRIORITY 3u

static vscp_rs485_rx_t s_rx;
static uint16_t s_last_char_ms;

static vscp_rs485_frame_t s_txq[RS485_NODE_TXQ_SIZE];
static uint8_t s_txq_head; // Next to send
static uint8_t s_txq_count;

static vscp_rs485_frame_t s_rxq[RS485_NODE_RXQ_SIZE];
static uint8_t s_rxq_head; // Next to read
static uint8_t s_rxq_count;

static rs485_node_stats_t s_stats;

///////////////////////////////////////////////////////////////////////////////
// rs485_node_init
//

void
rs485_node_init(void)
{
  vscp_rs485_options_t opt;
  opt.crc   = RS485_CRC_ATM ? VSCP_RS485_CRC_ATM : VSCP_RS485_CRC_DOW;
  opt.fixed = RS485_FIXED_LENGTH ? 1 : 0;
  vscp_rs485_rx_init(&s_rx, &opt);

  s_txq_head  = 0;
  s_txq_count = 0;
  s_rxq_head  = 0;
  s_rxq_count = 0;
  memset(&s_stats, 0, sizeof(s_stats));
  s_last_char_ms = hw_millis();
}

const rs485_node_stats_t *
rs485_node_stats(void)
{
  return &s_stats;
}

uint8_t
rs485_node_tx_pending(void)
{
  return s_txq_count;
}

///////////////////////////////////////////////////////////////////////////////
// answer_poll
//
// Send the oldest waiting event, or NO_EVENTS.
//

static void
answer_poll(void)
{
  vscp_rs485_frame_t answer;
  uint8_t buf[VSCP_RS485_MAX_FRAME];

  if (s_txq_count) {
    answer     = s_txq[s_txq_head];
    s_txq_head = (uint8_t) ((s_txq_head + 1u) % RS485_NODE_TXQ_SIZE);
    s_txq_count--;
    s_stats.sent++;
  }
  else {
    memset(&answer, 0, sizeof(answer));
    answer.op = VSCP_RS485_OP_NO_EVENTS;
  }
  answer.dest = VSCP_RS485_ADDR_MASTER;
  answer.src  = vscp_nickname;

  const size_t len = vscp_rs485_encode(&s_rx.opt, &answer, buf);
  if (len) {
    hw_rs485_send(buf, (uint8_t) len);
  }
  s_stats.polls++;
}

///////////////////////////////////////////////////////////////////////////////
// handle_frame
//

static void
handle_frame(const vscp_rs485_frame_t *pframe)
{
  const uint8_t me = vscp_nickname;

  if ((VSCP_RS485_OP_POLL == pframe->op) && (pframe->dest == me) && (VSCP_RS485_ADDR_MASTER == pframe->src)) {
    answer_poll();
    return;
  }

  if ((VSCP_RS485_OP_EVENT == pframe->op) && ((pframe->dest == me) || (VSCP_RS485_ADDR_BROADCAST == pframe->dest)) &&
      (pframe->src != me)) {
    if (s_rxq_count >= RS485_NODE_RXQ_SIZE) {
      s_stats.rx_overruns++;
      return;
    }
    s_rxq[(uint8_t) ((s_rxq_head + s_rxq_count) % RS485_NODE_RXQ_SIZE)] = *pframe;
    s_rxq_count++;
    s_stats.received++;
  }
  // Everything else is traffic between the master and other nodes
}

///////////////////////////////////////////////////////////////////////////////
// rs485_node_work
//

void
rs485_node_work(void)
{
  uint8_t c;
  int8_t mark;
  vscp_rs485_frame_t frame;

#if !RS485_NINE_BIT
  // Without the ninth bit frames are separated by pauses
  if (s_rx.pos && ((uint16_t) (hw_millis() - s_last_char_ms) > RS485_GAP_MS)) {
    vscp_rs485_rx_reset(&s_rx);
    s_stats.frame_errors++;
  }
#endif

  while (hw_rs485_read(&c, &mark)) {
    s_last_char_ms = hw_millis();
    switch (vscp_rs485_rx_char(&s_rx, c, mark, &frame)) {
      case VSCP_RS485_RX_FRAME:
        handle_frame(&frame);
        break;
      case VSCP_RS485_RX_ERR_CRC:
        s_stats.crc_errors++;
        break;
      case VSCP_RS485_RX_ERR_FORMAT:
      case VSCP_RS485_RX_ERR_RESYNC:
        s_stats.frame_errors++;
        break;
      default:
        break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// getVSCPFrame
//
// Callback for vscp-firmware.c. Fetch the next received event, if any.
//

int8_t
getVSCPFrame(uint16_t *pvscpclass,
             uint8_t *pvscptype,
             uint8_t *pNodeId,
             uint8_t *pPriority,
             uint8_t *pSize,
             uint8_t *pData)
{
  if (0 == s_rxq_count) {
    return FALSE;
  }

  const vscp_rs485_frame_t *pframe = &s_rxq[s_rxq_head];
  *pvscpclass                      = pframe->vscp_class;
  *pvscptype                       = pframe->vscp_type;
  *pNodeId                         = pframe->src;
  *pPriority                       = RS485_RX_PRIORITY;
  *pSize                           = pframe->size;
  memcpy(pData, pframe->data, pframe->size);

  s_rxq_head = (uint8_t) ((s_rxq_head + 1u) % RS485_NODE_RXQ_SIZE);
  s_rxq_count--;
  return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// sendVSCPFrame
//
// Callback for vscp-firmware.c. Queue an event; it is sent when the master
// polls this node.
//

int8_t
sendVSCPFrame(uint16_t vscpclass, uint8_t vscptype, uint8_t nodeid, uint8_t priority, uint8_t size, uint8_t *pData)
{
  (void) nodeid;   // The node's address is added when the event is sent
  (void) priority; // Not carried on RS-485

  if ((size > 8u) || (vscpclass > 511u)) {
    return FALSE;
  }
  if (s_txq_count >= RS485_NODE_TXQ_SIZE) {
    s_stats.tx_overruns++;
    return FALSE;
  }

  vscp_rs485_frame_t *pframe = &s_txq[(uint8_t) ((s_txq_head + s_txq_count) % RS485_NODE_TXQ_SIZE)];
  memset(pframe, 0, sizeof(*pframe));
  pframe->op         = VSCP_RS485_OP_EVENT;
  pframe->vscp_class = vscpclass;
  pframe->vscp_type  = vscptype;
  pframe->size       = size;
  if (size && (NULL != pData)) {
    memcpy(pframe->data, pData, size);
  }
  s_txq_count++;
  return TRUE;
}
