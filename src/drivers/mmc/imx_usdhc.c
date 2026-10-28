/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
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

static const struct imx_usdhc_config *usdhc;
static unsigned int input_clock_hz;
static struct usdhc_adma2_desc tbl[512] __section(".no_init") __aligned(64);
static bool bus_ddr_enable = false;

/* Polling timeouts, the data timeout grows with the transfer at 1 MB/s */
#define USDHC_CLK_TIMEOUT_us   10000
#define USDHC_RESET_TIMEOUT_us 100000
#define USDHC_BUSY_TIMEOUT_us  5000000
/* A command and its response: 48 + 64 + 136 clocks at 400 kHz is 0.6 ms */
#define USDHC_CMD_TIMEOUT_us   10000

/* Waits until the bits in 'mask' of register 'reg' read as 'value' */
static int usdhc_wait(unsigned int reg, uint32_t mask, uint32_t value, uint32_t timeout_us)
{
    struct pb_timeout to;

    pb_timeout_init_us(&to, timeout_us);
    while ((mmio_read_32(usdhc->base + reg) & mask) != value) {
        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("Timeout, reg 0x%03x = 0x%08x", reg, mmio_read_32(usdhc->base + reg));
            return -PB_ERR_TIMEOUT;
        }
    }
    return PB_OK;
}

static int imx_usdhc_set_bus_clock(unsigned int clk_hz)
{
    int div = 1;
    int pre_div = 1;
    int rc;

    LOG_DBG("Trying to set bus clock to %u kHz", clk_hz / 1000);

    if (bus_ddr_enable)
        clk_hz *= 2;

    if (clk_hz <= 0)
        return -PB_ERR_PARAM;

    while (input_clock_hz / (16 * pre_div) > clk_hz && pre_div < 256)
        pre_div *= 2;

    while (input_clock_hz / div > clk_hz && div < 16)
        div++;

    /* SDCLKFS: 0 is /1, 1 << n is /2^(n + 1). DVS: /(n + 1) */
    uint16_t clk_reg = ((pre_div >> 1) << 8) | ((div - 1) << 4);

    /*
     * RM: the card clock isn't gated during a divider change. The divider is
     * changed with SDSTB set and FRC_SDCLK_ON clear, then SDSTB is waited
     * for again. VEND_SPEC[14:11] (clock soft enables on the i.MX6UL) are
     * reserved on the i.MX8M.
     */
    rc = usdhc_wait(USDHC_PRES_STATE, PSTATE_SDSTB, PSTATE_SDSTB, USDHC_CLK_TIMEOUT_us);
    if (rc != PB_OK)
        return rc;

    mmio_clrbits_32(usdhc->base + USDHC_VEND_SPEC, VENDSPEC_FRC_SDCLK_ON);
    mmio_clrsetbits_32(usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_CLOCK_MASK, clk_reg);

    rc = usdhc_wait(USDHC_PRES_STATE, PSTATE_SDSTB, PSTATE_SDSTB, USDHC_CLK_TIMEOUT_us);
    if (rc != PB_OK)
        return rc;

    pb_delay_us(100);
    LOG_DBG("Actual bus rate = %u kHz", (input_clock_hz / (pre_div * div)) / 1000);
    return PB_OK;
}

static int imx_usdhc_setup(void)
{
    int rc;

    LOG_DBG("Base = %p, input clock = %u kHz", (void *)usdhc->base, input_clock_hz / 1000);
    /* reset the controller */
    mmio_setbits_32(usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_RSTA | USDHC_SYSCTRL_RSTT);

    /* wait for reset done */
    rc = usdhc_wait(USDHC_SYSCTRL, USDHC_SYSCTRL_RSTA, 0, USDHC_RESET_TIMEOUT_us);
    if (rc != PB_OK)
        return rc;

    mmio_write_32(usdhc->base + USDHC_MMC_BOOT, 0);
    mmio_write_32(usdhc->base + USDHC_MIX_CTRL, MIXCTRL_RSVD_31);
    bus_ddr_enable = false;
    mmio_write_32(usdhc->base + USDHC_CLK_TUNE_CTRL_STATUS, 0);

    mmio_write_32(usdhc->base + USDHC_VEND_SPEC, VENDSPEC_INIT);
    mmio_write_32(usdhc->base + USDHC_DLL_CTRL, 0);

    /* Enable interrupt status for all interrupts */
    mmio_write_32(usdhc->base + USDHC_INT_STATUS_EN, EMMC_INTSTATEN_BITS);

    /* configure as little endian */
    mmio_write_32(usdhc->base + USDHC_PROT_CTRL,
                  PROTCTRL_RSVD_23_21 | PROTCTRL_LE | (2 << 8) | /* ADMA 2, TODO: Defines */
                      (1 << 27)); /* Burst length enabled for INCR */

    /* Set timeout to the maximum value */
    mmio_clrsetbits_32(
        usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_TIMEOUT_MASK, USDHC_SYSCTRL_TIMEOUT(15));

    /* set wartermark level as 16 for safe for MMC */
    mmio_clrsetbits_32(usdhc->base + USDHC_WTMK_LVL, WMKLV_MASK, 16 | (16 << 16));

    /* Force manual delay tuning */
    mmio_write_32(usdhc->base + USDHC_DLL_CTRL, ((usdhc->delay_tap & 0x7f) << 9) | BIT(8));

    /*
     * RM, initialization: the 80 clocks (INITA) at about 400 kHz, once
     * CIHB and CDIHB are clear (INITA is ignored otherwise)
     */
    rc = imx_usdhc_set_bus_clock(400 * 1000);
    if (rc != PB_OK)
        return rc;

    rc = usdhc_wait(PSTATE, PSTATE_CDIHB | PSTATE_CIHB, 0, USDHC_BUSY_TIMEOUT_us);
    if (rc != PB_OK)
        return rc;

    mmio_setbits_32(usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_INITA);
    rc = usdhc_wait(USDHC_SYSCTRL, USDHC_SYSCTRL_INITA, 0, USDHC_RESET_TIMEOUT_us);
    if (rc != PB_OK)
        return rc;

    return PB_OK;
}

static int imx_usdhc_set_delay_tap(unsigned int tap)
{
    mmio_write_32(usdhc->base + USDHC_DLL_CTRL, ((tap & 0x7f) << 9) | BIT(8));
    return PB_OK;
}

static int imx_usdhc_prepare(void *buf, size_t block_size, size_t blocks);

static int imx_usdhc_request(struct mmc_cmd *cmd)
{
    unsigned int xfertype = 0, mixctl = 0, err = 0;
    unsigned int idx = cmd->idx;
    uint32_t arg = cmd->arg;
    uint16_t resp_type = cmd->resp_type;
    uint32_t *result = cmd->resp;
    bool data = cmd->blocks > 0;
    bool multiple = cmd->blocks > 1;
    bool stop = false; /* A data error left the card sending or receiving */
    unsigned int state, flags = INTSTATEN_CC | INTSTATEN_CTOE;

    /* clear all irq status */
    mmio_write_32(usdhc->base + USDHC_INT_STATUS, 0xffffffff);

    /* Wait for the bus to be idle */
    err = usdhc_wait(PSTATE, PSTATE_CDIHB | PSTATE_CIHB | PSTATE_DLA, 0, USDHC_BUSY_TIMEOUT_us);
    if (err != PB_OK)
        return err;

    mmio_write_32(usdhc->base + INTSIGEN, 0);
    pb_delay_us(1000);

    if (idx == MMC_CMD_STOP_TRANSMISSION)
        xfertype |= XFERTYPE_CMDTYP_ABORT;

    if (data) {
        err = imx_usdhc_prepare(cmd->buf, cmd->block_size, cmd->blocks);
        if (err != PB_OK)
            return err;
        if (!cmd->write)
            mixctl |= MIXCTRL_DTDSEL;
    }

    /*
     * No auto CMD12: when TC is set the auto CMD12 response may not be in
     * CMD_RSP3 yet (seen on the i.MX8M Plus, there is no flag for it), and
     * it carries the transfer's execution errors. CMD12 is sent after the
     * data instead.
     */
    if (multiple) {
        mixctl |= MIXCTRL_MSBSEL;
        mixctl |= MIXCTRL_BCEN;
    }

    if (data) {
        xfertype |= XFERTYPE_DPSEL;
        mixctl |= MIXCTRL_DMAEN;
    }

    /* RM, CMD_XFR_TYP: response type and checks */
    if (!(resp_type & MMC_RSP_PRESENT))
        ;
    else if (resp_type & MMC_RSP_136)
        xfertype |= XFERTYPE_RSPTYP_136;
    else if (resp_type & MMC_RSP_BUSY)
        xfertype |= XFERTYPE_RSPTYP_48_BUSY;
    else
        xfertype |= XFERTYPE_RSPTYP_48;

    if (resp_type & MMC_RSP_OPCODE)
        xfertype |= XFERTYPE_CICEN;

    if (resp_type & MMC_RSP_CRC)
        xfertype |= XFERTYPE_CCCEN;

    xfertype |= XFERTYPE_CMD(idx);

    /* Send the command */
    /* DDR_EN stays as set_bus_width() set it */
    mmio_clrsetbits_32(usdhc->base + USDHC_MIX_CTRL, MIXCTRL_DATMASK & ~MIXCTRL_DDREN, mixctl);
    mmio_write_32(usdhc->base + USDHC_CMD_ARG, arg);
    mmio_write_32(usdhc->base + XFERTYPE, xfertype);
    /*
     * TODO: Maybe have CONFIG_USDHC_EXTRA_DEBUG ?  */
    /*
    LOG_DBG("PROT_CTL = 0x%08x", mmio_read_32(usdhc->base + USDHC_PROT_CTRL));
    LOG_DBG("MIXCTRL  = 0x%08x", mmio_read_32(usdhc->base + USDHC_MIX_CTRL));
    LOG_DBG("XFERTYPE = 0x%08x", mmio_read_32(usdhc->base + XFERTYPE));*/
    /* Wait for the command done. The controller reports CTOE after 64 clocks
     * without a response, this only catches one that never finishes. */
    struct pb_timeout cmd_to;

    pb_timeout_init_us(&cmd_to, USDHC_CMD_TIMEOUT_us);
    do {
        state = mmio_read_32(usdhc->base + USDHC_INT_STATUS);
        if (!(state & flags) && pb_timeout_has_expired(&cmd_to)) {
            err = -PB_ERR_TIMEOUT;
            break;
        }
    } while (!(state & flags));

    if (err || (state & (INTSTATEN_CTOE | CMD_ERR))) {
        if (!err)
            err = -PB_ERR_IO;
        LOG_ERR("imx_usdhc mmc cmd %d state 0x%x errno=%d", idx, state, err);
        goto out;
    }

    /* Copy the response to the response buffer */
    if (result) {
        if (resp_type & MMC_RSP_136) {
            unsigned int cmdrsp3, cmdrsp2, cmdrsp1, cmdrsp0;

            cmdrsp3 = mmio_read_32(usdhc->base + USDHC_CMD_RSP3);
            cmdrsp2 = mmio_read_32(usdhc->base + USDHC_CMD_RSP2);
            cmdrsp1 = mmio_read_32(usdhc->base + USDHC_CMD_RSP1);
            cmdrsp0 = mmio_read_32(usdhc->base + USDHC_CMD_RSP0);
            result[3] = (cmdrsp3 << 8) | (cmdrsp2 >> 24);
            result[2] = (cmdrsp2 << 8) | (cmdrsp1 >> 24);
            result[1] = (cmdrsp1 << 8) | (cmdrsp0 >> 24);
            result[0] = (cmdrsp0 << 8);
        } else {
            result[0] = mmio_read_32(usdhc->base + USDHC_CMD_RSP0);
        }
    }

    /* Wait until all of the blocks are transferred, or for the end of busy */
    if (!data && (resp_type & MMC_RSP_BUSY)) {
        /*
         * The controller sets no TC when the busy of a command without
         * data ends (seen on the i.MX8M Plus, the RM says it does): watch
         * DAT0, which the card holds low while it is busy.
         */
        err = usdhc_wait(USDHC_PRES_STATE, PSTATE_DAT0, PSTATE_DAT0, USDHC_BUSY_TIMEOUT_us);
        if (err != PB_OK)
            goto out;
    } else if (data) {
        /* TODO: WAS 'DATA_COMPLETE', But 'DINT' is never set.
         *  Investigate what's needed when using ADMA2, what's the
         *  differance between 'TC' and 'DINT' */
        struct pb_timeout to;

        pb_timeout_init_us(&to, USDHC_BUSY_TIMEOUT_us + cmd->block_size * cmd->blocks);
        flags = INTSTATEN_TC;
        do {
            state = mmio_read_32(usdhc->base + USDHC_INT_STATUS);

            if (state & (INTSTATEN_DTOE | DATA_ERR)) {
                err = -PB_ERR_IO;
                stop = multiple;
                LOG_ERR("imx_usdhc mmc data state 0x%x", state);
                goto out;
            }
            if (pb_timeout_has_expired(&to)) {
                err = -PB_ERR_TIMEOUT;
                stop = multiple;
                LOG_ERR("imx_usdhc CMD%u timeout, state 0x%x", idx, state);
                goto out;
            }
        } while ((state & flags) != flags);
    }

    /* An open-ended transfer: CMD12, its response has the execution errors */
    if (cmd->stop) {
        struct mmc_cmd stop_cmd = {
            .idx = MMC_CMD_STOP_TRANSMISSION,
            .resp_type = MMC_RSP_R1b,
        };
        uint32_t errors = STATUS_ERROR_MASK;

        err = imx_usdhc_request(&stop_cmd);
        if (err != PB_OK)
            goto out;

        /* A read up to the last block may read ahead past it, JESD84-B51 6.8.3 */
        if (!cmd->write)
            errors &= ~STATUS_ADDRESS_OUT_OF_RANGE;

        if (stop_cmd.resp[0] & errors) {
            LOG_ERR("imx_usdhc CMD%u card status 0x%08x", idx, stop_cmd.resp[0]);
            err = -PB_ERR_IO;
        }
    }

out:
    /* Reset CMD and DATA on error */
    if (err) {
        mmio_setbits_32(usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_RSTC);
        (void)usdhc_wait(USDHC_SYSCTRL, USDHC_SYSCTRL_RSTC, 0, USDHC_RESET_TIMEOUT_us);

        if (data) {
            mmio_setbits_32(usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_RSTD);
            (void)usdhc_wait(USDHC_SYSCTRL, USDHC_SYSCTRL_RSTD, 0, USDHC_RESET_TIMEOUT_us);
        }

        /* After a data error the card is still sending or receiving: stop it */
        if (stop) {
            struct mmc_cmd stop_cmd = {
                .idx = MMC_CMD_STOP_TRANSMISSION,
                .resp_type = MMC_RSP_R1b,
            };

            (void)imx_usdhc_request(&stop_cmd);
        }
    }

    /* clear all irq status */
    mmio_write_32(usdhc->base + USDHC_INT_STATUS, 0xffffffff);

    /* All transfers are performed with ADMA2 */
    if (data && !cmd->write)
        arch_invalidate_cache_range((uintptr_t)cmd->buf, cmd->block_size * cmd->blocks);

    return err;
}

static int imx_usdhc_set_bus_width(enum mmc_bus_width width)
{
#if LOGLEVEL >= 3
    const char *bus_widths[] = {
        "Invalid", "1-Bit", "4-Bit", "8-Bit", "4-Bit DDR", "8-Bit DDR", "8-Bit DDR + Strobe",
    };
#endif

    LOG_DBG("Width = %s", bus_widths[width]);
    bus_ddr_enable = false;

    if (width == MMC_BUS_WIDTH_4BIT) {
        mmio_clrsetbits_32(usdhc->base + USDHC_PROT_CTRL, PROTCTRL_WIDTH_MASK, PROTCTRL_WIDTH_4);
    } else if (width == MMC_BUS_WIDTH_4BIT_DDR) {
        mmio_clrsetbits_32(usdhc->base + USDHC_PROT_CTRL, PROTCTRL_WIDTH_MASK, PROTCTRL_WIDTH_4);
        bus_ddr_enable = true;
    } else if (width == MMC_BUS_WIDTH_8BIT) {
        mmio_clrsetbits_32(usdhc->base + USDHC_PROT_CTRL, PROTCTRL_WIDTH_MASK, PROTCTRL_WIDTH_8);
    } else if (width == MMC_BUS_WIDTH_8BIT_DDR) {
        mmio_clrsetbits_32(usdhc->base + USDHC_PROT_CTRL, PROTCTRL_WIDTH_MASK, PROTCTRL_WIDTH_8);
        bus_ddr_enable = true;
    } else {
        LOG_ERR("Unsupported bus width");
        return -1;
    }

    /*
     * DDR_EN also selects the DDR prescaler table (RM, SYS_CTRL.SDCLKFS),
     * so it is set for all commands, not just data, and before the clock
     * is set for DDR.
     */
    if (bus_ddr_enable)
        mmio_setbits_32(usdhc->base + USDHC_MIX_CTRL, MIXCTRL_DDREN);
    else
        mmio_clrbits_32(usdhc->base + USDHC_MIX_CTRL, MIXCTRL_DDREN);

    return 0;
}

static int imx_usdhc_prepare(void *buf_p, size_t block_size, size_t blocks)
{
    uintptr_t buf = (uintptr_t)buf_p;
    size_t length = block_size * blocks;
    struct usdhc_adma2_desc *tbl_ptr = tbl;
    uintptr_t buf_ptr = buf;
    size_t bytes_to_transfer = length;
    size_t chunk_length;
    size_t n_descriptors = 0;

    /* For now we don't support transfers of more than 512*0xffff bytes.
     * This is because we set block size 512 in BLK_ATT which limits the block
     * count portion of the register to 0xffff.
     *
     * Another hard limitation on transfer size is the number of allocated
     * adma2 descriptors, which is now 512, which translates to ~32 MByte.
     *
     * */
    if (blocks > 0xffff) {
        return -PB_ERR_IO;
    }

#ifdef CONFIG_IMX_USDHC_XTRA_DEBUG
    LOG_DBG("length = %zu, buf = %p", length, (void *)buf);
#endif

    if (buf && length) {
        arch_clean_cache_range(buf, length);
    }

    do {
        if (bytes_to_transfer > ADMA2_MAX_BYTES_PER_DESC)
            chunk_length = ADMA2_MAX_BYTES_PER_DESC;
        else
            chunk_length = bytes_to_transfer;

        bytes_to_transfer -= chunk_length;

        tbl_ptr->len = chunk_length;
        tbl_ptr->cmd = ADMA2_TRAN_VALID;
        tbl_ptr->addr = (uint32_t)buf_ptr;

        if (!bytes_to_transfer)
            tbl_ptr->cmd |= ADMA2_END;

        buf_ptr += chunk_length;
        tbl_ptr++;
        n_descriptors++;
    } while (bytes_to_transfer);

    arch_clean_cache_range((uintptr_t)tbl, sizeof(struct usdhc_adma2_desc) * n_descriptors);

#ifdef CONFIG_IMX_USDHC_XTRA_DEBUG
    LOG_DBG("Configured %zu adma2 descriptors", n_descriptors);
#endif
    mmio_write_32(usdhc->base + USDHC_ADMA_SYS_ADDR, (uint32_t)(uintptr_t)tbl);

    mmio_write_32(usdhc->base + USDHC_BLK_ATT, (blocks << 16) | (uint16_t)block_size);

    return 0;
}

int imx_usdhc_init(const struct imx_usdhc_config *cfg, unsigned int clk_hz)
{
    static const struct mmc_hal hal = {
        .init = imx_usdhc_setup,
        .request = imx_usdhc_request,
        .set_bus_clock = imx_usdhc_set_bus_clock,
        .set_bus_width = imx_usdhc_set_bus_width,
        .set_delay_tap = imx_usdhc_set_delay_tap,
        .max_chunk_bytes = SZ_MiB(30),
    };

    usdhc = cfg;
    input_clock_hz = clk_hz;

    return mmc_init(&hal, &cfg->mmc_config);
}
