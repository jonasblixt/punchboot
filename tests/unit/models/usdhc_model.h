/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A register model of the NXP uSDHC with a card model on its bus, written
 * from the uSDHC chapters of the i.MX6UL (chapter 56) and i.MX8M Plus
 * (10.3) reference manuals.
 *
 * The test routes fake_mmio_read/write (fakes/mmio) for the controller's
 * address range here. The model runs commands, ADMA2 transfers and auto
 * CMD12 the way the RM describes, sets the INT_STATUS bits (only those
 * enabled in INT_STATUS_EN) and logs programming that the RM does not allow
 * as violations (bench.h), for example:
 *
 *  - reserved bits not written as the RM requires ("always write as ...")
 *  - SDCLKFS/DVS changed while SDSTB is 0 or FRC_SDCLK_ON is set, an
 *    SDCLKFS with more than one bit set
 *  - response checks that don't match the response type (RM Table 10-47)
 *  - a command while CIHB is set, a data command while CDIHB is set
 *  - ADMA2 descriptors that are not word aligned or of a length that is not
 *    a multiple of 4, a descriptor list that doesn't match the block count
 *  - MIX_CTRL written while a data transfer is in progress
 *  - no 80 clock initialization (INITA) before the first command
 *
 * Commands and data run when CMD_XFR_TYP is written, busy (R1b and after a
 * write) lasts as long as the card holds DAT0, and every register read
 * advances simulated time by 1 us. As on the i.MX8M Plus, and unlike the
 * RM, the end of busy of a command without data sets no TC: a driver has to
 * watch DAT0 (PRES_STATE.DLSL). Also as on that SoC, the auto CMD12
 * response reaches CMD_RSP3 some microseconds after TC.
 *
 * DMA addresses are 32 bits: buffers must be below 4 GiB, so tests link
 * with -no-pie and use static buffers.
 *
 * Not modeled: PIO through DATA_BUFF_ACC_PORT, SDMA, ADMA1, tuning circuits
 * (tuning commands get their data like a read), the DLLs, CQE, boot mode,
 * interrupts (INT_SIGNAL_EN).
 */

#ifndef TESTS_UNIT_MODELS_USDHC_MODEL_H
#define TESTS_UNIT_MODELS_USDHC_MODEL_H

#include "mmc_bus.h"

enum usdhc_soc {
    USDHC_IMX6UL, /* VEND_SPEC clock soft enables, DTOCV = 2^(13+n) */
    USDHC_IMX8M, /* i.MX8M Plus RM; VEND_SPEC[14:11] reserved */
};

struct usdhc_model_config {
    uintptr_t base;
    unsigned int base_clk_hz; /* Root clock into the controller */
    enum usdhc_soc soc;
    struct bus_card *card;
    bool write_protect; /* WP pin level: protected */
    /* VSELECT drives the signal voltage (SD slots). Otherwise it is fixed */
    bool vselect_wired;
    unsigned int vccq_mv; /* Signal voltage when VSELECT isn't wired */
};

void usdhc_model_init(const struct usdhc_model_config *cfg);

bool usdhc_model_owns(uintptr_t addr);
uint32_t usdhc_model_read(uintptr_t addr);
void usdhc_model_write(uintptr_t addr, uint32_t value);

/* The bus as the controller drives it */
const struct bus_cfg *usdhc_model_bus(void);

/* Commands sent on the bus, auto CMD12 included */
unsigned int usdhc_model_cmds(void);

/* The next data transfer fails with an ADMA error (DMAE) */
void usdhc_model_fail_dma(void);

#endif // TESTS_UNIT_MODELS_USDHC_MODEL_H
