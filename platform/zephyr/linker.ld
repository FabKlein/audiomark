/*
 * Copyright (C) 2026 Arm Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ddr4), okay)
GROUP_START(DDR4)

	SECTION_DATA_PROLOGUE(_DDR4_SECTION_NAME,,SUBALIGN(16))
	{
		. = ALIGN(16);
		*(nn_model)
		*(tflm_model)
		*(.bss.NoInit.activation_buf_sram)
		*(tflm_arena)
		*(ifm)
	} GROUP_LINK_IN(DDR4)

GROUP_END(DDR4)
#endif
