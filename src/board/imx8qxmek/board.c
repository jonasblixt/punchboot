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
#include <pb/gpt_ptbl.h>
#include <pb/board.h>
#include <pb/storage.h>
#include <pb/boot.h>
#include <pb/delay.h>
#include <pb/mmc.h>
#include <plat/imx/ehci.h>
#include <plat/imx/usdhc.h>
#include <plat/defs.h>
#include <plat/sci/sci_ipc.h>
#include <plat/sci/sci.h>
#include <plat/sci/svc/seco/sci_seco_api.h>
#include <plat/imx8x/plat.h>
#include <arch/armv8a/timer.h>
#include <arch/arch_helpers.h>
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

#define DEF_FLAGS (PB_STORAGE_MAP_FLAG_WRITABLE | \
                   PB_STORAGE_MAP_FLAG_VISIBLE)
#ifdef __NOPE
static struct pb_storage_map map[] =
{
    PB_STORAGE_MAP("9eef7544-bf68-4bf7-8678-da117cbccba8",
        "eMMC boot0", 2048, DEF_FLAGS | PB_STORAGE_MAP_FLAG_EMMC_BOOT0 | \
                        PB_STORAGE_MAP_FLAG_STATIC_MAP),

    PB_STORAGE_MAP("4ee31690-0c9b-4d56-a6a6-e6d6ecfd4d54",
        "eMMC boot1", 2048, DEF_FLAGS | PB_STORAGE_MAP_FLAG_EMMC_BOOT1 | \
                        PB_STORAGE_MAP_FLAG_STATIC_MAP),

    PB_STORAGE_MAP("8d75d8b9-b169-4de6-bee0-48abdc95c408",
        "eMMC RPMB", 2048, PB_STORAGE_MAP_FLAG_VISIBLE | \
                            PB_STORAGE_MAP_FLAG_EMMC_RPMB | \
                            PB_STORAGE_MAP_FLAG_STATIC_MAP),

    PB_STORAGE_MAP("2af755d8-8de5-45d5-a862-014cfa735ce0", "System A", 0xf000,
            DEF_FLAGS | PB_STORAGE_MAP_FLAG_BOOTABLE),

    PB_STORAGE_MAP("c046ccd8-0f2e-4036-984d-76c14dc73992", "System B", 0xf000,
            DEF_FLAGS | PB_STORAGE_MAP_FLAG_BOOTABLE),

    PB_STORAGE_MAP("c284387a-3377-4c0f-b5db-1bcbcff1ba1a", "Root A", 0x40000,
            DEF_FLAGS),

    PB_STORAGE_MAP("ac6a1b62-7bd0-460b-9e6a-9a7831ccbfbb", "Root B", 0x40000,
            DEF_FLAGS),

    PB_STORAGE_MAP("f5f8c9ae-efb5-4071-9ba9-d313b082281e", "PB State Primary",
            1, PB_STORAGE_MAP_FLAG_VISIBLE),

    PB_STORAGE_MAP("656ab3fc-5856-4a5e-a2ae-5a018313b3ee", "PB State Backup",
            1, PB_STORAGE_MAP_FLAG_VISIBLE),

    PB_STORAGE_MAP("4581af22-99e6-4a94-b821-b60c42d74758", "Root overlay A",
                        0xf000, DEF_FLAGS),

    PB_STORAGE_MAP("da2ca04f-a693-4284-b897-3906cfa1eb13", "Root overlay B",
                        0xf000, DEF_FLAGS),

    PB_STORAGE_MAP("23477731-7e33-403b-b836-899a0b1d55db", "RoT extension A",
                        0x100, DEF_FLAGS),

    PB_STORAGE_MAP("6ffd077c-32df-49e7-b11e-845449bd8edd", "RoT extension B",
                        0x100, DEF_FLAGS),

    PB_STORAGE_MAP("9697399d-e2da-47d9-8eb5-88daea46da1b", "System storage A",
                        0x40000, DEF_FLAGS),

    PB_STORAGE_MAP("c5b8b41c-0fb5-494d-8b0e-eba400e075fa", "System storage B",
                        0x40000, DEF_FLAGS),

    PB_STORAGE_MAP("39792364-d3e3-4013-ac51-caaea65e4334", "Mass storage",
                        0x200000, DEF_FLAGS),
    PB_STORAGE_MAP_END
};
#endif

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

    rc = usdhc_emmc_setup(priv);

    if (rc != PB_OK) {
        LOG_ERR("usdhc init failed (%i)", rc);
        return rc;
    }

    rc = gpt_ptbl_init(bio_part_get_by_uu(UUID_1aad85a9_75cd_426d_8dc4_e9bdfeeb6875));

    if (rc != PB_OK) {
        LOG_ERR("GPT ptbl init failed (%i)", rc);
        return rc;
    }

    for (int i = 0; i < 3; i++) {
        unsigned int ts_start = plat_get_us_tick();
        const size_t bytes_to_read = 1024*1024*30;
        rc = mmc_read(0, bytes_to_read, 0x95000000);
        if (rc == 0) {
            unsigned int ts_end = plat_get_us_tick();
            printf("~%li MB/s\n\r", bytes_to_read / (ts_end-ts_start));
        } else {
            printf("Read failed (%i)\n\r", rc);
        }
    }

    bio_dev_t root_a = bio_part_get_by_uu_str("c046ccd8-0f2e-4036-984d-76c14dc73992");

    if (root_a < 0)
        LOG_ERR("Could not get root a part");
    else {
        struct bpak_header hdr;
        int lba = (bio_size(root_a) - sizeof(struct bpak_header)) / bio_block_size(root_a) - 1;
        bio_read(root_a, lba, sizeof(struct bpak_header), (uintptr_t) &hdr);

        if (bpak_valid_header(&hdr) == BPAK_OK) {
            LOG_DBG("Read valid BPAK header!");
        } else {
            LOG_ERR("Bad magic: 0x%08x", hdr.magic);
        }
    }

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
