/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

SECTIONS
{
    .  = PB_ENTRY ;

    .text :
    {
        PROVIDE (_code_start = .);
        KEEP(*(.vectors .vectors.*))
        KEEP(*(.vector_handlers .vector_handlers.*))
        . = ALIGN(16);
        *(.text .text.*)
        *(.init_array .init_array.*)
        . = ALIGN(16);
        _etext = .;
        PROVIDE (_code_end = .);
    } > flash

    .data :
    {
        _sdata = .;
        . = ALIGN(16);
        PROVIDE (_data_region_start = .);
        *(.data .data.*)
        . = ALIGN(16);
        _edata = .;
        PROVIDE (_data_region_end = .);
    } > ram AT > flash

    .ARM.extab :
    {
        *(.ARM.extab .ARM.extab*)
    } > flash

    .ARM.exidx :
    {
        __exidx_start = ABSOLUTE(.);
        *(.ARM.exidx .ARM.exidx*)
        __exidx_end = ABSOLUTE(.);
    } > flash

    .ro_data :
    {
        . = ALIGN(16);
        PROVIDE (_ro_data_region_start = .);
        *(.rodata)
        *(.igot.plt)
        *(.got .got.*)
        *(.rodata .rodata.*)
        *(.fixup)
        *(.gnu.warning)
        *(.gnu.linkonce.t.*)
        *(.glue_7)
        *(.glue_7t)
        *(.got)
        *(.gcc_except_table)
        *(.gnu.linkonce.r.*)
        . = ALIGN(16);
        PROVIDE (_self_test_fn_start = .);
        *(.self_test_fn .self_test_fn.*)
        PROVIDE (_self_test_fn_end = .);
        . = ALIGN(1024);
        _erodata = .;
        *(.keystore_header)
        *(.keystore_key)
        . = ALIGN(1024);
        PROVIDE (_ro_data_region_end = .);
    } > flash

     .stack (NOLOAD):
    {
        . = ALIGN(16);
        PROVIDE(_stack_start = .);
        . = . + PB_STACK_SIZE_KiB*1024 ;
        . = ALIGN(16);
        PROVIDE(_stack_end = .);
    } > ram

    .bss (NOLOAD):
    {
        . = ALIGN(16);
        PROVIDE (_zero_region_start = .);
        *(.bss)
        *(.bss .bss.*)
        . = ALIGN(16);
        PROVIDE (_zero_region_end = .);
    } > ram

    .no_init (NOLOAD):
    {
        . = ALIGN(16);
        PROVIDE(_no_init_start = .);
        *(.no_init .no_init*)
        . = ALIGN(16);
        *(xlat_table)
        . = ALIGN(16);
        PROVIDE(_no_init_end = .);
    } > buffers

    PROVIDE(end = .);

    /* Stabs debugging sections. */

    .stab 0 : { *(.stab) }
    .stabstr 0 : { *(.stabstr) }
    .stab.excl 0 : { *(.stab.excl) }
    .stab.exclstr 0 : { *(.stab.exclstr) }
    .stab.index 0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }
    .comment 0 : { *(.comment) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_info 0 : { *(.debug_info) }
    .debug_line 0 : { *(.debug_line) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_aranges 0 : { *(.debug_aranges) }
}
