#include <pb.h>
#include <boot.h>
#include <config.h>
#include <gpt.h>
#include <image.h>
#include <tinyprintf.h>
#include <board_config.h>
#include <keys.h>
#include <board.h>
#include <plat.h>
#include <board_config.h>
#include <libfdt.h>
#include <uuid.h>

void pb_boot_linux_with_dt(struct pb_pbi *pbi, uint8_t system_index)
{

    struct pb_component_hdr *dtb = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_DT);

    struct pb_component_hdr *linux = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_LINUX);

    unsigned char part_uuid[37];

    LOG_INFO(" LINUX %lX, DTB %lX", linux->load_addr_low, dtb->load_addr_low);
    
    switch (system_index)
    {
        case SYSTEM_A:
        {
            LOG_INFO ("Using root A");
            uuid_to_string(part_type_root_a, part_uuid);
        }
        break;
        case SYSTEM_B:
        {
            LOG_INFO ("Using root B");
            uuid_to_string(part_type_root_b, part_uuid);
        }
        break;
        default:
        {
            LOG_ERR("Invalid root partition %x", system_index);
            return PB_ERR;
        }
    }

    const void *fdt = (void *) dtb->load_addr_low;

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

            /* get the name */
            const char *name = fdt_get_name(fdt, offset, NULL);
            if (!name)
                continue;

            if (strcmp(name, "chosen") == 0) 
            {
                /* A: 3F85291C-C6FB-42D0-9E1A-AC6B3560C304 */
                /* B: 3F85292C-C6FB-42D0-9E1A-AC6B3560C304 */
                unsigned char new_bootargs[128];
            
                
                tfp_sprintf (new_bootargs, "console=ttymxc1,115200 " \
                    "root=PARTUUID=%s " \
                    "rw rootfstype=ext4 gpt rootwait quiet", part_uuid);

                err = fdt_setprop_string(fdt, offset, "bootargs", new_bootargs);

                if (err)
                {
                    LOG_ERR("Could not update bootargs");
                }
                
                break;
            }
        }
    }

    volatile uint32_t dtb_addr = dtb->load_addr_low;
    volatile uint32_t linux_addr = linux->load_addr_low;


    asm volatile(   "mov r0, #0" "\n\r"
                    "mov r1, #0xFFFFFFFF" "\n\r"
                    "mov r2, %0" "\n\r"
                    :
                    : "r" (dtb_addr));

    asm volatile(  "mov pc, %0" "\n\r"
                    :
                    : "r" (linux_addr));
}

uint32_t pb_boot_image(struct pb_pbi *pbi, uint8_t system_index)
{
    uint32_t boot_count = 0;


    config_get_uint32_t(PB_CONFIG_BOOT_COUNT, &boot_count);
    boot_count = boot_count + 1;
    config_set_uint32_t(PB_CONFIG_BOOT_COUNT, boot_count);
    config_commit();
 
    plat_wdog_kick();

    PB_BOOT_FUNC(pbi, system_index);

    return PB_OK;
}

uint32_t pb_boot_load_part(uint8_t boot_part, struct pb_pbi **pbi)
{
    uint32_t err = PB_OK;
    uint32_t boot_lba_offset = 0;

    switch (boot_part)
    {
        case SYSTEM_A:
        {
            LOG_INFO ("Loading from system A");
            err = gpt_get_part_by_uuid(part_type_system_a, &boot_lba_offset);
        }
        break;
        case SYSTEM_B:
        {
            LOG_INFO ("Loading from system B");
            err = gpt_get_part_by_uuid(part_type_system_b, &boot_lba_offset);
        }
        break;
        default:
        {
            LOG_ERR("Invalid boot partition %x", boot_part);
            err = PB_ERR;
        }
    }

    if (err != PB_OK)
        return err;

    err = pb_image_load_from_fs(boot_lba_offset, pbi);
    
    if (err != PB_OK)
    {
        LOG_ERR("Unable to load image");
        return err;
    }

    err = pb_image_verify(*pbi, PB_KEY_DEV);

   
    return err;
}


