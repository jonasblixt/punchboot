/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The FlexSPI configuration block for the ISSI IS25WP064 (8 MiB QSPI NOR) on
 * the MIMXRT1060-EVK (revision A) and the EVKB: quad I/O reads at 120 MHz,
 * the read clock looped back from the DQS pad. The same bytes as the block
 * NXP's tools put on a rev A2 EVK. Not linked into pb: the build puts it in
 * front of pb.imx in pb_flash.bin.
 */

#include <drivers/memc/imx_flexspi.h>
#include <plat/imxrt/fcb.h>

#define LUT(seq, n) [4 * (seq) + (n)]

IMXRT_FCB const struct imxrt_fcb imxrt1060evk_fcb = {
    .tag = IMXRT_FCB_TAG,
    .version = IMXRT_FCB_VERSION,
    .read_sample_clk_src = IMXRT_FCB_READ_SAMPLE_DQS_PAD_LOOPBACK,
    .cs_hold_time = 3,
    .cs_setup_time = 3,
    .controller_misc_option = IMXRT_FCB_MISC_SAFE_CONFIG_FREQ,
    .device_type = IMXRT_FCB_DEVICE_SERIAL_NOR,
    .sflash_pad_type = IMXRT_FCB_PADS_QUAD,
    .serial_clk_freq = IMXRT_FCB_CLK_120MHZ,
    .sflash_a1_size = 8 * 1024 * 1024,
    .lookup_table = {
        /* Fast read quad I/O: 24 bit address, 6 dummy cycles (mode + dummy) */
        LUT(IMXRT_FCB_LUT_READ, 0) = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR, FLEXSPI_1PAD, 0xeb,
                                                     FLEXSPI_COMMAND_RADDR_SDR, FLEXSPI_4PAD, 0x18),
        LUT(IMXRT_FCB_LUT_READ, 1) = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_DUMMY_SDR, FLEXSPI_4PAD, 0x06,
                                                     FLEXSPI_COMMAND_READ_SDR, FLEXSPI_4PAD, 0x04),
        LUT(IMXRT_FCB_LUT_READ_STATUS, 0) = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR, FLEXSPI_1PAD, 0x05,
                                                            FLEXSPI_COMMAND_READ_SDR, FLEXSPI_1PAD, 0x04),
        LUT(IMXRT_FCB_LUT_WRITE_ENABLE, 0) = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR, FLEXSPI_1PAD, 0x06,
                                                             FLEXSPI_COMMAND_STOP, FLEXSPI_1PAD, 0),
        /* 4 KiB sector erase */
        LUT(IMXRT_FCB_LUT_ERASE_SECTOR, 0) = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR, FLEXSPI_1PAD, 0x20,
                                                             FLEXSPI_COMMAND_RADDR_SDR, FLEXSPI_1PAD, 0x18),
        /* 64 KiB block erase */
        LUT(IMXRT_FCB_LUT_ERASE_BLOCK, 0) = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR, FLEXSPI_1PAD, 0xd8,
                                                            FLEXSPI_COMMAND_RADDR_SDR, FLEXSPI_1PAD, 0x18),
        LUT(IMXRT_FCB_LUT_PAGE_PROGRAM, 0) = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR, FLEXSPI_1PAD, 0x02,
                                                             FLEXSPI_COMMAND_RADDR_SDR, FLEXSPI_1PAD, 0x18),
        LUT(IMXRT_FCB_LUT_PAGE_PROGRAM, 1) = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_WRITE_SDR, FLEXSPI_1PAD, 0x04,
                                                             FLEXSPI_COMMAND_STOP, FLEXSPI_1PAD, 0),
        LUT(IMXRT_FCB_LUT_CHIP_ERASE, 0) = FLEXSPI_LUT_SEQ(FLEXSPI_COMMAND_SDR, FLEXSPI_1PAD, 0x60,
                                                           FLEXSPI_COMMAND_STOP, FLEXSPI_1PAD, 0),
    },
    .page_size = 256,
    .sector_size = 4 * 1024,
    .ip_cmd_serial_clk_freq = IMXRT_FCB_CLK_30MHZ,
    .block_size = 64 * 1024,
};
