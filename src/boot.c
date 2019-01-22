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
#include <arch.h>

static struct atf_bl31_params bl31_params;
static struct entry_point_info bl33_ep;
static struct atf_image_info bl33_image;

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
        LOG_INFO(" LINUX %lX, DTB %lX", linux->load_addr_low, dtb->load_addr_low);
    
    if (atf)
        LOG_INFO("  ATF: 0x%8.8X", atf->load_addr_low);

    volatile uint32_t atf_addr = atf->load_addr_low;
    volatile uint32_t dtb_addr = dtb->load_addr_low;
    volatile uint32_t linux_addr = linux->load_addr_low;

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
    bl33_ep.pc = (uintptr_t) linux_addr;
    bl33_ep.args.arg0 = dtb->load_addr_low;
    bl33_ep.args.arg1 = 0;

    bl33_image.h.type = PARAM_IMAGE_BINARY;
    bl33_image.h.version = 1;
    bl33_image.h.size = sizeof(struct atf_image_info);
    bl33_image.h.attr = 1;

    bl33_image.image_base = (uintptr_t) linux_addr;
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

    uint32_t ts0 = plat_get_us_tick();

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
                char new_bootargs[256];
            
                
                tfp_sprintf (new_bootargs, "console=ttymxc0,115200 " \
                    "earlycon=ec_imx6q,0x30860000,115200 earlyprintk " \
                    "cma=768M " \
                    "root=PARTUUID=%s " \
                    "rw rootfstype=ext4 gpt rootwait", part_uuid);

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

    uint32_t ts1 = plat_get_us_tick();
    tfp_printf ("%lus %lus\n\r",ts0, ts1);

    arch_jump(linux_addr, 0, 0xFFFFFFFF, dtb_addr, 0);

    /*
    asm volatile(  "mov x4, %0" "\n\r"
                   "mov x0, %1" "\n\r"
                   "br x4" "\n\r"
                    :
                    : "r" (atf_addr),
                      "r" (&bl31_params));
*/
/* TODO: This must be moved to ARCH-code

    asm volatile(   "mov r0, #0" "\n\r"
                    "mov r1, #0xFFFFFFFF" "\n\r"
                    "mov r2, %0" "\n\r"
                    :
                    : "r" (dtb_addr));

    asm volatile(  "mov pc, %0" "\n\r"
                    :
                    : "r" (linux_addr));
*/

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


