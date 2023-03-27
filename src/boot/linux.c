/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <inttypes.h>
#include <pb/pb.h>
#include <arch/arch.h>
#include <pb/arch.h>
#include <pb/plat.h>
#include <bpak/bpak.h>
#include <bpak/id.h>
#include <boot/linux.h>
#include <libfdt.h>
#include <device_uuid.h>

static const struct boot_driver_linux_config *cfg;
static uintptr_t jump_addr;
static uintptr_t ramdisk_addr;
static uintptr_t dtb_addr;

int boot_driver_linux_init(const struct boot_driver_linux_config *cfg_in)
{
    cfg = cfg_in;

    if (cfg->image_bpak_id == 0)
        return -PB_ERR_PARAM;

    return PB_OK;
}

int boot_driver_linux_prepare(struct bpak_header *hdr, uuid_t boot_part_uu)
{
    int rc;
    uuid_t device_uu;
    enum pb_slc slc;
    char device_uu_str[37];
    int depth;
    int offset;
    bool found_chosen_node;
    void *fdt;
    size_t dtb_length;
    size_t ramdisk_length;
    struct pb_result_slc_key_status *key_status;
    struct bpak_part_header *ph;
    struct bpak_meta_header *mh;

    rc = bpak_get_meta(hdr,
                        BPAK_ID_PB_LOAD_ADDR,
                        cfg->image_bpak_id,
                        &mh);

    if (rc != BPAK_OK) {
        LOG_ERR("Could not read boot image meta data (%i)", rc);
        return -PB_ERR_BAD_META;
    }

    jump_addr = (uintptr_t) *bpak_get_meta_ptr(hdr, mh, uint64_t);

    LOG_INFO("Boot entry: 0x%" PRIxPTR, jump_addr);

    device_uuid(device_uu);
    uuid_unparse(device_uu, device_uu_str);

    if (cfg->ramdisk_bpak_id) {
        rc = bpak_get_meta(hdr,
                            BPAK_ID_PB_LOAD_ADDR,
                            cfg->ramdisk_bpak_id,
                            &mh);

        ramdisk_addr = (uintptr_t) *bpak_get_meta_ptr(hdr, mh, uint64_t);

        if (rc == PB_OK) {
            LOG_INFO("Ramdisk: %" PRIxPTR, ramdisk_addr);
        } else {
            LOG_ERR("Could not read ramdisk load addr (%i)", rc);
            return -PB_ERR_BAD_META;
        }
    }

    if (cfg->dtb_bpak_id) {
        rc = bpak_get_part(hdr, cfg->dtb_bpak_id, &ph);

        if (rc != BPAK_OK) {
            LOG_ERR("Could not read dtb bpak part meta");
            return -PB_ERR_BAD_META;
        }

        rc = bpak_get_meta(hdr,
                            BPAK_ID_PB_LOAD_ADDR,
                            cfg->dtb_bpak_id,
                            &mh);

        if (rc != BPAK_OK) {
            LOG_ERR("Could not read dtb load addr meta");
            return -PB_ERR_BAD_META;
        }

        dtb_addr = (uintptr_t) *bpak_get_meta_ptr(hdr, mh, uint64_t);
        dtb_length = bpak_part_size(ph);
        LOG_INFO("DTB: %" PRIxPTR, dtb_addr);

        /* Locate the chosen node */
        fdt = (void *) dtb_addr;
        rc = fdt_check_header(fdt);

        if (rc < 0) {
            LOG_ERR("Invalid device tree");
            return -PB_ERR;
        }

        depth = 0;
        offset = 0;
        found_chosen_node = false;

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

        rc = fdt_setprop_string((void *) fdt, offset, "pb,device-uuid",
                    (const char *) device_uu_str);

        if (rc != 0) {
            LOG_ERR("fdt error: device-uuid (%i)", rc);
            return -1;
        }

        if (cfg->dtb_patch_cb) {
            cfg->dtb_patch_cb(fdt, offset);

            if (rc != PB_OK) {
                LOG_ERR("Patch bootargs error (%i)", rc);
                return rc;
            }
        }

        /* Update SLC related parameters in DT */
        rc = plat_slc_read(&slc);

        if (rc != PB_OK)
            return rc;

        /* SLC state */
        rc = fdt_setprop_u32((void *) fdt, offset, "pb,slc", slc);

        if (rc != 0) {
            LOG_ERR("fdt error: slc (%i)", rc);
            return -PB_ERR;
        }

        /* Current key ID we're using for boot image */
        rc = fdt_setprop_u32((void *) fdt, offset, "pb,slc-active-key",
                                hdr->key_id);

        if (rc != 0) {
            LOG_ERR("fdt error: active-key (%i)", rc);
            return -PB_ERR;
        }

        rc = plat_slc_get_key_status(&key_status);

        if (rc != PB_OK)
            return rc;

        rc = fdt_delprop((void *) fdt, offset, "pb,slc-available-keys");

        if (rc != 0) {
            LOG_ERR("fdt error: del available keys (%i)", rc);
            return -1;
        }

        for (size_t i = 0; i < membersof(key_status->active); i++) {
            if (key_status->active[i]) {
                rc = fdt_appendprop_u32((void *) fdt, offset,
                            "pb,slc-available-keys", key_status->active[i]);

                if (rc != 0) {
                    LOG_ERR("fdt error: available keys (%i)", rc);
                    return -1;
                }
            }
        }

        if (cfg->ramdisk_bpak_id) {
            rc = bpak_get_part(hdr,
                               cfg->ramdisk_bpak_id,
                               &ph);

            if (rc != BPAK_OK) {
                LOG_ERR("Could not read ramdisk metadata");
                return -PB_ERR_BAD_META;
            }

            ramdisk_length = bpak_part_size(ph);

            rc = fdt_setprop_u32((void *) fdt, offset, "linux,initrd-start",
                                ramdisk_addr);

            if (rc != 0) {
                LOG_ERR("fdt error: ramdisk (%i)", rc);
                return -PB_ERR;
            }

            rc = fdt_setprop_u32((void *) fdt, offset, "linux,initrd-end",
                                                 ramdisk_addr + ramdisk_length);

            if (rc) {
                LOG_ERR("Could not patch initrd");
                return -PB_ERR;
            }

            LOG_DBG("Ramdisk %"PRIxPTR" -> %"PRIxPTR, ramdisk_addr,
                                                 ramdisk_addr + ramdisk_length);
        }

        if (cfg->resolve_part_name) {
            rc = fdt_setprop_string(fdt, offset, "pb,active-system",
                                       cfg->resolve_part_name(boot_part_uu));
            if (rc != PB_OK) {
                LOG_ERR("fdt error: active-system (%i)", rc);
                return -1;
            }
        }
    }

    if (cfg->dtb_bpak_id) {
        arch_clean_cache_range(dtb_addr, dtb_length);
    }

    return PB_OK;
}

void boot_driver_linux_jump(void)
{
#ifdef CONFIG_PRINT_TIMESTAMPS
    ts_print();
#endif
    arch_clean_cache_range((uintptr_t) &jump_addr, sizeof(jump_addr));
    arch_disable_mmu();

    LOG_DBG("Jumping to %" PRIxPTR, jump_addr);
    if (cfg->set_dtb_boot_arg)
        arch_jump((void *) jump_addr, (void *) dtb_addr, NULL, NULL, NULL);
    else
        arch_jump((void *) jump_addr, NULL, NULL, NULL, NULL);

    LOG_ERR("Jump returned %" PRIxPTR, jump_addr);
}
