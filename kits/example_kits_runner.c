/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * example_kits_runner.c - TikuKits example dispatcher
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku.h"

#if defined(HAS_TIKUKITS) && defined(HAS_EXAMPLES) && TIKU_EXAMPLES_ENABLE

/*---------------------------------------------------------------------------*/
/* Forward declarations for example entry points                             */
/*---------------------------------------------------------------------------*/

/* Maths */
#if TIKU_EXAMPLE_KITS_MATRIX
extern void example_matrix_run(void);
#endif
#if TIKU_EXAMPLE_KITS_STATISTICS
extern void example_statistics_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DISTANCE
extern void example_distance_run(void);
#endif

/* Data Structures */
#if TIKU_EXAMPLE_KITS_DS_ARRAY
extern void example_ds_array_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DS_BITMAP
extern void example_ds_bitmap_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DS_BTREE
extern void example_ds_btree_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DS_HTABLE
extern void example_ds_htable_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DS_LIST
extern void example_ds_list_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DS_PQUEUE
extern void example_ds_pqueue_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DS_QUEUE
extern void example_ds_queue_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DS_RINGBUF
extern void example_ds_ringbuf_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DS_SM
extern void example_ds_sm_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DS_SORTARRAY
extern void example_ds_sortarray_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DS_STACK
extern void example_ds_stack_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DS_BLOOM
extern void example_ds_bloom_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DS_CIRCLOG
extern void example_ds_circlog_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DS_DEQUE
extern void example_ds_deque_run(void);
#endif
#if TIKU_EXAMPLE_KITS_DS_TRIE
extern void example_ds_trie_run(void);
#endif

/* Machine Learning */
#if TIKU_EXAMPLE_KITS_ML_DTREE
extern void example_ml_dtree_run(void);
#endif
#if TIKU_EXAMPLE_KITS_ML_KNN
extern void example_ml_knn_run(void);
#endif
#if TIKU_EXAMPLE_KITS_ML_LINREG
extern void example_ml_linreg_run(void);
#endif
#if TIKU_EXAMPLE_KITS_ML_LINSVM
extern void example_ml_linsvm_run(void);
#endif
#if TIKU_EXAMPLE_KITS_ML_LOGREG
extern void example_ml_logreg_run(void);
#endif
#if TIKU_EXAMPLE_KITS_ML_NBAYES
extern void example_ml_nbayes_run(void);
#endif
#if TIKU_EXAMPLE_KITS_ML_TNN
extern void example_ml_tnn_run(void);
#endif

/* Sensors */
#if TIKU_EXAMPLE_KITS_SENSORS
extern void example_sensors_run(void);
#endif

/* Signal Features */
#if TIKU_EXAMPLE_KITS_SIGFEATURES
extern void example_sigfeatures_run(void);
#endif

/* Text Compression */
#if TIKU_EXAMPLE_KITS_TEXTCOMPRESSION
extern void example_textcompression_run(void);
#endif

/*---------------------------------------------------------------------------*/
/* Dispatcher                                                                */
/*---------------------------------------------------------------------------*/

void example_kits_run(void)
{
#if TIKU_EXAMPLE_KITS_MATRIX
    example_matrix_run();
#elif TIKU_EXAMPLE_KITS_STATISTICS
    example_statistics_run();
#elif TIKU_EXAMPLE_KITS_DISTANCE
    example_distance_run();
#elif TIKU_EXAMPLE_KITS_DS_ARRAY
    example_ds_array_run();
#elif TIKU_EXAMPLE_KITS_DS_BITMAP
    example_ds_bitmap_run();
#elif TIKU_EXAMPLE_KITS_DS_BTREE
    example_ds_btree_run();
#elif TIKU_EXAMPLE_KITS_DS_HTABLE
    example_ds_htable_run();
#elif TIKU_EXAMPLE_KITS_DS_LIST
    example_ds_list_run();
#elif TIKU_EXAMPLE_KITS_DS_PQUEUE
    example_ds_pqueue_run();
#elif TIKU_EXAMPLE_KITS_DS_QUEUE
    example_ds_queue_run();
#elif TIKU_EXAMPLE_KITS_DS_RINGBUF
    example_ds_ringbuf_run();
#elif TIKU_EXAMPLE_KITS_DS_SM
    example_ds_sm_run();
#elif TIKU_EXAMPLE_KITS_DS_SORTARRAY
    example_ds_sortarray_run();
#elif TIKU_EXAMPLE_KITS_DS_STACK
    example_ds_stack_run();
#elif TIKU_EXAMPLE_KITS_DS_BLOOM
    example_ds_bloom_run();
#elif TIKU_EXAMPLE_KITS_DS_CIRCLOG
    example_ds_circlog_run();
#elif TIKU_EXAMPLE_KITS_DS_DEQUE
    example_ds_deque_run();
#elif TIKU_EXAMPLE_KITS_DS_TRIE
    example_ds_trie_run();
#elif TIKU_EXAMPLE_KITS_ML_DTREE
    example_ml_dtree_run();
#elif TIKU_EXAMPLE_KITS_ML_KNN
    example_ml_knn_run();
#elif TIKU_EXAMPLE_KITS_ML_LINREG
    example_ml_linreg_run();
#elif TIKU_EXAMPLE_KITS_ML_LINSVM
    example_ml_linsvm_run();
#elif TIKU_EXAMPLE_KITS_ML_LOGREG
    example_ml_logreg_run();
#elif TIKU_EXAMPLE_KITS_ML_NBAYES
    example_ml_nbayes_run();
#elif TIKU_EXAMPLE_KITS_ML_TNN
    example_ml_tnn_run();
#elif TIKU_EXAMPLE_KITS_SENSORS
    example_sensors_run();
#elif TIKU_EXAMPLE_KITS_SIGFEATURES
    example_sigfeatures_run();
#elif TIKU_EXAMPLE_KITS_TEXTCOMPRESSION
    example_textcompression_run();
#endif
}

#else /* !HAS_TIKUKITS || !HAS_EXAMPLES || !TIKU_EXAMPLES_ENABLE */

void example_kits_run(void)
{
    /* Nothing to run */
}

#endif
