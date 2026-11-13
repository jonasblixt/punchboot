/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * What the next stage gets with the Firmware Handoff handover (boot.md 5.4):
 * the registers, the transfer list and pb's entry in it. Shared by pb and
 * the next stage, in C or C++: it needs <stdint.h> and <stddef.h>, nothing
 * else of pb's.
 *
 * Arm's Firmware Handoff specification, 2.0: transfer list version 2,
 * register convention 1. Everything in the list is little-endian, and
 * nothing in it points into it: the next stage may move it.
 *
 * The registers at the next stage's entry:
 *
 *  AArch64  x0 = the device tree in the list or 0, x1 = PB_FH_REG_X1,
 *           x2 = 0, x3 = the list
 *  AArch32  r0 = 0, r1 = PB_FH_REG_R1, r2 = the device tree in the list
 *           or 0, r3 = the list
 *  ARMv7-M  AArch32's. The specification covers AArch64 and AArch32 only:
 *           pb's extension
 *
 * The list holds one entry, of one of two kinds, never both: the image's
 * device tree (PB_FH_TAG_FDT), with pb's information in its /chosen, when
 * the board names a device tree part; else pb's structure
 * (PB_FH_TAG_HANDOVER, struct pb_fh_handover).
 */

#ifndef INCLUDE_BOOT_PB_FH_H
#define INCLUDE_BOOT_PB_FH_H

#include <stddef.h>
#include <stdint.h>

/** The register convention's version, and the signature register's value:
 * the list's signature, as much of it as the convention has room for, and
 * the version above it (bits 39:32 of x1, bits 31:24 of r1) */
#define PB_FH_REG_VERSION          1
#define PB_FH_REG_X1               (UINT64_C(0x4a0fb10b) | ((uint64_t)PB_FH_REG_VERSION << 32))
#define PB_FH_REG_R1               (UINT32_C(0x0fb10b) | ((uint32_t)PB_FH_REG_VERSION << 24))

#define PB_FH_TL_SIGNATURE         UINT32_C(0x4a0fb10b)
#define PB_FH_TL_VERSION           2
/** The largest alignment an entry's data needs, as a power of two: 8 bytes */
#define PB_FH_TL_ALIGNMENT         3
/** flags: the list uses checksums */
#define PB_FH_TL_FLAG_HAS_CHECKSUM (UINT32_C(1) << 0)
/** An entry starts at a multiple of this from the list's base */
#define PB_FH_TL_GRANULE           8

/**
 * The transfer list's header, at the list's base: an address that isn't 0
 * and is 8-byte aligned.
 *
 * 'checksum' makes the bytes of the used part, [0, used_size), sum to 0
 * modulo 256. The specification's text has it two ways; this is its header
 * table's and the reference library's (TF-Shared's transfer-list-library).
 */
struct pb_fh_tl_header {
    uint32_t signature; /*!< PB_FH_TL_SIGNATURE */
    uint8_t checksum;
    uint8_t version; /*!< PB_FH_TL_VERSION */
    uint8_t hdr_size; /*!< sizeof(struct pb_fh_tl_header), 0x18 */
    uint8_t alignment; /*!< PB_FH_TL_ALIGNMENT */
    uint32_t used_size; /*!< The header, the entries and the padding after
                             each: a multiple of 8 */
    uint32_t total_size; /*!< The list's region: a multiple of 8 */
    uint32_t flags; /*!< PB_FH_TL_FLAG_HAS_CHECKSUM */
    uint32_t reserved; /*!< 0 */
};

/**
 * An entry's header. The first one follows the list's header, the next one
 * starts at the next multiple of 8 after the entry's data. The data follows
 * the header, so it is 8-byte aligned.
 */
struct pb_fh_tl_entry {
    /** tag_id in bits 23:0, hdr_size in bits 31:24:
     * sizeof(struct pb_fh_tl_entry), 8 */
    uint32_t tag_and_hdr_size;
    uint32_t data_size; /*!< The data's size, without the padding after it */
};

#define PB_FH_TL_ENTRY_TAG(_entry)      ((_entry)->tag_and_hdr_size & UINT32_C(0xffffff))
#define PB_FH_TL_ENTRY_HDR_SIZE(_entry) ((_entry)->tag_and_hdr_size >> 24)

/** The specification's entries pb writes or a next stage meets first */
#define PB_FH_TAG_VOID                  0 /*!< Nothing: an entry that was removed */
#define PB_FH_TAG_FDT                   1 /*!< A device tree, the one the register points to */

/**
 * pb's structure's entry.
 *
 * NOT ALLOCATED. The tag is allocated in the specification, by a request
 * upstream that adds the layout below to its text (boot.md 5.4), and that
 * has not happened yet. Until then this is a number from the range the
 * specification leaves to use without allocation (0xfff000 to 0xffffff),
 * which it discourages for anything but local experiments: it is nobody's,
 * so another stage's entry can have it too.
 *
 * It MUST change to the allocated tag before pb's structure is in a
 * release: what is released is fixed, and the tag can't change afterwards
 * (boot.md 5.4, 7). The device tree variant has the specification's tag
 * and doesn't wait.
 */
#define PB_FH_TAG_HANDOVER              UINT32_C(0xfffb00)

/** boot_mode: how the boot was chosen (boot.md 5.6) */
#define PB_FH_BOOT_MODE_NORMAL          0 /*!< The first candidate of the boot */
#define PB_FH_BOOT_MODE_FALLBACK        1 /*!< Another one, the first failed */
#define PB_FH_BOOT_MODE_SPECULATIVE     2 /*!< A/B's speculative mode */

/** slc of a device without a security life cycle */
#define PB_FH_NO_SLC                    (-1)

/** The keys pb's structure has room for */
#define PB_FH_MAX_KEYS                  16

/**
 * pb's structure (boot.md 5.4): pb's information (boot.md 5.6), the data of
 * the entry PB_FH_TAG_HANDOVER.
 *
 * Little-endian, no pointers. Fields are only appended, and the structure
 * has no size or version of its own: the entry's data_size is its size, so
 * read the fields that data_size covers. A field that isn't there was added
 * after the pb that booted the image.
 */
struct pb_fh_handover {
    uint32_t boot_mode; /*!< PB_FH_BOOT_MODE_* */
    uint8_t device_uuid[16]; /*!< The bytes in the order of the UUID's string */
    uint8_t boot_partition_uuid[16]; /*!< All zero: none, the host named no partition */
    char system_name[8]; /*!< "A", "B", "?" or empty; NUL terminated */
    int32_t slc; /*!< The wire's pb_slc values; PB_FH_NO_SLC without an SLC */
    uint32_t active_key; /*!< The id of the key the image is signed with */
    uint32_t no_of_available_keys;
    uint32_t available_keys[PB_FH_MAX_KEYS]; /*!< The ids of the keys not revoked */
    uint32_t status; /*!< What pb booted without, bit flags (boot.md 5.6); 0: nothing */
    char version[32]; /*!< pb's version, NUL terminated */
};

/* The layouts, checked where the header is compiled: C has _Static_assert,
 * a next stage in C++ static_assert */
#ifdef __cplusplus
#define PB_FH_STATIC_ASSERT(_condition, _message) static_assert(_condition, _message)
#else
#define PB_FH_STATIC_ASSERT(_condition, _message) _Static_assert(_condition, _message)
#endif

PB_FH_STATIC_ASSERT(sizeof(struct pb_fh_tl_header) == 0x18, "transfer list header");
PB_FH_STATIC_ASSERT(sizeof(struct pb_fh_tl_entry) == 8, "transfer list entry header");
PB_FH_STATIC_ASSERT(offsetof(struct pb_fh_handover, device_uuid) == 4, "pb_fh_handover layout");
PB_FH_STATIC_ASSERT(offsetof(struct pb_fh_handover, boot_partition_uuid) == 20,
                    "pb_fh_handover layout");
PB_FH_STATIC_ASSERT(offsetof(struct pb_fh_handover, system_name) == 36, "pb_fh_handover layout");
PB_FH_STATIC_ASSERT(offsetof(struct pb_fh_handover, slc) == 44, "pb_fh_handover layout");
PB_FH_STATIC_ASSERT(offsetof(struct pb_fh_handover, active_key) == 48, "pb_fh_handover layout");
PB_FH_STATIC_ASSERT(offsetof(struct pb_fh_handover, no_of_available_keys) == 52,
                    "pb_fh_handover layout");
PB_FH_STATIC_ASSERT(offsetof(struct pb_fh_handover, available_keys) == 56, "pb_fh_handover layout");
PB_FH_STATIC_ASSERT(offsetof(struct pb_fh_handover, status) == 120, "pb_fh_handover layout");
PB_FH_STATIC_ASSERT(offsetof(struct pb_fh_handover, version) == 124, "pb_fh_handover layout");
PB_FH_STATIC_ASSERT(sizeof(struct pb_fh_handover) == 156, "pb_fh_handover size");

#endif /* INCLUDE_BOOT_PB_FH_H */
