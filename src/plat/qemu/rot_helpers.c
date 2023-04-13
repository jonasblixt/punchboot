#include <pb/pb.h>
#include <pb/rot.h>
#include <drivers/fuse/test_fuse_bio.h>
#include <plat/qemu/qemu.h>

int qemu_revoke_key(const struct rot_key *key)
{
    int val = test_fuse_read(FUSE_REVOKE);

    if (val < 0)
        LOG_ERR("Could not read fuse");

    return test_fuse_write(8, (uint32_t) (1 << key->param1));
}

int qemu_read_key_status(const struct rot_key *key)
{
    int val = test_fuse_read(FUSE_REVOKE);

    if (val < 0)
        LOG_ERR("Could not read fuse");

    if (val & (1 << key->param1))
        return -PB_ERR_KEY_REVOKED;
    return PB_OK;
}
