/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its
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

/*
 * Minimal Ethos-U timing adapter setup for Corstone-320/MPS4.
 *
 * MLEK enables this before the U85 is used on MPS4. Without it, command stream
 * submission can stall before the NPU ever raises an interrupt.
 */

#include <stdint.h>

#include "RTE_Components.h"
#include CMSIS_device_header

#if defined(SSE_320_FPGA) && defined(ETHOSU85) && defined(NPU0_APB_BASE_S)

    #define TA_VERSION_SUPPORTED 0x1117u

    #define TA_SRAM_BASE 0x51102000u
    #define TA_EXT_BASE 0x51102400u

    #define TA_MAXR 0x00u
    #define TA_MAXW 0x04u
    #define TA_MAXRW 0x08u
    #define TA_RLATENCY 0x0Cu
    #define TA_WLATENCY 0x10u
    #define TA_PULSE_ON 0x14u
    #define TA_PULSE_OFF 0x18u
    #define TA_BWCAP 0x1Cu
    #define TA_PERFCTRL 0x20u
    #define TA_PERFCNT 0x24u
    #define TA_MODE 0x28u
    #define TA_HISTBIN 0x30u
    #define TA_HISTCNT 0x34u
    #define TA_VERSION 0x38u

typedef struct
{
    uint32_t maxr;
    uint32_t maxw;
    uint32_t maxrw;
    uint32_t rlatency;
    uint32_t wlatency;
    uint32_t pulse_on;
    uint32_t pulse_off;
    uint32_t bwcap;
    uint32_t perfctrl;
    uint32_t perfcnt;
    uint32_t mode;
    uint32_t histbin;
    uint32_t histcnt;
} ta_settings_t;

static uint32_t ta_read(uintptr_t base, uint32_t off) { return *(volatile uint32_t *)(base + off); }

static void ta_write(uintptr_t base, uint32_t off, uint32_t val) { *(volatile uint32_t *)(base + off) = val; }

static int ta_configure(uintptr_t base, const char *name, const ta_settings_t *settings)
{
    const uint32_t version = ta_read(base, TA_VERSION);
    (void)name;

    if (version != TA_VERSION_SUPPORTED)
    {
        return -1;
    }

    ta_write(base, TA_MAXR, settings->maxr & 0x3fu);
    ta_write(base, TA_MAXW, settings->maxw & 0x3fu);
    ta_write(base, TA_MAXRW, settings->maxrw & 0x3fu);
    ta_write(base, TA_RLATENCY, settings->rlatency & 0xfffu);
    ta_write(base, TA_WLATENCY, settings->wlatency & 0xfffu);
    ta_write(base, TA_PULSE_ON, settings->pulse_on & 0xffffu);
    ta_write(base, TA_PULSE_OFF, settings->pulse_off & 0xffffu);
    ta_write(base, TA_BWCAP, settings->bwcap & 0xffffu);
    ta_write(base, TA_PERFCTRL, settings->perfctrl & 0x3fu);
    ta_write(base, TA_PERFCNT, settings->perfcnt);
    ta_write(base, TA_MODE, settings->mode & 0xfffu);
    ta_write(base, TA_HISTBIN, settings->histbin & 0x0fu);
    ta_write(base, TA_HISTCNT, settings->histcnt);

    return 0;
}

int arm_ethosu_timing_adapter_fpga_init(void)
{
    static const ta_settings_t sram = {
        .maxr = 8,
        .maxw = 8,
        .maxrw = 0,
        .rlatency = 32,
        .wlatency = 32,
        .pulse_on = 3999,
        .pulse_off = 1,
        .bwcap = 4000,
        .perfctrl = 0,
        .perfcnt = 0,
        .mode = 1,
        .histbin = 0,
        .histcnt = 0,
    };

    static const ta_settings_t ext = {
        .maxr = 64,
        .maxw = 32,
        .maxrw = 0,
        .rlatency = 500,
        .wlatency = 250,
        .pulse_on = 4000,
        .pulse_off = 1000,
        .bwcap = 3750,
        .perfctrl = 0,
        .perfcnt = 0,
        .mode = 1,
        .histbin = 0,
        .histcnt = 0,
    };

    if (ta_configure(TA_SRAM_BASE, "SRAM", &sram) != 0)
    {
        return -1;
    }

    if (ta_configure(TA_EXT_BASE, "EXT", &ext) != 0)
    {
        return -1;
    }

    return 0;
}

#endif /* defined(SSE_320_FPGA) && defined(ETHOSU85) && defined(NPU0_APB_BASE_S) */
