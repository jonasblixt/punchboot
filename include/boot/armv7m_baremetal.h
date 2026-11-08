/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Mårten Svanfeldt <marten.svanfeldt@actia.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BOOT_ARMV7M_BAREMETAL_H
#define INCLUDE_BOOT_ARMV7M_BAREMETAL_H

#include <bpak/bpak.h>
#include <uuid.h>

/**
 * A bare metal image on an ARMv7-M core. The part 'boot_partition_id' starts
 * with the image's vector table. Two ways to run it:
 *
 * - In place (XIP), 'image_base_address' != 0: the boot partition is
 *   mapped at 'image_base_address' (e.g. FlexSPI NOR), the vector table is
 *   at the part's offset in the payload. Boot it with BOOT_SOURCE_IN_MEM.
 * - From RAM, 'image_base_address' == 0: pb loads the parts to their
 *   pb-load-addr (BOOT_SOURCE_BIO) and the vector table is at the load
 *   address of 'boot_partition_id'.
 *
 * At the image's reset handler: VTOR is its vector table, MSP and PSP its
 * initial stack pointer, r0 the name of the booted system ("A", "B", from
 * 'resolve_part_name', or NULL) and r1 a pointer to the 16 byte UUID of the
 * boot partition. Both point into pb's memory: copy them before using it.
 */
struct boot_driver_armv7m_baremetal_config {
    uintptr_t image_base_address; /*!< XIP address of the payload, or 0 */
    size_t image_size; /*!< Size of the XIP partition */
    bpak_id_t boot_partition_id; /*!< Part that starts with the vector table */
    const char *(*resolve_part_name)(uuid_t part_uu); /*!< Optional: r0 */
};

int boot_driver_armv7m_baremetal_init(const struct boot_driver_armv7m_baremetal_config *cfg);
int boot_driver_armv7m_baremetal_prepare(struct bpak_header *hdr, uuid_t boot_part_uu);
void boot_driver_armv7m_baremetal_jump(void);

#endif /* INCLUDE_BOOT_ARMV7M_BAREMETAL_H */
