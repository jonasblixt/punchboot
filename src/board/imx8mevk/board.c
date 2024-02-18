/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

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
#include <plat/imx8m/imx8m.h>
#include <stdbool.h>
#include <stdio.h>

#include "partitions.h"

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

static int board_command(uint32_t command,
                         uint8_t *bfr,
                         size_t size,
                         uint8_t *response_bfr,
                         size_t *response_size)
{
    LOG_DBG("%x, %p, %zu", command, bfr, size);

    if (command == 0xf93ba110) {
        LOG_DBG("Got test command");
        char *response = (char *)response_bfr;
        size_t resp_buf_size = *response_size;

        (*response_size) = snprintf(response, resp_buf_size, "Test command hello 0x%x\n", command);

        response[(*response_size)++] = 0;
    } else {
        *response_size = 0;
    }

    return PB_OK;
}

void board_console_init(struct imx8m_platform *plat_)
{
    /* Enable UART1 clock */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(UART1_CLK_ROOT), CLK_ROOT_ON);
    /* Ungate UART1 clock */
    mmio_write_32(CCM_CCGR_SET(CCGR_UART1), CCGR_CLK_ON_MASK);

    /* UART1 pad mux */
    mmio_write_32(IOMUXC_MUX_UART1_RXD, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_UART1_TXD, MUX_ALT(0));

    /* UART1 PAD settings */
    mmio_write_32(IOMUXC_PAD_UART1_RXD, PAD_CTL_DSE_40_OHM);
    mmio_write_32(IOMUXC_PAD_UART1_TXD, PAD_CTL_DSE_40_OHM);

    imx_uart_init(IMX8M_UART1_BASE, MHz(25), 115200);

    static const struct console_ops ops = {
        .putc = imx_uart_putc,
    };

    console_init(IMX8M_UART1_BASE, &ops);
}

static int usdhc_emmc_setup(void)
{
    unsigned int rate;
    /* Enable and ungate USDHC1 clock */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(USDHC1_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_USDHC1), CCGR_CLK_ON_MASK);

    /* USDHC1 mux */
    mmio_write_32(IOMUXC_MUX_SD1_CLK, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_CMD, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA0, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA1, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA2, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA3, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA4, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA5, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA6, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA7, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_RESET_B, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_STROBE, MUX_ALT(0));

    /* Setup USDHC1 pins */
#define USDHC1_PAD_CONF (PAD_CTL_HYS | PAD_CTL_PUE | PAD_CTL_SRE_FAST | PAD_CTL_DSE_45_OHM)
    mmio_write_32(IOMUXC_PAD_SD1_CLK, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_CMD, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA0, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA1, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA2, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA3, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA4, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA5, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA6, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA7, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_STROBE, USDHC1_PAD_CONF);

    rate = MHz(200);

    static const struct imx_usdhc_config cfg = {
        .base = IMX8M_USDHC1_BASE,
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

    return imx_usdhc_init(&cfg, rate);
}

int board_init(struct imx8m_platform *plat_)
{
    int rc;

    rc = imx_caam_init(IMX8M_CAAM_JR1_BASE);

    if (rc != PB_OK) {
        LOG_ERR("CAAM init failed (%i)", rc);
        return rc;
    }

    usdhc_emmc_setup();

    bio_dev_t user_part = bio_get_part_by_uu(PART_user);

    if (user_part < 0) {
        LOG_ERR("Can't find partition (%i)", user_part);
    }

    (void)gpt_ptbl_init(user_part, gpt_tables, ARRAY_SIZE(gpt_tables));
    /* eMMC User partition now only has the visible flag to report capacity */
    (void)bio_set_flags(user_part, BIO_FLAG_VISIBLE);

    static const struct rot_config rot_config = {
        .revoke_key = NULL,
        .read_key_status = NULL,
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

    return -PB_ERR;
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

    /* Enable and ungate USB clocks */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(USB_BUS_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(USB_CORE_REF_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(USB_PHY_REF_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_USB_CTRL1), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_USB_PHY1), CCGR_CLK_ON_MASK);

    rc = imx8m_usb_phy_init(IMX8M_USB1_PHY_BASE);

    if (rc != PB_OK) {
        LOG_ERR("imx8m usb phy failed (%i)", rc);
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
        .name = "imx8mevk",
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
