/**
 * Copyright (C) 2022 EEMBC
 * Copyright (C) 2022 Arm Limited
 *
 * All EEMBC Benchmark Software are products of EEMBC and are provided under the
 * terms of the EEMBC Benchmark License Agreements. The EEMBC Benchmark Software
 * are proprietary intellectual properties of EEMBC and its Members and is
 * protected under all applicable laws, including all applicable copyright laws.
 *
 * If you received this EEMBC Benchmark Software without having a currently
 * effective EEMBC Benchmark License Agreement, you must discontinue use.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "ee_audiomark.h"

// There are several POSIX assumptions in this implementation.
#if defined __linux__ || __APPLE__
#include <time.h>
#elif defined _WIN32
#include <sys\timeb.h>
#elif defined __arm__
#include <RTE_Components.h>
#if defined __PERF_COUNTER__
#include "perf_counter.h"
#endif
#else
#error "Operating system not recognized"
#endif
#include <assert.h>


#if defined __arm__
#define STACK_WATERMARK_WORD 0xA5A55A5Au

/*
 * Stack watermarking uses the Arm Compiler scatter-loader stack region names.
 * AC6 provides these symbols natively from ARM_LIB_STACK. Other embedded
 * toolchains must provide compatible aliases in their linker script, for
 * example mapping them to __stack_limit and __stack. Host builds stub this out.
 */
extern uint32_t Image$$ARM_LIB_STACK$$ZI$$Base[] __asm("Image$$ARM_LIB_STACK$$ZI$$Base");
extern uint32_t Image$$ARM_LIB_STACK$$ZI$$Limit[] __asm("Image$$ARM_LIB_STACK$$ZI$$Limit");

static uint32_t *g_stack_watermark_end;

static uint32_t
read_msp(void)
{
    uint32_t sp;
    __asm volatile("mrs %0, msp" : "=r"(sp));
    return sp;
}

static void
stack_watermark_init(void)
{
    uint32_t *base = Image$$ARM_LIB_STACK$$ZI$$Base;
    uint32_t *limit = Image$$ARM_LIB_STACK$$ZI$$Limit;
    uint32_t *end = (uint32_t *)(read_msp() & ~(uint32_t)0x3);

    if (end > limit)
    {
        end = limit;
    }
    if (end <= base)
    {
        g_stack_watermark_end = base;
        return;
    }

    g_stack_watermark_end = end;
    for (uint32_t *p = base; p < end; ++p)
    {
        *p = STACK_WATERMARK_WORD;
    }
}

static size_t
stack_watermark_used_bytes(void)
{
    uint32_t *base = Image$$ARM_LIB_STACK$$ZI$$Base;
    uint32_t *limit = Image$$ARM_LIB_STACK$$ZI$$Limit;
    uint32_t *end = g_stack_watermark_end ? g_stack_watermark_end : base;
    uint32_t *p = base;

    while (p < end && *p == STACK_WATERMARK_WORD)
    {
        ++p;
    }

    return (size_t)((uintptr_t)limit - (uintptr_t)p);
}

static void
stack_watermark_report(void)
{
    size_t total = (size_t)((uintptr_t)Image$$ARM_LIB_STACK$$ZI$$Limit
                            - (uintptr_t)Image$$ARM_LIB_STACK$$ZI$$Base);
    size_t used = stack_watermark_used_bytes();
    size_t free = used < total ? total - used : 0;

    printf("Stack watermark  : %u / %u bytes used, %u bytes free\n",
           (unsigned)used,
           (unsigned)total,
           (unsigned)free);
}
#else
static void stack_watermark_init(void) {}
static void stack_watermark_report(void) {}
#endif

uint64_t
th_microseconds(void)
{
    uint64_t usec = 0;
#if defined __linux__ || __APPLE__
    const long      NSEC_PER_SEC      = 1000 * 1000 * 1000;
    const long      TIMER_RES_DIVIDER = 1000;
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    usec = t.tv_sec * (NSEC_PER_SEC / TIMER_RES_DIVIDER)
           + t.tv_nsec / TIMER_RES_DIVIDER;
#elif defined _WIN32
    struct timeb t;
    ftime(&t);
    usec = ((uint64_t)t.time) * 1000 * 1000 + ((uint64_t)t.millitm) * 1000;
#elif defined __arm__ && defined __PERF_COUNTER__
    usec = (uint64_t)get_system_us();
#else
#error "Operating system not recognized"
#endif
    return usec;
}

bool
time_audiomark_run(uint32_t iterations, uint64_t *dt)
{
    uint64_t t0  = 0;
    uint64_t t1  = 0;
    bool     err = false;

    t0 = th_microseconds();
    for (uint32_t i = 0; i < iterations; ++i)
    {
        if (ee_audiomark_run())
        {
            err = true;
            break;
        }
    }
    t1  = th_microseconds();
    *dt = t1 - t0;
    return err;
}

int
main(void)
{
    bool     err        = false;
    uint32_t iterations = 1;
    uint64_t dt         = 0;

    printf("Initializing\n");

    if (ee_audiomark_initialize())
    {
        printf("Failed to initialize\n");
        return -1;
    }

    stack_watermark_init();

    printf("Computing run speed\n");

    do
    {
        iterations *= 2;
        err = time_audiomark_run(iterations, &dt);
        if (err)
        {
            break;
        }
    } while (dt < 1e6f);

    if (err)
    {
        printf("Failed to compute iteration speed\n");
        goto exit;
    }

    // Must run for 10 sec. or at least 10 iterations
    float scale = 11e6f / dt;
    iterations  = (uint32_t)((float)iterations * scale);
    iterations  = iterations < 10 ? 10 : iterations;

    printf("Measuring\n");

    err = time_audiomark_run(iterations, &dt);
    if (err)
    {
        printf("Failed main performance run\n");
        goto exit;
    }

    /**
     * The input stream is 24e3 samples at 16 kHz, which means to exactly
     * match the throughput of the stream the score would be one iteration
     * per 1.5 seconds. The score is how many times faster than the ADC
     * the pipeline runs. x 1000 to make it a bigger number.
     */
    float sec   = (float)dt / 1.0e6f;
    float score = (float)iterations / sec * 1000.f * (1.0f / 1.5f);

    printf("Total runtime    : %.3f seconds\n", sec);
    printf("Total iterations : %d iterations\n", iterations);
    printf("Score            : %f AudioMarks\n", score);
    stack_watermark_report();
exit:
    ee_audiomark_release();
    return err ? -1 : 0;
}
