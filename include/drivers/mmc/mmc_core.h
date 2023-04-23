/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_MMC_H
#define INCLUDE_PB_MMC_H

#include <pb/utils_def.h>
#include <uuid.h>

/* MMC Commands */
#define MMC_CMD_GO_IDLE_STATE           0
#define MMC_CMD_SEND_OP_COND            1
#define MMC_CMD_ALL_SEND_CID            2
#define MMC_CMD_SET_RELATIVE_ADDR       3
#define MMC_CMD_SET_DSR                 4
#define MMC_CMD_SWITCH                  6
#define MMC_CMD_SELECT_CARD             7
#define MMC_CMD_SEND_EXT_CSD            8
#define MMC_CMD_SEND_CSD                9
#define MMC_CMD_SEND_CID                10
#define MMC_CMD_STOP_TRANSMISSION       12
#define MMC_CMD_SEND_STATUS             13
#define MMC_CMD_SET_BLOCKLEN            16
#define MMC_CMD_READ_SINGLE_BLOCK       17
#define MMC_CMD_READ_MULTIPLE_BLOCK     18
#define MMC_CMD_SEND_TUNING_BLOCK_HS200 21
#define MMC_CMD_SET_BLOCK_COUNT         23
#define MMC_CMD_WRITE_SINGLE_BLOCK      24
#define MMC_CMD_WRITE_MULTIPLE_BLOCK    25
#define MMC_CMD_ERASE_GROUP_START       35
#define MMC_CMD_ERASE_GROUP_END         36
#define MMC_CMD_ERASE                   38
#define MMC_CMD_APP_CMD                 55
#define MMC_CMD_SPI_READ_OCR            58
#define MMC_CMD_SPI_CRC_ON_OFF          59
#define SD_CMD_SEND_OP_COND                 41
#define SD_CMD_SEND_SCR                 51
#define SD_CMD_SET_BUS_WIDTH            6
/*
 * EXT_CSD fields
 */

#define EXT_CSD_CMDQ_MODE_EN        15    /* R/W */
#define EXT_CSD_FLUSH_CACHE        32      /* W */
#define EXT_CSD_CACHE_CTRL        33      /* R/W */
#define EXT_CSD_POWER_OFF_NOTIFICATION    34    /* R/W */
#define EXT_CSD_PACKED_FAILURE_INDEX    35    /* RO */
#define EXT_CSD_PACKED_CMD_STATUS    36    /* RO */
#define EXT_CSD_EXP_EVENTS_STATUS    54    /* RO, 2 bytes */
#define EXT_CSD_EXP_EVENTS_CTRL        56    /* R/W, 2 bytes */
#define EXT_CSD_DATA_SECTOR_SIZE    61    /* R */
#define EXT_CSD_ENH_SIZE_MULT       140   /* R/W, 3 bytes */
#define EXT_CSD_GP_SIZE_MULT        143    /* R/W */
#define EXT_CSD_PARTITION_SETTING_COMPLETED 155    /* R/W */
#define EXT_CSD_PARTITION_ATTRIBUTE    156    /* R/W */
#define EXT_CSD_MAX_ENH_SIZE_MULT    157    /* RO, 3 bytes */
#define EXT_CSD_PARTITION_SUPPORT    160    /* RO */
#define EXT_CSD_HPI_MGMT        161    /* R/W */
#define EXT_CSD_RST_N_FUNCTION        162    /* R/W */
#define EXT_CSD_BKOPS_EN        163    /* R/W */
#define EXT_CSD_BKOPS_START        164    /* W */
#define EXT_CSD_SANITIZE_START        165     /* W */
#define EXT_CSD_WR_REL_PARAM        166    /* RO */
#define EXT_CSD_RPMB_MULT        168    /* RO */
#define EXT_CSD_FW_CONFIG        169    /* R/W */
#define EXT_CSD_BOOT_WP            173    /* R/W */
#define EXT_CSD_ERASE_GROUP_DEF        175    /* R/W */
#define EXT_CSD_BOOT_BUS_CONDITIONS 177
#define EXT_CSD_PART_CONFIG        179    /* R/W */
#define EXT_CSD_ERASED_MEM_CONT        181    /* RO */
#define EXT_CSD_BUS_WIDTH        183    /* R/W */
#define EXT_CSD_STROBE_SUPPORT        184    /* RO */
#define EXT_CSD_HS_TIMING        185    /* R/W */
#define EXT_CSD_POWER_CLASS        187    /* R/W */
#define EXT_CSD_REV            192    /* RO */
#define EXT_CSD_STRUCTURE        194    /* RO */
#define EXT_CSD_CARD_TYPE        196    /* RO */
#define EXT_CSD_DRIVER_STRENGTH        197    /* RO */
#define EXT_CSD_OUT_OF_INTERRUPT_TIME    198    /* RO */
#define EXT_CSD_PART_SWITCH_TIME        199     /* RO */
#define EXT_CSD_PWR_CL_52_195        200    /* RO */
#define EXT_CSD_PWR_CL_26_195        201    /* RO */
#define EXT_CSD_PWR_CL_52_360        202    /* RO */
#define EXT_CSD_PWR_CL_26_360        203    /* RO */
#define EXT_CSD_SEC_CNT            212    /* RO, 4 bytes */
#define EXT_CSD_S_A_TIMEOUT        217    /* RO */
#define EXT_CSD_REL_WR_SEC_C        222    /* RO */
#define EXT_CSD_HC_WP_GRP_SIZE        221    /* RO */
#define EXT_CSD_ERASE_TIMEOUT_MULT    223    /* RO */
#define EXT_CSD_HC_ERASE_GRP_SIZE    224    /* RO */
#define EXT_CSD_BOOT_MULT        226    /* RO */
#define EXT_CSD_SEC_TRIM_MULT        229    /* RO */
#define EXT_CSD_SEC_ERASE_MULT        230    /* RO */
#define EXT_CSD_SEC_FEATURE_SUPPORT    231    /* RO */
#define EXT_CSD_TRIM_MULT        232    /* RO */
#define EXT_CSD_PWR_CL_200_195        236    /* RO */
#define EXT_CSD_PWR_CL_200_360        237    /* RO */
#define EXT_CSD_PWR_CL_DDR_52_195    238    /* RO */
#define EXT_CSD_PWR_CL_DDR_52_360    239    /* RO */
#define EXT_CSD_BKOPS_STATUS        246    /* RO */
#define EXT_CSD_POWER_OFF_LONG_TIME    247    /* RO */
#define EXT_CSD_GENERIC_CMD6_TIME    248    /* RO */
#define EXT_CSD_CACHE_SIZE        249    /* RO, 4 bytes */
#define EXT_CSD_PWR_CL_DDR_200_360    253    /* RO */
#define EXT_CSD_FIRMWARE_VERSION    254    /* RO, 8 bytes */
#define EXT_CSD_PRE_EOL_INFO        267    /* RO */
#define EXT_CSD_DEVICE_LIFE_TIME_EST_TYP_A    268    /* RO */
#define EXT_CSD_DEVICE_LIFE_TIME_EST_TYP_B    269    /* RO */
#define EXT_CSD_CMDQ_DEPTH        307    /* RO */
#define EXT_CSD_CMDQ_SUPPORT        308    /* RO */
#define EXT_CSD_SUPPORTED_MODE        493    /* RO */
#define EXT_CSD_TAG_UNIT_SIZE        498    /* RO */
#define EXT_CSD_DATA_TAG_SUPPORT    499    /* RO */
#define EXT_CSD_MAX_PACKED_WRITES    500    /* RO */
#define EXT_CSD_MAX_PACKED_READS    501    /* RO */
#define EXT_CSD_BKOPS_SUPPORT        502    /* RO */
#define EXT_CSD_HPI_FEATURES        503    /* RO */

/*
 * EXT_CSD field definitions
 */

#define EXT_CSD_BUS_WIDTH_1    0    /* Card is in 1 bit mode */
#define EXT_CSD_BUS_WIDTH_4    1    /* Card is in 4 bit mode */
#define EXT_CSD_BUS_WIDTH_8    2    /* Card is in 8 bit mode */
#define EXT_CSD_BUS_WIDTH_4_DDR    5    /* Card is in 4 bit DDR mode */
#define EXT_CSD_BUS_WIDTH_8_DDR    6    /* Card is in 8 bit DDR mode */
#define EXT_CSD_BUS_WIDTH_STROBE BIT(7)    /* Enhanced strobe mode */

#define EXT_CSD_TIMING_BC    0    /* Backwards compatility */
#define EXT_CSD_TIMING_HS    1    /* High speed */
#define EXT_CSD_TIMING_HS200    2    /* HS200 */
#define EXT_CSD_TIMING_HS400    3    /* HS400 */
#define EXT_CSD_DRV_STR_SHIFT    4    /* Driver Strength shift */

/* Responses */
#define MMC_RSP_PRESENT (1 << 0)
#define MMC_RSP_136    (1 << 1)        /* 136 bit response */
#define MMC_RSP_CRC    (1 << 2)        /* expect valid crc */
#define MMC_RSP_BUSY    (1 << 3)        /* card may send busy */
#define MMC_RSP_OPCODE    (1 << 4)        /* response contains opcode */

#define MMC_RSP_NONE    (0)
#define MMC_RSP_R1    (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R1b    (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE| \
            MMC_RSP_BUSY)
#define MMC_RSP_R2    (MMC_RSP_PRESENT|MMC_RSP_136|MMC_RSP_CRC)
#define MMC_RSP_R3    (MMC_RSP_PRESENT)
#define MMC_RSP_R4    (MMC_RSP_PRESENT)
#define MMC_RSP_R5    (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R6    (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R7    (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)

/* Value randomly chosen for eMMC RCA, it should be > 1 */
#define MMC_FIX_RCA            10
#define RCA_SHIFT_OFFSET        16

/*
 * Flags
 */


#define OCR_POWERUP            BIT(31)
#define OCR_HCS                BIT(30)
#define OCR_BYTE_MODE          (0 << 29)
#define OCR_SECTOR_MODE        (2 << 29)
#define OCR_ACCESS_MODE_MASK   (3 << 29)
#define OCR_3_5_3_6            BIT(23)
#define OCR_3_4_3_5            BIT(22)
#define OCR_3_3_3_4            BIT(21)
#define OCR_3_2_3_3            BIT(20)
#define OCR_3_1_3_2            BIT(19)
#define OCR_3_0_3_1            BIT(18)
#define OCR_2_9_3_0            BIT(17)
#define OCR_2_8_2_9            BIT(16)
#define OCR_2_7_2_8            BIT(15)
#define OCR_VDD_MIN_2V7        GENMASK(23, 15)
#define OCR_VDD_MIN_2V0        GENMASK(14, 8)
#define OCR_VDD_MIN_1V7        BIT(7)

#define MMC_GET_STATE(x)        (((x) >> 9) & 0xf)
#define MMC_STATE_IDLE            0
#define MMC_STATE_READY            1
#define MMC_STATE_IDENT            2
#define MMC_STATE_STBY            3
#define MMC_STATE_TRAN            4
#define MMC_STATE_DATA            5
#define MMC_STATE_RCV            6
#define MMC_STATE_PRG            7
#define MMC_STATE_DIS            8
#define MMC_STATE_BTST            9
#define MMC_STATE_SLP            10

#define STATUS_READY_FOR_DATA        BIT(8)
#define STATUS_SWITCH_ERROR        BIT(7)

#define EXTCSD_SET_CMD            (U(0) << 24)
#define EXTCSD_SET_BITS            (U(1) << 24)
#define EXTCSD_CLR_BITS            (U(2) << 24)
#define EXTCSD_WRITE_BYTES        (U(3) << 24)
#define EXTCSD_CMD(x)            (((x) & 0xff) << 16)
#define EXTCSD_VALUE(x)            (((x) & 0xff) << 8)
#define EXTCSD_CMD_SET_NORMAL        U(1)

#define MMC_BLOCK_SIZE            U(512)
#define MMC_BLOCK_MASK            (MMC_BLOCK_SIZE - U(1))
#define MMC_BOOT_CLK_RATE        (400 * 1000)

/* EXT_CSD_BOOT_BUS_CONDITIONS */
#define EXT_CSD_BOOT_SDR_HS      BIT(3)
#define EXT_CSD_BOOT_DDR         BIT(4)
#define EXT_CSD_BOOT_BUS_WIDTH_4 BIT(0)
#define EXT_CSD_BOOT_BUS_WIDTH_8 BIT(1)

enum mmc_part {
    MMC_PART_USER  = 0,
    MMC_PART_BOOT0 = 1,
    MMC_PART_BOOT1 = 2,
    MMC_PART_RPMB  = 3,
    MMC_PART_END,
};

enum mmc_bus_mode
{
    MMC_BUS_MODE_INVALID = 0,
    MMC_BUS_MODE_DDR52,
    MMC_BUS_MODE_HS200,
    MMC_BUS_MODE_HS400,
    MMC_BUS_MODE_END,
};

enum mmc_bus_width
{
    MMC_BUS_WIDTH_INVALID = 0,
    MMC_BUS_WIDTH_1BIT,
    MMC_BUS_WIDTH_4BIT,
    MMC_BUS_WIDTH_8BIT,
    MMC_BUS_WIDTH_4BIT_DDR,
    MMC_BUS_WIDTH_8BIT_DDR,
    MMC_BUS_WIDTH_8BIT_DDR_STROBE,
};

enum mmc_card_type
{
    MMC_CARD_TYPE_INVALID = 0,
    MMC_CARD_TYPE_EMMC,
    MMC_CARD_TYPE_SD,
    MMC_CARD_TYPE_SD_HC,
    MMC_CARD_TYPE_END,
};

struct mmc_csd_emmc {
    unsigned int        not_used:               1;
    unsigned int        crc:                    7;
    unsigned int        ecc:                    2;
    unsigned int        file_format:            2;
    unsigned int        tmp_write_protect:      1;
    unsigned int        perm_write_protect:     1;
    unsigned int        copy:                   1;
    unsigned int        file_format_grp:        1;

    unsigned int        reserved_1:             5;
    unsigned int        write_bl_partial:       1;
    unsigned int        write_bl_len:           4;
    unsigned int        r2w_factor:             3;
    unsigned int        default_ecc:            2;
    unsigned int        wp_grp_enable:          1;

    unsigned int        wp_grp_size:            5;
    unsigned int        erase_grp_mult:         5;
    unsigned int        erase_grp_size:         5;
    unsigned int        c_size_mult:            3;
    unsigned int        vdd_w_curr_max:         3;
    unsigned int        vdd_w_curr_min:         3;
    unsigned int        vdd_r_curr_max:         3;
    unsigned int        vdd_r_curr_min:         3;
    unsigned int        c_size_low:             2;

    unsigned int        c_size_high:            10;
    unsigned int        reserved_2:             2;
    unsigned int        dsr_imp:                1;
    unsigned int        read_blk_misalign:      1;
    unsigned int        write_blk_misalign:     1;
    unsigned int        read_bl_partial:        1;
    unsigned int        read_bl_len:            4;
    unsigned int        ccc:                    12;

    unsigned int        tran_speed:             8;
    unsigned int        nsac:                   8;
    unsigned int        taac:                   8;
    unsigned int        reserved_3:             2;
    unsigned int        spec_vers:              4;
    unsigned int        csd_structure:          2;
};

struct mmc_cmd {
    uint32_t arg;           /*!< MMC command argument */
    uint16_t resp_type;     /*!< Type of response to expect */
    uint16_t idx;           /*!< Index of command to execute */
};

typedef uint32_t mmc_cmd_resp_t[4];

typedef int (*mmc_init_t)(void);
typedef int (*mmc_io_t)(unsigned int lba, size_t length, uintptr_t buf);

typedef int (*mmc_set_bus_clock_t)(unsigned int clk_hz);
typedef int (*mmc_set_bus_width_t)(enum mmc_bus_width width);

typedef int (*mmc_send_cmd_t)(const struct mmc_cmd *cmd,
                              mmc_cmd_resp_t result);
struct mmc_hal {
    mmc_init_t init;                /*!< Initilize HAL */
    mmc_send_cmd_t send_cmd;        /*!< Send MMC command */
    mmc_set_bus_clock_t set_bus_clock; /*!< Set bus clock rate */
    mmc_set_bus_width_t set_bus_width; /*!< Set bus with */
    mmc_io_t prepare;  /*!< Prepare DMA and start xfer, this is optional */
    mmc_io_t read;     /*!< Perform read op */
    mmc_io_t write;    /*!< Perform write op */
    int (*set_delay_tap)(unsigned int tap); /*!< Select bus delay tap */
    size_t max_chunk_bytes; /*!< Maximum bytes per iop */
};

struct mmc_device_info {
    size_t block_size;                /* !< Block size in bytes */
    unsigned int ocr_voltage;         /* !< OCR voltage */
    enum mmc_bus_mode mode;           /* !< MMC bus mode */
    enum mmc_bus_width width;         /* !< MMC bus width */
};

struct mmc_device_config {
    enum mmc_bus_mode mode;         /*!< Requested bus mode */
    enum mmc_bus_width width;       /*!< Requested bus width */
    const unsigned char *boot0_uu;  /*!< UUID to assign to boot 0 partition */
    const unsigned char *boot1_uu;  /*!< UUID to assign to boot 1 partition */
    const unsigned char *rpmb_uu;   /*!< UUID to assign to RPMB partition */
    const unsigned char *user_uu;   /*!< UUID to assign to user partition */
    uint32_t flags;
    uint8_t boot_mode;              /*!< Optional, used to update the boot
                                      mode in extcsd, for example, for fast boot */
};

/**
 * Initialize the mmc module
 *
 * @param[in] hal Pointer to ops structure of hardware layer
 * @param[in] cfg Pointer to MMC config struct
 *
 * @return PB_OK on success
 *        -PB_ERR_PARAM, on invalid bus mode or NULL parameters
 *        -PB_ERR_NOT_IMPLEMENTED, on invalid bus mode
 *        -PB_ERR_IO, on read errors
 */
int mmc_init(const struct mmc_hal *hal, const struct mmc_device_config *cfg);

/**
 * Get the card device info struct for the currently enumerated card.
 *
 * @return Pointer to device info struct or NULL on error
 */
struct mmc_device_info * mmc_device_info(void);

/**
 * Read bytes from current partition.
 *
 * @param[in] lba Start block to read from
 * @param[in] length Length in bytes
 * @param[out] buf Output buffer
 *
 * @return PB_OK on success
 *        -PB_ERR_IO, on read request to large
 */
int mmc_read(unsigned int lba, size_t length, uintptr_t buf);

/**
 * Write bytes to current partition.
 *
 * @param[in] lba Start block to write to
 * @param[in] length Length in bytes
 * @param[in] buf Buffer to write
 *
 * @return PB_OK on success
 *        -PB_ERR_IO, on write request to large
 */
int mmc_write(unsigned int lba, size_t length, const uintptr_t buf);

/**
 * Switch hardware partition
 *
 * @param[in] part Hardware partition to switch to
 *
 * @return PB_OK on success
 */
int mmc_part_switch(enum mmc_part part);

#endif
