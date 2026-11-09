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

#endif
