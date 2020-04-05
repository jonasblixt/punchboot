
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
#include <board/config.h>
#include <pb/crypto.h>
#include <pb/board.h>
#include <pb/plat.h>
#include <pb/atf.h>
#include <pb/timing_report.h>
#include <libfdt.h>
#include <uuid/uuid.h>
#include <bpak/bpak.h>

extern void arch_jump_linux_dt(volatile void* addr, volatile void * dt)
                                 __attribute__((noreturn));

extern void arch_jump_atf(void* atf_addr, void * atf_param)
                                 __attribute__((noreturn));

extern uint32_t board_linux_patch_dt(void *fdt, int offset);
static __a16b char device_uuid[37];
static __a4k __no_bss char device_uuid_raw[16];

void pb_boot_(struct bpak_header *h, uint32_t system_index, bool verbose)
{
#ifdef __NOPE
    int rc = BPAK_OK;
    uint32_t *dtb = NULL;
    uint32_t *linux = NULL;
    uint32_t *atf = NULL;
    uint32_t *tee = NULL;
    uint32_t *ramdisk = NULL;
    uint64_t ramdisk_size = 0;

    tr_stamp_begin(TR_DT_PATCH);

    rc = bpak_valid_header(h);

    if (rc != BPAK_OK)
    {
        LOG_ERR("Invalid BPAK header");
        return;
    }

    bpak_foreach_part(h, p)
    {
        if (!p->id)
            break;

        if (p->id == 0xec103b08) /* kernel */
        {
            rc = bpak_get_meta_with_ref(h, 0xd1e64a4b, p->id, (void **) &linux);
            if (rc != BPAK_OK)
                break;
        }
        else if (p->id == 0x56f91b86) /* dt */
        {
            rc = bpak_get_meta_with_ref(h, 0xd1e64a4b, p->id, (void **) &dtb);

            if (rc != BPAK_OK)
                break;
        }
        else if (p->id == 0xf4cdac1f) /* ramdisk */
        {
            rc = bpak_get_meta_with_ref(h, 0xd1e64a4b, p->id, (void **) &ramdisk);
            ramdisk_size = p->size;

            if (rc != BPAK_OK)
                break;
        }
        else if (p->id == 0x76aacab9) /* tee */
        {
            rc = bpak_get_meta_with_ref(h, 0xd1e64a4b, p->id, (void **) &tee);

            if (rc != BPAK_OK)
                break;
        }
        else if (p->id == 0xa697d988) /* atf */
        {
            rc = bpak_get_meta_with_ref(h, 0xd1e64a4b, p->id, (void **) &atf);

            if (rc != BPAK_OK)
                break;
        }

        if (rc != BPAK_OK)
        {
            LOG_ERR("Could get entry for %x", p->id);
            return;
        }
    }



    if (dtb)
        LOG_INFO("DTB 0x%x", *dtb);
    else
        LOG_INFO("No DTB");

    if (linux)
        LOG_INFO("LINUX 0x%x", *linux);
    else
        LOG_ERR("No linux kernel");

    if (atf)
        LOG_INFO("ATF: 0x%x", *atf);
    else
        LOG_INFO("ATF: None");

    if (tee)
        LOG_INFO("TEE: 0x%x", *tee);
    else
        LOG_INFO("TEE: None");

    if (ramdisk)
        LOG_INFO("RAMDISK: 0x%x", *ramdisk);
    else
        LOG_INFO("RAMDISK: None");

    LOG_DBG("Patching DT 0x%x", *dtb);

    void *fdt = (void *)(uintptr_t) *dtb;
    int err = fdt_check_header(fdt);

    if (err < 0)
    {
        LOG_ERR("Invalid device tree");
        return;
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
        return;
    }

    LOG_DBG("Patching...");

    if (ramdisk)
    {
        err = fdt_setprop_u32((void *) fdt, offset, "linux,initrd-start",
                                        *ramdisk);

        if (err)
        {
            LOG_ERR("Could not patch initrd");
            return;
        }

        err = fdt_setprop_u32((void *) fdt, offset, "linux,initrd-end",
                                                    *ramdisk + ramdisk_size);

        LOG_DBG("Ramdisk %x -> %llx", *ramdisk, *ramdisk + ramdisk_size);
    }

    if (err)
    {
        LOG_ERR("Could not patch initrd");
        return;
    }

    if (verbose)
    {
        LOG_DBG("Verbose boot %s", BOARD_BOOT_ARGS_VERBOSE);
        err = fdt_setprop_string((void *) fdt, offset,
                    "bootargs",
                    (const char *) BOARD_BOOT_ARGS_VERBOSE);
    } else {
        err = fdt_setprop_string((void *) fdt, offset,
                    "bootargs",
                    (const char *) BOARD_BOOT_ARGS);
    }

    if (err)
    {
        LOG_ERR("Could not update bootargs");
        return;
    } else {
        LOG_INFO("Bootargs patched");
    }


    plat_get_uuid(device_uuid_raw);
    uuid_to_string((uint8_t *)device_uuid_raw, device_uuid);

    LOG_DBG("Device UUID: %s", device_uuid);
    fdt_setprop_string((void *) fdt, offset, "device-uuid",
                (const char *) device_uuid);


    if (err)
        LOG_ERR("Could not update board specific params");
    else
        LOG_INFO("board params patched");

    if (system_index == SYSTEM_A)
    {
        LOG_DBG("Booting system A");
        fdt_setprop_string((void *) fdt, offset, "active-system", "A");
    }
    else if (system_index == SYSTEM_B)
    {
        LOG_DBG("Booting system B");
        fdt_setprop_string((void *) fdt, offset, "active-system", "B");
    }
    else
    {
        LOG_ERR("No active system");
        fdt_setprop_string((void *) fdt, offset, "active-system", "none");
    }

    plat_preboot_cleanup();
    tr_stamp_end(TR_TOTAL);
    tr_stamp_end(TR_DT_PATCH);

    tr_print_result();

    plat_wdog_kick();


#endif
}

int pb_boot_init(struct pb_boot_context *ctx,
                 struct pb_boot_driver *driver,
                 struct pb_storage *storage)
{
    ctx->driver = driver;
    return PB_OK;
}

int pb_boot_free(struct pb_boot_context *ctx)
{
    return PB_OK;
}

int pb_boot_set_active(struct pb_boot_context *ctx, uint8_t *uu)
{
    return PB_OK;
}

int pb_boot_is_active(struct pb_boot_context *ctx, uint8_t *uu, bool *active)
{
    return PB_OK;
}

int pb_boot(struct pb_boot_context *ctx, uint8_t *uu, bool verbose, bool force)
{
    int rc;

    rc = ctx->driver->boot(ctx->driver);

    return rc;
}
