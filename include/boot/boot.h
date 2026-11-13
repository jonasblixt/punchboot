/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BOOT_H
#define INCLUDE_BOOT_H

#include <boot/handover.h>
#include <boot/policy.h>
#include <pb/bio.h>
#include <pb/utils_def.h>
#include <uuid.h>

/* Reads 'length' bytes at 'offset' bytes from the start of the image's
 * payload. The header is at -sizeof(struct bpak_header). */
typedef int (*boot_read_cb_t)(int64_t offset, size_t length, void *buf);
typedef int (*boot_result_cb_t)(int result);

/** Boot sources */
enum boot_source {
    BOOT_SOURCE_INVALID = 0, /*!< Invalid boot mode */
    BOOT_SOURCE_BIO, /*!< A partition: loaded, or verified in place when the
                          board's partition table says so */
    BOOT_SOURCE_CB, /*!< Load through callback functions */
    BOOT_SOURCE_END,
};

/**
 * Boot flags
 *
 * Bits 15 - 0 are reserved for punchboot
 * Bits 31 - 16 are intended for board/user
 *
 * \def BOOT_FLAG_CMD
 * Set when boot was initiated through the command mode interface.
 *
 * \def BOOT_FLAG_VERBOSE
 * Set to 1 to request verbose output during boot
 *
 */
#define BOOT_FLAG_CMD     BIT(0)
#define BOOT_FLAG_VERBOSE BIT(1)

struct boot_driver {
    enum boot_source default_boot_source;
    int (*early_boot_cb)(void);
    /* The board's boot source policy (boot/policy.h). NULL, or one without
     * ops: no boot from storage */
    const struct boot_policy *policy;
    const struct boot_handover *handover; /*!< The next stage's entry and arguments */
    const struct boot_delivery *delivery; /*!< How they reach it. NULL: directly */
    int (*late_boot_cb)(uuid_t boot_part_uu);
};

/**
 * Initializes the boot module
 *
 * @param[in] driver Configuration input
 *
 * @return PB_OK on success
 *        -PB_ERR_PARAM on invalid configuration
 */
int boot_init(const struct boot_driver *driver);

/**
 * Configure boot source
 *
 * @param[in] source Boot source
 *
 * @return PB_OK on success
 *        -PB_ERR_PARAM on invalid boot source
 *
 */
int boot_set_source(enum boot_source source);

/**
 * Configure read/result callbacks for boot source: 'BOOT_SOURCE_CB'
 *
 * When the boot source is set to 'BOOT_SOURCE_CB' the boot module
 * will call these functions in the following order
 *
 * 1. read_f(-4096, 4096) To read the BPAK header
 * 2. result_f with the result from the header validation/authenticatoin
 * 3. read_f(<offset>, <chunk sz>) Repetedly to transfer chunks of parts
 * 4. result_f When the chunked transfer of one part is complete
 *
 *  Steps 3 and 4 are repeted until all parts are transfered
 *
 * 5. result_f When payload has been verified
 *
 * @param[in] read_f Read function callback
 * @param[in] result_f Optional result function callback
 *
 */
void boot_configure_load_cb(boot_read_cb_t read_f, boot_result_cb_t result_f);

/**
 * Clear and/or Set flags
 *
 * These bits are available through out the boot flow and can control
 * board specific behaviour.
 *
 * The lower 16 bit's are reserved for punchboot and the upper 16 bit's
 * can be used by board code.
 *
 * @param[in] clear Bit flags to clear
 * @param[in] set Bit flags to set
 *
 */
void boot_clear_set_flags(uint32_t clear, uint32_t set);

/**
 * Read current boot flags
 *
 * @return Current boot flags
 */
uint32_t boot_get_flags(void);

/**
 * Command mode's 'boot activate': the board's policy activates 'part_uu'
 * (boot_policy_activate()).
 *
 * @param[in] part_uu UUID of partition to set as active
 *
 * @return PB_OK on success,
 *        -PB_ERR_NOT_SUPPORTED before boot_init(), without a policy, and
 *         where the policy has nothing to activate
 */
int boot_set_boot_partition(uuid_t part_uu);

/**
 * Command mode's 'boot status': the board's policy's partition
 * (boot_policy_status()).
 *
 * @param[out] part_uu UUID of the partition, or the null UUID
 */
void boot_get_boot_partition(uuid_t part_uu);

/**
 * Boot the system. This fuction tries to load/auth and execute a boot image
 *
 * Without 'boot_part_override_uu' the board's boot source policy hands out
 * the candidates (boot.md 2, 3): each is loaded, verified and prepared, the
 * policy hears the result, and the first that succeeds is booted. With it,
 * that partition is the one candidate; the policy isn't asked.
 *
 * Before the first candidate, pb's information for the handover is
 * collected (struct boot_info, boot.md 5.6): the device's identity, the
 * life cycle and the keys that aren't revoked. One that can't be read
 * stops the boot, and no candidate is tried. The boot's mode in it is the
 * candidate's: normal for a partition that was named and for the host's
 * stream (BOOT_SOURCE_CB), the policy's for a candidate of its own.
 *
 * @param[in] boot_part_override_uu Optional partition UUID to boot.
 *  NULL or the null UUID: the policy's candidates.
 *
 * @return This function will not return on sucess. The first candidate's
 *  error when every candidate failed, -PB_ERR_NO_ACTIVE_BOOT_PARTITION when
 *  the policy had none, the error of pb's information that couldn't be
 *  read.
 */
int boot(uuid_t boot_part_override_uu);

int boot_load(uuid_t boot_part_override_uu);
int boot_jump(void);

#endif
