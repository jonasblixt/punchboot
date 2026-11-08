/**
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The part of bpak's lib/internal.h that src/lib/bpak.c uses, which
 * includes it as "internal.h". The rest is for the file and package
 * functions that punchboot does not vendor (scripts/sync-bpak).
 */

#ifndef BPAK_LIB_INTERNAL_H
#define BPAK_LIB_INTERNAL_H

#include <bpak/bpak.h>
#include <stdint.h>

/** Iterate over the parts in use, they end at the first empty slot */
#define bpak_foreach_used_part(_hdr, _var)                          \
    for (struct bpak_part_header *_var = (_hdr)->parts;             \
         _var != &((_hdr)->parts[BPAK_MAX_PARTS]) && _var->id != 0; \
         _var++)

/** Size of a part in the decoded package, with its padding */
static inline uint64_t bpak_part_decoded_size(const struct bpak_part_header *part)
{
    return part->size + part->pad_bytes;
}

int bpak_get_meta_value(struct bpak_header *hdr,
                        bpak_id_t id,
                        bpak_id_t part_id_ref,
                        size_t min_size,
                        size_t max_size,
                        uint8_t **value);

int bpak_same_package_uuid(struct bpak_header *a, struct bpak_header *b);

off_t bpak_part_data_offset(struct bpak_header *h,
                            struct bpak_part_header *part,
                            off_t data_offset);

#endif // BPAK_LIB_INTERNAL_H
