/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Trie (Prefix Tree)
 *
 * Demonstrates the nibble trie for embedded applications:
 *   - Command name lookup table
 *   - Sensor ID to channel mapping
 *   - Prefix-based key matching
 *
 * All tries use static storage (no heap). Maximum nodes
 * controlled by TIKU_KITS_DS_TRIE_MAX_NODES (default 64).
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "examples/kits/example_kits_printf.h"
#include <stdint.h>
#include <string.h>

#include "tikukits/ds/tiku_kits_ds.h"
#include "tikukits/ds/trie/tiku_kits_ds_trie.h"

#if defined(HAS_TIKUKITS)

/*---------------------------------------------------------------------------*/
/* Example 1: Command lookup table                                           */
/*---------------------------------------------------------------------------*/

static void example_command_lookup(void)
{
    struct tiku_kits_ds_trie trie;
    tiku_kits_ds_elem_t cmd_id;
    int rc;

    /* Command IDs */
    enum { CMD_READ = 1, CMD_WRITE = 2, CMD_RESET = 3,
           CMD_STATUS = 4, CMD_HELP = 5 };

    printf("--- Command Lookup Table ---\n");

    tiku_kits_ds_trie_init(&trie);

    /* Register commands */
    tiku_kits_ds_trie_insert(&trie,
        (const uint8_t *)"read", 4, CMD_READ);
    tiku_kits_ds_trie_insert(&trie,
        (const uint8_t *)"write", 5, CMD_WRITE);
    tiku_kits_ds_trie_insert(&trie,
        (const uint8_t *)"reset", 5, CMD_RESET);
    tiku_kits_ds_trie_insert(&trie,
        (const uint8_t *)"status", 6, CMD_STATUS);
    tiku_kits_ds_trie_insert(&trie,
        (const uint8_t *)"help", 4, CMD_HELP);

    printf("  Registered %d commands\n",
           (int)tiku_kits_ds_trie_count(&trie));

    /* Look up commands */
    rc = tiku_kits_ds_trie_search(&trie,
        (const uint8_t *)"read", 4, &cmd_id);
    printf("  'read'   -> %s (id=%ld)\n",
           rc == TIKU_KITS_DS_OK ? "FOUND" : "NOT FOUND",
           (long)cmd_id);

    rc = tiku_kits_ds_trie_search(&trie,
        (const uint8_t *)"status", 6, &cmd_id);
    printf("  'status' -> %s (id=%ld)\n",
           rc == TIKU_KITS_DS_OK ? "FOUND" : "NOT FOUND",
           (long)cmd_id);

    /* Unknown command */
    rc = tiku_kits_ds_trie_search(&trie,
        (const uint8_t *)"delete", 6, &cmd_id);
    printf("  'delete' -> %s\n",
           rc == TIKU_KITS_DS_OK ? "FOUND" : "NOT FOUND");
}

/*---------------------------------------------------------------------------*/
/* Example 2: Sensor ID mapping                                              */
/*---------------------------------------------------------------------------*/

static void example_sensor_mapping(void)
{
    struct tiku_kits_ds_trie trie;
    tiku_kits_ds_elem_t channel;

    printf("--- Sensor ID -> Channel Mapping ---\n");

    tiku_kits_ds_trie_init(&trie);

    /* Map sensor IDs to ADC channels */
    tiku_kits_ds_trie_insert(&trie,
        (const uint8_t *)"T1", 2, 0);  /* Temp sensor 1 -> ch 0 */
    tiku_kits_ds_trie_insert(&trie,
        (const uint8_t *)"T2", 2, 1);  /* Temp sensor 2 -> ch 1 */
    tiku_kits_ds_trie_insert(&trie,
        (const uint8_t *)"H1", 2, 4);  /* Humidity -> ch 4 */
    tiku_kits_ds_trie_insert(&trie,
        (const uint8_t *)"P1", 2, 5);  /* Pressure -> ch 5 */

    /* Look up channels */
    if (tiku_kits_ds_trie_search(&trie,
            (const uint8_t *)"T1", 2, &channel)
        == TIKU_KITS_DS_OK) {
        printf("  T1 -> ADC channel %ld\n", (long)channel);
    }

    if (tiku_kits_ds_trie_search(&trie,
            (const uint8_t *)"H1", 2, &channel)
        == TIKU_KITS_DS_OK) {
        printf("  H1 -> ADC channel %ld\n", (long)channel);
    }

    /* Check membership */
    printf("  'T2' registered: %s\n",
           tiku_kits_ds_trie_contains(&trie,
               (const uint8_t *)"T2", 2) ? "yes" : "no");
    printf("  'X9' registered: %s\n",
           tiku_kits_ds_trie_contains(&trie,
               (const uint8_t *)"X9", 2) ? "yes" : "no");
}

/*---------------------------------------------------------------------------*/
/* Example 3: Remove and reuse                                               */
/*---------------------------------------------------------------------------*/

static void example_remove(void)
{
    struct tiku_kits_ds_trie trie;
    tiku_kits_ds_elem_t val;

    printf("--- Remove and Reuse ---\n");

    tiku_kits_ds_trie_init(&trie);

    tiku_kits_ds_trie_insert(&trie,
        (const uint8_t *)"abc", 3, 100);
    tiku_kits_ds_trie_insert(&trie,
        (const uint8_t *)"abd", 3, 200);

    printf("  Keys: %d\n",
           (int)tiku_kits_ds_trie_count(&trie));

    /* Remove 'abc' */
    tiku_kits_ds_trie_remove(&trie,
        (const uint8_t *)"abc", 3);
    printf("  After removing 'abc': %d keys\n",
           (int)tiku_kits_ds_trie_count(&trie));

    /* 'abd' should still be accessible */
    if (tiku_kits_ds_trie_search(&trie,
            (const uint8_t *)"abd", 3, &val)
        == TIKU_KITS_DS_OK) {
        printf("  'abd' still present: val=%ld\n",
               (long)val);
    }

    /* 'abc' should be gone */
    printf("  'abc' present: %s\n",
           tiku_kits_ds_trie_contains(&trie,
               (const uint8_t *)"abc", 3) ? "yes" : "no");

    /* Re-insert with new value */
    tiku_kits_ds_trie_insert(&trie,
        (const uint8_t *)"abc", 3, 999);
    tiku_kits_ds_trie_search(&trie,
        (const uint8_t *)"abc", 3, &val);
    printf("  Re-inserted 'abc': val=%ld\n", (long)val);
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_trie_run(void)
{
    printf("=== TikuKits Trie Examples ===\n\n");

    example_command_lookup();
    printf("\n");

    example_sensor_mapping();
    printf("\n");

    example_remove();
    printf("\n");
}

#endif /* HAS_TIKUKITS */
