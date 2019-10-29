/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <plat.h>
#include <pb.h>
#include <string.h>
#include <plat/imx/usdhc.h>
#include <io.h>

static uint32_t _raw_cid[4];
static uint32_t _raw_csd[4];
static __a4k __no_bss struct  usdhc_adma2_desc _tbl[1024];
static __a4k __no_bss uint8_t _raw_extcsd[512];

static uint32_t usdhc_emmc_wait_for_cc(struct usdhc_device *dev,
                                       uint32_t flags)
{
    volatile uint32_t irq_status;
    uint32_t timeout = plat_get_us_tick();

    while (1) {
        irq_status = pb_read32(dev->base + USDHC_INT_STATUS);

        if (!!(irq_status & flags))
            break;

        if ((plat_get_us_tick()-timeout) > 300000)
            return PB_TIMEOUT;
    }

    pb_write32(flags, dev->base+USDHC_INT_STATUS);

    return PB_OK;
}

uint32_t usdhc_emmc_wait_for_de(struct usdhc_device *dev)
{
    volatile uint32_t irq_status;
    uint32_t timeout = 0xFFFFF;

    /* Clear all pending interrupts */

    while (1)
    {
        irq_status = pb_read32(dev->base+ USDHC_INT_STATUS);

        if (!!(irq_status & USDHC_INT_DATA_END))
            break;

        timeout--;

        if (!timeout)
            return PB_TIMEOUT;
    }

    pb_write32(USDHC_INT_DATA_END, dev->base+ USDHC_INT_STATUS);
    return PB_OK;
}

static uint32_t usdhc_emmc_send_cmd(struct usdhc_device *dev,
                                    uint8_t cmd,
                                    uint32_t arg,
                                    uint8_t resp_type)
{
    volatile uint32_t pres_state = 0x00;
    uint32_t timeout = plat_get_us_tick();
    uint32_t err;
    uint32_t command = (cmd << 24) |(resp_type << 16);

    while (1)
    {
        pres_state = pb_read32(dev->base+ USDHC_PRES_STATE);

        if ((pres_state & 0x03) == 0x00)
        {
            break;
        }

        if ((plat_get_us_tick()-timeout) > 300000)
        {
            err = PB_TIMEOUT;
            goto usdhc_cmd_fail;
        }
    }

    pb_write32(arg, dev->base+ USDHC_CMD_ARG);
    pb_write32(command, dev->base+USDHC_CMD_XFR_TYP);

    if (cmd == MMC_CMD_SEND_TUNING_BLOCK_HS200)
        err = usdhc_emmc_wait_for_cc(dev, (1<<5));
    else
        err = usdhc_emmc_wait_for_cc(dev, USDHC_INT_RESPONSE);

usdhc_cmd_fail:

    if (err == PB_TIMEOUT)
        LOG_ERR("cmd %x timeout", cmd);
    return err;
}


static uint32_t usdhc_emmc_check_status(struct usdhc_device *dev)
{
    uint32_t err;

    err = usdhc_emmc_send_cmd(dev, MMC_CMD_SEND_STATUS, 10<<16, 0x1A);

    if (err != PB_OK)
        return err;

    uint32_t result = pb_read32(dev->base+USDHC_CMD_RSP0);

    if ((result & MMC_STATUS_RDY_FOR_DATA) &&
        (result & MMC_STATUS_CURR_STATE) != MMC_STATE_PRG)
    {
        return PB_OK;
    }
    else if (result & MMC_STATUS_MASK)
    {
        LOG_ERR("Status Error: 0x%x", result);
        return PB_ERR;
    }

    return PB_OK;
}


static uint32_t usdhc_emmc_read_extcsd(struct usdhc_device *dev)
{
    uint32_t err;

    _tbl[0].cmd = ADMA2_TRAN_VALID | ADMA2_END;
    _tbl[0].len = 512;
    _tbl[0].addr = (uint32_t)(uintptr_t) _raw_extcsd;

    while (pb_read32(dev->base + USDHC_INT_STATUS) & (1 << 1))
        __asm__("nop");

    pb_write32((uint32_t)(uintptr_t) &_tbl[0], dev->base+ USDHC_ADMA_SYS_ADDR);

    /* Set ADMA 2 transfer */
    pb_write32(0x08800224, dev->base+ USDHC_PROT_CTRL);
    pb_write32(0x00010200, dev->base+ USDHC_BLK_ATT);

    err = usdhc_emmc_send_cmd(dev, MMC_CMD_SEND_EXT_CSD, 0, 0x3A);

    if (err != PB_OK)
        return err;

    err = usdhc_emmc_wait_for_de(dev);

    if (err != PB_OK)
    {
        LOG_ERR("PRESENT_STATE = 0x%x",
                    pb_read32(dev->base + USDHC_PRES_STATE));
        LOG_ERR("ADMA_ERR_STATUS = 0x%x",
                    pb_read32(dev->base+USDHC_ADMA_ERR_STATUS));
        LOG_ERR("ADMA_SYSADDR = 0x%x",
                    pb_read32(dev->base+USDHC_ADMA_SYS_ADDR));

#if LOGLEVEL > 0
        struct usdhc_adma2_desc *d = (struct usdhc_adma2_desc *) (uintptr_t)
                pb_read32(dev->base + USDHC_ADMA_SYS_ADDR);
#endif
        LOG_ERR("desc->cmd  = 0x%4.4X", d->cmd);
        LOG_ERR("desc->len  = 0x%4.4X", d->len);
        LOG_ERR("desc->addr = 0x%x", d->addr);
        return err;
    }

    return PB_OK;
}


uint32_t usdhc_emmc_switch_part(struct usdhc_device *dev, uint8_t part_no)
{
    uint32_t err;
    uint8_t value = 0;

    switch (part_no)
    {
        case PLAT_EMMC_PART_BOOT0:
            value = 0x01;
        break;
        case PLAT_EMMC_PART_BOOT1:
            value = 0x02;
        break;
        case PLAT_EMMC_PART_USER:
            value = 0x08;
        break;
        default:
            return PB_ERR;
    }

    /* Switch active partition */
    err = usdhc_emmc_send_cmd(dev, MMC_CMD_SWITCH,
                (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
                (EXT_CSD_PART_CONFIG) << 16 |
                (value << 8) ,
                0x1B);  // R1b

    if (err != PB_OK)
        return err;

    return usdhc_emmc_check_status(dev);
}


uint32_t usdhc_emmc_xfer_blocks(struct usdhc_device *dev,
                                uint32_t start_lba,
                                uint8_t *bfr,
                                uint32_t nblocks,
                                uint8_t wr,
                                uint8_t async)
{
    struct usdhc_adma2_desc *tbl_ptr = _tbl;
    uint16_t transfer_sz = 0;
    uint32_t remaining_sz = nblocks*512;
    uint32_t flags = 0;
    uint32_t err;
    uint32_t cmd = 0;
    uint8_t *buf_ptr = bfr;

    LOG_DBG("start_lba %u, nblocks %u wr %u", start_lba, nblocks, wr);

    do {
        transfer_sz = remaining_sz > 0xFE00?0xFE00:remaining_sz;
        remaining_sz -= transfer_sz;

        tbl_ptr->len = transfer_sz;
        tbl_ptr->cmd = ADMA2_TRAN_VALID;
        tbl_ptr->addr = (uint32_t)(uintptr_t) buf_ptr;

        if (!remaining_sz)
            tbl_ptr->cmd |= ADMA2_END;

        buf_ptr += transfer_sz;
        tbl_ptr++;
    } while (remaining_sz);


    pb_write32((uint32_t)(uintptr_t) _tbl, dev->base+ USDHC_ADMA_SYS_ADDR);

    /* Set ADMA 2 transfer */
    pb_write32(0x08800224, dev->base+ USDHC_PROT_CTRL);

    pb_write32(0x00000200 | (nblocks << 16), dev->base+ USDHC_BLK_ATT);

    flags = dev->mix_shadow;

    cmd = MMC_CMD_WRITE_SINGLE_BLOCK;

    if (!wr)
    {
        flags |= (1 << 4);
        cmd = MMC_CMD_READ_SINGLE_BLOCK;
    }

    if (nblocks > 1)
    {
        flags |= (1<<5) | (1<<2) | (1<<1);
        cmd = MMC_CMD_WRITE_MULTIPLE_BLOCK;
        if (!wr)
            cmd = MMC_CMD_READ_MULTIPLE_BLOCK;
    }

    pb_write32(flags, dev->base + USDHC_MIX_CTRL);

    err = usdhc_emmc_send_cmd(dev, cmd, start_lba, MMC_RSP_R1 | 0x20);

    if (err != PB_OK)
        return err;

    if (!async)
        return usdhc_emmc_wait_for_de(dev);

    return PB_OK;
}

static uint32_t usdhc_setup_hs200(struct usdhc_device *dev)
{
    uint32_t err;

    LOG_DBG("Switching to HS200 timing");

    /* Switch to HS200 timing */
    err = usdhc_emmc_send_cmd(dev, MMC_CMD_SWITCH,
                (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
                (EXT_CSD_HS_TIMING) << 16 |
                (0x02) << 8,
                0x1B);  // R1b

    if (err != PB_OK)
    {
        LOG_ERR("Could not switch to high speed timing");
        return err;
    }

    /* Switch to 8-bit/4-bit bus */
    switch (dev->bus_width)
    {
        case USDHC_BUS_8BIT:
        {
            err = usdhc_emmc_send_cmd(dev, MMC_CMD_SWITCH,
                        (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
                        (EXT_CSD_BUS_WIDTH) << 16 |
                        (EXT_CSD_BUS_WIDTH_8) << 8,
                        0x1B);  // R1b
        }
        break;
        case USDHC_BUS_4BIT:
        default:
        {
            err = usdhc_emmc_send_cmd(dev, MMC_CMD_SWITCH,
                        (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
                        (EXT_CSD_BUS_WIDTH) << 16 |
                        (EXT_CSD_BUS_WIDTH_4) << 8,
                        0x1B);  // R1b
        }
        break;
    }

    if (err != PB_OK)
    {
        LOG_ERR("Could not switch bus width");
        return err;
    }

    dev->mix_shadow = (1<<31) | 1;
    pb_write32(dev->mix_shadow | (1 << 4), dev->base+USDHC_MIX_CTRL);
    pb_write32(dev->clk | (0xF << 16)|(1 << 28), dev->base+USDHC_SYS_CTRL);

    LOG_DBG("Waiting for clock switch");
    while (1)
    {
        uint32_t pres_state = pb_read32(dev->base+USDHC_PRES_STATE);
        if (pres_state & (1 << 3))
            break;
    }

    return PB_OK;

    /* We are now in HS200 mode, Execute tuning */
/*
    pb_write32((1 << 24) | (40 << 8) | (2 << 20) | (1 << 16) , 
                    dev->base + USDHC_TUNING_CTRL);

    pb_clrbit32(USDHC_MIX_CTRL_SMPCLK_SEL, 
                dev->base + USDHC_AUTOCMD12_ERR_STATUS);

    pb_write32(0, dev->base + USDHC_DLL_CTRL);
    pb_write32(0, dev->base + USDHC_AUTOCMD12_ERR_STATUS);

    pb_clrbit32(USDHC_MIX_CTRL_AUTO_TUNE_EN |
                                 USDHC_MIX_CTRL_FBCLK_SEL,
                        dev->base + USDHC_MIX_CTRL);
    pb_setbit32(USDHC_MIX_CTRL_EXE_TUNE, 
                dev->base + USDHC_AUTOCMD12_ERR_STATUS);

    pb_write32(dev->mix_shadow | USDHC_MIX_CTRL_AUTO_TUNE_EN |
                                 USDHC_MIX_CTRL_FBCLK_SEL |
                                 (1 << 4),
                                dev->base + USDHC_MIX_CTRL);

    pb_setbit32(1 << 5, dev->base + USDHC_INT_STATUS_EN);
*/
    /* Set ADMA 2 transfer */
/*
    pb_write32(0x08800224, dev->base+ USDHC_PROT_CTRL);
    pb_write32(0x00010080, dev->base+ USDHC_BLK_ATT);

    memset(_raw_extcsd,0,512);
    err = PB_TIMEOUT;

    for (uint32_t n = 0; n < 40; n++)
    {
        _tbl[0].cmd = ADMA2_TRAN_VALID | ADMA2_END;
        _tbl[0].len = 128;
        _tbl[0].addr = (uint32_t)(uintptr_t) _raw_extcsd;

        pb_write32((uint32_t)(uintptr_t) &_tbl[0], 
                        dev->base+ USDHC_ADMA_SYS_ADDR);

        uint32_t err2 = usdhc_emmc_send_cmd(dev, 
                                  MMC_CMD_SEND_TUNING_BLOCK_HS200, 
                                  0,
                                  0x1A);

        if (err2 != PB_OK)
            break;

        reg = pb_read32(dev->base + USDHC_AUTOCMD12_ERR_STATUS);

        if (    (!(reg & USDHC_EXE_TUNE)) && 
                (reg & USDHC_SMPCLK_SEL)) 
        {
            LOG_INFO("SUCCESS!");
            err = PB_OK;
            break;
        } 

        LOG_INFO("RESP0 = %8.8X", pb_read32(dev->base + USDHC_CMD_RSP0)); 
        LOG_INFO("RESP1 = %8.8X", pb_read32(dev->base + USDHC_CMD_RSP1)); 
        LOG_INFO("RESP2 = %8.8X", pb_read32(dev->base + USDHC_CMD_RSP2)); 
        LOG_INFO("RESP3 = %8.8X", pb_read32(dev->base + USDHC_CMD_RSP3)); 

        plat_wdog_kick();       
        for (uint32_t i = 0; i < 128; i++)
            tfp_printf("%2.2X ", _raw_extcsd[i]);
        tfp_printf("\n\r");


        LOG_ERR("AUTOCMD12 = 0x%8.8"PRIx32, reg);
        LOG_ERR("INT_STATUS = 0x%8.8"PRIx32,
                    pb_read32(dev->base + USDHC_INT_STATUS));
        LOG_ERR("PRESENT_STATE = 0x%8.8"PRIx32,
                    pb_read32(dev->base + USDHC_PRES_STATE));
        LOG_ERR("ADMA_ERR_STATUS = 0x%8.8"PRIx32,
                    pb_read32(dev->base+USDHC_ADMA_ERR_STATUS));

    }

    if (err != PB_OK)
    {
        LOG_ERR("Link training failed");
        return err;
    }

    LOG_INFO("Tuning completed");

    LOG_DBG("Configured USDHC for new timing");

    return err;
*/
}

static uint32_t usdhc_setup_hs(struct usdhc_device *dev)
{
    uint32_t err;

    LOG_DBG("Switching to HS timing");
    /* Switch to HS timing */
    err = usdhc_emmc_send_cmd(dev, MMC_CMD_SWITCH,
                (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
                (EXT_CSD_HS_TIMING) << 16 |
                (1) << 8,
                0x1B);  // R1b

    if (err != PB_OK)
    {
        LOG_ERR("Could not switch to high speed timing");
        return err;
    }


    /* Switch to 8-bit/4-bit bus */
    switch (dev->bus_width)
    {
        case USDHC_BUS_8BIT:
        {
            err = usdhc_emmc_send_cmd(dev, MMC_CMD_SWITCH,
                        (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
                        (EXT_CSD_BUS_WIDTH) << 16 |
                        (EXT_CSD_DDR_BUS_WIDTH_8) << 8,
                        0x1B);  // R1b
        }
        break;
        case USDHC_BUS_4BIT:
        default:
        {
            err = usdhc_emmc_send_cmd(dev, MMC_CMD_SWITCH,
                        (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
                        (EXT_CSD_BUS_WIDTH) << 16 |
                        (EXT_CSD_DDR_BUS_WIDTH_4) << 8,
                        0x1B);  // R1b
        }
        break;
    }

    if (err != PB_OK)
    {
        LOG_ERR("Could not switch bus width");
        return err;
    }

    dev->mix_shadow = (1<<31) | (1 << 3) | 1;
    pb_write32(dev->mix_shadow | (1 << 4), dev->base+USDHC_MIX_CTRL);
    pb_write32(dev->clk | (0x0e << 16), dev->base+USDHC_SYS_CTRL);

    LOG_DBG("Waiting for clock switch");
    while (1)
    {
        uint32_t pres_state = pb_read32(dev->base+USDHC_PRES_STATE);
        if (pres_state & (1 << 3))
            break;
    }

    LOG_DBG("Configured USDHC for new timing");

    return PB_OK;
}

void usdhc_emmc_reset(struct usdhc_device *dev)
{
    usdhc_emmc_send_cmd(dev, MMC_CMD_GO_IDLE_STATE, 0, MMC_RSP_NONE);
}

uint32_t usdhc_emmc_init(struct usdhc_device *dev)
{
    uint32_t err;

    LOG_DBG("Controller reset");

    /* Reset usdhc controller */
    pb_setbit32((1 << 28) | (1 << 24), dev->base + USDHC_SYS_CTRL);
    dev->mix_shadow = 0;

    while ((pb_read32(dev->base + USDHC_SYS_CTRL) & (1<<24)) == (1 << 24))
        __asm__("nop");

    /* Send reset */
    pb_setbit32((1 << 27), dev->base + USDHC_SYS_CTRL);

    while ((pb_read32(dev->base + USDHC_SYS_CTRL) & (1<<27)) == (1 << 27))
        __asm__("nop");

    LOG_DBG("Done");

    pb_write32(0x10801080, dev->base+USDHC_WTMK_LVL);

    pb_write32(0, dev->base + USDHC_MMC_BOOT);
    pb_write32((1<<31), dev->base + USDHC_MIX_CTRL);
    pb_write32(0, dev->base + USDHC_CLK_TUNE_CTRL_STATUS);

    /* Default setup, 1.8V IO */
    pb_write32(VENDORSPEC_INIT | (1 << 1), dev->base+USDHC_VEND_SPEC);

    pb_write32(0, dev->base + USDHC_DLL_CTRL);

    pb_write32(VENDORSPEC_INIT |
               VENDORSPEC_HCKEN |
               VENDORSPEC_IPGEN |
               VENDORSPEC_CKEN |
               VENDORSPEC_PEREN,
                dev->base + USDHC_VEND_SPEC);

    /* Set clock to 400 kHz */
    /* MMC Clock = base clock (196MHz) / (prescaler * divisor )*/

    pb_write32(dev->clk_ident , dev->base+USDHC_SYS_CTRL);

    pb_write32(PROCTL_INIT |(1<<23)|(1<<27), dev->base + USDHC_PROT_CTRL);

    while (1)
    {
        volatile uint32_t pres_state = pb_read32(dev->base + USDHC_PRES_STATE);

        if ((pres_state & (1 << 3)) == (1 << 3))
            break;
    }
    LOG_DBG("Clocks started");

    /* Configure IRQ's */
    pb_write32(0xFFFFFFFF, dev->base + USDHC_INT_STATUS_EN);

    err = usdhc_emmc_send_cmd(dev, MMC_CMD_GO_IDLE_STATE, 0, MMC_RSP_NONE);

    if (err != PB_OK)
        return err;

    while (1)
    {
        volatile uint32_t pres_state = pb_read32(dev->base + USDHC_PRES_STATE);

        if ((pres_state & (1 << 3)) == (1 << 3))
            break;
    }

    LOG_DBG("Waiting for eMMC to power up");
    while (1)
    {
        err = usdhc_emmc_send_cmd(dev, MMC_CMD_SEND_OP_COND, 0xC0ff8080, 2);

        if (err != PB_OK)
            return err;
        /* Wait for eMMC to power up */
        if ( (pb_read32(dev->base+USDHC_CMD_RSP0) ==  0xC0FF8080) )
            break;
    }

    LOG_DBG("Card reset complete");
    LOG_DBG("SEND CID");

    err = usdhc_emmc_send_cmd(dev, MMC_CMD_ALL_SEND_CID, 0, 0x09);  // R2

    if (err != PB_OK)
        return err;

    _raw_cid[0] = pb_read32(dev->base+ USDHC_CMD_RSP0);
    _raw_cid[1] = pb_read32(dev->base+ USDHC_CMD_RSP1);
    _raw_cid[2] = pb_read32(dev->base+ USDHC_CMD_RSP2);
    _raw_cid[3] = pb_read32(dev->base+ USDHC_CMD_RSP3);

    LOG_DBG("cid0: %x", _raw_cid[0]);
    LOG_DBG("cid1: %x", _raw_cid[1]);
    LOG_DBG("cid2: %x", _raw_cid[2]);
    LOG_DBG("cid3: %x", _raw_cid[3]);

    /* R6 */
    err = usdhc_emmc_send_cmd(dev, MMC_CMD_SET_RELATIVE_ADDR, 10<<16, 0x1A);

    if (err != PB_OK)
        return err;

    err = usdhc_emmc_send_cmd(dev, MMC_CMD_SEND_CSD, 10<<16, 0x09);  // R2

    if (err != PB_OK)
        return err;

    _raw_csd[3] = pb_read32(dev->base+ USDHC_CMD_RSP0);
    _raw_csd[2] = pb_read32(dev->base+ USDHC_CMD_RSP1);
    _raw_csd[1] = pb_read32(dev->base+ USDHC_CMD_RSP2);
    _raw_csd[0] = pb_read32(dev->base+ USDHC_CMD_RSP3);


    LOG_DBG("csd0: %x", _raw_csd[0]);
    LOG_DBG("csd1: %x", _raw_csd[1]);
    LOG_DBG("csd2: %x", _raw_csd[2]);
    LOG_DBG("csd3: %x", _raw_csd[3]);

    LOG_DBG("Select Card");
    /* Select Card */
    err = usdhc_emmc_send_cmd(dev, MMC_CMD_SELECT_CARD, 10<<16, 0x1A);

    if (err != PB_OK)
    {
        LOG_ERR("Could not select card");
        return err;
    }

    err = usdhc_emmc_check_status(dev);

    if (err != PB_OK)
        return err;

    switch (dev->bus_mode)
    {
        case USDHC_BUS_HS200:
            err = usdhc_setup_hs200(dev);
        break;
        case USDHC_BUS_DDR52:
            err = usdhc_setup_hs(dev);
        break;
        default:
            LOG_ERR("Invalid bus mode");
            err = PB_ERR;
    }

    if (err != PB_OK)
        return err;

    err = usdhc_emmc_read_extcsd(dev);
    if (err != PB_OK)
    {
        LOG_ERR("Could not read ext CSD");
        return err;
    }

    LOG_DBG("boot bus cond: %x", _raw_extcsd[EXT_CSD_BOOT_BUS_CONDITIONS]);

    if ( (_raw_extcsd[EXT_CSD_BOOT_BUS_CONDITIONS] != dev->boot_bus_cond) )
    {
        err = usdhc_emmc_send_cmd(dev, MMC_CMD_SWITCH,
                    (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
                    (EXT_CSD_BOOT_BUS_CONDITIONS) << 16 |
                    (0x12) << 8,
                    0x1B);  // R1b

        if (err != PB_OK)
            return err;

        LOG_INFO("Configured boot bus cond");
    }

    dev->sectors =
            _raw_extcsd[EXT_CSD_SEC_CNT + 0] << 0 |
            _raw_extcsd[EXT_CSD_SEC_CNT + 1] << 8 |
            _raw_extcsd[EXT_CSD_SEC_CNT + 2] << 16 |
            _raw_extcsd[EXT_CSD_SEC_CNT + 3] << 24;


#if LOGLEVEL > 1
    char mmc_drive_str[6] =
    {
        (char)(_raw_cid[2] >> 24) & 0xFF,
        (char)(_raw_cid[2] >> 16) & 0xFF,
        (char)(_raw_cid[2] >> 8) & 0xFF,
        (char)(_raw_cid[2] ) & 0xFF,
        (char)(_raw_cid[1] >> 24) & 0xFF,
        0,
    };
#endif

    LOG_INFO("%s: %llx sectors, %llu kBytes", mmc_drive_str,
        dev->sectors, (dev->sectors)>>1);
    LOG_INFO("Partconfig: %x", _raw_extcsd[EXT_CSD_PART_CONFIG]);

    return PB_OK;
}


