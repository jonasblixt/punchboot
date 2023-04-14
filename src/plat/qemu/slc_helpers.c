#include <pb/pb.h>
#include <plat/qemu/qemu.h>
#include <drivers/fuse/test_fuse_bio.h>

#define SEC_CONF_LOCKED BIT(0)
#define SEC_CONF_EOL BIT(1)

slc_t qemu_slc_read_status(void)
{
    int val = test_fuse_read(FUSE_SEC);

    if (val < 0)
        LOG_ERR("Could not read fuse");

    if (val & SEC_CONF_EOL)
        return SLC_EOL;
    else if (val & SEC_CONF_LOCKED)
        return SLC_CONFIGURATION_LOCKED;
    else
        return SLC_CONFIGURATION;

    return PB_OK;
}

int qemu_slc_set_configuration_locked(void)
{
    return test_fuse_write(FUSE_SEC, SEC_CONF_LOCKED);
}

int qemu_slc_set_eol(void)
{
    return test_fuse_write(FUSE_SEC, SEC_CONF_EOL);
}
