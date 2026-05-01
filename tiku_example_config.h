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

/** Master example enable - set to 1 to allow example selection.
 *  May also be set from the Makefile (-DTIKU_EXAMPLES_ENABLE=1)
 *  by example wrappers that auto-enable themselves. */
#ifndef TIKU_EXAMPLES_ENABLE
#define TIKU_EXAMPLES_ENABLE         0
#endif

#ifndef TIKU_EXAMPLE_BLINK
#define TIKU_EXAMPLE_BLINK           0  /**< 01: Single LED blink */
#endif
#ifndef TIKU_EXAMPLE_DUAL_BLINK
#define TIKU_EXAMPLE_DUAL_BLINK      0  /**< 02: Two LEDs, two processes */
#endif
#ifndef TIKU_EXAMPLE_BUTTON_LED
#define TIKU_EXAMPLE_BUTTON_LED      0  /**< 03: Button-controlled LED */
#endif
#ifndef TIKU_EXAMPLE_MULTI_PROCESS
#define TIKU_EXAMPLE_MULTI_PROCESS   0  /**< 04: Inter-process events */
#endif
#ifndef TIKU_EXAMPLE_STATE_MACHINE
#define TIKU_EXAMPLE_STATE_MACHINE   0  /**< 05: Event-driven state machine */
#endif
#ifndef TIKU_EXAMPLE_CALLBACK_TIMER
#define TIKU_EXAMPLE_CALLBACK_TIMER  0  /**< 06: Callback-mode timers */
#endif
#ifndef TIKU_EXAMPLE_BROADCAST
#define TIKU_EXAMPLE_BROADCAST       0  /**< 07: Broadcast events */
#endif
#ifndef TIKU_EXAMPLE_TIMEOUT
#define TIKU_EXAMPLE_TIMEOUT         0  /**< 08: Timeout pattern */
#endif
#ifndef TIKU_EXAMPLE_CHANNEL
#define TIKU_EXAMPLE_CHANNEL         0  /**< 09: Channel message passing */
#endif
#define TIKU_EXAMPLE_I2C_TEMP        0  /**< 10: I2C temperature sensor */
#define TIKU_EXAMPLE_DS18B20_TEMP    0  /**< 11: DS18B20 1-Wire temp sensor */
#ifndef TIKU_EXAMPLE_UDP_SEND
#define TIKU_EXAMPLE_UDP_SEND        0  /**< 12: UDP sender over SLIP (needs tikukits) */
#endif
#ifndef TIKU_EXAMPLE_TCP_SEND
#define TIKU_EXAMPLE_TCP_SEND        0  /**< 13: TCP sender over SLIP (needs tikukits + TCP + ext UART) */
#endif
#ifndef TIKU_EXAMPLE_DNS_RESOLVE
#define TIKU_EXAMPLE_DNS_RESOLVE     0  /**< 14: DNS resolver over SLIP (needs tikukits) */
#endif
#ifndef TIKU_EXAMPLE_HTTP_GET
#define TIKU_EXAMPLE_HTTP_GET        0  /**< 15: HTTPS GET over SLIP (needs tikukits + HTTP + TCP + ext UART) */
#endif
#ifndef TIKU_EXAMPLE_TCP_ECHO
#define TIKU_EXAMPLE_TCP_ECHO        0  /**< 16: TCP echo client over SLIP (needs tikukits + TCP + ext UART) */
#endif
#ifndef TIKU_EXAMPLE_HTTP_FETCH
#define TIKU_EXAMPLE_HTTP_FETCH      0  /**< 17: Fetch real webpage via gateway (needs tikukits + TCP + ext UART) */
#endif
#ifndef TIKU_EXAMPLE_HTTP_DIRECT
#define TIKU_EXAMPLE_HTTP_DIRECT     0  /**< 18: Direct HTTP GET to internet (needs tikukits + TCP + SLIP bridge) */
#endif
#ifndef TIKU_EXAMPLE_HTTPS_DIRECT
#define TIKU_EXAMPLE_HTTPS_DIRECT    0  /**< 19: HTTPS GET via PSK-TLS gateway (needs tikukits + HTTP + TLS + SLIP bridge) */
#endif
#ifndef TIKU_EXAMPLE_BITBANG
#define TIKU_EXAMPLE_BITBANG         0  /**< 20: htimer-driven GPIO bit-bang under tiku_crit (needs TIKU_BITBANG_ENABLE=1) */
#endif
#ifndef TIKU_EXAMPLE_CRIT_DEFER
#define TIKU_EXAMPLE_CRIT_DEFER      0  /**< 21: bit-bang under tiku_crit_begin_defer (cooperative variant) */
#endif
#ifndef TIKU_EXAMPLE_CLOCK_FAULT
#define TIKU_EXAMPLE_CLOCK_FAULT     0  /**< 22: tiku_clock_fault() boot-time diagnostic on LED1 */
#endif
#ifndef TIKU_EXAMPLE_LCD_DEMO
#define TIKU_EXAMPLE_LCD_DEMO        0  /**< 23: segment-LCD demo on FH-1138P (FR6989 LaunchPad) */
#endif

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
     !!TIKU_EXAMPLE_DS18B20_TEMP + !!TIKU_EXAMPLE_UDP_SEND + \
     !!TIKU_EXAMPLE_TCP_SEND + !!TIKU_EXAMPLE_DNS_RESOLVE + \
     !!TIKU_EXAMPLE_HTTP_GET + !!TIKU_EXAMPLE_TCP_ECHO + \
     !!TIKU_EXAMPLE_HTTP_FETCH + !!TIKU_EXAMPLE_HTTP_DIRECT + \
     !!TIKU_EXAMPLE_HTTPS_DIRECT + !!TIKU_EXAMPLE_BITBANG + \
     !!TIKU_EXAMPLE_CRIT_DEFER + !!TIKU_EXAMPLE_CLOCK_FAULT + \
     !!TIKU_EXAMPLE_LCD_DEMO)

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
#ifndef TIKU_EXAMPLE_KITS_MATRIX
#define TIKU_EXAMPLE_KITS_MATRIX          0  /**< Matrix operations */
#endif
#ifndef TIKU_EXAMPLE_KITS_STATISTICS
#define TIKU_EXAMPLE_KITS_STATISTICS      0  /**< Statistics functions */
#endif
#ifndef TIKU_EXAMPLE_KITS_DISTANCE
#define TIKU_EXAMPLE_KITS_DISTANCE        0  /**< Distance metrics */
#endif

/* Data Structures */
#ifndef TIKU_EXAMPLE_KITS_DS_ARRAY
#define TIKU_EXAMPLE_KITS_DS_ARRAY        0  /**< Array data structure */
#endif
#ifndef TIKU_EXAMPLE_KITS_DS_BITMAP
#define TIKU_EXAMPLE_KITS_DS_BITMAP       0  /**< Bitmap data structure */
#endif
#ifndef TIKU_EXAMPLE_KITS_DS_BTREE
#define TIKU_EXAMPLE_KITS_DS_BTREE        0  /**< B-tree data structure */
#endif
#ifndef TIKU_EXAMPLE_KITS_DS_HTABLE
#define TIKU_EXAMPLE_KITS_DS_HTABLE       0  /**< Hash table */
#endif
#ifndef TIKU_EXAMPLE_KITS_DS_LIST
#define TIKU_EXAMPLE_KITS_DS_LIST         0  /**< Linked list */
#endif
#ifndef TIKU_EXAMPLE_KITS_DS_PQUEUE
#define TIKU_EXAMPLE_KITS_DS_PQUEUE       0  /**< Priority queue */
#endif
#ifndef TIKU_EXAMPLE_KITS_DS_QUEUE
#define TIKU_EXAMPLE_KITS_DS_QUEUE        0  /**< Queue */
#endif
#ifndef TIKU_EXAMPLE_KITS_DS_RINGBUF
#define TIKU_EXAMPLE_KITS_DS_RINGBUF      0  /**< Ring buffer */
#endif
#ifndef TIKU_EXAMPLE_KITS_DS_SM
#define TIKU_EXAMPLE_KITS_DS_SM           0  /**< State machine */
#endif
#ifndef TIKU_EXAMPLE_KITS_DS_SORTARRAY
#define TIKU_EXAMPLE_KITS_DS_SORTARRAY    0  /**< Sorted array */
#endif
#ifndef TIKU_EXAMPLE_KITS_DS_STACK
#define TIKU_EXAMPLE_KITS_DS_STACK        0  /**< Stack */
#endif

/* Machine Learning */
#ifndef TIKU_EXAMPLE_KITS_ML_DTREE
#define TIKU_EXAMPLE_KITS_ML_DTREE        0  /**< Decision tree */
#endif
#ifndef TIKU_EXAMPLE_KITS_ML_KNN
#define TIKU_EXAMPLE_KITS_ML_KNN          0  /**< K-nearest neighbors */
#endif
#ifndef TIKU_EXAMPLE_KITS_ML_LINREG
#define TIKU_EXAMPLE_KITS_ML_LINREG       0  /**< Linear regression */
#endif
#ifndef TIKU_EXAMPLE_KITS_ML_LINSVM
#define TIKU_EXAMPLE_KITS_ML_LINSVM       0  /**< Linear SVM */
#endif
#ifndef TIKU_EXAMPLE_KITS_ML_LOGREG
#define TIKU_EXAMPLE_KITS_ML_LOGREG       0  /**< Logistic regression */
#endif
#ifndef TIKU_EXAMPLE_KITS_ML_NBAYES
#define TIKU_EXAMPLE_KITS_ML_NBAYES       0  /**< Naive Bayes */
#endif
#ifndef TIKU_EXAMPLE_KITS_ML_TNN
#define TIKU_EXAMPLE_KITS_ML_TNN          0  /**< Tiny neural network */
#endif

/* Sensors */
#ifndef TIKU_EXAMPLE_KITS_SENSORS
#define TIKU_EXAMPLE_KITS_SENSORS         0  /**< Temperature sensors */
#endif

/* Signal Features */
#ifndef TIKU_EXAMPLE_KITS_SIGFEATURES
#define TIKU_EXAMPLE_KITS_SIGFEATURES     0  /**< Signal feature extraction */
#endif

/* Text Compression */
#ifndef TIKU_EXAMPLE_KITS_TEXTCOMPRESSION
#define TIKU_EXAMPLE_KITS_TEXTCOMPRESSION 0  /**< Text compression */
#endif

/* Networking */
#define TIKU_EXAMPLE_KITS_NET_IPV4        0  /**< IPv4 fundamentals */
#ifndef TIKU_EXAMPLE_KITS_NET_UDP
#define TIKU_EXAMPLE_KITS_NET_UDP         0  /**< UDP datagrams */
#endif
#ifndef TIKU_EXAMPLE_KITS_NET_TFTP
#define TIKU_EXAMPLE_KITS_NET_TFTP        0  /**< TFTP client */
#endif
#ifndef TIKU_EXAMPLE_KITS_NET_TCP
#define TIKU_EXAMPLE_KITS_NET_TCP         0  /**< TCP transport */
#endif
#ifndef TIKU_EXAMPLE_KITS_NET_DNS
#define TIKU_EXAMPLE_KITS_NET_DNS         0  /**< DNS resolver */
#endif
#ifndef TIKU_EXAMPLE_KITS_NET_TLS
#define TIKU_EXAMPLE_KITS_NET_TLS         0  /**< TLS 1.3 PSK crypto */
#endif
#ifndef TIKU_EXAMPLE_KITS_NET_HTTP
#define TIKU_EXAMPLE_KITS_NET_HTTP        0  /**< HTTP/1.1 client */
#endif

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
     !!TIKU_EXAMPLE_KITS_TEXTCOMPRESSION +                                \
     !!TIKU_EXAMPLE_KITS_NET_IPV4 + !!TIKU_EXAMPLE_KITS_NET_UDP +       \
     !!TIKU_EXAMPLE_KITS_NET_TFTP + !!TIKU_EXAMPLE_KITS_NET_TCP +     \
     !!TIKU_EXAMPLE_KITS_NET_DNS + !!TIKU_EXAMPLE_KITS_NET_TLS + \
     !!TIKU_EXAMPLE_KITS_NET_HTTP)

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
