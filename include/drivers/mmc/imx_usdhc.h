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
#include <pb/utils_def.h>

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
 * of the slowest rate measured on a board (jiffy's eMMC, 77 MiB/s). A board
 * checks that one chunk fits its watchdog with room (platform.md 3.3).
 * On a bus slower than the floor (an SD card on one line, 3.1 MB/s at 25 MHz)
 * the driver waits the access time and twice the bus's own time instead.
 */
#define IMX_USDHC_READ_ACCESS_us   1000000ULL
#define IMX_USDHC_READ_ns_PER_BYTE 250ULL
#define IMX_USDHC_READ_TIMEOUT_us(bytes) \
    (IMX_USDHC_READ_ACCESS_us + ((unsigned long long)(bytes) * IMX_USDHC_READ_ns_PER_BYTE) / 1000)

/*
 * What a board that loads from a uSDHC states (platform.md 3.3, F-321): a
 * read of one chunk, the boot's and command mode's, waits at most half its
 * watchdog, so that a card that stops answering is -PB_ERR_TIMEOUT and the
 * next candidate, not a watchdog reset.
 */
#ifdef CONFIG_ENABLE_WATCHDOG
#define IMX_USDHC_FITS_THE_WATCHDOG(bytes) \
    (IMX_USDHC_READ_TIMEOUT_us(bytes) <= (unsigned long long)CONFIG_WATCHDOG_TIMEOUT * 500000ULL)
#else
#define IMX_USDHC_FITS_THE_WATCHDOG(bytes) 1
#endif

#ifdef CONFIG_CM
#define IMX_USDHC_CM_CHUNK_BYTES (CONFIG_CM_BUF_SIZE_KiB * 1024ULL)
#else
#define IMX_USDHC_CM_CHUNK_BYTES 0ULL
#endif

#define IMX_USDHC_CHECK_CHUNKS()                                                       \
    PB_STATIC_ASSERT(IMX_USDHC_FITS_THE_WATCHDOG(CONFIG_BOOT_LOAD_CHUNK_kB * 1024ULL), \
                     "a boot chunk's read must fit half the watchdog (F-321)");        \
    PB_STATIC_ASSERT(IMX_USDHC_FITS_THE_WATCHDOG(IMX_USDHC_CM_CHUNK_BYTES),            \
                     "a command mode buffer's read must fit half the watchdog (F-321)")

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
