/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <arch/arch.h>
#include <boot/dtb.h>
#include <boot/image.h>
#include <bpak/bpak.h>
#include <bpak/id.h>
#include <inttypes.h>
#include <libfdt.h>
#include <pb/arch.h>
#include <pb/pb.h>
#include <pb/plat.h>

/* AArch32: no machine type, the device tree describes the machine */
#define LINUX_ARM_MACH_TYPE_DT 0xffffffff

static const struct boot_handover_image_config *cfg;
static uintptr_t jump_addr;
static uintptr_t ramdisk_addr;
static uintptr_t dtb_addr;

int boot_handover_image_init(const struct boot_handover_image_config *cfg_in)
{
    cfg = cfg_in;

    if (cfg == NULL || cfg->image_bpak_id == 0)
        return -PB_ERR_PARAM;

    return PB_OK;
}

int boot_handover_image_prepare(struct bpak_header *hdr, uuid_t boot_part_uu)
{
    int rc;
    void *fdt;
    size_t dtb_length;
    struct bpak_part_header *ph;
    struct bpak_meta_header *mh;

    rc = bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, cfg->image_bpak_id, &mh);

    if (rc != BPAK_OK) {
        LOG_ERR("Could not read boot image meta data (%i)", rc);
        return -PB_ERR_BAD_META;
    }

    jump_addr = (uintptr_t)*bpak_get_meta_ptr(hdr, mh, uint64_t);

    LOG_INFO("Boot entry: 0x%" PRIxPTR, jump_addr);

    if (cfg->ramdisk_bpak_id) {
        rc = bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, cfg->ramdisk_bpak_id, &mh);

        if (rc != BPAK_OK) {
            LOG_ERR("Could not read ramdisk load addr (%i)", rc);
            return -PB_ERR_BAD_META;
        }

        ramdisk_addr = (uintptr_t)*bpak_get_meta_ptr(hdr, mh, uint64_t);
        LOG_INFO("Ramdisk: %" PRIxPTR, ramdisk_addr);
    }

    if (cfg->dtb_bpak_id) {
        rc = bpak_get_part(hdr, cfg->dtb_bpak_id, &ph);

        if (rc != BPAK_OK) {
            LOG_ERR("Could not read dtb bpak part meta");
            return -PB_ERR_BAD_META;
        }

        rc = bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, cfg->dtb_bpak_id, &mh);

        if (rc != BPAK_OK) {
            LOG_ERR("Could not read dtb load addr meta");
            return -PB_ERR_BAD_META;
        }

        dtb_addr = (uintptr_t)*bpak_get_meta_ptr(hdr, mh, uint64_t);
        LOG_INFO("DTB: %" PRIxPTR, dtb_addr);

        /* Room for /chosen and pb's properties */
        rc = boot_dtb_grow(hdr, cfg->dtb_bpak_id, CONFIG_BOOT_DTB_HEADROOM, &fdt);

        if (rc != PB_OK)
            return rc;

        dtb_length = fdt_totalsize(fdt);

        struct boot_dtb_chosen chosen = {
            .patch_cb = cfg->dtb_patch_cb,
            .ramdisk_id = cfg->ramdisk_bpak_id,
            .ramdisk_addr = ramdisk_addr,
            .resolve_part_name = cfg->resolve_part_name,
        };

        rc = boot_dtb_set_chosen(fdt, hdr, boot_part_uu, &chosen);

        if (rc != PB_OK)
            return rc;
    }

    if (cfg->dtb_bpak_id) {
        arch_clean_cache_range(dtb_addr, dtb_length);
    }

    return PB_OK;
}

void boot_handover_image_jump(void)
{
    arch_clean_cache_range((uintptr_t)&jump_addr, sizeof(jump_addr));
    arch_disable_mmu();

    LOG_DBG("Jumping to %" PRIxPTR, jump_addr);
    if (cfg->set_dtb_boot_arg) {
#if defined(CONFIG_ARCH_ARMV7)
        /* AArch32 Linux boot protocol: r0 = 0, r1 = the machine type (DT only), r2 = DTB */
        arch_jump((void *)jump_addr, NULL, (void *)LINUX_ARM_MACH_TYPE_DT, (void *)dtb_addr, NULL);
#else
        /* AArch64 Linux boot protocol: x0 = DTB */
        arch_jump((void *)jump_addr, (void *)dtb_addr, NULL, NULL, NULL);
#endif
    } else
        arch_jump((void *)jump_addr, NULL, NULL, NULL, NULL);

    LOG_ERR("Jump returned %" PRIxPTR, jump_addr);
}

static int handover_init(const void *config)
{
    return boot_handover_image_init(config);
}

const struct boot_handover_ops boot_handover_image_ops = {
    .init = handover_init,
    .prepare = boot_handover_image_prepare,
    .jump = boot_handover_image_jump,
};
