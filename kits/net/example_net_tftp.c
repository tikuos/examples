/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: TFTP Client
 *
 * Demonstrates the poll-based TFTP client:
 *   - Initiating a read request (RRQ)
 *   - Receiving DATA blocks via callback
 *   - State machine progression
 *   - Error handling and abort
 *   - Initiating a write request (WRQ) with supply callback
 *
 * Uses a loopback link with simulated server responses so the
 * complete TFTP flow can be exercised without hardware I/O.  Each
 * step prints its progress to show how the poll-based API works.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "examples/kits/example_kits_printf.h"
#include <stdint.h>
#include <string.h>

#include "tikukits/net/tiku_kits_net.h"
#include "tikukits/net/ipv4/tiku_kits_net_ipv4.h"
#include "tikukits/net/ipv4/tiku_kits_net_udp.h"
#include "tikukits/net/ipv4/tiku_kits_net_tftp.h"

#if defined(HAS_TIKUKITS)

/*---------------------------------------------------------------------------*/
/* Loopback link                                                             */
/*---------------------------------------------------------------------------*/

static uint8_t lb_buf[TIKU_KITS_NET_MTU + 20];
static uint16_t lb_len;

static int8_t lb_send(const uint8_t *pkt, uint16_t len)
{
    if (len > sizeof(lb_buf)) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }
    memcpy(lb_buf, pkt, len);
    lb_len = len;
    return TIKU_KITS_NET_OK;
}

static uint8_t lb_poll(uint8_t *b, uint16_t s, uint16_t *p)
{
    (void)b; (void)s; (void)p;
    return 0;
}

static const tiku_kits_net_link_t loopback = {
    .send = lb_send, .poll_rx = lb_poll, .name = "loopback"
};

/*---------------------------------------------------------------------------*/
/* Helper: build a simulated server DATA packet                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Build an IPv4/UDP/TFTP DATA packet from a simulated server.
 *
 * The packet looks like it came from 172.16.7.1:5000 (server TID)
 * to 172.16.7.2:49152 (our TFTP client port).
 */
static uint16_t
build_server_data(uint8_t *buf, uint16_t block,
                  const uint8_t *data, uint16_t data_len)
{
    uint16_t tftp_len = 4 + data_len;
    uint16_t udp_len  = 8 + tftp_len;
    uint16_t total    = 20 + udp_len;
    uint16_t chk;

    /* IPv4 header */
    buf[0]  = 0x45;
    buf[1]  = 0x00;
    buf[2]  = (uint8_t)(total >> 8);
    buf[3]  = (uint8_t)(total & 0xFF);
    buf[4]  = 0x00; buf[5] = 0x01;
    buf[6]  = 0x00; buf[7] = 0x00;
    buf[8]  = 0x40;
    buf[9]  = 17;   /* UDP */
    buf[10] = 0x00; buf[11] = 0x00;
    buf[12] = 0xAC; buf[13] = 0x10;
    buf[14] = 0x07; buf[15] = 0x01;  /* src: 172.16.7.1 */
    buf[16] = 0xAC; buf[17] = 0x10;
    buf[18] = 0x07; buf[19] = 0x02;  /* dst: 172.16.7.2 */

    chk = tiku_kits_net_ipv4_chksum(buf, 20);
    buf[10] = (uint8_t)(chk >> 8);
    buf[11] = (uint8_t)(chk & 0xFF);

    /* UDP header: sport=5000, dport=49152 */
    buf[20] = 0x13; buf[21] = 0x88;  /* 5000 */
    buf[22] = 0xC0; buf[23] = 0x00;  /* 49152 */
    buf[24] = (uint8_t)(udp_len >> 8);
    buf[25] = (uint8_t)(udp_len & 0xFF);
    buf[26] = 0x00; buf[27] = 0x00;  /* checksum: skip */

    /* TFTP DATA: opcode=3, block, data */
    buf[28] = 0x00; buf[29] = 0x03;
    buf[30] = (uint8_t)(block >> 8);
    buf[31] = (uint8_t)(block & 0xFF);
    if (data != NULL && data_len > 0) {
        memcpy(buf + 32, data, data_len);
    }

    return total;
}

/*---------------------------------------------------------------------------*/
/* Callbacks                                                                 */
/*---------------------------------------------------------------------------*/

static uint16_t total_received;

static uint16_t
my_data_cb(uint16_t block, const uint8_t *data, uint16_t len)
{
    printf("    Block %u: received %u bytes", block, len);
    if (len > 0 && len <= 8) {
        uint16_t i;
        printf(" [");
        for (i = 0; i < len; i++) {
            printf("0x%02X", data[i]);
            if (i < len - 1) {
                printf(" ");
            }
        }
        printf("]");
    }
    printf("\n");
    total_received += len;
    return len;
}

static uint16_t
my_supply_cb(uint16_t block, uint8_t *buf, uint16_t max_len)
{
    /* Supply 8 bytes for each block, last block < max signals EOF */
    uint16_t len = 8;
    uint16_t i;

    if (block > 2) {
        return 0;  /* no more data */
    }

    for (i = 0; i < len; i++) {
        buf[i] = (uint8_t)(block * 10 + i);
    }
    printf("    Supplied block %u: %u bytes\n", block, len);
    return len;
}

/*---------------------------------------------------------------------------*/
/* Helper: print state name                                                  */
/*---------------------------------------------------------------------------*/

static const char *state_name(tiku_kits_net_tftp_state_t s)
{
    switch (s) {
    case TIKU_KITS_NET_TFTP_STATE_IDLE:         return "IDLE";
    case TIKU_KITS_NET_TFTP_STATE_RRQ_SENT:     return "RRQ_SENT";
    case TIKU_KITS_NET_TFTP_STATE_WRQ_SENT:     return "WRQ_SENT";
    case TIKU_KITS_NET_TFTP_STATE_TRANSFERRING:  return "TRANSFERRING";
    case TIKU_KITS_NET_TFTP_STATE_SENDING:       return "SENDING";
    case TIKU_KITS_NET_TFTP_STATE_COMPLETE:      return "COMPLETE";
    case TIKU_KITS_NET_TFTP_STATE_ERROR:         return "ERROR";
    default:                                      return "?";
    }
}

/*---------------------------------------------------------------------------*/
/* Example 1: Read a file (RRQ) with simulated server                        */
/*---------------------------------------------------------------------------*/

static void example_rrq(void)
{
    uint8_t srv[4] = {0xAC, 0x10, 0x07, 0x01};
    uint8_t pkt[TIKU_KITS_NET_MTU];
    uint8_t block_data[8] = {0xAA, 0xBB, 0xCC, 0xDD,
                              0xEE, 0xFF, 0x11, 0x22};
    uint16_t len;
    int8_t rc;
    tiku_kits_net_tftp_evt_t evt;

    printf("--- Example 1: Read File (RRQ) ---\n");

    tiku_kits_net_ipv4_set_link(&loopback);
    tiku_kits_net_udp_init();
    tiku_kits_net_tftp_init();

    total_received = 0;
    rc = tiku_kits_net_tftp_get(srv, "hello.txt", my_data_cb);
    printf("  get(\"hello.txt\"): %s\n",
           rc == TIKU_KITS_NET_OK ? "OK" : "FAIL");
    printf("  State: %s\n",
           state_name(tiku_kits_net_tftp_get_state()));

    /* Simulate server sending DATA block 1 (8 bytes, < 96 = last) */
    printf("  Simulating server DATA(1) with 8 bytes...\n");
    len = build_server_data(pkt, 1, block_data, 8);
    tiku_kits_net_ipv4_input(pkt, len);

    /* Poll to process */
    evt = tiku_kits_net_tftp_poll();
    printf("  Poll returned: %s\n",
           evt == TIKU_KITS_NET_TFTP_EVT_COMPLETE ? "COMPLETE" :
           evt == TIKU_KITS_NET_TFTP_EVT_DATA_READY ? "DATA_READY" :
           "other");
    printf("  State: %s\n",
           state_name(tiku_kits_net_tftp_get_state()));
    printf("  Total received: %u bytes\n", total_received);
}

/*---------------------------------------------------------------------------*/
/* Example 2: Multi-block read                                               */
/*---------------------------------------------------------------------------*/

static void example_multi_block(void)
{
    uint8_t srv[4] = {0xAC, 0x10, 0x07, 0x01};
    uint8_t pkt[TIKU_KITS_NET_MTU];
    uint8_t data[96];
    uint16_t len, i;
    tiku_kits_net_tftp_evt_t evt;

    printf("--- Example 2: Multi-Block Read ---\n");

    tiku_kits_net_ipv4_set_link(&loopback);
    tiku_kits_net_udp_init();
    tiku_kits_net_tftp_init();
    total_received = 0;

    tiku_kits_net_tftp_get(srv, "big.bin", my_data_cb);

    /* Block 1: full 96 bytes */
    for (i = 0; i < 96; i++) {
        data[i] = (uint8_t)i;
    }
    printf("  Sending block 1 (96 bytes, full)...\n");
    len = build_server_data(pkt, 1, data, 96);
    tiku_kits_net_ipv4_input(pkt, len);
    evt = tiku_kits_net_tftp_poll();
    printf("  State: %s\n",
           state_name(tiku_kits_net_tftp_get_state()));

    /* Block 2: 32 bytes (short = last block) */
    printf("  Sending block 2 (32 bytes, last)...\n");
    len = build_server_data(pkt, 2, data, 32);
    tiku_kits_net_ipv4_input(pkt, len);
    evt = tiku_kits_net_tftp_poll();
    printf("  State: %s\n",
           state_name(tiku_kits_net_tftp_get_state()));
    printf("  Total received: %u bytes\n", total_received);

    (void)evt;
}

/*---------------------------------------------------------------------------*/
/* Example 3: Error handling                                                 */
/*---------------------------------------------------------------------------*/

static void example_error(void)
{
    uint8_t srv[4] = {0xAC, 0x10, 0x07, 0x01};
    uint8_t pkt[TIKU_KITS_NET_MTU];
    uint16_t total, chk;
    tiku_kits_net_tftp_evt_t evt;

    printf("--- Example 3: Error Handling ---\n");

    tiku_kits_net_ipv4_set_link(&loopback);
    tiku_kits_net_udp_init();
    tiku_kits_net_tftp_init();

    tiku_kits_net_tftp_get(srv, "missing.bin", my_data_cb);
    printf("  Requested \"missing.bin\"\n");

    /* Simulate server ERROR (code 1 = file not found) */
    total = 20 + 8 + 5;  /* IPv4 + UDP + ERROR(opcode+code+NUL) */

    pkt[0]  = 0x45; pkt[1] = 0x00;
    pkt[2]  = (uint8_t)(total >> 8);
    pkt[3]  = (uint8_t)(total & 0xFF);
    pkt[4]  = 0; pkt[5] = 1; pkt[6] = 0; pkt[7] = 0;
    pkt[8]  = 0x40; pkt[9] = 17;
    pkt[10] = 0; pkt[11] = 0;
    pkt[12] = 0xAC; pkt[13] = 0x10;
    pkt[14] = 0x07; pkt[15] = 0x01;
    pkt[16] = 0xAC; pkt[17] = 0x10;
    pkt[18] = 0x07; pkt[19] = 0x02;
    chk = tiku_kits_net_ipv4_chksum(pkt, 20);
    pkt[10] = (uint8_t)(chk >> 8);
    pkt[11] = (uint8_t)(chk & 0xFF);

    pkt[20] = 0x13; pkt[21] = 0x88;  /* sport=5000 */
    pkt[22] = 0xC0; pkt[23] = 0x00;  /* dport=49152 */
    pkt[24] = 0; pkt[25] = 13;       /* UDP len */
    pkt[26] = 0; pkt[27] = 0;

    /* TFTP ERROR: opcode=5, code=1, msg="" + NUL */
    pkt[28] = 0; pkt[29] = 5;   /* opcode = ERROR */
    pkt[30] = 0; pkt[31] = 1;   /* error code = 1 */
    pkt[32] = 0;                 /* empty message NUL */

    tiku_kits_net_ipv4_input(pkt, total);
    evt = tiku_kits_net_tftp_poll();

    printf("  Server responded with ERROR\n");
    printf("  State:      %s\n",
           state_name(tiku_kits_net_tftp_get_state()));
    printf("  Error code: %u (file not found)\n",
           tiku_kits_net_tftp_get_error());

    (void)evt;
}

/*---------------------------------------------------------------------------*/
/* Example 4: Abort a transfer                                               */
/*---------------------------------------------------------------------------*/

static void example_abort(void)
{
    uint8_t srv[4] = {0xAC, 0x10, 0x07, 0x01};

    printf("--- Example 4: Abort ---\n");

    tiku_kits_net_ipv4_set_link(&loopback);
    tiku_kits_net_udp_init();
    tiku_kits_net_tftp_init();

    tiku_kits_net_tftp_get(srv, "large.bin", my_data_cb);
    printf("  Started transfer, state: %s\n",
           state_name(tiku_kits_net_tftp_get_state()));

    tiku_kits_net_tftp_abort();
    printf("  After abort, state: %s\n",
           state_name(tiku_kits_net_tftp_get_state()));

    /* Can start a new transfer after abort */
    {
        int8_t rc = tiku_kits_net_tftp_get(
            srv, "retry.bin", my_data_cb);
        printf("  New get() after abort: %s\n",
               rc == TIKU_KITS_NET_OK ? "OK" : "FAIL");
        tiku_kits_net_tftp_abort();
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_net_tftp_run(void)
{
    printf("=== TikuKits TFTP Client Examples ===\n\n");

    example_rrq();
    printf("\n");

    example_multi_block();
    printf("\n");

    example_error();
    printf("\n");

    example_abort();
    printf("\n");
}

#endif /* HAS_TIKUKITS */
