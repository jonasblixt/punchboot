#include "uuid.h"

int uuid_to_guid(const uuid_t uuid, uuid_t guid)
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

    return 0;
}
