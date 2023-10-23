#ifndef INCLUDE_PLAT_IMXRT_IMXRT_H
#define INCLUDE_PLAT_IMXRT_IMXRT_H

#include <stdint.h>
#include <pb/rot.h>
#include <pb/slc.h>

struct imxrt_platform
{
};

int board_init(struct imxrt_platform *plat);

int imxrt_slc_set_configuration_locked(void);
slc_t imxrt_slc_read_status(void);
int imxrt_revoke_key(const struct rot_key *key);
int imxrt_read_key_status(const struct rot_key *key);

#endif
