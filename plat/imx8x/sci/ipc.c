/*
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <pb.h>
#include <plat/sci/scfw.h>
#include <plat/sci/ipc.h>
#include <plat/sci/rpc.h>
#include <stdint.h>

#include "mx8_mu.h"


void sc_call_rpc(sc_ipc_t ipc, sc_rpc_msg_t *msg, bool no_resp)
{

	sc_ipc_write(ipc, msg);
	if (!no_resp)
		sc_ipc_read(ipc, msg);

}

sc_err_t sc_ipc_open(sc_ipc_t *ipc, sc_ipc_id_t id)
{
	uint32_t base = id;
	uint32_t i;
	
	/* Get MU base associated with IPC channel */
	if ((ipc == NULL) || (base == 0))
		return SC_ERR_IPC;


	/* Init MU */
	MU_Init(base);

	/* Enable all RX interrupts */
	for (i = 0; i < MU_RR_COUNT; i++) {
		MU_EnableRxFullInt(base, i);
	}

	/* Return MU address as handle */
	*ipc = (sc_ipc_t) id;

	return SC_ERR_NONE;
}

void sc_ipc_close(sc_ipc_t ipc)
{
	uint32_t base = ipc;

	if (base != 0)
		MU_Init(base);
}

void sc_ipc_read(sc_ipc_t ipc, void *data)
{
	uint32_t base = ipc;
	sc_rpc_msg_t *msg = (sc_rpc_msg_t*) data;
	uint8_t count = 0;

	/* Check parms */
	if ((base == 0) || (msg == NULL))
		return;

	/* Read first word */
	MU_ReceiveMsg(base, 0, (uint32_t*) msg);   
	count++;

	/* Check size */
	if (msg->size > SC_RPC_MAX_MSG) {
		*((uint32_t*) msg) = 0;
		return;
	}
	
	/* Read remaining words */
	while (count < msg->size) {
		MU_ReceiveMsg(base, count % MU_RR_COUNT,
			&(msg->DATA.u32[count - 1]));   
		count++;
	}
}

void sc_ipc_write(sc_ipc_t ipc, void *data)
{
	sc_rpc_msg_t *msg = (sc_rpc_msg_t*) data;
	uint32_t base = ipc;
	uint8_t count = 0;

	/* Check parms */
	if ((base == 0) || (msg == NULL))
		return;

	/* Check size */
	if (msg->size > SC_RPC_MAX_MSG)
		return;

	/* Write first word */
	MU_SendMessage(base, 0, *((uint32_t*) msg));   
	count++;
	
	/* Write remaining words */
	while (count < msg->size) {
		MU_SendMessage(base, count % MU_TR_COUNT,
			msg->DATA.u32[count - 1]);   
		count++;
	}
}

