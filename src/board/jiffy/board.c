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
#include <drivers/fuse/imx_ocotp.h>
#include <drivers/mmc/imx_usdhc.h>
#include <drivers/mmc/mmc_core.h>
#include <drivers/partition/gpt.h>
#include <drivers/uart/imx_uart.h>
#include <drivers/usb/imx_ci_udc.h>
#include <drivers/usb/imx_usb2_phy.h>
#include <drivers/usb/pb_dev_cls.h>
#include <drivers/usb/usbd.h>
#include <libfdt.h>
#include <pb/bio.h>
#include <pb/cm.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <plat/imx6ul/fusebox.h>
#include <plat/imx6ul/imx6ul.h>

#include "partitions.h"

static struct imx6ul_platform *plat;

static const struct gpt_part_table default_gpt_tbl[] = {
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
        .table = default_gpt_tbl,
        .table_length = ARRAY_SIZE(default_gpt_tbl),
    },
};

static int usdhc_emmc_setup(void)
{
    /* Ungate USDHC1 clock */
    mmio_clrsetbits_32(IMX6UL_CCM_CCGR6, 0, CCM_CCGR6_USDHC1);

    /* Configure pinmux for usdhc1 */
    mmio_write_32(SW_MUX_CTL_PAD_SD1_CLK, MUX_ALT(0)); /* CLK MUX */
    mmio_write_32(SW_MUX_CTL_PAD_SD1_CMD, MUX_ALT(0)); /* CMD MUX */
    mmio_write_32(SW_MUX_CTL_PAD_SD1_DATA0, MUX_ALT(0)); /* DATA0 MUX */
    mmio_write_32(SW_MUX_CTL_PAD_SD1_DATA1, MUX_ALT(0)); /* DATA1 MUX */
    mmio_write_32(SW_MUX_CTL_PAD_SD1_DATA2, MUX_ALT(0)); /* DATA2 MUX */
    mmio_write_32(SW_MUX_CTL_PAD_SD1_DATA3, MUX_ALT(0)); /* DATA3 MUX */
    mmio_write_32(SW_MUX_CTL_PAD_NAND_READY_B, MUX_ALT(1)); /* DATA4 MUX */
    mmio_write_32(SW_MUX_CTL_PAD_NAND_CE0_B, MUX_ALT(1)); /* DATA5 MUX */
    mmio_write_32(SW_MUX_CTL_PAD_NAND_CE1_B, MUX_ALT(1)); /* DATA6 MUX */
    mmio_write_32(SW_MUX_CTL_PAD_NAND_CLE, MUX_ALT(1)); /* DATA7 MUX */
    mmio_write_32(SW_MUX_CTL_PAD_NAND_DQS, MUX_ALT(1)); /* RESET MUX */

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

    return imx_usdhc_init(&cfg, MHz(plat->usdhc1_clk_MHz));
}

static int early_boot(void)
{
    return PB_OK;
}

static int board_command(uint32_t command,
                         uint8_t *bfr,
                         size_t size,
                         uint8_t *response_bfr,
                         size_t *response_size)
{
    LOG_DBG("%x, %p, %zu", command, bfr, size);
    *response_size = 0;
    return PB_OK;
}

static int board_status(uint8_t *response_bfr, size_t *response_size)
{
    char *response = (char *)response_bfr;
    size_t resp_buf_size = *response_size;

    (*response_size) = snprintf(response, resp_buf_size, "Board status OK!\n");
    response[(*response_size)++] = 0;

    return PB_OK;
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

static int board_set_slc_configuration(void)
{
    int rc;
    /**
     * Warning: This is an example configuration. Be carful with the slc
     * commands.
     *
     * Setting SLC Configuration will fuse SRK values matching the public keys
     * that can be found in the 'pki' directory.
     *
     * This will also configure the SoC to boot from eMMC0 with 8-bit DDR HS.
     */
    static const struct imx6ul_srk_fuses srk_fuses = {
        .srk[0] = 0x5020C7D7,
        .srk[1] = 0xBB62B945,
        .srk[2] = 0xDD97C8BE,
        .srk[3] = 0xDC6710DD,
        .srk[4] = 0x2756B777,
        .srk[5] = 0xEF43BC0A,
        .srk[6] = 0x7185604B,
        .srk[7] = 0x3F335991,
    };

    rc = imx6ul_fuse_write_srk(&srk_fuses);

    if (rc != PB_OK) {
        LOG_ERR("SRK fusing failed (%i)", rc);
        return rc;
    }

    rc = imx_ocotp_write(0, 5, 0x0000c060);
    if (rc != PB_OK) {
        LOG_ERR("Failed to write boot0 config (%i)", rc);
        return rc;
    }

    rc = imx_ocotp_write(0, 6, 0x00000010);
    if (rc != PB_OK) {
        LOG_ERR("Failed to write boot1 config (%i)", rc);
        return rc;
    }
    return PB_OK;
}

void board_console_init(struct imx6ul_platform *plat_)
{
#define UART_PAD_CTRL                                                                          \
    (PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm | \
     PAD_CTL_SRE_FAST | PAD_CTL_HYS)

    /* Ungate uart clock */
    mmio_clrsetbits_32(IMX6UL_CCM_CCGR0, 0, CCM_CCGR0_UART2);

    /* Configure console UART mux and pads*/
    mmio_write_32(SW_MUX_CTL_PAD_UART2_TX_DATA, MUX_ALT(0));
    mmio_write_32(SW_MUX_CTL_PAD_UART2_RX_DATA, MUX_ALT(0));
    mmio_write_32(SW_PAD_CTL_PAD_UART2_TX_DATA, UART_PAD_CTRL);
    mmio_write_32(SW_PAD_CTL_PAD_UART2_RX_DATA, UART_PAD_CTRL);

    imx_uart_init(IMX6UL_UART2_BASE, MHz(80), 115200);

    static const struct console_ops ops = {
        .putc = imx_uart_putc,
    };

    console_init(IMX6UL_UART2_BASE, &ops);
}

int board_init(struct imx6ul_platform *plat_)
{
    int rc;

    plat = plat_;

    rc = imx_caam_init(IMX6UL_CAAM_JR1_BASE);

    if (rc != PB_OK) {
        LOG_ERR("CAAM init failed (%i)", rc);
        return rc;
    }

    rc = usdhc_emmc_setup();

    if (rc != PB_OK) {
        LOG_ERR("usdhc init failed (%i)", rc);
        goto err_out;
    }

    bio_dev_t user_part = bio_get_part_by_uu(PART_user);

    if (user_part < 0) {
        goto err_out;
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
            goto err_out;
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

    if (rc != PB_OK) {
        goto err_out;
    }

    static const struct boot_driver boot_driver = {
        .default_boot_source = BOOT_SOURCE_BIO,
        .early_boot_cb = early_boot,
        .get_boot_bio_device = boot_ab_state_get,
        .set_boot_partition = boot_ab_state_set_boot_partition,
        .get_boot_partition = boot_ab_state_get_boot_partition,
        .prepare = boot_driver_linux_prepare,
        .late_boot_cb = NULL,
        .jump = boot_driver_linux_jump,
    };

    rc = boot_init(&boot_driver);

    if (rc != PB_OK) {
        goto err_out;
    }

    static const struct rot_config rot_config = {
        .revoke_key = imx6ul_revoke_key,
        .read_key_status = imx6ul_read_key_status,
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

    static const struct slc_config slc_config = {
        .read_status = imx6ul_slc_read_status,
        .set_configuration = board_set_slc_configuration,
        .set_configuration_locked = imx6ul_slc_set_configuration_locked,
        .set_eol = NULL,
    };

    rc = slc_init(&slc_config);

    if (rc != PB_OK) {
        LOG_ERR("SLC init failed (%i)", rc);
        return rc;
    }

err_out:
    return rc;
    return PB_OK;
}

int cm_board_init(void)
{
    int rc;
    /* Enable USB PLL */
    mmio_clrsetbits_32(IMX6UL_CCM_ANALOG_PLL_USB1, 0, CCM_ANALOG_PLL_USB1_EN_USB_CLKS);

    imx_usb2_phy_init(IMX6UL_USBPHY1_BASE);
    rc = imx_ci_udc_init(IMX6UL_USB1_BASE);

    if (rc != PB_OK) {
        LOG_ERR("imx_ci_udc_init failed (%i)", rc);
        return rc;
    }

    rc = pb_dev_cls_init();

    if (rc != PB_OK)
        return rc;

    static const struct cm_config cfg = {
        .name = "jiffy",
        .status = board_status,
        .password_auth = NULL,
        .command = board_command,
        .tops = {
            .init = usbd_init,
            .connect = usbd_connect,
            .disconnect = usbd_disconnect,
            .read = pb_dev_cls_read,
            .write = pb_dev_cls_write,
        },
    };

    return cm_init(&cfg);
}
