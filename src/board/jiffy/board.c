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
#include <boot/image.h>
#include <drivers/mmc/imx_usdhc.h>
#include <drivers/mmc/mmc_core.h>
#include <drivers/partition/gpt.h>
#include <drivers/pinctrl/imx_iomuxc.h>
#include <drivers/uart/imx_uart.h>
#include <pb/bio.h>
#include <pb/board.h>
#include <pb/cm.h>
#include <pb/command.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/slc.h>
#include <plat/imx6ul/imx6ul.h>

#include "partitions.h"

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

    rc = usdhc_emmc_setup();

    if (rc != PB_OK)
        LOG_ERR("usdhc init failed (%i)", rc);

    return rc;
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

static const struct boot_handover_image_config boot_image = {
    .image_bpak_id = 0xec103b08, /* bpak_id("kernel") */
    .dtb_bpak_id = 0x56f91b86, /* bpak_id("dt")  */
    .ramdisk_bpak_id = 0xf4cdac1f, /* bpak_id("ramdisk") */
    .dtb_patch_cb = board_patch_bootargs,
    .set_dtb_boot_arg = true,
};

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

static const struct boot_ab_state_config boot_state_cfg = {
    .primary_state_part_uu = PART_primary_state,
    .backup_state_part_uu = PART_backup_state,
    .sys_a_uu = PART_sys_a,
    .sys_b_uu = PART_sys_b,
};

/*
 * Where an image's parts may be loaded (board.md 2.9): the DRAM, 1 GiB from
 * 0x80000000, without pb, which runs in it. pb is linked from 0x80001000
 * with 1 MiB to grow in, and its buffers from 0x81021000 to 0x82000000,
 * where the kernel of the images in use is.
 *
 * The regions and the link have one source, the defconfig (platform.md
 * 5.1): a region starts where pb's region (CONFIG_PB_ENTRY, CONFIG_PB_SIZE)
 * or its buffers' (CONFIG_PB_BUFFERS_BASE, _SIZE) ends, so the link fails
 * when pb or its buffers grow into one. board_setup() checks the list
 * against what is linked.
 */
#define BOARD_PB_END      ((uint64_t)CONFIG_PB_ENTRY + CONFIG_PB_SIZE)
#define BOARD_BUFFERS_END ((uint64_t)CONFIG_PB_BUFFERS_BASE + CONFIG_PB_BUFFERS_SIZE)

_Static_assert(CONFIG_PB_SIZE != 0,
               "CONFIG_PB_SIZE is where the image data region after pb starts");

static const struct boot_image_region image_regions[] = {
    /* Between pb and its buffers */
    BOOT_IMAGE_REGION(BOARD_PB_END, CONFIG_PB_BUFFERS_BASE - BOARD_PB_END),
    BOOT_IMAGE_REGION(BOARD_BUFFERS_END, BOARD_RAM_END - BOARD_BUFFERS_END),
    BOOT_IMAGE_REGION_END,
};

const struct board board = {
    .name = "jiffy",
    .console_init = board_console_init,
    .slc_fuses = config_fuses,
    .slc_fuses_length = ARRAY_SIZE(config_fuses),
    .init = board_hw_init,
    .ptbl = PTBL_GPT(PART_user, gpt_tables),
    .boot_policy = BOOT_POLICY_AB(&boot_state_cfg),
    .boot_handover = BOOT_HANDOVER_IMAGE(&boot_image),
    .image_regions = image_regions,
    .bootargs = "console=ttymxc1,115200 quiet ",
    .bootargs_verbose = "console=ttymxc1,115200 earlyprintk ",
};
