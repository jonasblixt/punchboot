/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * NXP uSDHC host controller, the HAL of mmc_core. Every transfer uses
 * ADMA2. The register fields are in imx_usdhc_private.h; the procedures
 * follow the uSDHC chapters of the i.MX6UL and i.MX8M Plus RMs.
 */

#include <arch/arch.h>
#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <stdio.h>
#include <string.h>

#include "imx_usdhc_private.h"
#include <drivers/mmc/imx_usdhc.h>
#include <drivers/mmc/mmc_core.h>

/* 512 descriptors of ADMA2_MAX_BYTES: a little over 30 MiB per request */
#define ADMA2_DESCS            512
#define USDHC_MAX_CHUNK_BYTES  SZ_MiB(30)

/* Polling timeouts, the data timeout grows with the transfer at 1 MB/s */
#define USDHC_CLK_TIMEOUT_us   10000
#define USDHC_RESET_TIMEOUT_us 100000
#define USDHC_BUSY_TIMEOUT_us  5000000
/* A command and its response: 48 + 64 + 136 clocks at 400 kHz is 0.6 ms */
#define USDHC_CMD_TIMEOUT_us   10000

/* The identification clock, JESD84-B51 and the RM's initialization */
#define USDHC_INIT_CLK_HZ      400000

#define INT_STATUS_ENABLED                                                                       \
    (INT_CC | INT_TC | INT_DINT | INT_BWR | INT_BRR | INT_CINT | INT_CTOE | INT_CCE | INT_CEBE | \
     INT_CIE | INT_DTOE | INT_DCE | INT_DEBE | INT_DMAE | INT_AC12E)

#define VEND_SPEC_INIT                                                           \
    (VEND_SPEC_RSVD_29 | VEND_SPEC_SOFT_EN_MASK | VEND_SPEC_AC12_WR_CHKBUSY_EN | \
     VEND_SPEC_EXT_DMA_EN)

/* The host controllers on the board */
#define USDHC_MAX_INSTANCES 2

struct imx_usdhc {
    struct usdhc_adma2_desc *tbl;
    const struct imx_usdhc_config *cfg;
    uintptr_t base;
    unsigned int input_clock_hz;
    bool bus_ddr_enable;
    struct mmc_hal hal;
};

static struct imx_usdhc instances[USDHC_MAX_INSTANCES];
/* Not zeroed at boot */
static struct usdhc_adma2_desc tbls[USDHC_MAX_INSTANCES][ADMA2_DESCS] __section(".no_init")
    __aligned(64);

static uint32_t usdhc_read(struct imx_usdhc *u, unsigned int reg)
{
    return mmio_read_32(u->base + reg);
}

static void usdhc_write(struct imx_usdhc *u, unsigned int reg, uint32_t value)
{
    mmio_write_32(u->base + reg, value);
}

/* Waits until the bits in 'mask' of register 'reg' read as 'value' */
static int usdhc_wait(struct imx_usdhc *u,
                      unsigned int reg,
                      uint32_t mask,
                      uint32_t value,
                      uint32_t timeout_us)
{
    struct pb_timeout to;

    pb_timeout_init_us(&to, timeout_us);
    while ((usdhc_read(u, reg) & mask) != value) {
        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("Timeout, reg 0x%03x = 0x%08x", reg, usdhc_read(u, reg));
            return -PB_ERR_TIMEOUT;
        }
    }
    return PB_OK;
}

/* A self clearing reset or INITA */
static int usdhc_sys_ctrl_pulse(struct imx_usdhc *u, uint32_t bit)
{
    mmio_setbits_32(u->base + USDHC_SYS_CTRL, bit);
    return usdhc_wait(u, USDHC_SYS_CTRL, bit, 0, USDHC_RESET_TIMEOUT_us);
}

static int imx_usdhc_set_bus_clock(void *priv, unsigned int clk_hz)
{
    struct imx_usdhc *u = priv;
    unsigned int div = 1;
    unsigned int pre_div = 1;
    int rc;

    LOG_DBG("Trying to set bus clock to %u kHz", clk_hz / 1000);

    if (clk_hz == 0)
        return -PB_ERR_PARAM;

    /* The DDR prescaler table divides by two more */
    if (u->bus_ddr_enable)
        clk_hz *= 2;

    while (u->input_clock_hz / (DVS_MAX_DIV * pre_div) > clk_hz && pre_div < SDCLKFS_MAX_DIV)
        pre_div *= 2;

    while (u->input_clock_hz / div > clk_hz && div < DVS_MAX_DIV)
        div++;

    /*
     * RM: the card clock isn't gated during a divider change. The divider is
     * changed with SDSTB set and FRC_SDCLK_ON clear, then SDSTB is waited
     * for again.
     */
    rc = usdhc_wait(u, USDHC_PRES_STATE, PRES_STATE_SDSTB, PRES_STATE_SDSTB, USDHC_CLK_TIMEOUT_us);
    if (rc != PB_OK)
        return rc;

    mmio_clrbits_32(u->base + USDHC_VEND_SPEC, VEND_SPEC_FRC_SDCLK_ON);
    mmio_clrsetbits_32(u->base + USDHC_SYS_CTRL,
                       SYS_CTRL_SDCLKFS_MASK | SYS_CTRL_DVS_MASK,
                       SYS_CTRL_SDCLKFS(pre_div >> 1) | SYS_CTRL_DVS(div - 1));

    rc = usdhc_wait(u, USDHC_PRES_STATE, PRES_STATE_SDSTB, PRES_STATE_SDSTB, USDHC_CLK_TIMEOUT_us);
    if (rc != PB_OK)
        return rc;

    pb_delay_us(100);
    LOG_DBG("Actual bus rate = %u kHz", (u->input_clock_hz / (pre_div * div)) / 1000);
    return PB_OK;
}

static int imx_usdhc_setup(void *priv)
{
    struct imx_usdhc *u = priv;
    int rc;

    LOG_DBG("Base = %p, input clock = %u kHz", (void *)u->base, u->input_clock_hz / 1000);

    mmio_setbits_32(u->base + USDHC_SYS_CTRL, SYS_CTRL_RSTA | SYS_CTRL_RSTT);
    rc = usdhc_wait(u, USDHC_SYS_CTRL, SYS_CTRL_RSTA, 0, USDHC_RESET_TIMEOUT_us);
    if (rc != PB_OK)
        return rc;

    usdhc_write(u, USDHC_MMC_BOOT, 0);
    usdhc_write(u, USDHC_MIX_CTRL, MIX_CTRL_RSVD_31);
    u->bus_ddr_enable = false;
    usdhc_write(u, USDHC_CLK_TUNE_CTRL_STATUS, 0);
    usdhc_write(u, USDHC_VEND_SPEC, VEND_SPEC_INIT);
    usdhc_write(u, USDHC_DLL_CTRL, 0);
    usdhc_write(u, USDHC_INT_STATUS_EN, INT_STATUS_ENABLED);

    usdhc_write(u,
                USDHC_PROT_CTRL,
                PROT_CTRL_BURST_LEN_EN_INCR | PROT_CTRL_RSVD_23_21 |
                    PROT_CTRL_DMASEL(DMASEL_ADMA2) | PROT_CTRL_EMODE(EMODE_LITTLE_ENDIAN));

    mmio_clrsetbits_32(u->base + USDHC_SYS_CTRL, SYS_CTRL_DTOCV_MASK, SYS_CTRL_DTOCV(DTOCV_MAX));

    mmio_clrsetbits_32(u->base + USDHC_WTMK_LVL,
                       WTMK_LVL_WR_WML_MASK | WTMK_LVL_RD_WML_MASK,
                       WTMK_LVL_WR_WML(WTMK_LVL_DMA) | WTMK_LVL_RD_WML(WTMK_LVL_DMA));

    /* The board's fixed delay line tap */
    usdhc_write(
        u, USDHC_DLL_CTRL, DLL_CTRL_SLV_OVERRIDE_VAL(u->cfg->delay_tap) | DLL_CTRL_SLV_OVERRIDE);

    /*
     * RM, initialization: the 80 clocks (INITA) at about 400 kHz, once
     * CIHB and CDIHB are clear (INITA is ignored otherwise)
     */
    rc = imx_usdhc_set_bus_clock(u, USDHC_INIT_CLK_HZ);
    if (rc != PB_OK)
        return rc;

    rc = usdhc_wait(
        u, USDHC_PRES_STATE, PRES_STATE_CDIHB | PRES_STATE_CIHB, 0, USDHC_BUSY_TIMEOUT_us);
    if (rc != PB_OK)
        return rc;

    return usdhc_sys_ctrl_pulse(u, SYS_CTRL_INITA);
}

static int imx_usdhc_set_delay_tap(void *priv, unsigned int tap)
{
    struct imx_usdhc *u = priv;

    usdhc_write(u, USDHC_DLL_CTRL, DLL_CTRL_SLV_OVERRIDE_VAL(tap) | DLL_CTRL_SLV_OVERRIDE);
    return PB_OK;
}

/* The ADMA2 descriptors and block attributes of a transfer */
static int imx_usdhc_prepare(struct imx_usdhc *u, void *buf, size_t block_size, size_t blocks)
{
    uintptr_t buf_ptr = (uintptr_t)buf;
    size_t length = block_size * blocks;
    size_t n = 0;

    if (blocks > BLK_ATT_BLKCNT_MAX || length > ADMA2_DESCS * ADMA2_MAX_BYTES)
        return -PB_ERR_IO;

    arch_clean_cache_range(buf_ptr, length);

    while (length) {
        size_t chunk = (length > ADMA2_MAX_BYTES) ? ADMA2_MAX_BYTES : length;

        length -= chunk;
        u->tbl[n].len = chunk;
        u->tbl[n].attr = ADMA2_ATTR_ACT_TRAN | ADMA2_ATTR_VALID | (length ? 0 : ADMA2_ATTR_END);
        u->tbl[n].addr = (uint32_t)buf_ptr;
        buf_ptr += chunk;
        n++;
    }

    arch_clean_cache_range((uintptr_t)u->tbl, sizeof(u->tbl[0]) * n);

    usdhc_write(u, USDHC_ADMA_SYS_ADDR, (uint32_t)(uintptr_t)u->tbl);
    usdhc_write(u, USDHC_BLK_ATT, BLK_ATT_BLKCNT(blocks) | BLK_ATT_BLKSIZE(block_size));
    return PB_OK;
}

/* RM, CMD_XFR_TYP: the response type and its checks */
static uint32_t xfr_typ_response(uint16_t resp_type)
{
    uint32_t xfr_typ;

    if (!(resp_type & MMC_RSP_PRESENT))
        xfr_typ = CMD_XFR_TYP_RSPTYP(RSPTYP_NONE);
    else if (resp_type & MMC_RSP_136)
        xfr_typ = CMD_XFR_TYP_RSPTYP(RSPTYP_136);
    else if (resp_type & MMC_RSP_BUSY)
        xfr_typ = CMD_XFR_TYP_RSPTYP(RSPTYP_48_BUSY);
    else
        xfr_typ = CMD_XFR_TYP_RSPTYP(RSPTYP_48);

    if (resp_type & MMC_RSP_OPCODE)
        xfr_typ |= CMD_XFR_TYP_CICEN;
    if (resp_type & MMC_RSP_CRC)
        xfr_typ |= CMD_XFR_TYP_CCCEN;

    return xfr_typ;
}

static void read_response(struct imx_usdhc *u, struct mmc_cmd *cmd)
{
    if (cmd->resp_type & MMC_RSP_136) {
        uint32_t rsp3 = usdhc_read(u, USDHC_CMD_RSP3);
        uint32_t rsp2 = usdhc_read(u, USDHC_CMD_RSP2);
        uint32_t rsp1 = usdhc_read(u, USDHC_CMD_RSP1);
        uint32_t rsp0 = usdhc_read(u, USDHC_CMD_RSP0);

        cmd->resp[3] = (rsp3 << 8) | (rsp2 >> 24);
        cmd->resp[2] = (rsp2 << 8) | (rsp1 >> 24);
        cmd->resp[1] = (rsp1 << 8) | (rsp0 >> 24);
        cmd->resp[0] = rsp0 << 8;
    } else {
        cmd->resp[0] = usdhc_read(u, USDHC_CMD_RSP0);
    }
}

static int imx_usdhc_request(void *priv, struct mmc_cmd *cmd)
{
    struct imx_usdhc *u = priv;
    bool data = cmd->blocks > 0;
    bool multiple = cmd->blocks > 1;
    bool stop = false; /* A data error left the card sending or receiving */
    uint32_t xfr_typ = CMD_XFR_TYP_CMDINX(cmd->idx) | xfr_typ_response(cmd->resp_type);
    uint32_t mix = 0;
    uint32_t state;
    int err;

    usdhc_write(u, USDHC_INT_STATUS, INT_ALL);

    /* Wait for the bus to be idle */
    err = usdhc_wait(u,
                     USDHC_PRES_STATE,
                     PRES_STATE_CDIHB | PRES_STATE_CIHB | PRES_STATE_DLA,
                     0,
                     USDHC_BUSY_TIMEOUT_us);
    if (err != PB_OK)
        return err;

    usdhc_write(u, USDHC_INT_SIGNAL_EN, 0);

    if (cmd->idx == MMC_CMD_STOP_TRANSMISSION)
        xfr_typ |= CMD_XFR_TYP_CMDTYP(CMDTYP_ABORT);

    if (data) {
        err = imx_usdhc_prepare(u, cmd->buf, cmd->block_size, cmd->blocks);
        if (err != PB_OK)
            return err;

        xfr_typ |= CMD_XFR_TYP_DPSEL;
        mix |= MIX_CTRL_DMAEN;
        if (!cmd->write)
            mix |= MIX_CTRL_DTDSEL;
    }

    /*
     * No auto CMD12: when TC is set the auto CMD12 response may not be in
     * CMD_RSP3 yet (seen on the i.MX8M Plus, there is no flag for it), and
     * it carries the transfer's execution errors. CMD12 is sent after the
     * data instead.
     */
    if (multiple)
        mix |= MIX_CTRL_MSBSEL | MIX_CTRL_BCEN;

    /* DDR_EN stays as set_bus_width() set it */
    mmio_clrsetbits_32(u->base + USDHC_MIX_CTRL, MIX_CTRL_XFER_MASK, mix);
    usdhc_write(u, USDHC_CMD_ARG, cmd->arg);
    usdhc_write(u, USDHC_CMD_XFR_TYP, xfr_typ);

    /* Wait for the command done. The controller reports CTOE after 64 clocks
     * without a response, this only catches one that never finishes. */
    struct pb_timeout cmd_to;

    pb_timeout_init_us(&cmd_to, USDHC_CMD_TIMEOUT_us);
    do {
        state = usdhc_read(u, USDHC_INT_STATUS);
        if (!(state & (INT_CC | INT_CTOE)) && pb_timeout_has_expired(&cmd_to)) {
            err = -PB_ERR_TIMEOUT;
            break;
        }
    } while (!(state & (INT_CC | INT_CTOE)));

    if (err || (state & INT_CMD_ERRORS)) {
        if (!err)
            err = -PB_ERR_IO;
        LOG_ERR("imx_usdhc mmc cmd %d state 0x%x errno=%d", cmd->idx, state, err);
        goto out;
    }

    read_response(u, cmd);

    /*
     * Wait until all of the blocks are transferred, or for the end of busy.
     * With ADMA2 TC is the end of the transfer, DINT only comes from
     * descriptors with the Int attribute.
     */
    if (!data && (cmd->resp_type & MMC_RSP_BUSY)) {
        /*
         * The controller sets no TC when the busy of a command without
         * data ends (seen on the i.MX8M Plus, the RM says it does): watch
         * DAT0, which the card holds low while it is busy.
         */
        err = usdhc_wait(
            u, USDHC_PRES_STATE, PRES_STATE_DAT0, PRES_STATE_DAT0, USDHC_BUSY_TIMEOUT_us);
        if (err != PB_OK)
            goto out;
    } else if (data) {
        struct pb_timeout to;

        pb_timeout_init_us(&to, USDHC_BUSY_TIMEOUT_us + cmd->block_size * cmd->blocks);
        do {
            state = usdhc_read(u, USDHC_INT_STATUS);

            if (state & INT_DATA_ERRORS) {
                err = -PB_ERR_IO;
                stop = multiple;
                LOG_ERR("imx_usdhc mmc data state 0x%x", state);
                goto out;
            }
            if (pb_timeout_has_expired(&to)) {
                err = -PB_ERR_TIMEOUT;
                stop = multiple;
                LOG_ERR("imx_usdhc CMD%u timeout, state 0x%x", cmd->idx, state);
                goto out;
            }
        } while (!(state & INT_TC));
    }

    /* An open-ended transfer: CMD12, its response has the execution errors */
    if (cmd->stop) {
        struct mmc_cmd stop_cmd = {
            .idx = MMC_CMD_STOP_TRANSMISSION,
            .resp_type = MMC_RSP_R1b,
        };
        uint32_t errors = STATUS_ERROR_MASK;

        err = imx_usdhc_request(u, &stop_cmd);
        if (err != PB_OK)
            goto out;

        /* A read up to the last block may read ahead past it, JESD84-B51 6.8.3 */
        if (!cmd->write)
            errors &= ~STATUS_ADDRESS_OUT_OF_RANGE;

        if (stop_cmd.resp[0] & errors) {
            LOG_ERR("imx_usdhc CMD%u card status 0x%08x", cmd->idx, stop_cmd.resp[0]);
            err = -PB_ERR_IO;
        }
    }

out:
    /* Reset CMD and DATA on error */
    if (err) {
        mmio_setbits_32(u->base + USDHC_SYS_CTRL, SYS_CTRL_RSTC);
        (void)usdhc_wait(u, USDHC_SYS_CTRL, SYS_CTRL_RSTC, 0, USDHC_RESET_TIMEOUT_us);

        if (data) {
            mmio_setbits_32(u->base + USDHC_SYS_CTRL, SYS_CTRL_RSTD);
            (void)usdhc_wait(u, USDHC_SYS_CTRL, SYS_CTRL_RSTD, 0, USDHC_RESET_TIMEOUT_us);
        }

        /* After a data error the card is still sending or receiving: stop it */
        if (stop) {
            struct mmc_cmd stop_cmd = {
                .idx = MMC_CMD_STOP_TRANSMISSION,
                .resp_type = MMC_RSP_R1b,
            };

            (void)imx_usdhc_request(u, &stop_cmd);
        }
    }

    usdhc_write(u, USDHC_INT_STATUS, INT_ALL);

    if (data && !cmd->write)
        arch_invalidate_cache_range((uintptr_t)cmd->buf, cmd->block_size * cmd->blocks);

    return err;
}

static int imx_usdhc_set_bus_width(void *priv, enum mmc_bus_width width)
{
    struct imx_usdhc *u = priv;
#if LOGLEVEL >= 3
    const char *bus_widths[] = {
        "Invalid", "1-Bit", "4-Bit", "8-Bit", "4-Bit DDR", "8-Bit DDR", "8-Bit DDR + Strobe",
    };
#endif
    uint32_t dtw;

    LOG_DBG("Width = %s", bus_widths[width]);

    switch (width) {
    case MMC_BUS_WIDTH_4BIT:
        dtw = DTW_4BIT;
        u->bus_ddr_enable = false;
        break;
    case MMC_BUS_WIDTH_4BIT_DDR:
        dtw = DTW_4BIT;
        u->bus_ddr_enable = true;
        break;
    case MMC_BUS_WIDTH_8BIT:
        dtw = DTW_8BIT;
        u->bus_ddr_enable = false;
        break;
    case MMC_BUS_WIDTH_8BIT_DDR:
        dtw = DTW_8BIT;
        u->bus_ddr_enable = true;
        break;
    default:
        LOG_ERR("Unsupported bus width");
        return -PB_ERR_PARAM;
    }

    mmio_clrsetbits_32(u->base + USDHC_PROT_CTRL, PROT_CTRL_DTW_MASK, PROT_CTRL_DTW(dtw));

    /*
     * DDR_EN also selects the DDR prescaler table (RM, SYS_CTRL.SDCLKFS),
     * so it is set for all commands, not just data, and before the clock
     * is set for DDR.
     */
    if (u->bus_ddr_enable)
        mmio_setbits_32(u->base + USDHC_MIX_CTRL, MIX_CTRL_DDR_EN);
    else
        mmio_clrbits_32(u->base + USDHC_MIX_CTRL, MIX_CTRL_DDR_EN);

    return PB_OK;
}

int imx_usdhc_init(const struct imx_usdhc_config *cfg, unsigned int clk_hz)
{
    struct imx_usdhc *u = NULL;

    for (unsigned int i = 0; i < USDHC_MAX_INSTANCES && u == NULL; i++) {
        if (instances[i].cfg != NULL && instances[i].base == cfg->base)
            u = &instances[i];
    }
    for (unsigned int i = 0; i < USDHC_MAX_INSTANCES && u == NULL; i++) {
        if (instances[i].cfg == NULL) {
            u = &instances[i];
            u->tbl = tbls[i];
        }
    }
    if (u == NULL)
        return -PB_ERR_MEM;

    u->cfg = cfg;
    u->base = cfg->base;
    u->input_clock_hz = clk_hz;
    u->bus_ddr_enable = false;
    u->hal = (struct mmc_hal){
        .priv = u,
        .init = imx_usdhc_setup,
        .request = imx_usdhc_request,
        .set_bus_clock = imx_usdhc_set_bus_clock,
        .set_bus_width = imx_usdhc_set_bus_width,
        .set_delay_tap = imx_usdhc_set_delay_tap,
        .max_chunk_bytes = USDHC_MAX_CHUNK_BYTES,
    };

    return mmc_init(&u->hal, &cfg->mmc_config, NULL);
}
