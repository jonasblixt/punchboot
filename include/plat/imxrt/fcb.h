/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The FlexSPI configuration block (FCB): the 512 bytes at offset 0 of a
 * FlexSPI NOR boot device that tell the i.MX RT10xx boot ROM how to read the
 * flash, before it reads the IVT at 0x1000. From the i.MX RT1060 RM, 9.6.3.1
 * "FlexSPI Configuration Block" (Table 9-15) and 9.6.3.2 "Serial NOR
 * configuration block" (Table 9-17). Boards provide one per flash chip
 * (src/board/<board>/fcb_*.c); the build puts it in front of pb.imx.
 */

#ifndef INCLUDE_PLAT_IMXRT_FCB_H
#define INCLUDE_PLAT_IMXRT_FCB_H

#include <stddef.h>
#include <stdint.h>

#define IMXRT_FCB_TAG                           0x42464346 /* "FCFB" */
/* 'V' 1.4.0, what NXP's tools write. The RM lists 1.0.0 and 1.1.0; the ROM
 * only checks the major version */
#define IMXRT_FCB_VERSION                       0x56010400

/* readSampleClkSrc */
#define IMXRT_FCB_READ_SAMPLE_INTERNAL_LOOPBACK 0
#define IMXRT_FCB_READ_SAMPLE_DQS_PAD_LOOPBACK  1
#define IMXRT_FCB_READ_SAMPLE_FLASH_DQS         3

/* controllerMiscOption */
#define IMXRT_FCB_MISC_SAFE_CONFIG_FREQ         (1u << 4)

/* deviceType */
#define IMXRT_FCB_DEVICE_SERIAL_NOR             1

/* sflashPadType */
#define IMXRT_FCB_PADS_QUAD                     4

/* serialClkFreq, ipCmdSerialClkFreq */
#define IMXRT_FCB_CLK_30MHZ                     1
#define IMXRT_FCB_CLK_120MHZ                    7
#define IMXRT_FCB_CLK_133MHZ                    8

/* Sequences in lookupTable that the ROM uses (Table 9-16), 4 entries each */
#define IMXRT_FCB_LUT_READ                      0
#define IMXRT_FCB_LUT_READ_STATUS               1
#define IMXRT_FCB_LUT_WRITE_ENABLE              3
#define IMXRT_FCB_LUT_ERASE_SECTOR              5
/* Not in Table 9-16: NXP's flash tools look for the block erase here */
#define IMXRT_FCB_LUT_ERASE_BLOCK               8
#define IMXRT_FCB_LUT_PAGE_PROGRAM              9
#define IMXRT_FCB_LUT_CHIP_ERASE                11

struct imxrt_fcb {
    uint32_t tag; /* 0x000 */
    uint32_t version;
    uint32_t rsvd0;
    uint8_t read_sample_clk_src; /* 0x00c */
    uint8_t cs_hold_time;
    uint8_t cs_setup_time;
    uint8_t column_address_width;
    uint8_t device_mode_cfg_enable; /* 0x010 */
    uint8_t rsvd1;
    uint16_t wait_time_cfg_commands;
    uint32_t device_mode_seq; /* 0x014 */
    uint32_t device_mode_arg;
    uint8_t config_cmd_enable; /* 0x01c */
    uint8_t rsvd2[3];
    uint32_t config_cmd_seqs[3]; /* 0x020 */
    uint32_t rsvd3;
    uint32_t cfg_cmd_args[3]; /* 0x030 */
    uint32_t rsvd4;
    uint32_t controller_misc_option; /* 0x040 */
    uint8_t device_type; /* 0x044 */
    uint8_t sflash_pad_type;
    uint8_t serial_clk_freq;
    uint8_t lut_custom_seq_enable;
    uint32_t rsvd5[2]; /* 0x048 */
    uint32_t sflash_a1_size; /* 0x050 */
    uint32_t sflash_a2_size;
    uint32_t sflash_b1_size;
    uint32_t sflash_b2_size;
    uint32_t cs_pad_setting_override; /* 0x060 */
    uint32_t sclk_pad_setting_override;
    uint32_t data_pad_setting_override;
    uint32_t dqs_pad_setting_override;
    uint32_t timeout_ms; /* 0x070 */
    uint32_t command_interval;
    uint32_t data_valid_time;
    uint16_t busy_offset; /* 0x07c */
    uint16_t busy_bit_polarity;
    uint32_t lookup_table[64]; /* 0x080 */
    uint32_t lut_custom_seq[12]; /* 0x180 */
    uint32_t rsvd6[4]; /* 0x1b0 */
    /* Serial NOR (Table 9-17) */
    uint32_t page_size; /* 0x1c0 */
    uint32_t sector_size;
    uint8_t ip_cmd_serial_clk_freq; /* 0x1c8 */
    uint8_t rsvd7[7];
    /* Not in Table 9-17, the ROM doesn't use it: NXP's flash tools do */
    uint32_t block_size; /* 0x1d0 */
    uint32_t rsvd8[11];
};

_Static_assert(offsetof(struct imxrt_fcb, read_sample_clk_src) == 0x00c, "FCB layout");
_Static_assert(offsetof(struct imxrt_fcb, config_cmd_seqs) == 0x020, "FCB layout");
_Static_assert(offsetof(struct imxrt_fcb, controller_misc_option) == 0x040, "FCB layout");
_Static_assert(offsetof(struct imxrt_fcb, sflash_a1_size) == 0x050, "FCB layout");
_Static_assert(offsetof(struct imxrt_fcb, busy_offset) == 0x07c, "FCB layout");
_Static_assert(offsetof(struct imxrt_fcb, lookup_table) == 0x080, "FCB layout");
_Static_assert(offsetof(struct imxrt_fcb, lut_custom_seq) == 0x180, "FCB layout");
_Static_assert(offsetof(struct imxrt_fcb, page_size) == 0x1c0, "FCB layout");
_Static_assert(offsetof(struct imxrt_fcb, ip_cmd_serial_clk_freq) == 0x1c8, "FCB layout");
_Static_assert(offsetof(struct imxrt_fcb, block_size) == 0x1d0, "FCB layout");
_Static_assert(sizeof(struct imxrt_fcb) == 512, "FCB size");

/* Puts a board's FCB where the build takes it from */
#define IMXRT_FCB __attribute__((section(".fcb"), used))

#endif // INCLUDE_PLAT_IMXRT_FCB_H
