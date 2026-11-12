/**
 * Punch BOOT
 *
 * Copyright (C) 2026 ACTIA Nordic AB
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The delivery through TF-A (boot.md 5.3): pb starts BL31 with a bl_params
 * list (v2) in x0. BL33's entry point in it is the board's handover, its
 * entry and its four arguments; BL32's, when the board names its part, has
 * OP-TEE's arguments. ARMv8-A only.
 */

#include <arch/arch.h>
#include <arch/arch_helpers.h>
#include <boot/atf.h>
#include <bpak/bpak.h>
#include <bpak/id.h>
#include <inttypes.h>
#include <pb/arch.h>
#include <pb/pb.h>
#include <string.h>

#include "atf_interface.h"

#define BPAK_ID_OPTEE_ARCH 0x99e5e80a /* bpak_id("optee-arch"), holds 64 or 32 */

/* Everything BL31 reads, in one piece of memory */
struct tfa_params {
    bl_params_t params;
    bl_params_node_t bl33_node;
    image_info_t bl33_info;
    entry_point_info_t bl33_ep;
    bl_params_node_t bl32_node;
    image_info_t bl32_info;
    entry_point_info_t bl32_ep;
};

/*
 * Where they live is the platform's to say (platform.md 5.3): a BL31 may
 * take its parameters only from some memory. Such a platform gives the
 * memory, plat_bl_params. Without it they are in pb's own data.
 */
#ifdef PLAT_BL_PARAMS_SIZE
_Static_assert(sizeof(struct tfa_params) <= PLAT_BL_PARAMS_SIZE,
               "bl_params don't fit the platform's memory for them");
_Static_assert(__alignof__(struct tfa_params) <= PLAT_BL_PARAMS_ALIGN,
               "the platform's memory for bl_params isn't aligned for them");
#define TFA_PARAMS ((struct tfa_params *)(void *)plat_bl_params)
#else
static struct tfa_params own_params;
#define TFA_PARAMS (&own_params)
#endif

static const struct boot_delivery_tfa_config *cfg;

/* What prepare found in the image, for the jump */
static uintptr_t bl31_addr;
static uint32_t bl33_size;
static uintptr_t bl32_addr;
static uint32_t bl32_size;
static uint32_t bl32_rw;

static void set_param_head(param_header_t *h, uint8_t type, uint16_t size, uint32_t attr)
{
    h->type = type;
    h->version = PARAM_VERSION_2;
    h->size = size;
    h->attr = attr;
}

/* AArch64, at EL2 when the CPU has it, else at EL1 */
static uint32_t bl33_spsr(void)
{
    unsigned int mode;

    if (((read_id_aa64pfr0_el1() >> ID_AA64PFR0_EL2_SHIFT) & ID_AA64PFR0_ELX_MASK) != 0)
        mode = MODE_EL2;
    else
        mode = MODE_EL1;

    return SPSR_64(mode, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);
}

/* OP-TEE's execution state, from the image's attribute for it: 64 without one */
static uint32_t optee_rw(struct bpak_header *hdr)
{
    struct bpak_meta_header *mh;

    if (bpak_get_meta_anyref(hdr, BPAK_ID_OPTEE_ARCH, &mh) == BPAK_OK)
        return (*bpak_get_meta_ptr(hdr, mh, uint32_t) == 32) ? MODE_RW_32 : MODE_RW_64;

    return MODE_RW_64;
}

/* The size of the part that is loaded at 'addr' */
static int part_size_at(struct bpak_header *hdr, uintptr_t addr, uint32_t *size)
{
    struct bpak_meta_header *mh;

    bpak_foreach_part(hdr, p) {
        if (!p->id)
            break;

        if (bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, p->id, &mh) != BPAK_OK)
            continue;

        if ((uintptr_t)*bpak_get_meta_ptr(hdr, mh, uint64_t) == addr) {
            *size = bpak_part_size(p);
            return PB_OK;
        }
    }

    return -PB_ERR_NOT_FOUND;
}

int boot_delivery_tfa_init(const struct boot_delivery_tfa_config *cfg_in)
{
    cfg = cfg_in;

    /* BL32 is optional */
    if (cfg == NULL || cfg->bl31_bpak_id == 0)
        return -PB_ERR_PARAM;

    return PB_OK;
}

int boot_delivery_tfa_prepare(struct bpak_header *hdr, const struct boot_handover_entry *next)
{
    int rc;
    struct bpak_part_header *ph;
    struct bpak_meta_header *mh;

    if (cfg == NULL)
        return -PB_ERR_STATE;

    rc = bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, cfg->bl31_bpak_id, &mh);

    if (rc != BPAK_OK) {
        LOG_ERR("Could not read BL31 image meta data (%i)", rc);
        return -PB_ERR_BAD_META;
    }

    bl31_addr = (uintptr_t)*bpak_get_meta_ptr(hdr, mh, uint64_t);

    LOG_INFO("BL31: 0x%" PRIxPTR, bl31_addr);

    /* BL33 is the handover's entry, and its image info is the part that is
     * loaded there. An entry without a part is refused: the image has a load
     * address for BL33 and nothing to load */
    if (part_size_at(hdr, next->entry, &bl33_size) != PB_OK) {
        LOG_ERR("Could not read BL33 part meta");
        return -PB_ERR_BAD_META;
    }

    LOG_INFO("BL33: %" PRIxPTR " (%" PRIu32 " bytes)", next->entry, bl33_size);

    bl32_addr = 0;
    bl32_size = 0;
    bl32_rw = MODE_RW_64;

    if (cfg->bl32_bpak_id) {
        rc = bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, cfg->bl32_bpak_id, &mh);

        if (rc != BPAK_OK) {
            LOG_ERR("Could not read BL32 image meta data (%i)", rc);
            return -PB_ERR_BAD_META;
        }

        bl32_addr = (uintptr_t)*bpak_get_meta_ptr(hdr, mh, uint64_t);

        rc = bpak_get_part(hdr, cfg->bl32_bpak_id, &ph);

        if (rc != BPAK_OK) {
            LOG_ERR("Could not read BL32 part meta (%i)", rc);
            return -PB_ERR_BAD_META;
        }

        bl32_size = bpak_part_size(ph);
        bl32_rw = optee_rw(hdr);

        LOG_INFO("BL32: %" PRIxPTR " (%" PRIu32 " bytes)", bl32_addr, bl32_size);
    }

    return PB_OK;
}

void boot_delivery_tfa_jump(const struct boot_handover_entry *next)
{
    struct tfa_params *p = TFA_PARAMS;

    arch_disable_mmu();

    /* Set after MMU disabled to ensure all addresses are raw and not memory mapped */
    memset(p, 0, sizeof(*p));

    set_param_head(&p->params.h, PARAM_BL_PARAMS, sizeof(p->params), 0);
    p->params.head = &p->bl33_node;

    /* BL33: the board's handover, in the normal world */
    p->bl33_node.image_id = BL33_IMAGE_ID;
    p->bl33_node.image_info = &p->bl33_info;
    p->bl33_node.ep_info = &p->bl33_ep;
    p->bl33_node.next_params_info = NULL;

    set_param_head(&p->bl33_info.h, PARAM_IMAGE_BINARY, sizeof(p->bl33_info), 0);
    p->bl33_info.image_base = next->entry;
    p->bl33_info.image_size = bl33_size;

    set_param_head(&p->bl33_ep.h, PARAM_EP, sizeof(p->bl33_ep), EP_NON_SECURE | EP_EXECUTABLE);
    p->bl33_ep.pc = next->entry;
    p->bl33_ep.spsr = bl33_spsr();
    p->bl33_ep.args.arg0 = next->arg[0];
    p->bl33_ep.args.arg1 = next->arg[1];
    p->bl33_ep.args.arg2 = next->arg[2];
    p->bl33_ep.args.arg3 = next->arg[3];

    if (cfg->bl32_bpak_id) {
        p->bl33_node.next_params_info = &p->bl32_node;

        p->bl32_node.image_id = BL32_IMAGE_ID;
        p->bl32_node.image_info = &p->bl32_info;
        p->bl32_node.ep_info = &p->bl32_ep;
        p->bl32_node.next_params_info = NULL;

        set_param_head(&p->bl32_info.h, PARAM_IMAGE_BINARY, sizeof(p->bl32_info), 0);
        p->bl32_info.image_base = bl32_addr;
        p->bl32_info.image_size = bl32_size;

        set_param_head(&p->bl32_ep.h, PARAM_EP, sizeof(p->bl32_ep), EP_SECURE | EP_EXECUTABLE);
        p->bl32_ep.pc = bl32_addr;

        /*
         * OP-TEE (opteed) default handoff, see opteed_main.c opteed_setup().
         * OP-TEE must be compiled with CFG_EXTERNAL_DTB_OVERLAY=n if the
         * device tree is not pre-prepped with the relevant firmware and
         * reserved memory nodes.
         */
        p->bl32_ep.args.arg0 = bl32_rw; /* aarch32/aarch64 */
        p->bl32_ep.args.arg1 = 0; /* no pageable part */
        p->bl32_ep.args.arg2 = 0; /* no mem limit */
        p->bl32_ep.args.arg3 = next->dt; /* the handover's device tree, or 0 */

        if (bl32_rw == MODE_RW_32) {
            p->bl32_ep.spsr = SPSR_MODE32(
                MODE32_svc, SPSR_T_ARM, SPSR_E_LITTLE, DAIF_FIQ_BIT | DAIF_IRQ_BIT | DAIF_ABT_BIT);
        } else {
            p->bl32_ep.spsr = SPSR_64(MODE_EL1, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);
        }
    }

    LOG_DBG("Jumping to %" PRIxPTR, bl31_addr);
    arch_jump((void *)bl31_addr, (void *)p, NULL, NULL, NULL);

    LOG_ERR("Jump returned %" PRIxPTR, bl31_addr);
}

static int delivery_init(const void *config)
{
    return boot_delivery_tfa_init(config);
}

const struct boot_delivery_ops boot_delivery_tfa_ops = {
    .init = delivery_init,
    .prepare = boot_delivery_tfa_prepare,
    .jump = boot_delivery_tfa_jump,
};
