/**
 * Punch BOOT
 *
 * Copyright (C) 2020, 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * TechNexion's PICO-IMX8MQ (pico8ml): an i.MX8MQ with 1 to 4 GiB of LPDDR4,
 * eMMC on USDHC1, the console on UART1. The pins and the density straps
 * are TechNexion's U-Boot's (board/technexion/pico-imx8mq).
 */

#include <boot/ab_state.h>
#include <drivers/gpio/imx_gpio.h>
#include <drivers/mmc/imx_usdhc.h>
#include <drivers/partition/gpt.h>
#include <drivers/pinctrl/imx_iomuxc.h>
#include <drivers/uart/imx_uart.h>
#include <pb/board.h>
#include <pb/cm.h>
#include <pb/command.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/slc.h>
#include <plat/imx8m/dram.h>
#include <plat/imx8m/fusebox.h>
#include <plat/imx8m/imx8m.h>
#include <stdbool.h>
#include <stdio.h>

#include "lpddr4_timing.h"
#include "partitions.h"

/*
 * The boot configuration fused with the life cycle's (slc configure, board.md
 * 3): the eMMC on USDHC1, 8 bits DDR at normal speed, its IO at 3.3 V
 * (fusebox.h). What the pico8ml fused before 3.0.0, 0x2060. The boot mode
 * stays the pins' (no BT_FUSE_SEL): the board has boot switches
 */
#define PICO8ML_BOOT_CFG                                                              \
    (IMX8MQ_BOOT_CFG_DEVICE(IMX8MQ_BOOT_DEVICE_MMC) | IMX8MQ_BOOT_CFG_USDHC_PORT(0) | \
     IMX8MQ_BOOT_CFG_MMC_BUS_WIDTH(IMX8MQ_BOOT_MMC_BUS_8BIT_DDR) |                    \
     IMX8MQ_BOOT_CFG_MMC_SPEED(IMX8MQ_BOOT_MMC_SPEED_NORMAL))

_Static_assert(PICO8ML_BOOT_CFG == 0x2060, "the pico8ml's boot configuration before 3.0.0");

static const struct slc_fuse config_fuses[] = {
    { IMX8M_FUSE(IMX8MQ_FUSE_BOOT_CFG_BANK, IMX8MQ_FUSE_BOOT_CFG_WORD), PICO8ML_BOOT_CFG },
};

/* DRAM: both set points of the timings, for DDR frequency scaling in Linux */
static const unsigned int pico8ml_dram_mts[] = { 3200, 667 };

struct dram_rates dram_rates = {
    .mts = pico8ml_dram_mts,
    .num = ARRAY_SIZE(pico8ml_dram_mts),
};

#define IMX8MQ_GPIO1_BASE 0x30200000
#define DDR_DET_BANK      2 /* GPIO3 */
#define DDR_DET_1         11
#define DDR_DET_2         12
#define DDR_DET_3         13

/* The DRAM's density in GiB, read from its straps, for the log */
static unsigned int dram_gib;

/*
 * The density straps (TechNexion's spl.c): DDR_DET_1, _2 and _3 on
 * GPIO3_IO11, _12 and _13, the pads NAND_DATA05 to 07.
 *
 *   DDR_DET_1  DDR_DET_2  DDR_DET_3
 *       0          0          1      4 GiB
 *       1          1          1      3 GiB
 *       1          1          0      2 GiB
 *       1          0          1      1 GiB
 *
 * pb itself uses the first GiB (BOARD_DRAM_SIZE, D-079), what every
 * variant has.
 */
const struct imx8m_dram_timing *imx8m_board_dram_timing(void)
{
    static const struct imx_pin pins[] = {
        IMX8M_PIN(NAND_DATA05, MUX_ALT(5), IMX_PAD_KEEP),
        IMX8M_PIN(NAND_DATA06, MUX_ALT(5), IMX_PAD_KEEP),
        IMX8M_PIN(NAND_DATA07, MUX_ALT(5), IMX_PAD_KEEP),
    };
    unsigned int straps;

    imx_iomuxc_setup(pins, ARRAY_SIZE(pins));
    imx_gpio_init(IMX8MQ_GPIO1_BASE);

    straps = (imx_gpio_get_pin(DDR_DET_BANK, DDR_DET_1) << 2) |
             (imx_gpio_get_pin(DDR_DET_BANK, DDR_DET_2) << 1) |
             imx_gpio_get_pin(DDR_DET_BANK, DDR_DET_3);

    switch (straps) {
    case 0x1:
        dram_gib = 4;
        return &dram_timing_4gb;
    case 0x7:
        dram_gib = 3;
        return &dram_timing_3gb;
    case 0x6:
        dram_gib = 2;
        return &dram_timing_2gb;
    case 0x5:
        dram_gib = 1;
        return &dram_timing_1gb;
    default:
        LOG_ERR("Unknown DRAM density straps 0x%x", straps);
        return NULL;
    }
}

/* The board's named command 'test' (command-mode.md 5.6), pb_id() 0xf93ba110,
 * the id it had before 3.0.0. Its answer names that id, as it did */
static int
cmd_test(const uint8_t *request, size_t request_size, uint8_t *response, size_t *response_size)
{
    char *text = (char *)response;
    size_t room = *response_size;

    (void)request;
    (void)request_size;

    LOG_DBG("Got test command");
    int n = snprintf(text, room, "Test command hello 0x%x\n", 0xf93ba110U);

    /* The text and its NUL, cut to the room there is */
    if (n < 0 || room == 0) {
        *response_size = 0;
    } else {
        *response_size = ((size_t)n < room) ? (size_t)n : room - 1;
        text[(*response_size)++] = 0;
    }

    return PB_OK;
}

DEFINE_COMMAND("test", "Answers with a greeting", cmd_test);

static void board_console_init(void)
{
    /* Enable UART1 clock */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(UART1_CLK_ROOT), CLK_ROOT_ON);
    /* Ungate UART1 clock */
    mmio_write_32(CCM_CCGR_SET(CCGR_UART1), CCGR_CLK_ON_MASK);

    static const struct imx_pin pins[] = {
        IMX8M_PIN(UART1_RXD, MUX_ALT(0), PAD_CTL_DSE_40_OHM),
        IMX8M_PIN(UART1_TXD, MUX_ALT(0), PAD_CTL_DSE_40_OHM),
    };

    imx_iomuxc_setup(pins, ARRAY_SIZE(pins));

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

#define USDHC1_PAD_CONF (PAD_CTL_HYS | PAD_CTL_PUE | PAD_CTL_SRE_FAST | PAD_CTL_DSE_45_OHM)
    static const struct imx_pin pins[] = {
        IMX8M_PIN(SD1_CLK, MUX_ALT(0), USDHC1_PAD_CONF),
        IMX8M_PIN(SD1_CMD, MUX_ALT(0), USDHC1_PAD_CONF),
        IMX8M_PIN(SD1_DATA0, MUX_ALT(0), USDHC1_PAD_CONF),
        IMX8M_PIN(SD1_DATA1, MUX_ALT(0), USDHC1_PAD_CONF),
        IMX8M_PIN(SD1_DATA2, MUX_ALT(0), USDHC1_PAD_CONF),
        IMX8M_PIN(SD1_DATA3, MUX_ALT(0), USDHC1_PAD_CONF),
        IMX8M_PIN(SD1_DATA4, MUX_ALT(0), USDHC1_PAD_CONF),
        IMX8M_PIN(SD1_DATA5, MUX_ALT(0), USDHC1_PAD_CONF),
        IMX8M_PIN(SD1_DATA6, MUX_ALT(0), USDHC1_PAD_CONF),
        IMX8M_PIN(SD1_DATA7, MUX_ALT(0), USDHC1_PAD_CONF),
        IMX8M_PIN(SD1_RESET_B, MUX_ALT(0), IMX_PAD_KEEP),
        IMX8M_PIN(SD1_STROBE, MUX_ALT(0), USDHC1_PAD_CONF),
    };

    imx_iomuxc_setup(pins, ARRAY_SIZE(pins));

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

static int board_hw_init(void)
{
    LOG_INFO("LPDDR4: %u GiB, pb uses the first", dram_gib);
    return usdhc_emmc_setup();
}

/*
 * No A/B state and no boot driver yet, as the 8MQ EVK (F-033): board_setup()
 * fails after the partitions and pb always enters command mode.
 */
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

/*
 * Where an image's parts may be loaded (board.md 2.9): the DRAM pb maps, the
 * first GiB, from 0x40000000 to BOARD_RAM_END, above what pb has at its
 * start: the DDR controller's state and pb's buffers (src/plat/imx8m/link.lds).
 * pb itself runs from on-chip RAM. The board has no handover yet, so nothing
 * is loaded.
 *
 * The region and the link have one source, the board's makefile.mk (platform.md 5.1):
 * the region starts where the buffers' region (PB_BUFFERS_BASE,
 * _SIZE) ends, so the link fails when the buffers grow into it.
 */
#define BOARD_BUFFERS_END ((uint64_t)PB_BUFFERS_BASE + PB_BUFFERS_SIZE)

static const struct boot_image_region image_regions[] = {
    BOOT_IMAGE_REGION(BOARD_BUFFERS_END, BOARD_RAM_END - BOARD_BUFFERS_END),
    BOOT_IMAGE_REGION_END,
};

const struct board board = {
    .name = "pico8ml",
    .slc_fuses = config_fuses,
    .slc_fuses_length = ARRAY_SIZE(config_fuses),
    .console_init = board_console_init,
    .init = board_hw_init,
    .ptbl = PTBL_GPT(PART_user, gpt_tables),
    .image_regions = image_regions,
};
