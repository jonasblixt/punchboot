/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef DRIVERS_MMC_IMX_USDHC
#define DRIVERS_MMC_IMX_USDHC

#include <drivers/mmc/mmc_core.h>

struct imx_usdhc_config {
    uintptr_t base; /*!< Base address of USDHC controller */
    unsigned int delay_tap;
    bool card_detect; /*!< CD_B is connected (SD slots) */
    const struct mmc_device_config mmc_config; /*!< MMC layer configuration */
};

/*
 * What the driver leaves behind (bio.md 5), as the code does it today. Its
 * calls are synchronous, so it has no stop. The controller stays enabled as
 * the last call left it: the card clock's dividers for the bus mode, the bus
 * width and DDR_EN, the delay line's tap, ADMA2 selected and ADMA_SYS_ADDR
 * at the driver's descriptor table, which the controller reads only when a
 * command with data is written. Its interrupt signals are off. The card's
 * side is in drivers/mmc/mmc_core.h.
 */

/*
 * The longest a read of 'bytes' bytes in one request waits for its data
 * (drivers.md 3, D-073): the access time of the first block and a floor on
 * the rate. JESD84-B51 Table 71 and 7.3.3: an eMMC's NAC is at most
 * 10 x (TAAC + NSAC), with TAAC at most 80 ms and NSAC 25.5k clocks, about
 * 800 ms; an SD card's read timeout is 100 ms. The floor is 4 MB/s: a third
 * of an SD card's default speed on four lines (12.5 MB/s), and a twentieth
 * of the slowest rate measured on a board (jiffy's eMMC, 77 MiB/s). The core
 * reads in chunks that fit the watchdog with room (platform.md 3.3).
 */
#define IMX_USDHC_READ_ACCESS_us   1000000ULL
#define IMX_USDHC_READ_ns_PER_BYTE 250ULL
#define IMX_USDHC_READ_TIMEOUT_us(bytes) \
    (IMX_USDHC_READ_ACCESS_us + ((unsigned long long)(bytes) * IMX_USDHC_READ_ns_PER_BYTE) / 1000)

/**
 * Initialize the usdhc mmc controller
 *
 * @param[in] cfg Pointer to a USDHC configuration struct
 * @param[in] intput_clock_hz USDHC input clock (Hz)
 *
 * @return PB_OK on success
 */
int imx_usdhc_init(const struct imx_usdhc_config *cfg, unsigned int input_clock_hz);

#endif
