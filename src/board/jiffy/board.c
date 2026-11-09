/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <boot/ab_state.h>
#include <boot/boot.h>
#include <boot/linux.h>
#include <drivers/crypto/imx_caam.h>
#include <drivers/fuse/imx_hab_fuses.h>
#include <drivers/fuse/imx_ocotp.h>
#include <drivers/mmc/imx_usdhc.h>
#include <drivers/mmc/mmc_core.h>
#include <drivers/pinctrl/imx_iomuxc.h>
#include <drivers/rom_log/imx_rom_log.h>
#include <drivers/uart/imx_uart.h>
#include <drivers/usb/imx_ci_udc.h>
#include <drivers/usb/imx_usb2_phy.h>
#include <drivers/usb/pb_dev_cls.h>
#include <pb/bio.h>
#include <pb/board.h>
#include <pb/board_layout.h>
#include <pb/cm.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <plat/imx6ul/imx6ul.h>

static int usdhc_emmc_setup(void)
{
    /* Ungate USDHC1 clock */
    mmio_clrsetbits_32(IMX6UL_CCM_CCGR6, 0, CCM_CCGR6_USDHC1);

    /* uSDHC1, the pads as the ROM left them */
    static const struct imx_pin pins[] = {
        IMX6UL_PIN(SD1_CLK, MUX_ALT(0), IMX_PAD_KEEP),
        IMX6UL_PIN(SD1_CMD, MUX_ALT(0), IMX_PAD_KEEP),
        IMX6UL_PIN(SD1_DATA0, MUX_ALT(0), IMX_PAD_KEEP),
        IMX6UL_PIN(SD1_DATA1, MUX_ALT(0), IMX_PAD_KEEP),
        IMX6UL_PIN(SD1_DATA2, MUX_ALT(0), IMX_PAD_KEEP),
        IMX6UL_PIN(SD1_DATA3, MUX_ALT(0), IMX_PAD_KEEP),
        IMX6UL_PIN(NAND_READY_B, MUX_ALT(1), IMX_PAD_KEEP), /* DATA4 */
        IMX6UL_PIN(NAND_CE0_B, MUX_ALT(1), IMX_PAD_KEEP), /* DATA5 */
        IMX6UL_PIN(NAND_CE1_B, MUX_ALT(1), IMX_PAD_KEEP), /* DATA6 */
        IMX6UL_PIN(NAND_CLE, MUX_ALT(1), IMX_PAD_KEEP), /* DATA7 */
        IMX6UL_PIN(NAND_DQS, MUX_ALT(1), IMX_PAD_KEEP), /* RESET */
    };

    imx_iomuxc_setup(pins, ARRAY_SIZE(pins));

    static const struct imx_usdhc_config cfg = {
        .base = IMX6UL_USDHC1_BASE,
        .delay_tap = 0,
        .mmc_config = {
            .mode = MMC_BUS_MODE_DDR52,
            .width = MMC_BUS_WIDTH_8BIT_DDR,
            .boot_mode = EXT_CSD_BOOT_DDR |
                      EXT_CSD_BOOT_BUS_WIDTH_8,
            .boot0_uu = PART_boot0,
            .boot1_uu = PART_boot1,
            .user_uu = PART_user,
            .rpmb_uu = PART_rpmb,
            .flags = 0,
        },
    };

    return imx_usdhc_init(&cfg, MHz(imx6ul_plat()->usdhc1_clk_MHz));
}

static int board_command(uint32_t command,
                         uint8_t *bfr,
                         size_t size,
                         uint8_t *response_bfr,
                         size_t *response_size)
{
    LOG_DBG("%x, %p, %zu", command, bfr, size);

    if (command == 0xd3399b11) /* dump-rom-log */
        return imx_rom_log_dump((char *)response_bfr, response_size);

    LOG_ERR("Unknown command %x", command);
    *response_size = 0;
    return -PB_ERR_NOT_SUPPORTED;
}

static void boot_source_str(char *out, size_t size)
{
    struct pb_boot_source src;
    char uu_str[37];

    if (plat_get_boot_source(&src) != PB_OK) {
        snprintf(out, size, "Not supported");
        return;
    }

    switch (src.kind) {
    case PB_BOOT_SOURCE_STORAGE: {
        bio_dev_t dev = bio_get_part_by_uu(src.part_uu);

        uuid_unparse(src.part_uu, uu_str);
        snprintf(out, size, "%s (%s)", (dev >= 0) ? bio_get_description(dev) : "?", uu_str);
    } break;
    case PB_BOOT_SOURCE_USB:
        snprintf(out, size, "USB");
        break;
    case PB_BOOT_SOURCE_UART:
        snprintf(out, size, "UART");
        break;
    case PB_BOOT_SOURCE_DEBUGGER:
        snprintf(out, size, "Debugger");
        break;
    default:
        snprintf(out, size, "Unknown");
        break;
    }
}

/*
 * The boot configuration the ROM used (RM 8.2, Table 8-15 for uSDHC): SRC
 * SBMR1 is BOOT_CFG1..4 from the fuses or the boot pins, the BOOT_CFG fuse
 * word is the shadow at 0x450. Only read.
 */
#define SRC_SBMR1       (IMX6UL_SRC_BASE + 0x04)
#define SRC_SBMR2       (IMX6UL_SRC_BASE + 0x1c)
#define OCOTP_BOOT_CFG0 (IMX6UL_OCOTP_CTRL_BASE + 0x450)

static const char *boot_bus_width_str(uint32_t cfg)
{
    switch (IMX6UL_BOOT_CFG_BUS_WIDTH_OF(cfg)) {
    case IMX6UL_BOOT_BUS_1BIT:
        return "1-bit";
    case IMX6UL_BOOT_BUS_4BIT:
        return "4-bit";
    case IMX6UL_BOOT_BUS_8BIT:
        return "8-bit";
    case IMX6UL_BOOT_BUS_4BIT_DDR:
        return "4-bit DDR";
    case IMX6UL_BOOT_BUS_8BIT_DDR:
        return "8-bit DDR";
    default:
        return "reserved";
    }
}

static int board_status(uint8_t *response_bfr, size_t *response_size)
{
    char *response = (char *)response_bfr;
    size_t resp_buf_size = *response_size;
    char boot_source[128];
    uint32_t sbmr1 = mmio_read_32(SRC_SBMR1);

    boot_source_str(boot_source, sizeof(boot_source));

    (*response_size) = snprintf(response,
                                resp_buf_size,
                                "Board status OK!\n"
                                "Boot source: %s\n"
                                "Boot config: SBMR1 0x%08x, SBMR2 0x%08x, fuses 0x450 0x%08x\n"
                                "ROM eMMC boot: %s, %s, %s speed, %s boot\n",
                                boot_source,
                                (unsigned int)sbmr1,
                                (unsigned int)mmio_read_32(SRC_SBMR2),
                                (unsigned int)mmio_read_32(OCOTP_BOOT_CFG0),
                                (sbmr1 & IMX6UL_BOOT_CFG_MMC) ? "MMC" : "SD",
                                boot_bus_width_str(sbmr1),
                                (sbmr1 & IMX6UL_BOOT_CFG_MMC_HIGH_SPEED) ? "high" : "normal",
                                (sbmr1 & IMX6UL_BOOT_CFG_FAST_BOOT) ? "fast" : "normal");
    response[(*response_size)++] = 0;

    return PB_OK;
}

/*
 * Warning: an example configuration. Be careful with the slc commands.
 * slc_set_configuration() fuses the SRK hash of CONFIG_SRK_FUSE, the keys pb
 * is signed with, then these: boot from the eMMC on uSDHC1, 8-bit DDR, and
 * take the boot configuration from the fuses, not the boot pins.
 */
static const struct slc_fuse config_fuses[] = {
    { IMX6UL_FUSE(IMX6UL_FUSE_BOOT_CFG_BANK, IMX6UL_FUSE_BOOT_CFG_WORD),
      IMX6UL_BOOT_CFG_USDHC | IMX6UL_BOOT_CFG_MMC | IMX6UL_BOOT_CFG_USDHC_PORT(0) |
          IMX6UL_BOOT_CFG_BUS_WIDTH(IMX6UL_BOOT_BUS_8BIT_DDR) },
    { IMX6UL_FUSE(IMX6UL_FUSE_SEC_CONFIG_BANK, IMX6UL_FUSE_SEC_CONFIG_WORD),
      IMX6UL_FUSE_BT_FUSE_SEL },
};

static void board_console_init(void)
{
#define UART_PAD_CTRL                                                                          \
    (PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm | \
     PAD_CTL_SRE_FAST | PAD_CTL_HYS)

    /* Ungate uart clock */
    mmio_clrsetbits_32(IMX6UL_CCM_CCGR0, 0, CCM_CCGR0_UART2);

    static const struct imx_pin pins[] = {
        IMX6UL_PIN(UART2_TX_DATA, MUX_ALT(0), UART_PAD_CTRL),
        IMX6UL_PIN(UART2_RX_DATA, MUX_ALT(0), UART_PAD_CTRL),
    };

    imx_iomuxc_setup(pins, ARRAY_SIZE(pins));

    imx_uart_init(IMX6UL_UART2_BASE, MHz(80), 115200);

    static const struct console_ops ops = {
        .putc = imx_uart_putc,
    };

    console_init(IMX6UL_UART2_BASE, &ops);
}

static int board_hw_init(void)
{
    int rc;

    rc = imx_caam_init(IMX6UL_CAAM_JR0_BASE);

    if (rc != PB_OK) {
        LOG_ERR("CAAM init failed (%i)", rc);
        return rc;
    }

    /* Linux can't instantiate the RNG when it runs in the normal world. Without
     * it Linux has no hardware RNG, which is no reason not to boot. */
    rc = imx_caam_rng_init(IMX6UL_CAAM_BASE);

    if (rc != PB_OK)
        LOG_ERR("CAAM RNG init failed (%i)", rc);

    rc = usdhc_emmc_setup();

    if (rc != PB_OK)
        LOG_ERR("usdhc init failed (%i)", rc);

    return rc;
}

static int board_cm_init(void)
{
    /* Enable USB PLL */
    mmio_clrsetbits_32(IMX6UL_CCM_ANALOG_PLL_USB1, 0, CCM_ANALOG_PLL_USB1_EN_USB_CLKS);

    imx_usb2_phy_init(IMX6UL_USBPHY1_BASE);
    return imx_ci_udc_init(IMX6UL_USB1_BASE);
}

static const struct rot_config rot = {
    .revoke_key = imx_hab_revoke_key,
    .read_key_status = imx_hab_read_key_status,
};

static const struct slc_config slc = {
    .read_status = imx_hab_slc_read_status,
    .set_configuration = imx_hab_slc_set_configuration,
    .set_configuration_locked = imx_hab_slc_set_configuration_locked,
    .fuses = config_fuses,
    .fuses_length = ARRAY_SIZE(config_fuses),
    .fuse_read = imx_ocotp_fuse_read,
    .fuse_write = imx_ocotp_fuse_write,
};

static const struct boot_driver_linux_config boot_linux = {
    .image_bpak_id = 0xec103b08, /* bpak_id("kernel") */
    .dtb_bpak_id = 0x56f91b86, /* bpak_id("dt")  */
    .ramdisk_bpak_id = 0xf4cdac1f, /* bpak_id("ramdisk") */
    .dtb_patch_cb = board_patch_bootargs,
    .resolve_part_name = boot_ab_part_uu_to_name,
    .set_dtb_boot_arg = true,
};

const struct board board = {
    .name = "jiffy",
    .console_init = board_console_init,
    .rot = &rot,
    .slc = &slc,
    .init = board_hw_init,
    .gpt_dev_uu = PART_user,
    .gpt_tables = board_layout_gpt,
    .gpt_tables_length = ARRAY_SIZE(board_layout_gpt),
    .ab = &board_layout_ab,
    .boot_linux = &boot_linux,
    .bootargs = "console=ttymxc1,115200 quiet ",
    .bootargs_verbose = "console=ttymxc1,115200 earlyprintk ",
    .cm_init = board_cm_init,
    .cm_transport = &pb_dev_cls_transport,
    .status = board_status,
    .command = board_command,
};
