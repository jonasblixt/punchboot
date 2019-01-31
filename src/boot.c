#include <pb.h>
#include <boot.h>
#include <config.h>
#include <gpt.h>
#include <image.h>
#include <tinyprintf.h>
#include <board/config.h>
#include <keys.h>
#include <board.h>
#include <plat.h>
#include <libfdt.h>
#include <uuid.h>
#include <atf.h>
#include <timing_report.h>
#include <inttypes.h>

static struct atf_bl31_params bl31_params;
static struct entry_point_info bl33_ep;
static struct atf_image_info bl33_image;

extern void arch_jump_linux_dt(void* addr, void * dt)
                                 __attribute__ ((noreturn));

extern void arch_jump_atf(void* atf_addr, void * atf_param)
                                 __attribute__ ((noreturn));

void pb_boot_linux_with_dt(struct pb_pbi *pbi, uint8_t system_index)
{

    struct pb_component_hdr *dtb = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_DT);

    struct pb_component_hdr *linux = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_LINUX);

    struct pb_component_hdr *atf = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_ATF);

    char part_uuid[37];

    if (dtb && linux)
    {
        LOG_INFO(" LINUX %"PRIx32", DTB %"PRIx32, linux->load_addr_low, dtb->load_addr_low);
    }

    if (atf)
    {
        LOG_INFO("  ATF: 0x%8.8"PRIx32, atf->load_addr_low);
    }

    /* Parameter struct for ATF BL31 */
    bl31_params.h.type = PARAM_BL_PARAMS;
    bl31_params.h.version = 1;
    bl31_params.h.size = sizeof(struct atf_bl31_params);
    bl31_params.h.attr = 1;

    bl31_params.bl31_image_info = NULL;
    bl31_params.bl32_ep_info = NULL;
    bl31_params.bl31_image_info = NULL;
    bl31_params.bl33_ep_info = &bl33_ep;
    bl31_params.bl33_image_info = &bl33_image;

    bl33_ep.h.type = PARAM_EP;
    bl33_ep.h.size = sizeof(struct entry_point_info);
    bl33_ep.h.version = 1;
    bl33_ep.h.attr = 1;
    bl33_ep.spsr = 0x000003C9;
    bl33_ep.pc = (uintptr_t) linux->load_addr_low;
    bl33_ep.args.arg0 = dtb->load_addr_low;
    bl33_ep.args.arg1 = 0;

    bl33_image.h.type = PARAM_IMAGE_BINARY;
    bl33_image.h.version = 1;
    bl33_image.h.size = sizeof(struct atf_image_info);
    bl33_image.h.attr = 1;

    bl33_image.image_base = (uintptr_t) linux->load_addr_low;
    bl33_image.image_size = linux->component_size;

    switch (system_index)
    {
        case SYSTEM_A:
        {
            LOG_INFO ("Using root A");
            uuid_to_string((uint8_t *) part_type_root_a, part_uuid);
        }
        break;
        case SYSTEM_B:
        {
            LOG_INFO ("Using root B");
            uuid_to_string((uint8_t *) part_type_root_b, part_uuid);
        }
        break;
        default:
        {
            LOG_ERR("Invalid root partition %x", system_index);
            return ;
        }
    }


    const void *fdt = (void *)(uintptr_t) dtb->load_addr_low;

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
                char new_bootargs[256];
            
                board_configure_bootargs(new_bootargs, part_uuid);

                err = fdt_setprop_string( (void *) fdt, offset, "bootargs", 
                            (const char *) new_bootargs);

                if (err)
                {
                    LOG_ERR("Could not update bootargs");
                }
                
                break;
            }
        }
    }

    tr_stamp(TR_FINAL);
    tr_print_result();

    if (atf)
    {
        arch_jump_atf((void *)(uintptr_t) atf->load_addr_low, 
                      (void *)(uintptr_t) &bl31_params);
    } else {
        arch_jump_linux_dt((void *)(uintptr_t) linux->load_addr_low, 
                           (void *)(uintptr_t) dtb->load_addr_low);
    }

    while(1);
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

    err = pb_image_verify(*pbi);

   
    return err;
}


