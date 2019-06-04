
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb.h>
#include <io.h>
#include <boot.h>
#include <gpt.h>
#include <image.h>
#include <board/config.h>
#include <crypto.h>
#include <board.h>
#include <plat.h>
#include <atf.h>
#include <timing_report.h>
#include <libfdt.h>
#include <uuid.h>

extern void arch_jump(void* addr, void * p0, void *p1, void *p2)
                                 __attribute__ ((noreturn));



void pb_boot(struct pb_pbi *pbi, uint32_t system_index, bool verbose)
{

    UNUSED(system_index);
    UNUSED(verbose);

    struct pb_component_hdr *kernel = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_KERNEL);

    struct pb_component_hdr *atf = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_ATF);
    
    plat_preboot_cleanup();
    tr_stamp_end(TR_TOTAL);
    tr_stamp_end(TR_DT_PATCH);

    tr_print_result();

    plat_wdog_kick();

    arch_jump((void *)(uintptr_t) atf->load_addr_low, NULL, NULL, NULL);


    while(1)
        __asm__ ("nop");
}

