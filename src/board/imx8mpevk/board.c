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
 * Linux boots through TF-A: the boot image has BL31 in the part "atf" and
 * Linux as BL33 in "kernel" (testbench/board/common/post-image.sh).
 */

#include <boot/ab_state.h>
#include <boot/atf.h>
#include <boot/boot.h>
#include <boot/image.h>
#include <drivers/crypto/imx_caam.h>
#include <drivers/mmc/imx_usdhc.h>
#include <drivers/partition/gpt.h>
#include <drivers/pinctrl/imx_iomuxc.h>
#include <drivers/uart/imx_uart.h>
#include <pb/board.h>
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

static void board_console_init(void)
{
    /* UART2_CLK_ROOT runs from the 24 MHz reference after reset */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(UART2_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_UART2), CCGR_CLK_ON_MASK);

    static const struct imx_pin pins[] = {
        IMX8M_PIN(UART2_RXD, MUX_ALT(0), IMX_PAD_KEEP),
        IMX8M_PIN(UART2_TXD, MUX_ALT(0), IMX_PAD_KEEP),
    };

    imx_iomuxc_setup(pins, ARRAY_SIZE(pins));

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

#define USDHC3_PAD_CLK             (PAD_CTL_HYS | PAD_CTL_FSEL_FAST | PAD_CTL_DSE_X6)
#define USDHC3_PAD_DATA            (PAD_CTL_PE | PAD_CTL_PUE | USDHC3_PAD_CLK)
#define USDHC3_PAD_STROBE          (PAD_CTL_PE | USDHC3_PAD_CLK) /* Pull down */
    /* ALT2, and the select inputs: they default to the ENET pads */
#define USDHC3_PIN(n, pad, in_reg) IMX8M_PIN_IN(n, MUX_ALT(2), pad, in_reg, 1)
    static const struct imx_pin pins[] = {
        USDHC3_PIN(NAND_WE_B, USDHC3_PAD_CLK, IOMUXC_USDHC3_CARD_CLK_IN_SELECT_INPUT),
        USDHC3_PIN(NAND_WP_B, USDHC3_PAD_DATA, IOMUXC_USDHC3_CMD_IN_SELECT_INPUT),
        USDHC3_PIN(NAND_DATA04, USDHC3_PAD_DATA, IOMUXC_USDHC3_DAT_IN_SELECT_INPUT(0)),
        USDHC3_PIN(NAND_DATA05, USDHC3_PAD_DATA, IOMUXC_USDHC3_DAT_IN_SELECT_INPUT(1)),
        USDHC3_PIN(NAND_DATA06, USDHC3_PAD_DATA, IOMUXC_USDHC3_DAT_IN_SELECT_INPUT(2)),
        USDHC3_PIN(NAND_DATA07, USDHC3_PAD_DATA, IOMUXC_USDHC3_DAT_IN_SELECT_INPUT(3)),
        USDHC3_PIN(NAND_RE_B, USDHC3_PAD_DATA, IOMUXC_USDHC3_DAT_IN_SELECT_INPUT(4)),
        USDHC3_PIN(NAND_CE2_B, USDHC3_PAD_DATA, IOMUXC_USDHC3_DAT_IN_SELECT_INPUT(5)),
        USDHC3_PIN(NAND_CE3_B, USDHC3_PAD_DATA, IOMUXC_USDHC3_DAT_IN_SELECT_INPUT(6)),
        USDHC3_PIN(NAND_CLE, USDHC3_PAD_DATA, IOMUXC_USDHC3_DAT_IN_SELECT_INPUT(7)),
        USDHC3_PIN(NAND_CE1_B, USDHC3_PAD_STROBE, IOMUXC_USDHC3_STROBE_SELECT_INPUT),
        IMX8M_PIN(NAND_READY_B, MUX_ALT(2), USDHC3_PAD_DATA), /* RESET_B */
    };

    imx_iomuxc_setup(pins, ARRAY_SIZE(pins));

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

/*
 * The microSD slot on uSDHC2. Not set up: the card isn't needed to boot from
 * eMMC and its init takes ~49 ms, most of it the card powering up (ACMD41).
 * Kept as an example, set BOARD_SD_CARD to use it.
 */
#define BOARD_SD_CARD 0

static int usdhc_sd_setup(void)
{
    /* USDHC2_CLK_ROOT: SYSTEM_PLL1_DIV2 (400 MHz) / 2 = 200 MHz */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_CLR(IMX8MP_USDHC2_CLK_ROOT),
                  CLK_ROOT_SRC_MUX_MASK | CLK_ROOT_PRE_DIV_MASK);
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(IMX8MP_USDHC2_CLK_ROOT),
                  CLK_ROOT_ON | CLK_ROOT_SOURCE_SEL(1) | CLK_ROOT_PRE_DIV(CLK_ROOT_PRE_DIV2));
    mmio_write_32(CCM_CCGR_SET(IMX8MP_CCGR_USDHC2), CCGR_CLK_ON_MASK);

    /*
     * SD2_VSEL (GPIO1_IO04) selects NVCC_SD2 (the PMIC's LDO5):
     * VEND_SPEC.VSELECT 0, 3.3 V. SD2_RESET_B enables the card supply
     * through a pull-up and is left as it is.
     */
#define USDHC2_PAD_CLK  (PAD_CTL_HYS | PAD_CTL_FSEL_FAST | PAD_CTL_DSE_X4)
#define USDHC2_PAD_DATA (PAD_CTL_PE | PAD_CTL_PUE | USDHC2_PAD_CLK)
    static const struct imx_pin pins[] = {
        IMX8M_PIN(SD2_CD_B, MUX_ALT(0), PAD_CTL_HYS | PAD_CTL_PE | PAD_CTL_PUE),
        IMX8M_PIN(SD2_CLK, MUX_ALT(0), USDHC2_PAD_CLK),
        IMX8M_PIN(SD2_CMD, MUX_ALT(0), USDHC2_PAD_DATA),
        IMX8M_PIN(SD2_DATA0, MUX_ALT(0), USDHC2_PAD_DATA),
        IMX8M_PIN(SD2_DATA1, MUX_ALT(0), USDHC2_PAD_DATA),
        IMX8M_PIN(SD2_DATA2, MUX_ALT(0), USDHC2_PAD_DATA),
        IMX8M_PIN(SD2_DATA3, MUX_ALT(0), USDHC2_PAD_DATA),
        IMX8M_PIN(GPIO1_IO04, MUX_ALT(1), PAD_CTL_DSE_X1),
    };

    imx_iomuxc_setup(pins, ARRAY_SIZE(pins));

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

/* Revocation is a fuse, which this board never writes */
static int board_read_key_status(const struct rot_key *key)
{
    (void)key;
    return PB_OK;
}

static int board_hw_init(void)
{
    int rc;

    /* Hash and signature verification. The ROM leaves the CAAM clock on */
    rc = imx_caam_init(IMX8M_CAAM_JR0_BASE);

    if (rc != PB_OK) {
        LOG_ERR("CAAM init failed (%i)", rc);
        return rc;
    }

    /* Linux can't instantiate the RNG when it runs in the normal world. Without
     * it Linux has no hardware RNG, which is no reason not to boot. */
    rc = imx_caam_rng_init(IMX8M_CAAM_BASE);

    if (rc != PB_OK)
        LOG_ERR("CAAM RNG init failed (%i)", rc);

    rc = usdhc_emmc_setup();

    if (rc != PB_OK) {
        LOG_ERR("eMMC init failed (%i)", rc);
        return rc;
    }

    /* The microSD slot: no card, or one that fails, doesn't stop the boot */
    if (BOARD_SD_CARD) {
        rc = usdhc_sd_setup();
        if (rc != PB_OK && rc != -PB_ERR_NOT_FOUND)
            LOG_ERR("SD init failed (%i)", rc);
    }

    return PB_OK;
}

/* Revocation is a fuse, which this board never writes */
static const struct rot_config rot = {
    .read_key_status = board_read_key_status,
};

/* No fuse access, see the top of this file */
static const struct slc_config slc = { 0 };

/* Linux, through TF-A: BL31 starts it as BL33 at EL2, with the device tree in x0 */
static const struct boot_handover_image_config boot_image = {
    .image_bpak_id = 0xec103b08, /* bpak_id("kernel") */
    .dtb_bpak_id = 0x56f91b86, /* bpak_id("dt") */
    .ramdisk_bpak_id = 0xf4cdac1f, /* bpak_id("ramdisk") */
    .dtb_patch_cb = board_patch_bootargs,
    .set_dtb_boot_arg = true,
};

static const struct boot_delivery_tfa_config boot_tfa = {
    .bl31_bpak_id = 0xa697d988, /* bpak_id("atf") */
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

static const struct boot_ab_state_config boot_state_cfg = {
    .primary_state_part_uu = PART_primary_state,
    .backup_state_part_uu = PART_backup_state,
    .sys_a_uu = PART_sys_a,
    .sys_b_uu = PART_sys_b,
};

/*
 * Where an image's parts may be loaded (board.md 2.9): the DRAM pb maps, from
 * 0x40000000 to BOARD_RAM_END, above what pb has at its start: the DDR
 * controller's state and pb's buffers, to 0x41060000
 * (src/plat/imx8m/link.lds). And the on-chip RAM TF-A's BL31 is linked for,
 * above pb, which runs from 0x920000. board_setup() checks the list against
 * what is linked.
 */
static const struct boot_image_region image_regions[] = {
    BOOT_IMAGE_REGION(0x41060000, BOARD_RAM_END - 0x41060000),
    BOOT_IMAGE_REGION(IMX8MP_BL31_BASE, IMX8MP_BL31_SIZE),
    BOOT_IMAGE_REGION_END,
};

const struct board board = {
    .name = "imx8mpevk",
    .console_init = board_console_init,
    .rot = &rot,
    .slc = &slc,
    .init = board_hw_init,
    .ptbl = PTBL_GPT(PART_user, gpt_tables),
    .boot_policy = BOOT_POLICY_AB(&boot_state_cfg),
    .boot_handover = BOOT_HANDOVER_IMAGE(&boot_image),
    .boot_delivery = BOOT_DELIVERY_TFA(&boot_tfa),
    .image_regions = image_regions,
    .bootargs = "console=ttymxc1,115200 quiet ",
    /* arm64 has no earlyprintk: earlycon, on stdout-path */
    .bootargs_verbose = "console=ttymxc1,115200 earlycon ",
};
