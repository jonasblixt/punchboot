/*
 * Copyright 2017 NXP
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of NXP nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SC_SCI_H
#define _SC_SCI_H

/* Defines */

/* Includes */

#include <plat/sci/types.h>
#include <plat/sci/ipc.h>
#include <plat/sci/svc/pad/api.h>
#include <plat/sci/svc/pm/api.h>
#include <plat/sci/svc/rm/api.h>
#include <plat/sci/svc/timer/api.h>
#include <plat/sci/svc/misc/api.h>

#define MU_BASE_ADDR(id)	((0x5D1B0000UL + (id*0x10000)))
#define SC_IPC_AP_CH0       	(MU_BASE_ADDR(0))
#define SC_IPC_AP_CH1       	(MU_BASE_ADDR(1))
#define SC_IPC_AP_CH2       	(MU_BASE_ADDR(2))
#define SC_IPC_AP_CH3       	(MU_BASE_ADDR(3))
#define SC_IPC_AP_CH4       	(MU_BASE_ADDR(4))

#define SC_IPC_CH		SC_IPC_AP_CH0

#endif /* _SC_SCI_H */
