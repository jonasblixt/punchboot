#include <stdio.h>
#include <pb.h>
#include <io.h>
#include <boot.h>
#include <gpt.h>
#include <image.h>
#include <board/config.h>
#include <keys.h>
#include <board.h>
#include <plat.h>
#include <uuid.h>
#include <atf.h>
#include <timing_report.h>

static struct atf_bl31_params bl31_params;
static struct entry_point_info bl33_ep;
static struct atf_image_info bl33_image;

extern void arch_jump_linux_dt(void* addr, void * dt)
                                 __attribute__ ((noreturn));

extern void arch_jump_atf(void* atf_addr, void * atf_param)
                                 __attribute__ ((noreturn));

void pb_boot_linux_with_dt(struct pb_pbi *pbi)
{

    struct pb_component_hdr *dtb = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_DT);

    struct pb_component_hdr *linux2 = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_LINUX);

    struct pb_component_hdr *atf = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_ATF);

    tr_stamp_begin(TR_FINAL);

    if (dtb && linux2)
    {
        LOG_INFO(" LINUX %x, DTB %x", linux2->load_addr_low, dtb->load_addr_low);
    }

    if (atf)
    {
        LOG_INFO("  ATF: 0x%x", atf->load_addr_low);
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
    bl33_ep.pc = (uintptr_t) linux2->load_addr_low;
    bl33_ep.args.arg0 = dtb->load_addr_low;
    bl33_ep.args.arg1 = 0;

    bl33_image.h.type = PARAM_IMAGE_BINARY;
    bl33_image.h.version = 1;
    bl33_image.h.size = sizeof(struct atf_image_info);
    bl33_image.h.attr = 1;

    bl33_image.image_base = (uintptr_t) linux2->load_addr_low;
    bl33_image.image_size = linux2->component_size;

    tr_stamp_end(TR_FINAL);
    tr_print_result();

    plat_wdog_kick();

    if (atf)
    {
        arch_jump_atf((void *)(uintptr_t) atf->load_addr_low, 
                      (void *)(uintptr_t) &bl31_params);
    } else {
        arch_jump_linux_dt((void *)(uintptr_t) linux2->load_addr_low, 
                           (void *)(uintptr_t) dtb->load_addr_low);
    }

    while(1)
        __asm__ ("nop");
}

