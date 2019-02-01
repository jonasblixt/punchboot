/*
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <pb.h>
#include <io.h>
#include "mx8_mu.h"

void MU_EnableRxFullInt(uint32_t base, uint32_t index)
{
	uint32_t reg = pb_read32(base + MU_ACR_OFFSET1);

	reg &= ~(MU_CR_GIRn_MASK1 | MU_CR_NMI_MASK1);
	reg |= MU_CR_RIE0_MASK1 >> index;
	pb_write32(reg, base + MU_ACR_OFFSET1);
}

void MU_EnableGeneralInt(uint32_t base, uint32_t index)
{
	uint32_t reg = pb_read32(base + MU_ACR_OFFSET1);

	reg &= ~(MU_CR_GIRn_MASK1 | MU_CR_NMI_MASK1);
	reg |= MU_CR_GIE0_MASK1 >> index;
	pb_write32(reg, base + MU_ACR_OFFSET1);
}

void MU_SendMessage(uint32_t base, uint32_t regIndex, uint32_t msg)
{
	uint32_t mask = MU_SR_TE0_MASK1 >> regIndex;

	/* Wait TX register to be empty. */
	while (!(pb_read32(base + MU_ASR_OFFSET1) & mask))
		;
	pb_write32(msg, base + MU_ATR0_OFFSET1 + (regIndex * 4));
}

void MU_ReceiveMsg(uint32_t base, uint32_t regIndex, uint32_t *msg)
{
	uint32_t mask = MU_SR_RF0_MASK1 >> regIndex;

	/* Wait RX register to be full. */
	while (!(pb_read32(base + MU_ASR_OFFSET1) & mask))
		;
	*msg = pb_read32(base + MU_ARR0_OFFSET1 + (regIndex * 4));
}

void MU_Init(uint32_t base)
{
	uint32_t reg;

	reg = pb_read32(base + MU_ACR_OFFSET1);
	/* Clear GIEn, RIEn, TIEn, GIRn and ABFn. */
	reg &= ~(MU_CR_GIEn_MASK1 | MU_CR_RIEn_MASK1 | MU_CR_TIEn_MASK1
			| MU_CR_GIRn_MASK1 | MU_CR_Fn_MASK1);
	pb_write32(reg, base + MU_ACR_OFFSET1);
}
