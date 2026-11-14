/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The delivery through OP-TEE (boot.md 5.7), ARMv7-A: pb enters OP-TEE, its
 * own secure monitor, with the handover's entry as the normal world's. An
 * image without OP-TEE's part is jumped to directly.
 */

#include <arch/arch.h>
#include <boot/dtb.h>
#include <boot/image.h>
#include <boot/image_helpers.h>
#include <boot/optee.h>
#include <bpak/bpak.h>
#include <inttypes.h>
#include <libfdt.h>
#include <pb/arch.h>
#include <pb/pb.h>

/* The least of OP-TEE's part that the hash covers: the instruction at its
 * entry */
#define OPTEE_ENTRY_MIN_SIZE 4

static const struct boot_delivery_optee_config *cfg;

/* What prepare found in the image, for the jump. 0: no OP-TEE part, the
 * image is jumped to directly */
static uintptr_t optee_addr;

int boot_delivery_optee_init(const struct boot_delivery_optee_config *cfg_in)
{
    if (cfg_in == NULL || cfg_in->bl32_bpak_id == 0)
        return -PB_ERR_PARAM;

    cfg = cfg_in;
    return PB_OK;
}

static size_t dtb_max_size(void)
{
    return cfg->dtb_max_size ? cfg->dtb_max_size : BOOT_OPTEE_DTB_MAX_SIZE_DEFAULT;
}

/* The part loaded at 'addr', the device tree's */
static bpak_id_t part_at(struct bpak_header *hdr, uintptr_t addr)
{
    uint64_t load_addr;

    bpak_foreach_part(hdr, p) {
        if (!p->id)
            break;

        if (boot_image_load_addr(hdr, p->id, &load_addr) == PB_OK && (uintptr_t)load_addr == addr)
            return p->id;
    }

    return 0;
}

/*
 * OP-TEE grows the device tree in place to its CFG_DTB_MAX_SIZE
 * (fdt_open_into): the room from the device tree's load address is checked
 * as the handover's own headroom is (boot.md 4.3), and the tree, already
 * grown by the handover, must fit it, or OP-TEE stops.
 */
static int check_dtb(struct bpak_header *hdr, uintptr_t dt)
{
    size_t room = dtb_max_size();
    size_t size = fdt_totalsize((const void *)dt);
    int rc;

    if (size > room) {
        LOG_ERR(
            "The device tree (%zu bytes) is larger than OP-TEE's room for it (%zu)", size, room);
        return -PB_ERR_MEM;
    }

    rc = boot_dtb_check_room(hdr, part_at(hdr, dt), dt, room);

    if (rc != PB_OK)
        LOG_ERR("No room for OP-TEE's device tree (%zu bytes)", room);

    return rc;
}

int boot_delivery_optee_prepare(struct bpak_header *hdr, const struct boot_handover_entry *next)
{
    struct bpak_part_header *ph;
    uint64_t load_addr;
    int rc;

    if (cfg == NULL)
        return -PB_ERR_STATE;

    optee_addr = 0;

    /* Without OP-TEE's part the image is the handover's alone: a released
     * image with OP-TEE in the kernel's part (CFG_NS_ENTRY_ADDR) keeps
     * booting when the board turns this delivery on (D-008) */
    if (bpak_get_part(hdr, cfg->bl32_bpak_id, &ph) != BPAK_OK) {
        LOG_INFO("No OP-TEE part: the image is started directly");
        return PB_OK;
    }

    rc = boot_image_load_addr(hdr, cfg->bl32_bpak_id, &load_addr);

    if (rc != PB_OK) {
        LOG_ERR("Could not read OP-TEE's load address (%i)", rc);
        return -PB_ERR_BAD_META;
    }

    /* pb jumps there: its entry is in bytes the payload's hash covers
     * (security.md 4.1); an empty part passes the hash with nothing there */
    if (bpak_part_size(ph) < OPTEE_ENTRY_MIN_SIZE) {
        LOG_ERR("OP-TEE's part has no entry: it is empty");
        return -PB_ERR_BAD_META;
    }

    /* The normal world's entry is the handover's, checked there */
    if (next->dt) {
        rc = check_dtb(hdr, next->dt);

        if (rc != PB_OK)
            return rc;
    }

    optee_addr = (uintptr_t)load_addr;
    LOG_INFO("OP-TEE: 0x%" PRIxPTR ", the normal world at 0x%" PRIxPTR, optee_addr, next->entry);
    return PB_OK;
}

void boot_delivery_optee_jump(const struct boot_handover_entry *next)
{
    /* The entry and the arguments are read with the cache off */
    arch_clean_cache_range((uintptr_t)next, sizeof(*next));
    arch_disable_mmu();

    if (optee_addr == 0) {
        LOG_DBG("Jumping to %" PRIxPTR, next->entry);
        arch_jump((void *)next->entry,
                  (void *)next->arg[0],
                  (void *)next->arg[1],
                  (void *)next->arg[2],
                  (void *)next->arg[3]);
        LOG_ERR("Jump returned %" PRIxPTR, next->entry);
        return;
    }

    LOG_DBG("Entering OP-TEE at %" PRIxPTR, optee_addr);
    boot_optee_enter(optee_addr, next->entry, next->arg[1], next->arg[2]);
    LOG_ERR("OP-TEE returned %" PRIxPTR, optee_addr);
}

static int delivery_init(const void *config)
{
    return boot_delivery_optee_init(config);
}

/* OP-TEE takes no normal world entry with a transfer list on AArch32
 * (boot.md 5.7): the image handover only */
static bool delivery_takes(const struct boot_handover_ops *handover)
{
    return handover == &boot_handover_image_ops;
}

const struct boot_delivery_ops boot_delivery_optee_ops = {
    .init = delivery_init,
    .takes = delivery_takes,
    .prepare = boot_delivery_optee_prepare,
    .jump = boot_delivery_optee_jump,
};
