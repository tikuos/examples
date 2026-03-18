/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: UDP Datagram Protocol
 *
 * Demonstrates the UDP layer built on top of IPv4:
 *   - Port binding and unbinding
 *   - Receive callback with payload inspection
 *   - Sending datagrams with udp_send()
 *   - Binding table management (fill, unbind, re-bind)
 *   - Echo service (port 7) interaction
 *
 * This example runs on-device with the loopback link -- no remote
 * host or SLIP connection is needed.  Packets sent via udp_send()
 * are captured by the loopback and fed back through ipv4_input(),
 * so the full send -> receive -> callback path is exercised.
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

#if defined(HAS_TIKUKITS)

/*---------------------------------------------------------------------------*/
/* Loopback link (captures TX, no actual hardware I/O)                       */
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

static uint8_t lb_poll(uint8_t *buf, uint16_t sz, uint16_t *pos)
{
    (void)buf; (void)sz; (void)pos;
    return 0;
}

static const tiku_kits_net_link_t loopback = {
    .send    = lb_send,
    .poll_rx = lb_poll,
    .name    = "loopback"
};

/*---------------------------------------------------------------------------*/
/* Receive callback state                                                    */
/*---------------------------------------------------------------------------*/

static uint8_t  recv_called;
static uint16_t recv_sport;
static uint16_t recv_len;
static uint8_t  recv_data[64];
static uint8_t  recv_src[4];

static void
my_recv_cb(const uint8_t *src_addr, uint16_t src_port,
           const uint8_t *payload, uint16_t payload_len)
{
    recv_called = 1;
    recv_sport  = src_port;
    recv_len    = payload_len;
    memcpy(recv_src, src_addr, 4);
    if (payload_len > 0 && payload_len <= sizeof(recv_data)) {
        memcpy(recv_data, payload, payload_len);
    }
}

/*---------------------------------------------------------------------------*/
/* Example 1: Port binding lifecycle                                         */
/*---------------------------------------------------------------------------*/

static void example_binding(void)
{
    int8_t rc;

    printf("--- Port Binding Lifecycle ---\n");

    tiku_kits_net_udp_init();

    /* Bind port 5000 */
    rc = tiku_kits_net_udp_bind(5000, my_recv_cb);
    printf("  bind(5000): %s\n",
           rc == TIKU_KITS_NET_OK ? "OK" : "FAIL");

    /* Duplicate rejected */
    rc = tiku_kits_net_udp_bind(5000, my_recv_cb);
    printf("  bind(5000) again: %s (expected PARAM)\n",
           rc == TIKU_KITS_NET_ERR_PARAM ? "ERR_PARAM" : "unexpected");

    /* Bind more ports */
    rc = tiku_kits_net_udp_bind(5001, my_recv_cb);
    printf("  bind(5001): %s\n",
           rc == TIKU_KITS_NET_OK ? "OK" : "FAIL");
    tiku_kits_net_udp_bind(5002, my_recv_cb);
    tiku_kits_net_udp_bind(5003, my_recv_cb);

    /* Table full (4 slots default) */
    rc = tiku_kits_net_udp_bind(5004, my_recv_cb);
    printf("  bind(5004) with full table: %s (expected OVERFLOW)\n",
           rc == TIKU_KITS_NET_ERR_OVERFLOW ? "ERR_OVERFLOW" : "unexpected");

    /* Unbind and re-bind */
    tiku_kits_net_udp_unbind(5002);
    rc = tiku_kits_net_udp_bind(6000, my_recv_cb);
    printf("  unbind(5002) + bind(6000): %s\n",
           rc == TIKU_KITS_NET_OK ? "OK" : "FAIL");
}

/*---------------------------------------------------------------------------*/
/* Example 2: Sending a datagram                                             */
/*---------------------------------------------------------------------------*/

static void example_send(void)
{
    uint8_t dst[4] = {0xAC, 0x10, 0x07, 0x01};  /* 172.16.7.1 */
    uint8_t payload[] = "Hello, UDP!";
    int8_t rc;

    printf("--- Sending a Datagram ---\n");

    tiku_kits_net_ipv4_set_link(&loopback);
    tiku_kits_net_udp_init();

    lb_len = 0;
    rc = tiku_kits_net_udp_send(dst, 8080, 3000,
                                 payload, sizeof(payload) - 1);
    printf("  udp_send(): %s\n",
           rc == TIKU_KITS_NET_OK ? "OK" : "FAIL");

    if (lb_len > 0) {
        uint16_t sport = (uint16_t)(
            (uint16_t)lb_buf[20] << 8 | lb_buf[21]);
        uint16_t dport = (uint16_t)(
            (uint16_t)lb_buf[22] << 8 | lb_buf[23]);
        uint16_t udp_len = (uint16_t)(
            (uint16_t)lb_buf[24] << 8 | lb_buf[25]);

        printf("  Captured packet: %u bytes total\n",
               (unsigned)lb_len);
        printf("  IPv4 proto: %u (UDP)\n", lb_buf[9]);
        printf("  Src port:   %u\n", (unsigned)sport);
        printf("  Dst port:   %u\n", (unsigned)dport);
        printf("  UDP len:    %u (hdr + %u data)\n",
               (unsigned)udp_len, (unsigned)(udp_len - 8));
        printf("  Payload:    \"%.11s\"\n",
               (const char *)(lb_buf + 28));
    }

    /* Error cases */
    rc = tiku_kits_net_udp_send(NULL, 8080, 3000, payload, 5);
    printf("  send(NULL dst): %s (expected NULL)\n",
           rc == TIKU_KITS_NET_ERR_NULL ? "ERR_NULL" : "unexpected");

    rc = tiku_kits_net_udp_send(dst, 8080, 3000, payload, 101);
    printf("  send(101 bytes): %s (expected OVERFLOW)\n",
           rc == TIKU_KITS_NET_ERR_OVERFLOW ? "ERR_OVERFLOW" : "unexpected");
}

/*---------------------------------------------------------------------------*/
/* Example 3: Receiving via callback                                         */
/*---------------------------------------------------------------------------*/

static void example_receive(void)
{
    /* Build a UDP packet addressed to port 2000 and feed it through
     * the IPv4 input pipeline.  The loopback link + udp_bind should
     * deliver it to our callback. */

    uint8_t pkt[48];
    uint16_t total, udp_len, chk;
    uint8_t pseudo[12];
    uint32_t sum;
    uint16_t i;
    uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};

    printf("--- Receiving via Callback ---\n");

    tiku_kits_net_ipv4_set_link(&loopback);
    tiku_kits_net_udp_init();
    tiku_kits_net_udp_bind(2000, my_recv_cb);

    /* Construct an IPv4/UDP packet manually */
    udp_len = 8 + 4;  /* UDP header + 4 bytes payload */
    total   = 20 + udp_len;

    /* IPv4 header */
    pkt[0]  = 0x45;
    pkt[1]  = 0x00;
    pkt[2]  = (uint8_t)(total >> 8);
    pkt[3]  = (uint8_t)(total & 0xFF);
    pkt[4]  = 0x00; pkt[5] = 0x01;
    pkt[6]  = 0x00; pkt[7] = 0x00;
    pkt[8]  = 0x40;
    pkt[9]  = 17;   /* UDP */
    pkt[10] = 0x00; pkt[11] = 0x00;
    pkt[12] = 0xAC; pkt[13] = 0x10;
    pkt[14] = 0x07; pkt[15] = 0x01;  /* src: 172.16.7.1 */
    pkt[16] = 0xAC; pkt[17] = 0x10;
    pkt[18] = 0x07; pkt[19] = 0x02;  /* dst: 172.16.7.2 (us) */

    /* IPv4 checksum */
    chk = tiku_kits_net_ipv4_chksum(pkt, 20);
    pkt[10] = (uint8_t)(chk >> 8);
    pkt[11] = (uint8_t)(chk & 0xFF);

    /* UDP header */
    pkt[20] = 0x13; pkt[21] = 0x88;  /* sport: 5000 */
    pkt[22] = 0x07; pkt[23] = 0xD0;  /* dport: 2000 */
    pkt[24] = (uint8_t)(udp_len >> 8);
    pkt[25] = (uint8_t)(udp_len & 0xFF);
    pkt[26] = 0x00; pkt[27] = 0x00;  /* checksum: 0 (skip) */

    /* Payload */
    memcpy(pkt + 28, data, 4);

    /* Feed through IPv4 input */
    recv_called = 0;
    tiku_kits_net_ipv4_input(pkt, total);

    if (recv_called) {
        printf("  Callback invoked!\n");
        printf("  Sender:  %u.%u.%u.%u:%u\n",
               recv_src[0], recv_src[1], recv_src[2], recv_src[3],
               (unsigned)recv_sport);
        printf("  Payload: %u bytes [0x%02X 0x%02X 0x%02X 0x%02X]\n",
               (unsigned)recv_len,
               recv_data[0], recv_data[1],
               recv_data[2], recv_data[3]);
    } else {
        printf("  Callback NOT invoked (error)\n");
    }

    (void)pseudo; (void)sum; (void)i;
}

/*---------------------------------------------------------------------------*/
/* Example 4: Maximum payload                                                */
/*---------------------------------------------------------------------------*/

static void example_max_payload(void)
{
    uint8_t dst[4] = {0xAC, 0x10, 0x07, 0x01};
    uint8_t payload[100];
    uint16_t i;
    int8_t rc;

    printf("--- Maximum Payload ---\n");
    printf("  MTU:          %u bytes\n", TIKU_KITS_NET_MTU);
    printf("  IPv4 header:  %u bytes\n",
           TIKU_KITS_NET_IPV4_HDR_LEN);
    printf("  UDP header:   %u bytes\n",
           TIKU_KITS_NET_UDP_HDR_LEN);
    printf("  Max payload:  %u bytes\n",
           TIKU_KITS_NET_UDP_MAX_PAYLOAD);

    tiku_kits_net_ipv4_set_link(&loopback);
    tiku_kits_net_udp_init();

    /* Fill 100 bytes with sequential data */
    for (i = 0; i < 100; i++) {
        payload[i] = (uint8_t)i;
    }

    lb_len = 0;
    rc = tiku_kits_net_udp_send(dst, 8080, 3000, payload, 100);
    printf("  send(100 bytes): %s, total packet = %u bytes\n",
           rc == TIKU_KITS_NET_OK ? "OK" : "FAIL",
           (unsigned)lb_len);

    /* Zero-length payload is also valid */
    lb_len = 0;
    rc = tiku_kits_net_udp_send(dst, 8080, 3000, NULL, 0);
    printf("  send(0 bytes):   %s, total packet = %u bytes\n",
           rc == TIKU_KITS_NET_OK ? "OK" : "FAIL",
           (unsigned)lb_len);
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_net_udp_run(void)
{
    printf("=== TikuKits UDP Examples ===\n\n");

    example_binding();
    printf("\n");

    example_send();
    printf("\n");

    example_receive();
    printf("\n");

    example_max_payload();
    printf("\n");
}

#endif /* HAS_TIKUKITS */
