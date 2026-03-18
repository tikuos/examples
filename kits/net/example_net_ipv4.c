/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: IPv4 Networking Fundamentals
 *
 * Demonstrates the low-level IPv4 primitives available in the
 * TikuOS networking stack:
 *   - RFC 1071 ones-complement checksum
 *   - IPv4 header construction and field access
 *   - Byte-order conversion (htons / ntohs)
 *   - Packet buffer and address accessors
 *
 * This example runs entirely on-device using printf output -- it
 * does not require an active SLIP link or remote host.  It exercises
 * the same APIs that ICMP, UDP, and TFTP use internally, making it
 * a useful reference for understanding how the stack constructs and
 * validates packets.
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

#if defined(HAS_TIKUKITS)

/*---------------------------------------------------------------------------*/
/* Example 1: Byte-order helpers                                             */
/*---------------------------------------------------------------------------*/

static void example_byte_order(void)
{
    uint16_t host_val = 0x1234;
    uint16_t net_val;

    printf("--- Byte-Order Helpers ---\n");

    net_val = tiku_kits_net_htons(host_val);
    printf("  htons(0x%04X) = 0x%04X\n",
           (unsigned)host_val, (unsigned)net_val);

    printf("  ntohs(0x%04X) = 0x%04X\n",
           (unsigned)net_val,
           (unsigned)tiku_kits_net_ntohs(net_val));

    /* Round-trip: htons(ntohs(x)) == x */
    printf("  Round-trip: htons(ntohs(0xABCD)) = 0x%04X\n",
           (unsigned)tiku_kits_net_htons(
               tiku_kits_net_ntohs(0xABCD)));
}

/*---------------------------------------------------------------------------*/
/* Example 2: IPv4 checksum                                                  */
/*---------------------------------------------------------------------------*/

static void example_ipv4_checksum(void)
{
    /* A minimal IPv4 header (20 bytes) with checksum field = 0 */
    uint8_t hdr[20] = {
        0x45, 0x00, 0x00, 0x3C,  /* ver/ihl, tos, total_len=60 */
        0x1C, 0x46, 0x40, 0x00,  /* id, flags+frag */
        0x40, 0x06, 0x00, 0x00,  /* ttl=64, proto=TCP, chksum=0 */
        0xAC, 0x10, 0x0A, 0x63,  /* src: 172.16.10.99 */
        0xAC, 0x10, 0x0A, 0x0C   /* dst: 172.16.10.12 */
    };
    uint16_t chk;

    printf("--- IPv4 Checksum (RFC 1071) ---\n");

    /* Step 1: compute checksum with zeroed field */
    chk = tiku_kits_net_ipv4_chksum(hdr, 20);
    printf("  Computed checksum: 0x%04X\n", (unsigned)chk);

    /* Step 2: write it back into the header */
    hdr[10] = (uint8_t)(chk >> 8);
    hdr[11] = (uint8_t)(chk & 0xFF);
    printf("  Header bytes [10..11]: 0x%02X 0x%02X\n",
           hdr[10], hdr[11]);

    /* Step 3: verify -- checksum of valid header should be 0 */
    chk = tiku_kits_net_ipv4_chksum(hdr, 20);
    printf("  Verification: 0x%04X (should be 0x0000)\n",
           (unsigned)chk);
}

/*---------------------------------------------------------------------------*/
/* Example 3: IPv4 header field access                                       */
/*---------------------------------------------------------------------------*/

static void example_header_access(void)
{
    /* Build a small IPv4 + ICMP echo request packet */
    uint8_t pkt[28] = {
        /* IPv4 header */
        0x45, 0x00, 0x00, 0x1C,  /* ver=4, ihl=5, totlen=28 */
        0x00, 0x01, 0x00, 0x00,  /* id=1, frag=0 */
        0x40, 0x01, 0x00, 0x00,  /* ttl=64, proto=ICMP(1), chk=0 */
        0xAC, 0x10, 0x07, 0x01,  /* src: 172.16.7.1 */
        0xAC, 0x10, 0x07, 0x02,  /* dst: 172.16.7.2 */
        /* ICMP echo request */
        0x08, 0x00, 0x00, 0x00,  /* type=8, code=0, chk=0 */
        0x00, 0x01, 0x00, 0x01   /* id=1, seq=1 */
    };

    printf("--- IPv4 Header Field Access ---\n");

    printf("  Version:   %u\n", TIKU_KITS_NET_IPV4_VER(pkt));
    printf("  IHL:       %u (x4 = %u bytes)\n",
           TIKU_KITS_NET_IPV4_IHL(pkt),
           TIKU_KITS_NET_IPV4_IHL(pkt) * 4);
    printf("  Total len: %u bytes\n",
           TIKU_KITS_NET_IPV4_TOTLEN(pkt));
    printf("  Protocol:  %u", TIKU_KITS_NET_IPV4_PROTO(pkt));
    if (TIKU_KITS_NET_IPV4_PROTO(pkt) == 1) {
        printf(" (ICMP)");
    } else if (TIKU_KITS_NET_IPV4_PROTO(pkt) == 17) {
        printf(" (UDP)");
    }
    printf("\n");
    printf("  Source:     %u.%u.%u.%u\n",
           TIKU_KITS_NET_IPV4_SRC(pkt)[0],
           TIKU_KITS_NET_IPV4_SRC(pkt)[1],
           TIKU_KITS_NET_IPV4_SRC(pkt)[2],
           TIKU_KITS_NET_IPV4_SRC(pkt)[3]);
    printf("  Dest:      %u.%u.%u.%u\n",
           TIKU_KITS_NET_IPV4_DST(pkt)[0],
           TIKU_KITS_NET_IPV4_DST(pkt)[1],
           TIKU_KITS_NET_IPV4_DST(pkt)[2],
           TIKU_KITS_NET_IPV4_DST(pkt)[3]);
}

/*---------------------------------------------------------------------------*/
/* Example 4: Buffer and address accessors                                   */
/*---------------------------------------------------------------------------*/

static void example_buffer_access(void)
{
    uint8_t *buf;
    uint16_t buf_size;
    const uint8_t *our_ip;

    printf("--- Buffer & Address Accessors ---\n");

    buf = tiku_kits_net_ipv4_get_buf(&buf_size);
    printf("  Shared buffer: %p, size = %u bytes (MTU)\n",
           (void *)buf, (unsigned)buf_size);

    our_ip = tiku_kits_net_ipv4_get_addr();
    printf("  Our IP:        %u.%u.%u.%u\n",
           our_ip[0], our_ip[1], our_ip[2], our_ip[3]);
    printf("  Configured as: " );
    printf("TIKU_KITS_NET_IP_ADDR\n");
    printf("  Default TTL:   %u\n", TIKU_KITS_NET_TTL);
    printf("  MTU:           %u bytes\n", TIKU_KITS_NET_MTU);
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_net_ipv4_run(void)
{
    printf("=== TikuKits IPv4 Networking Examples ===\n\n");

    example_byte_order();
    printf("\n");

    example_ipv4_checksum();
    printf("\n");

    example_header_access();
    printf("\n");

    example_buffer_access();
    printf("\n");
}

#endif /* HAS_TIKUKITS */
