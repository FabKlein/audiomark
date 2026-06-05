/**
 * Private Arm-port AudioMark timing wrappers.
 *
 * This file is intentionally kept under ports/arm so the upstream benchmark
 * run loop can remain untouched.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(AUDIOMARK_ARM_PROFILE_COUNTER_LINUX_NS)
#include <time.h>
#endif

#include "ee_api.h"
#include "ee_audiomark.h"

#define AUDIO_COUNTER_MAX_ITERATIONS 20
#define AUDIO_COUNTER_ESTIMATE_FIRST_ITERATION 10
#define AUDIO_COUNTER_KWS_MIN_TICKS 1000ULL

#if defined(AUDIOMARK_ARM_PROFILE_COUNTER_LINUX_NS)
#define AUDIO_PROFILE_COUNTER_UNIT "ns"
#else
#define AUDIO_PROFILE_COUNTER_UNIT "ticks"
#endif

enum audio_profile_stage
{
    AUDIO_PROFILE_ABF = 0,
    AUDIO_PROFILE_AEC,
    AUDIO_PROFILE_ANR,
    AUDIO_PROFILE_KWS,
    AUDIO_PROFILE_INVOKE,
    AUDIO_PROFILE_STAGE_COUNT,
};

static const char *stage_names[AUDIO_PROFILE_STAGE_COUNT]
    = { "ABF", "AEC", "ANR", "KWS", "TFLite Invoke" };

static uint64_t stage_samples[AUDIO_PROFILE_STAGE_COUNT][AUDIO_COUNTER_MAX_ITERATIONS];
static uint64_t stage_totals[AUDIO_PROFILE_STAGE_COUNT];
static uint32_t stage_counts[AUDIO_PROFILE_STAGE_COUNT];
static uint32_t audio_profile_sample_count;
static int      audio_profile_printed;

int32_t __real_ee_abf_f32(int32_t command, void **pp_inst, void *p_data, void *p_params);
int32_t __real_ee_aec_f32(int32_t command, void **pp_inst, void *p_data, void *p_params);
int32_t __real_ee_anr_f32(int32_t command, void **pp_inst, void *p_data, void *p_params);
int32_t __real_ee_kws_f32(int32_t command, void **pp_inst, void *p_data, void *p_params);

static inline uint64_t
audio_profile_read_counter(void)
{
#if defined(AUDIOMARK_ARM_PROFILE_COUNTER_LINUX_NS)
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
#elif defined(__aarch64__)
    uint64_t value;

    __asm__ volatile("isb\n"
                     "mrs %0, cntvct_el0\n"
                     "isb"
                     : "=r"(value)
                     :
                     : "memory");
    return value;
#else
    return 0;
#endif
}

static double
audio_profile_counter_to_cycles(uint64_t count)
{
#if defined(AUDIOMARK_ARM_PROFILE_COUNTER_LINUX_NS)
#if defined(AUDIOMARK_ARM_PROFILE_CORE_FREQ_HZ)
    return ((double)count * (double)AUDIOMARK_ARM_PROFILE_CORE_FREQ_HZ) / 1000000000.0;
#else
    (void)count;
    return 0.0;
#endif
#else
    return (double)count;
#endif
}

static int
audio_profile_can_estimate_audiomark(void)
{
#if defined(AUDIOMARK_ARM_PROFILE_COUNTER_LINUX_NS) \
    && !defined(AUDIOMARK_ARM_PROFILE_CORE_FREQ_HZ)
    return 0;
#else
    return 1;
#endif
}

static void
audio_profile_record(enum audio_profile_stage stage, uint64_t ticks)
{
    uint32_t sample_index = audio_profile_sample_count;

    if (sample_index < AUDIO_COUNTER_MAX_ITERATIONS)
    {
        stage_samples[stage][sample_index] = ticks;
        stage_totals[stage] += ticks;
        stage_counts[stage]++;
    }
}

static void
audio_profile_print_summary_row(enum audio_profile_stage stage)
{
    uint32_t count = stage_counts[stage];
    uint64_t total = stage_totals[stage];

    printf("%-14s %8u %16llu %16llu\n",
           stage_names[stage],
           count,
           (unsigned long long)total,
           count == 0 ? 0 : (unsigned long long)(total / count));
}

static void
audio_profile_print_estimate(void)
{
    const uint32_t first_index = AUDIO_COUNTER_ESTIMATE_FIRST_ITERATION - 1;
    double         abf_total   = 0.0;
    double         aec_total   = 0.0;
    double         anr_total   = 0.0;
    double         kws_total   = 0.0;
    uint32_t       kws_count   = 0;
    uint32_t       window_count;
    double         avg_abf;
    double         avg_aec;
    double         avg_anr;
    double         avg_kws;
    double         denominator;
    double         audiomark_per_mhz;

    printf("AudioMark/MHz estimate:\n");

    if (!audio_profile_can_estimate_audiomark())
    {
        printf("disabled: %s samples require AUDIOMARK_ARM_PROFILE_CORE_FREQ_HZ\n",
               AUDIO_PROFILE_COUNTER_UNIT);
        return;
    }

    if (audio_profile_sample_count <= first_index)
    {
        printf("not enough samples: need iteration %u, got %u\n",
               AUDIO_COUNTER_ESTIMATE_FIRST_ITERATION,
               audio_profile_sample_count);
        return;
    }

    for (uint32_t i = first_index; i < audio_profile_sample_count; i++)
    {
        abf_total += audio_profile_counter_to_cycles(stage_samples[AUDIO_PROFILE_ABF][i]);
        aec_total += audio_profile_counter_to_cycles(stage_samples[AUDIO_PROFILE_AEC][i]);
        anr_total += audio_profile_counter_to_cycles(stage_samples[AUDIO_PROFILE_ANR][i]);

        if (stage_samples[AUDIO_PROFILE_KWS][i] >= AUDIO_COUNTER_KWS_MIN_TICKS)
        {
            kws_total += audio_profile_counter_to_cycles(stage_samples[AUDIO_PROFILE_KWS][i]);
            kws_count++;
        }
    }

    window_count = audio_profile_sample_count - first_index;
    avg_abf      = (double)abf_total / (double)window_count;
    avg_aec      = (double)aec_total / (double)window_count;
    avg_anr      = (double)anr_total / (double)window_count;
    avg_kws      = kws_count == 0 ? 0.0 : (double)kws_total / (double)kws_count;
    denominator  = 95.0 * (avg_abf + avg_aec + avg_anr) + 73.0 * avg_kws;

    audiomark_per_mhz = denominator == 0.0 ? 0.0 : (1.0 / 1.5) * 1000000000.0 / denominator;

    printf("using iterations %u-%u, KWS samples >= %llu %s\n",
           AUDIO_COUNTER_ESTIMATE_FIRST_ITERATION,
           audio_profile_sample_count,
           (unsigned long long)AUDIO_COUNTER_KWS_MIN_TICKS,
           AUDIO_PROFILE_COUNTER_UNIT);
#if defined(AUDIOMARK_ARM_PROFILE_COUNTER_LINUX_NS) \
    && defined(AUDIOMARK_ARM_PROFILE_CORE_FREQ_HZ)
    printf("converted with core frequency: %llu Hz\n",
           (unsigned long long)AUDIOMARK_ARM_PROFILE_CORE_FREQ_HZ);
#endif
    printf("%-14s %8s %16s\n", "stage", "samples", "avg cycles");
    printf("%-14s %8u %16llu\n", "ABF", window_count, (unsigned long long)avg_abf);
    printf("%-14s %8u %16llu\n", "AEC", window_count, (unsigned long long)avg_aec);
    printf("%-14s %8u %16llu\n", "ANR", window_count, (unsigned long long)avg_anr);
    printf("%-14s %8u %16llu\n", "KWS", kws_count, (unsigned long long)avg_kws);
    printf("AudioMark/MHz = %.6f\n", audiomark_per_mhz);
}

static void
audio_profile_print_summary(void)
{
    if (audio_profile_printed)
    {
        return;
    }

    audio_profile_printed = 1;

    printf("Audio counter samples (%s):\n", AUDIO_PROFILE_COUNTER_UNIT);
    printf("%5s %14s %14s %14s %14s %16s\n",
           "iter",
           "ABF",
           "AEC",
           "ANR",
           "KWS",
           "TFLite Invoke");

    for (uint32_t i = 0; i < audio_profile_sample_count; i++)
    {
        printf("%5u %14llu %14llu %14llu %14llu %16llu\n",
               i + 1,
               (unsigned long long)stage_samples[AUDIO_PROFILE_ABF][i],
               (unsigned long long)stage_samples[AUDIO_PROFILE_AEC][i],
               (unsigned long long)stage_samples[AUDIO_PROFILE_ANR][i],
               (unsigned long long)stage_samples[AUDIO_PROFILE_KWS][i],
               (unsigned long long)stage_samples[AUDIO_PROFILE_INVOKE][i]);
    }

    printf("Audio counter summary (%s):\n", AUDIO_PROFILE_COUNTER_UNIT);
    printf("%-14s %8s %16s %16s\n", "stage", "calls", "total", "avg");
    audio_profile_print_summary_row(AUDIO_PROFILE_ABF);
    audio_profile_print_summary_row(AUDIO_PROFILE_AEC);
    audio_profile_print_summary_row(AUDIO_PROFILE_ANR);
    audio_profile_print_summary_row(AUDIO_PROFILE_KWS);
    audio_profile_print_summary_row(AUDIO_PROFILE_INVOKE);
    audio_profile_print_estimate();
}

static int32_t
audio_profile_wrap_component(enum audio_profile_stage stage,
                             int32_t (*real_fn)(int32_t, void **, void *, void *),
                             int32_t command,
                             void  **pp_inst,
                             void   *p_data,
                             void   *p_params)
{
    uint64_t begin;
    int32_t  status;
    uint64_t end;

    if (command != NODE_RUN || audio_profile_sample_count >= AUDIO_COUNTER_MAX_ITERATIONS)
    {
        return real_fn(command, pp_inst, p_data, p_params);
    }

    begin  = audio_profile_read_counter();
    status = real_fn(command, pp_inst, p_data, p_params);
    end    = audio_profile_read_counter();

    audio_profile_record(stage, end - begin);

    return status;
}

int32_t
__wrap_ee_abf_f32(int32_t command, void **pp_inst, void *p_data, void *p_params)
{
    return audio_profile_wrap_component(
        AUDIO_PROFILE_ABF, __real_ee_abf_f32, command, pp_inst, p_data, p_params);
}

int32_t
__wrap_ee_aec_f32(int32_t command, void **pp_inst, void *p_data, void *p_params)
{
    return audio_profile_wrap_component(
        AUDIO_PROFILE_AEC, __real_ee_aec_f32, command, pp_inst, p_data, p_params);
}

int32_t
__wrap_ee_anr_f32(int32_t command, void **pp_inst, void *p_data, void *p_params)
{
    return audio_profile_wrap_component(
        AUDIO_PROFILE_ANR, __real_ee_anr_f32, command, pp_inst, p_data, p_params);
}

int32_t
__wrap_ee_kws_f32(int32_t command, void **pp_inst, void *p_data, void *p_params)
{
    uint64_t begin;
    int32_t  status;
    uint64_t end;
    uint64_t invoke_ticks;

    if (command != NODE_RUN || audio_profile_sample_count >= AUDIO_COUNTER_MAX_ITERATIONS)
    {
        return __real_ee_kws_f32(command, pp_inst, p_data, p_params);
    }

    th_nn_reset_last_invoke_cycles();

    begin  = audio_profile_read_counter();
    status = __real_ee_kws_f32(command, pp_inst, p_data, p_params);
    end    = audio_profile_read_counter();

    invoke_ticks = th_nn_last_invoke_cycles();

    audio_profile_record(AUDIO_PROFILE_KWS, end - begin);
    stage_samples[AUDIO_PROFILE_INVOKE][audio_profile_sample_count] = invoke_ticks;

    if (invoke_ticks != 0)
    {
        stage_totals[AUDIO_PROFILE_INVOKE] += invoke_ticks;
        stage_counts[AUDIO_PROFILE_INVOKE]++;
    }

    audio_profile_sample_count++;

    if (audio_profile_sample_count >= AUDIO_COUNTER_MAX_ITERATIONS)
    {
        audio_profile_print_summary();
        fflush(stdout);
#if defined(AUDIOMARK_ARM_PROFILE_EXIT_AFTER_SAMPLES)
        exit(0);
#endif
    }

    return status;
}
