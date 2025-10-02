/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright 2017-2022 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*!
 * File containing client-side RPC functions for the PM service. These
 * functions are ported to clients that communicate to the SC.
 *
 * @addtogroup PM_SVC
 * @{
 */

/* Includes */

#include <sci/types.h>
#include <sci/svc/rm/api.h>
#include <sci/svc/pm/api.h>
#include <sci/rpc.h>
#include "rpc.h"

/* Local Defines */

/* Local Types */

/* Local Functions */

sc_err_t sc_pm_set_sys_power_mode(sc_ipc_t ipc, sc_pm_power_mode_t mode)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_SET_SYS_POWER_MODE);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(mode);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_set_partition_power_mode(sc_ipc_t ipc, sc_rm_pt_t pt,
    sc_pm_power_mode_t mode)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_SET_PARTITION_POWER_MODE);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(pt);
    RPC_U8(&msg, 1U) = U8(mode);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

void sc_pm_partition_power_off(sc_ipc_t ipc)
{
    sc_rpc_msg_t msg;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_PARTITION_POWER_OFF);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_TRUE);
}

sc_err_t sc_pm_get_sys_power_mode(sc_ipc_t ipc, sc_rm_pt_t pt,
    sc_pm_power_mode_t *mode)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_GET_SYS_POWER_MODE);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(pt);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (mode != NULL)
    {
        *mode = (sc_pm_power_mode_t) RPC_U8(&msg, 0U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_pm_partition_wake(sc_ipc_t ipc, sc_rm_pt_t pt)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_PARTITION_WAKE);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(pt);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_set_resource_power_mode(sc_ipc_t ipc, sc_rsrc_t resource,
    sc_pm_power_mode_t mode)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_SET_RESOURCE_POWER_MODE);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);
    RPC_U8(&msg, 2U) = U8(mode);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_set_resource_power_mode_all(sc_ipc_t ipc, sc_rm_pt_t pt,
    sc_pm_power_mode_t mode, sc_rsrc_t exclude)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_SET_RESOURCE_POWER_MODE_ALL);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(exclude);
    RPC_U8(&msg, 2U) = U8(pt);
    RPC_U8(&msg, 3U) = U8(mode);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_get_resource_power_mode(sc_ipc_t ipc, sc_rsrc_t resource,
    sc_pm_power_mode_t *mode)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_GET_RESOURCE_POWER_MODE);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (mode != NULL)
    {
        *mode = (sc_pm_power_mode_t) RPC_U8(&msg, 0U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_pm_req_low_power_mode(sc_ipc_t ipc, sc_rsrc_t resource,
    sc_pm_power_mode_t mode)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_REQ_LOW_POWER_MODE);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);
    RPC_U8(&msg, 2U) = U8(mode);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_req_cpu_low_power_mode(sc_ipc_t ipc, sc_rsrc_t resource,
    sc_pm_power_mode_t mode, sc_pm_wake_src_t wake_src)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_REQ_CPU_LOW_POWER_MODE);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);
    RPC_U8(&msg, 2U) = U8(mode);
    RPC_U8(&msg, 3U) = U8(wake_src);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_set_cpu_resume_addr(sc_ipc_t ipc, sc_rsrc_t resource,
    sc_faddr_t address)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 4U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_SET_CPU_RESUME_ADDR);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(address >> 32ULL);
    RPC_U32(&msg, 4U) = U32(address);
    RPC_U16(&msg, 8U) = U16(resource);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_set_cpu_resume(sc_ipc_t ipc, sc_rsrc_t resource,
    sc_bool_t isPrimary, sc_faddr_t address)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 4U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_SET_CPU_RESUME);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(address >> 32ULL);
    RPC_U32(&msg, 4U) = U32(address);
    RPC_U16(&msg, 8U) = U16(resource);
    RPC_U8(&msg, 10U) = B2U8(isPrimary);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_req_sys_if_power_mode(sc_ipc_t ipc, sc_rsrc_t resource,
    sc_pm_sys_if_t sys_if, sc_pm_power_mode_t hpm, sc_pm_power_mode_t lpm)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_REQ_SYS_IF_POWER_MODE);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);
    RPC_U8(&msg, 2U) = U8(sys_if);
    RPC_U8(&msg, 3U) = U8(hpm);
    RPC_U8(&msg, 4U) = U8(lpm);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_set_clock_rate(sc_ipc_t ipc, sc_rsrc_t resource, sc_pm_clk_t clk,
    sc_pm_clock_rate_t *rate)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_SET_CLOCK_RATE);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(*rate);
    RPC_U16(&msg, 4U) = U16(resource);
    RPC_U8(&msg, 6U) = U8(clk);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    *rate = (sc_pm_clock_rate_t) RPC_U32(&msg, 0U);

    /* Return result */
    return err;
}

sc_err_t sc_pm_get_clock_rate(sc_ipc_t ipc, sc_rsrc_t resource, sc_pm_clk_t clk,
    sc_pm_clock_rate_t *rate)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_GET_CLOCK_RATE);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);
    RPC_U8(&msg, 2U) = U8(clk);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (rate != NULL)
    {
        *rate = (sc_pm_clock_rate_t) RPC_U32(&msg, 0U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_pm_clock_enable(sc_ipc_t ipc, sc_rsrc_t resource, sc_pm_clk_t clk,
    sc_bool_t enable, sc_bool_t autog)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 3U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_CLOCK_ENABLE);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);
    RPC_U8(&msg, 2U) = U8(clk);
    RPC_U8(&msg, 3U) = B2U8(enable);
    RPC_U8(&msg, 4U) = B2U8(autog);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_set_clock_parent(sc_ipc_t ipc, sc_rsrc_t resource,
    sc_pm_clk_t clk, sc_pm_clk_parent_t parent)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_SET_CLOCK_PARENT);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);
    RPC_U8(&msg, 2U) = U8(clk);
    RPC_U8(&msg, 3U) = U8(parent);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_get_clock_parent(sc_ipc_t ipc, sc_rsrc_t resource,
    sc_pm_clk_t clk, sc_pm_clk_parent_t *parent)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_GET_CLOCK_PARENT);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);
    RPC_U8(&msg, 2U) = U8(clk);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (parent != NULL)
    {
        *parent = (sc_pm_clk_parent_t) RPC_U8(&msg, 0U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_pm_reset(sc_ipc_t ipc, sc_pm_reset_type_t type)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_RESET);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(type);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_reset_stage(sc_ipc_t ipc, sc_pm_reset_stage_t stage)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_RESET_STAGE);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(stage);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_reset_reason(sc_ipc_t ipc, sc_pm_reset_reason_t *reason)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_RESET_REASON);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (reason != NULL)
    {
        *reason = (sc_pm_reset_reason_t) RPC_U8(&msg, 0U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_pm_get_reset_part(sc_ipc_t ipc, sc_rm_pt_t *pt)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_GET_RESET_PART);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Copy out receive message */
    if (pt != NULL)
    {
        *pt = (sc_rm_pt_t) RPC_U8(&msg, 0U);
    }

    /* Return result */
    return err;
}

sc_err_t sc_pm_boot(sc_ipc_t ipc, sc_rm_pt_t pt, sc_rsrc_t resource_cpu,
    sc_faddr_t boot_addr, sc_rsrc_t resource_mu, sc_rsrc_t resource_dev)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 5U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_BOOT);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(boot_addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(boot_addr);
    RPC_U16(&msg, 8U) = U16(resource_cpu);
    RPC_U16(&msg, 10U) = U16(resource_mu);
    RPC_U16(&msg, 12U) = U16(resource_dev);
    RPC_U8(&msg, 14U) = U8(pt);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_set_boot_parm(sc_ipc_t ipc, sc_rsrc_t resource_cpu,
    sc_faddr_t boot_addr, sc_rsrc_t resource_mu, sc_rsrc_t resource_dev)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 5U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_SET_BOOT_PARM);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(boot_addr >> 32ULL);
    RPC_U32(&msg, 4U) = U32(boot_addr);
    RPC_U16(&msg, 8U) = U16(resource_cpu);
    RPC_U16(&msg, 10U) = U16(resource_mu);
    RPC_U16(&msg, 12U) = U16(resource_dev);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

void sc_pm_reboot(sc_ipc_t ipc, sc_pm_reset_type_t type)
{
    sc_rpc_msg_t msg;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_REBOOT);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(type);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_TRUE);
}

sc_err_t sc_pm_reboot_partition(sc_ipc_t ipc, sc_rm_pt_t pt,
    sc_pm_reset_type_t type)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_REBOOT_PARTITION);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(pt);
    RPC_U8(&msg, 1U) = U8(type);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_reboot_continue(sc_ipc_t ipc, sc_rm_pt_t pt)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_REBOOT_CONTINUE);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(pt);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_err_t sc_pm_cpu_start(sc_ipc_t ipc, sc_rsrc_t resource, sc_bool_t enable,
    sc_faddr_t address)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 4U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_CPU_START);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(address >> 32ULL);
    RPC_U32(&msg, 4U) = U32(address);
    RPC_U16(&msg, 8U) = U16(resource);
    RPC_U8(&msg, 10U) = B2U8(enable);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

void sc_pm_cpu_reset(sc_ipc_t ipc, sc_rsrc_t resource, sc_faddr_t address)
{
    sc_rpc_msg_t msg;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 4U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_CPU_RESET);

    /* Fill in send message */
    RPC_U32(&msg, 0U) = U32(address >> 32ULL);
    RPC_U32(&msg, 4U) = U32(address);
    RPC_U16(&msg, 8U) = U16(resource);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_TRUE);
}

sc_err_t sc_pm_resource_reset(sc_ipc_t ipc, sc_rsrc_t resource)
{
    sc_rpc_msg_t msg;
    sc_err_t err;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_RESOURCE_RESET);

    /* Fill in send message */
    RPC_U16(&msg, 0U) = U16(resource);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    err = (sc_err_t) RPC_R8(&msg);

    /* Return result */
    return err;
}

sc_bool_t sc_pm_is_partition_started(sc_ipc_t ipc, sc_rm_pt_t pt)
{
    sc_rpc_msg_t msg;
    sc_bool_t result;

    /* Fill in header */
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 2U;
    RPC_SVC(&msg) = U8(SC_RPC_SVC_PM);
    RPC_FUNC(&msg) = U8(PM_FUNC_IS_PARTITION_STARTED);

    /* Fill in send message */
    RPC_U8(&msg, 0U) = U8(pt);

    /* Call RPC */
    sc_call_rpc(ipc, &msg, SC_FALSE);

    /* Copy out result */
    result = (sc_bool_t) U2B(RPC_R8(&msg));

    /* Return result */
    return result;
}

/** @} */

