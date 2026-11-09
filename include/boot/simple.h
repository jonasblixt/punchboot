/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Mårten Svanfeldt <marten.svanfeldt@actia.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BOOT_SIMPLE_H
#define INCLUDE_BOOT_SIMPLE_H

#include <bpak/bpak.h>
#include <uuid.h>

/**
 * pb's simple boot protocol: no OS handover data (Linux's DTB, TF-A's
 * bl_params), only the entry, the booted system's name and the boot
 * partition's UUID. For any image that takes that: an application, an RTOS
 * (Zephyr, FreeRTOS), on any arch pb runs on. The part 'boot_partition_id'
 * starts with the image's entry: its vector table on ARMv7-M, its first
 * instruction on ARMv7-A and ARMv8-A. Two ways to run it:
 *
 * - In place (XIP), 'image_base_address' != 0: the boot partition is
 *   mapped at 'image_base_address' (e.g. FlexSPI NOR), the entry is at the
 *   part's offset in the payload. Boot it with BOOT_SOURCE_IN_MEM.
 * - From RAM, 'image_base_address' == 0: pb loads the parts to their
 *   pb-load-addr (BOOT_SOURCE_BIO) and the entry is at the load address of
 *   'boot_partition_id'.
 *
 * The image starts as every next stage does (include/pb/arch.h): the MMU
 * (MPU) and the D-cache off, the I-cache invalidated, interrupts masked, at
 * the exception level pb runs at (EL3 on the i.MX8M and i.MX8X). On ARMv7-M
 * VTOR is its vector table and MSP and PSP its initial stack pointer. The
 * first argument (r0, x0) is the name of the booted system ("A", "B", from
 * 'resolve_part_name', or NULL), the second (r1, x1) a pointer to the 16
 * byte UUID of the boot partition. Both point into pb's memory: copy them
 * before using it.
 */
struct boot_driver_simple_config {
    uintptr_t image_base_address; /*!< XIP address of the payload, or 0 */
    size_t image_size; /*!< Size of the XIP partition */
    bpak_id_t boot_partition_id; /*!< Part that starts with the vector table */
    const char *(*resolve_part_name)(uuid_t part_uu); /*!< Optional: r0 */
};

int boot_driver_simple_init(const struct boot_driver_simple_config *cfg);
int boot_driver_simple_prepare(struct bpak_header *hdr, uuid_t boot_part_uu);
void boot_driver_simple_jump(void);

#endif /* INCLUDE_BOOT_SIMPLE_H */
