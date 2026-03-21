/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: TCP Transport Protocol
 *
 * Demonstrates the TCP layer built on top of IPv4:
 *   - Listener registration (passive open)
 *   - Receive callback with data read from RX ring buffer
 *   - Active open with tcp_connect()
 *   - Sending data with tcp_send()
 *   - Graceful close with tcp_close()
 *   - RST handling for unbound ports
 *   - Built-in echo service on port 7
 *
 * This example runs on-device with the loopback link -- no remote
 * host or SLIP connection is needed.  Packets sent via tcp_send()
 * or tcp_connect() are captured by the loopback and fed back
 * through ipv4_input(), so the full state machine path is exercised.
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

#if defined(HAS_TIKUKITS) && TIKU_KITS_NET_TCP_ENABLE

#include "tikukits/net/ipv4/tiku_kits_net_tcp.h"

/*---------------------------------------------------------------------------*/
/* Loopback link (captures TX, feeds back through ipv4_input)                */
/*---------------------------------------------------------------------------*/

static uint8_t lb_buf[TIKU_KITS_NET_MTU + 20];
static uint16_t lb_len;
static uint8_t lb_feed_back;  /* 1 = feed TX packets back as RX */

static int8_t lb_send(const uint8_t *pkt, uint16_t len)
{
    if (len > sizeof(lb_buf)) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }
    memcpy(lb_buf, pkt, len);
    lb_len = len;

    if (lb_feed_back) {
        /* Swap src/dst IP so the packet passes the destination check
         * when fed back through ipv4_input */
        uint8_t tmp[4];
        memcpy(tmp, lb_buf + 12, 4);
        memcpy(lb_buf + 12, lb_buf + 16, 4);
        memcpy(lb_buf + 16, tmp, 4);

        /* Recompute IPv4 header checksum after IP swap */
        lb_buf[10] = 0; lb_buf[11] = 0;
        uint16_t chk = tiku_kits_net_ipv4_chksum(lb_buf, 20);
        lb_buf[10] = (uint8_t)(chk >> 8);
        lb_buf[11] = (uint8_t)(chk & 0xFF);

        tiku_kits_net_ipv4_input(lb_buf, lb_len);
    }
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
/* Application callback state                                                */
/*---------------------------------------------------------------------------*/

static uint8_t  recv_called;
static uint16_t recv_avail;
static uint8_t  event_type;
static uint8_t  event_called;

static void
my_recv_cb(struct tiku_kits_net_tcp_conn *conn, uint16_t available)
{
    recv_called = 1;
    recv_avail  = available;
    (void)conn;
}

static void
my_event_cb(struct tiku_kits_net_tcp_conn *conn, uint8_t event)
{
    event_called = 1;
    event_type   = event;
    (void)conn;
}

/*---------------------------------------------------------------------------*/
/* Helper: build and inject a TCP packet into ipv4_input                     */
/*---------------------------------------------------------------------------*/

static void
inject_tcp(uint16_t sport, uint16_t dport,
           uint32_t seq, uint32_t ack,
           uint8_t flags, uint16_t window,
           const uint8_t *data, uint16_t data_len,
           uint16_t mss)
{
    uint8_t pkt[TIKU_KITS_NET_MTU];
    uint8_t *tcp;
    uint16_t tcp_hdr_len;
    uint16_t tcp_total, total;
    uint16_t chk;
    uint8_t pseudo[12];
    uint32_t sum;
    uint16_t i;

    /* TCP header length */
    tcp_hdr_len = (mss > 0) ? 24 : 20;
    tcp_total = tcp_hdr_len + data_len;
    total = 20 + tcp_total;

    /* IPv4 header */
    pkt[0]  = 0x45;
    pkt[1]  = 0x00;
    pkt[2]  = (uint8_t)(total >> 8);
    pkt[3]  = (uint8_t)(total & 0xFF);
    pkt[4]  = 0x00; pkt[5] = 0x01;
    pkt[6]  = 0x00; pkt[7] = 0x00;
    pkt[8]  = 0x40;
    pkt[9]  = 6;   /* TCP */
    pkt[10] = 0x00; pkt[11] = 0x00;
    /* src: 172.16.7.1 (remote host) */
    pkt[12] = 0xAC; pkt[13] = 0x10;
    pkt[14] = 0x07; pkt[15] = 0x01;
    /* dst: 172.16.7.2 (us) */
    pkt[16] = 0xAC; pkt[17] = 0x10;
    pkt[18] = 0x07; pkt[19] = 0x02;

    /* IPv4 checksum */
    chk = tiku_kits_net_ipv4_chksum(pkt, 20);
    pkt[10] = (uint8_t)(chk >> 8);
    pkt[11] = (uint8_t)(chk & 0xFF);

    /* TCP header */
    tcp = pkt + 20;
    tcp[0]  = (uint8_t)(sport >> 8);
    tcp[1]  = (uint8_t)(sport & 0xFF);
    tcp[2]  = (uint8_t)(dport >> 8);
    tcp[3]  = (uint8_t)(dport & 0xFF);
    tcp[4]  = (uint8_t)(seq >> 24);
    tcp[5]  = (uint8_t)(seq >> 16);
    tcp[6]  = (uint8_t)(seq >> 8);
    tcp[7]  = (uint8_t)(seq);
    tcp[8]  = (uint8_t)(ack >> 24);
    tcp[9]  = (uint8_t)(ack >> 16);
    tcp[10] = (uint8_t)(ack >> 8);
    tcp[11] = (uint8_t)(ack);
    tcp[12] = (uint8_t)((tcp_hdr_len / 4) << 4);
    tcp[13] = flags;
    tcp[14] = (uint8_t)(window >> 8);
    tcp[15] = (uint8_t)(window & 0xFF);
    tcp[16] = 0; tcp[17] = 0;
    tcp[18] = 0; tcp[19] = 0;

    /* MSS option */
    if (mss > 0) {
        tcp[20] = 2; tcp[21] = 4;
        tcp[22] = (uint8_t)(mss >> 8);
        tcp[23] = (uint8_t)(mss & 0xFF);
    }

    /* Payload */
    if (data_len > 0 && data != (void *)0) {
        memcpy(tcp + tcp_hdr_len, data, data_len);
    }

    /* TCP pseudo-header checksum */
    memcpy(pseudo,     pkt + 12, 4);
    memcpy(pseudo + 4, pkt + 16, 4);
    pseudo[8]  = 0;
    pseudo[9]  = 6;
    pseudo[10] = (uint8_t)(tcp_total >> 8);
    pseudo[11] = (uint8_t)(tcp_total & 0xFF);

    sum = 0;
    for (i = 0; i < 12; i += 2) {
        sum += (uint16_t)((uint16_t)pseudo[i] << 8 | pseudo[i + 1]);
    }
    for (i = 0; i + 1 < tcp_total; i += 2) {
        sum += (uint16_t)((uint16_t)tcp[i] << 8 | tcp[i + 1]);
    }
    if (tcp_total & 1) {
        sum += (uint16_t)((uint16_t)tcp[tcp_total - 1] << 8);
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    chk = (uint16_t)(~sum & 0xFFFF);
    tcp[16] = (uint8_t)(chk >> 8);
    tcp[17] = (uint8_t)(chk & 0xFF);

    tiku_kits_net_ipv4_input(pkt, total);
}

/*---------------------------------------------------------------------------*/
/* Example 1: Listener registration                                          */
/*---------------------------------------------------------------------------*/

static void example_listen(void)
{
    int8_t rc;

    printf("--- Listener Registration ---\n");

    /* Port 7 is already taken by the built-in echo service */
    rc = tiku_kits_net_tcp_listen(7, my_recv_cb, my_event_cb);
    printf("  listen(7): %s (expected PARAM -- echo owns it)\n",
           rc == TIKU_KITS_NET_ERR_PARAM ? "ERR_PARAM" : "unexpected");

    /* Bind port 8080 */
    rc = tiku_kits_net_tcp_listen(8080, my_recv_cb, my_event_cb);
    printf("  listen(8080): %s\n",
           rc == TIKU_KITS_NET_OK ? "OK" : "FAIL");

    /* Duplicate rejected */
    rc = tiku_kits_net_tcp_listen(8080, my_recv_cb, my_event_cb);
    printf("  listen(8080) again: %s (expected PARAM)\n",
           rc == TIKU_KITS_NET_ERR_PARAM ? "ERR_PARAM" : "unexpected");

    /* Unlisten and re-listen */
    tiku_kits_net_tcp_unlisten(8080);
    rc = tiku_kits_net_tcp_listen(9090, my_recv_cb, my_event_cb);
    printf("  unlisten(8080) + listen(9090): %s\n",
           rc == TIKU_KITS_NET_OK ? "OK" : "FAIL");

    tiku_kits_net_tcp_unlisten(9090);
}

/*---------------------------------------------------------------------------*/
/* Example 2: 3-way handshake (passive open via loopback)                    */
/*---------------------------------------------------------------------------*/

static void example_handshake(void)
{
    uint32_t srv_iss;
    uint16_t sport = 40000;
    uint16_t dport = 8080;

    printf("--- 3-Way Handshake (Passive Open) ---\n");

    tiku_kits_net_tcp_listen(dport, my_recv_cb, my_event_cb);
    lb_feed_back = 0;  /* capture TX, don't feed back */
    lb_len = 0;

    /* Step 1: Inject SYN from remote host */
    event_called = 0;
    inject_tcp(sport, dport, 0x1000, 0,
               TIKU_KITS_NET_TCP_FLAG_SYN, 4096,
               (void *)0, 0, 88);

    if (lb_len > 0) {
        uint8_t *tcp = lb_buf + 20;
        uint8_t flags = tcp[13];
        srv_iss = (uint32_t)(
            (uint32_t)tcp[4] << 24 | (uint32_t)tcp[5] << 16 |
            (uint32_t)tcp[6] << 8  | (uint32_t)tcp[7]);
        printf("  SYN injected -> captured SYN+ACK\n");
        printf("  Flags: 0x%02X (SYN=%u ACK=%u)\n",
               flags,
               !!(flags & TIKU_KITS_NET_TCP_FLAG_SYN),
               !!(flags & TIKU_KITS_NET_TCP_FLAG_ACK));
        printf("  Server ISS: 0x%04lX\n", (unsigned long)srv_iss);
    } else {
        printf("  SYN injected -> no SYN+ACK captured (error)\n");
        tiku_kits_net_tcp_unlisten(dport);
        return;
    }

    /* Step 2: Inject ACK to complete handshake */
    lb_len = 0;
    inject_tcp(sport, dport, 0x1001, srv_iss + 1,
               TIKU_KITS_NET_TCP_FLAG_ACK, 4096,
               (void *)0, 0, 0);

    printf("  ACK injected -> state = %s\n",
           event_called && event_type == TIKU_KITS_NET_TCP_EVT_CONNECTED
           ? "ESTABLISHED" : "(not connected)");

    /* Clean up: abort the connection */
    /* Find the connection by injecting a RST */
    inject_tcp(sport, dport, 0x1001, 0,
               TIKU_KITS_NET_TCP_FLAG_RST, 0,
               (void *)0, 0, 0);

    tiku_kits_net_tcp_unlisten(dport);
    printf("  Connection cleaned up\n");
}

/*---------------------------------------------------------------------------*/
/* Example 3: Data transfer with echo                                        */
/*---------------------------------------------------------------------------*/

static void example_data_echo(void)
{
    uint32_t srv_iss;
    uint16_t sport = 40001;
    uint16_t dport = 7;  /* built-in echo service */
    uint8_t msg[] = "Hello TCP!";

    printf("--- Data Transfer (Echo on Port 7) ---\n");

    lb_feed_back = 0;
    lb_len = 0;

    /* SYN */
    inject_tcp(sport, dport, 0x2000, 0,
               TIKU_KITS_NET_TCP_FLAG_SYN, 4096,
               (void *)0, 0, 88);

    if (lb_len == 0) {
        printf("  Error: no SYN+ACK\n");
        return;
    }
    srv_iss = (uint32_t)(
        (uint32_t)lb_buf[24] << 24 | (uint32_t)lb_buf[25] << 16 |
        (uint32_t)lb_buf[26] << 8  | (uint32_t)lb_buf[27]);
    printf("  SYN -> SYN+ACK (srv_iss=0x%04lX)\n",
           (unsigned long)srv_iss);

    /* ACK + DATA piggybacked */
    lb_len = 0;
    inject_tcp(sport, dport, 0x2001, srv_iss + 1,
               TIKU_KITS_NET_TCP_FLAG_ACK |
               TIKU_KITS_NET_TCP_FLAG_PSH,
               4096,
               msg, sizeof(msg) - 1, 0);

    if (lb_len > 0) {
        uint8_t *tcp = lb_buf + 20;
        uint8_t doff = (tcp[12] >> 4) * 4;
        uint16_t payload_len = lb_len - 20 - doff;
        printf("  DATA -> ACK captured (%u bytes response)\n",
               (unsigned)lb_len);

        /* The echo service reads data and sends it back.
         * The first captured TX is the ACK for our data.
         * The echo data may be in the same packet or a
         * subsequent one.  Check for payload. */
        if (payload_len > 0) {
            printf("  Echo payload: \"%.*s\" (%u bytes)\n",
                   (int)payload_len,
                   (const char *)(tcp + doff),
                   (unsigned)payload_len);
        } else {
            printf("  ACK captured (echo may follow)\n");
        }
    } else {
        printf("  Error: no response to DATA\n");
    }

    /* Clean up with RST */
    inject_tcp(sport, dport, 0x2001 + sizeof(msg) - 1, 0,
               TIKU_KITS_NET_TCP_FLAG_RST, 0,
               (void *)0, 0, 0);
}

/*---------------------------------------------------------------------------*/
/* Example 4: RST for unbound port                                           */
/*---------------------------------------------------------------------------*/

static void example_rst(void)
{
    printf("--- RST for Unbound Port ---\n");

    lb_feed_back = 0;
    lb_len = 0;

    /* SYN to port 9999 (nobody listening) */
    inject_tcp(50000, 9999, 0x5000, 0,
               TIKU_KITS_NET_TCP_FLAG_SYN, 4096,
               (void *)0, 0, 88);

    if (lb_len > 0) {
        uint8_t *tcp = lb_buf + 20;
        uint8_t flags = tcp[13];
        uint32_t ack_val = (uint32_t)(
            (uint32_t)tcp[8] << 24 | (uint32_t)tcp[9] << 16 |
            (uint32_t)tcp[10] << 8 | (uint32_t)tcp[11]);
        printf("  SYN(9999) -> RST+ACK\n");
        printf("  Flags: 0x%02X (RST=%u ACK=%u)\n",
               flags,
               !!(flags & TIKU_KITS_NET_TCP_FLAG_RST),
               !!(flags & TIKU_KITS_NET_TCP_FLAG_ACK));
        printf("  ACK = 0x%04lX (SYN seq + 1)\n",
               (unsigned long)ack_val);
    } else {
        printf("  Error: no RST captured\n");
    }
}

/*---------------------------------------------------------------------------*/
/* Example 5: Active open (connect)                                          */
/*---------------------------------------------------------------------------*/

static void example_connect(void)
{
    tiku_kits_net_tcp_conn_t *conn;
    uint8_t dst[4] = {0xAC, 0x10, 0x07, 0x01};  /* 172.16.7.1 */

    printf("--- Active Open (Connect) ---\n");

    lb_feed_back = 0;
    lb_len = 0;
    event_called = 0;

    conn = tiku_kits_net_tcp_connect(
        dst, 80, 50000, my_recv_cb, my_event_cb);

    if (conn != (void *)0) {
        printf("  connect() -> connection allocated\n");
        if (lb_len > 0) {
            uint8_t *tcp = lb_buf + 20;
            uint8_t flags = tcp[13];
            printf("  Captured SYN: flags=0x%02X (SYN=%u)\n",
                   flags,
                   !!(flags & TIKU_KITS_NET_TCP_FLAG_SYN));
            printf("  Destination: %u.%u.%u.%u:%u\n",
                   lb_buf[16], lb_buf[17],
                   lb_buf[18], lb_buf[19],
                   (unsigned)((uint16_t)tcp[2] << 8 | tcp[3]));
        }
        /* Clean up */
        tiku_kits_net_tcp_abort(conn);
        printf("  Connection aborted (cleanup)\n");
    } else {
        printf("  connect() -> failed (no free slot?)\n");
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_net_tcp_run(void)
{
    printf("=== TikuKits TCP Examples ===\n\n");

    tiku_kits_net_ipv4_set_link(&loopback);
    tiku_kits_net_tcp_init();

    example_listen();
    printf("\n");

    example_handshake();
    printf("\n");

    example_data_echo();
    printf("\n");

    example_rst();
    printf("\n");

    example_connect();
    printf("\n");
}

#else /* !HAS_TIKUKITS || !TIKU_KITS_NET_TCP_ENABLE */

void example_net_tcp_run(void)
{
    /* TCP not enabled */
}

#endif
