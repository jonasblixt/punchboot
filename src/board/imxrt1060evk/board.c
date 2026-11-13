#include <boot/ab_state.h>
#include <boot/boot.h>
#include <boot/fh.h>
#include <drivers/fuse/imx_hab_fuses.h>
#include <drivers/memc/imx_flexspi.h>
#include <drivers/partition/gpt.h>
#include <drivers/partition/static.h>
#include <drivers/pinctrl/imx_iomuxc.h>
#include <drivers/uart/imx_lpuart.h>
#include <pb/board.h>
#include <pb/cm.h>
#include <pb/command.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <plat/imxrt/imxrt.h>
#include <string.h>

#include "partitions.h"

#define UUID_1bd35a54_485d_4fb6_a822_cb58c3c5a068 \
    (const unsigned char *)"\x1b\xd3\x5a\x54\x48\x5d\x4f\xb6\xa8\x22\xcb\x58\xc3\xc5\xa0\x68"

/*
 * The QSPI NOR (8 MiB): pb from offset 0, the boot state, System A and B.
 * The system images run from RAM, pb copies them there (the test bench's
 * MCU application, testbench/simple).
 */
static const struct static_part_table nor_table[] = {
    {
        .uu = PART_NOR_PBL,
        .description = "BL",
        .size = SZ_KiB(256),
    },
    {
        .uu = PART_primary_state,
        .description = "PB State Primary",
        .size = SZ_KiB(4),
    },
    {
        .uu = PART_backup_state,
        .description = "PB State Backup",
        .size = SZ_KiB(4),
    },
    {
        .uu = PART_sys_a,
        .description = "System A",
        .first_lba = SZ_KiB(512) / SZ_KiB(4),
        .size = SZ_MiB(1),
    },
    {
        .uu = PART_sys_b,
        .description = "System B",
        .size = SZ_MiB(1),
    },
    {
        .uu = NULL,
    },
};

/*
 * A charger on the USB port stops the boot: pb stays in command mode. The
 * detector's result is read here, after the image is loaded and verified,
 * since the detection takes more than 100 ms from reset. Nothing in pb starts
 * the detector: the hook relies on it running from reset.
 */
static int board_late_boot(uuid_t boot_part_uu)
{
    (void)boot_part_uu;

    /* If the command interface initiated the boot, skip charger detection,
     * since we're connected but want to boot anyway */
    if (boot_get_flags() & BOOT_FLAG_CMD)
        return PB_OK;

    /* Perform CDP detection */
    uint32_t charger_status = mmio_read_32(IMXRT_USB1_CHRG_DETECT_STAT);
    if (charger_status & IMXRT_CHRG_DETECT_STAT_CHRG_DETECTED) {
        printf("Charger detected, entering command mode\n\r");
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

#define FLEXSPI_PAD_CONFIG (IMXRT_IOMUX_SRE | IMXRT_IOMUX_SPEED(3) | IMXRT_IOMUX_DSE(6))
    static const struct imx_pin pins[] = {
        IMXRT_PIN(GPIO_SD_B1_08, IMXRT_IOMUX_ALT(1), FLEXSPI_PAD_CONFIG), /* DATA00 */
        IMXRT_PIN(GPIO_SD_B1_09, IMXRT_IOMUX_ALT(1), FLEXSPI_PAD_CONFIG), /* DATA01 */
        IMXRT_PIN(GPIO_SD_B1_10, IMXRT_IOMUX_ALT(1), FLEXSPI_PAD_CONFIG), /* DATA02 */
        IMXRT_PIN(GPIO_SD_B1_11, IMXRT_IOMUX_ALT(1), FLEXSPI_PAD_CONFIG), /* DATA03 */
        IMXRT_PIN(GPIO_SD_B1_07, IMXRT_IOMUX_ALT(1), FLEXSPI_PAD_CONFIG), /* SCLK */
        IMXRT_PIN(GPIO_SD_B1_06, IMXRT_IOMUX_ALT(1), FLEXSPI_PAD_CONFIG), /* SS0_B */
        IMXRT_PIN(GPIO_SD_B1_05,
                  IMXRT_IOMUX_SION | IMXRT_IOMUX_ALT(1),
                  FLEXSPI_PAD_CONFIG | IMXRT_IOMUX_HYS), /* DQS */
    };

    imx_iomuxc_setup(pins, ARRAY_SIZE(pins));

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

    return PB_OK;
}

/*
 * Command mode on request: an image writes PB_CM_REQUEST to SRC_GPR5 and
 * resets the core (SYSRESETREQ). SRC_GPR5 keeps its value over the warm
 * reset, it is the one SRC general purpose register that the ROM leaves to
 * software (RM 21.6.9). The test bench's MCU application uses it to give
 * the board back to the host.
 */
#define PB_CM_REQUEST 0x5042434d /* "PBCM" */

static int board_early_boot(void)
{
    if (mmio_read_32(IMXRT_SRC_GPR5) == PB_CM_REQUEST) {
        mmio_write_32(IMXRT_SRC_GPR5, 0);

        /* A boot from command mode goes on */
        if (!(boot_get_flags() & BOOT_FLAG_CMD)) {
            printf("Command mode requested\n\r");
            return -PB_ERR_ABORT;
        }
    }

    return PB_OK;
}

static void board_console_init(void)
{
    // Ungate lpuart1 clock
    mmio_clrsetbits_32(IMXRT_CCM_CCGR5, 0, CCGR5_LPUART1);

    static const struct imx_pin pins[] = {
        IMXRT_PIN(GPIO_AD_B0_13, IMXRT_IOMUX_ALT(2), IMX_PAD_KEEP), /* LPUART1 RX */
        IMXRT_PIN(GPIO_AD_B0_12, IMXRT_IOMUX_ALT(2), IMX_PAD_KEEP), /* LPUART1 TX */
    };

    imx_iomuxc_setup(pins, ARRAY_SIZE(pins));

    imx_lpuart_init(IMXRT_LPUART1_BASE, MHz(20), 115200);

    static const struct console_ops ops = {
        .putc = imx_lpuart_putc,
    };

    console_init(IMXRT_LPUART1_BASE, &ops);
}

/* The board's named command 'test' (command-mode.md 5.6), pb_id() 0xf93ba110,
 * the id it had before 3.0.0 */
static int
cmd_test(const uint8_t *request, size_t request_size, uint8_t *response, size_t *response_size)
{
    static const uint8_t test_data[] = { 0x01, 0x02, 0x03, 0x04 };
    size_t room = *response_size;

    printf("Arg: ");
    for (size_t i = 0; i < request_size; i++)
        printf("%02x", request[i]);
    printf("\n\r");

    /* As much of it as there is room for */
    *response_size = (room < sizeof(test_data)) ? room : sizeof(test_data);
    memcpy(response, test_data, *response_size);

    return PB_OK;
}

DEFINE_COMMAND("test", "Prints its request, answers with four bytes", cmd_test);

static const struct rot_config rot = {
    .read_key_status = imx_hab_read_key_status,
};

static const struct slc_config slc = {
    .read_status = imx_hab_slc_read_status,
};

/*
 * The image runs from RAM, from the load address of its part "app", and gets
 * pb's information in a transfer list (boot.md 5.4): pb's structure, the
 * board names no device tree part. The list's region is in OCRAM2 right
 * above the ROM's data: CONFIG_BOOT_FH_REGION_BASE and _SIZE, 4 KiB at
 * 0x20208000 in the defconfig. An image keeps it free until it has read the
 * list.
 */
static const struct boot_handover_fh_config boot_fh = {
    .entry_bpak_id = 0x362fa9dd, /* bpak_id("app") */
};

static const struct boot_ab_state_config boot_state_cfg = {
    .primary_state_part_uu = PART_primary_state,
    .backup_state_part_uu = PART_backup_state,
    .sys_a_uu = PART_sys_a,
    .sys_b_uu = PART_sys_b,
};

/*
 * Where an image's parts may be loaded (board.md 2.9): the RAM an application
 * is linked for. OCRAM2 between the transfer list's region, which follows the
 * ROM's data, and pb, which is linked from 0x20249000 to the end of the OCRAM
 * (link.lds), and the two TCMs, which pb doesn't use. The SDRAM isn't set up.
 * board_setup() checks the list against what is linked and against the
 * transfer list's region.
 *
 * The transfer list's region has one source, the defconfig (platform.md 5.1):
 * the OCRAM2 region starts where it ends. A defconfig that has it somewhere
 * else than after the ROM's data, or leaves nothing below pb, doesn't build.
 */
#define BOARD_OCRAM2_IMAGE_BASE (CONFIG_BOOT_FH_REGION_BASE + CONFIG_BOOT_FH_REGION_SIZE)
#define BOARD_OCRAM2_IMAGE_END  0x20249000

_Static_assert(CONFIG_BOOT_FH_REGION_BASE == IMXRT_OCRAM2_BASE + IMXRT_OCRAM2_ROM_SIZE,
               "CONFIG_BOOT_FH_REGION_BASE is not the end of the ROM data in OCRAM2");
_Static_assert(BOARD_OCRAM2_IMAGE_BASE < BOARD_OCRAM2_IMAGE_END,
               "CONFIG_BOOT_FH_REGION_SIZE leaves no OCRAM2 below pb for an image");

static const struct boot_image_region image_regions[] = {
    BOOT_IMAGE_REGION(BOARD_OCRAM2_IMAGE_BASE, BOARD_OCRAM2_IMAGE_END - BOARD_OCRAM2_IMAGE_BASE),
    BOOT_IMAGE_REGION(IMXRT_ITCM_BASE, IMXRT_ITCM_SIZE),
    BOOT_IMAGE_REGION(IMXRT_DTCM_BASE, IMXRT_DTCM_SIZE),
    BOOT_IMAGE_REGION_END,
};

const struct board board = {
    .name = "imxrt1060evk",
    .console_init = board_console_init,
    .rot = &rot,
    .slc = &slc,
    .init = flexspi_nor_setup,
    .ptbl = PTBL_STATIC(UUID_1bd35a54_485d_4fb6_a822_cb58c3c5a068, nor_table),
    .boot_policy = BOOT_POLICY_AB(&boot_state_cfg),
    .boot_handover = BOOT_HANDOVER_FH(&boot_fh),
    .image_regions = image_regions,
    .early_boot = board_early_boot,
    .late_boot = board_late_boot,
};
