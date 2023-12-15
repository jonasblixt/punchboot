#ifndef INCLUDE_PLAT_IMXRT_IMXRT_H
#define INCLUDE_PLAT_IMXRT_IMXRT_H

#include <pb/rot.h>
#include <pb/slc.h>
#include <stdint.h>

#include "clock.h"
#include "fusebox.h"
#include "mm.h"
#include "pins.h"

struct imxrt_platform {
    uint8_t si_rev;
    uint8_t speed_grade;
    unsigned int ahb_root_clk_MHz;
    unsigned int ipg_root_clk_MHz;
    unsigned int per_root_clk_MHz;
};

int board_init(struct imxrt_platform *plat);
void board_console_init(struct imxrt_platform *plat);

int imxrt_slc_set_configuration_locked(void);
slc_t imxrt_slc_read_status(void);
int imxrt_revoke_key(const struct rot_key *key);
int imxrt_read_key_status(const struct rot_key *key);

#endif
