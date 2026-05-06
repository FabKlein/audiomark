/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2019 Keith Packard
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* ----------------------------------------------------------------------------
  Stack seal size definition
 *----------------------------------------------------------------------------*/
#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
#define __STACKSEAL_SIZE   ( 8 )
#else
#define __STACKSEAL_SIZE   ( 0 )
#endif

/* TCMs */
#define DTCM_BASE  0x30000000
#define DTCM_SIZE  0x00008000

/* FVP image layout.
 * Match the working AC6/CLANG layout: keep only the boot vectors at the
 * secure boot alias, and place the application image in BRAM.
 */
#define BOOT_BASE  S_ROM_ALIAS
#define BOOT_SIZE  0x00010000
#define BRAM_BASE  0x12000000
#define BRAM_SIZE  0x00200000

/* Internal  SRAM */
#define ISRAM_BASE S_RAM_ALIAS
#define ISRAM_SIZE TOTAL_S_RAM_SIZE

#ifndef HEAP_SIZE
#define HEAP_SIZE 0x00097000
#endif
#undef STACK_SIZE
#define STACK_SIZE 0x00008000

#if defined (__FVP__) && (__FVP__ == 1)
#define DDR_BASE  0x31000000
#else
#define DDR_BASE  0x70000000
#endif
#define DDR_SIZE  0x10000000



/* ----------------------------------------------------------------------------
  Memory definition
 *----------------------------------------------------------------------------*/
MEMORY
{
  BOOT  (rx!w)  : ORIGIN = BOOT_BASE, LENGTH = BOOT_SIZE
  BRAM  (rwx)   : ORIGIN = BRAM_BASE, LENGTH = BRAM_SIZE
#if __ROM1_SIZE > 0
  ROM1  (rx!w)  : ORIGIN = __ROM1_BASE, LENGTH = __ROM1_SIZE
#endif
#if __ROM2_SIZE > 0
  ROM2  (rx!w)  : ORIGIN = __ROM2_BASE, LENGTH = __ROM2_SIZE
#endif
#if __ROM3_SIZE > 0
  ROM3  (rx!w)  : ORIGIN = __ROM3_BASE, LENGTH = __ROM3_SIZE
#endif

  DTCM  (w!rx) : ORIGIN = DTCM_BASE, LENGTH = DTCM_SIZE
  ISRAM (w!rx) : ORIGIN = ISRAM_BASE,  LENGTH = ISRAM_SIZE
  DDR   (w!rx) : ORIGIN = DDR_BASE,  LENGTH = DDR_SIZE

#if __RAM2_SIZE > 0
  RAM2  (w!rx) : ORIGIN = __RAM2_BASE, LENGTH = __RAM2_SIZE
#endif
#if __RAM3_SIZE > 0
  RAM3  (w!rx) : ORIGIN = __RAM3_BASE, LENGTH = __RAM3_SIZE
#endif
}

ENTRY(Reset_Handler)

PHDRS
{
	boot PT_LOAD;
	bram_ro PT_LOAD;
	bram_rw PT_LOAD;
	isram PT_LOAD;
	dtcm PT_LOAD;
	tls PT_TLS;
}

SECTIONS
{
	.itcm : {
		/* Keep the AudioMark CMSIS-DSP floating-point kernels near the main
		 * application image. Do not catch all .text.arm_* sections: CMSIS-NN
		 * also uses that prefix and GCC 15 emits enough NN code to grow this
		 * area substantially.
		 */
		* (.text.arm_*_f32)
        } >BRAM AT>BRAM :bram_ro

	.dtcm : {
    	/**(.text.arm_cfft_*)*/
		* (.rodata.ee_mfcc_*)
		* (.rodata.w_hanning_*)
		*CommonTables* (.rodata*)
        } >BRAM AT>BRAM :bram_ro

	.init : {
		KEEP (*(.vectors))
		KEEP (*(.text.init.enter))
		KEEP (*(.data.init.enter))
		KEEP (*(SORT_BY_NAME(.init) SORT_BY_NAME(.init.*)))
	} >BOOT AT>BOOT :boot




	.text : {

		/* code */
		*(.text.unlikely .text.unlikely.*)
		*(.text.startup .text.startup.*)
		*(.text .text.* .opd .opd.*)
		*(.gnu.linkonce.t.*)
		KEEP (*(.fini .fini.*))
		__text_end = .;

		PROVIDE (__etext = __text_end);
		PROVIDE (_etext = __text_end);
		PROVIDE (etext = __text_end);

		/* read-only data */
		*(.gnu.linkonce.r.*)

		*(.srodata.cst16)
		*(.srodata.cst8)
		*(.srodata.cst4)
		*(.srodata.cst2)
		*(.srodata .srodata.*)
		*(.data.rel.ro .data.rel.ro.*)
		*(.got .got.*)

		/* Need to pre-align so that the symbols come after padding */
		. = ALIGN(8);

		/* lists of constructors and destructors */
		PROVIDE_HIDDEN ( __preinit_array_start = . );
		KEEP (*(.preinit_array))
		PROVIDE_HIDDEN ( __preinit_array_end = . );

		PROVIDE_HIDDEN ( __init_array_start = . );
		KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP (*(.init_array .ctors))
		PROVIDE_HIDDEN ( __init_array_end = . );

		PROVIDE_HIDDEN ( __fini_array_start = . );
		KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
		KEEP (*(.fini_array .dtors))
		PROVIDE_HIDDEN ( __fini_array_end = . );

	} >BRAM AT>BRAM :bram_ro

#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
	.veneers :
	{
		. = ALIGN(32);
		KEEP(*(.gnu.sgstubs))
	} > BRAM AT>BRAM :bram_ro
#endif

	.toc : {
		*(.toc .toc.*)
	} >BRAM AT>BRAM :bram_ro

	/* additional sections when compiling with C++ exception support */

	.except_ordered : {
		*(.gcc_except_table *.gcc_except_table.*)
		KEEP (*(.eh_frame .eh_frame.*))
		*(.ARM.extab* .gnu.linkonce.armextab.*)
	} >BRAM AT>BRAM :bram_ro

	.except_unordered : {
		. = ALIGN(8);

		PROVIDE(__exidx_start = .);
		*(.ARM.exidx*)
		PROVIDE(__exidx_end = .);
	} >BRAM AT>BRAM :bram_ro

#if 1
  .copy.table :
  {
    . = ALIGN(4);
    __copy_table_start__ = .;

    LONG (LOADADDR(.data))
    LONG (ADDR(.data))
    LONG (SIZEOF(.data) / 4)

    /* Add each additional data section here */
/*
    LONG (LOADADDR(.data2))
    LONG (ADDR(.data2))
    LONG (SIZEOF(.data2) / 4)
*/
    __copy_table_end__ = .;
  } > BRAM :bram_ro


  .zero.table :
  {
    . = ALIGN(4);
    __zero_table_start__ = .;

/*  .bss initialization to zero is already done during C Run-Time Startup.
    LONG (ADDR(.bss))
    LONG (SIZEOF(.bss) / 4)
*/

    /* Add each additional bss section here */
/*
    LONG (ADDR(.bss2))
    LONG (SIZEOF(.bss2) / 4)
*/
    __zero_table_end__ = .;
  } > BRAM :bram_ro
#endif

	/*
	 * Data values which are preserved across reset
	 */
	.preserve (NOLOAD) : {
		PROVIDE(__preserve_start__ = .);
		KEEP(*(SORT_BY_NAME(.preserve.*)))
		KEEP(*(.preserve))
		PROVIDE(__preserve_end__ = .);
	} >BRAM AT>BRAM :bram_rw

    .sram_model : {
        /* Keep Ethos-U constants in internal SRAM for the FVP SRAM-only flow.
         * This intentionally avoids a DDR load segment.
         */
        . = ALIGN(16);
        * (ifm)
        . = ALIGN(16);
        * (nn_model)
        . = ALIGN(16);
        * (labels)
        . = ALIGN(16);
        * (activation_buf_dram)
        . = ALIGN(16);
    } > ISRAM : isram

    .isram (NOLOAD):     {
        /* Cache area (if used) */
        * (.bss.NoInit.ethos_u_cache)

        /* activation buffers a.k.a tensor arena when */
        /* memory mode sram only or shared sram */
        *(.bss.NoInit.activation_buf_sram)
    } > ISRAM : isram


	.data :  {
		*(.data .data.*)
		*(.gnu.linkonce.d.*)
		*(.rodata .rodata.*)

		/* Need to pre-align so that the symbols come after padding */
		. = ALIGN(8);

		PROVIDE( __global_pointer$ = . + 0x800 );
		*(.sdata .sdata.* .sdata2.*)
		*(.gnu.linkonce.s.*)
	} >BRAM AT>BRAM :bram_rw
	PROVIDE(__data_start = ADDR(.data));
	PROVIDE(__data_source = LOADADDR(.data));

	/* Thread local initialized data. This gets
	 * space allocated as it is expected to be placed
	 * in ram to be used as a template for TLS data blocks
	 * allocated at runtime. We're slightly abusing that
	 * by placing the data in flash where it will be copied
	 * into the allocate ram addresses by the existing
	 * data initialization code in crt0
	 */
	.tdata :  {
		*(.tdata .tdata.* .gnu.linkonce.td.*)
		PROVIDE(__data_end = .);
		PROVIDE(__tdata_end = .);
	} >BRAM AT>BRAM :tls :bram_rw
	PROVIDE( __tls_base = ADDR(.tdata));
	PROVIDE( __tdata_start = ADDR(.tdata));
	PROVIDE( __tdata_source = LOADADDR(.tdata) );
	PROVIDE( __tdata_source_end = LOADADDR(.tdata) + SIZEOF(.tdata) );
	PROVIDE( __data_source_end = __tdata_source_end );
	PROVIDE( __tdata_size = SIZEOF(.tdata) );
	PROVIDE( __tls_align = MAX(ALIGNOF(.tdata),ALIGNOF(.tbss)) );

	PROVIDE( __edata = __data_end );
	PROVIDE( _edata = __data_end );
	PROVIDE( edata = __data_end );
	PROVIDE( __data_size = __data_end - __data_start );
	PROVIDE( __data_source_size = __data_source_end - __data_source );

	.tbss (NOLOAD) : {
		*(.tbss .tbss.* .gnu.linkonce.tb.*)
		*(.tcommon)
		PROVIDE( __tls_end = . );
		PROVIDE( __tbss_end = . );
	} >DTCM AT>DTCM :tls :dtcm
	PROVIDE( __bss_start = ADDR(.tbss));
	PROVIDE( __tbss_start = ADDR(.tbss));
	PROVIDE( __tbss_offset = ADDR(.tbss) - ADDR(.tdata) );
	PROVIDE( __tbss_size = SIZEOF(.tbss) );
	PROVIDE( __tls_size = __tls_end - __tls_base );
	PROVIDE( __tls_align = MAX(ALIGNOF(.tdata), ALIGNOF(.tbss)) );
	PROVIDE( __arm32_tls_tcb_offset = MAX(8, __tls_align) );
	PROVIDE( __arm64_tls_tcb_offset = MAX(16, __tls_align) );

	/*
	 * The linker special cases .tbss segments which are
	 * identified as segments which are not loaded and are
	 * thread_local.
	 *
	 * For these segments, the linker does not advance 'dot'
	 * across them.  We actually need memory allocated for tbss,
	 * so we create a special segment here just to make room
	 */
	/*
	.tbss_space (NOLOAD) : {
		. = ADDR(.tbss);
		. = . + SIZEOF(.tbss);
	} >BRAM AT>BRAM :bram_rw
	*/

	.bss (NOLOAD) : {
    . = ALIGN(4);
    __bss_start__ = .;
		*(.sbss*)
		*(.gnu.linkonce.sb.*)
		*(.bss .bss.*)
		*(.gnu.linkonce.b.*)
		*(COMMON)

		/* Align the heap */
		. = ALIGN(8);
		__bss_end = .;
    __bss_end__ = .;

	} >BRAM AT>BRAM :bram_rw
	PROVIDE( __non_tls_bss_start = ADDR(.bss) );
	PROVIDE( __end = __bss_end );
	PROVIDE( _end = __bss_end );
	PROVIDE( end = __bss_end );
	PROVIDE( __end__ = __bss_end );
	PROVIDE( __bss_size = __bss_end - __bss_start );

	/* Make the rest of memory available for heap storage */
	PROVIDE (__heap_start = __end);
#ifdef HEAP_SIZE
	PROVIDE (__heap_end = __heap_start + HEAP_SIZE);

#else
	PROVIDE (__heap_end = __stack - STACK_SIZE);
	PROVIDE (HEAP_SIZE = __heap_end - __heap_start);
#endif
PROVIDE (__HeapLimit = __heap_end);

	.heap (NOLOAD) : {
		. += HEAP_SIZE;
	} >BRAM :bram_rw

	/* Define a stack region to make sure it fits in memory */
	PROVIDE(__stack = ORIGIN(DTCM) + LENGTH(DTCM) - __STACKSEAL_SIZE);
	PROVIDE(__stack_limit = __stack - STACK_SIZE);

	PROVIDE(__StackTop = __stack);
	PROVIDE(__StackLimit = __stack_limit);
	.stack (__stack_limit) (NOLOAD) : {
		. += STACK_SIZE;
	} >DTCM :dtcm

#if __STACKSEAL_SIZE > 0
	PROVIDE(__stack_seal = __stack);
  	.stackseal (__stack) (NOLOAD) :
  	{
    	. += __STACKSEAL_SIZE;
  	} >DTCM :dtcm
#endif

	/* Throw away C++ exception handling information */

	/*

	/DISCARD/ : {
		*(.note .note.*)
		*(.eh_frame .eh_frame.*)
		*(.ARM.extab* .gnu.linkonce.armextab.*)
		*(.ARM.exidx*)
	}

	*/


	/* Stabs debugging sections.  */
	.stab          0 : { *(.stab) }
	.stabstr       0 : { *(.stabstr) }
	.stab.excl     0 : { *(.stab.excl) }
	.stab.exclstr  0 : { *(.stab.exclstr) }
	.stab.index    0 : { *(.stab.index) }
	.stab.indexstr 0 : { *(.stab.indexstr) }
	.comment       0 : { *(.comment) }
	.gnu.build.attributes : { *(.gnu.build.attributes .gnu.build.attributes.*) }
	/* DWARF debug sections.
	   Symbols in the DWARF debugging sections are relative to the beginning
	   of the section so we begin them at 0.  */
	/* DWARF 1.  */
	.debug          0 : { *(.debug) }
	.line           0 : { *(.line) }
	/* GNU DWARF 1 extensions.  */
	.debug_srcinfo  0 : { *(.debug_srcinfo) }
	.debug_sfnames  0 : { *(.debug_sfnames) }
	/* DWARF 1.1 and DWARF 2.  */
	.debug_aranges  0 : { *(.debug_aranges) }
	.debug_pubnames 0 : { *(.debug_pubnames) }
	/* DWARF 2.  */
	.debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
	.debug_abbrev   0 : { *(.debug_abbrev) }
	.debug_line     0 : { *(.debug_line .debug_line.* .debug_line_end) }
	.debug_frame    0 : { *(.debug_frame) }
	.debug_str      0 : { *(.debug_str) }
	.debug_loc      0 : { *(.debug_loc) }
	.debug_macinfo  0 : { *(.debug_macinfo) }
	/* SGI/MIPS DWARF 2 extensions.  */
	.debug_weaknames 0 : { *(.debug_weaknames) }
	.debug_funcnames 0 : { *(.debug_funcnames) }
	.debug_typenames 0 : { *(.debug_typenames) }
	.debug_varnames  0 : { *(.debug_varnames) }
	/* DWARF 3.  */
	.debug_pubtypes 0 : { *(.debug_pubtypes) }
	.debug_ranges   0 : { *(.debug_ranges) }
	/* DWARF 5.  */
	.debug_addr     0 : { *(.debug_addr) }
	.debug_line_str 0 : { *(.debug_line_str) }
	.debug_loclists 0 : { *(.debug_loclists) }
	.debug_macro    0 : { *(.debug_macro) }
	.debug_names    0 : { *(.debug_names) }
	.debug_rnglists 0 : { *(.debug_rnglists) }
	.debug_str_offsets 0 : { *(.debug_str_offsets) }
	.debug_sup      0 : { *(.debug_sup) }
	.gnu.attributes 0 : { KEEP (*(.gnu.attributes)) }
}
/*
 * Check that sections that are copied from flash to RAM have matching
 * padding, so that a single memcpy() of __data_size copies the correct bytes.
 */
ASSERT( __data_size == __data_source_size,
	"ERROR: .data/.tdata flash size does not match RAM size");
