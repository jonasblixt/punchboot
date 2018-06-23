/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <board.h>
#include <plat.h>
#include <tinyprintf.h>
#include <recovery.h>
#include <config.h>
#include <gpt.h>
#include <boot.h>
#include <board_config.h>

#include "pb_fsm.h"

#undef MAIN_DEBUG

static bool flag_run_recovery_task = false;

void pb_execute_tee_jump(void)
{
}

void pb_execute_vmm_jump(void)
{
}

void pb_execute_generic_jump(void)
{
}

void pb_set_svc_mode(void)
{
}

bool pb_has_vmm(void)
{
    return true;
}

void pb_set_hyp_mode(void)
{
}

bool pb_has_tee(void)
{
    return true;
}

void pb_inc_error_cnt(void)
{
}

void stop_recovery(void)
{
    flag_run_recovery_task = false;
}

void start_recovery(void)
{
    usb_init();
    recovery_initialize();
    flag_run_recovery_task = true;
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
    //return board_force_recovery();
    return true;
}



static void debug_transition (struct ufsm_transition *t)
{
 
    LOG_INFO2 ("    | Transition | %s {%s} --> %s {%s}\n\r", t->source->name,
                                            ufsm_state_kinds[t->source->kind],
                                            t->dest->name,
                                            ufsm_state_kinds[t->dest->kind]);
}

static void debug_enter_region(struct ufsm_region *r)
{
    LOG_INFO2 ("    | R enter    | %s, H=%i\n\r", r->name, r->has_history);
}

static void debug_leave_region(struct ufsm_region *r)
{
    LOG_INFO2 ("    | R exit     | %s, H=%i\n\r", r->name, r->has_history);
}

static void debug_event(uint32_t ev)
{
    LOG_INFO2 (" %-3li|            |\n\r",ev);
}

static void debug_action(struct ufsm_action *a)
{
    LOG_INFO2 ("    | Action     | %s()\n\r",a->name);
}

static void debug_guard(struct ufsm_guard *g, bool result) 
{
    LOG_INFO2 ("    | Guard      | %s() = %i\n\r", g->name, result);
}

static void debug_enter_state(struct ufsm_state *s)
{
    LOG_INFO2 ("    | S enter    | %s {%s}\n\r", 
                                        s->name,ufsm_state_kinds[s->kind]);
}

static void debug_exit_state(struct ufsm_state *s)
{
    LOG_INFO2 ("    | S exit     | %s {%s}\n\r", 
                                        s->name,ufsm_state_kinds[s->kind]);
}


static void debug_reset(struct ufsm_machine *m)
{
    LOG_INFO2 (" -- | RESET      | %s\n\r", m->name);
}

/*
static void print_bootmsg(uint32_t param1, int32_t param2, char bp) {
    tfp_printf("\n\rPB: " VERSION ", %lu ms, %lu - %li %c\n\r", plat_get_ms_tick(), \
                    param1, param2, bp);

}
*/

/*
static void print_board_uuid(void) {
    uint8_t board_uuid[16];

    board_get_uuid(board_uuid);

    tfp_printf ("Board UUID: ");
    for (int i = 0; i < 16; i++) {
        tfp_printf ("%2.2X", board_uuid[i]);
        if ((i == 3) || (i == 6) || (i == 8)) 
            tfp_printf ("-");
    }
    tfp_printf("\n\r");


}*/

void pb_main(void) 
{

    if (board_init() == PB_ERR) 
    {
        LOG_ERR ("Board init failed...");
        plat_reset();
    }

    LOG_INFO ("PB: " VERSION " starting...");
    struct ufsm_machine *m = get_MainMachine();

    m->debug_transition = &debug_transition;
    m->debug_enter_region = &debug_enter_region;
    m->debug_leave_region = &debug_leave_region;
    m->debug_event = &debug_event;
    m->debug_action = &debug_action;
    m->debug_guard = &debug_guard;
    m->debug_enter_state = &debug_enter_state;
    m->debug_exit_state = &debug_exit_state;
    m->debug_reset = &debug_reset;

    ufsm_init_machine(m);

    struct ufsm_queue *q = ufsm_get_queue(m);
    uint32_t ev;

    while (true)
    {
        if (ufsm_queue_get(q, &ev) == UFSM_OK)
            ufsm_process(m, ev);
        if (flag_run_recovery_task)
            usb_task();
    }
}
