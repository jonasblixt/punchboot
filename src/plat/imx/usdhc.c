/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/mmc.h>

#include "usdhc.h"
#include "usdhc_private.h"

static const struct imx_usdhc_config *usdhc;
static struct  usdhc_adma2_desc tbl[1024] PB_ALIGN_4k;

static int usdhc_set_clk(unsigned int clk_hz)
{
    int div = 1;
    int pre_div = 1;

    LOG_DBG("Trying to set bus clock to %u kHz", clk_hz / 1000);

    if (clk_hz <= 0)
        return -PB_ERR_PARAM;

    while (usdhc->clock_freq_hz / (16 * pre_div) > clk_hz && pre_div < 256)
        pre_div *= 2;

    while (usdhc->clock_freq_hz / div > clk_hz && div < 16)
        div++;

    pre_div >>= 1;
    div -= 1;
    clk_hz = (pre_div << 8) | (div << 4);

    mmio_clrbits_32(usdhc->base + USDHC_VEND_SPEC, VENDSPEC_CARD_CLKEN);
    mmio_clrsetbits_32(usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_CLOCK_MASK, clk_hz);
    pb_delay_ms(10);
    mmio_setbits_32(usdhc->base + USDHC_VEND_SPEC,
                   VENDSPEC_PER_CLKEN | VENDSPEC_CARD_CLKEN);

    pb_delay_us(100);

    return PB_OK;
}

static int usdhc_init(void)
{
    unsigned int timeout = 10000;
    LOG_DBG("base = %p", (void *) usdhc->base);
    /* reset the controller */
    mmio_setbits_32(usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_RSTA);

    /* wait for reset done */
    while ((mmio_read_32(usdhc->base + USDHC_SYSCTRL) & USDHC_SYSCTRL_RSTA)) {
        if (!timeout) {
            LOG_ERR("Reset timeout");
            return -PB_TIMEOUT;
        }
        timeout--;
    }

    mmio_write_32(usdhc->base + USDHC_MMC_BOOT, 0);
    mmio_write_32(usdhc->base + USDHC_MIX_CTRL, 0);
    mmio_write_32(usdhc->base + USDHC_CLK_TUNE_CTRL_STATUS, 0);

    mmio_write_32(usdhc->base + USDHC_VEND_SPEC, VENDSPEC_INIT);
    mmio_write_32(usdhc->base + USDHC_DLL_CTRL, 0);

    /* Set the initial boot clock rate */
    usdhc_set_clk(MMC_BOOT_CLK_RATE);
    pb_delay_us(100);

    /* Enable interrupt status for all interrupts */
    mmio_write_32(usdhc->base + USDHC_INT_STATUS_EN, EMMC_INTSTATEN_BITS);

    /* configure as little endian */
    mmio_write_32(usdhc->base + USDHC_PROT_CTRL, PROTCTRL_LE);

    /* Set timeout to the maximum value */
    mmio_clrsetbits_32(usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_TIMEOUT_MASK,
              USDHC_SYSCTRL_TIMEOUT(15));

    /* set wartermark level as 16 for safe for MMC */
    mmio_clrsetbits_32(usdhc->base + USDHC_WTMK_LVL,
                       WMKLV_MASK, 16 | (16 << 16));

    return PB_OK;
}

#define USDHC_CMD_RETRIES    1000

static int usdhc_send_cmd(const struct mmc_cmd *cmd, mmc_cmd_resp_t result)
{
    unsigned int xfertype = 0, mixctl = 0, multiple = 0, data = 0, err = 0;
    unsigned int state, flags = INTSTATEN_CC | INTSTATEN_CTOE;
    unsigned int cmd_retries = 0;

    /* clear all irq status */
    mmio_write_32(usdhc->base + USDHC_INT_STATUS, 0xffffffff);

    /* Wait for the bus to be idle */
    do {
        state = mmio_read_32(usdhc->base + PSTATE);
    } while (state & (PSTATE_CDIHB | PSTATE_CIHB));

    while (mmio_read_32(usdhc->base + PSTATE) & PSTATE_DLA)
        ;

    mmio_write_32(usdhc->base + INTSIGEN, 0);
    pb_delay_us(1000);

    switch (cmd->idx) {
    case MMC_CMD_STOP_TRANSMISSION:
        xfertype |= XFERTYPE_CMDTYP_ABORT;
        break;
    case MMC_CMD_READ_MULTIPLE_BLOCK:
        multiple = 1;
        /* for read op */
        /* fallthrough */
    case MMC_CMD_READ_SINGLE_BLOCK:
    case MMC_CMD_SEND_EXT_CSD:
        mixctl |= MIXCTRL_DTDSEL;
        data = 1;
        break;
    case MMC_CMD_WRITE_MULTIPLE_BLOCK:
        multiple = 1;
        /* for data op flag */
        /* fallthrough */
    case MMC_CMD_WRITE_SINGLE_BLOCK:
        data = 1;
        break;
    default:
        break;
    }


    if (multiple) {
        mixctl |= MIXCTRL_MSBSEL;
        mixctl |= MIXCTRL_BCEN;
        mixctl |= MIXCTRL_AC12EN;
    }

    if (data) {
        xfertype |= XFERTYPE_DPSEL;
        mixctl |= MIXCTRL_DMAEN;
    }

    if (cmd->resp_type & MMC_RSP_PRESENT && cmd->resp_type != MMC_RSP_R2)
        xfertype |= XFERTYPE_RSPTYP_48;
    else if (cmd->resp_type & MMC_RSP_136)
        xfertype |= XFERTYPE_RSPTYP_136;
    else if (cmd->resp_type & MMC_RSP_BUSY)
        xfertype |= XFERTYPE_RSPTYP_48_BUSY;

    if (cmd->resp_type & MMC_RSP_BUSY)
        xfertype |= XFERTYPE_CICEN;

    if (cmd->resp_type & MMC_RSP_CRC)
        xfertype |= XFERTYPE_CCCEN;

    xfertype |= XFERTYPE_CMD(cmd->idx);

    /* Send the command */
    mmio_write_32(usdhc->base + USDHC_CMD_ARG, cmd->arg);
    mmio_clrsetbits_32(usdhc->base + USDHC_MIX_CTRL, MIXCTRL_DATMASK, mixctl);
    mmio_write_32(usdhc->base + XFERTYPE, xfertype);

    /* Wait for the command done */
    do {
        state = mmio_read_32(usdhc->base + USDHC_INT_STATUS);
        if (cmd_retries)
            pb_delay_us(1);
    } while ((!(state & flags)) && ++cmd_retries < USDHC_CMD_RETRIES);

    if ((state & (INTSTATEN_CTOE | CMD_ERR)) || cmd_retries == USDHC_CMD_RETRIES) {
        if (cmd_retries == USDHC_CMD_RETRIES)
            err = -PB_TIMEOUT;
        else
            err = -PB_ERR_IO;
        LOG_ERR("imx_usdhc mmc cmd %d state 0x%x errno=%d",
              cmd->idx, state, err);
        goto out;
    }

    /* Copy the response to the response buffer */
    if (cmd->resp_type & MMC_RSP_136) {
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

    /* Wait until all of the blocks are transferred */
    if (data) {
        flags = DATA_COMPLETE;
        do {
            state = mmio_read_32(usdhc->base + USDHC_INT_STATUS);

            if (state & DATA_ERR) {
                err = -PB_ERR_IO;
                LOG_ERR("imx_usdhc mmc data state 0x%x", state);
                goto out;
            }
        } while ((state & flags) != flags);
    }

out:
    /* Reset CMD and DATA on error */
    if (err) {
        mmio_setbits_32(usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_RSTC);
        while (mmio_read_32(usdhc->base + USDHC_SYSCTRL) & USDHC_SYSCTRL_RSTC)
            ;

        if (data) {
            mmio_setbits_32(usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_RSTD);
            while (mmio_read_32(usdhc->base + USDHC_SYSCTRL) & USDHC_SYSCTRL_RSTD)
                ;
        }
    }

    /* clear all irq status */
    mmio_write_32(usdhc->base + USDHC_INT_STATUS, 0xffffffff);

    return err;
}

static int usdhc_set_ios(unsigned int clk_hz, enum mmc_bus_width width)
{
    const char *bus_widths[] = {
        "1-Bit",
        "4-Bit",
        "8-Bit",
        "4-Bit DDR",
        "8-Bit DDR",
    };

    LOG_DBG("Freq %i kHz, Width = %s", clk_hz/1000, bus_widths[width]);
    usdhc_set_clk(clk_hz);

    if (width == MMC_BUS_WIDTH_4BIT)
        mmio_clrsetbits_32(usdhc->base + USDHC_PROT_CTRL, PROTCTRL_WIDTH_MASK,
                  PROTCTRL_WIDTH_4);
    else if (width == MMC_BUS_WIDTH_8BIT)
        mmio_clrsetbits_32(usdhc->base + USDHC_PROT_CTRL, PROTCTRL_WIDTH_MASK,
                  PROTCTRL_WIDTH_8);

    return 0;
}

static int usdhc_prepare(unsigned int lba, size_t length, uintptr_t buf)
{
    struct usdhc_adma2_desc *tbl_ptr = tbl;
    uintptr_t buf_ptr = buf;
    size_t bytes_to_transfer = length;
    size_t chunk_length;
    size_t n_descriptors = 0;
    uint32_t n_blocks = length / 512;

    LOG_DBG("lba = %d, length = %zu, buf = %p",
            lba, length, (void *) buf);

    do {
        if (bytes_to_transfer > ADMA2_MAX_BYTES_PER_DESC)
            chunk_length = ADMA2_MAX_BYTES_PER_DESC;
        else
            chunk_length = bytes_to_transfer;

        bytes_to_transfer -= chunk_length;

        tbl_ptr->len = chunk_length;
        tbl_ptr->cmd = ADMA2_TRAN_VALID;
        tbl_ptr->addr = (uint32_t) buf_ptr;

        if (!bytes_to_transfer)
            tbl_ptr->cmd |= ADMA2_END;

        buf_ptr += chunk_length;
        tbl_ptr++;
        n_descriptors++;
    } while (bytes_to_transfer);

    arch_clean_cache_range((uintptr_t) tbl,
                           sizeof(struct  usdhc_adma2_desc) * n_descriptors);

    mmio_write_32(usdhc->base + USDHC_ADMA_SYS_ADDR, (uint32_t)(uintptr_t) tbl);

    /* Set ADMA 2 transfer */
    /* TODO: FIX Bit fields */
    mmio_write_32(usdhc->base + USDHC_PROT_CTRL, 0x08800224);
    mmio_write_32(usdhc->base + USDHC_BLK_ATT, 0x00000200 | (n_blocks << 16));

    return 0;
}

static int usdhc_read(unsigned int lba, size_t length, uintptr_t buf)
{
    return 0;
}

static int usdhc_write(unsigned int lba, size_t length, uintptr_t buf)
{
    return 0;
}

int imx_usdhc_init(const struct imx_usdhc_config *cfg)
{
    static const struct mmc_hal hal = {
        .init = usdhc_init,
        .send_cmd = usdhc_send_cmd,
        .set_ios = usdhc_set_ios,
        .prepare = usdhc_prepare,
        .read = usdhc_read,
        .write = usdhc_write,
    };

    usdhc = cfg;

    return mmc_init(&hal, &cfg->mmc_config);
}

