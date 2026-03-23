/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: DNS Stub Resolver
 *
 * Demonstrates the DNS resolver built on top of UDP/IPv4:
 *   - DNS initialisation and server configuration
 *   - Hostname encoding into DNS label format
 *   - Query construction and wire-format verification
 *   - Response injection via loopback (simulated server reply)
 *   - Poll-based state machine (SENT -> DONE)
 *   - Cache hit (repeated resolve returns immediately)
 *   - NXDOMAIN error handling
 *   - Cache flush and re-resolve
 *
 * This example runs on-device with a loopback link -- no remote
 * DNS server or network connection is needed.  The loopback
 * captures the outgoing UDP query, the example then constructs
 * a matching DNS response and injects it through ipv4_input().
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
#include "tikukits/net/ipv4/tiku_kits_net_dns.h"

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
/* Helper: extract the DNS transaction ID from a captured packet             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Extract DNS txn_id from a captured IPv4+UDP+DNS packet.
 *
 * Layout: IPv4 header (20 B) + UDP header (8 B) + DNS header.
 * The transaction ID is the first two bytes of the DNS header
 * (offset 28-29 in the raw packet).
 *
 * @param pkt  Raw captured packet
 * @return 16-bit transaction ID in host byte order
 */
static uint16_t get_dns_txn_id(const uint8_t *pkt)
{
    return (uint16_t)((uint16_t)pkt[28] << 8 | pkt[29]);
}

/*---------------------------------------------------------------------------*/
/* Helper: build and inject a DNS response via ipv4_input                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Build a DNS A-record response and inject it through
 *        ipv4_input as if it came from the DNS server.
 *
 * Constructs: IPv4 header (20 B) + UDP header (8 B) + DNS
 * response (header + question + answer).
 *
 * @param txn_id     Transaction ID to match the query
 * @param hostname   Hostname in dot-notation (e.g. "pool.ntp.org")
 * @param addr       4-byte IPv4 address for the answer
 * @param ttl        TTL for the A record (seconds)
 * @param rcode      DNS response code (0 = OK, 3 = NXDOMAIN)
 */
static void
inject_dns_response(uint16_t txn_id, const char *hostname,
                    const uint8_t *addr, uint32_t ttl,
                    uint8_t rcode)
{
    uint8_t pkt[TIKU_KITS_NET_MTU];
    uint8_t *udp;
    uint8_t *dns;
    uint16_t dns_off;
    uint16_t qname_len;
    uint16_t ans_count;
    uint16_t dns_len;
    uint16_t udp_len;
    uint16_t total;
    uint16_t chk;
    uint16_t i;
    const char *p;
    const char *dot;

    memset(pkt, 0, sizeof(pkt));

    /* DNS header starts at offset 28 (20 IPv4 + 8 UDP) */
    dns = pkt + 28;
    dns_off = 0;

    /* Transaction ID */
    dns[dns_off++] = (uint8_t)(txn_id >> 8);
    dns[dns_off++] = (uint8_t)(txn_id & 0xFF);

    /* Flags: QR=1, RD=1, RCODE */
    {
        uint16_t flags = TIKU_KITS_NET_DNS_FLAG_QR |
                         TIKU_KITS_NET_DNS_FLAG_RD |
                         (rcode & TIKU_KITS_NET_DNS_RCODE_MASK);
        dns[dns_off++] = (uint8_t)(flags >> 8);
        dns[dns_off++] = (uint8_t)(flags & 0xFF);
    }

    /* QDCOUNT = 1 */
    dns[dns_off++] = 0;
    dns[dns_off++] = 1;

    /* ANCOUNT: 1 if OK, 0 if NXDOMAIN */
    ans_count = (rcode == TIKU_KITS_NET_DNS_RCODE_OK && addr)
                ? 1 : 0;
    dns[dns_off++] = 0;
    dns[dns_off++] = (uint8_t)ans_count;

    /* NSCOUNT = 0, ARCOUNT = 0 */
    dns[dns_off++] = 0; dns[dns_off++] = 0;
    dns[dns_off++] = 0; dns[dns_off++] = 0;

    /* Question section: encode hostname as DNS labels */
    p = hostname;
    while (*p) {
        dot = p;
        while (*dot && *dot != '.') {
            dot++;
        }
        qname_len = (uint16_t)(dot - p);
        dns[dns_off++] = (uint8_t)qname_len;
        memcpy(dns + dns_off, p, qname_len);
        dns_off += qname_len;
        p = *dot ? dot + 1 : dot;
    }
    dns[dns_off++] = 0;  /* Root label */

    /* QTYPE = A (1), QCLASS = IN (1) */
    dns[dns_off++] = 0;
    dns[dns_off++] = TIKU_KITS_NET_DNS_QTYPE_A;
    dns[dns_off++] = 0;
    dns[dns_off++] = TIKU_KITS_NET_DNS_QCLASS_IN;

    /* Answer section (only if RCODE == OK) */
    if (ans_count > 0) {
        /* NAME: pointer to offset 12 in DNS header (0xC00C) */
        dns[dns_off++] = 0xC0;
        dns[dns_off++] = 0x0C;

        /* TYPE = A (1) */
        dns[dns_off++] = 0;
        dns[dns_off++] = TIKU_KITS_NET_DNS_QTYPE_A;

        /* CLASS = IN (1) */
        dns[dns_off++] = 0;
        dns[dns_off++] = TIKU_KITS_NET_DNS_QCLASS_IN;

        /* TTL (4 bytes, big-endian) */
        dns[dns_off++] = (uint8_t)(ttl >> 24);
        dns[dns_off++] = (uint8_t)(ttl >> 16);
        dns[dns_off++] = (uint8_t)(ttl >> 8);
        dns[dns_off++] = (uint8_t)(ttl);

        /* RDLENGTH = 4 (IPv4 address) */
        dns[dns_off++] = 0;
        dns[dns_off++] = 4;

        /* RDATA: IPv4 address */
        memcpy(dns + dns_off, addr, 4);
        dns_off += 4;
    }

    /* Compute lengths */
    dns_len = dns_off;
    udp_len = 8 + dns_len;
    total   = 20 + udp_len;

    /* IPv4 header */
    pkt[0]  = 0x45;
    pkt[1]  = 0x00;
    pkt[2]  = (uint8_t)(total >> 8);
    pkt[3]  = (uint8_t)(total & 0xFF);
    pkt[4]  = 0x00; pkt[5] = 0x01;
    pkt[6]  = 0x00; pkt[7] = 0x00;
    pkt[8]  = 0x40;
    pkt[9]  = 17;  /* UDP */
    pkt[10] = 0x00; pkt[11] = 0x00;
    /* src: 172.16.7.1 (DNS server) */
    pkt[12] = 0xAC; pkt[13] = 0x10;
    pkt[14] = 0x07; pkt[15] = 0x01;
    /* dst: 172.16.7.2 (us) */
    pkt[16] = 0xAC; pkt[17] = 0x10;
    pkt[18] = 0x07; pkt[19] = 0x02;

    /* IPv4 checksum */
    chk = tiku_kits_net_ipv4_chksum(pkt, 20);
    pkt[10] = (uint8_t)(chk >> 8);
    pkt[11] = (uint8_t)(chk & 0xFF);

    /* UDP header */
    udp = pkt + 20;
    /* src port: 53 (DNS server) */
    udp[0] = 0;
    udp[1] = TIKU_KITS_NET_DNS_PORT;
    /* dst port: DNS client local port */
    udp[2] = (uint8_t)(TIKU_KITS_NET_DNS_LOCAL_PORT >> 8);
    udp[3] = (uint8_t)(TIKU_KITS_NET_DNS_LOCAL_PORT & 0xFF);
    /* UDP length */
    udp[4] = (uint8_t)(udp_len >> 8);
    udp[5] = (uint8_t)(udp_len & 0xFF);
    /* UDP checksum: 0 (optional for IPv4) */
    udp[6] = 0; udp[7] = 0;

    /* Suppress unused-variable warning for loop index */
    (void)i;

    tiku_kits_net_ipv4_input(pkt, total);
}

/*---------------------------------------------------------------------------*/
/* Example 1: DNS init and server configuration                              */
/*---------------------------------------------------------------------------*/

static void example_dns_init(void)
{
    static const uint8_t dns_srv[] = {172, 16, 7, 1};
    int8_t rc;

    printf("--- DNS Init and Server Config ---\n");

    tiku_kits_net_dns_init();
    printf("  dns_init(): OK\n");

    rc = tiku_kits_net_dns_set_server(dns_srv);
    printf("  set_server(172.16.7.1): %s\n",
           rc == TIKU_KITS_NET_OK ? "PASS" : "FAIL");

    rc = tiku_kits_net_dns_set_server((void *)0);
    printf("  set_server(NULL): %s (expected ERR_NULL)\n",
           rc == TIKU_KITS_NET_ERR_NULL ? "PASS" : "FAIL");
}

/*---------------------------------------------------------------------------*/
/* Example 2: Hostname encoding verification                                 */
/*---------------------------------------------------------------------------*/

static void example_hostname_encoding(void)
{
    printf("--- Hostname Encoding ---\n");

    /*
     * DNS label encoding of "pool.ntp.org":
     *   \x04 p o o l \x03 n t p \x03 o r g \x00
     *
     * We verify this by inspecting the captured query packet.
     * The DNS question section starts at offset 40 in the raw
     * packet (20 IPv4 + 8 UDP + 12 DNS header).
     */

    lb_len = 0;
    tiku_kits_net_dns_init();
    {
        static const uint8_t srv[] = {172, 16, 7, 1};
        tiku_kits_net_dns_set_server(srv);
    }

    tiku_kits_net_dns_resolve("pool.ntp.org");

    if (lb_len > 0) {
        const uint8_t *qname = lb_buf + 40;
        /* Expected: 04 "pool" 03 "ntp" 03 "org" 00 */
        uint8_t ok = 1;
        ok = ok && (qname[0] == 4);
        ok = ok && (memcmp(qname + 1, "pool", 4) == 0);
        ok = ok && (qname[5] == 3);
        ok = ok && (memcmp(qname + 6, "ntp", 3) == 0);
        ok = ok && (qname[9] == 3);
        ok = ok && (memcmp(qname + 10, "org", 3) == 0);
        ok = ok && (qname[13] == 0);
        printf("  \"pool.ntp.org\" encoding: %s\n",
               ok ? "PASS" : "FAIL");
        printf("  Labels: [%u]pool [%u]ntp [%u]org [0]\n",
               qname[0], qname[5], qname[9]);
    } else {
        printf("  No query captured: FAIL\n");
    }

    tiku_kits_net_dns_abort();
}

/*---------------------------------------------------------------------------*/
/* Example 3: Query construction verification                                */
/*---------------------------------------------------------------------------*/

static void example_query_construction(void)
{
    printf("--- Query Construction ---\n");

    lb_len = 0;
    tiku_kits_net_dns_init();
    {
        static const uint8_t srv[] = {172, 16, 7, 1};
        tiku_kits_net_dns_set_server(srv);
    }

    tiku_kits_net_dns_resolve("pool.ntp.org");

    if (lb_len > 0) {
        const uint8_t *dns = lb_buf + 28;
        uint16_t flags = (uint16_t)(
            (uint16_t)dns[2] << 8 | dns[3]);
        uint16_t qdcount = (uint16_t)(
            (uint16_t)dns[4] << 8 | dns[5]);
        uint16_t txn_id = get_dns_txn_id(lb_buf);

        /* QR=0 (query) */
        uint8_t qr = !!(flags & TIKU_KITS_NET_DNS_FLAG_QR);
        /* RD=1 (recursion desired) */
        uint8_t rd = !!(flags & TIKU_KITS_NET_DNS_FLAG_RD);

        printf("  txn_id: 0x%04X\n", txn_id);
        printf("  QR=%u (expected 0): %s\n",
               qr, qr == 0 ? "PASS" : "FAIL");
        printf("  RD=%u (expected 1): %s\n",
               rd, rd == 1 ? "PASS" : "FAIL");
        printf("  QDCOUNT=%u (expected 1): %s\n",
               qdcount, qdcount == 1 ? "PASS" : "FAIL");

        /* QTYPE and QCLASS after the QNAME
         * QNAME for "pool.ntp.org" is 14 bytes
         * (4+pool+3+ntp+3+org+0 = 1+4+1+3+1+3+1 = 14) */
        {
            const uint8_t *qtype_ptr = dns + 12 + 14;
            uint16_t qtype = (uint16_t)(
                (uint16_t)qtype_ptr[0] << 8 | qtype_ptr[1]);
            uint16_t qclass = (uint16_t)(
                (uint16_t)qtype_ptr[2] << 8 | qtype_ptr[3]);
            printf("  QTYPE=%u (A=1): %s\n",
                   qtype,
                   qtype == TIKU_KITS_NET_DNS_QTYPE_A
                   ? "PASS" : "FAIL");
            printf("  QCLASS=%u (IN=1): %s\n",
                   qclass,
                   qclass == TIKU_KITS_NET_DNS_QCLASS_IN
                   ? "PASS" : "FAIL");
        }
    } else {
        printf("  No query captured: FAIL\n");
    }

    tiku_kits_net_dns_abort();
}

/*---------------------------------------------------------------------------*/
/* Example 4: Response injection and poll                                    */
/*---------------------------------------------------------------------------*/

static void example_response_inject(void)
{
    static const uint8_t answer_ip[] = {10, 0, 0, 1};
    uint16_t txn_id;
    int8_t rc;
    tiku_kits_net_dns_state_t state;

    printf("--- Response Injection and Poll ---\n");

    lb_len = 0;
    tiku_kits_net_dns_init();
    {
        static const uint8_t srv[] = {172, 16, 7, 1};
        tiku_kits_net_dns_set_server(srv);
    }

    rc = tiku_kits_net_dns_resolve("pool.ntp.org");
    printf("  resolve(\"pool.ntp.org\"): %s\n",
           rc == TIKU_KITS_NET_OK ? "OK" : "FAIL");

    if (lb_len == 0) {
        printf("  No query captured: FAIL\n");
        return;
    }

    /* Extract txn_id from the captured query */
    txn_id = get_dns_txn_id(lb_buf);
    printf("  Captured query txn_id=0x%04X\n", txn_id);

    /* Inject a response with IP=10.0.0.1, TTL=300 */
    inject_dns_response(txn_id, "pool.ntp.org",
                        answer_ip, 300,
                        TIKU_KITS_NET_DNS_RCODE_OK);
    printf("  Injected response (10.0.0.1, TTL=300)\n");

    /* Poll to process the response */
    rc = tiku_kits_net_dns_poll();
    state = tiku_kits_net_dns_get_state();
    printf("  poll(): %s, state=%s\n",
           rc == TIKU_KITS_NET_OK ? "OK" : "not OK",
           state == TIKU_KITS_NET_DNS_STATE_DONE
           ? "DONE" : "not DONE");

    /* Verify resolved address */
    {
        uint8_t resolved[4] = {0};
        rc = tiku_kits_net_dns_get_addr(resolved);
        printf("  get_addr(): %u.%u.%u.%u -> %s\n",
               resolved[0], resolved[1],
               resolved[2], resolved[3],
               (rc == TIKU_KITS_NET_OK &&
                memcmp(resolved, answer_ip, 4) == 0)
               ? "PASS" : "FAIL");
    }

    /* Verify TTL */
    {
        uint32_t ttl = tiku_kits_net_dns_get_ttl();
        printf("  get_ttl(): %lu -> %s\n",
               (unsigned long)ttl,
               ttl == 300 ? "PASS" : "FAIL");
    }
}

/*---------------------------------------------------------------------------*/
/* Example 5: Cache hit                                                      */
/*---------------------------------------------------------------------------*/

static void example_cache_hit(void)
{
    int8_t rc;
    tiku_kits_net_dns_state_t state;

    printf("--- Cache Hit ---\n");

    /*
     * After example_response_inject(), "pool.ntp.org" should be
     * cached.  Resolving it again should return immediately
     * (state = DONE) without sending a new query.
     */
    lb_len = 0;

    rc = tiku_kits_net_dns_resolve("pool.ntp.org");
    state = tiku_kits_net_dns_get_state();

    printf("  resolve(\"pool.ntp.org\") again: %s\n",
           rc == TIKU_KITS_NET_OK ? "OK" : "FAIL");
    printf("  state=%s (expected DONE, cache hit)\n",
           state == TIKU_KITS_NET_DNS_STATE_DONE
           ? "DONE" : "not DONE");
    printf("  No new query sent: %s\n",
           lb_len == 0 ? "PASS (no packet)" : "FAIL (packet sent)");

    /* Verify address is still correct */
    {
        uint8_t resolved[4] = {0};
        tiku_kits_net_dns_get_addr(resolved);
        printf("  Cached addr: %u.%u.%u.%u -> %s\n",
               resolved[0], resolved[1],
               resolved[2], resolved[3],
               (resolved[0] == 10 && resolved[1] == 0 &&
                resolved[2] == 0  && resolved[3] == 1)
               ? "PASS" : "FAIL");
    }
}

/*---------------------------------------------------------------------------*/
/* Example 6: NXDOMAIN handling                                              */
/*---------------------------------------------------------------------------*/

static void example_nxdomain(void)
{
    uint16_t txn_id;
    int8_t rc;
    tiku_kits_net_dns_state_t state;

    printf("--- NXDOMAIN Handling ---\n");

    lb_len = 0;
    tiku_kits_net_dns_init();
    {
        static const uint8_t srv[] = {172, 16, 7, 1};
        tiku_kits_net_dns_set_server(srv);
    }

    rc = tiku_kits_net_dns_resolve("nonexistent.invalid");
    printf("  resolve(\"nonexistent.invalid\"): %s\n",
           rc == TIKU_KITS_NET_OK ? "OK" : "FAIL");

    if (lb_len == 0) {
        printf("  No query captured: FAIL\n");
        return;
    }

    txn_id = get_dns_txn_id(lb_buf);
    printf("  Captured query txn_id=0x%04X\n", txn_id);

    /* Inject NXDOMAIN response (RCODE=3, no answer) */
    inject_dns_response(txn_id, "nonexistent.invalid",
                        (void *)0, 0,
                        TIKU_KITS_NET_DNS_RCODE_NXDOMAIN);
    printf("  Injected NXDOMAIN response\n");

    /* Poll to process the response */
    tiku_kits_net_dns_poll();
    state = tiku_kits_net_dns_get_state();
    printf("  state=%s -> %s\n",
           state == TIKU_KITS_NET_DNS_STATE_ERROR
           ? "ERROR" : "not ERROR",
           state == TIKU_KITS_NET_DNS_STATE_ERROR
           ? "PASS" : "FAIL");
}

/*---------------------------------------------------------------------------*/
/* Example 7: Cache flush and re-resolve                                     */
/*---------------------------------------------------------------------------*/

static void example_cache_flush(void)
{
    static const uint8_t answer_ip[] = {10, 0, 0, 2};
    uint16_t txn_id;
    int8_t rc;
    tiku_kits_net_dns_state_t state;

    printf("--- Cache Flush and Re-resolve ---\n");

    /* Re-init and populate the cache first */
    tiku_kits_net_dns_init();
    {
        static const uint8_t srv[] = {172, 16, 7, 1};
        tiku_kits_net_dns_set_server(srv);
    }

    /* Resolve and inject to populate cache */
    lb_len = 0;
    tiku_kits_net_dns_resolve("pool.ntp.org");
    if (lb_len > 0) {
        txn_id = get_dns_txn_id(lb_buf);
        inject_dns_response(txn_id, "pool.ntp.org",
                            answer_ip, 60,
                            TIKU_KITS_NET_DNS_RCODE_OK);
        tiku_kits_net_dns_poll();
    }

    /* Flush the cache */
    tiku_kits_net_dns_cache_flush();
    printf("  cache_flush(): OK\n");

    /* Re-resolve: should send a new query (cache miss) */
    lb_len = 0;
    rc = tiku_kits_net_dns_resolve("pool.ntp.org");
    state = tiku_kits_net_dns_get_state();

    printf("  resolve after flush: %s\n",
           rc == TIKU_KITS_NET_OK ? "OK" : "FAIL");
    printf("  state=%s (expected SENT, not cached)\n",
           state == TIKU_KITS_NET_DNS_STATE_SENT
           ? "SENT" : "not SENT");
    printf("  New query sent: %s\n",
           lb_len > 0 ? "PASS (packet captured)" : "FAIL");

    tiku_kits_net_dns_abort();
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_net_dns_run(void)
{
    printf("=== TikuKits DNS Resolver Examples ===\n\n");

    tiku_kits_net_ipv4_set_link(&loopback);
    tiku_kits_net_udp_init();

    example_dns_init();
    printf("\n");

    example_hostname_encoding();
    printf("\n");

    example_query_construction();
    printf("\n");

    example_response_inject();
    printf("\n");

    example_cache_hit();
    printf("\n");

    example_nxdomain();
    printf("\n");

    example_cache_flush();
    printf("\n");
}

#else /* !HAS_TIKUKITS */

void example_net_dns_run(void)
{
    /* DNS not available */
}

#endif
