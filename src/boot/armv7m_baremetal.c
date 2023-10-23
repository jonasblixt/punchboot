/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Mårten Svanfeldt <marten.svanfeldt@actia.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <arch/arch.h>
#include <pb/arch.h>
#include <boot/armv7m_baremetal.h>

static const struct boot_driver_armv7m_baremetal_config *cfg;
static uintptr_t jump_part_offset = 0;

int boot_driver_armv7m_baremetal_init(const struct boot_driver_armv7m_baremetal_config *cfg_in)
{
    cfg = cfg_in;

    if (cfg->image_base_address == 0 ||
        cfg->image_size < sizeof(struct bpak_header) ||
        cfg->boot_partition_id == 0)
        return -PB_ERR_PARAM;

    return PB_OK;
}

int boot_driver_armv7m_baremetal_prepare(struct bpak_header *hdr, uuid_t boot_part_uu)
{
    (void)boot_part_uu;

    bool found_jump = false;

    bpak_foreach_part(hdr, p) {
        if (!p->id)
            break;

        if (p->id == cfg->boot_partition_id) {
            jump_part_offset = bpak_part_offset(hdr, p) - sizeof(struct bpak_header);
            found_jump = true;
            break;
        }
    }

    if (!found_jump) {
        LOG_ERR("Boot failed to find jump address");
        return -PB_ERR_BAD_META;
    }

    LOG_INFO("Boot entry: 0x%" PRIxPTR, cfg->image_base_address + jump_part_offset);

    return PB_OK;
}

void boot_driver_armv7m_baremetal_jump(void)
{
    uintptr_t jump_addr = cfg->image_base_address + jump_part_offset;

#ifdef CONFIG_PRINT_TIMESTAMPS
    ts_print();
#endif
    // arch_clean_cache_range((uintptr_t) &jump_addr, sizeof(jump_addr));
    arch_disable_mmu();

    LOG_DBG("Jumping to %" PRIxPTR, jump_addr);
    arch_jump((void *) jump_addr, NULL, NULL, NULL, NULL);

    LOG_ERR("Jump returned %" PRIxPTR, jump_addr);
}
