#include <string.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <uuid/uuid.h>

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

static __a4k __no_bss uint8_t _uuid_aligned_buf[64];
static __a4k __no_bss uint8_t _uuid_aligned_buf2[64];
static struct pb_hash_context hash_ctx;

int uuid_gen_uuid3(const char *namespace_uu,
                       const char *unique, size_t size, char *out)
{
    int err;

    err = plat_hash_init(&hash_ctx, PB_HASH_MD5);

    if (err != PB_OK)
        return err;

    if (size > sizeof(_uuid_aligned_buf2))
        return PB_ERR;

    memset(_uuid_aligned_buf, 0, sizeof(_uuid_aligned_buf));
    memcpy(_uuid_aligned_buf, namespace_uu, 16);

    err = plat_hash_update(&hash_ctx, _uuid_aligned_buf,
                                        sizeof(_uuid_aligned_buf));

    if (err != PB_OK)
        return err;

    memset(_uuid_aligned_buf2, 0, sizeof(_uuid_aligned_buf2));
    memcpy(_uuid_aligned_buf2, unique, size);

    err = plat_hash_update(&hash_ctx, _uuid_aligned_buf2,
                            sizeof(_uuid_aligned_buf2));

    if (err != PB_OK)
        return err;

    err = plat_hash_finalize(&hash_ctx, NULL, 0);

    if (err != PB_OK)
        return err;

    memcpy(out, hash_ctx.buf, 16);

    uuid3_t *u = (uuid3_t *) out;

    u->uuid.time_hi_and_version &= 0xFF0F;
    u->uuid.time_hi_and_version |= 0x0030; /* Version 3*/

    u->uuid.clock_seq_hi_and_res &= 0x1F;
    u->uuid.clock_seq_hi_and_res |= 0x80; /* RFC4122 variant*/
    return PB_OK;
}
