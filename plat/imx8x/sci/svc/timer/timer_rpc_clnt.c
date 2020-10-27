/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright 2017-2019 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*!
 * File containing client-side RPC functions for the TIMER service. These
 * functions are ported to clients that communicate to the SC.
 *
 * @addtogroup TIMER_SVC
 * @{
 */

/* Includes */

#include <plat/sci/sci_types.h>
#include <plat/sci/svc/rm/sci_rm_api.h>
#include <plat/sci/svc/timer/sci_timer_api.h>
#include <plat/sci/sci_rpc.h>
#include "sci_timer_rpc.h"
#include <stdlib.h>

/* Local Defines */

/* Local Types */

/* Local Functions */

sc_err_t sc_timer_set_wdog_timeout(sc_ipc_t ipc, sc_timer_wdog_time_t timeout)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 2U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_SET_WDOG_TIMEOUT);

	RPC_U32(&msg, 0U) = U32(timeout);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

sc_err_t sc_timer_set_wdog_pre_timeout(sc_ipc_t ipc,
				       sc_timer_wdog_time_t pre_timeout)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 2U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_SET_WDOG_PRE_TIMEOUT);

	RPC_U32(&msg, 0U) = U32(pre_timeout);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

sc_err_t sc_timer_set_wdog_window(sc_ipc_t ipc, sc_timer_wdog_time_t window)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 2U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_SET_WDOG_WINDOW);

	RPC_U32(&msg, 0U) = U32(window);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

sc_err_t sc_timer_start_wdog(sc_ipc_t ipc, sc_bool_t lock)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 2U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_START_WDOG);

	RPC_U8(&msg, 0U) = B2U8(lock);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

sc_err_t sc_timer_stop_wdog(sc_ipc_t ipc)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 1U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_STOP_WDOG);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

sc_err_t sc_timer_ping_wdog(sc_ipc_t ipc)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 1U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_PING_WDOG);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

sc_err_t sc_timer_get_wdog_status(sc_ipc_t ipc, sc_timer_wdog_time_t *timeout,
				  sc_timer_wdog_time_t *max_timeout,
				  sc_timer_wdog_time_t *remaining_time)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 1U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_GET_WDOG_STATUS);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	if (timeout != NULL) {
		*timeout = (sc_timer_wdog_time_t)RPC_U32(&msg, 0U);
	}
	if (max_timeout != NULL) {
		*max_timeout = (sc_timer_wdog_time_t)RPC_U32(&msg, 4U);
	}
	if (remaining_time != NULL) {
		*remaining_time = (sc_timer_wdog_time_t)RPC_U32(&msg, 8U);
	}

	return err;
}

sc_err_t sc_timer_pt_get_wdog_status(sc_ipc_t ipc, sc_rm_pt_t pt,
				     sc_bool_t *enb,
				     sc_timer_wdog_time_t *timeout,
				     sc_timer_wdog_time_t *remaining_time)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 2U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_PT_GET_WDOG_STATUS);

	RPC_U8(&msg, 0U) = U8(pt);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	if (timeout != NULL) {
		*timeout = (sc_timer_wdog_time_t)RPC_U32(&msg, 0U);
	}
	if (remaining_time != NULL) {
		*remaining_time = (sc_timer_wdog_time_t)RPC_U32(&msg, 4U);
	}
	if (enb != NULL) {
		*enb = (sc_bool_t)U2B(RPC_U8(&msg, 8U));
	}

	return err;
}

sc_err_t sc_timer_set_wdog_action(sc_ipc_t ipc, sc_rm_pt_t pt,
				  sc_timer_wdog_action_t action)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 2U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_SET_WDOG_ACTION);

	RPC_U8(&msg, 0U) = U8(pt);
	RPC_U8(&msg, 1U) = U8(action);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

sc_err_t sc_timer_set_rtc_time(sc_ipc_t ipc, uint16_t year, uint8_t mon,
			       uint8_t day, uint8_t hour, uint8_t min,
			       uint8_t sec)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 3U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_SET_RTC_TIME);

	RPC_U16(&msg, 0U) = U16(year);
	RPC_U8(&msg, 2U) = U8(mon);
	RPC_U8(&msg, 3U) = U8(day);
	RPC_U8(&msg, 4U) = U8(hour);
	RPC_U8(&msg, 5U) = U8(min);
	RPC_U8(&msg, 6U) = U8(sec);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

sc_err_t sc_timer_get_rtc_time(sc_ipc_t ipc, uint16_t *year, uint8_t *mon,
			       uint8_t *day, uint8_t *hour, uint8_t *min,
			       uint8_t *sec)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 1U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_GET_RTC_TIME);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	if (year != NULL) {
		*year = (uint16_t)RPC_U16(&msg, 0U);
	}
	if (mon != NULL) {
		*mon = (uint8_t)RPC_U8(&msg, 2U);
	}
	if (day != NULL) {
		*day = (uint8_t)RPC_U8(&msg, 3U);
	}
	if (hour != NULL) {
		*hour = (uint8_t)RPC_U8(&msg, 4U);
	}
	if (min != NULL) {
		*min = (uint8_t)RPC_U8(&msg, 5U);
	}
	if (sec != NULL) {
		*sec = (uint8_t)RPC_U8(&msg, 6U);
	}

	return err;
}

sc_err_t sc_timer_get_rtc_sec1970(sc_ipc_t ipc, uint32_t *sec)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 1U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_GET_RTC_SEC1970);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	if (sec != NULL) {
		*sec = (uint32_t)RPC_U32(&msg, 0U);
	}

	return err;
}

sc_err_t sc_timer_set_rtc_alarm(sc_ipc_t ipc, uint16_t year, uint8_t mon,
				uint8_t day, uint8_t hour, uint8_t min,
				uint8_t sec)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 3U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_SET_RTC_ALARM);

	RPC_U16(&msg, 0U) = U16(year);
	RPC_U8(&msg, 2U) = U8(mon);
	RPC_U8(&msg, 3U) = U8(day);
	RPC_U8(&msg, 4U) = U8(hour);
	RPC_U8(&msg, 5U) = U8(min);
	RPC_U8(&msg, 6U) = U8(sec);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

sc_err_t sc_timer_set_rtc_periodic_alarm(sc_ipc_t ipc, uint32_t sec)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 2U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_SET_RTC_PERIODIC_ALARM);

	RPC_U32(&msg, 0U) = U32(sec);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

sc_err_t sc_timer_cancel_rtc_alarm(sc_ipc_t ipc)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 1U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_CANCEL_RTC_ALARM);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

sc_err_t sc_timer_set_rtc_calb(sc_ipc_t ipc, int8_t count)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 2U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_SET_RTC_CALB);

	RPC_I8(&msg, 0U) = I8(count);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

sc_err_t sc_timer_set_sysctr_alarm(sc_ipc_t ipc, uint64_t ticks)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 3U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_SET_SYSCTR_ALARM);

	RPC_U32(&msg, 0U) = U32(ticks >> 32ULL);
	RPC_U32(&msg, 4U) = U32(ticks);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

sc_err_t sc_timer_set_sysctr_periodic_alarm(sc_ipc_t ipc, uint64_t ticks)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 3U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_SET_SYSCTR_PERIODIC_ALARM);

	RPC_U32(&msg, 0U) = U32(ticks >> 32ULL);
	RPC_U32(&msg, 4U) = U32(ticks);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

sc_err_t sc_timer_cancel_sysctr_alarm(sc_ipc_t ipc)
{
	sc_rpc_msg_t msg;
	sc_err_t err;

	RPC_VER(&msg) = SC_RPC_VERSION;
	RPC_SIZE(&msg) = 1U;
	RPC_SVC(&msg) = U8(SC_RPC_SVC_TIMER);
	RPC_FUNC(&msg) = U8(TIMER_FUNC_CANCEL_SYSCTR_ALARM);

	sc_call_rpc(ipc, &msg, SC_FALSE);

	err = (sc_err_t)RPC_R8(&msg);

	return err;
}

/**@}*/
