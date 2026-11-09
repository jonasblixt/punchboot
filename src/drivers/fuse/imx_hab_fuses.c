/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <drivers/fuse/imx_hab_fuses.h>
#include <drivers/fuse/imx_ocotp.h>
#include <pb/pb.h>
#include <pb/srk_fuse.h>

static const struct imx_hab_fuses *cfg;

void imx_hab_fuses_init(const struct imx_hab_fuses *cfg_)
{
    cfg = cfg_;
}

slc_t imx_hab_slc_read_status(void)
{
    uint32_t val;
    int rc;

    if (cfg == NULL)
        return -PB_ERR_STATE;

    rc = imx_ocotp_read(cfg->sec_config_bank, cfg->sec_config_word, &val);

    if (rc != PB_OK)
        return rc;

    if (val & cfg->closed)
        return SLC_CONFIGURATION_LOCKED;

    /* A partly fused SRK hash is CONFIGURATION too: fusing it is not done */
    for (unsigned int n = 0; n < IMX_HAB_SRK_HASH_WORDS; n++) {
        rc = imx_ocotp_read(cfg->srk_bank, cfg->srk_word + n, &val);

        if (rc != PB_OK)
            return rc;

        if (val != 0)
            return SLC_CONFIGURATION;
    }

    return SLC_NOT_CONFIGURED;
}

int imx_hab_slc_set_configuration(void)
{
    return imx_hab_fuse_srk(srk_fuse, srk_fuse_words);
}

int imx_hab_slc_set_configuration_locked(void)
{
    if (cfg == NULL)
        return -PB_ERR_STATE;

    LOG_INFO("About to change security state to locked");
    return imx_ocotp_write(cfg->sec_config_bank, cfg->sec_config_word, cfg->closed);
}

/* Key n is revoked by bit n, also past the SRKs HAB can revoke: pb
 * revoked those before it refused to (F-165) */
int imx_hab_read_key_status(const struct rot_key *key)
{
    uint32_t val;
    int rc;

    if (cfg == NULL)
        return -PB_ERR_STATE;

    if (key->slot >= 32)
        return PB_OK;

    rc = imx_ocotp_read(cfg->revoke_bank, cfg->revoke_word, &val);

    if (rc != PB_OK)
        return rc;

    if (val & (1u << key->slot))
        return -PB_ERR_KEY_REVOKED;

    return PB_OK;
}

int imx_hab_revoke_key(const struct rot_key *key)
{
    uint32_t val;
    uint32_t bit;
    int rc;

    if (cfg == NULL)
        return -PB_ERR_STATE;

    if (key->slot >= cfg->revocable) {
        LOG_ERR("Key %u can't be revoked, HAB revokes SRK0-%u", key->slot, cfg->revocable - 1);
        return -PB_ERR_PARAM;
    }

    if (cfg->revoke_allowed) {
        rc = cfg->revoke_allowed();

        if (rc != PB_OK) {
            LOG_ERR("HAB is reporting errors, aborting");
            return -PB_ERR_STATE;
        }
    }

    rc = imx_ocotp_read(cfg->revoke_bank, cfg->revoke_word, &val);

    if (rc != PB_OK) {
        LOG_ERR("Could not read revoke fuse");
        return rc;
    }

    LOG_DBG("Current revoke value: 0x%x", val);
    bit = 1u << key->slot;

    if (val & bit) {
        LOG_INFO("Key already revoked");
        return PB_OK;
    }

    LOG_INFO("Revoking key %u (0x%x)", key->slot, key->id);
    return imx_ocotp_write(cfg->revoke_bank, cfg->revoke_word, val | bit);
}

int imx_hab_fuse_srk(const uint32_t *srk, size_t words)
{
    bool fuse = false;
    uint32_t val;
    int rc;

    if (cfg == NULL)
        return -PB_ERR_STATE;

    LOG_INFO("Fusing SRK fuses");

    if (words != IMX_HAB_SRK_HASH_WORDS) {
        LOG_ERR(
            "The SRK hash is %zu words, not %u (CONFIG_SRK_FUSE)", words, IMX_HAB_SRK_HASH_WORDS);
        return -PB_ERR_PARAM;
    }

    for (unsigned int n = 0; n < IMX_HAB_SRK_HASH_WORDS; n++) {
        if (srk[n] == 0)
            return -PB_ERR_PARAM;
    }

    for (unsigned int n = 0; n < IMX_HAB_SRK_HASH_WORDS; n++) {
        rc = imx_ocotp_read(cfg->srk_bank, cfg->srk_word + n, &val);

        if (rc != PB_OK)
            return rc;

        if (val != 0 && val != srk[n]) {
            LOG_ERR("Trying to change already fused srk[%u] from 0x%x to 0x%x", n, val, srk[n]);
            return -PB_ERR_MEM;
        } else if (val != 0) {
            LOG_INFO("srk[%u] already fused to 0x%x", n, val);
        } else {
            fuse = true;
        }
    }

    if (!fuse)
        return PB_OK;

    LOG_INFO("Writing SRK...");

    for (unsigned int n = 0; n < IMX_HAB_SRK_HASH_WORDS; n++) {
        LOG_INFO("Fusing srk[%u] to 0x%x", n, srk[n]);
        rc = imx_ocotp_write(cfg->srk_bank, cfg->srk_word + n, srk[n]);

        if (rc != PB_OK)
            return rc;
    }

    return PB_OK;
}
