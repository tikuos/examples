/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * crit_defer.c - TikuOS Example 21: Cooperative crit window
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file    crit_defer.c
 * @brief   TikuOS Example 21 - Bit-bang under tiku_crit_begin_defer()
 *
 * Same physical pattern as Example 20 (0xA5 0x5A on the bit-bang pin,
 * once per second), but the wrapping critical window uses the
 * cooperative variant -- tiku_crit_begin_defer() -- which does NOT
 * mask any peripheral IE bits. The system tick keeps firing, UART
 * RX continues to receive, ADC conversions continue to complete, etc.
 *
 * Built to make the two-mode design tangible: with this example
 * loaded, run `make monitor` on the LaunchPad UART and type while
 * the packet bursts are happening -- characters survive because UART
 * RX IE was never cleared. With Example 20 loaded, the same input
 * stream drops bytes that arrive during the ~1.6 ms window.
 *
 * The trade-off: bit edges are ~3-5 us jitterier per Timer A0 tick
 * that fires during the packet (~one tick per 7.81 ms, so usually
 * zero or one tick lands inside a 1.6 ms packet -- easier to see
 * with a stress source like the shell running).
 *
 * Hardware: MSP430FR5969 LaunchPad
 *   - LED1 (red, P4.6)              -- per-packet heartbeat
 *   - Bit-bang pin (P1.4 by default) -- header J1.5, GND on J1.20
 */

#include "tiku.h"

#if TIKU_EXAMPLE_CRIT_DEFER

#include <kernel/timers/tiku_bitbang.h>
#include <kernel/timers/tiku_crit.h>

#define BIT_PERIOD_TICKS  100u
#define PATTERN_BITS      16u
#define CRIT_BUDGET_US    5000u

static const uint8_t pattern[2] = { 0xA5, 0x5A };

TIKU_PROCESS(crit_defer_process, "CritDefer");

static volatile uint8_t bb_done;

static void bb_on_done(void *ctx)
{
    (void)ctx;
    bb_done = 1;
    tiku_process_poll(&crit_defer_process);
}

static struct tiku_timer cadence_timer;

TIKU_PROCESS_THREAD(crit_defer_process, ev, data)
{
    static tiku_bitbang_t cfg;

    TIKU_PROCESS_BEGIN();

    tiku_common_led1_init();

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

        bb_done = 0;

        /* Cooperative window: defers the software-timer dispatcher
         * but leaves every peripheral IE bit untouched. Other ISRs
         * (Timer A0 system tick, UART RX, ADC done, ...) fire
         * normally during the bit-bang. */
        tiku_crit_begin_defer(CRIT_BUDGET_US);

        tiku_bitbang_tx(&cfg);
        TIKU_PROCESS_WAIT_EVENT_UNTIL(bb_done);

        tiku_crit_end();

        tiku_common_led1_toggle();
        tiku_timer_reset(&cadence_timer);
    }

    TIKU_PROCESS_END();
}

TIKU_AUTOSTART_PROCESSES(&crit_defer_process);

#endif /* TIKU_EXAMPLE_CRIT_DEFER */
