/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The parts of mmc_core shared by the card types: the device state and the
 * command helpers in mmc_core.c, used by mmc_emmc.c and mmc_sd.c.
 */

#ifndef DRIVERS_MMC_MMC_CORE_PRIVATE_H
#define DRIVERS_MMC_MMC_CORE_PRIVATE_H

#include <drivers/mmc/mmc_core.h>
#include <pb/bio.h>
#include <stdbool.h>
#include <stdint.h>

#define MMC_CORE_MAX_DEVICES    CONFIG_MMC_CORE_MAX_DEVICES
#define MMC_DEFAULT_TIMEOUT_ms  15
#define MMC_ID_CLK_HZ           400000 /* Identification clock */

/* The largest value of the EXT_CSD timeout fields, used until they are read and when they are
 * 0, "not defined" (JESD84-B51 7.4.31, 7.4.32, 7.4.56) */
#define MMC_CMD6_TIME_MAX_ms    2550

/*
 * The card's state after a block transfer. The HAL has waited for the end of
 * the data and of the busy after written data (struct mmc_cmd), so a card is
 * back in Transfer state at the first CMD13. The bound is twice the longest
 * write busy the SD Physical Layer allows, 500 ms for SDXC (4.6.2.2), far
 * above that
 */
#define MMC_XFER_END_TIMEOUT_ms 1000

/* bio HAL flags: the e.MMC hardware partition of a bio device */
#define MMC_BIO_FLAG_BOOT0      BIT(0)
#define MMC_BIO_FLAG_BOOT1      BIT(1)
#define MMC_BIO_FLAG_RPMB       BIT(2)
#define MMC_BIO_FLAG_USER       BIT(3)

struct mmc_device {
    uint8_t ext_csd[512] __aligned(64); /* e.MMC */
    bool used;
    /* bio devices name this slot: it stays used after an init that failed */
    bool registered;
    bool ready; /* The last mmc_init() worked: the bio devices may be used */
    const struct mmc_hal *hal;
    const struct mmc_device_config *cfg;
    struct mmc_device_info info;
    uint16_t rca;
    uint32_t ocr;
    uint32_t csd[4];
    bool block_addressing; /* Data addresses are block numbers, not bytes */
    uint64_t blocks; /* User area */
    /* e.MMC */
    uint8_t part_config; /* PARTITION_CONFIG as the card has it */
    enum mmc_part part;
    unsigned int generic_cmd6_time_ms;
    unsigned int partition_switch_time_ms;
    unsigned int power_off_long_time_ms;
    /* SD: SCR and the CMD6 status */
    uint8_t sd_data[64] __aligned(64);
    uint8_t scr[8];
};

int mmc_request(struct mmc_device *dev, struct mmc_cmd *cmd);
int mmc_send_cmd(struct mmc_device *dev,
                 uint16_t cmd_idx,
                 uint32_t arg,
                 uint16_t resp_type,
                 mmc_cmd_resp_t result);
/* A command with an R1 response, failing on the error bits it reports */
int mmc_request_r1(struct mmc_device *dev, struct mmc_cmd *cmd);
int mmc_send_cmd_r1(struct mmc_device *dev, uint16_t cmd_idx, uint32_t arg);
int mmc_reset_to_idle(struct mmc_device *dev);
/* CMD13 until READY_FOR_DATA, returns the card state or a negative error */
int mmc_device_state(struct mmc_device *dev, unsigned int timeout_ms);
/* CMD13 until the card is in 'state' */
int mmc_wait_state(struct mmc_device *dev, int state, unsigned int timeout_ms);
int mmc_set_bus_clock(struct mmc_device *dev, unsigned int clk_hz);
/* Reads or writes the current partition in chunks the HAL can take */
int mmc_bio_xfer(struct mmc_device *dev, lba_t lba, size_t length, uintptr_t buf, bool write);
/*
 * Before a device is registered. A device is registered once (bio.md 2.1):
 * 1 when this controller's device with that UUID is there already, from an
 * mmc_init() before; a controller that is set up again keeps the devices it
 * has, with the flags they have by now. -PB_ERR_IO when that device's last
 * block isn't 'last_lba': a card of another size, and a device's range is
 * what the medium has. 0 when the UUID is free. -PB_ERR_PARAM when another
 * device has it. Checked for every device before the first is registered:
 * nothing is registered when init fails (drivers.md 2.1)
 */
int mmc_bio_check(struct mmc_device *dev, const unsigned char *uu, lba_t last_lba);
/* PB_OK when the card's last mmc_init() worked, -PB_ERR_STATE otherwise:
 * every bio callback asks first */
int mmc_bio_ready(const struct mmc_device *dev);

int mmc_emmc_setup(struct mmc_device *dev);
int mmc_sd_setup(struct mmc_device *dev);

#endif // DRIVERS_MMC_MMC_CORE_PRIVATE_H
