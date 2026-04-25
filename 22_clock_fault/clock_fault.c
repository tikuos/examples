/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * clock_fault.c - TikuOS Example 22: Clock-source fault diagnostic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file    clock_fault.c
 * @brief   TikuOS Example 22 - Boot-time XT1 health report
 *
 * On boot, the FR5969/FR5994 clock arch attempts to bring up the
 * 32.768 kHz XT1 crystal as ACLK. If the crystal fails to start
 * within the timeout, the arch silently falls back to VLO (~10 kHz),
 * which makes every software timer expire ~3x slower than
 * TIKU_CLOCK_SECOND implies.
 *
 * Before the fault flag landed, that fallback was invisible at the
 * application level. This example shows the new diagnostic API:
 *
 *   tiku_clock_fault() returns a non-zero code if the configured
 *   source could not be brought up. See tiku_clock_arch_fault_code
 *   in hal/tiku_clock_hal.h for the codes.
 *
 * LED behaviour:
 *   - Healthy (XT1 running, fault == NONE)
 *       LED1 blinks once per second (same as Example 01).
 *   - Faulted (XT1 failed, fault == LFXT_VLO)
 *       LED1 blinks rapidly (8 Hz) so the failure is unmistakable
 *       even without a serial console attached.
 *
 * Hardware: any MSP430 FR-series LaunchPad. The fault path only
 * triggers on devices that try XT1 at boot (FR5969, FR5994). On
 * FR2433 the arch uses REFOCLK and never reports a fault.
 */

#include "tiku.h"

#if TIKU_EXAMPLE_CLOCK_FAULT

#include <hal/tiku_clock_hal.h>

TIKU_PROCESS(clock_fault_process, "ClockFault");

static struct tiku_timer blink_timer;

TIKU_PROCESS_THREAD(clock_fault_process, ev, data)
{
    static tiku_clock_time_t period;
    static unsigned char fault;

    TIKU_PROCESS_BEGIN();

    tiku_common_led1_init();

    /* Snapshot the fault code captured during tiku_clock_arch_init().
     * It is set sticky and only cleared on the next clock init. */
    fault = tiku_clock_fault();

    if (fault == TIKU_CLOCK_ARCH_FAULT_NONE) {
        period = TIKU_CLOCK_SECOND;             /* 1 Hz */
    } else {
        /* 8 Hz blink: TIKU_CLOCK_SECOND / 16 toggles per second
         * => half-period = SECOND / 16. */
        period = TIKU_CLOCK_SECOND / 16;
        if (period == 0) {
            period = 1;                         /* defensive */
        }
    }

    tiku_timer_set_event(&blink_timer, period);

    while (1) {
        TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);
        tiku_common_led1_toggle();
        tiku_timer_reset(&blink_timer);
    }

    TIKU_PROCESS_END();
}

TIKU_AUTOSTART_PROCESSES(&clock_fault_process);

#endif /* TIKU_EXAMPLE_CLOCK_FAULT */
