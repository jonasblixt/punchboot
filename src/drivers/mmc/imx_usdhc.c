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

static int imx_usdhc_set_bus_clock(unsigned int clk_hz)
{
    int div = 1;
    int pre_div = 1;

    LOG_DBG("Trying to set bus clock to %u kHz", clk_hz / 1000);

    if (bus_ddr_enable)
        clk_hz *= 2;

    if (clk_hz <= 0)
        return -PB_ERR_PARAM;

    while (input_clock_hz / (16 * pre_div) > clk_hz && pre_div < 256)
        pre_div *= 2;

    while (input_clock_hz / div > clk_hz && div < 16)
        div++;

    pre_div >>= 1;
    div -= 1;
    uint16_t clk_reg = (pre_div << 8) | (div << 4);

    mmio_clrbits_32(usdhc->base + USDHC_VEND_SPEC, VENDSPEC_CARD_CLKEN);
    mmio_clrsetbits_32(usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_CLOCK_MASK, clk_reg);

    while (1) {
        uint32_t pres_state = mmio_read_32(usdhc->base + USDHC_PRES_STATE);
        if (pres_state & (1 << 3))
            break;
    }

    mmio_setbits_32(usdhc->base + USDHC_VEND_SPEC, VENDSPEC_PER_CLKEN | VENDSPEC_CARD_CLKEN);

    pb_delay_us(100);
    LOG_DBG("Actual bus rate = %d kHz", (input_clock_hz / (pre_div * div)) / 1000);
    return PB_OK;
}

static int imx_usdhc_setup(void)
{
    LOG_DBG("Base = %p, input clock = %u kHz", (void *)usdhc->base, input_clock_hz / 1000);
    /* reset the controller */
    mmio_setbits_32(usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_RSTA | USDHC_SYSCTRL_RSTT);

    /* wait for reset done */
    while ((mmio_read_32(usdhc->base + USDHC_SYSCTRL) & USDHC_SYSCTRL_RSTA))
        ;

    /* Send reset */
    mmio_setbits_32(usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_INITA);
    while ((mmio_read_32(usdhc->base + USDHC_SYSCTRL) & USDHC_SYSCTRL_INITA))
        ;

    mmio_write_32(usdhc->base + USDHC_MMC_BOOT, 0);
    mmio_write_32(usdhc->base + USDHC_MIX_CTRL, 0);
    mmio_write_32(usdhc->base + USDHC_CLK_TUNE_CTRL_STATUS, 0);

    mmio_write_32(usdhc->base + USDHC_VEND_SPEC, VENDSPEC_INIT);
    mmio_write_32(usdhc->base + USDHC_DLL_CTRL, 0);

    /* Enable interrupt status for all interrupts */
    mmio_write_32(usdhc->base + USDHC_INT_STATUS_EN, EMMC_INTSTATEN_BITS);

    /* configure as little endian */
    mmio_write_32(usdhc->base + USDHC_PROT_CTRL,
                  PROTCTRL_LE | (2 << 8) | /* ADMA 2, TODO: Defines */
                      (1 << 27)); /* Burst length enabled for INCR */

    /* Set timeout to the maximum value */
    mmio_clrsetbits_32(
        usdhc->base + USDHC_SYSCTRL, USDHC_SYSCTRL_TIMEOUT_MASK, USDHC_SYSCTRL_TIMEOUT(15));

    /* set wartermark level as 16 for safe for MMC */
    mmio_clrsetbits_32(usdhc->base + USDHC_WTMK_LVL, WMKLV_MASK, 16 | (16 << 16));

    /* Force manual delay tuning */
    mmio_write_32(usdhc->base + USDHC_DLL_CTRL, ((usdhc->delay_tap & 0x7f) << 9) | BIT(8));
    return PB_OK;
}

#define USDHC_CMD_RETRIES 1000

static int imx_usdhc_set_delay_tap(unsigned int tap)
{
    mmio_write_32(usdhc->base + USDHC_DLL_CTRL, ((tap & 0x7f) << 9) | BIT(8));
    return PB_OK;
}

static int imx_usdhc_send_cmd(uint16_t idx, uint32_t arg, uint16_t resp_type, mmc_cmd_resp_t result)
{
    unsigned int xfertype = 0, mixctl = 0, err = 0;
    bool multiple = false;
    bool data = false;
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

    switch (idx) {
    case MMC_CMD_STOP_TRANSMISSION:
        xfertype |= XFERTYPE_CMDTYP_ABORT;
        break;
    case MMC_CMD_READ_MULTIPLE_BLOCK:
        multiple = true;
        /* for read op */
        /* fallthrough */
    case MMC_CMD_SEND_TUNING_BLOCK_HS200:
    case MMC_CMD_READ_SINGLE_BLOCK:
    case MMC_CMD_SEND_EXT_CSD:
        mixctl |= MIXCTRL_DTDSEL;
        data = true;
        break;
    case MMC_CMD_WRITE_MULTIPLE_BLOCK:
        multiple = true;
        /* for data op flag */
        /* fallthrough */
    case MMC_CMD_WRITE_SINGLE_BLOCK:
        data = true;
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
        if (bus_ddr_enable) {
            mixctl |= MIXCTRL_DDREN;
        }
    }

    if (resp_type & MMC_RSP_PRESENT && resp_type != MMC_RSP_R2)
        xfertype |= XFERTYPE_RSPTYP_48;
    else if (resp_type & MMC_RSP_136)
        xfertype |= XFERTYPE_RSPTYP_136;
    else if (resp_type & MMC_RSP_BUSY)
        xfertype |= XFERTYPE_RSPTYP_48_BUSY;

    if (resp_type & MMC_RSP_BUSY)
        xfertype |= XFERTYPE_CICEN;

    if (resp_type & MMC_RSP_CRC)
        xfertype |= XFERTYPE_CCCEN;

    xfertype |= XFERTYPE_CMD(idx);

    /* Send the command */
    mmio_clrsetbits_32(usdhc->base + USDHC_MIX_CTRL, MIXCTRL_DATMASK, mixctl);
    mmio_write_32(usdhc->base + USDHC_CMD_ARG, arg);
    mmio_write_32(usdhc->base + XFERTYPE, xfertype);
    /*
     * TODO: Maybe have CONFIG_USDHC_EXTRA_DEBUG ?  */
    /*
    LOG_DBG("PROT_CTL = 0x%08x", mmio_read_32(usdhc->base + USDHC_PROT_CTRL));
    LOG_DBG("MIXCTRL  = 0x%08x", mmio_read_32(usdhc->base + USDHC_MIX_CTRL));
    LOG_DBG("XFERTYPE = 0x%08x", mmio_read_32(usdhc->base + XFERTYPE));*/
    /* Wait for the command done */
    do {
        state = mmio_read_32(usdhc->base + USDHC_INT_STATUS);
        if (cmd_retries)
            pb_delay_us(1);
    } while ((!(state & flags)) && ++cmd_retries < USDHC_CMD_RETRIES);

    if ((state & (INTSTATEN_CTOE | CMD_ERR)) || cmd_retries == USDHC_CMD_RETRIES) {
        if (cmd_retries == USDHC_CMD_RETRIES)
            err = -PB_ERR_TIMEOUT;
        else
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

    /* Wait until all of the blocks are transferred */
    if (data) {
        /* TODO: WAS 'DATA_COMPLETE', But 'DINT' is never set.
         *  Investigate what's needed when using ADMA2, what's the
         *  differance between 'TC' and 'DINT' */
        flags = INTSTATEN_TC;
        do {
            state = mmio_read_32(usdhc->base + USDHC_INT_STATUS);

            if (state & (INTSTATEN_DTOE | DATA_ERR)) {
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
    return 0;
}

static int imx_usdhc_prepare(unsigned int lba, size_t length, uintptr_t buf)
{
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
    if ((length / 512) > 0xffff) {
        return -PB_ERR_IO;
    }

#ifdef CONFIG_IMX_USDHC_XTRA_DEBUG
    LOG_DBG("lba = %d, length = %zu, buf = %p", lba, length, (void *)buf);
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

    if (length > 512) {
        mmio_write_32(usdhc->base + USDHC_BLK_ATT, 0x00000200 | ((length / 512) << 16));
    } else {
        mmio_write_32(usdhc->base + USDHC_BLK_ATT, (1 << 16) | (uint16_t)length);
    }

    return 0;
}

static int imx_usdhc_read(unsigned int lba, size_t length, uintptr_t buf)
{
    /* All transfers are performed with ADMA2 */
    if (length && buf) {
        arch_invalidate_cache_range(buf, length);
    }
    return 0;
}

static int imx_usdhc_write(unsigned int lba, size_t length, uintptr_t buf)
{
    /* All transfers are performed with ADMA2, this function is not used */
    return 0;
}

int imx_usdhc_init(const struct imx_usdhc_config *cfg, unsigned int clk_hz)
{
    static const struct mmc_hal hal = {
        .init = imx_usdhc_setup,
        .send_cmd = imx_usdhc_send_cmd,
        .set_bus_clock = imx_usdhc_set_bus_clock,
        .set_bus_width = imx_usdhc_set_bus_width,
        .prepare = imx_usdhc_prepare,
        .read = imx_usdhc_read,
        .write = imx_usdhc_write,
        .set_delay_tap = imx_usdhc_set_delay_tap,
        .max_chunk_bytes = SZ_MiB(30),
    };

    usdhc = cfg;
    input_clock_hz = clk_hz;

    return mmc_init(&hal, &cfg->mmc_config);
}
