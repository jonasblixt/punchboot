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

#include <boot/handover.h>
#include <bpak/bpak.h>
#include <pb/utils_def.h>
#include <uuid.h>

/**
 * pb's simple boot protocol: no OS handover data (Linux's DTB, TF-A's
 * bl_params), only the entry, the booted system's name and the boot
 * partition's UUID. For any image that takes that: an application, an RTOS
 * (Zephyr, FreeRTOS), on any arch pb runs on. The part 'boot_partition_id'
 * starts with the image's entry: its vector table on ARMv7-M, its first
 * instruction on ARMv7-A and ARMv8-A.
 *
 * The entry is the load address (pb-load-addr) of 'boot_partition_id',
 * wherever the image comes from: it is the address pb hashed the part at.
 *
 * - From RAM: pb loads the parts to their load addresses.
 * - In place (XIP): the boot partition has STATIC_PART_FLAG_IN_PLACE in the
 *   board's static partition table, set up with static_ptbl_init_mapped()
 *   (e.g. FlexSPI NOR). Nothing is loaded, and a part's load address is its
 *   address in the map: the address the image is linked for.
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
    bpak_id_t boot_partition_id; /*!< Part that starts with the vector table */
    const char *(*resolve_part_name)(uuid_t part_uu); /*!< Optional: r0 */
};

int boot_driver_simple_init(const struct boot_driver_simple_config *cfg);
int boot_driver_simple_prepare(struct bpak_header *hdr,
                               uuid_t boot_part_uu,
                               struct boot_handover_entry *next);

extern const struct boot_handover_ops boot_handover_simple_ops;

/**
 * A board's handover (board.boot_handover): the generic handover, until Firmware Handoff (F-259),
 * with
 * '_config', a const struct boot_driver_simple_config *.
 */
#define BOOT_HANDOVER_SIMPLE(_config)                                         \
    {                                                                         \
        .ops = &boot_handover_simple_ops,                                     \
        .config = PB_CONFIG(_config, const struct boot_driver_simple_config), \
    }

#endif /* INCLUDE_BOOT_SIMPLE_H */
