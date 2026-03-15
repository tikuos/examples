/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Circular Log Data Structure
 *
 * Demonstrates the circular log for embedded applications:
 *   - Structured event logging with timestamps
 *   - Log level filtering (DEBUG/INFO/WARN/ERROR)
 *   - Bounded FRAM log that overwrites oldest entries
 *
 * All circular logs use static storage (no heap). Maximum entries
 * controlled by TIKU_KITS_DS_CIRCLOG_MAX_ENTRIES (default 32).
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "examples/kits/example_kits_printf.h"
#include <stdint.h>
#include <string.h>

#include "tikukits/ds/tiku_kits_ds.h"
#include "tikukits/ds/circlog/tiku_kits_ds_circlog.h"

#if defined(HAS_TIKUKITS)

/*---------------------------------------------------------------------------*/
/* Helpers                                                                   */
/*---------------------------------------------------------------------------*/

static const char *level_str(uint8_t level)
{
    switch (level) {
    case 0: return "DBG";
    case 1: return "INF";
    case 2: return "WRN";
    case 3: return "ERR";
    default: return "???";
    }
}

static void print_entry(const struct tiku_kits_ds_circlog_entry *e)
{
    printf("    [%s] t=%ld tag=%d payload_len=%d\n",
           level_str(e->level),
           (long)e->timestamp,
           (int)e->tag,
           (int)e->payload_len);
}

/*---------------------------------------------------------------------------*/
/* Example 1: Basic event logging                                            */
/*---------------------------------------------------------------------------*/

static void example_basic_logging(void)
{
    struct tiku_kits_ds_circlog log;
    struct tiku_kits_ds_circlog_entry entry;
    uint8_t payload[4];

    printf("--- Basic Event Logging ---\n");

    tiku_kits_ds_circlog_init(&log, 8);

    /* Log a boot event */
    payload[0] = 0x01;
    tiku_kits_ds_circlog_append(&log, 1, 10, 1000,
                                payload, 1);

    /* Log a sensor reading */
    payload[0] = 0x19; /* 25 degrees */
    payload[1] = 0x00;
    tiku_kits_ds_circlog_append(&log, 0, 20, 2000,
                                payload, 2);

    /* Log a warning */
    payload[0] = 0x03; /* low battery */
    tiku_kits_ds_circlog_append(&log, 2, 30, 3000,
                                payload, 1);

    printf("  Log has %d entries (seq=%ld)\n",
           (int)tiku_kits_ds_circlog_count(&log),
           (long)tiku_kits_ds_circlog_sequence(&log));

    /* Read latest entry */
    tiku_kits_ds_circlog_read_latest(&log, &entry);
    printf("  Latest:");
    print_entry(&entry);

    /* Read all entries newest-first */
    printf("  All entries (newest first):\n");
    {
        uint16_t i;
        uint16_t n = tiku_kits_ds_circlog_count(&log);
        for (i = 0; i < n; i++) {
            tiku_kits_ds_circlog_read_at(&log, i, &entry);
            printf("   [%d]", (int)i);
            print_entry(&entry);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Example 2: Wraparound behavior                                           */
/*---------------------------------------------------------------------------*/

static void example_wraparound(void)
{
    struct tiku_kits_ds_circlog log;
    struct tiku_kits_ds_circlog_entry entry;
    uint8_t payload[1];
    uint16_t i;

    printf("--- Wraparound Behavior ---\n");

    tiku_kits_ds_circlog_init(&log, 4);

    /* Write 6 entries into a 4-slot log */
    for (i = 0; i < 6; i++) {
        payload[0] = (uint8_t)(i + 1);
        tiku_kits_ds_circlog_append(&log, 1, (uint8_t)i,
                                    (uint32_t)(i * 100),
                                    payload, 1);
    }

    printf("  After 6 appends to capacity-4 log:\n");
    printf("  Count: %d (capped at capacity)\n",
           (int)tiku_kits_ds_circlog_count(&log));
    printf("  Sequence: %ld (monotonic)\n",
           (long)tiku_kits_ds_circlog_sequence(&log));

    /* Newest should be entry 5, oldest should be entry 2 */
    tiku_kits_ds_circlog_read_at(&log, 0, &entry);
    printf("  Newest (idx 0): tag=%d payload[0]=%d\n",
           (int)entry.tag, (int)entry.payload[0]);
    tiku_kits_ds_circlog_read_at(&log, 3, &entry);
    printf("  Oldest (idx 3): tag=%d payload[0]=%d\n",
           (int)entry.tag, (int)entry.payload[0]);
}

/*---------------------------------------------------------------------------*/
/* Example 3: Error log with level filtering                                 */
/*---------------------------------------------------------------------------*/

static void example_error_log(void)
{
    struct tiku_kits_ds_circlog log;
    struct tiku_kits_ds_circlog_entry entry;
    uint8_t payload[2];
    uint16_t i, n, errors;

    printf("--- Error Log with Filtering ---\n");

    tiku_kits_ds_circlog_init(&log, 16);

    /* Mix of levels */
    payload[0] = 0x01;
    tiku_kits_ds_circlog_append(&log, 0, 1, 100, payload, 1);
    tiku_kits_ds_circlog_append(&log, 1, 2, 200, payload, 1);
    tiku_kits_ds_circlog_append(&log, 3, 3, 300, payload, 1);
    tiku_kits_ds_circlog_append(&log, 1, 4, 400, payload, 1);
    tiku_kits_ds_circlog_append(&log, 2, 5, 500, payload, 1);
    tiku_kits_ds_circlog_append(&log, 3, 6, 600, payload, 1);

    /* Count errors (level >= 3) */
    n = tiku_kits_ds_circlog_count(&log);
    errors = 0;
    for (i = 0; i < n; i++) {
        tiku_kits_ds_circlog_read_at(&log, i, &entry);
        if (entry.level >= 3) {
            errors++;
        }
    }

    printf("  Total entries: %d\n", (int)n);
    printf("  Error entries (level>=3): %d\n", (int)errors);

    /* Clear the log */
    tiku_kits_ds_circlog_clear(&log);
    printf("  After clear: %d entries\n",
           (int)tiku_kits_ds_circlog_count(&log));
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_circlog_run(void)
{
    printf("=== TikuKits Circular Log Examples ===\n\n");

    example_basic_logging();
    printf("\n");

    example_wraparound();
    printf("\n");

    example_error_log();
    printf("\n");
}

#endif /* HAS_TIKUKITS */
