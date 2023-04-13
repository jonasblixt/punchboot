#ifndef PLATFORM_IMX6UL_H
#define PLATFORM_IMX6UL_H

#include <stdint.h>
#include <pb/rot.h>
#include <pb/slc.h>

struct imx6ul_platform
{
};

int board_init(struct imx6ul_platform *plat);

int imx6ul_slc_set_configuration_locked(void);
slc_t imx6ul_slc_read_status(void);
int imx6ul_revoke_key(const struct rot_key *key);
int imx6ul_read_key_status(const struct rot_key *key);

#endif  // PLATFORM_IMX6UL_H
