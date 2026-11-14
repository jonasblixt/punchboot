/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonas@blxt.se>
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
        /* pb: from here to the end of the RAM log (platform.md 5.1) */
        PROVIDE (_pb_start = .);
        PROVIDE (_code_start = .);
        KEEP(*(.vectors .vectors.*))
        *(.text .text.*)
        /* ld's stubs: ARM and Thumb interworking, the VFP11 erratum. Code,
         * empty for the cores pb builds for */
        *(.glue_7 .glue_7t .vfp11_veneer .v4_bx)
        . = ALIGN(PB_SECTION_ALIGNMENT);
        _etext = .;
        PROVIDE (_code_end = .);
    } > ram

    /* The ARMv7-M's entry and exception handlers (arch/armv7m/entry.c), which
     * the vectors point to. A section of the script's, between _pb_start and
     * _pb_end: as an orphan it was where ld chose to put it */
    .vector_handlers :
    {
        *(.vector_handlers .vector_handlers.*)
    } > ram

    /* The unwind tables that libgcc's objects have on ARM, 8 bytes on the
     * RT1060. pb has no unwinder; they are where ld put them as orphans */
    .ARM.extab :
    {
        *(.ARM.extab .ARM.extab.* .gnu.linkonce.armextab.*)
    } > ram

    .ARM.exidx :
    {
        *(.ARM.exidx .ARM.exidx.* .gnu.linkonce.armexidx.*)
    } > ram

    /* What ld makes for ifuncs and run time relocations. pb is linked static,
     * has no ifunc and nothing that would apply a relocation: they are
     * empty, and the link fails when one isn't (below) */
    .iplt :
    {
        *(.iplt)
    } > ram

    .rel.dyn :
    {
        *(.rel.dyn .rel.iplt)
    } > ram

    .rela.dyn :
    {
        *(.rela.dyn .rela.iplt .rela.got)
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
        /* The tasks that DEFINE_TASK() defines (pb/task.h). No code names
         * them, so they are kept here (platform.md 5.1) */
        . = ALIGN(8);
        PROVIDE (__start_pb_tasks = .);
        KEEP(*(pb_tasks))
        PROVIDE (__stop_pb_tasks = .);
        /* The named commands that DEFINE_COMMAND() declares (pb/command.h),
         * kept for the same reason (platform.md 5.1) */
        . = ALIGN(8);
        PROVIDE (__start_pb_commands = .);
        KEEP(*(pb_commands))
        PROVIDE (__stop_pb_commands = .);
        . = ALIGN(8);
        . = ALIGN(PB_SECTION_ALIGNMENT);
        _edata = .;
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

    /* The RAM log: not zeroed, and in the RAM pb runs from, which is usable
     * from the start (the i.MX8M's buffers are in DRAM, trained later) */
    .ramlog (NOLOAD):
    {
        . = ALIGN(PB_SECTION_ALIGNMENT);
        *(.ramlog)
        . = ALIGN(PB_SECTION_ALIGNMENT);
        PROVIDE(_pb_end = .);
    } > ram

    /* buffers: the platform's region, from its origin to the end of what is
     * linked there (platform.md 5.1). pb's own start at PB_BUFFERS_BASE,
     * after what a platform links at the region's start (the i.MX8M) */
    PROVIDE(_buffers_start = ORIGIN(buffers));

    .no_init PB_BUFFERS_BASE (NOLOAD):
    {
        . = ALIGN(PB_SECTION_ALIGNMENT);
        PROVIDE(_no_init_start = .);
        *(.no_init .no_init*)
        . = ALIGN(PB_SECTION_ALIGNMENT);
        *(xlat_table)
        . = ALIGN(PB_SECTION_ALIGNMENT);
        PROVIDE(_no_init_end = .);
        PROVIDE(_buffers_end = .);
    } > buffers

    PROVIDE(end = .);

    /* What isn't loaded: the compiler's notes and the attributes, and the
     * debug information, as ld's own script has them. The note that the
     * stack isn't executable is for an OS's loader, which pb has none of */
    .comment 0 : { *(.comment) }
    .ARM.attributes 0 : { KEEP(*(.ARM.attributes)) KEEP(*(.gnu.attributes)) }
    .debug_aranges 0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_info 0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_line 0 : { *(.debug_line .debug_line.* .debug_line_end) }
    .debug_frame 0 : { *(.debug_frame) }
    .debug_str 0 : { *(.debug_str) }
    .debug_loc 0 : { *(.debug_loc) }
    .debug_macinfo 0 : { *(.debug_macinfo) }
    .debug_pubtypes 0 : { *(.debug_pubtypes) }
    .debug_ranges 0 : { *(.debug_ranges) }
    .debug_addr 0 : { *(.debug_addr) }
    .debug_line_str 0 : { *(.debug_line_str) }
    .debug_loclists 0 : { *(.debug_loclists) }
    .debug_macro 0 : { *(.debug_macro) }
    .debug_names 0 : { *(.debug_names) }
    .debug_rnglists 0 : { *(.debug_rnglists) }
    .debug_str_offsets 0 : { *(.debug_str_offsets) }
    /DISCARD/ : { *(.note.GNU-stack) }
}

ASSERT(SIZEOF(.iplt) == 0 && SIZEOF(.rel.dyn) == 0 && SIZEOF(.rela.dyn) == 0,
       "pb has an ifunc or a run time relocation (.iplt, .rel.dyn, .rela.dyn), which nothing in pb applies")

/* The stack grows down, towards .ro_data and the keystore at its end. Where
 * the arch has an MMU (its sections are page aligned) the platforms map
 * .ro_data read only, so once the MMU is on an overflow faults there and
 * rewrites nothing. That holds while the stack starts where .ro_data ends.
 * The ARMv7-M has no MMU and no such guard (F-209). */
ASSERT(PB_SECTION_ALIGNMENT < 4096 || ADDR(.stack) == ADDR(.ro_data) + SIZEOF(.ro_data),
       "The stack doesn't start where .ro_data ends: its overflow would not fault on read only memory")
