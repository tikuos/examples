/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tcp_echo.c - TikuOS Example 16: TCP Echo Client over SLIP
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
 * @file    tcp_echo.c
 * @brief   TikuOS Example 16 - TCP Echo Client over SLIP
 *
 * Demonstrates a TCP client that connects to a remote echo
 * server, sends data, reads the echoed response, and verifies
 * the data matches.
 *
 * The MSP430 connects to 172.16.7.1:7 (standard echo port),
 * sends a short message, waits for the echo, then disconnects.
 * LED1 blinks on successful echo, LED2 on mismatch or error.
 *
 * Hardware setup:
 *   - MSP430FR5969 LaunchPad with external FTDI/CP2102 adapter
 *     (eZ-FET backchannel cannot handle TCP's multi-exchange)
 *   - UART: 9600 baud, 8N1 (P2.0 TXD, P2.1 RXD)
 *   - TikuOS IP: 172.16.7.2 (default)
 *   - Host IP:   172.16.7.1 (running an echo server on port 7)
 *
 * What you learn:
 *   - Active TCP open with tiku_kits_net_tcp_connect()
 *   - Sending data with tiku_kits_net_tcp_send()
 *   - Reading echoed data with tiku_kits_net_tcp_read()
 *   - Verifying round-trip data integrity
 *   - Graceful close with tiku_kits_net_tcp_close()
 */

#include "tiku.h"

#if TIKU_EXAMPLE_TCP_ECHO && defined(HAS_TIKUKITS) && \
    TIKU_KITS_NET_TCP_ENABLE

#include "tikukits/net/tiku_kits_net.h"
#include "tikukits/net/ipv4/tiku_kits_net_ipv4.h"
#include "tikukits/net/ipv4/tiku_kits_net_tcp.h"
#include <string.h>

/*--------------------------------------------------------------------------*/
/* Configuration                                                             */
/*--------------------------------------------------------------------------*/

/** Host IP address running the echo server */
static const uint8_t host_ip[4] = {172, 16, 7, 1};

/** Echo service port (RFC 862) */
#define ECHO_DST_PORT       7

/** Local source port */
#define ECHO_SRC_PORT       3001

/** Number of echo rounds */
#define ECHO_ROUNDS         3

/** Interval between rounds (seconds) */
#define ECHO_INTERVAL       2

/*--------------------------------------------------------------------------*/
/* Connection state                                                          */
/*--------------------------------------------------------------------------*/

static tiku_kits_net_tcp_conn_t *conn;
static volatile uint8_t connected;
static volatile uint8_t closed;
static volatile uint8_t data_ready;

/*--------------------------------------------------------------------------*/
/* TCP callbacks                                                             */
/*--------------------------------------------------------------------------*/

static void
echo_recv_cb(struct tiku_kits_net_tcp_conn *c, uint16_t available)
{
    (void)c;
    (void)available;
    data_ready = 1;
}

static void
echo_event_cb(struct tiku_kits_net_tcp_conn *c, uint8_t event)
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

TIKU_PROCESS(tcp_echo_process, "TCP-Echo");

static struct tiku_timer echo_timer;

TIKU_PROCESS_THREAD(tcp_echo_process, ev, data)
{
    static uint8_t round;

    TIKU_PROCESS_BEGIN();

    tiku_common_led1_init();
    tiku_common_led2_init();

    /* Wait for net process to initialise */
    tiku_timer_set_event(&echo_timer, TIKU_CLOCK_SECOND * 2);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    /* Connect to echo server */
    connected = 0;
    closed = 0;
    conn = tiku_kits_net_tcp_connect(
        host_ip, ECHO_DST_PORT, ECHO_SRC_PORT,
        echo_recv_cb, echo_event_cb);

    if (conn == (void *)0) {
        tiku_common_led2_on();
        TIKU_PROCESS_EXIT();
    }

    /* Wait for connection (30s timeout) */
    tiku_timer_set_event(&echo_timer, TIKU_CLOCK_SECOND * 30);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(
        ev == TIKU_EVENT_TIMER || connected || closed);

    if (!connected) {
        tiku_common_led2_on();
        TIKU_PROCESS_EXIT();
    }

    /* Echo rounds */
    for (round = 0; round < ECHO_ROUNDS && !closed; round++) {
        static const uint8_t msg[] = "TikuOS echo";
        uint8_t rx_buf[16];
        uint16_t rx_len;
        int8_t rc;

        /* Send */
        rc = tiku_kits_net_tcp_send(conn, msg, sizeof(msg) - 1);
        if (rc != TIKU_KITS_NET_OK) {
            tiku_common_led2_toggle();
            break;
        }

        /* Wait for echo (5s timeout) */
        data_ready = 0;
        tiku_timer_set_event(&echo_timer,
            TIKU_CLOCK_SECOND * 5);
        TIKU_PROCESS_WAIT_EVENT_UNTIL(
            ev == TIKU_EVENT_TIMER || data_ready || closed);

        if (closed) {
            break;
        }

        /* Read echoed data */
        rx_len = tiku_kits_net_tcp_read(
            conn, rx_buf, sizeof(rx_buf));

        if (rx_len == sizeof(msg) - 1
            && memcmp(rx_buf, msg, rx_len) == 0) {
            tiku_common_led1_toggle();  /* Green: match */
        } else {
            tiku_common_led2_toggle();  /* Red: mismatch */
        }

        /* Pause between rounds */
        tiku_timer_set_event(&echo_timer,
            TIKU_CLOCK_SECOND * ECHO_INTERVAL);
        TIKU_PROCESS_WAIT_EVENT_UNTIL(
            ev == TIKU_EVENT_TIMER || closed);
    }

    /* Graceful close */
    if (!closed && conn != (void *)0) {
        tiku_kits_net_tcp_close(conn);
        tiku_timer_set_event(&echo_timer,
            TIKU_CLOCK_SECOND * 10);
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
TIKU_AUTOSTART_PROCESSES(&tiku_kits_net_process,
                         &tcp_echo_process);

#endif /* TIKU_EXAMPLE_TCP_ECHO && HAS_TIKUKITS && TCP_ENABLE */
