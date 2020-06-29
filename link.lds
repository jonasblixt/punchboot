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
        *(.vectors .vectors.*)
        *(.text .text.*)
        . = ALIGN(4096);
        _etext = .;
        PROVIDE (_code_end = .);
    } > pbram

    .data :
    {
        . = ALIGN(4096);
        PROVIDE (_data_region_start = .);
        *(.data .data.*)
        . = ALIGN(4096);
        _edata = .;
        PROVIDE (_data_region_end = .);
    } > pbram

    .ro_data :
    {
        . = ALIGN(4096);
        PROVIDE (_ro_data_region_start = .);
        *(.rodata)
        *(.igot.plt)
        *(.got .got.*)
        *(.rodata .rodata.*)
        . = ALIGN(4096);
        _edata = .;
        PROVIDE (_ro_data_region_end = .);
    } > pbram

    .bss (NOLOAD):
    {
        . = ALIGN(4096);
        PROVIDE (_zero_region_start = .);
        *(.bss)
        *(.bss .bss.*)
        . = ALIGN(4096);
        PROVIDE (_zero_region_end = .);
    } > pbram

     .stack (NOLOAD):
    {
        . = ALIGN(4096);
        PROVIDE(_stack_start = .);
        . = . + PB_STACK_SIZE_KB*1024 ;
        . = ALIGN(4096);
        PROVIDE(_stack_end = .);
    } > pbram

    .bigbuffer (NOLOAD):
    {
        . = ALIGN(4096);
        PROVIDE(_big_buffer_start = .);
        *(.bigbuffer .bigbuffer*)
        . = ALIGN(4096);
        *(xlat_table)
        . = ALIGN(4096);
        PROVIDE(_big_buffer_end = .);
    } > dram

    PROVIDE(end = .);
}
