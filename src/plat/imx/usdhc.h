/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX_USDHC_H_
#define PLAT_IMX_USDHC_H_

#include <pb.h>
#include <io.h>

#define USDHC_MAKE_CMD(c, f)        (((c & 0xff) << 24) | ((f & 0xff) << 16))


/* USDHC controller register */
#define USDHC_DS_ADDR              0x0000
#define USDHC_BLK_ATT              0x0004
#define USDHC_CMD_ARG              0x0008
#define USDHC_CMD_XFR_TYP          0x000C
#define USDHC_CMD_RSP0             0x0010
#define USDHC_CMD_RSP1             0x0014
#define USDHC_CMD_RSP2             0x0018
#define USDHC_CMD_RSP3             0x001C
#define USDHC_DATA_BUFF_ACC_PORT   0x0020
#define USDHC_PRES_STATE           0x0024
#define USDHC_PROT_CTRL            0x0028
#define USDHC_SYS_CTRL             0x002C
#define USDHC_INT_STATUS           0x0030
#define USDHC_INT_STATUS_EN        0x0034
#define USDHC_INT_SIGNAL_EN        0x0038
#define USDHC_AUTOCMD12_ERR_STATUS 0x003C
#define USDHC_HOST_CTRL_CAP        0x0040
#define USDHC_WTMK_LVL             0x0044
#define USDHC_MIX_CTRL             0x0048
#define USDHC_FORCE_EVENT          0x0050
#define USDHC_ADMA_ERR_STATUS      0x0054
#define USDHC_ADMA_SYS_ADDR        0x0058
#define USDHC_DLL_CTRL             0x0060
#define USDHC_DLL_STATUS           0x0064
#define USDHC_CLK_TUNE_CTRL_STATUS 0x0068
#define USDHC_VEND_SPEC            0x00C0
#define USDHC_MMC_BOOT             0x00C4
#define USDHC_VEND_SPEC2           0x00C8
#define USDHC_TUNING_CTRL          0x00CC


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
#define MMC_CMD_SET_BLOCK_COUNT         23
#define MMC_CMD_WRITE_SINGLE_BLOCK      24
#define MMC_CMD_WRITE_MULTIPLE_BLOCK    25
#define MMC_CMD_ERASE_GROUP_START       35
#define MMC_CMD_ERASE_GROUP_END         36
#define MMC_CMD_ERASE                   38
#define MMC_CMD_APP_CMD                 55
#define MMC_CMD_SPI_READ_OCR            58
#define MMC_CMD_SPI_CRC_ON_OFF          59
#define MMC_CMD_SEND_TUNING_BLOCK_HS200    21

/* MIX CTRL register */

#define USDHC_MIX_CTRL_FBCLK_SEL    (1 << 25)
#define USDHC_MIX_CTRL_AUTO_TUNE_EN (1 << 24)
#define USDHC_MIX_CTRL_SMPCLK_SEL   (1 << 23)
#define USDHC_MIX_CTRL_EXE_TUNE     (1 << 22)

#define USDHC_SMPCLK_SEL   (1 << 23)
#define USDHC_EXE_TUNE     (1 << 22)

#define MMC_SWITCH_MODE_WRITE_BYTE    0x03 /* Set target byte to value */





#define MMC_STATUS_MASK            (~0x0206BF7F)
#define MMC_STATUS_RDY_FOR_DATA     (1 << 8)
#define MMC_STATUS_CURR_STATE        (0xf << 9)
#define MMC_STATUS_ERROR        (1 << 19)

#define MMC_STATE_PRG            (7 << 9)



#define VENDORSPEC_CKEN        0x00004000
#define VENDORSPEC_PEREN    0x00002000
#define VENDORSPEC_HCKEN    0x00001000
#define VENDORSPEC_IPGEN    0x00000800
#define VENDORSPEC_INIT        0x20007809


#define PROCTL_INIT        0x00000020
#define PROCTL_DTW_4        0x00000002
#define PROCTL_DTW_8        0x00000004


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
#define EXT_CSD_GP_SIZE_MULT        143    /* R/W */
#define EXT_CSD_PARTITION_SETTING_COMPLETED 155    /* R/W */
#define EXT_CSD_PARTITION_ATTRIBUTE    156    /* R/W */
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

#define EXT_CSD_WR_REL_PARAM_EN        (1<<2)

#define EXT_CSD_BOOT_WP_B_PWR_WP_DIS    (0x40)
#define EXT_CSD_BOOT_WP_B_PERM_WP_DIS    (0x10)
#define EXT_CSD_BOOT_WP_B_PERM_WP_EN    (0x04)
#define EXT_CSD_BOOT_WP_B_PWR_WP_EN    (0x01)

#define EXT_CSD_PART_CONFIG_ACC_MASK    (0x7)
#define EXT_CSD_PART_CONFIG_ACC_BOOT0    (0x1)
#define EXT_CSD_PART_CONFIG_ACC_RPMB    (0x3)
#define EXT_CSD_PART_CONFIG_ACC_GP0    (0x4)

#define EXT_CSD_PART_SETTING_COMPLETED    (0x1)
#define EXT_CSD_PART_SUPPORT_PART_EN    (0x1)

#define EXT_CSD_CMD_SET_NORMAL        (1<<0)
#define EXT_CSD_CMD_SET_SECURE        (1<<1)
#define EXT_CSD_CMD_SET_CPSECURE    (1<<2)

#define EXT_CSD_CARD_TYPE_HS_26    (1<<0)    /* Card can run at 26MHz */
#define EXT_CSD_CARD_TYPE_HS_52    (1<<1)    /* Card can run at 52MHz */
#define EXT_CSD_CARD_TYPE_HS    (EXT_CSD_CARD_TYPE_HS_26 | \
                 EXT_CSD_CARD_TYPE_HS_52)
#define EXT_CSD_CARD_TYPE_DDR_1_8V  (1<<2)   /* Card can run at 52MHz */
                         /* DDR mode @1.8V or 3V I/O */
#define EXT_CSD_CARD_TYPE_DDR_1_2V  (1<<3)   /* Card can run at 52MHz */
                         /* DDR mode @1.2V I/O */
#define EXT_CSD_CARD_TYPE_DDR_52       (EXT_CSD_CARD_TYPE_DDR_1_8V  \
                    | EXT_CSD_CARD_TYPE_DDR_1_2V)
#define EXT_CSD_CARD_TYPE_HS200_1_8V    (1<<4)    /* Card can run at 200MHz */
#define EXT_CSD_CARD_TYPE_HS200_1_2V    (1<<5)    /* Card can run at 200MHz */
                        /* SDR mode @1.2V I/O */
#define EXT_CSD_CARD_TYPE_HS200        (EXT_CSD_CARD_TYPE_HS200_1_8V | \
                     EXT_CSD_CARD_TYPE_HS200_1_2V)

/* Card can run at 200MHz DDR, 1.8V */
#define EXT_CSD_CARD_TYPE_HS400_1_8V    (1<<6)

/* Card can run at 200MHz DDR, 1.2V */
#define EXT_CSD_CARD_TYPE_HS400_1_2V    (1<<7)

#define EXT_CSD_CARD_TYPE_HS400        (EXT_CSD_CARD_TYPE_HS400_1_8V | \
                     EXT_CSD_CARD_TYPE_HS400_1_2V)
#define EXT_CSD_CARD_TYPE_HS400ES    (1<<8)    /* Card can run at HS400ES */

#define EXT_CSD_BUS_WIDTH_1    0    /* Card is in 1 bit mode */
#define EXT_CSD_BUS_WIDTH_4    1    /* Card is in 4 bit mode */
#define EXT_CSD_BUS_WIDTH_8    2    /* Card is in 8 bit mode */
#define EXT_CSD_DDR_BUS_WIDTH_4    5    /* Card is in 4 bit DDR mode */
#define EXT_CSD_DDR_BUS_WIDTH_8    6    /* Card is in 8 bit DDR mode */
#define EXT_CSD_BUS_WIDTH_STROBE BIT(7)    /* Enhanced strobe mode */

#define EXT_CSD_TIMING_BC    0    /* Backwards compatility */
#define EXT_CSD_TIMING_HS    1    /* High speed */
#define EXT_CSD_TIMING_HS200    2    /* HS200 */
#define EXT_CSD_TIMING_HS400    3    /* HS400 */
#define EXT_CSD_DRV_STR_SHIFT    4    /* Driver Strength shift */

#define EXT_CSD_SEC_ER_EN    BIT(0)
#define EXT_CSD_SEC_BD_BLK_EN    BIT(2)
#define EXT_CSD_SEC_GB_CL_EN    BIT(4)
#define EXT_CSD_SEC_SANITIZE    BIT(6)  /* v4.5 only */

#define EXT_CSD_RST_N_EN_MASK    0x3
#define EXT_CSD_RST_N_ENABLED    1    /* RST_n is enabled on card */

#define EXT_CSD_NO_POWER_NOTIFICATION    0
#define EXT_CSD_POWER_ON        1
#define EXT_CSD_POWER_OFF_SHORT        2
#define EXT_CSD_POWER_OFF_LONG        3

#define EXT_CSD_PWR_CL_8BIT_MASK    0xF0    /* 8 bit PWR CLS */
#define EXT_CSD_PWR_CL_4BIT_MASK    0x0F    /* 8 bit PWR CLS */
#define EXT_CSD_PWR_CL_8BIT_SHIFT    4
#define EXT_CSD_PWR_CL_4BIT_SHIFT    0

#define EXT_CSD_PACKED_EVENT_EN    BIT(3)

/*
 * EXCEPTION_EVENT_STATUS field
 */
#define EXT_CSD_URGENT_BKOPS        BIT(0)
#define EXT_CSD_DYNCAP_NEEDED        BIT(1)
#define EXT_CSD_SYSPOOL_EXHAUSTED    BIT(2)
#define EXT_CSD_PACKED_FAILURE        BIT(3)

#define EXT_CSD_PACKED_GENERIC_ERROR    BIT(0)
#define EXT_CSD_PACKED_INDEXED_ERROR    BIT(1)

/*
 * BKOPS status level
 */
#define EXT_CSD_BKOPS_LEVEL_2        0x2

/*
 * BKOPS modes
 */
#define EXT_CSD_MANUAL_BKOPS_MASK    0x01
#define EXT_CSD_AUTO_BKOPS_MASK        0x02

/*
 * Command Queue
 */
#define EXT_CSD_CMDQ_MODE_ENABLED    BIT(0)
#define EXT_CSD_CMDQ_DEPTH_MASK        GENMASK(4, 0)
#define EXT_CSD_CMDQ_SUPPORTED        BIT(0)






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



#define  USDHC_INT_RESPONSE      0x00000001
#define  USDHC_INT_DATA_END      0x00000002
#define  USDHC_INT_DMA_END       0x00000008
#define  USDHC_INT_SPACE_AVAIL   0x00000010
#define  USDHC_INT_DATA_AVAIL    0x00000020
#define  USDHC_INT_CARD_INSERT   0x00000040
#define  USDHC_INT_CARD_REMOVE   0x00000080
#define  USDHC_INT_CARD_INT      0x00000100
#define  USDHC_INT_ERROR         0x00008000
#define  USDHC_INT_TIMEOUT       0x00010000
#define  USDHC_INT_CRC           0x00020000
#define  USDHC_INT_END_BIT       0x00040000
#define  USDHC_INT_INDEX         0x00080000
#define  USDHC_INT_DATA_TIMEOUT  0x00100000
#define  USDHC_INT_DATA_CRC      0x00200000
#define  USDHC_INT_DATA_END_BIT  0x00400000
#define  USDHC_INT_BUS_POWER     0x00800000
#define  USDHC_INT_ACMD12ERR     0x01000000
#define  USDHC_INT_DMA_ERROR     0x10000000



struct usdhc_adma2_desc {
    uint16_t    cmd;
    uint16_t    len;
    uint32_t    addr;
}  __attribute__((packed));

#define ADMA2_TRAN_VALID    0x21
#define ADMA2_NOP_END_VALID    0x3
#define ADMA2_END        0x2

#ifndef EMMC_ADMA2_QUEUE_LENGTH
    #define EMMC_ADMA2_QUEUE_LENGTH 512
#endif


struct mmc_cid {
    unsigned int        manfid;
    char            prod_name[8];
    unsigned char        prv;
    unsigned int        serial;
    unsigned short        oemid;
    unsigned short        year;
    unsigned char        hwrev;
    unsigned char        fwrev;
    unsigned char        month;
};

struct mmc_csd {
    unsigned char        structure;
    unsigned char        mmca_vsn;
    unsigned short        cmdclass;
    unsigned short        taac_clks;
    unsigned int        taac_ns;
    unsigned int        c_size;
    unsigned int        r2w_factor;
    unsigned int        max_dtr;
    unsigned int        erase_size;        /* In sectors */
    unsigned int        read_blkbits;
    unsigned int        write_blkbits;
    unsigned int        capacity;
    unsigned int        read_partial:1,
                read_misalign:1,
                write_partial:1,
                write_misalign:1,
                dsr_imp:1;
};

struct mmc_ext_csd {
    uint8_t            rev;
    uint8_t            erase_group_def;
    uint8_t            sec_feature_support;
    uint8_t            rel_sectors;
    uint8_t            rel_param;
    uint8_t            part_config;
    uint8_t            cache_ctrl;
    uint8_t            rst_n_function;
    uint8_t            max_packed_writes;
    uint8_t            max_packed_reads;
    uint8_t            packed_event_en;
    unsigned int        part_time;        /* Units: ms */
    unsigned int        sa_timeout;        /* Units: 100ns */
    unsigned int        generic_cmd6_time;    /* Units: 10ms */
    unsigned int            power_off_longtime;     /* Units: ms */
    uint8_t            power_off_notification;    /* state */
    unsigned int        hs_max_dtr;
    unsigned int        hs200_max_dtr;
#define MMC_HIGH_26_MAX_DTR    26000000
#define MMC_HIGH_52_MAX_DTR    52000000
#define MMC_HIGH_DDR_MAX_DTR    52000000
#define MMC_HS200_MAX_DTR    200000000
    unsigned int        sectors;
    unsigned int        hc_erase_size;        /* In sectors */
    unsigned int        hc_erase_timeout;    /* In milliseconds */
    unsigned int        sec_trim_mult;    /* Secure trim multiplier  */
    unsigned int        sec_erase_mult;    /* Secure erase multiplier */
    unsigned int        trim_timeout;        /* In milliseconds */
    bool            partition_setting_completed;    /* enable bit */
    unsigned long long    enhanced_area_offset;    /* Units: Byte */
    unsigned int        enhanced_area_size;    /* Units: KB */
    unsigned int        cache_size;        /* Units: KB */
    bool            hpi_en;            /* HPI enablebit */
    bool            hpi;            /* HPI support bit */
    unsigned int        hpi_cmd;        /* cmd used as HPI */
    bool            bkops;        /* background support bit */
    bool            man_bkops_en;    /* manual bkops enable bit */
    bool            auto_bkops_en;    /* auto bkops enable bit */
    unsigned int            data_sector_size;       /* 512 bytes or 4KB */
    unsigned int            data_tag_unit_size;     /* DATA TAG UNIT size */
    unsigned int        boot_ro_lock;        /* ro lock support */
    bool            boot_ro_lockable;
    bool            ffu_capable;    /* Firmware upgrade support */
    bool            cmdq_en;    /* Command Queue enabled */
    bool            cmdq_support;    /* Command Queue supported */
    unsigned int        cmdq_depth;    /* Command Queue depth */
#define MMC_FIRMWARE_LEN 8
    uint8_t            fwrev[MMC_FIRMWARE_LEN];  /* FW version */
    uint8_t            raw_exception_status;    /* 54 */
    uint8_t            raw_partition_support;    /* 160 */
    uint8_t            raw_rpmb_size_mult;    /* 168 */
    uint8_t            raw_erased_mem_count;    /* 181 */
    uint8_t            strobe_support;        /* 184 */
    uint8_t            raw_ext_csd_structure;    /* 194 */
    uint8_t            raw_card_type;        /* 196 */
    uint8_t            raw_driver_strength;    /* 197 */
    uint8_t            out_of_int_time;    /* 198 */
    uint8_t            raw_pwr_cl_52_195;    /* 200 */
    uint8_t            raw_pwr_cl_26_195;    /* 201 */
    uint8_t            raw_pwr_cl_52_360;    /* 202 */
    uint8_t            raw_pwr_cl_26_360;    /* 203 */
    uint8_t            raw_s_a_timeout;    /* 217 */
    uint8_t            raw_hc_erase_gap_size;    /* 221 */
    uint8_t            raw_erase_timeout_mult;    /* 223 */
    uint8_t            raw_hc_erase_grp_size;    /* 224 */
    uint8_t            raw_sec_trim_mult;    /* 229 */
    uint8_t            raw_sec_erase_mult;    /* 230 */
    uint8_t            raw_sec_feature_support;/* 231 */
    uint8_t            raw_trim_mult;        /* 232 */
    uint8_t            raw_pwr_cl_200_195;    /* 236 */
    uint8_t            raw_pwr_cl_200_360;    /* 237 */
    uint8_t            raw_pwr_cl_ddr_52_195;    /* 238 */
    uint8_t            raw_pwr_cl_ddr_52_360;    /* 239 */
    uint8_t            raw_pwr_cl_ddr_200_360;    /* 253 */
    uint8_t            raw_bkops_status;    /* 246 */
    uint8_t            raw_sectors[4];        /* 212 - 4 bytes */
    uint8_t            pre_eol_info;        /* 267 */
    uint8_t            device_life_time_est_typ_a;    /* 268 */
    uint8_t            device_life_time_est_typ_b;    /* 269 */

    unsigned int            feature_support;
#define MMC_DISCARD_FEATURE    BIT(0)                  /* CMD38 feature */
};


enum USDHC_BUS_MODE
{
    USDHC_BUS_DDR52,
    USDHC_BUS_HS200,
    USDHC_BUS_HS400,
};

enum USDHC_BUS_WITDTH
{
    USDHC_BUS_8BIT,
    USDHC_BUS_4BIT,
};

struct usdhc_device
{
    __iomem base;
    uint16_t clk_ident; /* Clock divider used during identification */
    uint16_t clk;       /* Clock divider used during normal operation */
    uint64_t sectors;   /* Number of sectors, populated by driver */
    uint32_t bus_mode;
    uint32_t bus_width;
    uint32_t mix_shadow;
    uint8_t boot_bus_cond;
    bool transfer_in_progress;
};


uint32_t usdhc_emmc_xfer_blocks(struct usdhc_device *dev,
                                uint32_t start_lba,
                                uint8_t *bfr,
                                uint32_t nblocks,
                                uint8_t wr,
                                uint8_t async);

uint32_t usdhc_emmc_switch_part(struct usdhc_device *dev,
                                uint8_t part_no);
uint32_t usdhc_emmc_init(struct usdhc_device *dev);

void usdhc_emmc_reset(struct usdhc_device *dev);
uint32_t usdhc_emmc_wait_for_de(struct usdhc_device *dev);

#endif  // PLAT_IMX_USDHC_H_
