/**
 * \file handover.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A board's handover and its delivery (board.boot_handover,
 * board.boot_delivery, boot.md 5.1): the module's functions and the board's
 * configuration for it, made by the module's macro: BOOT_HANDOVER_IMAGE()
 * (boot/linux.h), BOOT_HANDOVER_ATF() (boot/atf.h), BOOT_HANDOVER_SIMPLE()
 * (boot/simple.h).
 */

#ifndef INCLUDE_BOOT_HANDOVER_H
#define INCLUDE_BOOT_HANDOVER_H

#include <uuid.h>

struct bpak_header;

struct boot_handover_ops {
    int (*init)(const void *config);
    int (*prepare)(struct bpak_header *hdr, uuid_t boot_part_uu);
    void (*jump)(void);
};

struct boot_handover {
    const struct boot_handover_ops *ops;
    const void *config;
};

/* No delivery has an object yet: the TF-A delivery comes with F-258 */
struct boot_delivery_ops;

struct boot_delivery {
    const struct boot_delivery_ops *ops; /*!< NULL: directly */
    const void *config;
};

#endif // INCLUDE_BOOT_HANDOVER_H
