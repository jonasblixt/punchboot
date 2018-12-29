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

uint32_t pb_boot_image(struct pb_pbi *pbi)
{
    uint32_t boot_count = 0;

    config_get_uint32_t(PB_CONFIG_BOOT_COUNT, &boot_count);
    boot_count = boot_count + 1;
    config_set_uint32_t(PB_CONFIG_BOOT_COUNT, boot_count);
    config_commit();
    
    board_boot(pbi);

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
