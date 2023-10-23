/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Mårten Svanfeldt <marten.svanfeldt@actia.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <bpak/bpak.h>
#include <uuid.h>

struct boot_driver_armv7m_baremetal_config
{
    uintptr_t image_base_address;
    size_t image_size;
    bpak_id_t boot_partition_id;
};

int boot_driver_armv7m_baremetal_init(const struct boot_driver_armv7m_baremetal_config *cfg);
int boot_driver_armv7m_baremetal_prepare(struct bpak_header *hdr, uuid_t boot_part_uu);
void boot_driver_armv7m_baremetal_jump(void);
