#ifndef PLATFORM_IMX6UL_H
#define PLATFORM_IMX6UL_H

#include <pb/rot.h>
#include <pb/slc.h>
#include <stdint.h>

#include "clock.h"
#include "fusebox.h"
#include "hab.h"
#include "mm.h"
#include "pins.h"

struct imx6ul_platform {
    uint8_t si_rev;
    uint8_t speed_grade;
    unsigned int core_clk_MHz;
    unsigned int usdhc1_clk_MHz;
    unsigned int usdhc2_clk_MHz;
};

void board_console_init(struct imx6ul_platform *plat);
int board_init(struct imx6ul_platform *plat);

int imx6ul_slc_set_configuration_locked(void);
slc_t imx6ul_slc_read_status(void);
int imx6ul_revoke_key(const struct rot_key *key);
int imx6ul_read_key_status(const struct rot_key *key);

#endif // PLATFORM_IMX6UL_H
