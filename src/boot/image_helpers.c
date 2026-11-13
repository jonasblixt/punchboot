/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <boot/image_helpers.h>
#include <boot/image_regions.h>
#include <bpak/id.h>
#include <bpak/keystore.h>
#include <inttypes.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <string.h>

/* What pb links, all of it in one of the two (platform.md 5.1) */
IMPORT_SYM(uintptr_t, _pb_start, pb_start);
IMPORT_SYM(uintptr_t, _pb_end, pb_end);
IMPORT_SYM(uintptr_t, _buffers_start, buffers_start);
IMPORT_SYM(uintptr_t, _buffers_end, buffers_end);

/* The board's list once it passed its check, or NULL: nothing is loaded */
static const struct boot_image_region *image_regions;

static uint8_t signature[512];

int boot_image_auth_header(struct bpak_header *hdr)
{
    int rc;
    hash_t hash_kind = 0;
    dsa_t dsa_kind = 0;
    size_t digest_length;
    size_t signature_length;
    bool verified = false;
    uint8_t header_digest[64];
    const uint8_t *key_der_data;
    size_t key_der_data_length;

    rc = bpak_valid_header(hdr);

    if (rc != BPAK_OK) {
        return -PB_ERR_BAD_HEADER;
    }

    LOG_DBG("Key-store: %x", hdr->keystore_id);
    LOG_DBG("Key-ID: %x", hdr->key_id);

    rc = rot_read_key_status(hdr->key_id);

    if (rc != PB_OK) {
        LOG_ERR("Read key status failed (%i)", rc);
        return rc;
    }

    rc = rot_get_dsa_key(hdr->key_id, &dsa_kind, &key_der_data, &key_der_data_length);

    if (rc != PB_OK) {
        LOG_ERR("Get DSA key failed (%i)", rc);
        return rc;
    }

    switch (hdr->hash_kind) {
    case BPAK_HASH_SHA256:
        hash_kind = HASH_SHA256;
        digest_length = 32;
        break;
    case BPAK_HASH_SHA384:
        hash_kind = HASH_SHA384;
        digest_length = 48;
        break;
    case BPAK_HASH_SHA512:
        hash_kind = HASH_SHA512;
        digest_length = 64;
        break;
    default:
        return -PB_ERR_UNKNOWN_HASH;
    }

    rc = hash_init(hash_kind);

    if (rc != PB_OK) {
        LOG_ERR("Can't hash the image, hash kind 0x%x (%i)", hdr->hash_kind, rc);
        return rc;
    }

    signature_length = sizeof(signature);

    rc = bpak_copyz_signature(hdr, signature, &signature_length);

    if (rc != BPAK_OK)
        return -PB_ERR_BUF_TOO_SMALL;

    rc = hash_update(hdr, sizeof(*hdr));

    if (rc != PB_OK)
        return rc;

    rc = hash_final(header_digest, sizeof(header_digest));

    if (rc != PB_OK)
        return rc;

    rc = dsa_verify(dsa_kind,
                    signature,
                    signature_length,
                    key_der_data,
                    key_der_data_length,
                    hash_kind,
                    header_digest,
                    digest_length,
                    &verified);

    if (rc == 0 && verified) {
        LOG_INFO("Authentication successful");
    } else {
        LOG_ERR("Authentication failed");
        return -PB_ERR_AUTHENTICATION_FAILED;
    }

    return rc;
}

static bool region_is_end(const struct boot_image_region *r)
{
    return r->base == UINT64_MAX && r->size == 0;
}

int boot_image_set_regions(const struct boot_image_region *regions)
{
    /* What no region may hold, [start, end). The check of a part names none
     * of it (boot.md 4.3) */
    const struct {
        const char *name;
        uint64_t start;
        uint64_t end;
    } kept[] = {
        { "pb", pb_start, pb_end },
        { "buffers", buffers_start, buffers_end },
#ifdef CONFIG_BOOT_FH
        { "the Firmware Handoff region",
          CONFIG_BOOT_FH_REGION_BASE,
          (uint64_t)CONFIG_BOOT_FH_REGION_BASE + CONFIG_BOOT_FH_REGION_SIZE },
#endif
#ifdef PLAT_BL_PARAMS_BASE
        { "bl_params", PLAT_BL_PARAMS_BASE, (uint64_t)PLAT_BL_PARAMS_BASE + PLAT_BL_PARAMS_SIZE },
#endif
    };

    image_regions = NULL;

    if (regions == NULL)
        return PB_OK;

    for (unsigned int i = 0; !region_is_end(&regions[i]); i++) {
        const struct boot_image_region *r = &regions[i];
        /* The last byte: a region may end where the address space does */
        uint64_t last = r->base + (r->size - 1);

        if (r->size == 0 || last < r->base || (uintptr_t)last != last) {
            LOG_ERR("Board data: image region %u is empty, wraps or is above the arch's addresses",
                    i);
            return -PB_ERR_PARAM;
        }

        for (size_t k = 0; k < ARRAY_SIZE(kept); k++) {
            if (kept[k].start < kept[k].end && r->base < kept[k].end && kept[k].start <= last) {
                LOG_ERR("Board data: image region %u overlaps %s", i, kept[k].name);
                return -PB_ERR_PARAM;
            }
        }
    }

    image_regions = regions;
    return PB_OK;
}

bool boot_image_in_regions(uint64_t addr, uint64_t size)
{
    if (image_regions == NULL)
        return false;

    for (const struct boot_image_region *r = image_regions; !region_is_end(r); r++) {
        /* Without a sum of the range's, so nothing wraps */
        if (addr >= r->base && size <= r->size && addr - r->base <= r->size - size)
            return true;
    }

    return false;
}

/*
 * A part outside the payload's hash (bpak's dont-hash flag) is refused, before
 * anything is loaded (boot.md 4.2, security.md 4.2, D-035): nothing would
 * authenticate its bytes. No image that booted is refused: every release
 * hashed such a part with the others, so the image never passed its payload
 * hash, which upstream makes without the part.
 */
static int refuse_parts_outside_the_hash(struct bpak_header *hdr)
{
    bpak_foreach_part(hdr, p) {
        if (!p->id)
            break;

        if (p->flags & BPAK_FLAG_EXCLUDE_FROM_HASH) {
            LOG_ERR("Part 0x%x is outside the payload's hash (dont-hash): refused", p->id);
            return -PB_ERR_NOT_SUPPORTED;
        }
    }

    return PB_OK;
}

int boot_image_verify_parts(struct bpak_header *hdr)
{
    int rc;

    /* Not on the stack: 512 bytes, of the RT's 4 KiB */
    static struct {
        uint64_t addr;
        uint64_t last;
    } placed[BPAK_MAX_PARTS];

    size_t count = 0;

    if (bpak_valid_header(hdr) != BPAK_OK) {
        return -PB_ERR_BAD_HEADER;
    }

    rc = refuse_parts_outside_the_hash(hdr);

    if (rc != PB_OK)
        return rc;

    bpak_foreach_part(hdr, p) {
        if (!p->id)
            break;

        /* In 64 bits, as the image has it: an address the arch can't hold is
         * inside no region */
        uint64_t addr;
        uint64_t size = bpak_part_size(p);

        if (boot_image_load_addr(hdr, p->id, &addr) != PB_OK) {
            LOG_ERR("Could not read pb-entry for part %x", p->id);
            return -PB_ERR_BAD_META;
        }

        if (!boot_image_in_regions(addr, size)) {
            LOG_ERR("Part 0x%x isn't inside an image data region", p->id);
            return -PB_ERR_MEM;
        }

        /* An empty part is over nothing */
        if (size == 0)
            continue;

        /* The last bytes are compared: both parts are inside a region, so a
         * last byte doesn't wrap, where an end does in a region that ends
         * with the address space */
        uint64_t last = addr + (size - 1);

        for (size_t i = 0; i < count; i++) {
            if (addr <= placed[i].last && placed[i].addr <= last) {
                LOG_ERR("Part 0x%x overlaps another part", p->id);
                return -PB_ERR_MEM;
            }
        }

        placed[count].addr = addr;
        placed[count].last = last;
        count++;
    }

    return PB_OK;
}

int boot_image_verify_parts_in_place(struct bpak_header *hdr, uintptr_t base, size_t size)
{
    int rc;

    rc = bpak_valid_header(hdr);

    if (rc != BPAK_OK) {
        return -PB_ERR_BAD_HEADER;
    }

    rc = refuse_parts_outside_the_hash(hdr);

    if (rc != PB_OK)
        return rc;

    bpak_foreach_part(hdr, p) {
        if (!p->id)
            break;

        /* In 64 bits: nothing wraps, and an address the arch can't hold is
         * outside the range like any other */
        uint64_t load_addr;
        uint64_t part_size = bpak_part_size(p);

        if (boot_image_load_addr(hdr, p->id, &load_addr) != PB_OK) {
            LOG_ERR("Could not read pb-entry for part %x", p->id);
            return -PB_ERR_BAD_META;
        }

        if (load_addr < base || part_size > size || load_addr - base > size - part_size) {
            LOG_ERR("Part 0x%x isn't inside its partition", p->id);
            return -PB_ERR_MEM;
        }
    }

    return PB_OK;
}

/*
 * How long a part took to load: the time in read_f and the time in
 * hash_update_async(), which waits for the previous chunk's hash. The two
 * overlap when the hash is asynchronous, so the larger one is the bottleneck.
 */
static void log_part_time(uint32_t id, size_t bytes, uint32_t us, uint32_t read_us, uint32_t hash_us)
{
    LOG_DBG("Part %x: %zu bytes in %" PRIu32 " us (read %" PRIu32 " us, hash %" PRIu32
            " us), %" PRIu32 " KiB/s",
            id,
            bytes,
            us,
            read_us,
            hash_us,
            us ? (uint32_t)((uint64_t)bytes * 1000000 / 1024 / us) : 0);
    (void)id;
    (void)bytes;
    (void)us;
    (void)read_us;
    (void)hash_us;
}

int boot_image_load_and_hash(struct bpak_header *hdr,
                             size_t load_chunk_size,
                             boot_read_cb_t read_f,
                             boot_result_cb_t result_f,
                             uint8_t *payload_digest,
                             size_t payload_digest_size)
{
    int rc;
    uintptr_t load_addr;
    int hash_kind;
    size_t hash_length;

    rc = bpak_valid_header(hdr);

    if (rc != BPAK_OK) {
        return -PB_ERR_BAD_HEADER;
    }

    switch (hdr->hash_kind) {
    case BPAK_HASH_SHA256:
        hash_kind = HASH_SHA256;
        hash_length = 32;
        break;
    case BPAK_HASH_SHA384:
        hash_kind = HASH_SHA384;
        hash_length = 48;
        break;
    case BPAK_HASH_SHA512:
        hash_kind = HASH_SHA512;
        hash_length = 64;
        break;
    default:
        return -PB_ERR_UNKNOWN_HASH;
    }

    if (payload_digest_size < hash_length)
        return -PB_ERR_PARAM;

    rc = hash_init(hash_kind);

    if (rc != PB_OK)
        return rc;

    int hash_rc = PB_OK;
    size_t last_chunk_size = 0;

    bpak_foreach_part(hdr, p) {
        if (!p->id)
            break;

        uint64_t part_addr;

        load_addr = 0;
        rc = boot_image_load_addr(hdr, p->id, &part_addr);

        if (rc != PB_OK) {
            LOG_ERR("Could not read pb-entry for part %x", p->id);
            break;
        }

        load_addr = (uintptr_t)part_addr;

        LOG_DBG("Loading part %x --> %" PRIxPTR ", %zu bytes", p->id, load_addr, bpak_part_size(p));

        size_t bytes_to_read = bpak_part_size(p);
        size_t chunk_size = 0;
        size_t offset = 0;
        uint32_t part_start = plat_get_us_tick();
        uint32_t read_us = 0;
        uint32_t hash_us = 0;
        uint32_t t;

        while (bytes_to_read) {
            chunk_size = (bytes_to_read > load_chunk_size) ? load_chunk_size : bytes_to_read;

            uintptr_t addr = load_addr + offset;

            if (read_f) {
                /* The block size is up to read_f, it used to be 512 here (F-062) */
                int64_t read_offset =
                    (int64_t)(offset + bpak_part_offset(hdr, p) - sizeof(struct bpak_header));

                t = plat_get_us_tick();
                rc = read_f(read_offset, chunk_size, (void *)addr);
                read_us += plat_get_us_tick() - t;

                if (rc != PB_OK)
                    break;
            }

            /* Since we load chunks at an offset we can use the
             * async hash API (if the underlying driver supports it)
             */
            if (hash_rc == PB_OK) {
                t = plat_get_us_tick();
                hash_rc = hash_update_async((void *)addr, chunk_size);
                hash_us += plat_get_us_tick() - t;
            }

            /* A host streaming the image (result_f) sends all of it and
             * reads a result for every part, so after a hash error the
             * rest is still read to stay in step with it (D-008) */
            if (hash_rc != PB_OK && result_f == NULL)
                return hash_rc;

            /* Once per chunk, from every source (platform.md 3.3): after the
             * chunk, since that is the progress. A chunk that failed gets
             * none, and neither does a wait for one that never comes */
            plat_wdog_kick();

            bytes_to_read -= chunk_size;
            offset += chunk_size;
            last_chunk_size = chunk_size;
        }

        log_part_time(p->id, offset, plat_get_us_tick() - part_start, read_us, hash_us);

        int part_rc = (rc != PB_OK) ? rc : hash_rc;

        /* result_f reports whether the result could be sent, so it must
         * not replace the part's error (F-061) */
        if (result_f) {
            int result_rc = result_f(part_rc);

            if (result_rc != PB_OK)
                return result_rc;
        }

        /* Nothing more can be read after a read error */
        if (rc != PB_OK)
            return rc;
    }

    if (rc != PB_OK)
        return rc;

    if (hash_rc != PB_OK)
        return hash_rc;

    /* Waits for the last chunk's hash, the only one not overlapped with a read */
    uint32_t final_start = plat_get_us_tick();

    rc = hash_final(payload_digest, payload_digest_size);
    LOG_DBG("Payload hash final: %" PRIu32 " us, last chunk %zu bytes",
            plat_get_us_tick() - final_start,
            last_chunk_size);
    (void)final_start;
    (void)last_chunk_size;
    return rc;
}

int boot_image_copy_and_hash(struct bpak_header *hdr,
                             uintptr_t source_address,
                             size_t source_size,
                             uintptr_t destination_address,
                             size_t destination_size,
                             uint8_t *payload_digest,
                             size_t payload_digest_size)
{
    int rc;
    int hash_kind;
    size_t hash_length;

    rc = bpak_valid_header(hdr);

    if (rc != BPAK_OK) {
        return -PB_ERR_BAD_HEADER;
    }

    switch (hdr->hash_kind) {
    case BPAK_HASH_SHA256:
        hash_kind = HASH_SHA256;
        hash_length = 32;
        break;
    case BPAK_HASH_SHA384:
        hash_kind = HASH_SHA384;
        hash_length = 48;
        break;
    case BPAK_HASH_SHA512:
        hash_kind = HASH_SHA512;
        hash_length = 64;
        break;
    default:
        return -PB_ERR_UNKNOWN_HASH;
    }

    if (payload_digest_size < hash_length)
        return -PB_ERR_PARAM;

    rc = hash_init(hash_kind);

    if (rc != PB_OK)
        return rc;

    bpak_foreach_part(hdr, p) {
        if (!p->id)
            break;

        size_t bytes_to_copy = bpak_part_size(p);
        size_t part_offset = bpak_part_offset(hdr, p) - sizeof(struct bpak_header);

        if (part_offset > source_size || bytes_to_copy > source_size - part_offset ||
            part_offset > destination_size || bytes_to_copy > destination_size - part_offset) {
            rc = -PB_ERR_BUF_TOO_SMALL;
            break;
        }

        uintptr_t src_addr = source_address + part_offset;
        uintptr_t dst_addr = destination_address + part_offset;

        /* This could be optimized by a generic copy+hash
         * accelerator in the hash API
         */
        memcpy((void *)dst_addr, (const void *)src_addr, bytes_to_copy);

        rc = hash_update_async((void *)dst_addr, bytes_to_copy);

        if (rc != PB_OK)
            break;
    }

    if (rc == PB_OK)
        return hash_final(payload_digest, payload_digest_size);
    return rc;
}

int boot_image_verify_payload(struct bpak_header *hdr, uint8_t *payload_digest)
{
    int rc;
    size_t digest_length;

    rc = bpak_valid_header(hdr);

    if (rc != BPAK_OK) {
        return -PB_ERR_BAD_HEADER;
    }

    switch (hdr->hash_kind) {
    case BPAK_HASH_SHA256:
        digest_length = 32;
        break;
    case BPAK_HASH_SHA384:
        digest_length = 48;
        break;
    case BPAK_HASH_SHA512:
        digest_length = 64;
        break;
    default:
        return -PB_ERR_UNKNOWN_HASH;
    }

    if (memcmp(hdr->payload_hash, payload_digest, digest_length) != 0) {
        return -PB_ERR_BAD_PAYLOAD;
    }

    return PB_OK;
}
