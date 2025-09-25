/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright 2017-2022 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*!
 * File containing client-side RPC functions for the SECO service. These
 * functions are ported to clients that communicate to the SC.
 *
 * @addtogroup SECO_SVC
 * @{
 */

/* Includes */

#include <sci/types.h>
#include <sci/svc/rm/api.h>
#include <sci/svc/seco/api.h>
#include <sci/rpc.h>
#include "rpc.h"

/* Local Defines */

/* Local Types */

/* Local Functions */

sc_err_t sc_seco_image_load(sc_ipc_t ipc, sc_faddr_t addr_src,
    sc_faddr_t addr_dst, uint32_t len, sc_bool_t fw)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 7U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_IMAGE_LOAD);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr_src >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr_src);
    RPC_U32(&msg, 8U) = U32(addr_dst >> 32ULL);
    RPC_U32(&msg, 12U) = U32(addr_dst);
    RPC_U32(&msg, 16U) = U32(len);
    RPC_U8(&msg, 20U) = B2U8(fw);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_authenticate(sc_ipc_t ipc, sc_seco_auth_cmd_t cmd,
    sc_faddr_t addr)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 4U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_AUTHENTICATE);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr);
    RPC_U8(&msg, 8U) = U8(cmd);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_enh_authenticate(sc_ipc_t ipc, sc_seco_auth_cmd_t cmd,
    sc_faddr_t addr, uint32_t mask1, uint32_t mask2)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 6U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_ENH_AUTHENTICATE);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr);
    RPC_U32(&msg, 8U) = U32(mask1);
    RPC_U32(&msg, 12U) = U32(mask2);
    RPC_U8(&msg, 16U) = U8(cmd);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_forward_lifecycle(sc_ipc_t ipc, uint32_t change)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_FORWARD_LIFECYCLE);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(change);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_return_lifecycle(sc_ipc_t ipc, sc_faddr_t addr)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_RETURN_LIFECYCLE);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_commit(sc_ipc_t ipc, uint32_t *info)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_COMMIT);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(*info);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    *info = (uint32_t) RPC_U32(&msg, 0U);

    /* Return result */
    return err;
}

sc_err_t sc_seco_attest_mode(sc_ipc_t ipc, uint32_t mode)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_ATTEST_MODE);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(mode);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_attest(sc_ipc_t ipc, uint64_t nonce)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_ATTEST);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(nonce >> 32ULL);
    RPC_U32(&msg, 4U) = U32(nonce);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_get_attest_pkey(sc_ipc_t ipc, sc_faddr_t addr)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_GET_ATTEST_PKEY);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_get_attest_sign(sc_ipc_t ipc, sc_faddr_t addr)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_GET_ATTEST_SIGN);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_attest_verify(sc_ipc_t ipc, sc_faddr_t addr)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_ATTEST_VERIFY);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_gen_key_blob(sc_ipc_t ipc, uint32_t id, sc_faddr_t load_addr,
    sc_faddr_t export_addr, uint16_t max_size)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 7U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_GEN_KEY_BLOB);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(load_addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(load_addr);
    RPC_U32(&msg, 8U) = U32(export_addr >> 32ULL);
    RPC_U32(&msg, 12U) = U32(export_addr);
    RPC_U32(&msg, 16U) = U32(id);
    RPC_U16(&msg, 20U) = U16(max_size);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_load_key(sc_ipc_t ipc, uint32_t id, sc_faddr_t addr)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 4U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_LOAD_KEY);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr);
    RPC_U32(&msg, 8U) = U32(id);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_get_mp_key(sc_ipc_t ipc, sc_faddr_t dst_addr,
    uint16_t dst_size)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 4U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_GET_MP_KEY);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(dst_addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(dst_addr);
    RPC_U16(&msg, 8U) = U16(dst_size);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_update_mpmr(sc_ipc_t ipc, sc_faddr_t addr, uint8_t size,
    uint8_t lock)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 4U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_UPDATE_MPMR);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr);
    RPC_U8(&msg, 8U) = U8(size);
    RPC_U8(&msg, 9U) = U8(lock);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_get_mp_sign(sc_ipc_t ipc, sc_faddr_t msg_addr,
    uint16_t msg_size, sc_faddr_t dst_addr, uint16_t dst_size)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 6U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_GET_MP_SIGN);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(msg_addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(msg_addr);
    RPC_U32(&msg, 8U) = U32(dst_addr >> 32ULL);
    RPC_U32(&msg, 12U) = U32(dst_addr);
    RPC_U16(&msg, 16U) = U16(msg_size);
    RPC_U16(&msg, 18U) = U16(dst_size);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_v2x_build_info(sc_ipc_t ipc, uint32_t *version,
    uint32_t *commit)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_V2X_BUILD_INFO);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (version != NULL)
    {
        *version = (uint32_t) RPC_U32(&msg, 0U);
    }
    if (commit != NULL)
    {
        *commit = (uint32_t) RPC_U32(&msg, 4U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_seco_set_mono_counter_partition_hsm(sc_ipc_t ipc, uint16_t *she,
    uint16_t *hsm)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_SET_MONO_COUNTER_PARTITION_HSM);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(*she);
    RPC_U16(&msg, 2U) = U16(*hsm);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    *she = (uint16_t) RPC_U16(&msg, 0U);
    *hsm = (uint16_t) RPC_U16(&msg, 2U);

    /* Return result */
    return err;
}

sc_err_t sc_seco_fips_info(sc_ipc_t ipc, seco_fips_info_t *cert,
    seco_fips_info_t *mode)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_FIPS_INFO);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (cert != NULL)
    {
        *cert = (seco_fips_info_t) RPC_U8(&msg, 0U);
    }
    if (mode != NULL)
    {
        *mode = (seco_fips_info_t) RPC_U8(&msg, 1U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_seco_set_fips_mode(sc_ipc_t ipc, uint8_t mode, uint32_t *reason)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_SET_FIPS_MODE);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(mode);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (reason != NULL)
    {
        *reason = (uint32_t) RPC_U32(&msg, 0U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_seco_fips_degrade(sc_ipc_t ipc, sc_faddr_t addr, uint32_t *status)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_FIPS_DEGRADE);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (status != NULL)
    {
        *status = (uint32_t) RPC_U32(&msg, 0U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_seco_fips_key_zero(sc_ipc_t ipc, sc_faddr_t addr)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_FIPS_KEY_ZERO);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

void sc_seco_build_info(sc_ipc_t ipc, uint32_t *version, uint32_t *commit)
{
    sc_rpc_msg_t msg;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_BUILD_INFO);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out receive message */
    if (version != NULL)
    {
        *version = (uint32_t) RPC_U32(&msg, 0U);
    }
    if (commit != NULL)
    {
        *commit = (uint32_t) RPC_U32(&msg, 4U);
    }
}

sc_err_t sc_seco_chip_info(sc_ipc_t ipc, uint16_t *lc, uint16_t *monotonic,
    uint32_t *uid_l, uint32_t *uid_h)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_CHIP_INFO);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (uid_l != NULL)
    {
        *uid_l = (uint32_t) RPC_U32(&msg, 0U);
    }
    if (uid_h != NULL)
    {
        *uid_h = (uint32_t) RPC_U32(&msg, 4U);
    }
    if (lc != NULL)
    {
        *lc = (uint16_t) RPC_U16(&msg, 8U);
    }
    if (monotonic != NULL)
    {
        *monotonic = (uint16_t) RPC_U16(&msg, 10U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_seco_enable_debug(sc_ipc_t ipc, sc_faddr_t addr)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_ENABLE_DEBUG);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_get_event(sc_ipc_t ipc, uint8_t idx, uint32_t *event)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_GET_EVENT);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(idx);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (event != NULL)
    {
        *event = (uint32_t) RPC_U32(&msg, 0U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_seco_fuse_write(sc_ipc_t ipc, sc_faddr_t addr)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_FUSE_WRITE);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_patch(sc_ipc_t ipc, sc_faddr_t addr)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_PATCH);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_set_mono_counter_partition(sc_ipc_t ipc, uint16_t *she)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_SET_MONO_COUNTER_PARTITION);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(*she);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    *she = (uint16_t) RPC_U16(&msg, 0U);

    /* Return result */
    return err;
}

sc_err_t sc_seco_start_rng(sc_ipc_t ipc, sc_seco_rng_stat_t *status)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_START_RNG);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (status != NULL)
    {
        *status = (sc_seco_rng_stat_t) RPC_U32(&msg, 0U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_seco_sab_msg(sc_ipc_t ipc, sc_faddr_t addr)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_SAB_MSG);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(addr);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_caam_td_config(sc_ipc_t ipc, sc_rsrc_t resource,
    sc_bool_t allow, sc_bool_t lock)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_CAAM_TD_CONFIG);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);
    RPC_U8(&msg, 2U) = B2U8(allow);
    RPC_U8(&msg, 3U) = B2U8(lock);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_secvio_enable(sc_ipc_t ipc)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_SECVIO_ENABLE);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_seco_secvio_config(sc_ipc_t ipc, uint8_t id, uint8_t access,
    uint32_t *data0, uint32_t *data1, uint32_t *data2, uint32_t *data3,
    uint32_t *data4, uint8_t size)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 7U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_SECVIO_CONFIG);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(*data0);
    RPC_U32(&msg, 4U) = U32(*data1);
    RPC_U32(&msg, 8U) = U32(*data2);
    RPC_U32(&msg, 12U) = U32(*data3);
    RPC_U32(&msg, 16U) = U32(*data4);
    RPC_U8(&msg, 20U) = U8(id);
    RPC_U8(&msg, 21U) = U8(access);
    RPC_U8(&msg, 22U) = U8(size);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    *data0 = (uint32_t) RPC_U32(&msg, 0U);
    *data1 = (uint32_t) RPC_U32(&msg, 4U);
    *data2 = (uint32_t) RPC_U32(&msg, 8U);
    *data3 = (uint32_t) RPC_U32(&msg, 12U);
    *data4 = (uint32_t) RPC_U32(&msg, 16U);

    /* Return result */
    return err;
}

sc_err_t sc_seco_secvio_dgo_config(sc_ipc_t ipc, uint8_t id, uint8_t access,
    uint32_t *data)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_SECO);
    RPC_FUNC(&msg) = U8(SECO_FUNC_SECVIO_DGO_CONFIG);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(*data);
    RPC_U8(&msg, 4U) = U8(id);
    RPC_U8(&msg, 5U) = U8(access);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    *data = (uint32_t) RPC_U32(&msg, 0U);

    /* Return result */
    return err;
}

/** @} */

