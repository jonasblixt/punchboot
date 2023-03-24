#include <string.h>
#include <pb/pb.h>
#include <uuid.h>
#include <crypto.h>

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
} uuid3_t;

int uuid_gen_uuid3(const char *namespace_uu,
                       const char *unique, size_t size, char *out)
{
    int err;

    err = hash_init(HASH_MD5);

    if (err != PB_OK)
        return err;

    err = hash_update((uintptr_t) namespace_uu, 16);

    if (err != PB_OK)
        return err;

    err = hash_update((uintptr_t) unique, size);

    if (err != PB_OK)
        return err;

    err = hash_final((uint8_t *) out, 16);

    if (err != PB_OK)
        return err;

    uuid3_t *u = (uuid3_t *) out;

    u->uuid.time_hi_and_version &= 0xFF0F;
    u->uuid.time_hi_and_version |= 0x0030; /* Version 3*/

    u->uuid.clock_seq_hi_and_res &= 0x3F;
    u->uuid.clock_seq_hi_and_res |= 0x80; /* RFC4122 variant*/
    return PB_OK;
}
