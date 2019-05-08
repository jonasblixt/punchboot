#include <stdio.h>
#include <pb.h>
#include <plat.h>
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
} uuid_t;

static __a16b __no_bss uint8_t _uuid_aligned_buf[64];
static __a16b __no_bss uint8_t _uuid_aligned_buf2[64];

uint32_t uuid_gen_uuid3(const char *ns,
                        uint32_t ns_length,
                        const char *unique_data,
                        uint32_t unique_data_length,
                        char *out)
{
    uint32_t err;

    err = plat_hash_init(PB_HASH_MD5);

    if (err != PB_OK)
        return err;

    if (ns_length > 32)
        return PB_ERR;

    if (unique_data_length > 32)
        return PB_ERR;

    memset(_uuid_aligned_buf,0, 64);
    memcpy(_uuid_aligned_buf, ns, ns_length);
    plat_hash_update((uintptr_t)_uuid_aligned_buf,64);
/*
    for (uint32_t n = 0; n < ns_length; n++)
        printf ("%02x",_uuid_aligned_buf[n]);
    printf ("\n\r");
*/
    memset(_uuid_aligned_buf2,0, 64);
    memcpy(_uuid_aligned_buf2, unique_data, unique_data_length);
    plat_hash_update((uintptr_t)_uuid_aligned_buf2,64);
/*
    for (uint32_t n = 0; n < unique_data_length; n++)
        printf ("%02x",_uuid_aligned_buf[n]);
    printf ("\n\r");
*/

    err = plat_hash_finalize((uintptr_t)out);

    if (err != PB_OK)
        return err;

    uuid_t *u = (uuid_t *) out;

    u->uuid.time_hi_and_version &= 0xFF0F;
    u->uuid.time_hi_and_version |= 0x0030; /* Version 3*/

    u->uuid.clock_seq_hi_and_res &= 0xFF1F;
    u->uuid.clock_seq_hi_and_res |= 0x0080; /* RFC4122 variant*/
    return PB_OK;
}

uint32_t uuid_to_string(uint8_t *uuid, char *out)
{
    uint32_t *u0 = (uint32_t *) &uuid[0];
    uint16_t *u1 = (uint16_t *) &uuid[4];
    uint16_t *u2 = (uint16_t *) &uuid[6];
    uint16_t *u3 = (uint16_t *) &uuid[8];
    uint16_t *u4 = (uint16_t *) &uuid[10];
    uint32_t *u5 = (uint32_t *) &uuid[12];

    *u0 = ((*u0 >> 24) & 0xff) |
          ((*u0 << 8)  & 0xff0000) |
          ((*u0 >> 8)  & 0xff00) |
          ((*u0 << 24) & 0xff000000);


    *u1 = (*u1 >> 8) | (*u1 << 8);
    *u2 = (*u2 >> 8) | (*u2 << 8);
    *u3 = (*u3 >> 8) | (*u3 << 8);
    *u4 = (*u4 >> 8) | (*u4 << 8);

    *u5 = ((*u5 >> 24) & 0xff) |
          ((*u5 << 8)  & 0xff0000) |
          ((*u5 >> 8)  & 0xff00) |
          ((*u5 << 24) & 0xff000000);

    snprintf(out,37, "%08x-%04x-%04x-%04x-%04x%08x",
                *u0,*u1,*u2,*u3,*u4,*u5);

    

    return PB_OK;
}

uint32_t uuid_to_guid(uint8_t *uuid, uint8_t *guid)
{
    guid[0] = uuid[3];
    guid[1] = uuid[2];
    guid[2] = uuid[1];
    guid[3] = uuid[0];

    guid[4] = uuid[5];
    guid[5] = uuid[4];

    guid[6] = uuid[7];
    guid[7] = uuid[6];

    guid[8] = uuid[8];
    guid[9] = uuid[9];

    guid[10] = uuid[10];
    guid[11] = uuid[11];
    guid[12] = uuid[12];
    guid[13] = uuid[13];
    guid[14] = uuid[14];
    guid[15] = uuid[15];

    return PB_OK;
}
