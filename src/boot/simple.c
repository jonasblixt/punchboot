/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Mårten Svanfeldt <marten.svanfeldt@actia.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <boot/simple.h>
#include <bpak/id.h>
#include <inttypes.h>
#include <pb/arch.h>
#include <pb/pb.h>

static const struct boot_driver_simple_config *cfg;
static uintptr_t jump_addr;
static uuid_t boot_uu;
static const char *boot_name;

int boot_driver_simple_init(const struct boot_driver_simple_config *cfg_in)
{
    if (cfg_in == NULL || cfg_in->boot_partition_id == 0)
        return -PB_ERR_PARAM;

    if (cfg_in->image_base_address != 0 && cfg_in->image_size < sizeof(struct bpak_header))
        return -PB_ERR_PARAM;

    cfg = cfg_in;
    return PB_OK;
}

int boot_driver_simple_prepare(struct bpak_header *hdr, uuid_t boot_part_uu)
{
    struct bpak_part_header *part;
    struct bpak_meta_header *mh;

    if (cfg == NULL)
        return -PB_ERR_STATE;

    if (bpak_get_part(hdr, cfg->boot_partition_id, &part) != BPAK_OK) {
        LOG_ERR("Boot failed to find jump address");
        return -PB_ERR_BAD_META;
    }

    if (cfg->image_base_address != 0) {
        /* In place: the part's offset in the payload */
        jump_addr =
            cfg->image_base_address + bpak_part_offset(hdr, part) - sizeof(struct bpak_header);
    } else {
        /* Loaded to RAM: the part's load address */
        if (bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, part->id, &mh) != BPAK_OK) {
            LOG_ERR("No pb-load-addr for part %x", part->id);
            return -PB_ERR_BAD_META;
        }

        jump_addr = (uintptr_t)*bpak_get_meta_ptr(hdr, mh, uint64_t);
    }

    uuid_copy(boot_uu, boot_part_uu);
    boot_name = cfg->resolve_part_name ? cfg->resolve_part_name(boot_part_uu) : NULL;

    LOG_INFO("Boot entry: 0x%" PRIxPTR, jump_addr);

    return PB_OK;
}

void boot_driver_simple_jump(void)
{
    LOG_DBG("Jumping to %" PRIxPTR, jump_addr);

    /* Cleans the D-cache, arch_jump() invalidates the I-cache */
    arch_disable_mmu();
    arch_jump((void *)jump_addr, (void *)boot_name, (void *)boot_uu, NULL, NULL);
}
