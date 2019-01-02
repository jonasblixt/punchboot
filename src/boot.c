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

void pb_boot_linux_with_dt(struct pb_pbi *pbi)
{

    struct pb_component_hdr *dtb = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_DT);

    struct pb_component_hdr *linux = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_LINUX);

    LOG_INFO(" LINUX %lX, DTB %lX", linux->load_addr_low, dtb->load_addr_low);
    
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

uint32_t pb_boot_image(struct pb_pbi *pbi)
{
    uint32_t boot_count = 0;

    config_get_uint32_t(PB_CONFIG_BOOT_COUNT, &boot_count);
    boot_count = boot_count + 1;
    config_set_uint32_t(PB_CONFIG_BOOT_COUNT, boot_count);
    config_commit();
 
    plat_wdog_kick();

    PB_BOOT_FUNC(pbi);

    return PB_OK;
}

uint32_t pb_boot_load_part(uint8_t boot_part, struct pb_pbi **pbi)
{
    uint32_t err = PB_OK;
    uint32_t boot_lba_offset = 0;

    switch (boot_part)
    {
        case 0xAA:
        {
            LOG_INFO ("Loading from system A");
            err = gpt_get_part_by_uuid(part_type_system_a, &boot_lba_offset);
        }
        break;
        case 0xBB:
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


