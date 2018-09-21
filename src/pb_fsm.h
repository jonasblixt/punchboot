#ifndef UFSM_GEN_pb_fsm_H__
#define UFSM_GEN_pb_fsm_H__
#include <ufsm.h>
#ifndef NULL
 #define NULL (void *) 0
#endif
void pb_reset(void);
void pb_reset(void);
void start_recovery(void);
void stop_recovery(void);
bool pb_force_recovery(void);
bool pb_max_boot_cnt(void);
void pb_dflt_cfg(void);
void pb_dflt_cfg(void);
void pb_init_config(void);
void pb_init_fs(void);
void pb_start_counter(void);
void pb_stop_counter(void);
void pb_inc_error_cnt(void);
bool pb_has_tee(void);
void pb_set_hyp_mode(void);
bool pb_has_vmm(void);
void pb_set_svc_mode(void);
void pb_execute_generic_jump(void);
void pb_execute_vmm_jump(void);
void pb_execute_tee_jump(void);
enum {
  EV_FORCE_RECOVERY,
  EV_RESET,
  EV_OK,
  EV_ERROR,
};
struct ufsm_machine * get_MainMachine(void);
struct ufsm_machine * get_LoadMachine(void);
struct ufsm_machine * get_StartMachine(void);
#endif
