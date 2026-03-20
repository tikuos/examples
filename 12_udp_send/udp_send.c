/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * udp_send.c - TikuOS Example 12: UDP Sender over SLIP
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

/**
 * @file    udp_send.c
 * @brief   TikuOS Example 12 - UDP Sender over SLIP
 *
 * Demonstrates sending UDP datagrams from the MSP430 to a host PC
 * using the TikuKits networking stack over a SLIP serial link.
 *
 * The MSP430 sends a short message every 2 seconds to the host at
 * 172.16.7.1:5000. A companion Python script (labs/udp_receiver.py)
 * runs on the host, decodes SLIP frames from the serial port, and
 * displays the received UDP payloads.
 *
 * Hardware setup:
 *   - MSP430FR5969 LaunchPad connected via USB (eZ-FET backchannel UART)
 *   - UART: 9600 baud, 8N1
 *   - TikuOS IP: 172.16.7.2 (default)
 *   - Host IP:   172.16.7.1
 *
 * What you learn:
 *   - Starting the net process alongside an application process
 *   - Sending UDP datagrams with tiku_kits_net_udp_send()
 *   - SLIP transport over UART to a host PC
 *   - Using a Python script to receive and decode packets
 */

#include "tiku.h"

#if TIKU_EXAMPLE_UDP_SEND && defined(HAS_TIKUKITS)

#include "tikukits/net/tiku_kits_net.h"
#include "tikukits/net/ipv4/tiku_kits_net_ipv4.h"
#include "tikukits/net/ipv4/tiku_kits_net_udp.h"
#include <string.h>

/*--------------------------------------------------------------------------*/
/* Configuration                                                             */
/*--------------------------------------------------------------------------*/

/** Host IP address (the PC running udp_receiver.py) */
static const uint8_t host_ip[4] = {172, 16, 7, 1};

/** Destination port on the host */
#define UDP_SEND_DST_PORT       5000

/** Source port on the MSP430 */
#define UDP_SEND_SRC_PORT       3000

/** Send interval in seconds */
#define UDP_SEND_INTERVAL       2

/*--------------------------------------------------------------------------*/
/* Process declarations                                                      */
/*--------------------------------------------------------------------------*/

TIKU_PROCESS(udp_send_process, "UDP-Send");

/*--------------------------------------------------------------------------*/
/* Process thread                                                            */
/*--------------------------------------------------------------------------*/
static struct tiku_timer send_timer;
static uint16_t pkt_seq;

TIKU_PROCESS_THREAD(udp_send_process, ev, data)
{
    TIKU_PROCESS_BEGIN();

    /* Initialize LEDs for status indication */
    tiku_common_led1_init();
    tiku_common_led2_init();

    /* Brief delay to let the net process initialise SLIP */
    tiku_timer_set_event(&send_timer,
                         TIKU_CLOCK_SECOND * UDP_SEND_INTERVAL);

    pkt_seq = 0;

    while (1) {
        TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

        {
            /*
             * Build a simple text payload with a sequence number.
             * Format: "TikuOS #NNNNN" (up to 14 bytes)
             */
            char msg[32];
            uint8_t len;
            int8_t rc;
            uint16_t s;
            char *p;

            /* Manual uint16 -> decimal to avoid printf/snprintf */
            p = msg;
            *p++ = 'T'; *p++ = 'i'; *p++ = 'k'; *p++ = 'u';
            *p++ = 'O'; *p++ = 'S'; *p++ = ' '; *p++ = '#';

            /* Convert sequence number to decimal string */
            {
                char digits[5];
                uint8_t ndigits = 0;

                s = pkt_seq;
                if (s == 0) {
                    digits[ndigits++] = '0';
                } else {
                    while (s > 0) {
                        digits[ndigits++] = '0' + (char)(s % 10);
                        s /= 10;
                    }
                }
                /* Reverse digits into msg */
                while (ndigits > 0) {
                    *p++ = digits[--ndigits];
                }
            }
            len = (uint8_t)(p - msg);

            rc = tiku_kits_net_udp_send(host_ip, UDP_SEND_DST_PORT,
                                        UDP_SEND_SRC_PORT,
                                        (const uint8_t *)msg, len);

            if (rc == TIKU_KITS_NET_OK) {
                tiku_common_led1_toggle();   /* Green: packet sent */
            } else {
                tiku_common_led2_toggle();   /* Red: send failed */
            }

            pkt_seq++;
        }

        tiku_timer_reset(&send_timer);
    }

    TIKU_PROCESS_END();
}

/*--------------------------------------------------------------------------*/
/* Autostart: net process (SLIP polling) + our send process                  */
/*--------------------------------------------------------------------------*/
TIKU_AUTOSTART_PROCESSES(&tiku_kits_net_process, &udp_send_process);

#endif /* TIKU_EXAMPLE_UDP_SEND && HAS_TIKUKITS */
