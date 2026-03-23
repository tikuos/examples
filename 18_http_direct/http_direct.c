/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * http_direct.c - TikuOS Example 18: Direct HTTP GET to the internet
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
 * @file    http_direct.c
 * @brief   TikuOS Example 18 - Direct HTTP GET to the internet
 *
 * The MSP430 resolves a hostname via DNS, opens a TCP connection
 * to the real server, and fetches a webpage over plain HTTP ---
 * no helper script, no proxy.  The host PC simply bridges SLIP
 * to the internet using standard Linux networking.
 *
 * The full sequence running on the MCU:
 *   1. DNS resolve "example.com" via Google DNS (8.8.8.8)
 *   2. TCP connect to the resolved IP on port 80
 *   3. Send HTTP/1.1 GET request
 *   4. Receive and parse the real HTTP response
 *   5. Store body in FRAM, indicate result via LEDs
 *
 * Host setup (one-time, requires root):
 *   @code
 *     # Set up SLIP interface and internet bridge
 *     sudo tools/slip_bridge.sh /dev/ttyUSB0
 *   @endcode
 *
 *   Or manually:
 *   @code
 *     sudo slattach -L -p slip -s 9600 /dev/ttyUSB0 &
 *     sudo ip addr add 172.16.7.1/32 peer 172.16.7.2 dev sl0
 *     sudo ip link set sl0 up
 *     sudo sysctl -w net.ipv4.ip_forward=1
 *     sudo iptables -t nat -A POSTROUTING -s 172.16.7.2 \
 *          -j MASQUERADE
 *   @endcode
 *
 * Then reset the MSP430.  LED1 = HTTP 200 received,
 * LED2 = error.  Inspect resp_buf via debugger to see
 * the actual HTML from example.com.
 *
 * Hardware:
 *   - MSP430FR5969 LaunchPad with external FTDI/CP2102 adapter
 *   - UART: 9600 baud, 8N1
 *   - TikuOS IP: 172.16.7.2
 *
 * What you learn:
 *   - Full internet access from an MCU with 2 KB SRAM
 *   - DNS resolution to a real DNS server (8.8.8.8)
 *   - TCP connection to a real HTTP server
 *   - HTTP request/response over a constrained link
 *   - SLIP bridging with Linux NAT
 */

#include "tiku.h"

#if TIKU_EXAMPLE_HTTP_DIRECT && defined(HAS_TIKUKITS) && \
    TIKU_KITS_NET_TCP_ENABLE

#include "tikukits/net/tiku_kits_net.h"
#include "tikukits/net/ipv4/tiku_kits_net_ipv4.h"
#include "tikukits/net/ipv4/tiku_kits_net_udp.h"
#include "tikukits/net/ipv4/tiku_kits_net_dns.h"
#include "tikukits/net/ipv4/tiku_kits_net_tcp.h"
#include "tikukits/net/http/tiku_kits_net_http.h"
#include <stddef.h>

/*--------------------------------------------------------------------------*/
/* Configuration                                                             */
/*--------------------------------------------------------------------------*/

/** DNS server (Google Public DNS) */
static const uint8_t dns_server[4] = {8, 8, 8, 8};

/** Website to fetch */
#define HTTP_HOST       "example.com"
#define HTTP_PATH       "/"
#define HTTP_PORT       80

/** Local TCP port */
#define HTTP_SRC_PORT   3003

/** Response body buffer size (FRAM) */
#define HTTP_RESP_SIZE  256

/** DNS poll interval (ms) */
#define DNS_POLL_MS     100

/** Maximum DNS polls before timeout */
#define DNS_MAX_POLLS   100

/*--------------------------------------------------------------------------*/
/* FRAM-backed response (survives reset and power cycle)                     */
/*--------------------------------------------------------------------------*/

__attribute__((section(".persistent"), aligned(2)))
static uint8_t resp_buf[HTTP_RESP_SIZE];

__attribute__((section(".persistent"), aligned(2)))
static uint16_t resp_status;    /* HTTP status code */

__attribute__((section(".persistent"), aligned(2)))
static uint16_t resp_body_len;  /* Bytes of body stored */

/** Magic value to detect a valid cached response */
#define RESP_MAGIC  0xBEEF

__attribute__((section(".persistent"), aligned(2)))
static uint16_t resp_magic;

/** Diagnostic info saved to FRAM (survives reset) */
__attribute__((section(".persistent"), aligned(2)))
static int16_t diag_send_rc;

__attribute__((section(".persistent"), aligned(2)))
static uint16_t diag_req_len;

__attribute__((section(".persistent"), aligned(2)))
static uint16_t diag_connected;

__attribute__((section(".persistent"), aligned(2)))
static uint16_t diag_snd_mss;

__attribute__((section(".persistent"), aligned(2)))
static uint16_t diag_rx_total;

__attribute__((section(".persistent"), aligned(2)))
static uint16_t diag_closed_flag;

/*--------------------------------------------------------------------------*/
/* State                                                                     */
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
http_recv_cb(struct tiku_kits_net_tcp_conn *c,
             uint16_t available)
{
    (void)c;
    (void)available;
    data_ready = 1;
}

static void
http_event_cb(struct tiku_kits_net_tcp_conn *c,
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

TIKU_PROCESS(http_direct_process, "HTTP-Direct");

static struct tiku_timer htimer;

TIKU_PROCESS_THREAD(http_direct_process, ev, data)
{
    static uint8_t server_ip[4];
    static uint16_t polls;
    static tiku_kits_net_dns_state_t dns_state;

    TIKU_PROCESS_BEGIN();

    tiku_common_led1_init();
    tiku_common_led2_init();

    /* Boot diagnostic — printed BEFORE SLIP starts (3 s delay
     * follows), so raw text does not corrupt SLIP framing. */
    tiku_uart_printf("\r\n[FRAM] magic=%x status=%u "
                     "body_len=%u\r\n",
                     (unsigned)resp_magic,
                     (unsigned)resp_status,
                     (unsigned)resp_body_len);
    tiku_uart_printf("[DIAG] conn=%u mss=%u "
                     "req=%u send=%d "
                     "rx=%u closed=%u\r\n",
                     (unsigned)diag_connected,
                     (unsigned)diag_snd_mss,
                     (unsigned)diag_req_len,
                     (int)diag_send_rc,
                     (unsigned)diag_rx_total,
                     (unsigned)diag_closed_flag);

    /*----------------------------------------------------------*/
    /* Check FRAM for a cached response from a previous boot.   */
    /* If present, skip the fetch and just print it.            */
    /*----------------------------------------------------------*/
    if (resp_magic == RESP_MAGIC && resp_body_len > 0) {
        /* Short delay so the user can open a terminal */
        tiku_timer_set_event(&htimer,
                             TIKU_CLOCK_SECOND * 2);
        TIKU_PROCESS_WAIT_EVENT_UNTIL(
            ev == TIKU_EVENT_TIMER);
        goto print_result;
    }
    tiku_common_led2_init();

    /* Wait for net process to initialise SLIP */
    tiku_timer_set_event(&htimer, TIKU_CLOCK_SECOND * 3);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    /*----------------------------------------------------------*/
    /* Step 1: DNS — resolve "example.com" via 8.8.8.8          */
    /*----------------------------------------------------------*/
    tiku_kits_net_dns_init();
    tiku_kits_net_dns_set_server(dns_server);

    if (tiku_kits_net_dns_resolve(HTTP_HOST)
        != TIKU_KITS_NET_OK) {
        tiku_common_led2_on();
        TIKU_PROCESS_EXIT();
    }

    /* Poll until resolved (yield each iteration so the net
     * process can poll SLIP and deliver the DNS response) */
    for (polls = 0; polls < DNS_MAX_POLLS; polls++) {
        tiku_timer_set_event(&htimer,
            TIKU_CLOCK_MS_TO_TICKS(DNS_POLL_MS));
        TIKU_PROCESS_WAIT_EVENT_UNTIL(
            ev == TIKU_EVENT_TIMER);

        tiku_kits_net_dns_poll();
        dns_state = tiku_kits_net_dns_get_state();
        if (dns_state == TIKU_KITS_NET_DNS_STATE_DONE
            || dns_state == TIKU_KITS_NET_DNS_STATE_ERROR) {
            break;
        }
    }

    if (dns_state != TIKU_KITS_NET_DNS_STATE_DONE) {
        tiku_kits_net_dns_abort();
        tiku_common_led2_on();  /* DNS failed */
        TIKU_PROCESS_EXIT();
    }

    tiku_kits_net_dns_get_addr(server_ip);
    /* server_ip now holds the real IP of example.com */

    /* No printf here — raw text corrupts SLIP stream */

    /*----------------------------------------------------------*/
    /* Step 2: TCP — connect to the real server on port 80      */
    /*----------------------------------------------------------*/
    connected = 0;
    closed = 0;
    conn = tiku_kits_net_tcp_connect(
        server_ip, HTTP_PORT, HTTP_SRC_PORT,
        http_recv_cb, http_event_cb);

    if (conn == NULL) {
        tiku_uart_printf("[TCP] connect failed: NULL\r\n");
        tiku_common_led2_on();  /* No free TCP slot */
        TIKU_PROCESS_EXIT();
    }

    /* No printf here — raw text corrupts SLIP stream */

    /* Poll for 3-way handshake every 100 ms (up to 30 s).
     * The PROCESS_WAIT_EVENT_UNTIL only wakes on events;
     * the callback sets connected=1 but does not post an
     * event, so we must use short polling intervals. */
    for (polls = 0; polls < 300 && !connected && !closed;
         polls++) {
        tiku_timer_set_event(&htimer,
            TIKU_CLOCK_MS_TO_TICKS(100));
        TIKU_PROCESS_WAIT_EVENT_UNTIL(
            ev == TIKU_EVENT_TIMER);
    }

    if (!connected) {
        tiku_common_led2_on();  /* TCP connect failed */
        TIKU_PROCESS_EXIT();
    }

    {
        uint16_t saved = tiku_mpu_unlock_nvm();
        diag_connected = 1;
        diag_snd_mss = conn->snd_mss;
        tiku_mpu_lock_nvm(saved);
    }

    /*----------------------------------------------------------*/
    /* Step 3: HTTP — send GET request to the real server       */
    /*----------------------------------------------------------*/
    {
        uint8_t req_buf[128];
        uint16_t req_len;
        int8_t send_rc;

        req_len = tiku_kits_net_http_build_request(
            "GET", HTTP_HOST, HTTP_PATH,
            NULL, 0,
            req_buf, sizeof(req_buf));

        send_rc = tiku_kits_net_tcp_send(
            conn, req_buf, req_len);

        {
            uint16_t saved = tiku_mpu_unlock_nvm();
            diag_req_len = req_len;
            diag_send_rc = (int16_t)send_rc;
            tiku_mpu_lock_nvm(saved);
        }
    }

    /*----------------------------------------------------------*/
    /* Step 4: Receive the real HTTP response                    */
    /*----------------------------------------------------------*/
    tiku_kits_net_http_parser_init(
        &parser, resp_buf, HTTP_RESP_SIZE);

    {
        static uint8_t idle_polls;
        idle_polls = 0;

        while (idle_polls < 20) {
            tiku_timer_set_event(&htimer,
                                 TIKU_CLOCK_SECOND);
            TIKU_PROCESS_WAIT_EVENT_UNTIL(
                ev == TIKU_EVENT_TIMER);

            {
                uint8_t chunk[88];
                uint16_t n;
                uint8_t got_data = 0;
                do {
                    n = tiku_kits_net_tcp_read(
                        conn, chunk, sizeof(chunk));
                    if (n > 0) {
                        uint16_t saved =
                            tiku_mpu_unlock_nvm();
                        tiku_kits_net_http_parser_feed(
                            &parser, chunk, n);
                        tiku_mpu_lock_nvm(saved);
                        got_data = 1;
                    }
                } while (n > 0);

                if (got_data) {
                    idle_polls = 0;
                } else {
                    idle_polls++;
                }
            }

            if (closed) {
                break;
            }
        }
    }

    /*----------------------------------------------------------*/
    /* Step 5: Close TCP and save result to FRAM                */
    /*----------------------------------------------------------*/
    if (conn != NULL) {
        tiku_kits_net_tcp_abort(conn);  /* RST — instant */
    }

    /* Save result + diagnostics to FRAM */
    {
        uint16_t saved = tiku_mpu_unlock_nvm();
        resp_status   = parser.status_code;
        resp_body_len = parser.body_len;
        resp_magic    = RESP_MAGIC;
        diag_rx_total = parser.body_len;
        diag_closed_flag = closed;
        tiku_mpu_lock_nvm(saved);
    }

    /* Short delay for SLIP to quiesce */
    tiku_timer_set_event(&htimer, TIKU_CLOCK_SECOND * 2);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    /*----------------------------------------------------------*/
    /* Step 6: Print from FRAM                                   */
    /*----------------------------------------------------------*/
    /*
     * This label is also the entry point when a cached
     * response is found in FRAM at boot (no SLIP needed).
     *
     * tiku_uart_printf() writes directly to the hardware
     * UART, bypassing the TIKU_PRINTF suppression that
     * is active during SLIP operation.
     */
print_result:

    tiku_uart_printf("\r\n[FRAM] magic=%x status=%u "
                     "body_len=%u\r\n",
                     (unsigned)resp_magic,
                     (unsigned)resp_status,
                     (unsigned)resp_body_len);
    tiku_uart_printf("[DIAG] conn=%u mss=%u "
                     "req=%u send=%d "
                     "rx=%u closed=%u\r\n",
                     (unsigned)diag_connected,
                     (unsigned)diag_snd_mss,
                     (unsigned)diag_req_len,
                     (int)diag_send_rc,
                     (unsigned)diag_rx_total,
                     (unsigned)diag_closed_flag);

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
                         &http_direct_process);

#endif /* TIKU_EXAMPLE_HTTP_DIRECT && HAS_TIKUKITS && TCP_ENABLE */
