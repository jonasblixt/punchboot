/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright 2017-2022 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*!
 * File containing client-side RPC functions for the MISC service. These
 * functions are ported to clients that communicate to the SC.
 *
 * @addtogroup MISC_SVC
 * @{
 */

/* Includes */

#include <sci/types.h>
#include <sci/svc/rm/api.h>
#include <sci/svc/misc/api.h>
#include <sci/rpc.h>
#include "rpc.h"

/* Local Defines */

/* Local Types */

/* Local Functions */

sc_err_t sc_misc_set_control(sc_ipc_t ipc, sc_rsrc_t resource, sc_ctrl_t ctrl,
    uint32_t val)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 4U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_SET_CONTROL);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(ctrl);
    RPC_U32(&msg, 4U) = U32(val);
    RPC_U16(&msg, 8U) = U16(resource);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_misc_get_control(sc_ipc_t ipc, sc_rsrc_t resource, sc_ctrl_t ctrl,
    uint32_t *val)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_GET_CONTROL);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(ctrl);
    RPC_U16(&msg, 4U) = U16(resource);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (val != NULL)
    {
        *val = (uint32_t) RPC_U32(&msg, 0U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_misc_set_max_dma_group(sc_ipc_t ipc, sc_rm_pt_t pt,
    sc_misc_dma_group_t max)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_SET_MAX_DMA_GROUP);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(pt);
    RPC_U8(&msg, 1U) = U8(max);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_misc_set_dma_group(sc_ipc_t ipc, sc_rsrc_t resource,
    sc_misc_dma_group_t group)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_SET_DMA_GROUP);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);
    RPC_U8(&msg, 2U) = U8(group);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

void sc_misc_debug_out(sc_ipc_t ipc, uint8_t ch)
{
    sc_rpc_msg_t msg;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_DEBUG_OUT);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(ch);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);
}

sc_err_t sc_misc_waveform_capture(sc_ipc_t ipc, sc_bool_t enable)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_WAVEFORM_CAPTURE);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = B2U8(enable);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

void sc_misc_build_info(sc_ipc_t ipc, uint32_t *build, uint32_t *commit)
{
    sc_rpc_msg_t msg;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_BUILD_INFO);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out receive message */
    if (build != NULL)
    {
        *build = (uint32_t) RPC_U32(&msg, 0U);
    }
    if (commit != NULL)
    {
        *commit = (uint32_t) RPC_U32(&msg, 4U);
    }
}

void sc_misc_api_ver(sc_ipc_t ipc, uint16_t *cl_maj, uint16_t *cl_min,
    uint16_t *sv_maj, uint16_t *sv_min)
{
    sc_rpc_msg_t msg;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_API_VER);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out receive message */
    if (cl_maj != NULL)
    {
        *cl_maj = (uint16_t) SCFW_API_VERSION_MAJOR;
    }
    if (cl_min != NULL)
    {
        *cl_min = (uint16_t) SCFW_API_VERSION_MINOR;
    }
    if (sv_maj != NULL)
    {
        *sv_maj = (uint16_t) RPC_U16(&msg, 4U);
    }
    if (sv_min != NULL)
    {
        *sv_min = (uint16_t) RPC_U16(&msg, 6U);
    }
}

void sc_misc_unique_id(sc_ipc_t ipc, uint32_t *id_l, uint32_t *id_h)
{
    sc_rpc_msg_t msg;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_UNIQUE_ID);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out receive message */
    if (id_l != NULL)
    {
        *id_l = (uint32_t) RPC_U32(&msg, 0U);
    }
    if (id_h != NULL)
    {
        *id_h = (uint32_t) RPC_U32(&msg, 4U);
    }
}

sc_err_t sc_misc_set_ari(sc_ipc_t ipc, sc_rsrc_t resource,
    sc_rsrc_t resource_mst, uint16_t ari, sc_bool_t enable)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_SET_ARI);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);
    RPC_U16(&msg, 2U) = U16(resource_mst);
    RPC_U16(&msg, 4U) = U16(ari);
    RPC_U8(&msg, 6U) = B2U8(enable);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

void sc_misc_boot_status(sc_ipc_t ipc, sc_misc_boot_status_t status)
{
    sc_rpc_msg_t msg;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_BOOT_STATUS);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(status);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_TRUE);
}

sc_err_t sc_misc_boot_done(sc_ipc_t ipc, sc_rsrc_t cpu)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_BOOT_DONE);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(cpu);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_misc_otp_fuse_read(sc_ipc_t ipc, uint32_t word, uint32_t *val)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_OTP_FUSE_READ);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(word);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (val != NULL)
    {
        *val = (uint32_t) RPC_U32(&msg, 0U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_misc_otp_fuse_write(sc_ipc_t ipc, uint32_t word, uint32_t val)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_OTP_FUSE_WRITE);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(word);
    RPC_U32(&msg, 4U) = U32(val);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_misc_set_temp(sc_ipc_t ipc, sc_rsrc_t resource, sc_misc_temp_t temp,
    int16_t celsius, int8_t tenths)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_SET_TEMP);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);
    RPC_I16(&msg, 2U) = I16(celsius);
    RPC_U8(&msg, 4U) = U8(temp);
    RPC_I8(&msg, 5U) = I8(tenths);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_misc_get_temp(sc_ipc_t ipc, sc_rsrc_t resource, sc_misc_temp_t temp,
    int16_t *celsius, int8_t *tenths)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_GET_TEMP);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);
    RPC_U8(&msg, 2U) = U8(temp);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (celsius != NULL)
    {
        *celsius = (int16_t) RPC_I16(&msg, 0U);
    }
    if (tenths != NULL)
    {
        *tenths = (int8_t) RPC_I8(&msg, 2U);
    }

    /* Return result */
    return err;
}

void sc_misc_get_boot_dev(sc_ipc_t ipc, sc_rsrc_t *dev)
{
    sc_rpc_msg_t msg;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_GET_BOOT_DEV);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out receive message */
    if (dev != NULL)
    {
        *dev = (sc_rsrc_t) RPC_U16(&msg, 0U);
    }
}

sc_err_t sc_misc_get_boot_type(sc_ipc_t ipc, sc_misc_bt_t *type)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_GET_BOOT_TYPE);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (type != NULL)
    {
        *type = (sc_misc_bt_t) RPC_U8(&msg, 0U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_misc_get_boot_container(sc_ipc_t ipc, uint8_t *idx)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_GET_BOOT_CONTAINER);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (idx != NULL)
    {
        *idx = (uint8_t) RPC_U8(&msg, 0U);
    }

    /* Return result */
    return err;
}

void sc_misc_get_button_status(sc_ipc_t ipc, sc_bool_t *status)
{
    sc_rpc_msg_t msg;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_GET_BUTTON_STATUS);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out receive message */
    if (status != NULL)
    {
        *status = (sc_bool_t) U2B(RPC_U8(&msg, 0U));
    }
}

sc_err_t sc_misc_rompatch_checksum(sc_ipc_t ipc, uint32_t *checksum)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_ROMPATCH_CHECKSUM);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (checksum != NULL)
    {
        *checksum = (uint32_t) RPC_U32(&msg, 0U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_misc_board_ioctl(sc_ipc_t ipc, uint32_t *parm1, uint32_t *parm2,
    uint32_t *parm3)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 4U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_MISC);
    RPC_FUNC(&msg) = U8(MISC_FUNC_BOARD_IOCTL);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(*parm1);
    RPC_U32(&msg, 4U) = U32(*parm2);
    RPC_U32(&msg, 8U) = U32(*parm3);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    *parm1 = (uint32_t) RPC_U32(&msg, 0U);
    *parm2 = (uint32_t) RPC_U32(&msg, 4U);
    *parm3 = (uint32_t) RPC_U32(&msg, 8U);

    /* Return result */
    return err;
}

/** @} */

