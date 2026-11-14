/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <pb/slc.h>

static const struct slc_ops *ops;
static const struct slc_fuse *fuses;
static size_t fuses_length;

int slc_init(const struct slc_ops *ops_, const struct slc_fuse *fuses_, size_t fuses_length_)
{
    if (ops_ && fuses_length_ && (!fuses_ || !ops_->fuse_read || !ops_->fuse_write))
        return -PB_ERR_PARAM;

    ops = ops_;
    fuses = fuses_;
    fuses_length = fuses_length_;
    return PB_OK;
}

/* The fuses are one time programmable: set the bits that aren't set */
static int write_fuses(void)
{
    uint32_t val;
    int rc;

    for (size_t n = 0; n < fuses_length; n++) {
        const struct slc_fuse *fuse = &fuses[n];

        rc = ops->fuse_read(fuse->index, &val);

        if (rc != PB_OK) {
            LOG_ERR("Could not read fuse %u (%i)", fuse->index, rc);
            return rc;
        }

        if ((val & fuse->value) == fuse->value) {
            LOG_INFO("Fuse %u is 0x%x already", fuse->index, val);
            continue;
        }

        LOG_INFO("Fusing %u to 0x%x (was 0x%x)", fuse->index, fuse->value, val);
        rc = ops->fuse_write(fuse->index, fuse->value);

        if (rc != PB_OK) {
            LOG_ERR("Could not write fuse %u (%i)", fuse->index, rc);
            return rc;
        }
    }

    return PB_OK;
}

slc_t slc_read_status(void)
{
    /* Not set up: the life cycle is unknown, not unsupported */
    if (!ops)
        return -PB_ERR_STATE;

    if (!ops->read_status)
        return -PB_ERR_NOT_SUPPORTED;

    return ops->read_status();
}

/*
 * The life cycle only moves forward. The platform's operations only write
 * the fuses, the order is checked here:
 *
 *   NOT_CONFIGURED -> CONFIGURATION -> CONFIGURATION_LOCKED
 *
 * and EOL from any of them. Setting the state the device is in already is
 * done again for CONFIGURATION (its fuses can be written again, e.g. after
 * a failed write) and is PB_OK without a write for the others.
 */
int slc_set_configuration(void)
{
    slc_t slc;
    int rc;

    if (!ops || (!ops->set_configuration && !fuses_length))
        return -PB_ERR_NOT_SUPPORTED;

    slc = slc_read_status();

    /* Also a platform without read_status: the order can't be checked */
    if (slc < 0)
        return slc;

    if (slc == SLC_CONFIGURATION_LOCKED || slc == SLC_EOL) {
        LOG_ERR("The configuration is locked (%i)", slc);
        return -PB_ERR_STATE;
    }

    if (ops->set_configuration) {
        rc = ops->set_configuration();

        if (rc != PB_OK)
            return rc;
    }

    return write_fuses();
}

int slc_set_configuration_locked(void)
{
    slc_t slc;

    if (!ops || !ops->set_configuration_locked)
        return -PB_ERR_NOT_SUPPORTED;

    slc = slc_read_status();

    if (slc < 0)
        return slc;

    if (slc == SLC_CONFIGURATION_LOCKED) {
        LOG_INFO("Configuration already locked");
        return PB_OK;
    }

    if (slc != SLC_CONFIGURATION) {
        LOG_ERR("Not in configuration, can't lock (%i)", slc);
        return -PB_ERR_STATE;
    }

    return ops->set_configuration_locked();
}

/*
 * EOL is the product's option, CONFIG_SLC_EOL (system.md 4). Without it the
 * device can't enter EOL: nothing of the port's is called, whatever the port
 * has, and the answer is the one of a port without set_eol.
 */
int slc_set_eol(void)
{
#ifdef CONFIG_SLC_EOL
    slc_t slc;

    if (!ops || !ops->set_eol)
        return -PB_ERR_NOT_SUPPORTED;

    slc = slc_read_status();

    if (slc < 0)
        return slc;

    if (slc == SLC_EOL) {
        LOG_INFO("Already EOL");
        return PB_OK;
    }

    return ops->set_eol();
#else
    return -PB_ERR_NOT_SUPPORTED;
#endif
}
