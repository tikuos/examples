/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * bitbang.c - TikuOS Example 20: Hardware-timer bit-bang under crit
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
 * @file    bitbang.c
 * @brief   TikuOS Example 20 - Precision GPIO bit-bang with htimer + crit
 *
 * Once per second, drive a recognizable 16-bit pattern (0xA5 0x5A) on
 * a GPIO pin under a tiku_crit window so every edge fires from the
 * hardware-timer ISR with no peripheral-ISR jitter. LED1 toggles once
 * per packet so the activity is visible without a logic analyzer.
 *
 * Hardware: MSP430FR5969 LaunchPad
 *   - LED1 (red, P4.6)              -- per-packet heartbeat
 *   - Bit-bang pin (P1.4 by default) -- on header J1.5, GND on J1.20
 *
 * What you learn:
 *   - tiku_htimer used as a precise bit clock from inside an ISR
 *   - tiku_bitbang as a fire-and-forget bit-stream driver
 *   - tiku_crit_begin() with TIKU_CRIT_PRESERVE_HTIMER to mask all
 *     other peripheral IRQs while the bit clock keeps firing
 *   - on_done callback running in ISR context to signal completion
 *
 * Capture: a 100 us-per-bit packet on P1.4. The first bit you see is
 * the MSB of 0xA5 (a '1'), so the visible level sequence is
 *   1 0 1 0 0 1 0 1   0 1 0 1 1 0 1 0  ->  idle low.
 */

#include "tiku.h"

#if TIKU_EXAMPLE_BITBANG

/*
 * The bit-bang example needs the board to expose
 * TIKU_BOARD_BSCAT_PORT / TIKU_BOARD_BSCAT_PIN (the GPIO the
 * htimer ISR toggles). Boards that don't wire a backscatter pin
 * (e.g. the FR6989 LaunchPad — the LCD glass owns most of P5/P6)
 * skip the example so a generic build with TIKU_EXAMPLE_BITBANG=1
 * still links.
 */
#if !defined(TIKU_BOARD_BSCAT_PORT) || !defined(TIKU_BOARD_BSCAT_PIN)
#warning "TIKU_EXAMPLE_BITBANG requested but board has no BSCAT pin; skipping."
#else

#include <kernel/timers/tiku_bitbang.h>
#include <kernel/timers/tiku_crit.h>

/*--------------------------------------------------------------------------*/
/* Configuration                                                             */
/*--------------------------------------------------------------------------*/

/** Bit period in htimer ticks. With the default HIGH_ACCURACY preset
 *  this is microseconds (1 tick = 1 us). 100 us/bit -> 10 kbps. */
#define BIT_PERIOD_TICKS        100u

/** Recognizable, asymmetric pattern. 0xA5 = 10100101, 0x5A = 01011010. */
static const uint8_t pattern[2] = { 0xA5, 0x5A };
#define PATTERN_BITS            16u

/** Critical-window budget. PATTERN_BITS * BIT_PERIOD_TICKS = 1.6 ms;
 *  add slack for the first-edge guard time and the trailing idle edge. */
#define CRIT_BUDGET_US          5000u

/*--------------------------------------------------------------------------*/
/* Process                                                                   */
/*--------------------------------------------------------------------------*/

TIKU_PROCESS(bitbang_process, "Bitbang");

/*--------------------------------------------------------------------------*/
/* Completion signal                                                         */
/*--------------------------------------------------------------------------*/

/* Set by the on_done callback (ISR context); polled by the protothread.
 *
 * IMPORTANT: setting the flag alone is not enough -- the protothread
 * is parked in TIKU_PROCESS_WAIT_EVENT_UNTIL and only re-evaluates
 * the condition when the scheduler dispatches the process. We must
 * poke the scheduler from the ISR so the wait can complete. */
static volatile uint8_t bb_done;

static void bb_on_done(void *ctx)
{
    (void)ctx;
    bb_done = 1;
    tiku_process_poll(&bitbang_process);
}

/*--------------------------------------------------------------------------*/
/* Process thread                                                            */
/*--------------------------------------------------------------------------*/

static struct tiku_timer cadence_timer;

TIKU_PROCESS_THREAD(bitbang_process, ev, data)
{
    static tiku_bitbang_t cfg;

    TIKU_PROCESS_BEGIN();

    tiku_common_led1_init();

    /* Build the bit-bang config once. The struct is copied into the
     * engine on each tx call, so it is safe to keep static here. */
    cfg.port           = TIKU_BOARD_BSCAT_PORT;
    cfg.pin            = TIKU_BOARD_BSCAT_PIN;
    cfg.msb_first      = 1;
    cfg.idle_level     = 0;
    cfg.bit_time_ticks = BIT_PERIOD_TICKS;
    cfg.data           = pattern;
    cfg.bit_count      = PATTERN_BITS;
    cfg.on_done        = bb_on_done;
    cfg.ctx            = NULL;

    tiku_timer_set_event(&cadence_timer, TIKU_CLOCK_SECOND);

    while (1) {
        TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

        /* Open the masked window: only the bit-clock ISR may fire. */
        bb_done = 0;
        tiku_crit_begin(CRIT_BUDGET_US, TIKU_CRIT_PRESERVE_HTIMER);

        tiku_bitbang_tx(&cfg);

        /* Wait for the trailing edge. Inside the masked window the
         * only thing that can wake us is the bit-clock ISR firing
         * the on_done callback. */
        TIKU_PROCESS_WAIT_EVENT_UNTIL(bb_done);

        tiku_crit_end();

        tiku_common_led1_toggle();
        tiku_timer_reset(&cadence_timer);
    }

    TIKU_PROCESS_END();
}

/*--------------------------------------------------------------------------*/
/* Autostart                                                                 */
/*--------------------------------------------------------------------------*/
TIKU_AUTOSTART_PROCESSES(&bitbang_process);

#endif /* TIKU_BOARD_BSCAT_PORT */

#endif /* TIKU_EXAMPLE_BITBANG */
