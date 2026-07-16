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
#if defined(AUDIOMARK_ARM_WHOLE_RUN_PERFMON)
#include <errno.h>
#include <string.h>
#endif

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


#if defined(AUDIOMARK_ARM_WHOLE_RUN_PERFMON)
static FILE       *audiomark_perfmon_fp;
static const char *audiomark_perfmon_events = "17 8 33 27 114 115 0";

static bool
audiomark_perfmon_start(const char *phase)
{
    (void)phase;

    if (!audiomark_perfmon_fp)
    {
        audiomark_perfmon_fp = fopen("/proc/perfmon", "r+");
        if (!audiomark_perfmon_fp)
        {
            fprintf(stderr, "error: could not open /proc/perfmon: %s\n",
                    strerror(errno));
            return true;
        }
    }

    fprintf(audiomark_perfmon_fp, "%s\n", audiomark_perfmon_events);
    fflush(audiomark_perfmon_fp);
    __asm volatile("SEV \n\t" : : : "memory");

    return false;
}

static void
audiomark_perfmon_stop(const char *phase)
{
    (void)phase;

    __asm volatile("SEV \n\t" : : : "memory");

    if (audiomark_perfmon_fp)
    {
        /* Dump stats before disabling; the perfmon driver resets on disable. */
        (void)fgetc(audiomark_perfmon_fp);
        fputs("300\n", audiomark_perfmon_fp);
        fflush(audiomark_perfmon_fp);
        fclose(audiomark_perfmon_fp);
        audiomark_perfmon_fp = NULL;
    }
}
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

#if defined(AUDIOMARK_ARM_WHOLE_RUN_PERFMON)
static bool
time_audiomark_run_with_perfmon(uint32_t iterations, uint64_t *dt,
                                const char *phase)
{
    bool err = false;

    if (audiomark_perfmon_start(phase))
    {
        return true;
    }

    err = time_audiomark_run(iterations, dt);
    audiomark_perfmon_stop(phase);

    return err;
}
#endif

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

    printf("Computing run speed\n");

#if defined(AUDIOMARK_ARM_WHOLE_RUN_PERFMON)
    printf("Whole-run perfmon profiling enabled\n");
    iterations = 1;
    err        = time_audiomark_run_with_perfmon(iterations, &dt, "run-speed");
#else
    do
    {
        iterations *= 2;
        err = time_audiomark_run(iterations, &dt);
        if (err)
        {
            break;
        }
    } while (dt < 1e6f);
#endif

    if (err)
    {
        printf("Failed to compute iteration speed\n");
        goto exit;
    }

#if !defined(AUDIOMARK_ARM_WHOLE_RUN_PERFMON)
    // Must run for 10 sec. or at least 10 iterations
    float scale = 11e6f / dt;
    iterations  = (uint32_t)((float)iterations * scale);
    iterations  = iterations < 10 ? 10 : iterations;
#else
    iterations = 1;
#endif

    printf("Measuring\n");

#if defined(AUDIOMARK_ARM_WHOLE_RUN_PERFMON)
    err = time_audiomark_run_with_perfmon(iterations, &dt, "measurement");
#else
    err = time_audiomark_run(iterations, &dt);
#endif
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
exit:
    ee_audiomark_release();
    return err ? -1 : 0;
}
