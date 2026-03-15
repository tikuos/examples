/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Deque (Double-Ended Queue)
 *
 * Demonstrates the deque for embedded applications:
 *   - Sliding window min/max computation
 *   - Work-stealing task scheduler pattern
 *   - Undo/redo buffer with bounded history
 *
 * All deques use static storage (no heap). Maximum capacity is
 * controlled by TIKU_KITS_DS_DEQUE_MAX_SIZE (default 32).
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "examples/kits/example_kits_printf.h"
#include <stdint.h>

#include "tikukits/ds/tiku_kits_ds.h"
#include "tikukits/ds/deque/tiku_kits_ds_deque.h"

#if defined(HAS_TIKUKITS)

/*---------------------------------------------------------------------------*/
/* Example 1: Sliding window maximum                                         */
/*---------------------------------------------------------------------------*/

static void example_sliding_window_max(void)
{
    struct tiku_kits_ds_deque dq;
    /* Simulated sensor readings */
    static const tiku_kits_ds_elem_t readings[] = {
        23, 25, 21, 28, 22, 30, 27, 24, 26, 29
    };
    uint16_t n = 10;
    uint16_t window = 3;
    uint16_t i;
    tiku_kits_ds_elem_t val;

    printf("--- Sliding Window Maximum (k=%d) ---\n",
           (int)window);

    tiku_kits_ds_deque_init(&dq, 16);

    printf("  Readings: ");
    for (i = 0; i < n; i++) {
        printf("%ld ", (long)readings[i]);
    }
    printf("\n  Window maxima: ");

    for (i = 0; i < n; i++) {
        /* Remove indices of elements outside the window */
        while (!tiku_kits_ds_deque_empty(&dq)) {
            tiku_kits_ds_deque_peek_front(&dq, &val);
            if (val <= (tiku_kits_ds_elem_t)(i - window)) {
                tiku_kits_ds_deque_pop_front(&dq, &val);
            } else {
                break;
            }
        }

        /* Remove indices of smaller elements from back */
        while (!tiku_kits_ds_deque_empty(&dq)) {
            tiku_kits_ds_deque_peek_back(&dq, &val);
            if (readings[val] <= readings[i]) {
                tiku_kits_ds_deque_pop_back(&dq, &val);
            } else {
                break;
            }
        }

        tiku_kits_ds_deque_push_back(&dq, (tiku_kits_ds_elem_t)i);

        if (i >= window - 1) {
            tiku_kits_ds_deque_peek_front(&dq, &val);
            printf("%ld ", (long)readings[val]);
        }
    }
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Example 2: Double-ended task buffer                                       */
/*---------------------------------------------------------------------------*/

static void example_task_buffer(void)
{
    struct tiku_kits_ds_deque tasks;
    tiku_kits_ds_elem_t task_id;

    printf("--- Double-Ended Task Buffer ---\n");

    tiku_kits_ds_deque_init(&tasks, 8);

    /* High-priority tasks go to front */
    printf("  Push HIGH priority task 99 to front\n");
    tiku_kits_ds_deque_push_front(&tasks, 99);

    /* Normal tasks go to back */
    printf("  Push normal tasks 1, 2, 3 to back\n");
    tiku_kits_ds_deque_push_back(&tasks, 1);
    tiku_kits_ds_deque_push_back(&tasks, 2);
    tiku_kits_ds_deque_push_back(&tasks, 3);

    /* Another high-priority task */
    printf("  Push HIGH priority task 98 to front\n");
    tiku_kits_ds_deque_push_front(&tasks, 98);

    /* Process tasks from front (highest priority first) */
    printf("  Processing order: ");
    while (!tiku_kits_ds_deque_empty(&tasks)) {
        tiku_kits_ds_deque_pop_front(&tasks, &task_id);
        printf("%ld ", (long)task_id);
    }
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Example 3: Palindrome checker                                             */
/*---------------------------------------------------------------------------*/

static void example_palindrome(void)
{
    struct tiku_kits_ds_deque dq;
    static const tiku_kits_ds_elem_t seq1[] = {1, 2, 3, 2, 1};
    static const tiku_kits_ds_elem_t seq2[] = {1, 2, 3, 4, 5};
    tiku_kits_ds_elem_t front, back;
    uint16_t i;
    int is_palindrome;

    printf("--- Palindrome Check ---\n");

    /* Check sequence 1: {1,2,3,2,1} */
    tiku_kits_ds_deque_init(&dq, 8);
    for (i = 0; i < 5; i++) {
        tiku_kits_ds_deque_push_back(&dq, seq1[i]);
    }

    is_palindrome = 1;
    while (tiku_kits_ds_deque_count(&dq) > 1) {
        tiku_kits_ds_deque_pop_front(&dq, &front);
        tiku_kits_ds_deque_pop_back(&dq, &back);
        if (front != back) {
            is_palindrome = 0;
            break;
        }
    }
    printf("  {1,2,3,2,1}: %s\n",
           is_palindrome ? "palindrome" : "not palindrome");

    /* Check sequence 2: {1,2,3,4,5} */
    tiku_kits_ds_deque_init(&dq, 8);
    for (i = 0; i < 5; i++) {
        tiku_kits_ds_deque_push_back(&dq, seq2[i]);
    }

    is_palindrome = 1;
    while (tiku_kits_ds_deque_count(&dq) > 1) {
        tiku_kits_ds_deque_pop_front(&dq, &front);
        tiku_kits_ds_deque_pop_back(&dq, &back);
        if (front != back) {
            is_palindrome = 0;
            break;
        }
    }
    printf("  {1,2,3,4,5}: %s\n",
           is_palindrome ? "palindrome" : "not palindrome");
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_deque_run(void)
{
    printf("=== TikuKits Deque Examples ===\n\n");

    example_sliding_window_max();
    printf("\n");

    example_task_buffer();
    printf("\n");

    example_palindrome();
    printf("\n");
}

#endif /* HAS_TIKUKITS */
