/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <plat.h>
#include <pb.h>
#include <tinyprintf.h>

#include <plat/imx6ul/usdhc.h>
#include <io.h>
#include <plat/imx6ul/imx_regs.h>

static uint32_t _iobase = 0x02190000;
static uint32_t _raw_cid[4];
static uint32_t _raw_csd[4];
static uint8_t __no_bss _raw_extcsd[512];
static struct __no_bss usdhc_adma2_desc tbl[256];
static uint32_t _sectors = 0;

static void usdhc_emmc_wait_for_cc(void)
{
    volatile uint32_t irq_status;
    /* Clear all pending interrupts */

    while (1) {
        irq_status = pb_readl(_iobase+ USDHC_INT_STATUS);

        if (irq_status & USDHC_INT_RESPONSE)
            break;

    }

    pb_writel(USDHC_INT_RESPONSE, _iobase+USDHC_INT_STATUS);
}

static void usdhc_emmc_wait_for_de(void)
{
    volatile uint32_t irq_status;
    /* Clear all pending interrupts */

    while (1) {
        irq_status = pb_readl(_iobase+ USDHC_INT_STATUS);

        if (irq_status & USDHC_INT_DATA_END)
            break;
    }

    pb_writel(USDHC_INT_DATA_END, _iobase+ USDHC_INT_STATUS);
}

static void usdhc_emmc_send_cmd(uint8_t cmd, uint32_t arg, 
                                uint8_t resp_type) {

    volatile uint32_t pres_state = 0x00;

    while (1) {
        pres_state = pb_readl(_iobase+ USDHC_PRES_STATE);

        if ((pres_state & 0x03) == 0x00) {
            break;
        }
    }
    
    pb_writel(arg, _iobase+ USDHC_CMD_ARG);
    pb_writel(USDHC_MAKE_CMD(cmd, resp_type), _iobase+
                                USDHC_CMD_XFR_TYP);

    usdhc_emmc_wait_for_cc();
}


static int usdhc_emmc_check_status(void) {

    usdhc_emmc_send_cmd (MMC_CMD_SEND_STATUS, 10<<16,0x1A);

    uint32_t result = pb_readl(_iobase+USDHC_CMD_RSP0);

    if ((result & MMC_STATUS_RDY_FOR_DATA) &&
        (result & MMC_STATUS_CURR_STATE) != MMC_STATE_PRG) {
        return 0;
    } else if (result & MMC_STATUS_MASK) {
        LOG_ERR("Status Error: 0x%8.8lX",result);
        return -1;
    }

    return 0;
}


static void usdhc_emmc_read_extcsd(void) {

    tbl->cmd = ADMA2_TRAN_VALID | ADMA2_END;
    tbl->len = 512;
    tbl->addr = (uint32_t) _raw_extcsd;
 
    pb_writel((uint32_t) tbl, _iobase+ USDHC_ADMA_SYS_ADDR);

	/* Set ADMA 2 transfer */
	pb_writel(0x08800224, _iobase+ USDHC_PROT_CTRL);

	pb_writel(0x00010200, _iobase+ USDHC_BLK_ATT);
	usdhc_emmc_send_cmd(MMC_CMD_SEND_EXT_CSD, 0, 0x3A);
	
    usdhc_emmc_wait_for_de();
}


void usdhc_emmc_switch_part(uint8_t part_no) {

    uint8_t part_config = _raw_extcsd[EXT_CSD_PART_CONFIG];

    uint8_t value = part_config & ~0x07;
    value |= part_no;
    
    if (part_no == 0) 
        value = 0x10;


    /* Switch to HS timing */
    usdhc_emmc_send_cmd(MMC_CMD_SWITCH, 
                (MMC_SWITCH_MODE_WRITE_BYTE << 24) | 
                (EXT_CSD_PART_CONFIG) << 16 |
                (value << 8) ,
                0x1B); // R1b
 
   usdhc_emmc_check_status();

   //tfp_printf ("Switched to part: %i (%2.2X)\n\r", part_no, value);
}

void usdhc_emmc_xfer_blocks(uint32_t start_lba, uint8_t *bfr, 
                            uint32_t nblocks, uint8_t wr, uint8_t async) {
    struct usdhc_adma2_desc *tbl_ptr = tbl;
    uint16_t transfer_sz = 0;
    uint32_t remaining_sz = nblocks*512;
    uint32_t flags = 0;
    uint32_t cmd = 0;
    uint8_t *buf_ptr = bfr;

    do {
        transfer_sz = remaining_sz > 0xFE00?0xFE00:remaining_sz;
        remaining_sz -= transfer_sz;

        tbl_ptr->len = transfer_sz;
        tbl_ptr->cmd = ADMA2_TRAN_VALID;
        tbl_ptr->addr = (uint32_t) buf_ptr;

        if (!remaining_sz) 
            tbl_ptr->cmd |= ADMA2_END;

        buf_ptr += transfer_sz;
        tbl_ptr++;

    } while (remaining_sz);


    pb_writel((uint32_t) tbl, _iobase+ USDHC_ADMA_SYS_ADDR);
    
	/* Set ADMA 2 transfer */
	pb_writel(0x08800224, _iobase+ USDHC_PROT_CTRL);

	pb_writel(0x00000200 | (nblocks << 16), _iobase+ USDHC_BLK_ATT);
	
    flags = (1<<31)| (1<<3) | 1;

    cmd = MMC_CMD_WRITE_SINGLE_BLOCK;
    if (!wr) {
        flags |= (1 << 4);
        cmd = MMC_CMD_READ_SINGLE_BLOCK;
    }

    if (nblocks > 1) {
        flags |= (1<<5) | (1<<2) | (1<<1);
        cmd = MMC_CMD_WRITE_MULTIPLE_BLOCK;
        if (!wr)
            cmd = MMC_CMD_READ_MULTIPLE_BLOCK;
    }
    pb_writel(flags, _iobase + USDHC_MIX_CTRL);
        
    usdhc_emmc_send_cmd(cmd, start_lba, MMC_RSP_R1 | 0x20);
 
    if (!async)
        usdhc_emmc_wait_for_de();

}

void usdhc_emmc_init(void) {

    LOG_INFO ("Init...");

    pb_writel(1, 0x0219002c);
    /* Reset usdhc controller */
    pb_writel((1<<24), _iobase+ 0x2c);

    while (pb_readl(_iobase+0x2c) & (1<<24))
        asm("nop");
    pb_writel(0x10801080, _iobase+USDHC_WTMK_LVL);


    /* Set clock to 400 kHz */
    /* MMC Clock = base clock (196MHz) / (prescaler * divisor )*/
    pb_writel(0x10E1, _iobase+USDHC_SYS_CTRL);

    
    while (1) {
        uint32_t pres_state = pb_readl(_iobase+ USDHC_PRES_STATE);
        if (pres_state & (1 << 3))
            break;
    }

    //tfp_printf ("EMMC: Clocks started\n\r");

    /* Configure IRQ's */
    pb_writel(0xFFFFFFFF, _iobase+USDHC_INT_STATUS_EN);
    usdhc_emmc_send_cmd(MMC_CMD_GO_IDLE_STATE, 0,0);

    uint32_t r3;
    while (1) {
        usdhc_emmc_send_cmd(MMC_CMD_SEND_OP_COND, 0x00ff8080, 2);

        r3 = pb_readl(_iobase+USDHC_CMD_RSP0);

        if (r3 & 0x80000000) // Wait for eMMC to power up 
            break;
    }
 
    //tfp_printf ("EMMC: Card reset complete\n\r");

    usdhc_emmc_send_cmd(MMC_CMD_ALL_SEND_CID, 0, 0x09);
    
    _raw_cid[0] = pb_readl(_iobase+ USDHC_CMD_RSP0);
    _raw_cid[1] = pb_readl(_iobase+ USDHC_CMD_RSP1);
    _raw_cid[2] = pb_readl(_iobase+ USDHC_CMD_RSP2);
    _raw_cid[3] = pb_readl(_iobase+ USDHC_CMD_RSP3);

    usdhc_emmc_send_cmd(MMC_CMD_SET_RELATIVE_ADDR, 10<<16, 0x1A);  // R6
    
    usdhc_emmc_send_cmd(MMC_CMD_SEND_CSD, 10<<16, 0x09); // R2

    _raw_csd[3] = pb_readl(_iobase+ USDHC_CMD_RSP0);
    _raw_csd[2] = pb_readl(_iobase+ USDHC_CMD_RSP1);
    _raw_csd[1] = pb_readl(_iobase+ USDHC_CMD_RSP2);
    _raw_csd[0] = pb_readl(_iobase+ USDHC_CMD_RSP3);


    /* Select Card */
    usdhc_emmc_send_cmd(MMC_CMD_SELECT_CARD, 10<<16, 0x1A);
    usdhc_emmc_check_status();
    /* Switch to HS timing */
    usdhc_emmc_send_cmd(MMC_CMD_SWITCH, 
                (MMC_SWITCH_MODE_WRITE_BYTE << 24) | 
                (EXT_CSD_HS_TIMING) << 16 |
                (1) << 8,
                0x1B); // R1b
    

    /* Switch to 8-bit bus */
    usdhc_emmc_send_cmd(MMC_CMD_SWITCH, 
                (MMC_SWITCH_MODE_WRITE_BYTE << 24) | 
                (EXT_CSD_BUS_WIDTH) << 16 |
                (EXT_CSD_DDR_BUS_WIDTH_8) << 8,
                0x1B); // R1b



    /* Switch to 52 MHz DDR */
    /* MMC Clock = base clock (196MHz) / (prescaler * divisor )*/
    pb_writel((1<<31)| (1<<3)| (1<<4)|1, _iobase+USDHC_MIX_CTRL);
    pb_writel(0x0101 | (0x0E << 16),_iobase+USDHC_SYS_CTRL);

    while (1) {
        uint32_t pres_state = pb_readl(_iobase+USDHC_PRES_STATE);
        if (pres_state & (1 << 3))
            break;
    }

	//tfp_printf("EMMC: Configured USDHC for new timing\n\r");
    usdhc_emmc_read_extcsd();

	_sectors =
			_raw_extcsd[EXT_CSD_SEC_CNT + 0] << 0 |
			_raw_extcsd[EXT_CSD_SEC_CNT + 1] << 8 |
			_raw_extcsd[EXT_CSD_SEC_CNT + 2] << 16 |
			_raw_extcsd[EXT_CSD_SEC_CNT + 3] << 24;

    LOG_INFO ("%lu sectors, %lu bytes",_sectors,_sectors*512);
    LOG_INFO ("Partconfig: %2.2X", _raw_extcsd[EXT_CSD_PART_CONFIG]);
}

/* Platform interface */

/* TODO: Fix return values*/
uint32_t plat_emmc_write_block(uint32_t lba_offset, uint8_t *bfr, uint32_t no_of_blocks) {
    usdhc_emmc_xfer_blocks(lba_offset, bfr, no_of_blocks, 1, 0);
    return PB_OK;
}

uint32_t plat_emmc_read_block(uint32_t lba_offset, uint8_t *bfr, uint32_t no_of_blocks) {
    usdhc_emmc_xfer_blocks(lba_offset, bfr, no_of_blocks, 0, 0);
    return PB_OK;
}

uint32_t plat_emmc_switch_part(uint8_t part_no) {
    usdhc_emmc_switch_part(part_no);

    return PB_OK;
}

uint64_t plat_emmc_get_lastlba(void) {
    return _sectors;  
}
