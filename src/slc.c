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

static const struct slc_config *cfg;

int slc_init(const struct slc_config *cfg_)
{
    cfg = cfg_;
    return PB_OK;
}

slc_t slc_read_status(void)
{
    if (!cfg || !cfg->read_status)
        return -PB_ERR_NOT_SUPPORTED;

    return cfg->read_status();
}

int slc_set_configuration(void)
{
    if (!cfg || !cfg->set_configuration)
        return -PB_ERR_NOT_SUPPORTED;

    return cfg->set_configuration();
}

int slc_set_configuration_locked(void)
{
    if (!cfg || !cfg->set_configuration_locked)
        return -PB_ERR_NOT_SUPPORTED;

    return cfg->set_configuration_locked();
}

int slc_set_eol(void)
{
    if (!cfg || !cfg->set_configuration_locked)
        return -PB_ERR_NOT_SUPPORTED;

    return cfg->set_eol();
}
