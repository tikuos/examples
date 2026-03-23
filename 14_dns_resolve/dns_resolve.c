/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * dns_resolve.c - TikuOS Example 14: DNS Resolver over SLIP
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file    dns_resolve.c
 * @brief   TikuOS Example 14 - DNS Resolver over SLIP
 *
 * Demonstrates resolving a hostname to an IPv4 address using
 * the TikuKits DNS stub resolver over a SLIP serial link.
 *
 * The MSP430 sends a DNS A-record query to the configured DNS
 * server (172.16.7.1) and polls until the response arrives.
 * The resolved address is printed over UART.  A companion
 * Python script on the host forwards DNS queries to a real
 * server (or responds directly).
 *
 * Hardware setup:
 *   - MSP430FR5969 LaunchPad connected via USB
 *   - UART: 9600 baud, 8N1
 *   - TikuOS IP: 172.16.7.2 (default)
 *   - Host IP / DNS server: 172.16.7.1
 *
 * What you learn:
 *   - Initialising the DNS resolver
 *   - Setting the DNS server address
 *   - Resolving a hostname with tiku_kits_net_dns_resolve()
 *   - Polling the DNS state machine
 *   - Retrieving the resolved IPv4 address and TTL
 *   - Handling DNS errors (timeout, NXDOMAIN)
 */

#include "tiku.h"

#if TIKU_EXAMPLE_DNS_RESOLVE && defined(HAS_TIKUKITS)

#include "tikukits/net/tiku_kits_net.h"
#include "tikukits/net/ipv4/tiku_kits_net_ipv4.h"
#include "tikukits/net/ipv4/tiku_kits_net_udp.h"
#include "tikukits/net/ipv4/tiku_kits_net_dns.h"

/*--------------------------------------------------------------------------*/
/* Configuration                                                             */
/*--------------------------------------------------------------------------*/

/** DNS server address (the host running slipcat / DNS forwarder) */
static const uint8_t dns_server[4] = {172, 16, 7, 1};

/** Hostname to resolve */
#define DNS_HOSTNAME    "pool.ntp.org"

/** Resolve interval in seconds */
#define DNS_INTERVAL    10

/** Maximum poll cycles before giving up */
#define DNS_MAX_POLLS   200

/*--------------------------------------------------------------------------*/
/* Process                                                                   */
/*--------------------------------------------------------------------------*/

TIKU_PROCESS(dns_resolve_process, "DNS-Resolve");

static struct tiku_timer dns_timer;

TIKU_PROCESS_THREAD(dns_resolve_process, ev, data)
{
    TIKU_PROCESS_BEGIN();

    tiku_common_led1_init();
    tiku_common_led2_init();

    /* Let the net process initialise */
    tiku_timer_set_event(&dns_timer, TIKU_CLOCK_SECOND * 2);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    /* Configure DNS server */
    tiku_kits_net_dns_init();
    tiku_kits_net_dns_set_server(dns_server);

    while (1) {
        int8_t rc;
        uint16_t polls;
        tiku_kits_net_dns_state_t state;

        /* Start DNS query */
        rc = tiku_kits_net_dns_resolve(DNS_HOSTNAME);
        if (rc != TIKU_KITS_NET_OK) {
            tiku_common_led2_toggle();
            goto next;
        }

        /* Poll until DONE or ERROR */
        for (polls = 0; polls < DNS_MAX_POLLS; polls++) {
            tiku_timer_set_event(&dns_timer,
                TIKU_CLOCK_MS_TO_TICKS(50));
            TIKU_PROCESS_WAIT_EVENT_UNTIL(
                ev == TIKU_EVENT_TIMER);

            tiku_kits_net_dns_poll();
            state = tiku_kits_net_dns_get_state();
            if (state == TIKU_KITS_NET_DNS_STATE_DONE
                || state == TIKU_KITS_NET_DNS_STATE_ERROR) {
                break;
            }
        }

        if (state == TIKU_KITS_NET_DNS_STATE_DONE) {
            uint8_t addr[4];
            tiku_kits_net_dns_get_addr(addr);
            tiku_common_led1_toggle();  /* Green: resolved */
        } else {
            tiku_kits_net_dns_abort();
            tiku_common_led2_toggle();  /* Red: failed */
        }

next:
        tiku_timer_set_event(&dns_timer,
            TIKU_CLOCK_SECOND * DNS_INTERVAL);
        TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);
    }

    TIKU_PROCESS_END();
}

/*--------------------------------------------------------------------------*/
/* Autostart                                                                 */
/*--------------------------------------------------------------------------*/
TIKU_AUTOSTART_PROCESSES(&tiku_kits_net_process,
                         &dns_resolve_process);

#endif /* TIKU_EXAMPLE_DNS_RESOLVE && HAS_TIKUKITS */
