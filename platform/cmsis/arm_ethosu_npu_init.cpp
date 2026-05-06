/*
 * SPDX-FileCopyrightText: Copyright 2022-2024 Arm Limited and/or its
 * affiliates <open-source-office@arm.com>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "arm_ethosu_npu_init.hpp"
#include "ee_types.h"

#if defined(__cplusplus)
extern "C" {
#endif // defined(__cplusplus)

#include "log_macros.h"

/* Platform dependent files */
#include "RTE_Components.h"  /* Provides definition for CMSIS_device_header */
#include CMSIS_device_header /* Gives us IRQ num, base addresses. */

#if defined(ETHOSU_ARCH)
#include "ethosu_driver.h" /* Arm Ethos-U NPU driver header */
#include "include/ethosu_mem_config.h" /* Arm Ethos-U NPU memory config */

#if defined(SSE_320_FPGA) && defined(ETHOSU85) && defined(NPU0_APB_BASE_S)
int arm_ethosu_timing_adapter_fpga_init(void);
#endif

/* The newer SSE BSPs expose the U85 as NPU0. NPU0_IRQn is an enum value, so
 * use the NPU0 base-address macro as the preprocessor guard. Older U55/U65 BSPs
 * use the Ethos-U55 IRQ name, so keep that as the fallback. */
#if defined(NPU0_APB_BASE_S)
#define ARM_ETHOSU_IRQNUM ((IRQn_Type)NPU0_IRQn)
#else
#define ARM_ETHOSU_IRQNUM ((IRQn_Type)ETHOS_U55_IRQn)
#endif

#ifndef ARM_ETHOSU_IRQ_PRIORITY
#define ARM_ETHOSU_IRQ_PRIORITY 5u
#endif

#if defined(ETHOS_U_CACHE_BUF_SZ) && (ETHOS_U_CACHE_BUF_SZ > 0)
static uint8_t cache_arena[ETHOS_U_CACHE_BUF_SZ] CACHE_BUF_ATTRIBUTE;
#else  /* defined (ETHOS_U_CACHE_BUF_SZ) && (ETHOS_U_CACHE_BUF_SZ > 0) */
static uint8_t* cache_arena = NULL;
#endif /* defined (ETHOS_U_CACHE_BUF_SZ) && (ETHOS_U_CACHE_BUF_SZ > 0) */

static uint8_t* get_cache_arena()
{
    return cache_arena;
}

static size_t get_cache_arena_size()
{
#if defined(ETHOS_U_CACHE_BUF_SZ) && (ETHOS_U_CACHE_BUF_SZ > 0)
    return sizeof(cache_arena);
#else  /* defined (ETHOS_U_CACHE_BUF_SZ) && (ETHOS_U_CACHE_BUF_SZ > 0) */
    return 0;
#endif /* defined (ETHOS_U_CACHE_BUF_SZ) && (ETHOS_U_CACHE_BUF_SZ > 0) */
}

struct ethosu_driver ethosu_drv; /* Default Ethos-U device driver */

#if defined(SSE_320_FPGA) && defined(ETHOSU85)
/* MLEK relocates the vector table to writable memory on MPS4 platforms with an
 * NPU because the Ethos-U driver installs its interrupt handler dynamically via
 * NVIC_SetVector(). Do the same for the CMSIS-Pack SSE-320 FPGA build while
 * keeping the CMSIS startup code.
 *
 * Reference implementation:
 * https://git.gitlab.arm.com/artificial-intelligence/ethos-u/ml-embedded-evaluation-kit/-/blob/main/source/hal/source/platform/mps4/source/platform_drivers.c
 */
#define ARM_ETHOSU_VECTOR_COUNT 256u
static uint32_t arm_ethosu_vector_table[ARM_ETHOSU_VECTOR_COUNT] __attribute__((aligned(1024)));
static uint8_t arm_ethosu_vector_table_relocated;

static void arm_ethosu_relocate_vector_table(void)
{
    if (arm_ethosu_vector_table_relocated) {
        return;
    }

    const uint32_t *src = (const uint32_t *)SCB->VTOR;
    for (uint32_t i = 0; i < ARM_ETHOSU_VECTOR_COUNT; ++i) {
        arm_ethosu_vector_table[i] = src[i];
    }

    /* Switch VTOR only after the copy is complete, with interrupts masked so no
     * exception observes a partially relocated table. */
    __disable_irq();
    SCB->VTOR = (uint32_t)arm_ethosu_vector_table;
    __DSB();
    __ISB();
    __enable_irq();

    arm_ethosu_vector_table_relocated = 1;
}
#else
static void arm_ethosu_relocate_vector_table(void)
{
}
#endif

/** @brief   Defines the Ethos-U interrupt handler: just a wrapper around the default
 *           implementation. */
static void arm_ethosu_npu_irq_handler(void)
{
    /* Call the default interrupt handler from the NPU driver */
    ethosu_irq_handler(&ethosu_drv);
}

/** @brief  Initialises the NPU IRQ */
static void arm_ethosu_npu_irq_init(void)
{
    const IRQn_Type ethosu_irqnum = ARM_ETHOSU_IRQNUM;

    arm_ethosu_relocate_vector_table();

    /* Register the Ethos-U IRQ handler in the active vector table. For the
     * SSE-320 FPGA path this table has just been relocated to RAM. */
    NVIC_SetVector(ethosu_irqnum, (uint32_t)arm_ethosu_npu_irq_handler);
    NVIC_SetPriority(ethosu_irqnum, ARM_ETHOSU_IRQ_PRIORITY);
    NVIC_ClearPendingIRQ(ethosu_irqnum);

    /* Enable the IRQ */
    NVIC_EnableIRQ(ethosu_irqnum);

    debug("EthosU IRQ#: %u, Handler: 0x%p\n", ethosu_irqnum, arm_ethosu_npu_irq_handler);
}

/** @brief  Initialises the NPU */
static int arm_ethosu_npu_init(void) {
    int err = EE_STATUS_OK;

#if defined(SSE_320_FPGA) && defined(ETHOSU85) && defined(NPU0_APB_BASE_S)
    if (arm_ethosu_timing_adapter_fpga_init() != 0) {
        printf_err("failed to initialise Ethos-U timing adapter\n");
        return EE_STATUS_ERROR;
    }
#endif

    /* Initialise the IRQ */
    arm_ethosu_npu_irq_init();

    /* Initialise Ethos-U device */
    #ifdef NPU0_APB_BASE_S
    /* Corstone-310/315 */
    void* ethosu_base_address = (void*)(NPU0_APB_BASE_S);
    #else
    void* ethosu_base_address = (void*)(ETHOS_U55_APB_BASE_S);
    #endif

    debug("Cache arena: 0x%p\n", get_cache_arena());

    if (EE_STATUS_OK != (err = ethosu_init(&ethosu_drv,         /* Ethos-U driver device pointer */
                                ethosu_base_address, /* Ethos-U NPU's base address. */
                                get_cache_arena(),   /* Pointer to fast mem area - NULL for U55. */
                                get_cache_arena_size(), /* Fast mem region size. */
                                1,                      /* Security enable. */
                                1)))                    /* Privilege enable. */
    {
        printf_err("failed to initialise Ethos-U device\n");
        return err;
    }

    info("Ethos-U device initialised\n");

    /* Get Ethos-U version */
    struct ethosu_driver_version driver_version;
    struct ethosu_hw_info hw_info;

    ethosu_get_driver_version(&driver_version);
    ethosu_get_hw_info(&ethosu_drv, &hw_info);

    info("Ethos-U version info:\n");
    info("\tArch:       v%" PRIu32 ".%" PRIu32 ".%" PRIu32 "\n",
         hw_info.version.arch_major_rev,
         hw_info.version.arch_minor_rev,
         hw_info.version.arch_patch_rev);
    info("\tDriver:     v%" PRIu8 ".%" PRIu8 ".%" PRIu8 "\n",
         driver_version.major,
         driver_version.minor,
         driver_version.patch);
    info("\tMACs/cc:    %" PRIu32 "\n", (uint32_t)(1 << hw_info.cfg.macs_per_cc));
    info("\tCmd stream: v%" PRIu32 "\n", hw_info.cfg.cmd_stream_version);

    return EE_STATUS_OK;
}

#endif /* if defined(ETHOSU_ARCH) */

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

int ethosu_npu_init() {

    int status = EE_STATUS_ERROR;

#if defined(ETHOSU_ARCH)
    status = arm_ethosu_npu_init();
#endif

    return status;

}
