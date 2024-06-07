#include <boot/armv7m_baremetal.h>
#include <boot/boot.h>
#include <boot/image_helpers.h>
#include <bpak/bpak.h>
#include <drivers/memc/imx_flexspi.h>
#include <drivers/partition/static.h>
#include <drivers/uart/imx_lpuart.h>
#include <drivers/usb/imx_ci_udc.h>
#include <drivers/usb/imx_usb2_phy.h>
#include <drivers/usb/pb_dev_cls.h>
#include <drivers/usb/usbd.h>
#include <pb/cm.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <plat/imxrt/imxrt.h>
#include <string.h>

#include "partitions.h"

static struct imxrt_platform *plat;

#define UUID_1bd35a54_485d_4fb6_a822_cb58c3c5a068 \
    (const unsigned char *)"\x1b\xd3\x5a\x54\x48\x5d\x4f\xb6\xa8\x22\xcb\x58\xc3\xc5\xa0\x68"

static const struct static_part_table nor_table[] = {
    {
        .uu = PART_NOR_PBL,
        .description = "BL",
        .size = SZ_KiB(256),
    },
    {
        .uu = PART_NOR_SBL1,
        .description = "SBL1",
        .size = SZ_KiB(256),
    },
    {
        .uu = PART_NOR_config,
        .description = "Config",
        .size = SZ_KiB(64),
    },
    {
        .uu = PART_NOR_application,
        .description = "Application",
        .size = SZ_KiB(1408),
    },
    {
        .uu = PART_NOR_SBL2,
        .description = "SBL2",
        .size = SZ_KiB(256 + 32 + 4),
    },
    {
        .uu = NULL,
    },
};

static int board_get_in_mem_image(struct bpak_header **header)
{
#ifdef __NOPE
    struct bpak_header *flash_header =
        (struct bpak_header *)(((uint8_t *)MIDDLEWARE_FLASH_ADDRESS) + MIDDLEWARE_IMAGE_SIZE -
                               sizeof(struct bpak_header));

    memcpy(*header, flash_header, sizeof(struct bpak_header));
#endif
    return PB_OK;
}

static int board_late_boot(struct bpak_header *header, uuid_t boot_part_uu)
{
    /* If the command interface initiated the boot, skip charger detection,
     * since we're connected but want to boot anyway */
    if (boot_get_flags() & BOOT_FLAG_CMD)
        return PB_OK;

    /* Perform CDP detection */
    uint32_t charger_status = mmio_read_32(IMXRT_ANATOP_BASE + 0x01D0);
    if (charger_status & (1 << 1)) {
        printf("Charger detected, entering command mode\n\r");
        return -PB_ERR_ABORT;
    }

    /* When serial loading on bench, bootmode registers are set so force command mode */
    uint32_t sbmr2 = mmio_read_32(IMXRT_SRC_BASE + 0x1C);
    if (sbmr2 & (3 << 24)) {
        printf("Serial boot detected, entering command mode\n\r");
        return -PB_ERR_ABORT;
    }

    /* On virgin cards boot-loaded over serial (no PBL loaded), in production
     * without wing board sbmr2 above will be 0b00 (fused boot) and as
     * the ROM code initializes the USB for serial downloader so charge
     * detection does not work either.
     *
     * If no PBL is loaded in start of flash, always go into recovery mode
     */
    uint32_t *pvl_ivt_header_addr = (uint32_t *)(0x60000000 + 0x1000);
    if (*pvl_ivt_header_addr == 0x00000000 || *pvl_ivt_header_addr == 0xFFFFFFFF) {
        printf("No pbl detected, entering command mode\n\r");
        return -PB_ERR_ABORT;
    }

    return PB_OK;
}

#define CUSTOM_LUT_LENGTH                      64 // No of uint32_t's

#define NOR_CMD_LUT_SEQ_IDX_READ_FAST_QUAD     0
#define NOR_CMD_LUT_SEQ_IDX_READSTATUS         1
#define NOR_CMD_LUT_SEQ_IDX_WRITEENABLE        2
#define NOR_CMD_LUT_SEQ_IDX_WRITEDISABLE       3
#define NOR_CMD_LUT_SEQ_IDX_ERASESECTOR        4
#define NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_QUAD   5
#define NOR_CMD_LUT_SEQ_IDX_ERASEBLOCK32       6
#define NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_SINGLE 7
#define NOR_CMD_LUT_SEQ_IDX_READ_NORMAL        8
#define NOR_CMD_LUT_SEQ_IDX_READID             9
#define NOR_CMD_LUT_SEQ_IDX_WRITESTATUSREG     10
#define NOR_CMD_LUT_SEQ_IDX_ENTERQPI           11
#define NOR_CMD_LUT_SEQ_IDX_EXITQPI            12
#define NOR_CMD_LUT_SEQ_IDX_READSTATUSREG      13
#define NOR_CMD_LUT_SEQ_IDX_READ_FAST          14
#define NOR_CMD_LUT_SEQ_IDX_ERASEBLOCK64       15

const uint32_t customNorLUT[CUSTOM_LUT_LENGTH] = {
    /* Read ID */
    [4 * NOR_CMD_LUT_SEQ_IDX_READID] = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR,
                                                       FLEXSPI_1PAD,
                                                       0x9F,
                                                       FLEXSPI_COMMAND_READ_SDR,
                                                       FLEXSPI_1PAD,
                                                       0x04),

    /* Read status */
    [4 * NOR_CMD_LUT_SEQ_IDX_READSTATUS] = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR,
                                                           FLEXSPI_1PAD,
                                                           0x05,
                                                           FLEXSPI_COMMAND_READ_SDR,
                                                           FLEXSPI_1PAD,
                                                           0x04),

    /* Fast read */
    [4 * NOR_CMD_LUT_SEQ_IDX_READ_FAST] = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR,
                                                          FLEXSPI_1PAD,
                                                          0x0B,
                                                          FLEXSPI_COMMAND_RADDR_SDR,
                                                          FLEXSPI_1PAD,
                                                          0x18),
    [4 * NOR_CMD_LUT_SEQ_IDX_READ_FAST + 1] = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_DUMMY_SDR,
                                                              FLEXSPI_1PAD,
                                                              0x08,
                                                              FLEXSPI_COMMAND_READ_SDR,
                                                              FLEXSPI_1PAD,
                                                              0x04),

    /* Write enable */
    [4 * NOR_CMD_LUT_SEQ_IDX_WRITEENABLE] = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR,
                                                            FLEXSPI_1PAD,
                                                            0x06,
                                                            FLEXSPI_COMMAND_STOP,
                                                            FLEXSPI_1PAD,
                                                            0x00),

    /* Write disable */
    [4 * NOR_CMD_LUT_SEQ_IDX_WRITEDISABLE] = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR,
                                                             FLEXSPI_1PAD,
                                                             0x04,
                                                             FLEXSPI_COMMAND_STOP,
                                                             FLEXSPI_1PAD,
                                                             0x00),

    /* Erase 4kb sector */
    [4 * NOR_CMD_LUT_SEQ_IDX_ERASESECTOR] = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR,
                                                            FLEXSPI_1PAD,
                                                            0xD7,
                                                            FLEXSPI_COMMAND_RADDR_SDR,
                                                            FLEXSPI_1PAD,
                                                            0x18),

    /* Erase 64kb block */
    [4U * NOR_CMD_LUT_SEQ_IDX_ERASEBLOCK64] = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR,
                                                              FLEXSPI_1PAD,
                                                              0xD8,
                                                              FLEXSPI_COMMAND_RADDR_SDR,
                                                              FLEXSPI_1PAD,
                                                              0x18),

    /* Page Program - single mode */
    [4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_SINGLE] = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR,
                                                                   FLEXSPI_1PAD,
                                                                   0x02,
                                                                   FLEXSPI_COMMAND_RADDR_SDR,
                                                                   FLEXSPI_1PAD,
                                                                   0x18),
    [4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_SINGLE + 1] = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_WRITE_SDR,
                                                                       FLEXSPI_1PAD,
                                                                       0x04,
                                                                       FLEXSPI_COMMAND_STOP,
                                                                       FLEXSPI_1PAD,
                                                                       0x00),

    /* Page Program - quad mode */
    [4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_QUAD] = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR,
                                                                 FLEXSPI_1PAD,
                                                                 0x32,
                                                                 FLEXSPI_COMMAND_RADDR_SDR,
                                                                 FLEXSPI_1PAD,
                                                                 0x18),
    [4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_QUAD + 1] = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_WRITE_SDR,
                                                                     FLEXSPI_4PAD,
                                                                     0x04,
                                                                     FLEXSPI_COMMAND_STOP,
                                                                     FLEXSPI_1PAD,
                                                                     0x00),

    /* Erase 32kb block */
    [4U * NOR_CMD_LUT_SEQ_IDX_ERASEBLOCK32] = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR,
                                                              FLEXSPI_1PAD,
                                                              0x52,
                                                              FLEXSPI_COMMAND_RADDR_SDR,
                                                              FLEXSPI_1PAD,
                                                              0x18),
};

static int flexspi_nor_setup(void)
{
    int rc;

    // PAD mux SPI DATA00 - alt 1
    mmio_write_32(SW_MUX_CTL_PAD_GPIO_SD_B1_08, IMXRT_IOMUX_ALT(1));
    // PAD mux SPI DATA01 - alt 1
    mmio_write_32(SW_MUX_CTL_PAD_GPIO_SD_B1_09, IMXRT_IOMUX_ALT(1));
    // PAD mux SPI DATA02 - alt 1
    mmio_write_32(SW_MUX_CTL_PAD_GPIO_SD_B1_10, IMXRT_IOMUX_ALT(1));
    // PAD mux SPI DATA03 - alt 1
    mmio_write_32(SW_MUX_CTL_PAD_GPIO_SD_B1_11, IMXRT_IOMUX_ALT(1));
    // PAD mux SPI SCLK - alt 1
    mmio_write_32(SW_MUX_CTL_PAD_GPIO_SD_B1_07, IMXRT_IOMUX_ALT(1));
    // PAD mux SPI SS0_B - alt 1
    mmio_write_32(SW_MUX_CTL_PAD_GPIO_SD_B1_06, IMXRT_IOMUX_ALT(1));
    // PAD mux SPI DQS - alt 1
    mmio_write_32(SW_MUX_CTL_PAD_GPIO_SD_B1_05, IMXRT_IOMUX_SION | IMXRT_IOMUX_ALT(1));

#define FLEXSPI_PAD_CONFIG (IMXRT_IOMUX_SRE | IMXRT_IOMUX_SPEED(3) | IMXRT_IOMUX_DSE(6))

    mmio_write_32(SW_PAD_CTL_PAD_GPIO_SD_B1_08, FLEXSPI_PAD_CONFIG);
    mmio_write_32(SW_PAD_CTL_PAD_GPIO_SD_B1_09, FLEXSPI_PAD_CONFIG);
    mmio_write_32(SW_PAD_CTL_PAD_GPIO_SD_B1_10, FLEXSPI_PAD_CONFIG);
    mmio_write_32(SW_PAD_CTL_PAD_GPIO_SD_B1_11, FLEXSPI_PAD_CONFIG);
    mmio_write_32(SW_PAD_CTL_PAD_GPIO_SD_B1_07, FLEXSPI_PAD_CONFIG);
    mmio_write_32(SW_PAD_CTL_PAD_GPIO_SD_B1_06, FLEXSPI_PAD_CONFIG);
    mmio_write_32(SW_PAD_CTL_PAD_GPIO_SD_B1_05, FLEXSPI_PAD_CONFIG | IMXRT_IOMUX_HYS);

    /* Derive clock from PLL3, target freq: 120 MHz */
    mmio_clrsetbits_32(IMXRT_CCM_CSCMR1,
                       CSCMR1_FLEXSPI_CLK_SEL_MASK | CSCMR1_FLEXSPI_PODF_MASK,
                       CSCMR1_FLEXSPI_CLK_SEL(1) | CSCMR1_FLEXSPI_PODF(3));

    mmio_clrsetbits_32(IMXRT_CCM_CCGR6, 0, CCGR6_FLEXSPI);

    static const struct flexspi_nor_config nor_cfg = {
        .name = "IS25WP064D",
        .uuid = UUID_1bd35a54_485d_4fb6_a822_cb58c3c5a068,
        .port = FLEXSPI_PORT_A1,
        .capacity = SZ_MiB(8),
        .block_size = SZ_KiB(4),
        .page_size = 256,
        .mfg_id = 0x9d,
        .mfg_device_type = 0x70,
        .mfg_capacity = 0x17,
        .time_page_program_ms = 100,
        .cr1 = FLEXSPI_CR1_CS_INTERVAL(2) | FLEXSPI_CR1_TCSH(3) | FLEXSPI_CR1_TCSS(3),
        .cr2 = FLEXSPI_CR2_ARDSEQNUM(1) | FLEXSPI_CR2_ARDSEQID(NOR_CMD_LUT_SEQ_IDX_READ_FAST),

        /* Note: Erase commands must be sorted in descending block size order */
        .erase_cmds = {
            {
                .block_size = SZ_KiB(64),
                .erase_time_ms = 1000,
                .lut_id = NOR_CMD_LUT_SEQ_IDX_ERASEBLOCK64,
            },
            {
                .block_size = SZ_KiB(32),
                .erase_time_ms = 500,
                .lut_id = NOR_CMD_LUT_SEQ_IDX_ERASEBLOCK32,
            },
            {
                .block_size = SZ_KiB(4),
                .erase_time_ms = 300,
                .lut_id = NOR_CMD_LUT_SEQ_IDX_ERASESECTOR,
            },
        },

        .lut_id_read_status = NOR_CMD_LUT_SEQ_IDX_READSTATUS,
        .lut_id_read_id = NOR_CMD_LUT_SEQ_IDX_READID,
        .lut_id_read = NOR_CMD_LUT_SEQ_IDX_READ_FAST,
        .lut_id_write = NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_QUAD,
        .lut_id_wr_enable = NOR_CMD_LUT_SEQ_IDX_WRITEENABLE,
        .lut_id_wr_disable = NOR_CMD_LUT_SEQ_IDX_WRITEDISABLE,
    };

    static const struct flexspi_core_config core_cfg = {
        .base = IMXRT_FLEXSPIC_BASE,
        .cr4 = FLEXSPI_CR4_WMOPT1,
        .dllacr = FLEXSPI_DLLCR_OVRDEN, // No read strobe from flash?
        .dllbcr = FLEXSPI_DLLCR_OVRDEN,
        .lut = customNorLUT,
        .lut_elements = CUSTOM_LUT_LENGTH,
        .mem_length = 1,
        .mem = {
            &nor_cfg,
        },
    };

    rc = imx_flexspi_init(&core_cfg);

    if (rc != 0) {
        LOG_ERR("Flexspi init failed (%i)", rc);
        return rc;
    }

    bio_dev_t nor_dev = bio_get_part_by_uu(UUID_1bd35a54_485d_4fb6_a822_cb58c3c5a068);

    if (nor_dev < 0) {
        LOG_ERR("Can't find NOR Memory (%i)", nor_dev);
        return nor_dev;
    }

    return static_ptbl_init(nor_dev, nor_table);
}

void board_console_init(struct imxrt_platform *plat_)
{
    // Ungate lpuart1 clock
    mmio_clrsetbits_32(IMXRT_CCM_CCGR5, 0, CCGR5_LPUART1);

    // PAD mux: lpuart1 RX = alt2
    mmio_write_32(SW_MUX_CTL_PAD_GPIO_AD_B0_13, IMXRT_IOMUX_ALT(2));
    // PAD mux: lpuart1 TX = alt2
    mmio_write_32(SW_MUX_CTL_PAD_GPIO_AD_B0_12, IMXRT_IOMUX_ALT(2));

    imx_lpuart_init(IMXRT_LPUART1_BASE, MHz(20), 115200);

    static const struct console_ops ops = {
        .putc = imx_lpuart_putc,
    };

    console_init(IMXRT_LPUART1_BASE, &ops);
}

int board_init(struct imxrt_platform *plat_)
{
    int rc;

    plat = plat_;

    rc = flexspi_nor_setup();

    if (rc != PB_OK) {
        LOG_ERR("Flexspi NOR init failed (%i)", rc);
        return rc;
    }

    static const struct rot_config rot_config = {
        .revoke_key = NULL,
        .read_key_status = imxrt_read_key_status,
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
        .read_status = imxrt_slc_read_status,
        .set_configuration = NULL,
        .set_configuration_locked = NULL,
        .set_eol = NULL,
    };

    rc = slc_init(&slc_config);

    if (rc != PB_OK) {
        LOG_ERR("SLC init failed (%i)", rc);
        return rc;
    }

err_out:
    return rc;
}

static int board_status(uint8_t *response_bfr, size_t *response_size)
{
    char *response = (char *)response_bfr;
    size_t resp_buf_size = *response_size;

    (*response_size) = snprintf(response, resp_buf_size, "Board status OK!\n");
    response[(*response_size)++] = 0;

    return PB_OK;
}

static int board_command(uint32_t command,
                         uint8_t *bfr,
                         size_t size,
                         uint8_t *response_bfr,
                         size_t *response_size)
{
    int rc;
    LOG_DBG("%x, %p, %zu", command, bfr, size);
    *response_size = 0;

    if (command == 0xf93ba110) { /* test */
        printf("Arg: ");
        for (unsigned int i = 0; i < size; i++)
            printf("%02x", bfr[i]);
        printf("\n\r");
        const uint8_t test_data[] = { 0x01, 0x02, 0x03, 0x04 };
        memcpy(response_bfr, test_data, sizeof(test_data));
        *response_size = sizeof(test_data);
    }

    return PB_OK;
}

int cm_board_init(void)
{
    int rc;
    /* Enable USB PLL */
    // TODO: Here we assume that a lot of other bit's are correctly set...
    mmio_clrsetbits_32(IMXRT_CCM_ANALOG_PLL_USB1, 0, ANALOG_PLL_USB_EN_USB_CLKS);

    mmio_setbits_32(IMXRT_CCM_CCGR6, CCGR6_USBOH3);

    imx_usb2_phy_init(IMXRT_USBPHY1_BASE);

    rc = imx_ci_udc_init(IMXRT_USB_BASE);

    if (rc != PB_OK) {
        LOG_ERR("imx_ci_udc_init failed (%i)", rc);
        return rc;
    }

    rc = pb_dev_cls_init();

    if (rc != PB_OK)
        return rc;

    static const struct cm_config cfg = {
        .name = "imxrt1060evk",
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
