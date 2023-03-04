
/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb/pb.h>
#include <pb/io.h>
#include <pb/boot.h>
#include <pb/arch.h>
#include <pb/board.h>
#include <pb/plat.h>
#include <pb/crc.h>
#include <pb/timestamp.h>
#include <libfdt.h>
#include <uuid.h>
#include <bpak/bpak.h>

extern char _code_start, _code_end,
            _data_region_start, _data_region_end,
            _zero_region_start, _zero_region_end,
            _stack_start, _stack_end, _big_buffer_start, _big_buffer_end;

extern struct bpak_keystore keystore_pb;

static struct bpak_header header __a4k __no_bss;
static uint8_t signature[512] __a4k __no_bss;
static size_t signature_sz;
static uint8_t hash[PB_HASH_MAX_LENGTH];
static struct pb_boot_state boot_state, boot_state_backup;
static struct pb_storage_map *primary_map, *backup_map;
static struct pb_storage_driver *sdrv;
static struct pb_result result;
static uuid_t active_uu;

typedef int (*pb_image_read_t) (void *buf, size_t size, void *private);
typedef int (*pb_image_result_t) (int rc, void *private);

static int check_header(void)
{
    int err = PB_OK;
    struct bpak_header *h = &header;
    struct bpak_meta_header *mh;
    err = bpak_valid_header(h);

    if (err != BPAK_OK) {
        LOG_ERR("Invalid header");
        return -PB_ERR;
    }

    bpak_foreach_part(h, p) {
        if (!p->id)
            break;

        size_t sz = bpak_part_size(p);
        uint64_t *load_addr = NULL;

        err = bpak_get_meta(h, BPAK_ID_PB_LOAD_ADDR, p->id, &mh);

        if (err != BPAK_OK) {
            LOG_ERR("Could not read pb-entry for part %x", p->id);
            break;
        }

        load_addr = bpak_get_meta_ptr(h, mh, uint64_t);

        uint64_t la = *load_addr;

        if (PB_CHECK_OVERLAP(la, sz, &_stack_start, &_stack_end)) {
            err = -PB_ERR;
            LOG_ERR("image overlapping with PB stack");
        }

        if (PB_CHECK_OVERLAP(la, sz, &_data_region_start, &_data_region_end)) {
            err = -PB_ERR;
            LOG_ERR("image overlapping with PB data");
        }

        if (PB_CHECK_OVERLAP(la, sz, &_zero_region_start, &_zero_region_end)) {
            err = -PB_ERR;
            LOG_ERR("image overlapping with PB bss");
        }

        if (PB_CHECK_OVERLAP(la, sz, &_code_start, &_code_end)) {
            err = -PB_ERR;
            LOG_ERR("image overlapping with PB code");
        }

        if (PB_CHECK_OVERLAP(la, sz, &_no_init_start, &_no_init_end)) {
            err = -PB_ERR;
            LOG_ERR("image overlapping with PB buffer");
        }
    }

    return err;
}

static int image_load(pb_image_read_t read_f,
                  pb_image_result_t result_f,
                  size_t load_chunk_size,
                  void *priv)
{
    int rc;
    struct bpak_header *h = &header;
    struct bpak_meta_header *mh;
    struct bpak_key *k = NULL;
    int hash_kind;

    pb_timestamp_begin("Image load");

    rc = bpak_valid_header(h);

    if (rc != BPAK_OK) {
        LOG_ERR("Invalid BPAK header");
        return PB_ERR;
    }

    LOG_DBG("Key-store: %x", h->keystore_id);
    LOG_DBG("Key-ID: %x", h->key_id);

    if (h->keystore_id != keystore_pb.id) {
        LOG_ERR("Invalid key-store");
        return -PB_ERR;
    }

    for (int i = 0; i < keystore_pb.no_of_keys; i++) {
        if (keystore_pb.keys[i]->id == h->key_id) {
            k = keystore_pb.keys[i];
            break;
        }
    }

    if (!k) {
        LOG_ERR("Key not found");
        return -PB_ERR;
    }

    bool active = false;

    rc = plat_slc_key_active(h->key_id, &active);

    if (rc != PB_OK)
        return rc;

    if (!active) {
        LOG_ERR("Invalid or revoked key (%x)", h->key_id);
        return -PB_ERR;
    }

    size_t hash_sz = 0;

    switch (h->hash_kind) {
        case BPAK_HASH_SHA256:
            hash_kind = PB_HASH_SHA256;
            hash_sz = 32;
        break;
        case BPAK_HASH_SHA384:
            hash_kind = PB_HASH_SHA384;
            hash_sz = 48;
        break;
        case BPAK_HASH_SHA512:
            hash_kind = PB_HASH_SHA512;
            hash_sz = 64;
        break;
        default:
            LOG_ERR("Unknown hash_kind value 0x%x", h->hash_kind);
            rc = -PB_ERR;
    }

    if (rc != PB_OK)
        return rc;

    rc = plat_hash_init(hash_kind);

    if (rc != PB_OK)
        return rc;

    signature_sz = sizeof(signature);

    rc = bpak_copyz_signature(h, signature, &signature_sz);

    if (rc != PB_OK) {
        LOG_ERR("Invalid signature area: size=%d", h->signature_sz);
        return rc;
    }

    rc = plat_hash_update((uint8_t *) h, sizeof(*h));

    if (rc != PB_OK)
        return rc;

    rc = plat_hash_output(hash, sizeof(hash));

    if (rc != PB_OK)
        return rc;

    pb_timestamp_begin("Verify signature");

    rc = plat_pk_verify(signature, signature_sz, hash, hash_kind, k);

    if (rc == PB_OK) {
        LOG_INFO("Signature Valid");
    } else {
        LOG_ERR("Signature Invalid");
        return rc;
    }

    pb_timestamp_end();

    rc = check_header();

    if (rc != PB_OK)
        return rc;

    if (result_f) {
        rc = result_f(rc, priv);

        if (rc != PB_OK)
            return rc;
    }

    /* Compute payload hash */

    rc = plat_hash_init(hash_kind);

    if (rc != PB_OK)
        return rc;

    uint64_t *load_addr = NULL;

    bpak_foreach_part(h, p) {
        if (!p->id)
            break;

        load_addr = NULL;
        rc = bpak_get_meta(h, BPAK_ID_PB_LOAD_ADDR, p->id, &mh);

        if (rc != BPAK_OK) {
            LOG_ERR("Could not read pb-entry for part %x", p->id);
            break;
        }

        load_addr = bpak_get_meta_ptr(h, mh, uint64_t);

        LOG_DBG("Loading part %x --> %p, %zu bytes", p->id,
                                (void *)(uintptr_t) (*load_addr),
                                bpak_part_size(p));
        LOG_DBG("Part offset: %llu", p->offset);

        size_t bytes_to_read = bpak_part_size(p);
        size_t chunk_size = 0;
        size_t offset = 0;

        while (bytes_to_read) {
            chunk_size = (bytes_to_read>load_chunk_size)? \
                            load_chunk_size:bytes_to_read;

            uintptr_t addr = ((*load_addr) + offset);

            rc = read_f((void *) addr, chunk_size, priv);

            if (rc != PB_OK)
                break;

            rc = plat_hash_update((uint8_t *) addr, chunk_size);

            if (rc != PB_OK)
                break;

            bytes_to_read -= chunk_size;
            offset += chunk_size;
        }

        if (result_f) {
            rc = result_f(rc, priv);

            if (rc != PB_OK)
                return rc;
        }
    }

    if (rc != PB_OK) {
        LOG_ERR("Loading failed");
        return rc;
    }

    rc = plat_hash_output(hash, sizeof(hash));

    if (rc != PB_OK)
        return rc;

    if (memcmp(h->payload_hash, hash, hash_sz) != 0) {
        LOG_ERR("Payload hash incorrect");
        return -1;
    }

    pb_timestamp_end();

    return rc;
}

static int ram_load_read(void *buf, size_t size, void *private)
{
    LOG_DBG("Read %zu bytes", size);
    return plat_transport_read(buf, size);
}

static int ram_load_result(int rc, void *private)
{
    pb_wire_init_result(&result, rc);
    return plat_transport_write(&result, sizeof(result));
}

struct fs_load_private
{
    struct pb_storage_driver *sdrv;
    struct pb_storage_map *map;
    size_t block_offset;
};

static int fs_load_read(void *buf, size_t size, void *private)
{
    struct fs_load_private *priv = (struct fs_load_private *) private;
    int rc;
    size_t blocks = size / priv->sdrv->block_size;

    LOG_DBG("Read %zu blocks", blocks);

    rc = pb_storage_read(priv->sdrv, priv->map, buf,
                            blocks, priv->block_offset);

    priv->block_offset += blocks;
    return rc;
}

static int pb_boot_state_validate(struct pb_boot_state *state)
{
    uint32_t crc = state->crc;
    int err = PB_OK;

    state->crc = 0;

    if (state->magic != PB_STATE_MAGIC) {
        LOG_ERR("Incorrect magic");
        err = -PB_ERR;
        goto config_err_out;
    }

    if (crc != crc32(0, (uint8_t *) state, sizeof(struct pb_boot_state))) {
        LOG_ERR("CRC failed");
        err =- PB_ERR;
        goto config_err_out;
    }

config_err_out:
    return err;
}

static int pb_boot_state_defaults(struct pb_boot_state *state)
{
    memset(state, 0, sizeof(struct pb_boot_state));
    state->magic = PB_STATE_MAGIC;
    return PB_OK;
}

static int pb_boot_state_commit(void)
{
    int rc;

    boot_state.crc = 0;
    uint32_t crc = crc32(0, (const uint8_t *) &boot_state,
                                sizeof(struct pb_boot_state));
    boot_state.crc = crc;

    rc = pb_storage_write(sdrv, primary_map, &boot_state,
                       (sizeof(struct pb_boot_state) / sdrv->block_size), 0);

    if (rc != PB_OK)
        goto config_commit_err;

    rc = pb_storage_write(sdrv, backup_map, &boot_state,
                       (sizeof(struct pb_boot_state) / sdrv->block_size), 0);

config_commit_err:
    if (rc != PB_OK) {
        LOG_ERR("Could not write boot state");
    } else {
        LOG_INFO("Boot state written");
    }

    return rc;
}

static int pb_boot_state_init(void)
{
    int rc;
    bool primary_state_ok = false;
    bool backup_state_ok = false;

    rc = pb_storage_read(sdrv, primary_map, &boot_state,
                       (sizeof(struct pb_boot_state) / sdrv->block_size), 0);

    rc = pb_boot_state_validate(&boot_state);

    if (rc == PB_OK) {
        primary_state_ok = true;
    } else {
        LOG_ERR("Primary boot state data corrupt");
        primary_state_ok = false;
    }

    (void) pb_storage_read(sdrv, backup_map, &boot_state_backup,
                       (sizeof(struct pb_boot_state) / sdrv->block_size), 0);

    rc = pb_boot_state_validate(&boot_state_backup);

    if (rc == PB_OK) {
        backup_state_ok = true;
    } else {
        LOG_ERR("Backup boot state data corrupt");
        backup_state_ok = false;
    }

    if (!primary_state_ok && !backup_state_ok) {
        LOG_ERR("No valid state found, installing default");
        pb_boot_state_defaults(&boot_state);
        pb_boot_state_defaults(&boot_state_backup);
        rc = pb_boot_state_commit();
    } else if (!backup_state_ok && primary_state_ok) {
        LOG_ERR("Backup state corrupt, repairing");
        pb_boot_state_defaults(&boot_state_backup);
        rc = pb_boot_state_commit();
    } else if (backup_state_ok && !primary_state_ok) {
        LOG_ERR("Primary state corrupt, reparing");
        memcpy(&boot_state, &boot_state_backup, sizeof(struct pb_boot_state));
        rc = pb_boot_state_commit();
    } else {
        LOG_INFO("Boot state loaded");
        rc = PB_OK;
    }

    return rc;
}

int pb_boot_init(void)
{
    int rc;
    const char *active_uu_str;
    uuid_t primary_uu;
    uuid_t backup_uu;
    const struct pb_boot_config *boot_config = board_boot_config();

    rc = plat_early_boot();

    if (rc == -PB_ERR_ABORT) {
        LOG_INFO("Early boot, Aborting boot process");
        return PB_OK;
    } else if (rc < 0) {
        LOG_ERR("Boot early cb error (%i)", rc);
        return rc;
    }

    uuid_parse(boot_config->primary_state_part_uuid, primary_uu);
    uuid_parse(boot_config->backup_state_part_uuid, backup_uu);

    rc = pb_storage_get_part(primary_uu, &primary_map, &sdrv);

    if (rc != PB_OK) {
        LOG_ERR("Could not find primary state partition");
        return rc;
    }

    rc = pb_storage_get_part(backup_uu, &backup_map, &sdrv);

    if (rc != PB_OK) {
        LOG_ERR("Could not find backup state partition");
        return rc;
    }

    rc = pb_boot_state_init();

    if (rc != PB_OK)
        return rc;

    bool commit = false;

    LOG_DBG("A/B boot load state %u %u %u", boot_state.enable,
                                            boot_state.verified,
                                            boot_state.error);

    if (boot_state.enable & PB_STATE_A_ENABLED) {
        if (!(boot_state.verified & PB_STATE_A_VERIFIED) &&
             boot_state.remaining_boot_attempts > 0) {
            boot_state.remaining_boot_attempts--;
            commit = true; /* Update state data */
            active_uu_str = boot_config->a_boot_part_uuid;
        } else if (!(boot_state.verified & PB_STATE_A_VERIFIED)) {
            LOG_ERR("Rollback to B system");
            if (!(boot_state.verified & PB_STATE_B_VERIFIED)) {
                if (boot_config->rollback_mode == PB_ROLLBACK_MODE_SPECULATIVE) {
                    // Enable B
                    // Reset boot counter to one
                    commit = true;
                    boot_state.enable = PB_STATE_B_ENABLED;
                    boot_state.remaining_boot_attempts = 1;
                    boot_state.error = PB_STATE_ERROR_A_ROLLBACK;
                    active_uu_str = boot_config->b_boot_part_uuid;
                } else {
                    LOG_ERR("B system not verified, failing");
                    return -PB_ERR;
                }
            } else {
                commit = true;
                boot_state.enable = PB_STATE_B_ENABLED;
                boot_state.error = PB_STATE_ERROR_A_ROLLBACK;
                active_uu_str = boot_config->b_boot_part_uuid;
            }
        } else {
            active_uu_str = boot_config->a_boot_part_uuid;
        }
    } else if (boot_state.enable & PB_STATE_B_ENABLED) {
        if (!(boot_state.verified & PB_STATE_B_VERIFIED) &&
             boot_state.remaining_boot_attempts > 0) {
            boot_state.remaining_boot_attempts--;
            commit = true; /* Update state data */
            active_uu_str = boot_config->b_boot_part_uuid;
        } else if (!(boot_state.verified & PB_STATE_B_VERIFIED)) {
            LOG_ERR("Rollback to A system");
            if (!(boot_state.verified & PB_STATE_A_VERIFIED)) {
                if (boot_config->rollback_mode == PB_ROLLBACK_MODE_SPECULATIVE) {
                    // Enable A
                    // Reset boot counter to one
                    commit = true;
                    boot_state.enable = PB_STATE_A_ENABLED;
                    boot_state.remaining_boot_attempts = 1;
                    boot_state.error = PB_STATE_ERROR_B_ROLLBACK;
                    active_uu_str = boot_config->a_boot_part_uuid;
                } else {
                    LOG_ERR("A system not verified, failing");
                    return -PB_ERR;
                }
            }

            commit = true;
            boot_state.enable = PB_STATE_A_ENABLED;
            boot_state.error = PB_STATE_ERROR_B_ROLLBACK;
            active_uu_str = boot_config->a_boot_part_uuid;
        } else {
            active_uu_str = boot_config->b_boot_part_uuid;
        }
    } else {
        active_uu_str = NULL;
    }

    if (!active_uu_str) {
        LOG_INFO("No active system");
        active_uu_str = NULL;
        memset(active_uu, 0, 16);
    } else {
        uuid_parse(active_uu_str, active_uu);
    }

    if (commit) {
        rc = pb_boot_state_commit();

        if (rc != PB_OK)
            return rc;
    }

    return rc;
}

static int load_boot_image_from_part(void)
{
    int rc;
    struct pb_storage_driver *stream_drv;
    struct pb_storage_map *stream_map;
    char part_uu_str[37];

    rc = pb_storage_get_part(active_uu,
                             &stream_map,
                             &stream_drv);

    if (rc != PB_OK) {
        uuid_unparse(active_uu, part_uu_str);
        LOG_ERR("Could not find partition (%s)", part_uu_str);
        return rc;
    }

    if (!(stream_map->flags & PB_STORAGE_MAP_FLAG_BOOTABLE)) {
        uuid_unparse(active_uu, part_uu_str);
        LOG_ERR("Partition not bootable (%s)", part_uu_str);
        return -PB_RESULT_PART_NOT_BOOTABLE;
    }

    sdrv = stream_drv;
    struct pb_storage_map *map = stream_map;

    size_t blocks = sizeof(struct bpak_header) / sdrv->block_size;
    size_t block_offset = map->no_of_blocks - blocks;

    rc = pb_storage_read(sdrv, map, &header,
                            blocks, block_offset);

    if (rc != PB_OK)
        return rc;

    struct fs_load_private fs_priv;

    fs_priv.sdrv = sdrv;
    fs_priv.map = map;
    fs_priv.block_offset = 0;

    return image_load(fs_load_read, NULL,
                         CONFIG_LOAD_FS_MAX_CHUNK_KB*1024,
                         &fs_priv);
}

static int load_boot_image_over_transport(void)
{
    int rc;

    rc = plat_transport_read(&header, sizeof(struct bpak_header));

    if (rc != PB_OK)
        return rc;

    rc = bpak_valid_header(&header);

    if (rc != BPAK_OK) {
        LOG_ERR("Invalid BPAK header");
        return rc;
    }

    return image_load(ram_load_read, ram_load_result,
                         CONFIG_TRANSPORT_MAX_CHUNK_KB*1024,
                         NULL);
}

int pb_boot_load(enum pb_boot_source boot_source, uuid_t boot_part_uu)
{
    if (boot_part_uu) {
        memcpy(active_uu, boot_part_uu, sizeof(uuid_t));
    }

    switch (boot_source) {
    case PB_BOOT_SOURCE_BLOCK_DEV:
        return load_boot_image_from_part();
    case PB_BOOT_SOURCE_TRANSPORT:
        return load_boot_image_over_transport();
    default:
        LOG_ERR("Unknown boot source (%i)", boot_source);
        return -PB_ERR;
    }
}

int pb_boot(enum pb_boot_mode boot_mode, bool verbose)
{
    int rc;
    const struct pb_boot_config *boot_config = board_boot_config();
    uuid_t device_uuid;
    uintptr_t *entry = 0;
    uintptr_t *ramdisk = 0;
    uintptr_t *dtb = 0;
    struct bpak_part_header *pdtb = NULL;
    struct bpak_header *h = &header;
    struct bpak_meta_header *mh;
    static uintptr_t jump_addr;

    rc = bpak_get_meta(h,
                        BPAK_ID_PB_LOAD_ADDR,
                        boot_config->image_bpak_id,
                        &mh);

    if (rc != BPAK_OK) {
        LOG_ERR("Could not read boot image meta data (%i)", rc);
        return -PB_ERR;
    }

    entry = bpak_get_meta_ptr(h, mh, uintptr_t);

    LOG_INFO("Boot entry: 0x%lx", *entry);

    jump_addr = *entry;
    plat_get_uuid((char *) device_uuid);

    if (boot_config->ramdisk_bpak_id) {
        rc = bpak_get_meta(h,
                            BPAK_ID_PB_LOAD_ADDR,
                            boot_config->ramdisk_bpak_id,
                            &mh);

        ramdisk = bpak_get_meta_ptr(h, mh, uintptr_t);

        if (rc == PB_OK) {
            LOG_INFO("Ramdisk: %p", ramdisk);
        } else {
            LOG_ERR("Could not read ramdisk load addr (%i)", rc);
            return -1;
        }
    }

    if (boot_config->dtb_bpak_id) {
        pb_timestamp_begin("DT Patch");
        rc = bpak_get_part(h, boot_config->dtb_bpak_id, &pdtb);

        if (rc != BPAK_OK) {
            LOG_ERR("Could not read dtb bpak part meta");
            return -PB_ERR;
        }

        rc = bpak_get_meta(h,
                            BPAK_ID_PB_LOAD_ADDR,
                            boot_config->dtb_bpak_id,
                            &mh);

        if (rc != BPAK_OK) {
            LOG_ERR("Could not read dtb load addr meta");
            return -PB_ERR;
        }

        dtb = bpak_get_meta_ptr(h, mh, uintptr_t);
        LOG_INFO("DTB: %p (%x)", dtb, boot_config->dtb_bpak_id);

        /* Locate the chosen node */
        void *fdt = (void *)(uintptr_t) *dtb;
        rc = fdt_check_header(fdt);

        if (rc < 0) {
            LOG_ERR("Invalid device tree");
            return -PB_ERR;
        }

        int depth = 0;
        int offset = 0;
        bool found_chosen_node = false;

        for (;;) {
            offset = fdt_next_node(fdt, offset, &depth);

            if (offset < 0)
                break;

            const char *name = fdt_get_name(fdt, offset, NULL);

            if (!name)
                continue;

            if (strcmp(name, "chosen") == 0) {
                found_chosen_node = true;
                break;
            }
        }

        if (!found_chosen_node) {
            LOG_ERR("Could not locate chosen node");
            return -PB_ERR;
        }

        char device_uuid_str[37];
        uuid_unparse(device_uuid, device_uuid_str);

        LOG_DBG("Device UUID: %s", device_uuid_str);
        rc = fdt_setprop_string((void *) fdt, offset, "pb,device-uuid",
                    (const char *) device_uuid_str);

        if (rc != 0) {
            LOG_ERR("fdt error: device-uuid (%i)", rc);
            return -1;
        }

        LOG_DBG("Updating bootargs");
        rc = plat_patch_bootargs(fdt, offset, verbose);

        if (rc != 0) {
            LOG_ERR("Patch bootargs error (%i)", rc);
            return -1;
        }

        /* Update SLC related parameters in DT */
        enum pb_slc slc;

        rc = plat_slc_read(&slc);

        if (rc != PB_OK)
            return rc;

        /* SLC state */
        rc = fdt_setprop_u32((void *) fdt, offset, "pb,slc", slc);

        if (rc != 0) {
            LOG_ERR("fdt error: slc (%i)", rc);
            return -1;
        }

        /* Current key ID we're using for boot image */
        rc = fdt_setprop_u32((void *) fdt, offset, "pb,slc-active-key", h->key_id);

        if (rc != 0) {
            LOG_ERR("fdt error: active-key (%i)", rc);
            return -1;
        }

        struct pb_result_slc_key_status *key_status;

        rc = plat_slc_get_key_status(&key_status);

        if (rc != PB_OK)
            return rc;

        rc = fdt_delprop((void *) fdt, offset, "pb,slc-available-keys");

        if (rc != 0) {
            LOG_ERR("fdt error: del available keys (%i)", rc);
            return -1;
        }

        for (unsigned int i = 0; i < membersof(key_status->active); i++) {
            if (key_status->active[i]) {
                rc = fdt_appendprop_u32((void *) fdt, offset,
                            "pb,slc-available-keys", key_status->active[i]);

                if (rc != 0) {
                    LOG_ERR("fdt error: available keys (%i)", rc);
                    return -1;
                }
            }
        }

        if (boot_config->ramdisk_bpak_id) {
            struct bpak_part_header *pramdisk = NULL;

            rc = bpak_get_part(h,
                               boot_config->ramdisk_bpak_id,
                               &pramdisk);

            if (rc != BPAK_OK) {
                LOG_ERR("Could not read ramdisk metadata");
                return -PB_ERR;
            }

            size_t ramdisk_bytes = bpak_part_size(pramdisk);

            rc = fdt_setprop_u32((void *) fdt, offset, "linux,initrd-start",
                                *ramdisk);

            if (rc != 0) {
                LOG_ERR("fdt error: ramdisk (%i)", rc);
                return -1;
            }

            rc = fdt_setprop_u32((void *) fdt, offset, "linux,initrd-end",
                                                     *ramdisk + ramdisk_bytes);

            if (rc) {
                LOG_ERR("Could not patch initrd");
                return -PB_ERR;
            }

            LOG_DBG("Ramdisk %lx -> %lx", *ramdisk, *ramdisk + ramdisk_bytes);
        }

        char uu_str[37];
        uuid_unparse(active_uu, uu_str);

        if (strcmp(uu_str, boot_config->a_boot_part_uuid) == 0) {
            rc = fdt_setprop_string(fdt, offset, "pb,active-system", "A");
        } else if (strcmp(uu_str, boot_config->b_boot_part_uuid) == 0) {
            rc = fdt_setprop_string(fdt, offset, "pb,active-system", "B");
        } else {
            rc = fdt_setprop_string(fdt, offset, "pb,active-system", "?");
        }

        if (rc != PB_OK) {
            LOG_ERR("fdt error: active-system (%i)", rc);
            return -1;
        }

        pb_timestamp_end();  // DT Patch
    }

    LOG_DBG("Ready to jump");
    pb_timestamp_end(); // Total

    if (boot_config->print_time_measurements) {
        pb_timestamp_print();
    }

    if (boot_config->dtb_bpak_id) {
        size_t dtb_size = bpak_part_size(pdtb);
        arch_clean_cache_range(*dtb, dtb_size);
    }

    rc = plat_late_boot(active_uu, boot_mode);

    if (rc == -PB_ERR_ABORT) {
        LOG_INFO("Late boot, Aborting boot process");
        return PB_OK;
    } else if (rc < 0) {
        LOG_ERR("Boot late cb error (%i)", rc);
        return rc;
    }

    arch_clean_cache_range((uintptr_t) &jump_addr, sizeof(jump_addr));
    arch_disable_mmu();

    LOG_DBG("Jumping to %p", (void *) jump_addr);
    if (boot_config->set_dtb_boot_arg)
        arch_jump((void *) jump_addr, (void *) (*dtb), NULL, NULL, NULL);
    else
        arch_jump((void *) jump_addr, NULL, NULL, NULL, NULL);

    LOG_ERR("Jump returned %p", (void *) jump_addr);
    return -PB_ERR;
}

int pb_boot_activate(uuid_t uu)
{
    const struct pb_boot_config *boot_config = board_boot_config();
    char uu_str[37];

    uuid_unparse(uu, uu_str);

    LOG_DBG("Activating: %s", uu_str);

    if (strcmp(uu_str, boot_config->a_boot_part_uuid) == 0) {
        boot_state.enable = PB_STATE_A_ENABLED;
        boot_state.verified = PB_STATE_A_VERIFIED;
        boot_state.error = 0;
    } else if (strcmp(uu_str, boot_config->b_boot_part_uuid) == 0) {
        boot_state.enable = PB_STATE_B_ENABLED;
        boot_state.verified = PB_STATE_B_VERIFIED;
        boot_state.error = 0;
    } else if (strcmp(uu_str, "00000000-0000-0000-0000-000000000000") == 0) {
        LOG_INFO("Disable boot partition");
        boot_state.enable = 0;
        boot_state.verified = 0;
        boot_state.error = 0;
    } else {
        LOG_ERR("Invalid boot partition");
        boot_state.enable = 0;
        boot_state.verified = 0;
        boot_state.error = 0;
        return -PB_ERR;
    }

    memcpy(active_uu, uu, 16);
    return pb_boot_state_commit();
}

int pb_boot_read_active_part(uuid_t out)
{
    memcpy(out, active_uu, sizeof(uuid_t));
    return PB_OK;
}
