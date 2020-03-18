#include <pb-tools/slc.h>
#include <pb-tools/error.h>

const char *pb_slc_string(enum pb_security_life_cycle slc)
{
    switch (slc)
    {
#ifndef PB_PROTOCOL_DISABLE_STRINGS
        case PB_SLC_NOT_CONFIGURED:
            return "Not configured";
        case PB_SLC_CONFIGURATION:
            return "Configuration";
        case PB_SLC_CONFIGURATION_LOCKED:
            return "Configuration locked";
        case PB_SLC_EOL:
            return "EOL";
        case PB_SLC_INVALID:
        default:
            return "Invalid";
#else
        default:
            return "";
#endif
    }
}
