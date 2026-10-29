/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * NXP i.MX8M Plus EVK: 8MPLUS-BB base board with the 8MPLUSLPD4-CPU module
 * (6 GiB LPDDR4). The console is UART2, the third port of the FT4232H on
 * the debug USB. The eMMC is on uSDHC3, on the NAND pads. Command mode
 * runs on USB1, the USB-C port the ROM's serial download uses.
 *
 * This board never writes fuses: the SLC callbacks are NULL, so the SLC
 * commands are not supported, and no key is ever revoked.
 *
 * Booting Linux doesn't work yet: there is no TF-A handover. The boot driver
 * loads and verifies images, e.g. for 'punchboot boot partition'.
 */

#include <boot/ab_state.h>
#include <boot/boot.h>
#include <boot/linux.h>
#include <drivers/crypto/imx_caam.h>
#include <drivers/mmc/imx_usdhc.h>
#include <drivers/partition/gpt.h>
#include <drivers/uart/imx_uart.h>
#include <drivers/usb/dwc3_udc.h>
#include <drivers/usb/imx8m_phy.h>
#include <drivers/usb/pb_dev_cls.h>
#include <drivers/usb/usbd.h>
#include <libfdt.h>
#include <pb/cm.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <plat/imx8m/clock_imx8mp.h>
#include <plat/imx8m/dram.h>
#include <plat/imx8m/imx8m.h>

#include "partitions.h"

/*
 * DRAM: only the 4000 MT/s set point is trained. The timing also has 400
 * and 100 MT/s for DDR frequency scaling in Linux, which this board doesn't
 * use, and their training takes 215 ms of the boot.
 */
static const unsigned int imx8mpevk_dram_mts[] = { 4000 };

struct dram_rates dram_rates = {
    .mts = imx8mpevk_dram_mts,
    .num = ARRAY_SIZE(imx8mpevk_dram_mts),
};

static const struct gpt_part_table gpt_tbl_default[] = {
    {
        .uu = UUID_2af755d8_8de5_45d5_a862_014cfa735ce0,
        .description = "System A",
        .size = SZ_MiB(30),
    },
    {
        .uu = UUID_c046ccd8_0f2e_4036_984d_76c14dc73992,
        .description = "System B",
        .size = SZ_MiB(30),
    },
    {
        .uu = UUID_c284387a_3377_4c0f_b5db_1bcbcff1ba1a,
        .description = "Root A",
        .size = SZ_MiB(128),
    },
    {
        .uu = UUID_ac6a1b62_7bd0_460b_9e6a_9a7831ccbfbb,
        .description = "Root B",
        .size = SZ_MiB(128),
    },
    {
        .uu = UUID_f5f8c9ae_efb5_4071_9ba9_d313b082281e,
        .description = "PB State Primary",
        .size = 512,
    },
    {
        .uu = UUID_656ab3fc_5856_4a5e_a2ae_5a018313b3ee,
        .description = "PB State Backup",
        .size = 512,
    },
    {
        .uu = UUID_4581af22_99e6_4a94_b821_b60c42d74758,
        .description = "Root overlay A",
        .size = SZ_MiB(30),
    },
    {
        .uu = UUID_da2ca04f_a693_4284_b897_3906cfa1eb13,
        .description = "Root overlay B",
        .size = SZ_MiB(30),
    },
    {
        .uu = UUID_23477731_7e33_403b_b836_899a0b1d55db,
        .description = "RoT extension A",
        .size = SZ_KiB(128),
    },
    {
        .uu = UUID_6ffd077c_32df_49e7_b11e_845449bd8edd,
        .description = "RoT extension B",
        .size = SZ_KiB(128),
    },
    {
        .uu = UUID_9697399d_e2da_47d9_8eb5_88daea46da1b,
        .description = "System storage A",
        .size = SZ_MiB(128),
    },
    {
        .uu = UUID_c5b8b41c_0fb5_494d_8b0e_eba400e075fa,
        .description = "System storage B",
        .size = SZ_MiB(128),
    },
    {
        .uu = UUID_39792364_d3e3_4013_ac51_caaea65e4334,
        .description = "Mass storage",
        .size = SZ_GiB(1),
    },
};

static const struct gpt_table_list gpt_tables[] = {
    {
        .name = "Default",
        .variant = 0,
        .table = gpt_tbl_default,
        .table_length = ARRAY_SIZE(gpt_tbl_default),
    },
};

void board_console_init(struct imx8m_platform *plat_)
{
    (void)plat_;

    /* UART2_CLK_ROOT runs from the 24 MHz reference after reset */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(UART2_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_UART2), CCGR_CLK_ON_MASK);

    mmio_write_32(IOMUXC_MUX_UART2_RXD, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_UART2_TXD, MUX_ALT(0));

    imx_uart_init(IMX8M_UART2_BASE, MHz(24), 115200);

    static const struct console_ops ops = {
        .putc = imx_uart_putc,
    };

    console_init(IMX8M_UART2_BASE, &ops);
}

static int usdhc_emmc_setup(void)
{
    /* USDHC3_CLK_ROOT: SYSTEM_PLL1_DIV2 (400 MHz) / 2 = 200 MHz */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_CLR(IMX8MP_USDHC3_CLK_ROOT),
                  CLK_ROOT_SRC_MUX_MASK | CLK_ROOT_PRE_DIV_MASK);
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(IMX8MP_USDHC3_CLK_ROOT),
                  CLK_ROOT_ON | CLK_ROOT_SOURCE_SEL(1) | CLK_ROOT_PRE_DIV(CLK_ROOT_PRE_DIV2));
    mmio_write_32(CCM_CCGR_SET(IMX8MP_CCGR_USDHC3), CCGR_CLK_ON_MASK);

    static const uintptr_t mux[] = {
        IOMUXC_MUX_NAND_WE_B,   IOMUXC_MUX_NAND_WP_B,   IOMUXC_MUX_NAND_DATA04,
        IOMUXC_MUX_NAND_DATA05, IOMUXC_MUX_NAND_DATA06, IOMUXC_MUX_NAND_DATA07,
        IOMUXC_MUX_NAND_RE_B,   IOMUXC_MUX_NAND_CE2_B,  IOMUXC_MUX_NAND_CE3_B,
        IOMUXC_MUX_NAND_CLE,    IOMUXC_MUX_NAND_CE1_B,  IOMUXC_MUX_NAND_READY_B,
    };

    for (unsigned int i = 0; i < ARRAY_SIZE(mux); i++)
        mmio_write_32(mux[i], MUX_ALT(2));

    /* The inputs default to the ENET pads */
    mmio_write_32(IOMUXC_USDHC3_CARD_CLK_IN_SELECT_INPUT, 1);
    mmio_write_32(IOMUXC_USDHC3_CMD_IN_SELECT_INPUT, 1);
    for (unsigned int i = 0; i < 8; i++)
        mmio_write_32(IOMUXC_USDHC3_DAT_IN_SELECT_INPUT(i), 1);
    mmio_write_32(IOMUXC_USDHC3_STROBE_SELECT_INPUT, 1);

#define USDHC3_PAD_CLK    (PAD_CTL_HYS | PAD_CTL_FSEL_FAST | PAD_CTL_DSE_X6)
#define USDHC3_PAD_DATA   (PAD_CTL_PE | PAD_CTL_PUE | USDHC3_PAD_CLK)
#define USDHC3_PAD_STROBE (PAD_CTL_PE | USDHC3_PAD_CLK) /* Pull down */
    static const uintptr_t data_pads[] = {
        IOMUXC_PAD_NAND_WP_B,    IOMUXC_PAD_NAND_DATA04, IOMUXC_PAD_NAND_DATA05,
        IOMUXC_PAD_NAND_DATA06,  IOMUXC_PAD_NAND_DATA07, IOMUXC_PAD_NAND_RE_B,
        IOMUXC_PAD_NAND_CE2_B,   IOMUXC_PAD_NAND_CE3_B,  IOMUXC_PAD_NAND_CLE,
        IOMUXC_PAD_NAND_READY_B,
    };

    mmio_write_32(IOMUXC_PAD_NAND_WE_B, USDHC3_PAD_CLK);
    mmio_write_32(IOMUXC_PAD_NAND_CE1_B, USDHC3_PAD_STROBE);
    for (unsigned int i = 0; i < ARRAY_SIZE(data_pads); i++)
        mmio_write_32(data_pads[i], USDHC3_PAD_DATA);

    static const struct imx_usdhc_config cfg = {
        .base = IMX8M_USDHC3_BASE,
        .delay_tap = 35,
        .mmc_config = {
            .mode = MMC_BUS_MODE_HS200,
            .width = MMC_BUS_WIDTH_8BIT,
            .boot_mode = EXT_CSD_BOOT_DDR | EXT_CSD_BOOT_BUS_WIDTH_8,
            .boot0_uu = PART_boot0,
            .boot1_uu = PART_boot1,
            .user_uu = PART_user,
            .rpmb_uu = PART_rpmb,
            .flags = 0,
        },
    };

    return imx_usdhc_init(&cfg, MHz(200));
}

static int usdhc_sd_setup(void)
{
    /* USDHC2_CLK_ROOT: SYSTEM_PLL1_DIV2 (400 MHz) / 2 = 200 MHz */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_CLR(IMX8MP_USDHC2_CLK_ROOT),
                  CLK_ROOT_SRC_MUX_MASK | CLK_ROOT_PRE_DIV_MASK);
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(IMX8MP_USDHC2_CLK_ROOT),
                  CLK_ROOT_ON | CLK_ROOT_SOURCE_SEL(1) | CLK_ROOT_PRE_DIV(CLK_ROOT_PRE_DIV2));
    mmio_write_32(CCM_CCGR_SET(IMX8MP_CCGR_USDHC2), CCGR_CLK_ON_MASK);

    static const uintptr_t mux[] = {
        IOMUXC_MUX_SD2_CD_B,  IOMUXC_MUX_SD2_CLK,   IOMUXC_MUX_SD2_CMD,   IOMUXC_MUX_SD2_DATA0,
        IOMUXC_MUX_SD2_DATA1, IOMUXC_MUX_SD2_DATA2, IOMUXC_MUX_SD2_DATA3,
    };

    for (unsigned int i = 0; i < ARRAY_SIZE(mux); i++)
        mmio_write_32(mux[i], MUX_ALT(0));

    /*
     * SD2_VSEL selects NVCC_SD2 (the PMIC's LDO5): VEND_SPEC.VSELECT 0,
     * 3.3 V. SD2_RESET_B enables the card supply through a pull-up and is
     * left as it is.
     */
    mmio_write_32(IOMUXC_MUX_GPIO1_IO04, MUX_ALT(1));
    mmio_write_32(IOMUXC_PAD_GPIO1_IO04, PAD_CTL_DSE_X1);

#define USDHC2_PAD_CLK  (PAD_CTL_HYS | PAD_CTL_FSEL_FAST | PAD_CTL_DSE_X4)
#define USDHC2_PAD_DATA (PAD_CTL_PE | PAD_CTL_PUE | USDHC2_PAD_CLK)
    static const uintptr_t data_pads[] = {
        IOMUXC_PAD_SD2_CMD,   IOMUXC_PAD_SD2_DATA0, IOMUXC_PAD_SD2_DATA1,
        IOMUXC_PAD_SD2_DATA2, IOMUXC_PAD_SD2_DATA3,
    };

    mmio_write_32(IOMUXC_PAD_SD2_CLK, USDHC2_PAD_CLK);
    mmio_write_32(IOMUXC_PAD_SD2_CD_B, PAD_CTL_HYS | PAD_CTL_PE | PAD_CTL_PUE);
    for (unsigned int i = 0; i < ARRAY_SIZE(data_pads); i++)
        mmio_write_32(data_pads[i], USDHC2_PAD_DATA);

    static const struct imx_usdhc_config cfg = {
        .base = IMX8M_USDHC2_BASE,
        .card_detect = true,
        .mmc_config = {
            .card = MMC_CARD_TYPE_SD,
            .mode = MMC_BUS_MODE_SD_HS,
            .width = MMC_BUS_WIDTH_4BIT,
            .user_uu = PART_sd,
        },
    };

    return imx_usdhc_init(&cfg, MHz(200));
}

static int board_patch_bootargs(void *fdt, int offset)
{
    const char *bootargs = NULL;

    if (boot_get_flags() & BOOT_FLAG_VERBOSE) {
        bootargs = "console=ttymxc1,115200 earlyprintk ";
    } else {
        bootargs = "console=ttymxc1,115200 quiet ";
    }

    return fdt_setprop_string(fdt, offset, "bootargs", bootargs);
}

/* Revocation is a fuse, which this board never writes */
static int board_read_key_status(const struct rot_key *key)
{
    (void)key;
    return PB_OK;
}

int board_init(struct imx8m_platform *plat_)
{
    int rc;

    (void)plat_;

    /* Hash and signature verification. The ROM leaves the CAAM clock on */
    rc = imx_caam_init(IMX8M_CAAM_JR0_BASE);

    if (rc != PB_OK) {
        LOG_ERR("CAAM init failed (%i)", rc);
        return rc;
    }

    rc = usdhc_emmc_setup();

    if (rc != PB_OK) {
        LOG_ERR("eMMC init failed (%i)", rc);
        return rc;
    }

    /* The microSD slot: no card, or one that fails, doesn't stop the boot */
    rc = usdhc_sd_setup();
    if (rc != PB_OK && rc != -PB_ERR_NOT_FOUND)
        LOG_ERR("SD init failed (%i)", rc);

    bio_dev_t user_part = bio_get_part_by_uu(PART_user);

    if (user_part < 0) {
        LOG_ERR("Can't find partition (%i)", user_part);
        return user_part;
    }

    rc = gpt_ptbl_init(user_part, gpt_tables, ARRAY_SIZE(gpt_tables));
    /* eMMC User partition now only has the visible flag to report capacity */
    (void)bio_set_flags(user_part, BIO_FLAG_VISIBLE);

    if (rc == PB_OK) {
        static const struct boot_ab_state_config boot_state_cfg = {
            .primary_state_part_uu = PART_primary_state,
            .backup_state_part_uu = PART_backup_state,
            .sys_a_uu = PART_sys_a,
            .sys_b_uu = PART_sys_b,
            .rollback_mode = AB_ROLLBACK_MODE_NORMAL,
        };

        rc = boot_ab_state_init(&boot_state_cfg);

        if (rc != PB_OK) {
            LOG_ERR("Boot state init failed (%i)", rc);
            return rc;
        }
    } else {
        LOG_WARN("GPT ptbl init failed (%i)", rc);
    }

    static const struct boot_driver_linux_config linux_boot_cfg = {
        .image_bpak_id = 0xec103b08, /* bpak_id("kernel") */
        .dtb_bpak_id = 0x56f91b86, /* bpak_id("dt")  */
        .ramdisk_bpak_id = 0xf4cdac1f, /* bpak_id("ramdisk") */
        .dtb_patch_cb = board_patch_bootargs,
        .resolve_part_name = boot_ab_part_uu_to_name,
        .set_dtb_boot_arg = true,
    };

    rc = boot_driver_linux_init(&linux_boot_cfg);

    if (rc != PB_OK)
        return rc;

    static const struct boot_driver boot_driver = {
        .default_boot_source = BOOT_SOURCE_BIO,
        .get_boot_bio_device = boot_ab_state_get,
        .set_boot_partition = boot_ab_state_set_boot_partition,
        .get_boot_partition = boot_ab_state_get_boot_partition,
        .prepare = boot_driver_linux_prepare,
        .jump = boot_driver_linux_jump,
    };

    rc = boot_init(&boot_driver);

    if (rc != PB_OK)
        return rc;

    static const struct rot_config rot_config = {
        .revoke_key = NULL,
        .read_key_status = board_read_key_status,
        .key_map_length = 3,
        .key_map = {
            {
                .name = "pb-development",
                .id = 0xa90f9680,
                .param1 = 0,
            },
            {
                .name = "pb-development2",
                .id = 0x25c6dd36,
                .param1 = 1,
            },
            {
                .name = "pb-development3",
                .id = 0x52c1eda0,
                .param1 = 2,
            },
        },
    };

    rc = rot_init(&rot_config);

    if (rc != PB_OK) {
        LOG_ERR("RoT init failed (%i)", rc);
        return rc;
    }

    /* No fuse access, see the top of this file */
    static const struct slc_config slc_config = {
        .read_status = NULL,
        .set_configuration = NULL,
        .set_configuration_locked = NULL,
        .set_eol = NULL,
    };

    rc = slc_init(&slc_config);

    if (rc != PB_OK) {
        LOG_ERR("SLC init failed (%i)", rc);
        return rc;
    }

    return PB_OK;
}

static int board_command(uint32_t command,
                         uint8_t *bfr,
                         size_t size,
                         uint8_t *response_bfr,
                         size_t *response_size)
{
    (void)bfr;
    LOG_DBG("%x, %zu", command, size);
    *response_size = 0;
    return -PB_ERR_NOT_SUPPORTED;
}

static int board_status(uint8_t *response_bfr, size_t *response_size)
{
    char *response = (char *)response_bfr;
    size_t resp_buf_size = *response_size;

    (*response_size) = snprintf(response, resp_buf_size, "Board status OK!\n");
    response[(*response_size)++] = 0;

    return PB_OK;
}

int cm_board_init(void)
{
    int rc;

    /*
     * USB1. After a serial download boot the ROM has powered HSIOMIX and the
     * USB1 PHY and set their clocks; booting from eMMC needs the GPC power
     * up as well (not done yet).
     */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(IMX8MP_HSIO_AXI_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(IMX8MP_CCGR_HSIO), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(IMX8MP_CCGR_USB), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(IMX8MP_CCGR_USB_PHY), CCGR_CLK_ON_MASK);
    mmio_clrsetbits_32(IMX8MP_HSIO_BLK_CTRL_GPR_REG0, 0, HSIO_GPR_REG0_USB_CLOCK_MODULE_EN);

    rc = imx8m_usb_phy_init(IMX8M_USB1_PHY_BASE);

    if (rc != PB_OK) {
        LOG_ERR("USB PHY init failed (%i)", rc);
        return rc;
    }

    rc = imx_dwc3_udc_init(IMX8M_USB1_BASE);

    if (rc != PB_OK) {
        LOG_ERR("dwc3 failed (%i)", rc);
        return rc;
    }

    rc = pb_dev_cls_init();

    if (rc != PB_OK)
        return rc;

    static const struct cm_config cfg = {
        .name = "imx8mpevk",
        .status = board_status,
        .password_auth = NULL,
        .command = board_command,
        .tops = {
            .init = usbd_init,
            .connect = usbd_connect,
            .disconnect = usbd_disconnect,
            .read = pb_dev_cls_read,
            .write = pb_dev_cls_write,
            .complete = pb_dev_cls_xfer_complete,
        },
    };

    return cm_init(&cfg);
}
