/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M's SPL (D-088, platform.md 5.1): in the ROM's load window, from
 * IMX8M_SPL_ENTRY to the RAM log, which pb proper finds at the same address;
 * the DDR controller's state for TF-A at the start of DRAM. The values are
 * the platform's makefile.mk's.
 */

ENTRY(SPL_ENTRY_SYMBOL)

/* What the SPL doesn't load, .bss and its stack, is at the top of its
 * window, below the RAM log. The ROM loads more than the SPL's image after
 * it: on the 8MQ the plugin's CSF and pb proper's IVT (imx-mkimage's
 * layout), which the SPL reads (spl_imx8mq.c). IMX8M_SPL_AFTER_IMAGE is
 * that room, kept between the image's end and the stack */
IMX8M_SPL_RW_SIZE = 0x4000;
IMX8M_SPL_AFTER_IMAGE = 0x2800;

MEMORY
{
    spl (rwx)  : ORIGIN = IMX8M_SPL_ENTRY,
                 LENGTH = IMX8M_SPL_END - 0x4000 - 0x2800 - IMX8M_SPL_ENTRY
    spl_rw (rw) : ORIGIN = IMX8M_SPL_END - 0x4000, LENGTH = 0x4000
    ramlog (rw) : ORIGIN = IMX8M_RAMLOG_BASE, LENGTH = IMX8M_RAMLOG_SIZE
    umctl (rw) : ORIGIN = 0x40000000, LENGTH = 0x20000
    buffers (rw) : ORIGIN = PB_BUFFERS_BASE, LENGTH = 0x10000
}

/* The SPL's own header, which the ROM read before the entry (imx-mkimage's
 * imx_header_v2_t, 0x40 bytes) */
spl_plugin_header = IMX8M_SPL_ENTRY - 0x40;

SECTIONS
{
    .text :
    {
        KEEP(*(.spl_entry))
        *(.text .text.*)
    } > spl

    .rodata : ALIGN(8)
    {
        *(.rodata .rodata.*)
    } > spl

    /* What ld makes for ifuncs and run time relocations: empty, as pb's
     * (src/link.lds), checked below */
    .iplt : { *(.iplt .igot.plt) } > spl
    .rela.dyn : { *(.rela.dyn .rela.iplt .rela.got .rela.data .rela.data.* .rela.spl_entry) } > spl

    .data : ALIGN(16)
    {
        *(.data .data.*)
        *(.got .got.*)
        /* The image the ROM loads ends here, a multiple of 8 bytes */
        . = ALIGN(16);
    } > spl

    .bss (NOLOAD) : ALIGN(16)
    {
        _spl_bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        _spl_bss_end = .;
    } > spl_rw

    .stack (NOLOAD) : ALIGN(16)
    {
        . += IMX8M_SPL_STACK_SIZE;
        _spl_stack_end = .;
    } > spl_rw

    .ramlog (NOLOAD) :
    {
        *(.ramlog)
    } > ramlog

    .umctl (NOLOAD) :
    {
        *(.umctl2 .umctl2.*)
    } > umctl

    /* What a device reads and writes of the SPL's own (PB_DMA_BUFFER(),
     * drivers.md 4), the USB controller's: in DRAM, at the start of pb's
     * buffers, where pb proper expects nothing. Not in the SPL's window:
     * nothing says the controller reaches that RAM. Used after DRAM is up,
     * and never initialised */
    .no_init (NOLOAD) :
    {
        *(.no_init .no_init.*)
    } > buffers

    .debug_aranges 0 : { *(.debug_aranges) }
    .debug_info 0 : { *(.debug_info) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_line 0 : { *(.debug_line .debug_line.*) }
    .debug_frame 0 : { *(.debug_frame) }
    .debug_str 0 : { *(.debug_str) }
    .debug_loc 0 : { *(.debug_loc) }
    .debug_ranges 0 : { *(.debug_ranges) }
    .debug_line_str 0 : { *(.debug_line_str) }
    .debug_loclists 0 : { *(.debug_loclists) }
    .debug_rnglists 0 : { *(.debug_rnglists) }

    /DISCARD/ :
    {
        *(.comment) *(.note .note.*) *(.eh_frame .eh_frame_hdr)
    }
}

ASSERT(IMX8M_SPL_RW_SIZE == LENGTH(spl_rw) && ORIGIN(spl_rw) - (ORIGIN(spl) + LENGTH(spl)) == IMX8M_SPL_AFTER_IMAGE,
       "The SPL's link script's sizes disagree")

ASSERT(SIZEOF(.iplt) == 0 && SIZEOF(.rela.dyn) == 0,
       "The SPL has an ifunc or a run time relocation, which nothing applies")
