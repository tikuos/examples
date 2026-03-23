/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * https_direct.c - TikuOS Example 19: HTTPS GET via PSK-TLS gateway
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
 * @file    https_direct.c
 * @brief   TikuOS Example 19 - HTTPS GET via PSK-TLS gateway
 *
 * The MSP430 performs a TLS 1.3 PSK-encrypted HTTPS GET request.
 * The full on-device sequence is:
 *   1. DNS resolve hostname (gateway's DNS returns gateway IP)
 *   2. TCP connect to the resolved IP (gateway) on port 443
 *   3. TLS 1.3 PSK handshake (AES-128-GCM-SHA256)
 *   4. Send HTTP/1.1 GET request (encrypted)
 *   5. Receive HTTP response (encrypted)
 *   6. Parse response, store body in FRAM
 *
 * All encryption/decryption runs on the MSP430 itself.  The host
 * PC runs a PSK-TLS gateway (tools/https_psk_gateway.py) that
 * terminates the PSK-TLS session and proxies the request to the
 * real HTTPS endpoint on the internet.
 *
 * Host setup:
 *   @code
 *     # Terminal 1: SLIP bridge
 *     sudo tools/slip_bridge.sh /dev/ttyUSB0
 *
 *     # Terminal 2: PSK-TLS gateway
 *     sudo python3 tools/https_psk_gateway.py
 *   @endcode
 *
 * Then press RESET on the LaunchPad.
 *
 * Hardware:
 *   - MSP430FR5969 LaunchPad with external FTDI/CP2102 adapter
 *   - UART: 9600 baud, 8N1
 *   - TikuOS IP: 172.16.7.2
 *   - Gateway IP: 172.16.7.1
 *
 * What you learn:
 *   - Real TLS 1.3 encryption on an MCU with 2 KB SRAM
 *   - AES-128-GCM authenticated encryption
 *   - PSK key exchange (no certificates)
 *   - Blocking HTTP client API with TLS
 *   - FRAM-backed response buffers
 */

#include "tiku.h"

#if TIKU_EXAMPLE_HTTPS_DIRECT && defined(HAS_TIKUKITS) && \
    TIKU_KITS_NET_HTTP_ENABLE

#include "tikukits/net/tiku_kits_net.h"
#include "tikukits/net/ipv4/tiku_kits_net_ipv4.h"
#include "tikukits/net/ipv4/tiku_kits_net_dns.h"
#include "tikukits/net/http/tiku_kits_net_http.h"
#include "tikukits/crypto/tls/tiku_kits_crypto_tls.h"
#include <kernel/memory/tiku_mem.h>
#include <stddef.h>

/*--------------------------------------------------------------------------*/
/* Configuration                                                             */
/*--------------------------------------------------------------------------*/

/** Website to fetch (sent as Host header; gateway proxies to real site) */
#define HTTPS_HOST      "example.com"

/** Request path */
#define HTTPS_PATH      "/"

/** Response body buffer size (FRAM-backed) */
#define HTTPS_RESP_SIZE 256

/** Gateway IP — runs DNS responder + PSK-TLS proxy */
static const uint8_t gateway_ip[4] = {172, 16, 7, 1};

/*--------------------------------------------------------------------------*/
/* TLS PSK credentials (must match gateway configuration)                    */
/*--------------------------------------------------------------------------*/

static const uint8_t psk_key[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
};

static const uint8_t psk_identity[] = "tikuos_client";

/*--------------------------------------------------------------------------*/
/* FRAM-backed response (survives reset and power cycle)                     */
/*--------------------------------------------------------------------------*/

__attribute__((section(".persistent"), aligned(2)))
static uint8_t resp_buf[HTTPS_RESP_SIZE];

__attribute__((section(".persistent"), aligned(2)))
static uint16_t resp_status;

__attribute__((section(".persistent"), aligned(2)))
static uint16_t resp_body_len;

#define RESP_MAGIC  0xBEEF

__attribute__((section(".persistent"), aligned(2)))
static uint16_t resp_magic;

/*--------------------------------------------------------------------------*/
/* Process                                                                   */
/*--------------------------------------------------------------------------*/

TIKU_PROCESS(https_direct_process, "HTTPS-Direct");

static struct tiku_timer htimer;

TIKU_PROCESS_THREAD(https_direct_process, ev, data)
{
    TIKU_PROCESS_BEGIN();

    tiku_common_led1_init();
    tiku_common_led2_init();

    /*----------------------------------------------------------*/
    /* Check FRAM for a cached response from a previous boot    */
    /*----------------------------------------------------------*/
    if (resp_magic == RESP_MAGIC && resp_body_len > 0) {
        tiku_timer_set_event(&htimer,
                             TIKU_CLOCK_SECOND * 2);
        TIKU_PROCESS_WAIT_EVENT_UNTIL(
            ev == TIKU_EVENT_TIMER);
        goto print_result;
    }

    /* Wait for net process to initialise SLIP */
    tiku_timer_set_event(&htimer, TIKU_CLOCK_SECOND * 3);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    /*----------------------------------------------------------*/
    /* Set DNS server to gateway (it returns its own IP)         */
    /*----------------------------------------------------------*/
    tiku_kits_net_dns_init();
    tiku_kits_net_dns_set_server(gateway_ip);

    /*----------------------------------------------------------*/
    /* Configure TLS PSK                                        */
    /*----------------------------------------------------------*/
    tiku_kits_crypto_tls_init();
    tiku_kits_crypto_tls_set_psk(
        psk_key, sizeof(psk_key),
        psk_identity, sizeof(psk_identity) - 1);

    /*----------------------------------------------------------*/
    /* Perform HTTPS GET (DNS -> TCP -> TLS -> HTTP)             */
    /*----------------------------------------------------------*/
    {
        uint16_t resp_len = 0;
        int8_t rc;

        rc = tiku_kits_net_http_get(
            HTTPS_HOST, HTTPS_PATH, NULL,
            resp_buf, HTTPS_RESP_SIZE, &resp_len);

        /* Save result to FRAM */
        {
            uint16_t saved = tiku_mpu_unlock_nvm();
            resp_status   = tiku_kits_net_http_get_status_code();
            resp_body_len = resp_len;
            resp_magic    = RESP_MAGIC;
            tiku_mpu_lock_nvm(saved);
        }

        if (rc != TIKU_KITS_NET_OK) {
            tiku_uart_printf("\r\n[HTTPS] failed: rc=%d "
                             "status=%u\r\n",
                             (int)rc,
                             (unsigned)resp_status);
            tiku_common_led2_on();
            TIKU_PROCESS_EXIT();
        }
    }

    /* Short delay for SLIP to quiesce */
    tiku_timer_set_event(&htimer, TIKU_CLOCK_SECOND * 2);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    /*----------------------------------------------------------*/
    /* Print from FRAM                                          */
    /*----------------------------------------------------------*/
print_result:

    if (resp_status == 200 && resp_body_len > 0) {
        uint16_t i;

        tiku_common_led1_on();

        tiku_uart_printf("\r\n");
        tiku_uart_printf("=== HTTPS %u OK - %u bytes "
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
        tiku_uart_printf("=== HTTPS FAILED - status %u, "
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
                         &https_direct_process);

#endif /* TIKU_EXAMPLE_HTTPS_DIRECT && HAS_TIKUKITS && HTTP_ENABLE */
