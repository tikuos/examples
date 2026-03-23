/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * http_get.c - TikuOS Example 15: HTTPS GET over SLIP
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
 * @file    http_get.c
 * @brief   TikuOS Example 15 - HTTPS GET over SLIP
 *
 * Demonstrates performing an HTTP/1.1 GET request over TLS
 * using the TikuKits HTTP client.  The full sequence is:
 * DNS resolve -> TCP connect -> TLS handshake -> send HTTP
 * GET -> receive response -> parse status + body.
 *
 * The MSP430 connects to a configured host via HTTPS (port
 * 443), performs a GET request, and indicates success/failure
 * via LEDs.  The response body is stored in a FRAM-backed
 * buffer for inspection via the debugger.
 *
 * Hardware setup:
 *   - MSP430FR5969 LaunchPad with external FTDI/CP2102 adapter
 *   - UART: 9600 baud, 8N1
 *   - TikuOS IP: 172.16.7.2 (default)
 *   - Host / gateway: 172.16.7.1
 *   - TLS PSK must be pre-configured via tls_set_psk()
 *
 * What you learn:
 *   - Performing an HTTP GET with tiku_kits_net_http_get()
 *   - Setting up TLS PSK credentials
 *   - Checking HTTP status codes
 *   - Using FRAM-backed response buffers
 *   - Error handling for DNS, TCP, TLS, and HTTP failures
 */

#include "tiku.h"

#if TIKU_EXAMPLE_HTTP_GET && defined(HAS_TIKUKITS) && \
    TIKU_KITS_NET_HTTP_ENABLE

#include "tikukits/net/tiku_kits_net.h"
#include "tikukits/net/ipv4/tiku_kits_net_ipv4.h"
#include "tikukits/net/http/tiku_kits_net_http.h"
#include "tikukits/crypto/tls/tiku_kits_crypto_tls.h"
#include <stddef.h>

/*--------------------------------------------------------------------------*/
/* Configuration                                                             */
/*--------------------------------------------------------------------------*/

/** Host to connect to */
#define HTTP_HOST       "api.example.com"

/** Request path */
#define HTTP_PATH       "/v1/status"

/** API key (NULL to omit) */
#define HTTP_API_KEY    NULL

/** Response buffer size (FRAM-backed) */
#define HTTP_RESP_SIZE  256

/*--------------------------------------------------------------------------*/
/* TLS PSK credentials (must match server configuration)                     */
/*--------------------------------------------------------------------------*/

static const uint8_t psk_key[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
};

static const uint8_t psk_identity[] = "tikuos_client";

/*--------------------------------------------------------------------------*/
/* FRAM-backed response buffer                                               */
/*--------------------------------------------------------------------------*/

__attribute__((section(".persistent"), aligned(2)))
static uint8_t response_buf[HTTP_RESP_SIZE];

/*--------------------------------------------------------------------------*/
/* Process                                                                   */
/*--------------------------------------------------------------------------*/

TIKU_PROCESS(http_get_process, "HTTP-GET");

static struct tiku_timer http_timer;

TIKU_PROCESS_THREAD(http_get_process, ev, data)
{
    TIKU_PROCESS_BEGIN();

    tiku_common_led1_init();
    tiku_common_led2_init();

    /* Wait for net process to initialise */
    tiku_timer_set_event(&http_timer, TIKU_CLOCK_SECOND * 2);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    /* Configure TLS PSK */
    tiku_kits_crypto_tls_init();
    tiku_kits_crypto_tls_set_psk(
        psk_key, sizeof(psk_key),
        psk_identity, sizeof(psk_identity) - 1);

    /* Perform HTTP GET */
    {
        uint16_t resp_len = 0;
        int8_t rc;

        rc = tiku_kits_net_http_get(
            HTTP_HOST,
            HTTP_PATH,
            HTTP_API_KEY,
            response_buf,
            HTTP_RESP_SIZE,
            &resp_len);

        if (rc == TIKU_KITS_NET_OK) {
            /* HTTP 200 — response body in response_buf */
            tiku_common_led1_on();   /* Green: success */
        } else if (rc == TIKU_KITS_NET_ERR_HTTP_STATUS) {
            /* Non-200 status code */
            tiku_common_led2_on();   /* Red: HTTP error */
        } else if (rc == TIKU_KITS_NET_ERR_HTTP_DNS) {
            /* DNS resolution failed */
            tiku_common_led2_on();
        } else if (rc == TIKU_KITS_NET_ERR_HTTP_TCP) {
            /* TCP connection failed */
            tiku_common_led2_on();
        } else if (rc == TIKU_KITS_NET_ERR_HTTP_TLS) {
            /* TLS handshake failed */
            tiku_common_led2_on();
        } else {
            /* Other error (timeout, overflow) */
            tiku_common_led2_on();
        }

        /*
         * At this point, response_buf contains resp_len bytes
         * of the HTTP response body.  Inspect via debugger or
         * extend this example to process the JSON.
         *
         * The status code can be retrieved with:
         *   tiku_kits_net_http_get_status_code()
         */
    }

    TIKU_PROCESS_END();
}

/*--------------------------------------------------------------------------*/
/* Autostart                                                                 */
/*--------------------------------------------------------------------------*/
TIKU_AUTOSTART_PROCESSES(&tiku_kits_net_process,
                         &http_get_process);

#endif /* TIKU_EXAMPLE_HTTP_GET && HAS_TIKUKITS && HTTP_ENABLE */
