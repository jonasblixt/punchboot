/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright 2017-2019 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*!
 * Header file for the IRQ RPC implementation.
 *
 * @addtogroup IRQ_SVC
 * @{
 */

#ifndef SC_IRQ_RPC_H
#define SC_IRQ_RPC_H

/* Includes */

/* Defines */

/*!
 * @name Defines for RPC IRQ function calls
 */
/*@{*/
#define IRQ_FUNC_UNKNOWN 0	/* Unknown function */
#define IRQ_FUNC_ENABLE 1U	/* Index for sc_irq_enable() RPC call */
#define IRQ_FUNC_STATUS 2U	/* Index for sc_irq_status() RPC call */
/*@}*/

/* Types */

/* Functions */

/*!
 * This function dispatches an incoming IRQ RPC request.
 *
 * @param[in]     caller_pt   caller partition
 * @param[in]     mu          MU message came from
 * @param[in]     msg         pointer to RPC message
 */
void irq_dispatch(sc_rm_pt_t caller_pt, sc_rsrc_t mu, sc_rpc_msg_t *msg);

#endif				/* SC_IRQ_RPC_H */

/**@}*/
