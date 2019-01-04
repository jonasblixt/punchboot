#include <board.h>
#include <plat.h>
#include <io.h>
#include <tinyprintf.h>
#include <plat/imx6ul/gpt.h>
#include <plat/imx6ul/caam.h>
#include <plat/imx6ul/ocotp.h>
#include <plat/imx6ul/imx_uart.h>
#include <plat/imx6ul/usdhc.h>
#include <plat/imx6ul/hab.h>
#include <board_config.h>

static struct ocotp_dev ocotp;
static struct gp_timer platform_timer;
static struct fsl_caam caam;

uint32_t plat_get_us_tick(void) 
{
    return gp_timer_get_tick(&platform_timer);
}


uint32_t plat_get_uuid(uint8_t *uuid) 
{
    uint32_t *uuid_ptr = (uint32_t *) uuid;

    ocotp_read(BOARD_UUID_FUSE0, &uuid_ptr[0]);
    ocotp_read(BOARD_UUID_FUSE1, &uuid_ptr[1]);
    ocotp_read(BOARD_UUID_FUSE2, &uuid_ptr[2]);
    ocotp_read(BOARD_UUID_FUSE3, &uuid_ptr[3]);

    return PB_OK;
}

uint32_t plat_write_uuid(uint8_t *uuid, uint32_t key) 
{
    uint32_t *uuid_ptr = (uint32_t *) uuid;
    uint8_t tmp_uuid[16];

    if (key != BOARD_OTP_WRITE_KEY)
        return PB_ERR;

    plat_get_uuid(tmp_uuid);

    for (int i = 0; i < 16; i++) 
    {
        if (tmp_uuid[i] != 0) 
        {
            LOG_ERR ("Can't write UUID, fuses already programmed");
            return PB_ERR;
        }
    }
    ocotp_write(BOARD_UUID_FUSE0, uuid_ptr[0]);
    ocotp_write(BOARD_UUID_FUSE1, uuid_ptr[1]);
    ocotp_write(BOARD_UUID_FUSE2, uuid_ptr[2]);
    ocotp_write(BOARD_UUID_FUSE3, uuid_ptr[3]);

    return PB_OK;
}


uint32_t plat_early_init(void)
{
    uint32_t reg;

    platform_timer.base = GP_TIMER1_BASE;
    gp_timer_init(&platform_timer);


    /**
     * TODO: Some imx6ul can run at 696 MHz and some others at 528 MHz
     *   implement handeling of that.
     *
     */

    /*** Configure ARM Clock ***/
    reg = pb_read32(0x020C400C);
    /* Select step clock, so we can change arm PLL */
    pb_write32(reg | (1<<2), 0x020C400C);


    /* Power down */
    pb_write32((1<<12) , 0x020C8000);

    /* Configure divider and enable */
    /* f_CPU = 24 MHz * 88 / 4 = 528 MHz */
    pb_write32((1<<13) | 88, 0x020C8000);


    /* Wait for PLL to lock */
    while (!(pb_read32(0x020C8000) & (1<<31)))
        asm("nop");

    /* Select re-connect ARM PLL */
    pb_write32(reg & ~(1<<2), 0x020C400C);
    
    /*** End of ARM Clock config ***/



    /* Ungate all clocks */
    pb_write32(0xFFFFFFFF, 0x020C4000+0x68); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x6C); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x70); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x74); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x78); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x7C); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x80); /* Ungate usdhc clk*/


    uint32_t csu = 0x21c0000;
    /* Allow everything */
    for (int i = 0; i < 40; i ++) {
		*((uint32_t *)csu + i) = 0xffffffff;
	}
    
    board_init();

    if (board_get_debug_uart() != 0)
    {
        imx_uart_init(board_get_debug_uart());
        init_printf(NULL, &plat_uart_putc);
    }

    usdhc_emmc_init();

    ocotp.base = 0x021BC000;
    ocotp_init(&ocotp);

    /* Configure CAAM */
    caam.base = 0x02140000;

    if (caam_init(&caam) != PB_OK)
        return PB_ERR;

    if (hab_secureboot_active())
    {
        LOG_INFO("Secure boot active");
    } else {
        LOG_INFO("Secure boot disabled");
    }

    if (hab_has_no_errors() == PB_OK)
    {
        LOG_INFO("No HAB errors found");
    } else {
        LOG_ERR("HAB is reporting errors");
    }

    reg = pb_read32(0x020CC000 + 0x14);
    LOG_INFO("SVNS HPSR: %8.8lX", reg);

    for (uint32_t n = 0; n < 8; n++)
    {
        reg = pb_read32(0x021BC580 + n*0x10);
        LOG_INFO("SRK%lu = 0x%8.8lX", n, reg);
    }

    return PB_OK;
}

