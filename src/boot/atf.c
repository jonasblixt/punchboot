/**
 * Punch BOOT
 *
 * Copyright (C) 2026 ACTIA Nordic AB
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Boot driver for Arm Trusted Firmwares bootloader interface
 */

#include <arch/arch.h>
#include <arch/arch_helpers.h>
#include <boot/atf.h>
#include <boot/dtb.h>
#include <bpak/bpak.h>
#include <bpak/id.h>
#include <inttypes.h>
#include <libfdt.h>
#include <pb/arch.h>
#include <pb/pb.h>
#include <pb/plat.h>

#include "atf_interface.h"

#define BPAK_ID_OPTEE_ARCH 0x99e5e80a /* bpak_id("optee-arch"), holds 64 or 32 */

static const struct boot_driver_atf_config *cfg;
static uintptr_t jump_addr;
static uintptr_t ramdisk_addr;
/*
 * The parameters for BL31, in .data rather than at the end of .bss: TF-A's
 * i.MX8M BL31 only takes them from the low part of the OCRAM (v2.12
 * imx8mp_bl31_setup.c swaps base and size in that check, below 0x960000
 * then), and otherwise starts BL33 at its own fixed address without them.
 */
static bl_params_t atf_params __section(".data");
static bl_params_node_t atf_bl33_node __section(".data");
static image_info_t atf_bl33_info __section(".data");
static entry_point_info_t atf_bl33_ep __section(".data");
static bl_params_node_t atf_bl32_node __section(".data");
static image_info_t atf_bl32_info __section(".data");
static entry_point_info_t atf_bl32_ep __section(".data");

static void atf_set_param_head(param_header_t *h, uint8_t type, uint16_t size)
{
    h->type = type;
    h->version = PARAM_VERSION_2;
    h->size = size;
    h->attr = 0;
}

static uint32_t atf_bl33_spsr(void)
{
    unsigned int mode;

    if (((read_id_aa64pfr0_el1() >> ID_AA64PFR0_EL2_SHIFT) & ID_AA64PFR0_ELX_MASK) != 0)
        mode = MODE_EL2;
    else
        mode = MODE_EL1;

    return SPSR_64(mode, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);
}

static uint32_t atf_optee_rw(struct bpak_header *hdr)
{
#ifdef __aarch64__
    struct bpak_meta_header *mh;

    if (bpak_get_meta_anyref(hdr, BPAK_ID_OPTEE_ARCH, &mh) == BPAK_OK)
        return (*bpak_get_meta_ptr(hdr, mh, uint32_t) == 32) ? MODE_RW_32 : MODE_RW_64;

    return MODE_RW_64;
#else
    (void)hdr;
    return MODE_RW_32;
#endif
}

int boot_driver_atf_init(const struct boot_driver_atf_config *cfg_in)
{
    cfg = cfg_in;

    /* BL32 is optional */
    if (cfg == NULL || cfg->atf_bpak_id == 0 || cfg->bl33_bpak_id == 0)
        return -PB_ERR_PARAM;

    return PB_OK;
}

int boot_driver_atf_prepare(struct bpak_header *hdr, uuid_t boot_part_uu)
{
    int rc;
    void *fdt;
    struct bpak_part_header *ph;
    struct bpak_meta_header *mh;

    rc = bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, cfg->atf_bpak_id, &mh);

    if (rc != BPAK_OK) {
        LOG_ERR("Could not read ATF image meta data (%i)", rc);
        return -PB_ERR_BAD_META;
    }

    jump_addr = (uintptr_t)*bpak_get_meta_ptr(hdr, mh, uint64_t);

    LOG_INFO("Boot entry: 0x%" PRIxPTR, jump_addr);

    rc = bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, cfg->bl33_bpak_id, &mh);
    if (rc != BPAK_OK) {
        LOG_ERR("Could not read BL33 image meta data (%i)", rc);
        return -PB_ERR_BAD_META;
    }

    atf_bl33_info.image_base = atf_bl33_ep.pc = (uintptr_t)*bpak_get_meta_ptr(hdr, mh, uint64_t);

    rc = bpak_get_part(hdr, cfg->bl33_bpak_id, &ph);
    if (rc != BPAK_OK) {
        LOG_ERR("Could not read BL33 part meta (%i)", rc);
        return -PB_ERR_BAD_META;
    }

    atf_bl33_info.image_size = bpak_part_size(ph);

    LOG_INFO("BL33: %" PRIxPTR " (%u bytes)", atf_bl33_info.image_base, atf_bl33_info.image_size);

    if (cfg->bl32_bpak_id) {
        rc = bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, cfg->bl32_bpak_id, &mh);
        if (rc != BPAK_OK) {
            LOG_ERR("Could not read BL32 image meta data (%i)", rc);
            return -PB_ERR_BAD_META;
        }

        atf_bl32_info.image_base = atf_bl32_ep.pc =
            (uintptr_t)*bpak_get_meta_ptr(hdr, mh, uint64_t);

        rc = bpak_get_part(hdr, cfg->bl32_bpak_id, &ph);
        if (rc != BPAK_OK) {
            LOG_ERR("Could not read BL32 part meta (%i)", rc);
            return -PB_ERR_BAD_META;
        }

        atf_bl32_info.image_size = bpak_part_size(ph);

        LOG_INFO(
            "BL32: %" PRIxPTR " (%u bytes)", atf_bl32_info.image_base, atf_bl32_info.image_size);

        /*
         * OP-TEE (opteed) default handoff, see opteed_main.c opteed_setup().
         * OP-TEE must be compiled with CFG_EXTERNAL_DTB_OVERLAY=n if the
         * device tree is not pre-prepped with the relevant firmware and
         * reserved memory nodes.
         */
        atf_bl32_ep.args.arg0 = atf_optee_rw(hdr); /* aarch32/aarch64 */
        atf_bl32_ep.args.arg1 = 0; /* no pageable part */
        atf_bl32_ep.args.arg2 = 0; /* no mem limit */
        atf_bl32_ep.args.arg3 = 0; /* device tree, filled in below */
    }

    if (cfg->ramdisk_bpak_id) {
        rc = bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, cfg->ramdisk_bpak_id, &mh);
        if (rc != BPAK_OK) {
            LOG_ERR("Could not read ramdisk image meta data (%i)", rc);
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

        atf_bl33_ep.args.arg0 = *bpak_get_meta_ptr(hdr, mh, uint64_t);
        LOG_INFO("DTB: %" PRIxPTR, (uintptr_t)atf_bl33_ep.args.arg0);

        if (cfg->bl32_bpak_id)
            atf_bl32_ep.args.arg3 = atf_bl33_ep.args.arg0;

        /* Room for /chosen and pb's properties */
        rc = boot_dtb_grow(hdr, cfg->dtb_bpak_id, CONFIG_BOOT_DTB_HEADROOM, &fdt);

        if (rc != PB_OK)
            return rc;

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

    return PB_OK;
}

void boot_driver_atf_jump(void)
{
    arch_disable_mmu();

    /* Set after MMU disabled to ensure all addresses are raw and not memory mapped */

    atf_params.h.type = PARAM_BL_PARAMS;
    atf_params.h.version = PARAM_VERSION_2;
    atf_params.h.size = sizeof(atf_params);
    atf_params.h.attr = 0;
    atf_params.head = &atf_bl33_node;

    atf_bl33_node.image_id = BL33_IMAGE_ID;
    atf_bl33_node.image_info = &atf_bl33_info;
    atf_bl33_node.ep_info = &atf_bl33_ep;
    atf_bl33_node.next_params_info = NULL;

    atf_set_param_head(&atf_bl33_info.h, PARAM_IMAGE_BINARY, sizeof(atf_bl33_info));
    atf_set_param_head(&atf_bl33_ep.h, PARAM_EP, sizeof(atf_bl33_ep));
    atf_bl33_ep.h.attr = EP_NON_SECURE | EP_EXECUTABLE;
    atf_bl33_ep.spsr = atf_bl33_spsr();

    if (cfg->bl32_bpak_id) {
        atf_bl33_node.next_params_info = &atf_bl32_node;

        atf_bl32_node.image_id = BL32_IMAGE_ID;
        atf_bl32_node.image_info = &atf_bl32_info;
        atf_bl32_node.ep_info = &atf_bl32_ep;
        atf_bl32_node.next_params_info = NULL;

        atf_set_param_head(&atf_bl32_info.h, PARAM_IMAGE_BINARY, sizeof(atf_bl32_info));
        atf_set_param_head(&atf_bl32_ep.h, PARAM_EP, sizeof(atf_bl32_ep));
        atf_bl32_ep.h.attr = EP_SECURE | EP_EXECUTABLE;

        if (atf_bl32_ep.args.arg0 == MODE_RW_32) {
            atf_bl32_ep.spsr = SPSR_MODE32(
                MODE32_svc, SPSR_T_ARM, SPSR_E_LITTLE, DAIF_FIQ_BIT | DAIF_IRQ_BIT | DAIF_ABT_BIT);
        } else {
            atf_bl32_ep.spsr = SPSR_64(MODE_EL1, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);
        }
    }

    LOG_DBG("Jumping to %" PRIxPTR, jump_addr);
    arch_jump((void *)jump_addr, (void *)&atf_params, NULL, NULL, NULL);

    LOG_ERR("Jump returned %" PRIxPTR, jump_addr);
}

static int handover_init(const void *config)
{
    return boot_driver_atf_init(config);
}

const struct boot_handover_ops boot_handover_atf_ops = {
    .init = handover_init,
    .prepare = boot_driver_atf_prepare,
    .jump = boot_driver_atf_jump,
};
