/**
 * Punch BOOT
 *
 * Copyright (C) 2026 ACTIA Nordic AB
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BOOT_ATF_H
#define INCLUDE_BOOT_ATF_H

#include <boot/handover.h>
#include <bpak/bpak.h>
#include <pb/utils_def.h>
#include <stdint.h>

/**
 * The delivery through TF-A (boot.md 5.3), ARMv8-A only: pb jumps to BL31,
 * the part 'bl31_bpak_id', with x0 = a bl_params list (v2) and x1 to x3 = 0.
 * The list holds:
 *
 * - BL33: the board's handover. Its entry is BL33's, in AArch64 at EL2 when
 *   the CPU has it and else at EL1, and its four arguments are x0 to x3
 *   there. With the image handover that is Linux or Xen with the device
 *   tree in x0 ('set_dtb_boot_arg'). A part of the image is loaded at the
 *   entry: an image without it is refused.
 * - BL32, when the board names its part: OP-TEE's convention, as TF-A's
 *   opteed reads it. x0 = OP-TEE's execution state (the image's
 *   "optee-arch" meta: 64 or 32, 64 without it), x1 = 0 (no pageable part),
 *   x2 = 0 (no memory limit), x3 = the device tree the handover prepared
 *   for BL33, or 0 without one.
 *
 * A BL31 may take the list only from some memory (NXP's i.MX8M BL31: the
 * low part of the OCRAM). Such a platform defines PLAT_BL_PARAMS_SIZE and
 * the memory itself, plat_bl_params (platform.md 5.3). Without it the list
 * is in pb's own data.
 */
struct boot_delivery_tfa_config {
    bpak_id_t bl31_bpak_id; /*!< BL31's part */
    bpak_id_t bl32_bpak_id; /*!< Optional: BL32's part */
};

#ifdef PLAT_BL_PARAMS_SIZE
/** The platform's memory for bl_params: PLAT_BL_PARAMS_SIZE bytes, which the
 * platform defines with this alignment too */
#define PLAT_BL_PARAMS_ALIGN 16
extern uint8_t plat_bl_params[] __attribute__((aligned(PLAT_BL_PARAMS_ALIGN)));
#define PLAT_BL_PARAMS_BASE ((uintptr_t)plat_bl_params)
#endif

int boot_delivery_tfa_init(const struct boot_delivery_tfa_config *cfg);
int boot_delivery_tfa_prepare(struct bpak_header *hdr, const struct boot_handover_entry *next);
void boot_delivery_tfa_jump(const struct boot_handover_entry *next);

extern const struct boot_delivery_ops boot_delivery_tfa_ops;

/**
 * A board's delivery (board.boot_delivery): through TF-A, with '_config', a
 * const struct boot_delivery_tfa_config *.
 */
#define BOOT_DELIVERY_TFA(_config)                                           \
    {                                                                        \
        .ops = &boot_delivery_tfa_ops,                                       \
        .config = PB_CONFIG(_config, const struct boot_delivery_tfa_config), \
    }

#endif /* INCLUDE_BOOT_ATF_H */
