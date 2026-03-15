/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_example_config.h - Example selection configuration
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
 * @file tiku_example_config.h
 * @brief Example selection flags for enabling/disabling example applications
 *
 * Enable ONE example at a time (set to 1). All others must be 0.
 * When all are 0, no example runs and main.c controls startup.
 * See docs/Examples.md for details on each example.
 */

#ifndef TIKU_EXAMPLE_CONFIG_H_
#define TIKU_EXAMPLE_CONFIG_H_

/**
 * @defgroup TIKU_EXAMPLES Example Selection
 * @brief Enable ONE example at a time (set to 1). All others must be 0.
 *
 * When all are 0, no example runs and main.c controls startup.
 * See docs/Examples.md for details on each example.
 * @{
 */

/** Master example enable - set to 1 to allow example selection */
#define TIKU_EXAMPLES_ENABLE         0

#define TIKU_EXAMPLE_BLINK           0  /**< 01: Single LED blink */
#define TIKU_EXAMPLE_DUAL_BLINK      0  /**< 02: Two LEDs, two processes */
#define TIKU_EXAMPLE_BUTTON_LED      0  /**< 03: Button-controlled LED */
#define TIKU_EXAMPLE_MULTI_PROCESS   0  /**< 04: Inter-process events */
#define TIKU_EXAMPLE_STATE_MACHINE   0  /**< 05: Event-driven state machine */
#define TIKU_EXAMPLE_CALLBACK_TIMER  0  /**< 06: Callback-mode timers */
#define TIKU_EXAMPLE_BROADCAST       0  /**< 07: Broadcast events */
#define TIKU_EXAMPLE_TIMEOUT         0  /**< 08: Timeout pattern */
#define TIKU_EXAMPLE_CHANNEL         0  /**< 09: Channel message passing */
#define TIKU_EXAMPLE_I2C_TEMP        0  /**< 10: I2C temperature sensor */
#define TIKU_EXAMPLE_DS18B20_TEMP    0  /**< 11: DS18B20 1-Wire temp sensor */

/**
 * @defgroup TIKU_TEMP_SENSOR Temperature Sensor Selection (Example 10)
 * @brief Select ONE I2C temperature sensor (set to 1). All others must be 0.
 * @{
 */
#define TIKU_TEMP_SENSOR_MCP9808     0  /**< Microchip MCP9808 (addr 0x18) */
#define TIKU_TEMP_SENSOR_ADT7410     0  /**< Analog Devices ADT7410 (addr 0x48) */
/** @} */

/*---------------------------------------------------------------------------*/
/* MUTUAL EXCLUSION: only one example at a time                              */
/*---------------------------------------------------------------------------*/

#define _EXAMPLE_COUNT                                                       \
    (!!TIKU_EXAMPLE_BLINK + !!TIKU_EXAMPLE_DUAL_BLINK +                     \
     !!TIKU_EXAMPLE_BUTTON_LED + !!TIKU_EXAMPLE_MULTI_PROCESS +             \
     !!TIKU_EXAMPLE_STATE_MACHINE + !!TIKU_EXAMPLE_CALLBACK_TIMER +         \
     !!TIKU_EXAMPLE_BROADCAST + !!TIKU_EXAMPLE_TIMEOUT +                    \
     !!TIKU_EXAMPLE_CHANNEL + !!TIKU_EXAMPLE_I2C_TEMP +                     \
     !!TIKU_EXAMPLE_DS18B20_TEMP)

/*---------------------------------------------------------------------------*/
/* TIKUKITS EXAMPLES (only available when tikukits/ is present)              */
/*---------------------------------------------------------------------------*/

#if defined(HAS_TIKUKITS)

/**
 * @defgroup TIKU_KITS_EXAMPLES TikuKits Example Selection
 * @brief Enable ONE kits example at a time (set to 1). All others must be 0.
 *
 * These examples demonstrate TikuKits library modules (data structures,
 * maths, ML, sensors, signal features, text compression).
 * Requires tikukits/ directory to be present.
 * @{
 */

/* Maths */
#define TIKU_EXAMPLE_KITS_MATRIX          0  /**< Matrix operations */
#define TIKU_EXAMPLE_KITS_STATISTICS      0  /**< Statistics functions */
#define TIKU_EXAMPLE_KITS_DISTANCE        0  /**< Distance metrics */

/* Data Structures */
#define TIKU_EXAMPLE_KITS_DS_ARRAY        0  /**< Array data structure */
#define TIKU_EXAMPLE_KITS_DS_BITMAP       0  /**< Bitmap data structure */
#define TIKU_EXAMPLE_KITS_DS_BTREE        0  /**< B-tree data structure */
#define TIKU_EXAMPLE_KITS_DS_HTABLE       0  /**< Hash table */
#define TIKU_EXAMPLE_KITS_DS_LIST         0  /**< Linked list */
#define TIKU_EXAMPLE_KITS_DS_PQUEUE       0  /**< Priority queue */
#define TIKU_EXAMPLE_KITS_DS_QUEUE        0  /**< Queue */
#define TIKU_EXAMPLE_KITS_DS_RINGBUF      0  /**< Ring buffer */
#define TIKU_EXAMPLE_KITS_DS_SM           0  /**< State machine */
#define TIKU_EXAMPLE_KITS_DS_SORTARRAY    0  /**< Sorted array */
#define TIKU_EXAMPLE_KITS_DS_STACK        0  /**< Stack */

/* Machine Learning */
#define TIKU_EXAMPLE_KITS_ML_DTREE        0  /**< Decision tree */
#define TIKU_EXAMPLE_KITS_ML_KNN          0  /**< K-nearest neighbors */
#define TIKU_EXAMPLE_KITS_ML_LINREG       0  /**< Linear regression */
#define TIKU_EXAMPLE_KITS_ML_LINSVM       0  /**< Linear SVM */
#define TIKU_EXAMPLE_KITS_ML_LOGREG       0  /**< Logistic regression */
#define TIKU_EXAMPLE_KITS_ML_NBAYES       0  /**< Naive Bayes */
#define TIKU_EXAMPLE_KITS_ML_TNN          0  /**< Tiny neural network */

/* Sensors */
#define TIKU_EXAMPLE_KITS_SENSORS         0  /**< Temperature sensors */

/* Signal Features */
#define TIKU_EXAMPLE_KITS_SIGFEATURES     0  /**< Signal feature extraction */

/* Text Compression */
#define TIKU_EXAMPLE_KITS_TEXTCOMPRESSION 0  /**< Text compression */

#define _KITS_EXAMPLE_COUNT                                                  \
    (!!TIKU_EXAMPLE_KITS_MATRIX + !!TIKU_EXAMPLE_KITS_STATISTICS +          \
     !!TIKU_EXAMPLE_KITS_DISTANCE +                                         \
     !!TIKU_EXAMPLE_KITS_DS_ARRAY + !!TIKU_EXAMPLE_KITS_DS_BITMAP +        \
     !!TIKU_EXAMPLE_KITS_DS_BTREE + !!TIKU_EXAMPLE_KITS_DS_HTABLE +       \
     !!TIKU_EXAMPLE_KITS_DS_LIST + !!TIKU_EXAMPLE_KITS_DS_PQUEUE +        \
     !!TIKU_EXAMPLE_KITS_DS_QUEUE + !!TIKU_EXAMPLE_KITS_DS_RINGBUF +      \
     !!TIKU_EXAMPLE_KITS_DS_SM + !!TIKU_EXAMPLE_KITS_DS_SORTARRAY +       \
     !!TIKU_EXAMPLE_KITS_DS_STACK +                                         \
     !!TIKU_EXAMPLE_KITS_ML_DTREE + !!TIKU_EXAMPLE_KITS_ML_KNN +          \
     !!TIKU_EXAMPLE_KITS_ML_LINREG + !!TIKU_EXAMPLE_KITS_ML_LINSVM +     \
     !!TIKU_EXAMPLE_KITS_ML_LOGREG + !!TIKU_EXAMPLE_KITS_ML_NBAYES +     \
     !!TIKU_EXAMPLE_KITS_ML_TNN +                                          \
     !!TIKU_EXAMPLE_KITS_SENSORS + !!TIKU_EXAMPLE_KITS_SIGFEATURES +      \
     !!TIKU_EXAMPLE_KITS_TEXTCOMPRESSION)

/** @} */ /* End of TIKU_KITS_EXAMPLES group */

#else /* !HAS_TIKUKITS */

#define _KITS_EXAMPLE_COUNT 0

#endif /* HAS_TIKUKITS */

/*---------------------------------------------------------------------------*/
/* MUTUAL EXCLUSION: only one example (core OR kits) at a time               */
/*---------------------------------------------------------------------------*/

#if TIKU_EXAMPLES_ENABLE && ((_EXAMPLE_COUNT + _KITS_EXAMPLE_COUNT) > 1)
#error "Only one example may be enabled at a time"
#endif

/** @} */ /* End of TIKU_EXAMPLES group */

#endif /* TIKU_EXAMPLE_CONFIG_H_ */
