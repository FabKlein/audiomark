/* Header for target specific MPU definitions */
#ifndef CMSIS_device_header
/* CMSIS pack default header, containing the CMSIS_device_header definition */
#include "RTE_Components.h"
#endif
#include CMSIS_device_header

#include <stdint.h>

#if defined(SSE_320_FPGA)
/*
 * Corstone-320 FPGA uses a two-image load flow: boot.bin is loaded at
 * 0x11000000 and bram.bin is loaded at 0x12000000 by the board monitor.
 *
 * For this target we place hot code/data in ITCM/DTCM, and the SRAM-only
 * Ethos-U model in ISRAM, but their initialized bytes are carried in bram.bin.
 * The normal ELF loader/scatter-loader is not present in the FPGA boot path, so
 * copy those load regions into their execution addresses before any benchmark
 * code, CMSIS-DSP code, or Ethos-U model access can use them.
 *
 * Keep this SSE_320_FPGA-specific: other platforms either rely on their runtime
 * loader/linker startup, use a different memory map, or do not have TCM at all
 * such as Cortex-M4 based targets.
 */
extern uint8_t __itcm_load_start[] __attribute__((weak));
extern uint8_t __itcm_start[] __attribute__((weak));
extern uint8_t __itcm_end[] __attribute__((weak));
extern uint8_t __dtcm_load_start[] __attribute__((weak));
extern uint8_t __dtcm_start[] __attribute__((weak));
extern uint8_t __dtcm_end[] __attribute__((weak));
extern uint8_t __sram_model_load_start[] __attribute__((weak));
extern uint8_t __sram_model_start[] __attribute__((weak));
extern uint8_t __sram_model_end[] __attribute__((weak));

static void copy_load_region(uint8_t *dst, const uint8_t *src, const uint8_t *end)
{
    if ((dst == 0) || (src == 0) || (end == 0) || (dst == src))
    {
        return;
    }

    while (dst < end)
    {
        *dst++ = *src++;
    }
}

static void copy_sse320_fpga_load_regions(void)
{
    copy_load_region(__itcm_start, __itcm_load_start, __itcm_end);
    copy_load_region(__dtcm_start, __dtcm_load_start, __dtcm_end);
    copy_load_region(__sram_model_start, __sram_model_load_start, __sram_model_end);
}
#endif


#if defined __PERF_COUNTER__

#include "perf_counter.h"

__attribute__((used)) void SysTick_Handler(void)
{
    perfc_port_insert_to_system_timer_insert_ovf_handler();
}

static void init_perf_counter(void)
{
    extern void SystemCoreClockUpdate (void);

#if (defined (IOTKit_CM33) || (IOTKit_CM33_FP) || (IOTKit_CM33_VHT) || (IOTKit_CM33_FP_VHT))
    /*  Correct SystemCoreClock to match the clock speed of the FPGA platform */
    SystemCoreClock = 20000000UL;
#else
    SystemCoreClockUpdate();
#endif
    init_cycle_counter(false);
}

#endif

__attribute__((constructor(255)))
void platform_init(void)
{
#if defined(SSE_320_FPGA)
    copy_sse320_fpga_load_regions();
#endif

#if defined __PERF_COUNTER__
    init_perf_counter();
#endif

#ifdef RTE_CMSIS_Compiler_STDOUT_Custom
   extern void stdout_init(void);
   stdout_init();
#endif

#if (defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U))
    SCB_EnableDCache();
#endif

#if(defined (__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U))
    SCB_EnableICache();
#endif
}
