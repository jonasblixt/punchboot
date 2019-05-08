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


extern void arch_jump_linux_dt(void* addr, void * dt)
                                 __attribute__ ((noreturn));

extern void arch_jump_atf(void* atf_addr, void * atf_param)
                                 __attribute__ ((noreturn));

extern uint32_t board_linux_patch_dt (void *fdt, int offset);
static char new_bootargs[512];
static __a16b char device_uuid[37];
static __a4k __no_bss char device_uuid_raw[16];

void pb_boot(struct pb_pbi *pbi, uint32_t system_index, bool verbose)
{

    __a16b char part_uuid[37];
    char *part_uuid_raw = NULL;

    struct pb_component_hdr *dtb = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_DT);

    struct pb_component_hdr *linux2 = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_LINUX);

    struct pb_component_hdr *atf = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_ATF);
    
    struct pb_component_hdr *ramdisk = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_RAMDISK);

    tr_stamp_begin(TR_DT_PATCH);

    if (dtb && linux2 && atf)
    {
        LOG_INFO(" LINUX %x, DTB %x", linux2->load_addr_low, 
                                              dtb->load_addr_low);
    } 
    else
    {
        LOG_ERR("Can't boot image");
        return;
    }

    if (atf)
        LOG_INFO(" ATF: %x",atf->load_addr_low);
    else
        LOG_INFO(" ATF: None");

    if (ramdisk)
        LOG_INFO(" RAMDISK: %x",ramdisk->load_addr_low);
    else
        LOG_INFO(" RAMDISK: None");


    switch (system_index)
    {
        case SYSTEM_A:
        {
            LOG_INFO ("Using root A");
            part_uuid_raw = (char *) PB_PARTUUID_ROOT_A;
        }
        break;
        case SYSTEM_B:
        {
            LOG_INFO ("Using root B");
            part_uuid_raw = (char *) PB_PARTUUID_ROOT_B;
        }
        break;
        default:
        {
            LOG_ERR("Invalid root partition %x", system_index);
            return ;
        }
    }

    uuid_to_string((unsigned char *) part_uuid_raw, part_uuid);
    LOG_INFO("UUID = %s", part_uuid);
    LOG_DBG("Patching DT");
    void *fdt = (void *)(uintptr_t) dtb->load_addr_low;
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
                
                if (ramdisk)
                {
                    err = fdt_setprop_u32( (void *) fdt, offset, 
                                        "linux,initrd-start",
                                        ramdisk->load_addr_low);

                    if (err)
                    {
                        LOG_ERR("Could not patch initrd");
                        return;
                    }

                    err = fdt_setprop_u32( (void *) fdt, offset, 
                        "linux,initrd-end",
                        ramdisk->load_addr_low+ramdisk->component_size);

                    if (err)
                    {
                        LOG_ERR("Could not patch initrd");
                        return;
                    }

                    if (verbose)
                    {
                        err = fdt_setprop_string( (void *) fdt, offset, 
                                    "bootargs", 
                                    (const char *) BOARD_BOOT_ARGS_VERBOSE);
                    } else {
                        err = fdt_setprop_string( (void *) fdt, offset, 
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

                } else {

                    if (verbose)
                    {
                        snprintf (new_bootargs, 512, BOARD_BOOT_ARGS_VERBOSE, 
                                                            part_uuid);
                    } else {
                        snprintf (new_bootargs, 512, BOARD_BOOT_ARGS, 
                                                            part_uuid);
                    }

                    err = fdt_setprop_string( (void *) fdt, offset, "bootargs", 
                                (const char *) new_bootargs);

                    if (err)
                    {
                        LOG_ERR("Could not update bootargs");
                        return;
                    } else {
                        LOG_INFO("Bootargs patched");
                    }
                }

                plat_get_uuid(device_uuid_raw);
                uuid_to_string((uint8_t *)device_uuid_raw,device_uuid);
                LOG_DBG("Device UUID: %s", device_uuid);
                fdt_setprop_string( (void *) fdt, offset, "device-uuid", 
                            (const char *) device_uuid);


                err = board_linux_patch_dt(fdt, offset);

                if (err)
                {
                    LOG_ERR("Could not update board specific params");
                } else {
                    LOG_INFO("board params patched");
                }

                if (system_index == SYSTEM_A)
                    fdt_setprop_string( (void *) fdt, offset, "active-system", "A");
                else if(system_index == SYSTEM_B)
                    fdt_setprop_string( (void *) fdt, offset, "active-system", "B");
                else
                    fdt_setprop_string( (void *) fdt, offset, "active-system", "none");
                break;
            }
        }
    }

    LOG_DBG("Done");
    plat_preboot_cleanup();
    tr_stamp_end(TR_TOTAL);
    tr_stamp_end(TR_DT_PATCH);

    tr_print_result();

    plat_wdog_kick();

    if (atf && dtb && linux2)
    {
        LOG_DBG("ATF boot");
        arch_jump_atf((void *)(uintptr_t) atf->load_addr_low, 
                      (void *)(uintptr_t) NULL);
    } 
    else if(dtb && linux2)
    {
        arch_jump_linux_dt((void *)(uintptr_t) linux2->load_addr_low, 
                           (void *)(uintptr_t) dtb->load_addr_low);
    }

    while(1)
        __asm__ ("nop");
}

