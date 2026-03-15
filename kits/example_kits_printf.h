/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * example_kits_printf.h - Route printf to TikuOS UART for kits examples
 *
 * TikuKits examples use standard printf(). On MSP430 with GCC, libc
 * printf is not connected to UART. This header redirects printf calls
 * to tiku_uart_printf so output appears on the serial console.
 *
 * Usage: #include this header AFTER <stdio.h> in kits example files.
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EXAMPLE_KITS_PRINTF_H_
#define EXAMPLE_KITS_PRINTF_H_

#include "tiku.h"

/* Redirect standard printf to TikuOS UART printf */
#undef printf
#define printf(...) TIKU_PRINTF(__VA_ARGS__)

#endif /* EXAMPLE_KITS_PRINTF_H_ */
