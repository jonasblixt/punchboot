#ifndef INCLUDE_PB_SLC_H_
#define INCLUDE_PB_SLC_H_

#include <stdint.h>

enum pb_security_life_cycle
{
    PB_SLC_INVALID,
    PB_SLC_NOT_CONFIGURED,
    PB_SLC_CONFIGURATION,
    PB_SLC_CONFIGURATION_LOCKED,
    PB_SLC_EOL,
};

const char *pb_slc_string(enum pb_security_life_cycle slc);

#endif  // INCLUDE_PB_SLC_H_
