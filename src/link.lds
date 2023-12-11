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
    . = PB_ENTRY;

    .text :
     {
        . = ALIGN(PB_SECTION_ALIGNMENT);
        PROVIDE (_code_start = .);
        KEEP(*(.vectors .vectors.*))
        *(.text .text.*)
        . = ALIGN(PB_SECTION_ALIGNMENT);
        _etext = .;
        PROVIDE (_code_end = .);
    } > ram

    .data :
    {
        . = ALIGN(PB_SECTION_ALIGNMENT);
        PROVIDE (_data_region_start = .);
        *(.data .data.*)
        . = ALIGN(PB_SECTION_ALIGNMENT);
        _edata = .;
        PROVIDE (_data_region_end = .);
    } > ram

    .ro_data :
    {
        . = ALIGN(PB_SECTION_ALIGNMENT);
        PROVIDE (_ro_data_region_start = .);
        *(.rodata)
        *(.igot.plt)
        *(.got .got.*)
        *(.rodata .rodata.*)
        . = ALIGN(8);
        PROVIDE (_self_test_fn_start = .);
        *(.self_test_fn .self_test_fn.*)
        PROVIDE (_self_test_fn_end = .);
        . = ALIGN(PB_SECTION_ALIGNMENT);
        _edata = .;
        *(.keystore_header)
        *(.keystore_key)
        . = ALIGN(PB_SECTION_ALIGNMENT);
        PROVIDE (_ro_data_region_end = .);
    } > ram

     .stack (NOLOAD):
    {
        . = ALIGN(PB_SECTION_ALIGNMENT);
        PROVIDE(_stack_start = .);
        . = . + PB_STACK_SIZE_KiB*1024 ;
        . = ALIGN(PB_SECTION_ALIGNMENT);
        PROVIDE(_stack_end = .);
    } > ram

    .bss (NOLOAD):
    {
        . = ALIGN(PB_SECTION_ALIGNMENT);
        PROVIDE (_zero_region_start = .);
        *(.bss)
        *(.bss .bss.*)
        . = ALIGN(PB_SECTION_ALIGNMENT);
        PROVIDE (_zero_region_end = .);
    } > ram

    .no_init (NOLOAD):
    {
        . = ALIGN(PB_SECTION_ALIGNMENT);
        PROVIDE(_no_init_start = .);
        *(.no_init .no_init*)
        . = ALIGN(PB_SECTION_ALIGNMENT);
        *(xlat_table)
        . = ALIGN(PB_SECTION_ALIGNMENT);
        PROVIDE(_no_init_end = .);
    } > buffers

    PROVIDE(end = .);
}
