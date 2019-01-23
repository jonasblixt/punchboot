/*
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <pb.h>
#include <io.h>

enum fw_type {
	FW_1D_IMAGE,
	FW_2D_IMAGE,
};

void ddr_init(void);
void ddr_load_train_code(enum fw_type type);
void lpddr4_800M_cfg_phy(void);

inline static void reg32_write(unsigned long addr, uint32_t val)
{
	pb_write32(val, addr);
}

inline static uint32_t reg32_read(unsigned long addr)
{
	return pb_read32(addr);
}

inline static void dwc_ddrphy_apb_wr(unsigned long addr, uint32_t val)
{
    pb_write32(val, addr);
}

inline static void reg32setbit(unsigned long addr, uint32_t bit)
{
    volatile uint32_t reg = pb_read32(addr);
    reg |= (1 << bit);
    pb_write32(reg, addr);
}
