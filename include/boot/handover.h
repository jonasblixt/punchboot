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
 * configuration for it, made by the module's macro.
 *
 * A handover turns the verified image and pb's information into the next
 * stage's entry, its four arguments and what it reads in memory (a device
 * tree's /chosen): BOOT_HANDOVER_IMAGE() (boot/image.h),
 * BOOT_HANDOVER_SIMPLE() (boot/simple.h).
 *
 * A delivery brings them to the next stage. Without one the core does it
 * directly: arch_jump(entry, a0..a3), at the exception level pb runs at.
 * BOOT_DELIVERY_TFA() (boot/atf.h) goes through TF-A's BL31, which gets
 * them as BL33's entry point.
 */

#ifndef INCLUDE_BOOT_HANDOVER_H
#define INCLUDE_BOOT_HANDOVER_H

#include <stdint.h>
#include <uuid.h>

struct bpak_header;

/** What a handover gives its delivery */
struct boot_handover_entry {
    uintptr_t entry; /*!< The next stage's entry (ARMv7-M: its vector table) */
    uintptr_t arg[4]; /*!< r0..r3, x0..x3 */
    /** The device tree the handover prepared for the next stage, 0 without
     * one. Not an argument: for a delivery that tells another stage where
     * it is (TF-A's BL32) */
    uintptr_t dt;
};

struct boot_handover_ops {
    int (*init)(const void *config);
    /**
     * The entry and the arguments for the image 'hdr' was verified for, and
     * what the next stage reads in memory. 'next' is all zero when it is
     * called.
     */
    int (*prepare)(struct bpak_header *hdr, uuid_t boot_part_uu, struct boot_handover_entry *next);
};

struct boot_handover {
    const struct boot_handover_ops *ops;
    const void *config;
};

struct boot_delivery_ops {
    int (*init)(const void *config);
    /** After the handover's prepare: what the delivery itself needs of the image */
    int (*prepare)(struct bpak_header *hdr, const struct boot_handover_entry *next);
    /** Start the next stage with 'next'. Returns only when that failed */
    void (*jump)(const struct boot_handover_entry *next);
};

struct boot_delivery {
    const struct boot_delivery_ops *ops; /*!< NULL: directly */
    const void *config;
};

#endif // INCLUDE_BOOT_HANDOVER_H
