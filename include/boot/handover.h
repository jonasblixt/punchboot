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
 * tree's /chosen, a transfer list): BOOT_HANDOVER_IMAGE() (boot/image.h),
 * BOOT_HANDOVER_FH() (boot/fh.h).
 *
 * A delivery brings them to the next stage. Without one the core does it
 * directly: arch_jump(entry, a0..a3), at the exception level pb runs at.
 * BOOT_DELIVERY_TFA() (boot/atf.h) goes through TF-A's BL31, which gets
 * them as BL33's entry point.
 */

#ifndef INCLUDE_BOOT_HANDOVER_H
#define INCLUDE_BOOT_HANDOVER_H

#include <boot/policy.h>
#include <stddef.h>
#include <stdint.h>
#include <uuid.h>

struct bpak_header;

/** 'slc' of a board without a security life cycle */
#define BOOT_INFO_NO_SLC                    (-1)

/** 'status': what pb couldn't do and booted anyway, a bit per condition
 * (boot.md 5.6). A bit keeps its meaning, new ones are appended */
#define BOOT_INFO_STATUS_WATCHDOG_NOT_ARMED (UINT32_C(1) << 0) /*!< The watchdog didn't arm */
/** The board reported a failure the boot goes on without (board.md 2.3) */
#define BOOT_INFO_STATUS_BOARD_DEGRADED     (UINT32_C(1) << 1)

/** Room for every key a keystore can hold: its number of keys is a uint8_t.
 * A device tree's list has no limit, and no released build gets one here */
#define BOOT_INFO_MAX_KEYS                  255

/**
 * pb's information (boot.md 5.6): what the next stage learns about the
 * device and this boot. The core collects it, each handover writes it in
 * its format (/chosen's pb,* properties, boot.md 5.5) and reads nothing of
 * it from the RoT, the SLC or the policy itself. The names are the ones of
 * pb's structure (boot.md 5.4).
 */
struct boot_info {
    /* The build's */
    const char *version; /*!< pb's version, the wire's version command's string */

    /* Collected once in a boot, before the first candidate. A failure to
     * read them stops the boot */
    uuid_t device_uuid; /*!< The device's identity */
    int32_t slc; /*!< The life cycle, the wire's pb_slc values, or BOOT_INFO_NO_SLC */
    size_t no_of_available_keys;
    uint32_t available_keys[BOOT_INFO_MAX_KEYS]; /*!< The ids of the keys not revoked,
                                                      in the keystore's order */
    /** What pb booted without, BOOT_INFO_STATUS_* (boot.md 5.6): 'status' in
     * pb's structure. From the boot's preconditions, checked before
     * early_boot(), and from the board, asked after it. A device tree gets
     * no pb,status yet (F-248) */
    uint32_t status;

    /* The candidate's, set before each prepare */
    enum boot_policy_mode boot_mode; /*!< How it was chosen. Normal for a partition
                                          the host named and for the host's stream */
    uuid_t boot_partition_uuid; /*!< All zero: none, 'boot bpak' named no partition */
    /** The booted system's name, the policy's: "A", "B", or "?" for a
     * partition that isn't one of its systems. NULL: the policy has no names */
    const char *system_name;
    uint32_t active_key; /*!< The id of the key the image is signed with */
};

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
     * what the next stage reads in memory, with pb's information 'info' in
     * the handover's format. 'next' is all zero when it is called.
     */
    int (*prepare)(struct bpak_header *hdr,
                   const struct boot_info *info,
                   struct boot_handover_entry *next);
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
