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
#include <pb/pb.h>

/* The least of the entry's part that the hash covers. ARMv7-M: the jump reads
 * the initial stack pointer and the reset vector from it, the vector table's
 * first two words (D-028). ARMv7-A and ARMv8-A: the instruction at the entry
 * is half of that. One rule for all: a part with its padding is empty or 512
 * bytes and more */
#define ENTRY_MIN_SIZE 8

static const struct boot_driver_simple_config *cfg;
/* The second argument points here: pb's memory, until the image copied it */
static uuid_t boot_uu;

int boot_driver_simple_init(const struct boot_driver_simple_config *cfg_in)
{
    if (cfg_in == NULL || cfg_in->boot_partition_id == 0)
        return -PB_ERR_PARAM;

    cfg = cfg_in;
    return PB_OK;
}

int boot_driver_simple_prepare(struct bpak_header *hdr,
                               const struct boot_info *info,
                               struct boot_handover_entry *next)
{
    struct bpak_part_header *part;
    struct bpak_meta_header *mh;

    if (cfg == NULL)
        return -PB_ERR_STATE;

    if (bpak_get_part(hdr, cfg->boot_partition_id, &part) != BPAK_OK) {
        LOG_ERR("Boot failed to find jump address");
        return -PB_ERR_BAD_META;
    }

    /* What the jump reads and runs there is in bytes the payload's hash
     * covers (security.md 4.1). bpak_part_size() is what was hashed at the
     * load address, the padding too; an empty part passes the hash with
     * nothing there (F-292) */
    if (bpak_part_size(part) < ENTRY_MIN_SIZE) {
        LOG_ERR("The entry's part %x is empty", part->id);
        return -PB_ERR_BAD_META;
    }

    /* The address the part was hashed at, loaded or in place */
    if (bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, part->id, &mh) != BPAK_OK) {
        LOG_ERR("No pb-load-addr for part %x", part->id);
        return -PB_ERR_BAD_META;
    }

    next->entry = (uintptr_t)*bpak_get_meta_ptr(hdr, mh, uint64_t);

    uuid_copy(boot_uu, info->boot_partition_uuid);

    /* The booted system's name, the policy's (NULL: it has none), and the
     * boot partition's UUID: the two of pb's information this handover has
     * room for, until Firmware Handoff replaces it (F-259) */
    next->arg[0] = (uintptr_t)info->system_name;
    next->arg[1] = (uintptr_t)boot_uu;

    LOG_INFO("Boot entry: 0x%" PRIxPTR, next->entry);

    return PB_OK;
}

static int handover_init(const void *config)
{
    return boot_driver_simple_init(config);
}

const struct boot_handover_ops boot_handover_simple_ops = {
    .init = handover_init,
    .prepare = boot_driver_simple_prepare,
};
