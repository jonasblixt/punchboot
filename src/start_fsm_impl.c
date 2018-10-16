#include <pb.h>
#include <tinyprintf.h>
#include <pb_image.h>
#include "pb_fsm.h"

typedef int32_t bootfunc(void);

static bootfunc *_tee_boot_entry = NULL;
static bootfunc *_vmm_boot_entry = NULL;
 
bool pb_has_tee(void)
{
    struct pb_pbi *pbi = pb_get_image();

    for (uint32_t i = 0; i < pbi->hdr.no_of_components; i++) 
    {
        if (pbi->comp[i].component_type == PB_IMAGE_COMPTYPE_TEE)
        {
            LOG_INFO ("Found TEE component");
            _tee_boot_entry = (bootfunc *) pbi->comp[i].load_addr_low;
            return true;
        }
    }

    return false;
}

bool pb_has_vmm(void)
{

    struct pb_pbi *pbi = pb_get_image();

    for (uint32_t i = 0; i < pbi->hdr.no_of_components; i++) 
    {
        if (pbi->comp[i].component_type == PB_IMAGE_COMPTYPE_VMM)
        {
            _vmm_boot_entry = (bootfunc *) pbi->comp[i].load_addr_low;
            LOG_INFO ("Found VMM component");
            return true;
        }
    }

    return false;
}

bool pb_boot_atf(void)
{
    return false;
}


void pb_execute_atf_jump(void)
{
}

extern void _ns_reentry(void);

void pb_execute_tee_jump(void)
{

    LOG_INFO("Starting TEE %lX, ns reentry: %lX",_tee_boot_entry, _ns_reentry);

    asm volatile("ldr   lr, =_ns_reentry" "\n\r" // Jump to TEE
                 "mov   pc, %0" "\n\r"
                    : 
                    : "r" (_tee_boot_entry));


    struct ufsm_machine *m = get_MainMachine();
    struct ufsm_queue *q = ufsm_get_queue(m);
    
    ufsm_queue_put(q, EV_ERROR);
}

void pb_execute_vmm_jump(void)
{

    LOG_INFO("Starting VMM %lX", _vmm_boot_entry);

    asm volatile("mov   pc, %0" "\n\r"
                    : 
                    : "r" (_vmm_boot_entry));

}

void pb_execute_generic_jump(void)
{
}

void pb_set_svc_mode(void)
{
}

void pb_set_hyp_mode(void)
{
}


