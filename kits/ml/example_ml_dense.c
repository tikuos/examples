/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * example_ml_dense.c - quantized dense inference with caller workspace.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "examples/kits/example_kits_printf.h"
#include <hal/compute/tiku_compute.h>
#include <tikukits/ml/operators/tiku_kits_ml_dense.h>

/** @brief Evaluate two signed sums with per-tensor scaling. */
void example_ml_dense_run(void)
{
    static const _Alignas(16) int8_t weights[32] = {
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    static const _Alignas(16) int8_t input[16] = {
        1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16
    };
    static const int32_t multiplier = 1073741824;
    static const int8_t shift = -1;
    static const tiku_kits_ml_dense_t model = {
        16,2,16,weights,NULL,&multiplier,&shift,1,0,0,-128,127
    };
    int8_t output[2], scratch[2];
    tiku_kits_ml_dense_status_t status =
        tiku_kits_ml_dense_s8(&model,input,output,scratch,sizeof(scratch));
    printf("Dense status=%d, compute provider=%d, scratch=%u bytes\n",
           (int)status, (int)tiku_compute_provider(TIKU_COMPUTE_DOT_S8),
           (unsigned int)tiku_kits_ml_dense_scratch_size(&model));
    if (status == TIKU_KITS_ML_DENSE_OK) {
        printf("Output: %d %d; expected: 34 -34\n",
               (int)output[0], (int)output[1]);
    }
}
