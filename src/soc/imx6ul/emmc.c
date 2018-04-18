
#include <types.h>
#include <emmc.h>
#include <io.h>
#include <regs.h>
#include <tinyprintf.h>

static u32 _iobase = 0x02190000;
static u32 _raw_cid[4];
static u32 _raw_csd[4];
static u8  _raw_extcsd[512];


static void usdhc_emmc_wait_for_cc(void)
{
    volatile u32 irq_status;
    /* Clear all pending interrupts */

    while (1) {
        irq_status = pb_readl(REG(_iobase, USDHC_INT_STATUS));

        if (irq_status & USDHC_INT_RESPONSE)
            break;

    }

    pb_writel(USDHC_INT_RESPONSE, REG(_iobase, USDHC_INT_STATUS));
}

static void usdhc_emmc_wait_for_de(void)
{
    volatile u32 irq_status;
    /* Clear all pending interrupts */

    while (1) {
        irq_status = pb_readl(REG(_iobase, USDHC_INT_STATUS));

        if (irq_status & USDHC_INT_DATA_END)
            break;
    }

    pb_writel(USDHC_INT_DATA_END, REG(_iobase, USDHC_INT_STATUS));
}

static void usdhc_emmc_send_cmd(u8 cmd, u32 arg, 
                                u8 resp_type) {

    volatile u32 pres_state = 0x00;

    while (1) {
        pres_state = pb_readl(REG(_iobase,USDHC_PRES_STATE));

        if ((pres_state & 0x03) == 0x00) {
            break;
        }
    }
    
    pb_writel(arg, REG(_iobase, USDHC_CMD_ARG));
    pb_writel(USDHC_MAKE_CMD(cmd, resp_type), REG(_iobase,
                                USDHC_CMD_XFR_TYP));

    usdhc_emmc_wait_for_cc();
}


static int usdhc_emmc_check_status(void) {

    usdhc_emmc_send_cmd (MMC_CMD_SEND_STATUS, 10<<16,0x1A);

    u32 result = pb_readl(REG(_iobase,USDHC_CMD_RSP0));

    if ((result & MMC_STATUS_RDY_FOR_DATA) &&
        (result & MMC_STATUS_CURR_STATE) != MMC_STATE_PRG) {
        return 0;
    } else if (result & MMC_STATUS_MASK) {
        tfp_printf("Status Error: 0x%8.8X\n",result);
        return -1;
    }

    return 0;
}


static void usdhc_emmc_read_extcsd(void) {
    struct usdhc_adma2_desc tbl;

    tbl.cmd = ADMA2_TRAN_VALID | ADMA2_END;
    tbl.len = 512;
    tbl.addr = (u32) _raw_extcsd;
 
    pb_writel((u32) &tbl, REG(_iobase, USDHC_ADMA_SYS_ADDR));

	/* Set ADMA 2 transfer */
	pb_writel(0x08800224, REG(_iobase, USDHC_PROT_CTRL));

	pb_writel(0x00010200, REG(_iobase, USDHC_BLK_ATT));
	usdhc_emmc_send_cmd(MMC_CMD_SEND_EXT_CSD, 0, 0x3A);
	
    usdhc_emmc_wait_for_de();
}


void usdhc_emmc_switch_part(u8 part_no) {

    u8 part_config = _raw_extcsd[EXT_CSD_PART_CONFIG];

    u8 value = part_config & ~0x07;
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

   tfp_printf ("Switched to part: %i (%2.2X)\n\r", part_no, value);
}


void usdhc_emmc_xfer_blocks(u32 start_lba, u8 *bfr, u32 nblocks, u8 wr) {
    struct usdhc_adma2_desc tbl[256];
    struct usdhc_adma2_desc *tbl_ptr = tbl;
    u16 transfer_sz = 0;
    u32 remaining_sz = nblocks*512;
    u32 flags = 0;
    u32 cmd = 0;
    u8 *buf_ptr = bfr;

    do {
        transfer_sz = remaining_sz > 0xFE00?0xFE00:remaining_sz;
        remaining_sz -= transfer_sz;

        tbl_ptr->len = transfer_sz;
        tbl_ptr->cmd = ADMA2_TRAN_VALID;
        tbl_ptr->addr = (u32) buf_ptr;

        if (!remaining_sz) 
            tbl_ptr->cmd |= ADMA2_END;

        buf_ptr += transfer_sz;
        tbl_ptr++;

    } while (remaining_sz);


    pb_writel((u32) tbl, REG(_iobase, USDHC_ADMA_SYS_ADDR));
    
	/* Set ADMA 2 transfer */
	pb_writel(0x08800224, REG(_iobase, USDHC_PROT_CTRL));

	pb_writel(0x00000200 | (nblocks << 16), REG(_iobase, USDHC_BLK_ATT));
	
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
    pb_writel(flags, REG(_iobase, USDHC_MIX_CTRL));
        
    usdhc_emmc_send_cmd(cmd, start_lba, MMC_RSP_R1 | 0x20);
 
    usdhc_emmc_wait_for_de();

}

void soc_emmc_init(void) {

    tfp_printf ("EMMC: Init...\n\r");   


    pb_writel(1, REG(0x0219002c,0));
    /* Reset usdhc controller */
    pb_writel((1<<24), REG(_iobase, 0x2c));

    while (pb_readl(REG(_iobase, 0x2c)) & (1<<24))
        asm("nop");
    pb_writel(0x10401040, REG(_iobase, USDHC_WTMK_LVL));


    /* Set clock to 400 kHz */
    /* MMC Clock = base clock (196MHz) / (prescaler * divisor )*/
    pb_writel(0x10E1, REG(_iobase,  USDHC_SYS_CTRL));

    
    while (1) {
        u32 pres_state = pb_readl(REG(_iobase,  USDHC_PRES_STATE));
        if (pres_state & (1 << 3))
            break;
    }

    tfp_printf ("EMMC: Clocks started\n\r");

    /* Configure IRQ's */
    pb_writel(0xFFFFFFFF, REG(_iobase, USDHC_INT_STATUS_EN));
    usdhc_emmc_send_cmd(MMC_CMD_GO_IDLE_STATE, 0,0);

    u32 r3;
    while (1) {
        usdhc_emmc_send_cmd(MMC_CMD_SEND_OP_COND, 0x00ff8080, 2);

        r3 = pb_readl(REG(_iobase, USDHC_CMD_RSP0));

        if (r3 & 0x80000000) // Wait for eMMC to power up 
            break;
    }
 
    tfp_printf ("EMMC: Card reset complete\n\r");

    usdhc_emmc_send_cmd(MMC_CMD_ALL_SEND_CID, 0, 0x09);
    
    _raw_cid[0] = pb_readl(REG(_iobase, USDHC_CMD_RSP0));
    _raw_cid[1] = pb_readl(REG(_iobase, USDHC_CMD_RSP1));
    _raw_cid[2] = pb_readl(REG(_iobase, USDHC_CMD_RSP2));
    _raw_cid[3] = pb_readl(REG(_iobase, USDHC_CMD_RSP3));

    usdhc_emmc_send_cmd(MMC_CMD_SET_RELATIVE_ADDR, 10<<16, 0x1A);  // R6
    
    usdhc_emmc_send_cmd(MMC_CMD_SEND_CSD, 10<<16, 0x09); // R2

    _raw_csd[3] = pb_readl(REG(_iobase, USDHC_CMD_RSP0));
    _raw_csd[2] = pb_readl(REG(_iobase, USDHC_CMD_RSP1));
    _raw_csd[1] = pb_readl(REG(_iobase, USDHC_CMD_RSP2));
    _raw_csd[0] = pb_readl(REG(_iobase, USDHC_CMD_RSP3));


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
    pb_writel((1<<31)| (1<<3)| (1<<4)|1, REG(_iobase, USDHC_MIX_CTRL));
    pb_writel(0x0101 | (0x0E << 16), REG(_iobase, USDHC_SYS_CTRL));

    while (1) {
        u32 pres_state = pb_readl(REG(_iobase, USDHC_PRES_STATE));
        if (pres_state & (1 << 3))
            break;
    }

	tfp_printf("EMMC: Configured USDHC for new timing\n\r");
    usdhc_emmc_read_extcsd();

	u32 sectors =
			_raw_extcsd[EXT_CSD_SEC_CNT + 0] << 0 |
			_raw_extcsd[EXT_CSD_SEC_CNT + 1] << 8 |
			_raw_extcsd[EXT_CSD_SEC_CNT + 2] << 16 |
			_raw_extcsd[EXT_CSD_SEC_CNT + 3] << 24;

    tfp_printf ("EMMC: %u sectors, %u bytes\n\r",sectors,sectors*512);

    tfp_printf ("EMMC: Partconfig: %2.2X\n\r", _raw_extcsd[EXT_CSD_PART_CONFIG]);

}
