/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Bloom Filter Data Structure
 *
 * Demonstrates the bloom filter for embedded applications:
 *   - Wireless packet deduplication (seen-before check)
 *   - Sensor ID membership testing
 *   - Fast negative lookup for resource-constrained systems
 *
 * All bloom filters use static storage (no heap). Maximum bit count is
 * controlled by TIKU_KITS_DS_BLOOM_MAX_BITS (default 256).
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "examples/kits/example_kits_printf.h"
#include <stdint.h>
#include <string.h>

#include "tikukits/ds/tiku_kits_ds.h"
#include "tikukits/ds/bloom/tiku_kits_ds_bloom.h"

#if defined(HAS_TIKUKITS)

/*---------------------------------------------------------------------------*/
/* Example 1: Wireless packet deduplication                                  */
/*---------------------------------------------------------------------------*/

static void example_packet_dedup(void)
{
    struct tiku_kits_ds_bloom seen;
    uint8_t pkt_id[4];
    uint16_t i;

    printf("--- Wireless Packet Dedup ---\n");

    tiku_kits_ds_bloom_init(&seen, 128, 3);

    /* Simulate receiving 10 unique packets */
    for (i = 0; i < 10; i++) {
        pkt_id[0] = (uint8_t)(i >> 8);
        pkt_id[1] = (uint8_t)(i & 0xFF);
        pkt_id[2] = 0xAB;
        pkt_id[3] = 0xCD;

        if (tiku_kits_ds_bloom_check(&seen, pkt_id, 4)) {
            printf("  Pkt %d: DUPLICATE (suppressed)\n", i);
        } else {
            printf("  Pkt %d: NEW (processing)\n", i);
            tiku_kits_ds_bloom_add(&seen, pkt_id, 4);
        }
    }

    /* Replay packet 3 — should be detected as duplicate */
    pkt_id[0] = 0;
    pkt_id[1] = 3;
    pkt_id[2] = 0xAB;
    pkt_id[3] = 0xCD;
    printf("  Replay pkt 3: %s\n",
           tiku_kits_ds_bloom_check(&seen, pkt_id, 4)
           ? "DUPLICATE" : "NEW");

    printf("  Total packets tracked: %d\n",
           (int)tiku_kits_ds_bloom_count(&seen));
}

/*---------------------------------------------------------------------------*/
/* Example 2: Sensor ID allowlist                                            */
/*---------------------------------------------------------------------------*/

static void example_sensor_allowlist(void)
{
    struct tiku_kits_ds_bloom allowed;
    static const char *known[] = {
        "TEMP_01", "TEMP_02", "HUM_01", "PRESS_01", "LIGHT_01"
    };
    static const char *unknown[] = {
        "TEMP_99", "GPS_01", "MOTOR_01"
    };
    uint16_t i;

    printf("--- Sensor ID Allowlist ---\n");

    tiku_kits_ds_bloom_init(&allowed, 256, 3);

    /* Register known sensors */
    for (i = 0; i < 5; i++) {
        tiku_kits_ds_bloom_add(&allowed,
                               (const uint8_t *)known[i],
                               (uint16_t)strlen(known[i]));
    }

    /* Check known sensors */
    for (i = 0; i < 5; i++) {
        int found = tiku_kits_ds_bloom_check(
            &allowed,
            (const uint8_t *)known[i],
            (uint16_t)strlen(known[i]));
        printf("  %s: %s\n", known[i],
               found ? "ALLOWED" : "BLOCKED");
    }

    /* Check unknown sensors */
    for (i = 0; i < 3; i++) {
        int found = tiku_kits_ds_bloom_check(
            &allowed,
            (const uint8_t *)unknown[i],
            (uint16_t)strlen(unknown[i]));
        printf("  %s: %s\n", unknown[i],
               found ? "ALLOWED (false positive!)" : "BLOCKED");
    }
}

/*---------------------------------------------------------------------------*/
/* Example 3: Epoch-based filter with periodic reset                         */
/*---------------------------------------------------------------------------*/

static void example_epoch_filter(void)
{
    struct tiku_kits_ds_bloom filter;
    uint8_t key[2];
    uint16_t epoch, i;

    printf("--- Epoch-Based Filter ---\n");

    tiku_kits_ds_bloom_init(&filter, 64, 2);

    for (epoch = 0; epoch < 3; epoch++) {
        tiku_kits_ds_bloom_clear(&filter);
        printf("  Epoch %d: filter cleared\n", epoch);

        for (i = 0; i < 5; i++) {
            key[0] = (uint8_t)epoch;
            key[1] = (uint8_t)i;
            tiku_kits_ds_bloom_add(&filter, key, 2);
        }
        printf("  Epoch %d: %d items added\n", epoch,
               (int)tiku_kits_ds_bloom_count(&filter));
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_bloom_run(void)
{
    printf("=== TikuKits Bloom Filter Examples ===\n\n");

    example_packet_dedup();
    printf("\n");

    example_sensor_allowlist();
    printf("\n");

    example_epoch_filter();
    printf("\n");
}

#endif /* HAS_TIKUKITS */
