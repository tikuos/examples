/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * http_fetch.c - TikuOS Example 17: Fetch a real webpage
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
 * @file    http_fetch.c
 * @brief   TikuOS Example 17 - Fetch a real webpage from the internet
 *
 * The MSP430 sends an HTTP/1.1 GET request over plain TCP to a
 * gateway running on the host PC (172.16.7.1:80).  The gateway
 * (tools/http_fetch_gateway.py) reads the Host header, fetches
 * the actual URL from the internet, and sends the real HTTP
 * response back through the SLIP link.
 *
 * The MSP430 parses the response, extracts the body, and stores
 * it in a FRAM-backed buffer.  LED1 lights on HTTP 200, LED2 on
 * error.  The gateway prints the fetched content on the host
 * terminal so you can see what the MCU received.
 *
 * Hardware setup:
 *   - MSP430FR5969 LaunchPad with external FTDI/CP2102 adapter
 *     (eZ-FET cannot handle multi-exchange TCP)
 *   - UART: 9600 baud, 8N1
 *   - TikuOS IP: 172.16.7.2
 *   - Host IP:   172.16.7.1
 *   - Run: python3 tools/http_fetch_gateway.py --port /dev/ttyUSB0
 *
 * What you learn:
 *   - Building HTTP requests with the request builder
 *   - Sending over raw TCP (no TLS, gateway handles internet)
 *   - Parsing real HTTP responses with the response parser
 *   - FRAM-backed response buffers
 *   - Companion gateway pattern for internet access
 */

#include "tiku.h"

#if TIKU_EXAMPLE_HTTP_FETCH && defined(HAS_TIKUKITS) && \
    TIKU_KITS_NET_TCP_ENABLE

#include "tikukits/net/tiku_kits_net.h"
#include "tikukits/net/ipv4/tiku_kits_net_ipv4.h"
#include "tikukits/net/ipv4/tiku_kits_net_tcp.h"
#include "tikukits/net/http/tiku_kits_net_http.h"
#include <stddef.h>

/*--------------------------------------------------------------------------*/
/* Configuration                                                             */
/*--------------------------------------------------------------------------*/

/** Gateway IP (host running http_fetch_gateway.py) */
static const uint8_t gateway_ip[4] = {172, 16, 7, 1};

/** Gateway port */
#define FETCH_DST_PORT      80

/** Local TCP port */
#define FETCH_SRC_PORT      3002

/** Target hostname (sent in Host header, gateway fetches it) */
#define FETCH_HOST          "example.com"

/** Target path */
#define FETCH_PATH          "/"

/** Response body buffer size (FRAM-backed) */
#define FETCH_RESP_SIZE     256

/*--------------------------------------------------------------------------*/
/* FRAM-backed response buffer                                               */
/*--------------------------------------------------------------------------*/

__attribute__((section(".persistent"), aligned(2)))
static uint8_t resp_buf[FETCH_RESP_SIZE];

__attribute__((section(".persistent"), aligned(2)))
static uint16_t resp_status;

__attribute__((section(".persistent"), aligned(2)))
static uint16_t resp_body_len;

#define RESP_MAGIC  0xBEEF

__attribute__((section(".persistent"), aligned(2)))
static uint16_t resp_magic;

/*--------------------------------------------------------------------------*/
/* Connection state                                                          */
/*--------------------------------------------------------------------------*/

static tiku_kits_net_tcp_conn_t *conn;
static volatile uint8_t connected;
static volatile uint8_t closed;
static volatile uint8_t data_ready;

static tiku_kits_net_http_parser_t parser;

/*--------------------------------------------------------------------------*/
/* TCP callbacks                                                             */
/*--------------------------------------------------------------------------*/

static void
fetch_recv_cb(struct tiku_kits_net_tcp_conn *c,
              uint16_t available)
{
    (void)c;
    (void)available;
    data_ready = 1;
}

static void
fetch_event_cb(struct tiku_kits_net_tcp_conn *c,
               uint8_t event)
{
    (void)c;
    if (event == TIKU_KITS_NET_TCP_EVT_CONNECTED) {
        connected = 1;
    } else if (event == TIKU_KITS_NET_TCP_EVT_CLOSED
               || event == TIKU_KITS_NET_TCP_EVT_ABORTED) {
        closed = 1;
    }
}

/*--------------------------------------------------------------------------*/
/* Process                                                                   */
/*--------------------------------------------------------------------------*/

TIKU_PROCESS(http_fetch_process, "HTTP-Fetch");

static struct tiku_timer fetch_timer;

TIKU_PROCESS_THREAD(http_fetch_process, ev, data)
{
    TIKU_PROCESS_BEGIN();

    tiku_common_led1_init();
    tiku_common_led2_init();

    /* Debug: show FRAM state before doing anything */
    tiku_uart_printf("\r\n[FRAM] magic=%x status=%u "
                     "body_len=%u\r\n",
                     (unsigned)resp_magic,
                     (unsigned)resp_status,
                     (unsigned)resp_body_len);

    /* If FRAM has a cached response, skip fetch and print */
    if (resp_magic == RESP_MAGIC && resp_body_len > 0) {
        tiku_timer_set_event(&fetch_timer,
                             TIKU_CLOCK_SECOND * 2);
        TIKU_PROCESS_WAIT_EVENT_UNTIL(
            ev == TIKU_EVENT_TIMER);
        goto print_result;
    }

    /* Wait for net process to initialise SLIP */
    tiku_timer_set_event(&fetch_timer,
                         TIKU_CLOCK_SECOND * 2);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    /*----------------------------------------------------------*/
    /* 1. TCP connect to gateway                                */
    /*----------------------------------------------------------*/
    connected = 0;
    closed = 0;
    conn = tiku_kits_net_tcp_connect(
        gateway_ip, FETCH_DST_PORT, FETCH_SRC_PORT,
        fetch_recv_cb, fetch_event_cb);

    if (conn == NULL) {
        tiku_common_led2_on();
        TIKU_PROCESS_EXIT();
    }

    /* Wait for 3-way handshake (30 s timeout) */
    tiku_timer_set_event(&fetch_timer,
                         TIKU_CLOCK_SECOND * 30);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(
        ev == TIKU_EVENT_TIMER || connected || closed);

    if (!connected) {
        tiku_common_led2_on();
        TIKU_PROCESS_EXIT();
    }

    /*----------------------------------------------------------*/
    /* 2. Send HTTP GET request                                 */
    /*----------------------------------------------------------*/
    {
        uint8_t req_buf[128];
        uint16_t req_len;

        req_len = tiku_kits_net_http_build_request(
            "GET", FETCH_HOST, FETCH_PATH,
            NULL, 0,
            req_buf, sizeof(req_buf));

        tiku_kits_net_tcp_send(conn, req_buf, req_len);
    }

    /*----------------------------------------------------------*/
    /* 3. Receive and parse HTTP response                       */
    /*----------------------------------------------------------*/
    tiku_kits_net_http_parser_init(
        &parser, resp_buf, FETCH_RESP_SIZE);

    /*
     * Simple approach: wait, read, repeat.  Each 1-second
     * pause lets the gateway send a burst and the TCP stack
     * buffer it.  Reading frees buffer space for the next
     * burst.  Repeat 10 times to handle ~600 bytes at
     * 9600 baud with 256-byte RX buffer.
     */
    {
        static uint8_t round;
        for (round = 0; round < 10; round++) {
            uint8_t chunk[88];
            uint16_t n;

            tiku_timer_set_event(&fetch_timer,
                                 TIKU_CLOCK_SECOND);
            TIKU_PROCESS_WAIT_EVENT_UNTIL(
                ev == TIKU_EVENT_TIMER);

            do {
                n = tiku_kits_net_tcp_read(
                    conn, chunk, sizeof(chunk));
                if (n > 0) {
                    uint16_t saved = tiku_mpu_unlock_nvm();
                    tiku_kits_net_http_parser_feed(
                        &parser, chunk, n);
                    tiku_mpu_lock_nvm(saved);
                }
            } while (n > 0);
        }
    }

    /*----------------------------------------------------------*/
    /* 4. Close TCP and save to FRAM                            */
    /*----------------------------------------------------------*/
    if (conn != NULL) {
        tiku_kits_net_tcp_abort(conn);
    }

    {
        uint16_t saved = tiku_mpu_unlock_nvm();
        resp_status   = parser.status_code;
        resp_body_len = parser.body_len;
        resp_magic    = RESP_MAGIC;
        tiku_mpu_lock_nvm(saved);
    }

    tiku_timer_set_event(&fetch_timer,
                         TIKU_CLOCK_SECOND * 2);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    /*----------------------------------------------------------*/
    /* 5. Print from FRAM (also entry for cached boot)          */
    /*----------------------------------------------------------*/
print_result:

    if (resp_status == 200 && resp_body_len > 0) {
        uint16_t i;

        tiku_common_led1_on();

        tiku_uart_printf("\r\n");
        tiku_uart_printf("=== HTTP %u OK - %u bytes "
                         "of body ===\r\n",
                         (unsigned)resp_status,
                         (unsigned)resp_body_len);
        tiku_uart_printf("\r\n");

        for (i = 0; i < resp_body_len; i++) {
            uint8_t c = resp_buf[i];
            if (c == '\n') {
                tiku_uart_putc('\r');
            }
            if (c >= 0x20 || c == '\n' || c == '\r'
                || c == '\t') {
                tiku_uart_putc((char)c);
            }
        }

        tiku_uart_printf("\r\n\r\n");
        tiku_uart_printf("=== End (%u bytes in FRAM) "
                         "===\r\n",
                         (unsigned)resp_body_len);
    } else {
        tiku_common_led2_on();
        tiku_uart_printf("\r\n");
        tiku_uart_printf("=== HTTP FAILED - status %u, "
                         "%u bytes ===\r\n",
                         (unsigned)resp_status,
                         (unsigned)resp_body_len);
    }

    TIKU_PROCESS_END();
}

/*--------------------------------------------------------------------------*/
/* Autostart                                                                 */
/*--------------------------------------------------------------------------*/
TIKU_AUTOSTART_PROCESSES(&tiku_kits_net_process,
                         &http_fetch_process);

#endif /* TIKU_EXAMPLE_HTTP_FETCH && HAS_TIKUKITS && TCP_ENABLE */
