
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
#include <pb/atf.h>
#include <pb/crc.h>
#include <pb/time.h>
#include <libfdt.h>
#include <uuid.h>
#include <bpak/bpak.h>

static uint8_t boot_state[512] __no_bss __a4k;
static uint8_t boot_state_backup[512] __no_bss __a4k;
static uuid_t primary_uu;
static uuid_t backup_uu;
static struct pb_storage_map *primary_map, *backup_map;
static struct pb_storage_driver *sdrv;
static struct pb_result result __no_bss __a4k;
static uintptr_t jump_addr;

#ifdef CONFIG_BOOT_DT
static struct pb_timestamp ts_dtb_patch = TIMESTAMP("DT Patch");
#endif

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

    if (state->magic != PB_STATE_MAGIC)
    {
        LOG_ERR("Incorrect magic");
        err = -PB_ERR;
        goto config_err_out;
    }

    if (crc != crc32(0, (uint8_t *) state, sizeof(struct pb_boot_state)))
    {
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
    struct pb_boot_state *state = (struct pb_boot_state *) boot_state;

    state->crc = 0;
    uint32_t crc = crc32(0, (const uint8_t *) boot_state,
                                sizeof(struct pb_boot_state));
    state->crc = crc;

    memcpy(boot_state_backup, boot_state, sizeof(struct pb_boot_state));

    rc = pb_storage_write(sdrv, primary_map, boot_state,
                       (sizeof(struct pb_boot_state) / sdrv->block_size), 0);

    if (rc != PB_OK)
        goto config_commit_err;

    rc = pb_storage_write(sdrv, backup_map, boot_state_backup,
                       (sizeof(struct pb_boot_state) / sdrv->block_size), 0);

config_commit_err:

    if (rc != PB_OK)
    {
        LOG_ERR("Could not write boot state");
    }
    else
    {
        LOG_INFO("Boot state written");
    }

    return rc;
}

static int pb_boot_state_init(void)
{
    int rc;
    bool primary_state_ok = false;
    bool backup_state_ok = false;
    struct pb_boot_state *state = (struct pb_boot_state *) boot_state;
    struct pb_boot_state *state_backup = (struct pb_boot_state *) boot_state_backup;

    rc = pb_storage_read(sdrv, primary_map, state,
                       (sizeof(struct pb_boot_state) / sdrv->block_size), 0);


    rc = pb_boot_state_validate(state);

    if (rc == PB_OK)
    {
        primary_state_ok = true;
    }
    else
    {
        LOG_ERR("Primary boot state data corrupt");
        primary_state_ok = false;
    }

    rc = pb_storage_read(sdrv, backup_map, state_backup,
                       (sizeof(struct pb_boot_state) / sdrv->block_size), 0);


    rc = pb_boot_state_validate(state_backup);

    if (rc == PB_OK)
    {
        backup_state_ok = true;
    }
    else
    {
        LOG_ERR("Backup boot state data corrupt");
        backup_state_ok = false;
    }

    if (!primary_state_ok && !backup_state_ok)
    {
        LOG_ERR("No valid state found, installing default");
        pb_boot_state_defaults(state);
        pb_boot_state_defaults(state_backup);
        rc = pb_boot_state_commit();
    }
    else if (!backup_state_ok && primary_state_ok)
    {
        LOG_ERR("Backup state corrupt, repairing");
        pb_boot_state_defaults(state_backup);
        rc = pb_boot_state_commit();
    }
    else if (backup_state_ok && !primary_state_ok)
    {
        LOG_ERR("Primary state corrupt, reparing");
        memcpy(state, state_backup, sizeof(struct pb_boot_state));
        rc = pb_boot_state_commit();
    }
    else
    {
        LOG_INFO("Boot state loaded");
        rc = PB_OK;
    }

    return rc;
}

int pb_boot_init(void)
{
    int rc;

#ifdef CONFIG_CALL_EARLY_PLAT_BOOT
    rc = plat_early_boot();

    if (rc != PB_OK)
        return rc;
#endif

    uuid_parse(CONFIG_BOOT_STATE_PRIMARY_UU, primary_uu);
    uuid_parse(CONFIG_BOOT_STATE_BACKUP_UU, backup_uu);

    rc = pb_storage_get_part(primary_uu, &primary_map, &sdrv);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not find primary state partition");
        return rc;
    }

    rc = pb_storage_get_part(backup_uu, &backup_map, &sdrv);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not find backup state partition");
        return rc;
    }

    rc = pb_boot_state_init();

    if (rc != PB_OK)
        return rc;

    bool commit = false;

    pb_boot_driver_load_state((struct pb_boot_state *) boot_state, &commit);

    if (commit)
    {
        rc = pb_boot_state_commit();

        if (rc != PB_OK)
            return rc;
    }


    return rc;
}

int pb_boot_load_fs(uint8_t *boot_part_uu)
{
    int rc;
    struct pb_storage_driver *stream_drv;
    struct pb_storage_map *stream_map;
    uint8_t *part_uu = NULL;
    char part_uu_str[37];

    if (!boot_part_uu)
        part_uu = pb_boot_driver_get_part_uu();
    else
        part_uu = boot_part_uu;

    rc = pb_storage_get_part(part_uu,
                             &stream_map,
                             &stream_drv);

    if (rc != PB_OK)
    {
        uuid_unparse(part_uu, part_uu_str);
        LOG_ERR("Could not find partition (%s)", part_uu_str);
        return rc;
    }

    if (!(stream_map->flags & PB_STORAGE_MAP_FLAG_BOOTABLE))
    {
        uuid_unparse(part_uu, part_uu_str);
        LOG_ERR("Partition not bootable (%s)", part_uu_str);
        return -PB_RESULT_PART_NOT_BOOTABLE;
    }

    sdrv = stream_drv;
    struct pb_storage_map *map = stream_map;

    size_t blocks = sizeof(struct bpak_header) / sdrv->block_size;
    size_t block_offset = map->no_of_blocks - blocks;

    rc = pb_storage_read(sdrv, map, pb_image_header(),
                            blocks, block_offset);

    if (rc != PB_OK)
        return rc;

    struct fs_load_private fs_priv;

    fs_priv.sdrv = sdrv;
    fs_priv.map = map;
    fs_priv.block_offset = 0;

    return pb_image_load(fs_load_read, NULL,
                         CONFIG_LOAD_FS_MAX_CHUNK_KB*1024,
                         &fs_priv);
}

int pb_boot_load_transport(void)
{
    return pb_image_load(ram_load_read, ram_load_result,
                         CONFIG_TRANSPORT_MAX_CHUNK_KB*1024,
                         NULL);
}

int pb_boot(struct pb_timestamp *ts_total, bool verbose, bool manual)
{
    int rc;
    uintptr_t *entry = 0;

    uint8_t device_uuid[16];

    struct bpak_header *h = pb_image_header();
                                       /* pb-load-addr*/
    rc = bpak_get_meta_with_ref(h, 0xd1e64a4b,
                                CONFIG_BOOT_IMAGE_ID, (void **) &entry);

    if (rc != BPAK_OK)
    {
        LOG_ERR("Could not read boot meta data");
        return -PB_ERR;
    }

    LOG_INFO("Boot entry: 0x%lx", *entry);

    jump_addr = *entry;

    plat_get_uuid((char *) device_uuid);

#ifdef CONFIG_BOOT_RAMDISK
    uintptr_t *ramdisk = 0;

    rc = bpak_get_meta_with_ref(h, 0xd1e64a4b,
                            CONFIG_BOOT_RAMDISK_ID, (void **) &ramdisk);

    if (rc == PB_OK)
    {
        LOG_INFO("Ramdisk: 0x%lx", *ramdisk);
    }
#endif

#ifdef CONFIG_BOOT_DT
    uintptr_t *dtb = 0;

    struct bpak_part_header *pdtb = NULL;

    timestamp_begin(&ts_dtb_patch);

    rc = bpak_get_part(h,
                       CONFIG_BOOT_DT_ID,
                       &pdtb);

    if (rc != BPAK_OK)
        return -PB_ERR;

    rc = bpak_get_meta_with_ref(h, 0xd1e64a4b,
                                CONFIG_BOOT_DT_ID, (void **) &dtb);

    if (rc != BPAK_OK)
        return -PB_ERR;

    LOG_INFO("DTB: 0x%lx (%x)", *dtb, CONFIG_BOOT_DT_ID);

    /* Locate the chosen node */
    void *fdt = (void *)(uintptr_t) *dtb;
    rc = fdt_check_header(fdt);

    if (rc < 0)
    {
        LOG_ERR("Invalid device tree");
        return -PB_ERR;
    }

    int depth = 0;
    int offset = 0;
    bool found_chosen_node = false;

    for (;;)
    {
        offset = fdt_next_node(fdt, offset, &depth);

        if (offset < 0)
            break;

        const char *name = fdt_get_name(fdt, offset, NULL);

        if (!name)
            continue;

        if (strcmp(name, "chosen") == 0)
        {
            found_chosen_node = true;
            break;
        }

    }

    if (!found_chosen_node)
    {
        LOG_ERR("Could not locate chosen node");
        return -PB_ERR;
    }

    char device_uuid_str[37];
    uuid_unparse(device_uuid, device_uuid_str);

    LOG_DBG("Device UUID: %s", device_uuid_str);
    fdt_setprop_string((void *) fdt, offset, "pb,device-uuid",
                (const char *) device_uuid_str);

    LOG_DBG("Updating bootargs");
    rc = plat_patch_bootargs(fdt, offset, verbose);

    if (rc != 0)
        return -PB_ERR;

    /* Update SLC related parameters in DT */
    enum pb_slc slc;

    rc = plat_slc_read(&slc);

    if (rc != PB_OK)
        return rc;

    /* SLC state */
    fdt_setprop_u32((void *) fdt, offset, "pb,slc", slc);

    /* Current key ID we're using for boot image */
    fdt_setprop_u32((void *) fdt, offset, "pb,slc-active-key", h->key_id);

    struct pb_result_slc_key_status *key_status;

    rc = plat_slc_get_key_status(&key_status);

    if (rc != PB_OK)
        return rc;

    for (unsigned int i = 0; i < membersof(key_status->active); i++)
    {
        if (key_status->active[i])
        {
            fdt_appendprop_u32((void *) fdt, offset, "pb,slc-available-keys",
                                    key_status->active[i]);
        }
    }

#ifdef CONFIG_BOOT_RAMDISK
    struct bpak_part_header *pramdisk = NULL;

    rc = bpak_get_part(h,
                       CONFIG_BOOT_RAMDISK_ID,
                       &pramdisk);

    if (rc != BPAK_OK)
    {
        LOG_ERR("Could not read ramdisk metadata");
        return -PB_ERR;
    }

    size_t ramdisk_bytes = bpak_part_size(pramdisk);

    rc = fdt_setprop_u32((void *) fdt, offset, "linux,initrd-start",
                        *ramdisk);

    if (rc)
    {
        LOG_ERR("Could not patch initrd");
        return -PB_ERR;
    }

    rc = fdt_setprop_u32((void *) fdt, offset, "linux,initrd-end",
                                             *ramdisk + ramdisk_bytes);

    if (rc)
    {
        LOG_ERR("Could not patch initrd");
        return -PB_ERR;
    }

    LOG_DBG("Ramdisk %lx -> %lx", *ramdisk, *ramdisk + ramdisk_bytes);
#endif  // CONFIG_BOOT_RAMDISK
    timestamp_end(&ts_dtb_patch);
#endif  // CONFIG_BOOT_DT

    LOG_INFO("Calling boot driver");

#ifdef CONFIG_BOOT_DT
    rc = pb_boot_driver_boot(fdt, offset);
#else
    rc = pb_boot_driver_boot(NULL, 0);
#endif  // CONFIG_BOOT_DT

    if (rc != PB_OK)
    {
        LOG_ERR("Boot driver failed");
        return rc;
    }

    LOG_DBG("Ready to jump");

    if (ts_total)
    {
        timestamp_end(ts_total);

#if (CONFIG_BOOT_DT && CONFIG_BOOT_POP_TIMING)
        rc = fdt_setprop_u32((void *) fdt, offset, "pb,boot-time",
                                                 timestamp_read_us(ts_total));

        if (rc)
        {
            LOG_ERR("Could not pb,boot-time");
            return -PB_ERR;
        }
#endif
    }

#ifdef CONFIG_DUMP_TIMING_ANALYSIS
    struct pb_timestamp *first_ts = timestamp_get_first();

    printf("--- Timing report begin ---\n\r");
    timestamp_foreach(first_ts, ts)
    {
        printf("%s %u us\n\r", timestamp_description(ts),
                                timestamp_read_us(ts));
    }

    printf("--- Timing report end ---\n\r");
#endif

#ifdef CONFIG_BOOT_DT
    size_t dtb_size = bpak_part_size(pdtb);
    arch_clean_cache_range(*dtb, dtb_size);
#endif

#ifdef CONFIG_CALL_EARLY_PLAT_BOOT
    bool abort_boot = false;

    rc = plat_late_boot(&abort_boot, manual);

    if (rc != PB_OK)
        return rc;

    if (abort_boot)
    {
        LOG_INFO("Aborting boot process");
        return PB_OK;
    }
#endif


    arch_clean_cache_range((uintptr_t) &jump_addr, sizeof(jump_addr));
    arch_disable_mmu();

#ifdef CONFIG_OVERRIDE_ARCH_JUMP
    uint8_t *part_uu = pb_boot_driver_get_part_uu();
    plat_boot_override(part_uu);
#else
#ifdef CONFIG_BOOT_ENABLE_DTB_BOOTARG
    LOG_DBG("Jumping to %p, %p",
                (void *) jump_addr,
                (void *) (*dtb));
    arch_jump((void *) jump_addr,
                NULL,
                (void *) 0xffffffff,
                (void *) (*dtb),
                NULL);
#else
    LOG_DBG("Jumping to %p", (void *) jump_addr);
    arch_jump((void *) jump_addr, NULL, NULL, NULL, NULL);
#endif  // CONFIG_BOOT_ENABLE_DTB_BOOTARG
#endif  // CONFIG_OVERRIDE_ARCH_JUMP

    LOG_ERR("Jump returned %p", (void *) jump_addr);
    return -PB_ERR;
}

int pb_boot_activate(uint8_t *uu)
{
    int rc;

    rc = pb_boot_driver_activate((struct pb_boot_state *)boot_state, uu);

    if (rc != PB_OK)
        return rc;

    return pb_boot_state_commit();
}

void pb_boot_status(char *status_msg, size_t len)
{
    struct pb_boot_state *state = (struct pb_boot_state *) boot_state;

    pb_boot_driver_status(state, status_msg, len);
}
