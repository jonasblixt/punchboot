/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <boot/boot.h>
#include <boot/image_helpers.h>
#include <inttypes.h>
#include <pb/bio.h>
#include <pb/pb.h>
#include <pb/timestamp.h>
#include <string.h>

static const struct boot_driver *boot_cfg;
static struct bpak_header header __section(".no_init") __aligned(4096);
static struct bpak_header *header_ptr = &header;
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
    if (!boot_cfg || boot_cfg->set_boot_partition == NULL)
        return -PB_ERR_NOT_SUPPORTED;

    return boot_cfg->set_boot_partition(part_uu);
}

void boot_get_boot_partition(uuid_t part_uu)
{
    if (part_uu == NULL)
        return;

    if (!boot_cfg || boot_cfg->get_boot_partition == NULL) {
        uuid_clear(part_uu);
        return;
    }

    boot_cfg->get_boot_partition(part_uu);
}

static int boot_bio_read(int block_offset, size_t length, void *buf)
{
    return bio_read(boot_device, block_offset, length, buf);
}

static int load_auth_verify_from_bio(void)
{
    int rc;
    lba_t header_lba;

    if (boot_cfg->get_boot_bio_device == NULL)
        return -PB_ERR_NOT_SUPPORTED;

    if (uuid_is_null(boot_part_uu)) {
        boot_device = boot_cfg->get_boot_bio_device();

        if (boot_device < 0)
            return boot_device;

        uuid_copy(boot_part_uu, bio_get_uu(boot_device));
    } else {
        boot_device = bio_get_part_by_uu(boot_part_uu);
        if (boot_device < 0)
            return boot_device;
    }

#if (LOGLEVEL > 1)
    char boot_device_uu_str[37];
    uuid_unparse(bio_get_uu(boot_device), boot_device_uu_str);
    LOG_DBG("Loading image from partition %s", boot_device_uu_str);
#endif

    if (!(bio_get_flags(boot_device) & BIO_FLAG_BOOTABLE)) {
        return -PB_ERR_PART_NOT_BOOTABLE;
    }

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

static int auth_verify_in_mem(void)
{
    int rc;

    if (boot_cfg->get_in_mem_image == NULL)
        return -PB_ERR_NOT_SUPPORTED;

    rc = boot_cfg->get_in_mem_image(&header_ptr);

    if (rc != PB_OK)
        return rc;

    rc = boot_image_auth_header(header_ptr);

    if (rc != PB_OK)
        return rc;

    rc = boot_image_verify_parts(header_ptr);

    if (rc != PB_OK)
        return rc;

    rc = boot_image_load_and_hash(header_ptr,
                                  CONFIG_BOOT_LOAD_CHUNK_kB * 1024,
                                  NULL,
                                  NULL,
                                  payload_digest,
                                  sizeof(payload_digest));

    if (rc != PB_OK)
        return rc;

    return boot_image_verify_payload(header_ptr, payload_digest);
}

static int load_auth_verify_from_cb(void)
{
    int rc;

    if (read_cb == NULL)
        return -PB_ERR_NOT_SUPPORTED;

    rc = read_cb(-(int)sizeof(struct bpak_header) / 512, sizeof(struct bpak_header), &header);
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

    if (rc != PB_OK)
        return rc;

    rc = boot_image_verify_payload(&header, payload_digest);

    if (result_cb) {
        result_cb(rc);
    }

    if (rc != PB_OK)
        return rc;

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

    ts("Boot early");

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

    ts("Boot load");
    switch (boot_source) {
    case BOOT_SOURCE_BIO:
        rc = load_auth_verify_from_bio();
        break;
    case BOOT_SOURCE_IN_MEM:
        rc = auth_verify_in_mem();
        break;
    case BOOT_SOURCE_CB:
        rc = load_auth_verify_from_cb();
        break;
    case BOOT_SOURCE_CUSTOM:
        if (boot_cfg->authenticate_image)
            rc = boot_cfg->authenticate_image(&header_ptr);
        else
            rc = -PB_ERR_NOT_SUPPORTED;
        break;
    default:
        rc = -PB_ERR_PARAM;
        goto err_out;
    }

    if (rc == -PB_ERR_NO_ACTIVE_BOOT_PARTITION) {
        printf("No active boot partition\n\r");
        goto err_out;
    } else if (rc != PB_OK) {
        LOG_ERR("Load/Auth/Verify failed (%i)", rc);
        goto err_out;
    }

    ts("Boot prepare");
    if (boot_cfg->prepare) {
        rc = boot_cfg->prepare(header_ptr, boot_part_uu);
        if (rc != PB_OK) {
            goto err_out;
        }
    }

err_out:
    boot_flags = 0;
    return rc;
}

int boot_jump(void)
{
    int rc;

    ts("Boot late");
    if (boot_cfg->late_boot_cb) {
        rc = boot_cfg->late_boot_cb(header_ptr, boot_part_uu);
        if (rc != PB_OK) {
            return rc;
        }
    }

    ts("Boot jump");
    boot_cfg->jump();
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
