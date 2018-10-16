#include <pb.h>
#include <plat.h>
#include <gpt.h>
#include <config.h>
#include <board.h>

#include "pb_fsm.h"

static bool flag_run_recovery_task = false;

void pb_inc_error_cnt(void)
{
}


void pb_reset(void)
{
    plat_reset();
}

void pb_stop_counter(void)
{
}

void pb_start_counter(void)
{

}

void pb_init_fs(void)
{
    struct ufsm_machine *m = get_MainMachine();
    struct ufsm_queue *q = ufsm_get_queue(m);

    if (gpt_init() != PB_OK)
        ufsm_queue_put(q, EV_ERROR);
    else
        ufsm_queue_put(q, EV_OK);
 
}

void pb_init_config(void)
{
    struct ufsm_machine *m = get_MainMachine();
    struct ufsm_queue *q = ufsm_get_queue(m);

    if (config_init() != PB_OK)
        ufsm_queue_put(q, EV_ERROR);
    else
        ufsm_queue_put(q, EV_OK);
 
}

void pb_dflt_cfg(void)
{
}

void pb_dflt_gpt(void)
{
}

bool pb_max_boot_cnt(void)
{
    return false;
}

bool pb_force_recovery(void)
{
    return board_force_recovery();
}

