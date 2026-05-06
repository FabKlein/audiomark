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
 * AudioMark manages the relevant cache maintenance at the application/memory
 * placement level. Do not enable these Ethos-U driver cache callbacks here:
 * the driver calls them around every inference, and a coarse clean/invalidate
 * of the D-cache on that path adds measurable benchmark overhead.
 */
#if 0
#include "RTE_Components.h"
#include CMSIS_device_header

#include "ethosu_driver.h"

static uintptr_t align_down_32(uint64_t addr) { return (uintptr_t)(addr & ~31ull); }

static size_t align_size_32(uint64_t addr, size_t size)
{
    const uintptr_t start = align_down_32(addr);
    const uintptr_t end = (uintptr_t)((addr + size + 31ull) & ~31ull);

    return (size_t)(end - start);
}

void ethosu_flush_dcache(const uint64_t *base_addr, const size_t *base_addr_size, int num_base_addr)
{
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if (SCB->CCR & SCB_CCR_DC_Msk)
    {
        for (int i = 0; i < num_base_addr; ++i)
        {
            const uintptr_t addr = align_down_32(base_addr[i]);
            const size_t size = align_size_32(base_addr[i], base_addr_size[i]);
            SCB_CleanDCache_by_Addr((void *)addr, (int32_t)size);
        }
    }
#else
    (void)base_addr;
    (void)base_addr_size;
    (void)num_base_addr;
#endif
}

void ethosu_invalidate_dcache(const uint64_t *base_addr, const size_t *base_addr_size, int num_base_addr)
{
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if (SCB->CCR & SCB_CCR_DC_Msk)
    {
        for (int i = 0; i < num_base_addr; ++i)
        {
            const uintptr_t addr = align_down_32(base_addr[i]);
            const size_t size = align_size_32(base_addr[i], base_addr_size[i]);
            SCB_InvalidateDCache_by_Addr((void *)addr, (int32_t)size);
        }
    }
#else
    (void)base_addr;
    (void)base_addr_size;
    (void)num_base_addr;
#endif
}
#endif
