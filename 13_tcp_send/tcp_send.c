/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tcp_send.c - TikuOS Example 13: TCP Sender over SLIP
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
 * @file    tcp_send.c
 * @brief   TikuOS Example 13 - TCP Sender over SLIP
 *
 * Demonstrates sending TCP data from the MSP430 to a host PC
 * using the TikuKits networking stack over a SLIP serial link.
 *
 * The MSP430 connects to 172.16.7.1:5000, sends "TikuOS #N"
 * every 3 seconds, then closes the connection after 5 messages.
 * A companion Python script (tcp_receiver.py) runs on the host,
 * accepts the TCP connection, and displays the received data.
 *
 * Hardware setup:
 *   - MSP430FR5969 LaunchPad with external FTDI/CP2102 adapter
 *     (eZ-FET backchannel cannot handle TCP's multi-exchange)
 *   - UART: 9600 baud, 8N1 (P2.0 TXD, P2.1 RXD)
 *   - TikuOS IP: 172.16.7.2 (default)
 *   - Host IP:   172.16.7.1
 *
 * What you learn:
 *   - Active TCP open with tiku_kits_net_tcp_connect()
 *   - Sending data with tiku_kits_net_tcp_send()
 *   - Handling connection events (CONNECTED, CLOSED, ABORTED)
 *   - Graceful close with tiku_kits_net_tcp_close()
 *   - SLIP transport over UART to a host PC
 */

#include "tiku.h"

#if TIKU_EXAMPLE_TCP_SEND && defined(HAS_TIKUKITS) && \
    TIKU_KITS_NET_TCP_ENABLE

#include "tikukits/net/tiku_kits_net.h"
#include "tikukits/net/ipv4/tiku_kits_net_ipv4.h"
#include "tikukits/net/ipv4/tiku_kits_net_tcp.h"
#include <string.h>

/*--------------------------------------------------------------------------*/
/* Configuration                                                             */
/*--------------------------------------------------------------------------*/

/** Host IP address (the PC running tcp_receiver.py) */
static const uint8_t host_ip[4] = {172, 16, 7, 1};

/** Destination port on the host */
#define TCP_SEND_DST_PORT       5000

/** Source port on the MSP430 */
#define TCP_SEND_SRC_PORT       3000

/** Send interval in seconds */
#define TCP_SEND_INTERVAL       3

/** Number of messages to send before closing */
#define TCP_SEND_COUNT          5

/*--------------------------------------------------------------------------*/
/* Connection state                                                          */
/*--------------------------------------------------------------------------*/

static tiku_kits_net_tcp_conn_t *conn;
static volatile uint8_t connected;
static volatile uint8_t closed;

/*--------------------------------------------------------------------------*/
/* TCP callbacks                                                             */
/*--------------------------------------------------------------------------*/

static void
tcp_recv_cb(struct tiku_kits_net_tcp_conn *c, uint16_t available)
{
    /* Discard any incoming data (we're a sender) */
    uint8_t tmp[88];
    (void)tiku_kits_net_tcp_read(c, tmp, sizeof(tmp));
    (void)available;
}

static void
tcp_event_cb(struct tiku_kits_net_tcp_conn *c, uint8_t event)
{
    (void)c;
    if (event == TIKU_KITS_NET_TCP_EVT_CONNECTED) {
        connected = 1;
    } else if (event == TIKU_KITS_NET_TCP_EVT_CLOSED ||
               event == TIKU_KITS_NET_TCP_EVT_ABORTED) {
        closed = 1;
    }
}

/*--------------------------------------------------------------------------*/
/* Process                                                                   */
/*--------------------------------------------------------------------------*/

TIKU_PROCESS(tcp_send_process, "TCP-Send");

static struct tiku_timer send_timer;

TIKU_PROCESS_THREAD(tcp_send_process, ev, data)
{
    static uint16_t pkt_seq;

    TIKU_PROCESS_BEGIN();

    tiku_common_led1_init();
    tiku_common_led2_init();

    /* Wait for net process to initialize SLIP + TCP */
    tiku_timer_set_event(&send_timer, TIKU_CLOCK_SECOND * 2);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    /* Connect to host */
    connected = 0;
    closed = 0;
    conn = tiku_kits_net_tcp_connect(
        host_ip, TCP_SEND_DST_PORT, TCP_SEND_SRC_PORT,
        tcp_recv_cb, tcp_event_cb);

    if (conn == (void *)0) {
        /* No free connection slot */
        tiku_common_led2_on();
        TIKU_PROCESS_EXIT();
    }

    /* Wait for CONNECTED event (or timeout after 30s) */
    tiku_timer_set_event(&send_timer, TIKU_CLOCK_SECOND * 30);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(
        ev == TIKU_EVENT_TIMER || connected || closed);

    if (!connected) {
        tiku_common_led2_on();
        TIKU_PROCESS_EXIT();
    }

    tiku_common_led1_on();  /* Green: connected */

    /* Send messages */
    pkt_seq = 0;
    while (pkt_seq < TCP_SEND_COUNT && !closed) {
        char msg[32];
        uint8_t len;
        int8_t rc;
        char *p;

        /* Build "TikuOS #N" without printf */
        p = msg;
        *p++ = 'T'; *p++ = 'i'; *p++ = 'k'; *p++ = 'u';
        *p++ = 'O'; *p++ = 'S'; *p++ = ' '; *p++ = '#';
        {
            char digits[5];
            uint8_t ndigits = 0;
            uint16_t s = pkt_seq;
            if (s == 0) {
                digits[ndigits++] = '0';
            } else {
                while (s > 0) {
                    digits[ndigits++] = '0' + (char)(s % 10);
                    s /= 10;
                }
            }
            while (ndigits > 0) {
                *p++ = digits[--ndigits];
            }
        }
        len = (uint8_t)(p - msg);

        rc = tiku_kits_net_tcp_send(conn,
                                     (const uint8_t *)msg, len);
        if (rc == TIKU_KITS_NET_OK) {
            tiku_common_led1_toggle();
        } else {
            tiku_common_led2_toggle();
        }

        pkt_seq++;

        tiku_timer_set_event(&send_timer,
                             TIKU_CLOCK_SECOND * TCP_SEND_INTERVAL);
        TIKU_PROCESS_WAIT_EVENT_UNTIL(
            ev == TIKU_EVENT_TIMER || closed);
    }

    /* Graceful close */
    if (!closed && conn != (void *)0) {
        tiku_kits_net_tcp_close(conn);

        /* Wait for close to complete */
        tiku_timer_set_event(&send_timer, TIKU_CLOCK_SECOND * 10);
        TIKU_PROCESS_WAIT_EVENT_UNTIL(
            ev == TIKU_EVENT_TIMER || closed);
    }

    tiku_common_led1_off();
    tiku_common_led2_off();

    TIKU_PROCESS_END();
}

/*--------------------------------------------------------------------------*/
/* Autostart                                                                 */
/*--------------------------------------------------------------------------*/
TIKU_AUTOSTART_PROCESSES(&tiku_kits_net_process, &tcp_send_process);

#endif /* TIKU_EXAMPLE_TCP_SEND && HAS_TIKUKITS && TCP_ENABLE */
