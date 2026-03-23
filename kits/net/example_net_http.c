/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: HTTP/1.1 Client
 *
 * Demonstrates the HTTP request builder and response parser:
 *   - Building a POST request with API key and JSON body
 *   - Building a GET request (no body)
 *   - Parsing an HTTP 200 response (status + headers + body)
 *   - Parsing a non-200 response (error status extraction)
 *   - Body truncation when buffer is smaller than response
 *   - Header order verification
 *
 * This example runs on-device with no network connection.
 * It exercises the request builder and response parser
 * directly, printing results over UART.
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

#include "tikukits/net/http/tiku_kits_net_http.h"

#if defined(HAS_TIKUKITS)

/*---------------------------------------------------------------------------*/
/* Helper: check if a substring exists in a byte buffer                      */
/*---------------------------------------------------------------------------*/

static uint8_t
buf_contains(const uint8_t *buf, uint16_t buf_len,
             const char *needle)
{
    uint16_t nlen = 0;
    const char *p = needle;
    uint16_t i;

    while (*p++) {
        nlen++;
    }
    if (nlen == 0 || nlen > buf_len) {
        return 0;
    }
    for (i = 0; i <= buf_len - nlen; i++) {
        if (memcmp(&buf[i], needle, nlen) == 0) {
            return 1;
        }
    }
    return 0;
}

/*---------------------------------------------------------------------------*/
/* Example 1: Build a POST request                                           */
/*---------------------------------------------------------------------------*/

static void example_build_post(void)
{
    uint8_t buf[300];
    uint16_t len;

    printf("--- Build POST Request ---\n");

    len = tiku_kits_net_http_build_request(
        "POST", "api.anthropic.com", "/v1/messages",
        "sk-ant-test-key", 128,
        buf, sizeof(buf));

    printf("  Request length: %u bytes\n", (unsigned)len);

    /* Verify request line */
    printf("  Request line: %s\n",
           memcmp(buf,
                  "POST /v1/messages HTTP/1.1\r\n",
                  28) == 0
           ? "PASS" : "FAIL");

    /* Verify required headers */
    printf("  Host header: %s\n",
           buf_contains(buf, len,
                        "Host: api.anthropic.com\r\n")
           ? "PASS" : "FAIL");

    printf("  Content-Type: %s\n",
           buf_contains(buf, len,
                        "Content-Type: application/json\r\n")
           ? "PASS" : "FAIL");

    printf("  x-api-key: %s\n",
           buf_contains(buf, len,
                        "x-api-key: sk-ant-test-key\r\n")
           ? "PASS" : "FAIL");

    printf("  anthropic-version: %s\n",
           buf_contains(buf, len,
                        "anthropic-version: 2023-06-01\r\n")
           ? "PASS" : "FAIL");

    printf("  Content-Length: %s\n",
           buf_contains(buf, len,
                        "Content-Length: 128\r\n")
           ? "PASS" : "FAIL");

    printf("  Connection: close: %s\n",
           buf_contains(buf, len,
                        "Connection: close\r\n")
           ? "PASS" : "FAIL");

    /* Verify ends with blank line */
    printf("  Blank line terminator: %s\n",
           (len >= 4
            && buf[len - 4] == '\r' && buf[len - 3] == '\n'
            && buf[len - 2] == '\r' && buf[len - 1] == '\n')
           ? "PASS" : "FAIL");
}

/*---------------------------------------------------------------------------*/
/* Example 2: Build a GET request                                            */
/*---------------------------------------------------------------------------*/

static void example_build_get(void)
{
    uint8_t buf[256];
    uint16_t len;

    printf("--- Build GET Request ---\n");

    len = tiku_kits_net_http_build_request(
        "GET", "example.com", "/status",
        (void *)0, 0,
        buf, sizeof(buf));

    printf("  Request length: %u bytes\n", (unsigned)len);

    printf("  Request line: %s\n",
           memcmp(buf,
                  "GET /status HTTP/1.1\r\n", 22) == 0
           ? "PASS" : "FAIL");

    printf("  No Content-Type: %s\n",
           !buf_contains(buf, len, "Content-Type:")
           ? "PASS" : "FAIL");

    printf("  No Content-Length: %s\n",
           !buf_contains(buf, len, "Content-Length:")
           ? "PASS" : "FAIL");

    printf("  No x-api-key: %s\n",
           !buf_contains(buf, len, "x-api-key:")
           ? "PASS" : "FAIL");

    printf("  Host present: %s\n",
           buf_contains(buf, len, "Host: example.com\r\n")
           ? "PASS" : "FAIL");

    printf("  Connection: close: %s\n",
           buf_contains(buf, len, "Connection: close\r\n")
           ? "PASS" : "FAIL");
}

/*---------------------------------------------------------------------------*/
/* Example 3: Parse a 200 OK response                                        */
/*---------------------------------------------------------------------------*/

static void example_parse_200(void)
{
    static const char resp[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "{\"result\":42}";

    uint8_t body[64];
    tiku_kits_net_http_parser_t parser;

    printf("--- Parse 200 OK Response ---\n");

    tiku_kits_net_http_parser_init(
        &parser, body, sizeof(body));
    tiku_kits_net_http_parser_feed(
        &parser,
        (const uint8_t *)resp,
        (uint16_t)strlen(resp));

    printf("  Status code: %u -> %s\n",
           (unsigned)parser.status_code,
           parser.status_code == 200 ? "PASS" : "FAIL");

    printf("  Body length: %u -> %s\n",
           (unsigned)parser.body_len,
           parser.body_len == 13 ? "PASS" : "FAIL");

    printf("  Body content: %s\n",
           (parser.body_len == 13
            && memcmp(body, "{\"result\":42}", 13) == 0)
           ? "PASS" : "FAIL");
}

/*---------------------------------------------------------------------------*/
/* Example 4: Parse a 404 response                                           */
/*---------------------------------------------------------------------------*/

static void example_parse_404(void)
{
    static const char resp[] =
        "HTTP/1.1 404 Not Found\r\n"
        "\r\n"
        "not found";

    uint8_t body[32];
    tiku_kits_net_http_parser_t parser;

    printf("--- Parse 404 Response ---\n");

    tiku_kits_net_http_parser_init(
        &parser, body, sizeof(body));
    tiku_kits_net_http_parser_feed(
        &parser,
        (const uint8_t *)resp,
        (uint16_t)strlen(resp));

    printf("  Status code: %u -> %s\n",
           (unsigned)parser.status_code,
           parser.status_code == 404 ? "PASS" : "FAIL");

    printf("  Body: \"%.*s\" -> %s\n",
           (int)parser.body_len, (const char *)body,
           (parser.body_len == 9
            && memcmp(body, "not found", 9) == 0)
           ? "PASS" : "FAIL");
}

/*---------------------------------------------------------------------------*/
/* Example 5: Body truncation                                                */
/*---------------------------------------------------------------------------*/

static void example_truncation(void)
{
    static const char resp[] =
        "HTTP/1.1 200 OK\r\n"
        "\r\n"
        "ABCDEFGHIJKLMNOP";

    uint8_t body[8]; /* Only 8 bytes -- body is 16 */
    tiku_kits_net_http_parser_t parser;

    printf("--- Body Truncation ---\n");

    tiku_kits_net_http_parser_init(
        &parser, body, sizeof(body));
    tiku_kits_net_http_parser_feed(
        &parser,
        (const uint8_t *)resp,
        (uint16_t)strlen(resp));

    printf("  Body length: %u (max %u) -> %s\n",
           (unsigned)parser.body_len,
           (unsigned)sizeof(body),
           parser.body_len == 8 ? "PASS" : "FAIL");

    printf("  First 8 bytes: %s\n",
           memcmp(body, "ABCDEFGH", 8) == 0
           ? "PASS" : "FAIL");
}

/*---------------------------------------------------------------------------*/
/* Example 6: Byte-by-byte parsing                                           */
/*---------------------------------------------------------------------------*/

static void example_byte_by_byte(void)
{
    static const char resp[] =
        "HTTP/1.1 200 OK\r\n"
        "Server: tikuOS\r\n"
        "\r\n"
        "OK";

    uint8_t body[16];
    tiku_kits_net_http_parser_t parser;
    uint16_t i;
    uint16_t len = (uint16_t)strlen(resp);

    printf("--- Byte-by-Byte Parsing ---\n");

    tiku_kits_net_http_parser_init(
        &parser, body, sizeof(body));

    for (i = 0; i < len; i++) {
        tiku_kits_net_http_parser_feed(
            &parser,
            (const uint8_t *)&resp[i], 1);
    }

    printf("  Status code: %u -> %s\n",
           (unsigned)parser.status_code,
           parser.status_code == 200 ? "PASS" : "FAIL");

    printf("  Body: \"%.*s\" -> %s\n",
           (int)parser.body_len, (const char *)body,
           (parser.body_len == 2
            && memcmp(body, "OK", 2) == 0)
           ? "PASS" : "FAIL");
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_net_http_run(void)
{
    printf("=== TikuKits HTTP Client Examples ===\n\n");

    example_build_post();
    printf("\n");

    example_build_get();
    printf("\n");

    example_parse_200();
    printf("\n");

    example_parse_404();
    printf("\n");

    example_truncation();
    printf("\n");

    example_byte_by_byte();
    printf("\n");
}

#else /* !HAS_TIKUKITS */

void example_net_http_run(void)
{
    /* HTTP not available */
}

#endif
