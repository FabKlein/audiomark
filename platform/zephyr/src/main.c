/**
 * Copyright (C) 2023 EEMBC
 * Copyright (C) 2026 Arm Limited
 *
 * All EEMBC Benchmark Software are products of EEMBC and are provided under the
 * terms of the EEMBC Benchmark License Agreements. The EEMBC Benchmark Software
 * are proprietary intellectual properties of EEMBC and its Members and is
 * protected under all applicable laws, including all applicable copyright laws.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <zephyr/kernel.h>

#include "ee_audiomark.h"

static uint64_t
th_microseconds(void)
{
    return k_cyc_to_us_floor64(k_cycle_get_64());
}

static bool
time_audiomark_run(uint32_t iterations, uint64_t *dt)
{
    uint64_t t0  = th_microseconds();
    bool     err = false;

    for (uint32_t i = 0; i < iterations; ++i)
    {
        if (ee_audiomark_run())
        {
            err = true;
            break;
        }
    }

    *dt = th_microseconds() - t0;
    return err;
}

int
main(void)
{
    bool     err        = false;
    uint32_t iterations = 1;
    uint64_t dt         = 0;

    printf("Initializing AudioMark\n");

    if (ee_audiomark_initialize())
    {
        printf("Failed to initialize AudioMark\n");
        return -1;
    }

    printf("Computing run speed\n");

    do
    {
        iterations *= 2;
        err = time_audiomark_run(iterations, &dt);
        if (err)
        {
            break;
        }
    } while (dt < 1000000ULL);

    if (err)
    {
        printf("Failed to compute iteration speed\n");
        goto exit;
    }

    float scale = 11000000.0f / (float)dt;
    iterations  = (uint32_t)((float)iterations * scale);
    iterations  = iterations < 10 ? 10 : iterations;

    printf("Measuring\n");

    err = time_audiomark_run(iterations, &dt);
    if (err)
    {
        printf("Failed main performance run\n");
        goto exit;
    }

    float sec   = (float)dt / 1000000.0f;
    float score = (float)iterations / sec * 1000.0f * (1.0f / 1.5f);

    printf("Total runtime    : %.3f seconds\n", (double)sec);
    printf("Total iterations : %u iterations\n", iterations);
    printf("Score            : %f AudioMarks\n", (double)score);

exit:
    ee_audiomark_release();
    return err ? -1 : 0;
}
