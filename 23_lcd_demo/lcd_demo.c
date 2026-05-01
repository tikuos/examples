/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * lcd_demo.c - TikuOS Example 23: Segment-LCD demo
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
 * @file    lcd_demo.c
 * @brief   TikuOS Example 23 - Segment-LCD demo
 *
 * Showcases the platform-independent tiku_lcd interface on the
 * on-board FH-1138P 96-segment LCD of the MSP-EXP430FR6989
 * LaunchPad. Use this file as a template — the interface is the
 * same on every board that ships an LCD.
 *
 * Sequence (1 s per step, alternating forever):
 *   1. "TIKUOS"    via tiku_lcd_puts()        + HEART icon on
 *   2. "VER  3"    via tiku_lcd_puts_right()  + HEART icon off,
 *                                               TX icon on
 *
 * LED1 (red, P1.0) toggles on each step change so the alternation
 * is obvious from across the room even if the LCD glass isn't
 * facing you.
 *
 * Hardware: MSP-EXP430FR6989 LaunchPad (on-board LCD). Builds and
 * runs as a no-op on boards without TIKU_BOARD_HAS_LCD.
 */

#include "tiku.h"

#if TIKU_EXAMPLE_LCD_DEMO

#include <interfaces/lcd/tiku_lcd.h>

TIKU_PROCESS(lcd_demo_process, "LCDDemo");

static struct tiku_timer step_timer;

TIKU_PROCESS_THREAD(lcd_demo_process, ev, data)
{
    static uint8_t step;

    TIKU_PROCESS_BEGIN();

    tiku_common_led1_init();
    tiku_lcd_init();

    /* Paint the first frame immediately so the panel isn't blank
     * during the first 1 s tick. */
    if (TIKU_LCD_PRESENT) {
        tiku_lcd_puts("TIKUOS");
#ifdef TIKU_LCD_ICON_HEART
        tiku_lcd_icon_on(TIKU_LCD_ICON_HEART);
#endif
    }
    step = 1;

    tiku_timer_set_event(&step_timer, TIKU_CLOCK_SECOND);

    while (1) {
        TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);
        tiku_common_led1_toggle();

        if (TIKU_LCD_PRESENT) {
            if (step == 0) {
                /* Banner frame: left-aligned text + HEART icon.
                 * The digit write preserves the heart bit, so the
                 * icon stays lit across redraws. */
                tiku_lcd_puts("TIKUOS");
#ifdef TIKU_LCD_ICON_HEART
                tiku_lcd_icon_on(TIKU_LCD_ICON_HEART);
#endif
#ifdef TIKU_LCD_ICON_TX
                tiku_lcd_icon_off(TIKU_LCD_ICON_TX);
#endif
                step = 1;
            } else {
                /* Version frame: right-aligned text + TX icon. */
                tiku_lcd_puts_right("V 03");
#ifdef TIKU_LCD_ICON_HEART
                tiku_lcd_icon_off(TIKU_LCD_ICON_HEART);
#endif
#ifdef TIKU_LCD_ICON_TX
                tiku_lcd_icon_on(TIKU_LCD_ICON_TX);
#endif
                step = 0;
            }
        }

        tiku_timer_reset(&step_timer);
    }

    TIKU_PROCESS_END();
}

TIKU_AUTOSTART_PROCESSES(&lcd_demo_process);

#endif /* TIKU_EXAMPLE_LCD_DEMO */
