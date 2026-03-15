/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * example_kits_runner.h - TikuKits example dispatcher
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EXAMPLE_KITS_RUNNER_H_
#define EXAMPLE_KITS_RUNNER_H_

/**
 * @brief Run the currently enabled TikuKits example.
 *
 * Dispatches to whichever example_*_run() function corresponds to
 * the TIKU_EXAMPLE_KITS_* flag set to 1 in tiku_example_config.h.
 * Does nothing if no kits example is enabled.
 */
void example_kits_run(void);

#endif /* EXAMPLE_KITS_RUNNER_H_ */
