/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BOOT_OPTEE_H
#define INCLUDE_BOOT_OPTEE_H

#include <boot/handover.h>
#include <bpak/bpak.h>
#include <pb/utils_def.h>
#include <stddef.h>
#include <stdint.h>

/**
 * The delivery through OP-TEE (boot.md 5.7), ARMv7-A only, with pb in the
 * secure world: OP-TEE is its own secure monitor. pb enters OP-TEE, the part
 * 'bl32_bpak_id' (its raw image, tee-raw.bin, whose load address is its
 * entry), in secure SVC with OP-TEE's convention without TF-A:
 *
 *   r0  0: no pageable part
 *   r1  the handover's r1 (0xffffffff, no machine type)
 *   r2  the handover's r2 (the device tree)
 *   r3  0
 *   lr  the handover's entry: the normal world's, the kernel's load address
 *
 * OP-TEE relays r1 and r2 to the normal world. It grows the device tree in
 * place to its CFG_DTB_MAX_SIZE, adding its own nodes: 'dtb_max_size' is
 * that size, and the room is checked at the device tree's load address as
 * the handover's own headroom is (boot.md 4.3).
 *
 * With the image handover only (BOOT_HANDOVER_IMAGE()): OP-TEE takes no
 * normal world entry with a transfer list on AArch32, so a board with
 * Firmware Handoff and this delivery is refused when the boot is set up.
 *
 * An image without BL32's part is jumped to directly, as the image handover
 * alone: a released image with OP-TEE in the kernel's part keeps booting.
 */
struct boot_delivery_optee_config {
    bpak_id_t bl32_bpak_id; /*!< OP-TEE's part */
    size_t dtb_max_size; /*!< OP-TEE's CFG_DTB_MAX_SIZE. 0: OP-TEE's default, 64 KiB */
};

/** OP-TEE's default CFG_DTB_MAX_SIZE */
#define BOOT_OPTEE_DTB_MAX_SIZE_DEFAULT 0x10000

int boot_delivery_optee_init(const struct boot_delivery_optee_config *cfg);
int boot_delivery_optee_prepare(struct bpak_header *hdr, const struct boot_handover_entry *next);
void boot_delivery_optee_jump(const struct boot_handover_entry *next);

/**
 * Enter OP-TEE at 'entry' with r0 = 0, r1, r2, r3 = 0 and lr = 'ns_entry',
 * IRQ and FIQ masked, the I-cache invalidated (optee_a32.S). With the MMU
 * and the D-cache off. Doesn't return.
 */
void boot_optee_enter(uintptr_t entry, uintptr_t ns_entry, uintptr_t r1, uintptr_t r2);

extern const struct boot_delivery_ops boot_delivery_optee_ops;

/**
 * A board's delivery (board.boot_delivery): through OP-TEE, with '_config',
 * a const struct boot_delivery_optee_config *.
 */
#define BOOT_DELIVERY_OPTEE(_config)                                           \
    {                                                                          \
        .ops = &boot_delivery_optee_ops,                                       \
        .config = PB_CONFIG(_config, const struct boot_delivery_optee_config), \
    }

#endif /* INCLUDE_BOOT_OPTEE_H */
