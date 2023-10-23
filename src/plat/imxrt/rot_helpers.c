#include <pb/pb.h>
#include <pb/rot.h>
#include <plat/imxrt/imxrt.h>

int imxrt_revoke_key(const struct rot_key *key)
{
        return -PB_ERR_STATE;
}

int imxrt_read_key_status(const struct rot_key *key)
{
        return PB_OK;
}
