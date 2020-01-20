
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb.h>
#include <io.h>
#include <boot.h>
#include <gpt.h>
#include <image.h>
#include <board/config.h>
#include <crypto.h>
#include <board.h>
#include <plat.h>
#include <atf.h>
#include <timing_report.h>
#include <libfdt.h>
#include <uuid.h>
#include <bpak/bpak.h>

extern void arch_jump_linux_dt(volatile void* addr, volatile void * dt)
                                 __attribute__((noreturn));

extern void arch_jump_atf(void* atf_addr, void * atf_param)
                                 __attribute__((noreturn));

extern uint32_t board_linux_patch_dt(void *fdt, int offset);
static __a16b char device_uuid[37];
static __a4k __no_bss char device_uuid_raw[16];

void pb_boot(struct bpak_header *h, uint32_t system_index, bool verbose)
{
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

    if (err >= 0)
    {
        int depth = 0;
        int offset = 0;

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
                LOG_DBG("Patching...");
                err = fdt_setprop_u32((void *) fdt, offset,
                                    "linux,initrd-start",
                                    *ramdisk);

                if (err)
                {
                    LOG_ERR("Could not patch initrd");
                    return;
                }

                err = fdt_setprop_u32((void *) fdt, offset,
                    "linux,initrd-end", *ramdisk + ramdisk_size);

                LOG_DBG("Ramdisk %x -> %llx", *ramdisk, *ramdisk + ramdisk_size);
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


                err = board_linux_patch_dt(fdt, offset);

                if (err)
                    LOG_ERR("Could not update board specific params");
                else
                    LOG_INFO("board params patched");

                if (system_index == SYSTEM_A)
                {
                    LOG_DBG("Booting system A");
                    fdt_setprop_string((void *) fdt, offset,
                                                        "active-system", "A");
                }
                else if (system_index == SYSTEM_B)
                {
                    LOG_DBG("Booting system B");
                    fdt_setprop_string((void *) fdt, offset,
                                                        "active-system", "B");
                }
                else
                {
                    LOG_ERR("No active system");
                    fdt_setprop_string((void *) fdt, offset,
                                                      "active-system", "none");
                }
                break;
            }
        }
    }

    plat_preboot_cleanup();
    tr_stamp_end(TR_TOTAL);
    tr_stamp_end(TR_DT_PATCH);

    tr_print_result();

    plat_wdog_kick();


    if (atf && dtb && linux)
    {
        LOG_DBG("ATF boot");
        arch_jump_atf((void *)(uintptr_t) *atf,
                      (void *)(uintptr_t) NULL);
    }
    else if(dtb && linux && !atf && tee)
    {
        LOG_INFO("TEE boot");
        arch_jump_linux_dt((void *)(uintptr_t) *tee,
                           (void *)(uintptr_t) *dtb);
    }
    else if(dtb && linux)
    {
        arch_jump_linux_dt((void *)(uintptr_t) *linux,
                           (void *)(uintptr_t) *dtb);
    }

    while (1)
        __asm__("nop");
}

