#include <stdio.h>
#include <pb.h>
#include <uuid.h>

typedef union 
{
    uint8_t raw[16];
    struct
    {
        uint32_t time_low;
        uint16_t time_mid;
        uint16_t time_hi_and_version;
        uint8_t clock_seq_hi_and_res;
        uint8_t clock_seq_low;
        uint8_t node[6];
    } uuid __packed;
} uuid_t;

uint32_t uuid_to_string(uint8_t *uuid, char *out)
{
    uuid_t *u = (uuid_t *) uuid;

    snprintf(out,40, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        u->uuid.time_low,
        u->uuid.time_mid,
        u->uuid.time_hi_and_version,
        u->uuid.clock_seq_hi_and_res,
        u->uuid.clock_seq_low,
        u->uuid.node[0],
        u->uuid.node[1],
        u->uuid.node[2],
        u->uuid.node[3],
        u->uuid.node[4],
        u->uuid.node[5]);

    

    return PB_OK;
}
