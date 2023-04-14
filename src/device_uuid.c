#include <pb/errors.h>
#include <pb/plat.h>
#include <pb/device_uuid.h>
#include <uuid.h>
#include <platform_defs.h>

static uuid_t device_uu;

int device_uuid(uuid_t uu)
{
    int rc;
    uint8_t plat_unique[16];
    size_t plat_unique_len = sizeof(plat_unique);

    if (uuid_is_null(device_uu)) {
        rc = plat_get_unique_id(plat_unique, &plat_unique_len);

        if (rc != PB_OK)
            return rc;

        rc = uuid_gen_uuid3(PLATFORM_NS_UUID,
                            (const char *) plat_unique, plat_unique_len,
                            device_uu);

        if (rc != 0) {
            uuid_clear(device_uu);
            return rc;
        }
    }

    uuid_copy(uu, device_uu);
    return PB_OK;
}
