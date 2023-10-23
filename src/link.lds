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
        . = ALIGN(4096);
        PROVIDE (_code_start = .);
        KEEP(*(.vectors .vectors.*))
        *(.text .text.*)
        . = ALIGN(4096);
        _etext = .;
        PROVIDE (_code_end = .);
    } > ram

    .data :
    {
        . = ALIGN(4096);
        PROVIDE (_data_region_start = .);
        *(.data .data.*)
        . = ALIGN(4096);
        _edata = .;
        PROVIDE (_data_region_end = .);
    } > ram

    .ro_data :
    {
        . = ALIGN(4096);
        PROVIDE (_ro_data_region_start = .);
        *(.rodata)
        *(.igot.plt)
        *(.got .got.*)
        *(.rodata .rodata.*)
        . = ALIGN(8);
        PROVIDE (_self_test_fn_start = .);
        *(.self_test_fn .self_test_fn.*)
        PROVIDE (_self_test_fn_end = .);
        . = ALIGN(4096);
        _edata = .;
        *(.keystore_header)
        *(.keystore_key)
        . = ALIGN(4096);
        PROVIDE (_ro_data_region_end = .);
    } > ram

     .stack (NOLOAD):
    {
        . = ALIGN(4096);
        PROVIDE(_stack_start = .);
        . = . + PB_STACK_SIZE_KiB*1024 ;
        . = ALIGN(4096);
        PROVIDE(_stack_end = .);
    } > ram

    .bss (NOLOAD):
    {
        . = ALIGN(4096);
        PROVIDE (_zero_region_start = .);
        *(.bss)
        *(.bss .bss.*)
        . = ALIGN(4096);
        PROVIDE (_zero_region_end = .);
    } > ram

    .no_init (NOLOAD):
    {
        . = ALIGN(4096);
        PROVIDE(_no_init_start = .);
        *(.no_init .no_init*)
        . = ALIGN(4096);
        *(xlat_table)
        . = ALIGN(4096);
        PROVIDE(_no_init_end = .);
    } > buffers

    PROVIDE(end = .);
}
