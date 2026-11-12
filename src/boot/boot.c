/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <arch/arch.h>
#include <boot/boot.h>
#include <boot/image_helpers.h>
#include <drivers/partition/static.h>
#include <inttypes.h>
#include <pb/bio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <stdbool.h>
#include <string.h>

static const struct boot_driver *boot_cfg;
static struct bpak_header header __section(".no_init") __aligned(4096);
static uint32_t boot_flags;
static bio_dev_t boot_device;
static enum boot_source boot_source;
static uuid_t boot_part_uu;
static uint8_t payload_digest[64];
static boot_read_cb_t read_cb;
static boot_result_cb_t result_cb;

int boot_init(const struct boot_driver *cfg)
{
    if (cfg == NULL)
        return -PB_ERR_PARAM;

    if (cfg->jump == NULL)
        return -PB_ERR_PARAM;

    boot_cfg = cfg;
    boot_policy_set(cfg->policy);
    boot_device = -1;
    boot_source = cfg->default_boot_source;
    boot_flags = 0;
    return PB_OK;
}

int boot_set_source(enum boot_source source)
{
    if (source <= BOOT_SOURCE_INVALID || source >= BOOT_SOURCE_END)
        return -PB_ERR_PARAM;
    boot_source = source;
    return PB_OK;
}

void boot_configure_load_cb(boot_read_cb_t read_f, boot_result_cb_t result_f)
{
    read_cb = read_f;
    result_cb = result_f;
}

uint32_t boot_get_flags(void)
{
    return boot_flags;
}

void boot_clear_set_flags(uint32_t clear_flags, uint32_t set_flags)
{
    boot_flags = ((uint32_t)boot_flags & ~clear_flags) | set_flags;
}

int boot_set_boot_partition(uuid_t part_uu)
{
    if (part_uu == NULL)
        return -PB_ERR_PARAM;
    if (!boot_cfg)
        return -PB_ERR_NOT_SUPPORTED;

    return boot_policy_activate(part_uu);
}

void boot_get_boot_partition(uuid_t part_uu)
{
    if (part_uu == NULL)
        return;

    if (!boot_cfg) {
        uuid_clear(part_uu);
        return;
    }

    boot_policy_status(part_uu);
}

static int boot_bio_read(int64_t offset, size_t length, void *buf)
{
    ssize_t block_size = bio_block_size(boot_device);

    if (block_size < 0)
        return (int)block_size;

    /* In size_t, there's no 64-bit division on 32-bit targets */
    if (offset < 0 || (uint64_t)offset > SIZE_MAX)
        return -PB_ERR_ALIGN;

    size_t byte_offset = (size_t)offset;

    if ((byte_offset % (size_t)block_size) != 0)
        return -PB_ERR_ALIGN;

    return bio_read(boot_device, (lba_t)(byte_offset / (size_t)block_size), length, buf);
}

/*
 * The mapped range of a partition that the board's table says is booted in
 * place (board.md 2.4). Only a static table has such partitions.
 */
static int in_place_range(bio_dev_t dev, uintptr_t *base, size_t *size)
{
#ifdef CONFIG_PARTITION_STATIC
    return static_ptbl_in_place(dev, base, size);
#else
    (void)dev;
    (void)base;
    (void)size;
    return -PB_ERR_NOT_FOUND;
#endif
}

/*
 * In place (boot.md 4.1): nothing is copied but the header. The parts are
 * hashed at their load addresses, which lie inside the partition's mapped
 * range, and the entry is there.
 */
static int auth_verify_in_place(uintptr_t base, size_t size)
{
    int rc;

    if (size < sizeof(header))
        return -PB_ERR_BAD_HEADER;

    /* The flash is written past the cache. What the cache holds of the
     * partition from an earlier read is dropped, so that the image is hashed
     * as it is now, which is what runs */
    arch_invalidate_cache_range(base, size);

    size -= sizeof(header);

    /* The header is at the end of the partition, read through the map. It is
     * authenticated as a copy: bpak clears the signature in it */
    memcpy(&header, (const void *)(base + size), sizeof(header));

    rc = boot_image_auth_header(&header);

    if (rc != PB_OK)
        return rc;

    rc = boot_image_verify_parts_in_place(&header, base, size);

    if (rc != PB_OK)
        return rc;

    rc = boot_image_load_and_hash(&header,
                                  CONFIG_BOOT_LOAD_CHUNK_kB * 1024,
                                  NULL, /* Nothing to read */
                                  NULL,
                                  payload_digest,
                                  sizeof(payload_digest));

    if (rc != PB_OK)
        return rc;

    return boot_image_verify_payload(&header, payload_digest);
}

static bool has_policy(void)
{
    return boot_cfg->policy != NULL && boot_cfg->policy->ops != NULL;
}

/* The candidate is the partition 'boot_part_uu' */
static int load_auth_verify_from_bio(void)
{
    int rc;
    lba_t header_lba;
    uintptr_t in_place_base;
    size_t in_place_size;

    boot_device = bio_get_part_by_uu(boot_part_uu);
    if (boot_device < 0)
        return boot_device;

#if LOG_MAX_LEVEL >= 3
    char boot_device_uu_str[37];
    uuid_unparse(bio_get_uu(boot_device), boot_device_uu_str);
    LOG_DBG("Loading image from partition %s", boot_device_uu_str);
#endif

    if (!(bio_get_flags(boot_device) & BIO_FLAG_BOOTABLE)) {
        return -PB_ERR_PART_NOT_BOOTABLE;
    }

    if (in_place_range(boot_device, &in_place_base, &in_place_size) == PB_OK)
        return auth_verify_in_place(in_place_base, in_place_size);

    /* Load header located at the end of the partition */
    header_lba = bio_get_no_of_blocks(boot_device) -
                 (sizeof(struct bpak_header) / bio_block_size(boot_device));

    rc = bio_read(boot_device, header_lba, sizeof(struct bpak_header), &header);

    if (rc != PB_OK)
        return rc;

    rc = boot_image_auth_header(&header);

    if (rc != PB_OK)
        return rc;

    rc = boot_image_verify_parts(&header);

    if (rc != PB_OK)
        return rc;

    rc = boot_image_load_and_hash(&header,
                                  CONFIG_BOOT_LOAD_CHUNK_kB * 1024,
                                  boot_bio_read,
                                  NULL, /* No result function */
                                  payload_digest,
                                  sizeof(payload_digest));

    if (rc != PB_OK)
        return rc;

    return boot_image_verify_payload(&header, payload_digest);
}

static int load_auth_verify_from_cb(void)
{
    int rc;

    if (read_cb == NULL)
        return -PB_ERR_NOT_SUPPORTED;

    rc = read_cb(-(int64_t)sizeof(struct bpak_header), sizeof(struct bpak_header), &header);
    if (rc != PB_OK)
        return rc;

    rc = boot_image_auth_header(&header);

    if (rc != PB_OK) {
        if (result_cb) {
            result_cb(rc);
        }

        return rc;
    }

    rc = boot_image_verify_parts(&header);

    if (result_cb) {
        result_cb(rc);
    }

    if (rc != PB_OK)
        return rc;

    rc = boot_image_load_and_hash(&header,
                                  CONFIG_BOOT_LOAD_CHUNK_kB * 1024,
                                  read_cb,
                                  result_cb,
                                  payload_digest,
                                  sizeof(payload_digest));

    if (rc == PB_OK)
        rc = boot_image_verify_payload(&header, payload_digest);

    if (result_cb) {
        result_cb(rc);
    }

    if (rc != PB_OK)
        return rc;

    return rc;
}

static int prepare(void)
{
    LOG_DBG("Prepare");

    if (boot_cfg->prepare)
        return boot_cfg->prepare(&header, boot_part_uu);

    return PB_OK;
}

/*
 * The board's boot source policy hands out the candidates (boot.md 2, 3).
 * Load, verify and prepare are one attempt: the policy hears its result, and
 * after a failure the next candidate is tried.
 */
static int boot_from_policy(void)
{
    uuid_t candidate_uu;
    int first_rc = PB_OK;
    bool tried = false;
    int rc;

    rc = boot_policy_next(candidate_uu);

    while (rc == PB_OK) {
        uuid_copy(boot_part_uu, candidate_uu);

        rc = load_auth_verify_from_bio();

        if (rc == PB_OK)
            rc = prepare();

        boot_policy_report(candidate_uu, rc);

        if (rc == PB_OK)
            return PB_OK;

        LOG_ERR("The candidate failed (%i)", rc);

        if (!tried) {
            first_rc = rc;
            tried = true;
        }

        rc = boot_policy_next(candidate_uu);
    }

    /* Every candidate failed: the first one's error */
    if (tried)
        return first_rc;

    /* The policy had none, or its state couldn't be written */
    if (rc == -PB_ERR_NOT_FOUND)
        rc = -PB_ERR_NO_ACTIVE_BOOT_PARTITION;

    return rc;
}

int boot_load(uuid_t boot_part_override_uu)
{
    int rc;

    if (!boot_cfg)
        return -PB_ERR_PARAM;

    if (boot_cfg->jump == NULL) {
        rc = -PB_ERR_NOT_SUPPORTED;
        goto err_out;
    }

    LOG_DBG("Early boot");

    if (boot_cfg->early_boot_cb) {
        rc = boot_cfg->early_boot_cb();
        if (rc != PB_OK) {
            goto err_out;
        }
    }

    uuid_clear(boot_part_uu);

    if (boot_part_override_uu != NULL) {
        uuid_copy(boot_part_uu, boot_part_override_uu);
    }

    LOG_DBG("Load");
    switch (boot_source) {
    case BOOT_SOURCE_BIO:
        if (!has_policy()) {
            /* Only a policy's candidates are bootable */
            rc = -PB_ERR_NOT_SUPPORTED;
        } else if (uuid_is_null(boot_part_uu)) {
            rc = boot_from_policy();
        } else {
            /* One candidate, the partition that was named: the policy
             * isn't asked and hears nothing */
            rc = load_auth_verify_from_bio();

            if (rc == PB_OK)
                rc = prepare();
        }
        break;
    case BOOT_SOURCE_CB:
        rc = load_auth_verify_from_cb();

        if (rc == PB_OK)
            rc = prepare();
        break;
    default:
        rc = -PB_ERR_PARAM;
        goto err_out;
    }

    if (rc == -PB_ERR_NO_ACTIVE_BOOT_PARTITION) {
        printf("No active boot partition\n\r");
    } else if (rc != PB_OK) {
        LOG_ERR("Load/Auth/Verify failed (%i)", rc);
    }

err_out:
    /* The flags stay set for boot_jump, late_boot checks BOOT_FLAG_CMD */
    if (rc != PB_OK)
        boot_flags = 0;
    return rc;
}

int boot_jump(void)
{
    int rc;

    LOG_DBG("Late boot");
    if (boot_cfg->late_boot_cb) {
        rc = boot_cfg->late_boot_cb(&header, boot_part_uu);
        if (rc != PB_OK) {
            boot_flags = 0;
            return rc;
        }
    }

    rc = plat_lockdown();

    if (rc != PB_OK) {
        LOG_ERR("Lockdown failed (%i)", rc);
        boot_flags = 0;
        return rc;
    }

    LOG_DBG("Jump");
    boot_cfg->jump();
    boot_flags = 0;
    return -PB_ERR;
}

int boot(uuid_t boot_part_override_uu)
{
    int rc;
    rc = boot_load(boot_part_override_uu);
    if (rc != PB_OK)
        return rc;

    return boot_jump();
}
