/**
 * Punch BOOT
 *
 * Copyright (C) 2025 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <board/password.h>
#include <boot/ab_state.h>
#include <boot/atf.h>
#include <boot/boot.h>
#include <boot/image.h>
#include <drivers/crypto/imx_caam.h>
#include <drivers/mmc/imx_usdhc.h>
#include <drivers/mmc/mmc_core.h>
#include <drivers/partition/gpt.h>
#include <drivers/uart/imx_lpuart.h>
#include <pb/board.h>
#include <pb/cm.h>
#include <pb/console.h>
#include <pb/crypto.h>
#include <pb/delay.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <stdbool.h>
#include <uuid.h>

#include "partitions.h"

#define USDHC_PAD_CTRL                                                                             \
    (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | (SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | \
     (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) |                                                  \
     (SC_PAD_28FDSOI_DSE_18V_HS << PADRING_DSE_SHIFT) |                                            \
     (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define USDHC_CLK_PAD_CTRL                                                                         \
    (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | (SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | \
     (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) |                                                  \
     (SC_PAD_28FDSOI_DSE_18V_HS << PADRING_DSE_SHIFT) |                                            \
     (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

static int usdhc_emmc_setup(void)
{
    int rc;
    unsigned int rate;

    sc_pm_set_resource_power_mode(imx8x_ipc(), SC_R_SDHC_0, SC_PM_PW_MODE_ON);
    sc_pm_clock_enable(imx8x_ipc(), SC_R_SDHC_0, SC_PM_CLK_PER, false, false);

    rc = sc_pm_set_clock_parent(imx8x_ipc(), SC_R_SDHC_0, 2, SC_PM_PARENT_PLL1);

    if (rc != SC_ERR_NONE) {
        LOG_ERR("usdhc set clock parent failed");
        return -PB_ERR;
    }

    rate = MHz(200);
    sc_pm_set_clock_rate(imx8x_ipc(), SC_R_SDHC_0, 2, &rate);

    rc = sc_pm_clock_enable(imx8x_ipc(), SC_R_SDHC_0, SC_PM_CLK_PER, true, false);

    if (rc != SC_ERR_NONE) {
        LOG_ERR("SDHC_0 per clk enable failed!");
        return -PB_ERR;
    }

    static const struct imx8x_pad pads[] = {
        { SC_P_EMMC0_CLK, USDHC_CLK_PAD_CTRL }, { SC_P_EMMC0_CMD, USDHC_PAD_CTRL },
        { SC_P_EMMC0_DATA0, USDHC_PAD_CTRL },   { SC_P_EMMC0_DATA1, USDHC_PAD_CTRL },
        { SC_P_EMMC0_DATA2, USDHC_PAD_CTRL },   { SC_P_EMMC0_DATA3, USDHC_PAD_CTRL },
        { SC_P_EMMC0_DATA4, USDHC_PAD_CTRL },   { SC_P_EMMC0_DATA5, USDHC_PAD_CTRL },
        { SC_P_EMMC0_DATA6, USDHC_PAD_CTRL },   { SC_P_EMMC0_DATA7, USDHC_PAD_CTRL },
        { SC_P_EMMC0_STROBE, USDHC_PAD_CTRL },  { SC_P_EMMC0_RESET_B, USDHC_PAD_CTRL },
    };

    rc = imx8x_pads_setup(pads, ARRAY_SIZE(pads));

    if (rc != PB_OK)
        return rc;

    static const struct imx_usdhc_config cfg = { .base = IMX8X_USDHC1_BASE,
                                                 .delay_tap = 18,
                                                 .mmc_config = {
                                                     .mode = MMC_BUS_MODE_HS200,
                                                     .width = MMC_BUS_WIDTH_8BIT,
                                                     .boot_mode = EXT_CSD_BOOT_DDR |
                                                                  EXT_CSD_BOOT_BUS_WIDTH_8,
                                                     .boot0_uu = PART_boot0,
                                                     .boot1_uu = PART_boot1,
                                                     .user_uu = PART_user,
                                                     .rpmb_uu = PART_rpmb,
                                                     .flags = 0,
                                                 } };

    return imx_usdhc_init(&cfg, rate);
}

static int early_boot(void)
{
    sc_bool_t btn_status;
    sc_misc_bt_t boot_type;

    sc_misc_get_button_status(imx8x_ipc(), &btn_status);

    /* Always stop when button is pressed */
    if (btn_status == 1) {
        return -PB_ERR_ABORT;
    }

    sc_misc_get_boot_type(imx8x_ipc(), &boot_type);

    /* Don't stop boot flow when command mode requested the boot */
    if (!(boot_get_flags() & BOOT_FLAG_CMD) && (boot_type == SC_MISC_BT_SERIAL)) {
        return -PB_ERR_ABORT;
    }

    return PB_OK;
}

static int board_password_auth(const char *password, size_t length)
{
    /* This is just a simplistic example of password authentication.
     *
     * A real implementation should use some kind of OTP storage for a
     * device unique password that's not easily accesible.
     * For example RPMB if there is an eMMC present.
     *
     * The whole password is compared, in constant time, and not logged
     * (board.md 4.4).
     */
    static const char expected[] = "imx8qxpmek";

    return board_password_check(expected, sizeof(expected) - 1, password, length);
}

/*
 * Warning: an example configuration. Be careful with the slc commands.
 * slc_set_configuration() fuses the SRK hash of CONFIG_SRK_FUSE, the keys pb
 * is signed with, then these: boot from the eMMC on uSDHC0, fast boot, high
 * speed, 8-bit DDR, also with the boot pins at "boot from fuses" (BT_FUSE_SEL).
 */
static const struct slc_fuse config_fuses[] = {
    { IMX8X_FUSE_BOOT0, IMX8X_BOOT0_FUSE_SEL | IMX8X_BOOT0_EMMC0 },
    { IMX8X_FUSE_BOOT1,
      IMX8X_BOOT1_FAST | IMX8X_BOOT1_MMC_HS | IMX8X_BOOT1_BUS(IMX8X_BUS_8BIT_DDR) },
};

static void board_console_init(void)
{
    sc_pm_clock_rate_t rate = 80000000;
    uintptr_t uart_base = 0;

    uart_base = IMX8X_LPUART0_BASE;
    /* Power up UART0 */
    sc_pm_set_resource_power_mode(imx8x_ipc(), SC_R_UART_0, SC_PM_PW_MODE_ON);

    /* Set UART0 clock root to 80 MHz */
    sc_pm_set_clock_rate(imx8x_ipc(), SC_R_UART_0, SC_PM_CLK_PER, &rate);

    /* Enable UART0 clock root */
    sc_pm_clock_enable(imx8x_ipc(), SC_R_UART_0, SC_PM_CLK_PER, true, false);

    /* Configure UART pads */
    static const struct imx8x_pad pads[] = {
        { SC_P_UART0_RX, UART_PAD_CTRL },
        { SC_P_UART0_TX, UART_PAD_CTRL },
    };

    (void)imx8x_pads_setup(pads, ARRAY_SIZE(pads));

    (void)imx_lpuart_init(uart_base, rate, 115200);

    static const struct console_ops ops = {
        .putc = imx_lpuart_putc,
    };

    console_init(uart_base, &ops);
}

static int board_hw_init(void)
{
    int rc;

    /* Initialize CAAM JR2, JR0 and JR1 are owned by the SECO */
    sc_pm_set_resource_power_mode(imx8x_ipc(), SC_R_CAAM_JR2, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(imx8x_ipc(), SC_R_CAAM_JR2_OUT, SC_PM_PW_MODE_ON);

    rc = imx_caam_init(IMX_CAAM_JR2_BASE);

    if (rc != PB_OK) {
        LOG_ERR("CAAM init failed (%i)", rc);
        return rc;
    }

    rc = usdhc_emmc_setup();

    if (rc != PB_OK)
        LOG_ERR("usdhc init failed (%i)", rc);

    return rc;
}

static const struct rot_config rot = {
    .revoke_key = imx8x_revoke_key,
    .read_key_status = imx8x_read_key_status,
};

/*
 * No set_configuration_locked: the build can't sign pb yet (makefile.mk,
 * imx8xl_sign_image), and a closed DXL would refuse the only image there
 * is. 'slc lock' answers NOT_SUPPORTED until signing exists (F-212).
 */
static const struct slc_config slc = {
    .read_status = imx8x_slc_read_status,
    .set_configuration = imx8x_slc_set_configuration,
    .fuses = config_fuses,
    .fuses_length = ARRAY_SIZE(config_fuses),
    .fuse_read = imx8x_fuse_read,
    .fuse_write = imx8x_fuse_write,
};

/*
 * Linux, through TF-A (boot.md 5.3): BL31 gets the kernel's part as BL33 in
 * the bl_params list in x0, with the device tree in x0 there. Until 3.0.0 pb
 * started the part "atf" directly, with x0 to x3 = 0, and named no BL33.
 *
 * Only built, not run on a board, and this tree builds no image for the
 * board. Not known: what the board's BL31 does with the list, and what an
 * image in the field has in "atf" and at the address its BL31 starts BL33
 * at (F-295).
 */
static const struct boot_handover_image_config boot_image = {
    .image_bpak_id = 0xec103b08, /* bpak_id("kernel") */
    .dtb_bpak_id = 0x56f91b86, /* bpak_id("dt")  */
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
 * Where an image's parts may be loaded (board.md 2.9): the DRAM, 1 GiB from
 * 0x80000000, without pb, which runs in it (src/plat/imx8x/link.lds). BL31
 * goes where pb's reset stub was. pb is linked from 0x80020000 and its
 * buffers from 0x85000000, 16 MiB each. pb's 16 MiB aren't all kept: the
 * build uses less than 256 KiB, and a kernel with the older text offset is at
 * 0x80080000 (the MEK's images, this board's aren't known). board_setup()
 * checks the list against what is linked.
 */
static const struct boot_image_region image_regions[] = {
    BOOT_IMAGE_REGION(IMX8X_ATF_BASE, IMX8X_ATF_SIZE),
    BOOT_IMAGE_REGION(0x80080000, 0x85000000 - 0x80080000), /* Between pb and its buffers */
    BOOT_IMAGE_REGION(0x86000000, 0xc0000000 - 0x86000000),
    BOOT_IMAGE_REGION_END,
};

const struct board board = {
    .name = "imx8dxlevk",
    .console_init = board_console_init,
    .rot = &rot,
    .slc = &slc,
    .init = board_hw_init,
    .ptbl = PTBL_GPT(PART_user, gpt_tables),
    .boot_policy = BOOT_POLICY_AB(&boot_state_cfg),
    .boot_handover = BOOT_HANDOVER_IMAGE(&boot_image),
    .boot_delivery = BOOT_DELIVERY_TFA(&boot_tfa),
    .image_regions = image_regions,
    .early_boot = early_boot,
    .bootargs = "console=ttyLP0,115200 quiet ",
    .bootargs_verbose = "console=ttyLP0,115200  "
                        "earlycon=adma_lpuart32,0x5a060000,115200 earlyprintk ",
    .password_auth = board_password_auth,
};
