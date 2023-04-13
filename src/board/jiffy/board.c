/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <pb/mmio.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <pb/cm.h>
#include <boot/boot.h>
#include <boot/linux.h>
#include <boot/ab_state.h>
#include <drivers/block/bio.h>
#include <drivers/mmc/mmc_core.h>
#include <drivers/mmc/imx_usdhc.h>
#include <drivers/partition/gpt.h>
#include <drivers/usb/usbd.h>
#include <drivers/usb/imx_usb2_phy.h>
#include <drivers/usb/imx_ehci.h>
#include <drivers/usb/pb_dev_cls.h>
#include <drivers/crypto/imx_caam.h>
#include <drivers/fuse/imx_ocotp.h>
#include <plat/imx6ul/imx6ul.h>
#include <plat/imx6ul/fusebox.h>
#include <libfdt.h>

#include "partitions.h"

static struct imx6ul_platform *plat;

static const struct gpt_part_table gpt_tbl[]=
{
    {
        .uu = UUID_2af755d8_8de5_45d5_a862_014cfa735ce0,
        .description = "System A",
        .size = SZ_MB(30),
    },
    {
        .uu = UUID_c046ccd8_0f2e_4036_984d_76c14dc73992,
        .description = "System B",
        .size = SZ_MB(30),
    },
    {
        .uu = UUID_c284387a_3377_4c0f_b5db_1bcbcff1ba1a,
        .description = "Root A",
        .size = SZ_MB(128),
    },
    {
        .uu = UUID_ac6a1b62_7bd0_460b_9e6a_9a7831ccbfbb,
        .description = "Root B",
        .size = SZ_MB(128),
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
        .size = SZ_MB(30),
    },
    {
        .uu = UUID_da2ca04f_a693_4284_b897_3906cfa1eb13,
        .description = "Root overlay B",
        .size = SZ_MB(30),
    },
    {
        .uu = UUID_23477731_7e33_403b_b836_899a0b1d55db,
        .description = "RoT extension A",
        .size = SZ_kB(128),
    },
    {
        .uu = UUID_6ffd077c_32df_49e7_b11e_845449bd8edd,
        .description = "RoT extension B",
        .size = SZ_kB(128),
    },
    {
        .uu = UUID_9697399d_e2da_47d9_8eb5_88daea46da1b,
        .description = "System storage A",
        .size = SZ_MB(128),
    },
    {
        .uu = UUID_c5b8b41c_0fb5_494d_8b0e_eba400e075fa,
        .description = "System storage B",
        .size = SZ_MB(128),
    },
    {
        .uu = UUID_39792364_d3e3_4013_ac51_caaea65e4334,
        .description = "Mass storage",
        .size = SZ_GB(1),
    },
};

static int usdhc_emmc_setup(void)
{
    unsigned int rate;

    /* Configure pinmux for usdhc1 */
    mmio_write_32(0x020E0000+0x1C0, 0); /* CLK MUX */
    mmio_write_32(0x020E0000+0x1BC, 0); /* CMD MUX */
    mmio_write_32(0x020E0000+0x1C4, 0); /* DATA0 MUX */
    mmio_write_32(0x020E0000+0x1C8, 0); /* DATA1 MUX */
    mmio_write_32(0x020E0000+0x1CC, 0); /* DATA2 MUX */
    mmio_write_32(0x020E0000+0x1D0, 0); /* DATA3 MUX */
    mmio_write_32(0x020E0000+0x1A8, 1); /* DATA4 MUX */
    mmio_write_32(0x020E0000+0x1AC, 1); /* DATA5 MUX */
    mmio_write_32(0x020E0000+0x1B0, 1); /* DATA6 MUX */
    mmio_write_32(0x020E0000+0x1B4, 1); /* DATA7 MUX */
    mmio_write_32(0x020E0000+0x1A4, 1); /* RESET MUX */

    // TODO: What's our input clock rate?
    rate = MHz(200);

    static const struct imx_usdhc_config cfg = {
        .base = 0x02190000,
        .delay_tap = 0,
        .mmc_config = {
            .mode = MMC_BUS_MODE_DDR52,
            .width = MMC_BUS_WIDTH_8BIT_DDR,
            .boot_mode = EXT_CSD_BOOT_DDR | EXT_CSD_BOOT_BUS_WIDTH_8,
            .boot0_uu = PART_boot0,
            .boot1_uu = PART_boot1,
            .user_uu = PART_user,
            .rpmb_uu = PART_rpmb,
            .flags = 0,
        }
    };

    return imx_usdhc_init(&cfg, rate);
}

static int early_boot(void)
{
    /* Check force recovery input switch */
    if ( (mmio_read_32(0x020A8008) & (1 << 4)) == 0)
        return -PB_ERR_ABORT;

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

static int board_status(uint8_t *response_bfr,
                        size_t *response_size)
{
    char *response = (char *) response_bfr;
    size_t resp_buf_size = *response_size;

    (*response_size) = snprintf(response, resp_buf_size,
                            "Board status OK!\n");
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

int board_init(struct imx6ul_platform *plat_)
{
    int rc;

    plat = plat_;
   /* Configure NAND_DATA2 as GPIO4 4 Input with PU,
    *
    * This is used to force recovery mode
    *
    **/

    mmio_write_32(0x020E0188, 6);
    mmio_write_32(0x020E0414, 0x2000 | (1 << 14) | (1 << 12));

    rc = imx_caam_init(0x02141000);

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

    rc = gpt_ptbl_init(user_part, gpt_tbl, ARRAY_SIZE(gpt_tbl));
    /* eMMC User partition now only has the visible flag to report capacity */
    (void) bio_set_flags(user_part, BIO_FLAG_VISIBLE);

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
        .image_bpak_id     = 0xec103b08,    /* bpak_id("kernel") */
        .dtb_bpak_id       = 0x56f91b86,    /* bpak_id("dt")  */
        .ramdisk_bpak_id   = 0xf4cdac1f,    /* bpak_id("ramdisk") */
        .dtb_patch_cb      = board_patch_bootargs,
        .resolve_part_name = boot_ab_part_uu_to_name,
        .set_dtb_boot_arg  = true,
    };

    rc = boot_driver_linux_init(&linux_boot_cfg);

    if (rc != PB_OK) {
        goto err_out;
    }

    static const struct boot_driver boot_driver = {
        .default_boot_source = BOOT_SOURCE_BIO,
        .early_boot_cb       = early_boot,
        .get_boot_bio_device = boot_ab_state_get,
        .set_boot_partition  = boot_ab_state_set_boot_partition,
        .get_boot_partition  = boot_ab_state_get_boot_partition,
        .prepare             = boot_driver_linux_prepare,
        .late_boot_cb        = NULL,
        .jump                = boot_driver_linux_jump,
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
    mmio_clrsetbits_32(0x020C8010, 0, 1 << 6);


    /* Power up USB */
    mmio_write_32(0x020C9038, (1 << 31) | (1 << 30));
    mmio_write_32(0x020C9008, 0xFFFFFFFF);

    imx_usb2_phy_init(0x020C9000);
    rc = imx_ehci_init(0x02184000);

    if (rc != PB_OK) {
        LOG_ERR("imx_ehci_init failed (%i)", rc);
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
