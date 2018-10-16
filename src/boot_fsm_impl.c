#include <pb.h>
#include <plat.h>
#include <gpt.h>
#include <config.h>
#include <pb_image.h>
#include <tinyprintf.h>
#include "pb_fsm.h"



void load_sys_a(void)
{
    struct ufsm_machine *m = get_MainMachine();
    struct ufsm_queue *q = ufsm_get_queue(m);
    uint32_t lba_offset;

    if (gpt_get_part_by_uuid(part_type_system_a, &lba_offset) != PB_OK)
        ufsm_queue_put(q, EV_ERROR);

    if (pb_image_load_from_fs(lba_offset) != PB_OK)
         ufsm_queue_put(q, EV_ERROR);
}

void load_sys_b(void)
{
    struct ufsm_machine *m = get_MainMachine();
    struct ufsm_queue *q = ufsm_get_queue(m);
    uint32_t lba_offset;

    if (gpt_get_part_by_uuid(part_type_system_b, &lba_offset) != PB_OK)
        ufsm_queue_put(q, EV_ERROR);

    if (pb_image_load_from_fs(lba_offset) != PB_OK)
         ufsm_queue_put(q, EV_ERROR);
}

void validate_image(void)
{
    struct ufsm_machine *m = get_MainMachine();
    struct ufsm_queue *q = ufsm_get_queue(m);
 
    if (pb_verify_image(pb_get_image(),0) == PB_OK)
        ufsm_queue_put(q, EV_VERIFIED);
    else
        ufsm_queue_put(q, EV_ERROR);
}

bool boot_from_sys_a(void)
{
    uint32_t boot_part = 0;

    if (config_get_uint32_t(PB_CONFIG_BOOT, &boot_part) != PB_OK)
        return false;

    return (boot_part == 0xAA);
}

bool boot_from_sys_b(void)
{
    uint32_t boot_part = 0;

    if (config_get_uint32_t(PB_CONFIG_BOOT, &boot_part) != PB_OK)
        return false;

    return (boot_part == 0xBB);
}

