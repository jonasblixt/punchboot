/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <stdbool.h>
#include <pb/pb.h>
#include <pb/io.h>
#include <pb/plat.h>
#include <pb/usb.h>
#include <pb/fuse.h>
#include <pb/board.h>
#include <pb/storage.h>
#include <pb/boot.h>
#include <pb/delay.h>
#include <plat/imx/ehci.h>
#include <plat/defs.h>
#include <plat/sci/sci_ipc.h>
#include <plat/sci/sci.h>
#include <plat/sci/svc/seco/sci_seco_api.h>
#include <plat/imx8x/plat.h>
#include <arch/armv8a/timer.h>
#include <arch/arch_helpers.h>
#include <drivers/mmc/mmc_core.h>
#include <drivers/partition/gpt.h>
#include <drivers/mmc/imx_usdhc.h>
#include <plat/defs.h>
#include <uuid.h>
#include <libfdt.h>

#define USDHC_PAD_CTRL    (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
                         (SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | \
                         (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
                         (SC_PAD_28FDSOI_DSE_18V_HS << PADRING_DSE_SHIFT) | \
                         (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define USDHC_CLK_PAD_CTRL    (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
                             (SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | \
                             (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
                             (SC_PAD_28FDSOI_DSE_18V_HS << PADRING_DSE_SHIFT) | \
                             (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))
struct fuse fuses[] =
{
    IMX8X_FUSE_ROW_VAL(730, "SRK0", 0x6147e2e6),
    IMX8X_FUSE_ROW_VAL(731, "SRK1", 0xfc4dc849),
    IMX8X_FUSE_ROW_VAL(732, "SRK2", 0xb410b214),
    IMX8X_FUSE_ROW_VAL(733, "SRK3", 0x0f8d6212),
    IMX8X_FUSE_ROW_VAL(734, "SRK4", 0xad38b486),
    IMX8X_FUSE_ROW_VAL(735, "SRK5", 0x9b806149),
    IMX8X_FUSE_ROW_VAL(736, "SRK6", 0xdd6d397a),
    IMX8X_FUSE_ROW_VAL(737, "SRK7", 0x4c19d87b),
    IMX8X_FUSE_ROW_VAL(738, "SRK8", 0x24ac2acd),
    IMX8X_FUSE_ROW_VAL(739, "SRK9", 0xb6222a62),
    IMX8X_FUSE_ROW_VAL(740, "SRK10", 0xf36d6bd1),
    IMX8X_FUSE_ROW_VAL(741, "SRK11", 0x14cc8e16),
    IMX8X_FUSE_ROW_VAL(742, "SRK12", 0xd749170e),
    IMX8X_FUSE_ROW_VAL(743, "SRK13", 0x22fb187e),
    IMX8X_FUSE_ROW_VAL(744, "SRK14", 0x158f740c),
    IMX8X_FUSE_ROW_VAL(745, "SRK15", 0x8966b0f6),
    IMX8X_FUSE_ROW_VAL(18, "BOOT Config",  0x00000002),
    IMX8X_FUSE_ROW_VAL(19, "Bootconfig2" , 0x00000025),
    IMX8X_FUSE_END,
};

#define UUID_2af755d8_8de5_45d5_a862_014cfa735ce0 (const unsigned char *) "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62\x01\x4c\xfa\x73\x5c\xe0"
#define UUID_c046ccd8_0f2e_4036_984d_76c14dc73992 (const unsigned char *) "\xc0\x46\xcc\xd8\x0f\x2e\x40\x36\x98\x4d\x76\xc1\x4d\xc7\x39\x92"
#define UUID_c284387a_3377_4c0f_b5db_1bcbcff1ba1a (const unsigned char *) "\xc2\x84\x38\x7a\x33\x77\x4c\x0f\xb5\xdb\x1b\xcb\xcf\xf1\xba\x1a"
#define UUID_ac6a1b62_7bd0_460b_9e6a_9a7831ccbfbb (const unsigned char *) "\xac\x6a\x1b\x62\x7b\xd0\x46\x0b\x9e\x6a\x9a\x78\x31\xcc\xbf\xbb"
#define UUID_4581af22_99e6_4a94_b821_b60c42d74758 (const unsigned char *) "\x45\x81\xaf\x22\x99\xe6\x4a\x94\xb8\x21\xb6\x0c\x42\xd7\x47\x58"
#define UUID_da2ca04f_a693_4284_b897_3906cfa1eb13 (const unsigned char *) "\xda\x2c\xa0\x4f\xa6\x93\x42\x84\xb8\x97\x39\x06\xcf\xa1\xeb\x13"
#define UUID_23477731_7e33_403b_b836_899a0b1d55db (const unsigned char *) "\x23\x47\x77\x31\x7e\x33\x40\x3b\xb8\x36\x89\x9a\x0b\x1d\x55\xdb"
#define UUID_6ffd077c_32df_49e7_b11e_845449bd8edd (const unsigned char *) "\x6f\xfd\x07\x7c\x32\xdf\x49\xe7\xb1\x1e\x84\x54\x49\xbd\x8e\xdd"
#define UUID_9697399d_e2da_47d9_8eb5_88daea46da1b (const unsigned char *) "\x96\x97\x39\x9d\xe2\xda\x47\xd9\x8e\xb5\x88\xda\xea\x46\xda\x1b"
#define UUID_c5b8b41c_0fb5_494d_8b0e_eba400e075fa (const unsigned char *) "\xc5\xb8\xb4\x1c\x0f\xb5\x49\x4d\x8b\x0e\xeb\xa4\x00\xe0\x75\xfa"
#define UUID_39792364_d3e3_4013_ac51_caaea65e4334 (const unsigned char *) "\x39\x79\x23\x64\xd3\xe3\x40\x13\xac\x51\xca\xae\xa6\x5e\x43\x34"

static const struct gpt_part_table gpt_tbl[]=
{
    {
        .uu = UUID_2af755d8_8de5_45d5_a862_014cfa735ce0,
        .description = "System A",
        .size = SZ_MB(30),
        .valid = true,
    },
    {
        .uu = UUID_c046ccd8_0f2e_4036_984d_76c14dc73992,
        .description = "System B",
        .size = SZ_MB(30),
        .valid = true,
    },
    {
        .uu = UUID_c284387a_3377_4c0f_b5db_1bcbcff1ba1a,
        .description = "Root A",
        .size = SZ_MB(128),
        .valid = true,
    },
    {
        .uu = UUID_ac6a1b62_7bd0_460b_9e6a_9a7831ccbfbb,
        .description = "Root B",
        .size = SZ_MB(128),
        .valid = true,
    },
    {
        .uu = UUID_f5f8c9ae_efb5_4071_9ba9_d313b082281e,
        .description = "PB State Primary",
        .size = 512,
        .valid = true,
    },
    {
        .uu = UUID_656ab3fc_5856_4a5e_a2ae_5a018313b3ee,
        .description = "PB State Backup",
        .size = 512,
        .valid = true,
    },
    {
        .uu = UUID_4581af22_99e6_4a94_b821_b60c42d74758,
        .description = "Root overlay A",
        .size = SZ_MB(30),
        .valid = true,
    },
    {
        .uu = UUID_da2ca04f_a693_4284_b897_3906cfa1eb13,
        .description = "Root overlay B",
        .size = SZ_MB(30),
        .valid = true,
    },
    {
        .uu = UUID_23477731_7e33_403b_b836_899a0b1d55db,
        .description = "RoT extension A",
        .size = SZ_kB(128),
        .valid = true,
    },
    {
        .uu = UUID_6ffd077c_32df_49e7_b11e_845449bd8edd,
        .description = "RoT extension B",
        .size = SZ_kB(128),
        .valid = true,
    },
    {
        .uu = UUID_9697399d_e2da_47d9_8eb5_88daea46da1b,
        .description = "System storage A",
        .size = SZ_MB(128),
        .valid = true,
    },
    {
        .uu = UUID_c5b8b41c_0fb5_494d_8b0e_eba400e075fa,
        .description = "System storage B",
        .size = SZ_MB(128),
        .valid = true,
    },
    {
        .uu = UUID_39792364_d3e3_4013_ac51_caaea65e4334,
        .description = "Mass storage",
        .size = SZ_GB(1),
        .valid = true,
    },
    {
        .uu = NULL,
        .description = NULL,
        .size = 0,
        .valid = false,
    },
};


const uint32_t rom_key_map[] =
{
    0xa90f9680,
    0x25c6dd36,
    0x52c1eda0,
    0xcca57803,
    0x00000000,
};

static int patch_bootargs(void *plat, void *fdt, int offset, bool verbose_boot)
{
    const char *bootargs = NULL;

    if (verbose_boot) {
        bootargs = "console=ttyLP0,115200  " \
                   "earlycon=adma_lpuart32,0x5a060000,115200 earlyprintk ";
    } else {
        bootargs = "console=ttyLP0,115200 quiet ";
    }
    return fdt_setprop_string(fdt, offset, "bootargs", bootargs);

}

static int usdhc_emmc_setup(struct imx8x_private *priv)
{
    int rc;
    unsigned int rate;

    sc_pm_set_resource_power_mode(priv->ipc, SC_R_SDHC_0, SC_PM_PW_MODE_ON);
    sc_pm_clock_enable(priv->ipc, SC_R_SDHC_0, SC_PM_CLK_PER, false, false);

    rc = sc_pm_set_clock_parent(priv->ipc, SC_R_SDHC_0, 2, SC_PM_PARENT_PLL1);

    if (rc != SC_ERR_NONE) {
        LOG_ERR("usdhc set clock parent failed");
        return -PB_ERR;
    }

    rate = 200000000;
    sc_pm_set_clock_rate(priv->ipc, SC_R_SDHC_0, 2, &rate);

    rc = sc_pm_clock_enable(priv->ipc, SC_R_SDHC_0, SC_PM_CLK_PER, true, false);

    if (rc != SC_ERR_NONE) {
        LOG_ERR("SDHC_0 per clk enable failed!");
        return -PB_ERR;
    }

    sc_pad_set(priv->ipc, SC_P_EMMC0_CLK, USDHC_CLK_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_CMD, USDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA0, USDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA1, USDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA2, USDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA3, USDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA4, USDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA5, USDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA6, USDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA7, USDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_STROBE, USDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_RESET_B, USDHC_PAD_CTRL);

    static const struct imx_usdhc_config cfg = {
        .base = 0x5B010000,
        .mmc_config = {
            .mode = MMC_BUS_MODE_HS200,
            .width = MMC_BUS_WIDTH_8BIT,
            .card_type = MMC_CARD_TYPE_EMMC,
            .flags = 0,
        }
    };

    return imx_usdhc_init(&cfg, rate);
}

/*
 * TODO:
 *  struct platform *plat;
 *
 *  struct imx8x_platform = container_of(plat, struct imx8x_platform, plat);
 */
int board_early_init(void *plat)
{
    struct imx8x_private *priv = IMX8X_PRIV(plat);
    int rc;

    /* TODO: Rework and move the 'usdhc_emmc_setup' to imx8x platform,
     * And make it optional through KConfig. This way the upstream platform
     * code will cover most use cases and the special ones can still quite
     * easily be imlemented on board level.
     */
    rc = usdhc_emmc_setup(priv);

    if (rc != PB_OK) {
        LOG_ERR("usdhc init failed (%i)", rc);
        return rc;
    }

    bio_dev_t user_part = bio_get_part_by_uu(UUID_1aad85a9_75cd_426d_8dc4_e9bdfeeb6875);

    if (user_part < 0)
        return user_part;

    /* Default flags for user eMMC user partition, these  will
     * be inherited when/if a valid GPT partition table is found
     */
    bio_set_flags(user_part, BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE);

    rc = gpt_ptbl_init(user_part, gpt_tbl);

    if (rc != PB_OK) {
        LOG_ERR("GPT ptbl init failed (%i)", rc);
        return rc;
    }

    /* Clear all flags on user partition, since we don't want to expose this
     * over the command mode interface
     */
    bio_set_flags(user_part, 0);
#ifdef __TEST
    bio_dev_t root_a = bio_get_part_by_uu_str("c284387a-3377-4c0f-b5db-1bcbcff1ba1a");

    if (root_a < 0) {
        LOG_ERR("Could not get root a part (%i)", root_a);
        return root_a;
    }

    printf("\n\r--- Read test ---\n\r");
    for (int i = 0; i < 3; i++) {
        unsigned int ts_start = plat_get_us_tick();
        const size_t bytes_to_read = 1024*1024*30;
        bio_read(root_a, 0, bytes_to_read, 0x95000000);
        if (rc == 0) {
            unsigned int ts_end = plat_get_us_tick();
            printf("~%li MB/s\n\r", bytes_to_read / (ts_end-ts_start));
        } else {
            printf("Read failed (%i)\n\r", rc);
        }
    }

    printf("\n\r--- Bpak Header test ---\n\r");
    struct bpak_header hdr;
    int lba = (bio_size(root_a) - sizeof(struct bpak_header)) / bio_block_size(root_a) - 1;
    bio_read(root_a, lba, sizeof(struct bpak_header), (uintptr_t) &hdr);

    if (bpak_valid_header(&hdr) == BPAK_OK) {
        LOG_DBG("Read valid BPAK header from block device %i (%s)",
                    root_a, bio_get_description(root_a));
    } else {
        LOG_ERR("Bad magic: 0x%08x on %i (%s)", hdr.magic, root_a, bio_get_description(root_a));
    }

    printf("\n\rTrying to read beyond the end of the partition\n\r");

    rc = bio_read(root_a, bio_size(root_a) / 512 - 1, 513, (uintptr_t) &hdr);
    LOG_DBG("%i", rc);
#endif
    return PB_OK;
}

bool board_force_command_mode(void *plat)
{
    struct imx8x_private *priv = IMX8X_PRIV(plat);

    sc_bool_t btn_status;
    sc_misc_bt_t boot_type;
    bool usb_charger_detected = false;

    sc_misc_get_button_status(priv->ipc, &btn_status);
    sc_misc_get_boot_type(priv->ipc, &boot_type);

    LOG_INFO("Boot type: %u %i %p", boot_type, btn_status, priv);

    /* Pull up DP for usb charger detection */

    pb_setbit32(1 << 2, 0x5b100000+0xe0);
    LOG_DBG ("USB CHRG detect: 0x%08x",pb_read32(0x5B100000+0xf0));
    pb_delay_ms(1);
    if ((pb_read32(0x5b100000+0xf0) & 0x0C) == 0x0C)
        usb_charger_detected = true;
    pb_clrbit32(1 << 2, 0x5b100000+0xe0);

    if (usb_charger_detected) {
        LOG_INFO("USB Charger condition, entering bootloader");
    }

    return (btn_status == 1) || (boot_type == SC_MISC_BT_SERIAL) ||
            (usb_charger_detected);
}

const char * board_name(void)
{
    return "imx8qxmek";
}

int board_command(void *plat,
                     uint32_t command,
                     void *bfr,
                     size_t size,
                     void *response_bfr,
                     size_t *response_size)
{
    LOG_DBG("%x, %p, %zu", command, bfr, size);
    *response_size = 0;

    return PB_OK;
}

int board_status(void *plat,
                    void *response_bfr,
                    size_t *response_size)
{
    struct imx8x_private *priv = IMX8X_PRIV(plat);
    uint32_t scu_version;
    uint32_t scu_commit;
    uint32_t seco_version;
    uint32_t seco_commit;
    int16_t celsius;
    int8_t tenths;

    char *response = (char *) response_bfr;
    size_t resp_buf_size = *response_size;

    sc_misc_build_info(priv->ipc, &scu_version, &scu_commit);
    sc_seco_build_info(priv->ipc, &seco_version, &seco_commit);
    sc_misc_get_temp(priv->ipc, SC_R_SYSTEM, SC_MISC_TEMP, &celsius,
                        &tenths);

    (*response_size) = snprintf(response, resp_buf_size,
                            "SCFW:    %u, %x\n" \
                            "SECO:    %u, %x\n" \
                            "SOC ID:  %u\n" \
                            "SOC REV: %u\n" \
                            "CPU Temperature: %i.%i deg C",
                            scu_version, scu_commit,
                            seco_version, seco_commit,
                            priv->soc_id, priv->soc_rev,
                            celsius, tenths);

    response[(*response_size)++] = 0;

    return PB_OK;
}

#ifdef CONFIG_AUTH_METHOD_PASSWORD
int board_command_mode_auth(char *password, size_t length)
{
    int rc;
    bool authenticated = false;
    struct pb_storage_map *rpmb_map;
    struct pb_storage_driver *rpmb_drv;
    uint8_t secret[512];
    uint8_t hash[PB_HASH_MAX_LENGTH];
    uuid_t rpmb_uu;

    uuid_parse("8d75d8b9-b169-4de6-bee0-48abdc95c408", rpmb_uu);

    rc = pb_storage_get_part(rpmb_uu, &rpmb_map, &rpmb_drv);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not find partition");
        goto err_out;
    }

    rc = rpmb_drv->map_request(rpmb_drv, rpmb_map);

    if (rc != PB_OK) {
        LOG_ERR("map_request failed");
        goto err_out;
    }

    rc = pb_storage_read(rpmb_drv, rpmb_map, secret, 1, 0);

    if (rc != PB_OK)
    {
        LOG_ERR("RPMB Read failed");
        goto err_release_out;
    }

    printf("RPMB sha256: ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", secret[i] & 0xFF);
    }
    printf("\n\r");

    rc = plat_hash_init(PB_HASH_SHA256);

    if (rc != PB_OK) {
        goto err_release_out;
    }

    rc = plat_hash_update(password, length);

    if (rc != PB_OK) {
        goto err_release_out;
    }

    rc = plat_hash_out(hash, sizeof(hash));

    if (rc != PB_OK) {
        goto err_release_out;
    }

    if (memcmp(secret, hash, length) == 0) {
        LOG_DBG("Password auth: Success");
        authenticated = true;
    } else {
        LOG_DBG("Password auth: Failed");
        authenticated = false;
    }

err_release_out:
    rc = rpmb_drv->map_release(rpmb_drv, rpmb_map);

    if (rc != PB_OK) {
        LOG_ERR("map_release failed");
        return rc;
    }

err_out:
    if ((rc == PB_OK) && (authenticated == true)) {
        return PB_OK;
    } else {
        return -PB_ERR;
    }
}
#endif  // CONFIG_AUTH_METHOD_PASSWORD

const struct pb_boot_config * board_boot_config(void)
{
    static const struct pb_boot_config config = {
        .a_boot_part_uuid  = "2af755d8-8de5-45d5-a862-014cfa735ce0",
        .b_boot_part_uuid  = "c046ccd8-0f2e-4036-984d-76c14dc73992",
        .primary_state_part_uuid = "f5f8c9ae-efb5-4071-9ba9-d313b082281e",
        .backup_state_part_uuid  = "656ab3fc-5856-4a5e-a2ae-5a018313b3ee",
        .image_bpak_id     = 0xa697d988,    /* bpak_id("atf") */
        .dtb_bpak_id       = 0x56f91b86,    /* bpak_id("dt")  */
        .ramdisk_bpak_id   = 0xf4cdac1f,    /* bpak_id("ramdisk") */
        .rollback_mode     = PB_ROLLBACK_MODE_NORMAL,
        .early_boot_cb     = NULL,
        .late_boot_cb      = NULL,
        .dtb_patch_cb      = patch_bootargs,
        .print_time_measurements = true,
    };

    return &config;
}
