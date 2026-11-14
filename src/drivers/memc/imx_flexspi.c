/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 * Copyright (C) 2023 Marten Svanfeldt <marten.svanfeldt@actia.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/bio.h>
#include <pb/delay.h>
#include <pb/errors.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <string.h>

#include <drivers/memc/imx_flexspi.h>

#include "imx_flexspi_private.h"

static const struct flexspi_core_config *cfg;

/*
 * The bound on a wait for the controller (drivers.md 3): its reset, a
 * sequence's end, a FIFO's watermark. Each is one sequence of at most 64 KiB
 * or one watermark of 8 bytes, microseconds at the serial clocks a board
 * sets; the controller's own sequence timeout (MCR1.SEQWAIT, the largest
 * value) ends a sequence that hangs before this. A bound, not a measurement.
 * The flash's own times are the board's (struct flexspi_nor_config).
 */
#define FLEXSPI_TIMEOUT_us (1000 * 1000)

/* The longest read in one IP command: IPCR1.IDATSZ is 16 bits */
#define FLEXSPI_READ_CHUNK (32 * 1024)

static inline bool imx_flexspi_getbusidle(void)
{
    static const uint32_t mask = FLEXSPI_STS0_SEQIDLE | FLEXSPI_STS0_ARBIDLE;
    uint32_t val = mmio_read_32(cfg->base + FLEXSPI_STS0);
    return (val & mask) == mask;
}

static int imx_flexspi_wait_idle(void)
{
    struct pb_timeout to;

    pb_timeout_init_us(&to, FLEXSPI_TIMEOUT_us);

    while (!imx_flexspi_getbusidle()) {
        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("Timeout, the controller is busy");
            return -PB_ERR_TIMEOUT;
        }
    }

    return PB_OK;
}

static int imx_flexspi_wait_reset(void)
{
    struct pb_timeout to;

    pb_timeout_init_us(&to, FLEXSPI_TIMEOUT_us);

    while (mmio_read_32(cfg->base + FLEXSPI_MCR0) & FLEXSPI_MCR0_SWRESET) {
        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("Timeout, reset");
            return -PB_ERR_TIMEOUT;
        }
    }

    return PB_OK;
}

static int imx_flexspi_checkandclearerror(uint32_t status)
{
    int rc = 0;

    /* Check for error. */
    status &= FLEXSPI_INT_SEQTIMEOUTEN | FLEXSPI_INT_IPCMDERREN | FLEXSPI_INT_IPCMDGEEN;

    if (status) {
        /* Select the correct error code.. */
        if (status & FLEXSPI_INT_SEQTIMEOUTEN) {
            rc = -PB_ERR_TIMEOUT;
        } else if (status & FLEXSPI_INT_IPCMDERREN) {
            rc = -PB_ERR_IO;
        } else if (status & FLEXSPI_INT_IPCMDGEEN) {
            rc = -PB_ERR_IO;
        } else {
            LOG_ERR("Unhandled IRQ (%x)", status);
            rc = -PB_ERR_IO;
        }

        /* Clear the flags. */
        mmio_write_32(cfg->base + FLEXSPI_INTR, status);

        /* Reset fifos. These flags clear automatically. */
        mmio_clrsetbits_32(cfg->base + FLEXSPI_IPTXFCR, 0, FLEXSPI_IPTXFCR_CLRIPTXF_MASK);
        mmio_clrsetbits_32(cfg->base + FLEXSPI_IPRXFCR, 0, FLEXSPI_IPRXFCR_CLRIPRXF_MASK);
    }

    return rc;
}

static int imx_flexspi_writeblocking(uint8_t *buffer, size_t size)
{
    uint8_t txWatermark;
    uint32_t status;
    int rc = 0;
    uint32_t i = 0;
    struct pb_timeout to;

    txWatermark = mmio_read_32(cfg->base + FLEXSPI_IPTXFCR);
    txWatermark &= FLEXSPI_IPTXFCR_TXWMRK_MASK;
    txWatermark >>= FLEXSPI_IPTXFCR_TXWMRK_SHIFT;
    txWatermark += 1;

    /* Send data buffer */
    while (size) {
        /* Wait until there is room in the fifo. This also checks for errors. */
        pb_timeout_init_us(&to, FLEXSPI_TIMEOUT_us);

        while (!((status = mmio_read_32(cfg->base + FLEXSPI_INTR)) & FLEXSPI_INT_IPTXWEEN)) {
            if (pb_timeout_has_expired(&to)) {
                LOG_ERR("Timeout, TX FIFO");
                return -PB_ERR_TIMEOUT;
            }
        }

        rc = imx_flexspi_checkandclearerror(status);

        if (rc != 0) {
            LOG_ERR("xfer error (%x)", status);
            return rc;
        }

        /* Write watermark level data into tx fifo . */
        if (size >= 8 * txWatermark) {
            for (i = 0; i < 2 * txWatermark; i++) {
                uint32_t word;

                /* The caller's buffer has any alignment */
                memcpy(&word, buffer, sizeof(word));
                mmio_write_32(cfg->base + FLEXSPI_TFDR0 + i * 4, word);
                buffer += 4;
            }

            size = size - 8 * txWatermark;
        } else {
            /* The last piece: whole words, the last one padded, and not
             * a byte past the buffer. IDATSZ says how many go out */
            for (i = 0; size > 0; i++) {
                uint32_t word = 0;
                size_t n = (size < 4) ? size : 4;

                memcpy(&word, buffer, n);
                mmio_write_32(cfg->base + FLEXSPI_TFDR0 + i * 4, word);
                buffer += n;
                size -= n;
            }
        }

        /* Push a watermark level datas into IP TX FIFO. INTR is write 1 to
         * clear: a flag set now must not be written back */
        mmio_write_32(cfg->base + FLEXSPI_INTR, FLEXSPI_INT_IPTXWEEN);
    }

    return rc;
}

static int imx_flexspi_readblocking(uint8_t *buffer, size_t size)
{
    uint8_t rxWatermark;
    uint32_t status;
    int rc = 0;
    uint32_t i = 0;
    struct pb_timeout to;

    rxWatermark = mmio_read_32(cfg->base + FLEXSPI_IPRXFCR);
    rxWatermark &= FLEXSPI_IPRXFCR_RXWMRK_MASK;
    rxWatermark >>= FLEXSPI_IPRXFCR_RXWMRK_SHIFT;
    rxWatermark += 1;

    /* Send data buffer */
    while (size) {
        pb_timeout_init_us(&to, FLEXSPI_TIMEOUT_us);

        if (size >= 8 * rxWatermark) {
            /* Wait until there is room in the fifo. This also checks for errors. */
            while (!((status = mmio_read_32(cfg->base + FLEXSPI_INTR)) & FLEXSPI_INT_IPRXWAEN)) {
                rc = imx_flexspi_checkandclearerror(status);

                if (rc != 0)
                    return rc;

                if (pb_timeout_has_expired(&to)) {
                    LOG_ERR("Timeout, RX FIFO");
                    return -PB_ERR_TIMEOUT;
                }
            }
        } else {
            /* Wait fill level. This also checks for errors. */
            while (size >
                   ((((mmio_read_32(cfg->base + FLEXSPI_IPRXFSTS)) & FLEXSPI_IPRXFSTS_FILL_MASK) >>
                     FLEXSPI_IPRXFSTS_FILL_SHIFT) *
                    8U)) {
                rc = imx_flexspi_checkandclearerror(mmio_read_32(cfg->base + FLEXSPI_INTR));

                if (rc != 0)
                    return rc;

                if (pb_timeout_has_expired(&to)) {
                    LOG_ERR("Timeout, RX FIFO");
                    return -PB_ERR_TIMEOUT;
                }
            }
        }

        rc = imx_flexspi_checkandclearerror(mmio_read_32(cfg->base + FLEXSPI_INTR));

        if (rc != 0)
            return rc;

        /* Read watermark level data from rx fifo . */
        if (size >= 8 * rxWatermark) {
            for (i = 0; i < 2 * rxWatermark; i++) {
                uint32_t word = mmio_read_32(cfg->base + FLEXSPI_RFDR0 + i * 4);

                /* The caller's buffer has any alignment */
                memcpy(buffer, &word, sizeof(word));
                buffer += 4;
            }

            size = size - 8 * rxWatermark;
        } else {
            /* The last piece: the bytes asked for, not one more */
            for (i = 0; size > 0; i++) {
                uint32_t word = mmio_read_32(cfg->base + FLEXSPI_RFDR0 + i * 4);
                size_t n = (size < 4) ? size : 4;

                memcpy(buffer, &word, n);
                buffer += n;
                size -= n;
            }
        }

        /* Pop out a watermark level datas from IP RX FIFO. INTR is write 1
         * to clear: a flag set now must not be written back */
        mmio_write_32(cfg->base + FLEXSPI_INTR, FLEXSPI_INT_IPRXWAEN);
    }

    return rc;
}

static int imx_flexspi_xfer(enum flexspi_port port,
                            uint8_t lut_command,
                            uint32_t address,
                            enum flexspi_command_type command_type,
                            void *data,
                            size_t data_length)
{
    uintptr_t port_off = 0;
    uint32_t configValue = 0;
    int rc = 0;
    int rc_idle;

    /* IDATSZ is 16 bits: a longer transfer would be truncated */
    if (data_length > FLEXSPI_IPCR1_IDATSZ_MASK)
        return -PB_ERR_PARAM;

    /* Clear sequence pointer before sending data to external devices. */
    switch (port) {
    case FLEXSPI_PORT_A1:
        port_off = FLEXSPI_FLSHA1CR2;
        break;
    case FLEXSPI_PORT_A2:
        port_off = FLEXSPI_FLSHA2CR2;
        break;
    case FLEXSPI_PORT_B1:
        port_off = FLEXSPI_FLSHB1CR2;
        break;
    case FLEXSPI_PORT_B2:
        port_off = FLEXSPI_FLSHB2CR2;
        break;
    default:
        return -PB_ERR_PARAM;
    }

    mmio_clrsetbits_32(cfg->base + port_off, 0, FLEXSPI_FLSHCR2_CLRINSTRPTR_MASK);

    /* Clear former pending status before start this tranfer. */
    mmio_clrsetbits_32(cfg->base + FLEXSPI_INTR,
                       0,
                       FLEXSPI_INTR_AHBCMDERR_MASK | FLEXSPI_INTR_IPCMDERR_MASK |
                           FLEXSPI_INTR_AHBCMDGE_MASK | FLEXSPI_INTR_IPCMDGE_MASK);

    /* Configure base addresss. */
    mmio_write_32(cfg->base + FLEXSPI_IPCR0, address);

    /* Reset fifos. */
    mmio_clrsetbits_32(cfg->base + FLEXSPI_IPTXFCR, 0, FLEXSPI_IPTXFCR_CLRIPTXF_MASK);
    mmio_clrsetbits_32(cfg->base + FLEXSPI_IPRXFCR, 0, FLEXSPI_IPRXFCR_CLRIPRXF_MASK);

    /* Configure data size. */
    if ((command_type == FLEXSPI_READ) || (command_type == FLEXSPI_WRITE) ||
        (command_type == FLEXSPI_CONFIG)) {
        configValue = FLEXSPI_IPCR1_IDATSZ(data_length);
    }

    /* Configure sequence ID. */
    configValue |= FLEXSPI_IPCR1_ISEQID(lut_command) | FLEXSPI_IPCR1_ISEQNUM(0);
    mmio_write_32(cfg->base + FLEXSPI_IPCR1, configValue);

    /* Start Transfer. */
    mmio_clrsetbits_32(cfg->base + FLEXSPI_IPCMD, 0, FLEXSPI_IPCMD_TRG_MASK);

    if ((command_type == FLEXSPI_WRITE) || (command_type == FLEXSPI_CONFIG)) {
        rc = imx_flexspi_writeblocking(data, data_length);
    } else if (command_type == FLEXSPI_READ) {
        rc = imx_flexspi_readblocking(data, data_length);
    }

    /* Wait for bus idle. */
    rc_idle = imx_flexspi_wait_idle();

    if (rc != PB_OK)
        return rc;

    if (rc_idle != PB_OK)
        return rc_idle;

    if (command_type == FLEXSPI_COMMAND) {
        rc = imx_flexspi_checkandclearerror(mmio_read_32(cfg->base + FLEXSPI_INTR));
    }

    return rc;
}

/* Polls the flash's status until it isn't busy, for timeout_ms at most: the
 * board's time for the operation (struct flexspi_nor_config) */
static int imx_flexspi_busy_wait(struct flexspi_nor_config *cfg_nor,
                                 int polling_time_us,
                                 uint32_t timeout_ms)
{
    int rc;
    uint8_t sts[4];
    struct pb_timeout to;

    pb_timeout_init_us(&to, timeout_ms * 1000);

    do {
        rc = imx_flexspi_xfer(
            cfg_nor->port, cfg_nor->lut_id_read_status, 0, FLEXSPI_READ, sts, sizeof(sts));
        if (rc != 0)
            return rc;

        if (sts[0] & 1) {
            if (pb_timeout_has_expired(&to)) {
                LOG_ERR("Timeout, the flash is busy");
                return -PB_ERR_TIMEOUT;
            }

            pb_delay_us(polling_time_us);
        }

    } while (sts[0] & 1);

    return PB_OK;
}

static int
imx_flexspi_pp(struct flexspi_nor_config *cfg_nor, uint32_t address, uint8_t *data, size_t size)
{
    int rc = 0;
    uint32_t bytes_to_next_page_boundry =
        (uint32_t)cfg_nor->page_size - (address & (uint32_t)(cfg_nor->page_size - 1));

    if ((size > bytes_to_next_page_boundry) || (size > cfg_nor->page_size)) {
        LOG_ERR("Page program failed at address 0x%x, page boundry crossed", address);
        return -PB_ERR_PARAM;
    }

    /* Write enable */
    rc = imx_flexspi_xfer(cfg_nor->port, cfg_nor->lut_id_wr_enable, 0, FLEXSPI_COMMAND, NULL, 0);
    if (rc != PB_OK)
        return rc;

    rc = imx_flexspi_xfer(
        cfg_nor->port, cfg_nor->lut_id_write, address, FLEXSPI_WRITE, (void *)data, size);
    if (rc != PB_OK)
        return rc;

    rc = imx_flexspi_xfer(cfg_nor->port, cfg_nor->lut_id_wr_disable, 0, FLEXSPI_COMMAND, NULL, 0);
    if (rc != PB_OK)
        return rc;

    if (rc != 0) {
        LOG_ERR("Page program failed at address 0x%x, status: %i\n", address, rc);
        return rc;
    }

    return imx_flexspi_busy_wait(
        cfg_nor, cfg_nor->time_page_program_ms, cfg_nor->time_page_program_ms);
}

static int imx_flexspi_read(bio_dev_t dev, lba_t lba, size_t length, void *buf)
{
    uint8_t *buffer = (uint8_t *)buf;
    uint32_t address = lba * bio_block_size(dev);
    struct flexspi_nor_config *cfg_nor = (struct flexspi_nor_config *)bio_get_private(dev);
    int rc;

    /* Bounded by the longest operation, an erase of the largest block */
    rc = imx_flexspi_busy_wait(cfg_nor, 100, cfg_nor->erase_cmds[0].erase_time_ms);

    if (rc != PB_OK)
        return rc;

    /* In pieces that IDATSZ, 16 bits, holds */
    while (length > 0) {
        size_t chunk = length > FLEXSPI_READ_CHUNK ? FLEXSPI_READ_CHUNK : length;

        rc = imx_flexspi_xfer(
            cfg_nor->port, cfg_nor->lut_id_read, address, FLEXSPI_READ, buffer, chunk);

        if (rc != PB_OK)
            return rc;

        buffer += chunk;
        address += chunk;
        length -= chunk;
    }

    return PB_OK;
}

static int imx_flexspi_write(bio_dev_t dev, lba_t lba, size_t length, const void *buf)
{
    int rc = 0;
    struct flexspi_nor_config *cfg_nor = (struct flexspi_nor_config *)bio_get_private(dev);
    uint8_t *buffer = (uint8_t *)buf;
    uint32_t address = lba * bio_block_size(dev);
    uint32_t bytes_to_next_page_boundry =
        (uint32_t)cfg_nor->page_size - (address & (uint32_t)(cfg_nor->page_size - 1));

    LOG_DBG("Write buffer to flash at address: 0x%x, size: 0x%zx", address, length);

    /* A flash still busy ignores the write enable: bounded as a read's */
    rc = imx_flexspi_busy_wait(cfg_nor, 100, cfg_nor->erase_cmds[0].erase_time_ms);

    if (rc != PB_OK)
        return rc;

    if ((bytes_to_next_page_boundry < cfg_nor->page_size) &&
        (length > bytes_to_next_page_boundry)) {
        rc = imx_flexspi_pp(cfg_nor, address, buffer, bytes_to_next_page_boundry);
        if (rc != PB_OK)
            return rc;

        buffer += bytes_to_next_page_boundry;
        address += bytes_to_next_page_boundry;
        length -= bytes_to_next_page_boundry;
    }

    while (length > cfg_nor->page_size) {
        rc = imx_flexspi_pp(cfg_nor, address, buffer, cfg_nor->page_size);
        if (rc != PB_OK)
            return rc;

        buffer += cfg_nor->page_size;
        address += cfg_nor->page_size;
        length -= cfg_nor->page_size;
    }

    if (length > 0) {
        rc = imx_flexspi_pp(cfg_nor, address, buffer, length);
    }

    return rc;
}

/*
 * The board's erase commands are sorted largest first and end at a block
 * size of 0. This is the largest erase unit that starts at addr and that
 * the left bytes cover whole: a NOR erases the unit its address is in, so
 * a unit is only used where it starts (bio.md 3.3: exactly the blocks asked
 * for). NULL when none fits
 */
static const struct flexspi_nor_erase_cmds *
imx_flexspi_erase_cmd(const struct flexspi_nor_config *cfg_nor, uint32_t addr, size_t left)
{
    for (unsigned int i = 0; i < ARRAY_SIZE(cfg_nor->erase_cmds); i++) {
        const struct flexspi_nor_erase_cmds *cmd = &cfg_nor->erase_cmds[i];

        if (cmd->block_size == 0)
            break;

        if (cmd->block_size <= left && (addr % cmd->block_size) == 0)
            return cmd;
    }

    return NULL;
}

static int imx_flexspi_erase_part(bio_dev_t dev, lba_t first_lba, size_t count)
{
    int rc;
    struct flexspi_nor_config *cfg_nor = (struct flexspi_nor_config *)bio_get_private(dev);
    const struct flexspi_nor_erase_cmds *cmd;
    uint32_t addr = (uint32_t)first_lba * bio_block_size(dev);
    size_t bytes_to_erase = count * bio_block_size(dev);
    size_t smallest = 0;

    for (unsigned int i = 0; i < ARRAY_SIZE(cfg_nor->erase_cmds); i++) {
        if (cfg_nor->erase_cmds[i].block_size == 0)
            break;

        smallest = cfg_nor->erase_cmds[i].block_size;
    }

    if (smallest == 0)
        return -PB_ERR_NOT_SUPPORTED;

    /* The smallest unit has to cover the range whole: a short tail isn't
     * rounded up to it, nothing is erased */
    if ((addr % smallest) != 0 || (bytes_to_erase % smallest) != 0) {
        LOG_ERR(
            "0x%08x, %zu bytes, isn't whole erase units of %zu", addr, bytes_to_erase, smallest);
        return -PB_ERR_PARAM;
    }

    /* A flash still busy ignores the write enable: bounded as a read's */
    rc = imx_flexspi_busy_wait(cfg_nor, 100, cfg_nor->erase_cmds[0].erase_time_ms);

    if (rc != PB_OK)
        return rc;

    while (bytes_to_erase) {
        cmd = imx_flexspi_erase_cmd(cfg_nor, addr, bytes_to_erase);

        if (cmd == NULL) {
            LOG_ERR("No erase unit at 0x%08x", addr);
            return -PB_ERR_PARAM;
        }

        LOG_DBG("Erasing addr=0x%08x, block_sz=%zu, lut_id=%u", addr, cmd->block_size, cmd->lut_id);

        /* Write enable */
        rc =
            imx_flexspi_xfer(cfg_nor->port, cfg_nor->lut_id_wr_enable, 0, FLEXSPI_COMMAND, NULL, 0);

        if (rc != 0)
            return rc;

        rc = imx_flexspi_xfer(cfg_nor->port, cmd->lut_id, addr, FLEXSPI_COMMAND, NULL, 0);

        if (rc != 0)
            return rc;

        rc = imx_flexspi_busy_wait(cfg_nor, cmd->erase_time_ms, cmd->erase_time_ms);

        if (rc != 0)
            return rc;

        addr += cmd->block_size;
        bytes_to_erase -= cmd->block_size;
    }

    return imx_flexspi_xfer(cfg_nor->port, cfg_nor->lut_id_wr_disable, 0, FLEXSPI_COMMAND, NULL, 0);
}

static int imx_flexspi_config_lut(void)
{
    int rc;

    /* Wait for bus idle before change flash configuration. */
    rc = imx_flexspi_wait_idle();

    if (rc != PB_OK)
        return rc;

    /* Unlock LUT for update. */
    mmio_write_32(cfg->base + FLEXSPI_LUTKEY, FLEXSPI_LUT_KEY_VAL);
    mmio_write_32(cfg->base + FLEXSPI_LUTCR, FLEXSPI_LUTCR_UNLOCK);

    for (unsigned int i = 0; i < cfg->lut_elements; i++) {
        mmio_write_32(cfg->base + FLEXSPI_LUT + (4 * i), cfg->lut[i]);
    }

    /* Lock LUT. */
    mmio_write_32(cfg->base + FLEXSPI_LUTKEY, FLEXSPI_LUT_KEY_VAL);
    mmio_write_32(cfg->base + FLEXSPI_LUTCR, FLEXSPI_LUTCR_LOCK);
    return PB_OK;
}

static int imx_flexspi_mem_config(const struct flexspi_nor_config *nor_config)
{
    int rc;

    /* Wait for bus idle before change flash configuration. */
    rc = imx_flexspi_wait_idle();

    if (rc != PB_OK)
        return rc;

    mmio_write_32(cfg->base + FLEXSPI_FLSHCR0(nor_config->port), nor_config->capacity / 1024);

    /* Configure flash parameters. */
    mmio_write_32(cfg->base + FLEXSPI_FLSHCR1(nor_config->port), nor_config->cr1);
    mmio_write_32(cfg->base + FLEXSPI_FLSHCR2(nor_config->port), nor_config->cr2);

    return PB_OK;
}

static int imx_flexspi_mem_probe(const struct flexspi_nor_config *nor_config)
{
    int rc;
    uint8_t cmd_buf[4];

    rc = imx_flexspi_xfer(
        nor_config->port, nor_config->lut_id_read_id, 0, FLEXSPI_READ, cmd_buf, sizeof(cmd_buf));

    if (rc != 0)
        return rc;

    if (cmd_buf[0] == nor_config->mfg_id && cmd_buf[1] == nor_config->mfg_device_type &&
        cmd_buf[2] == nor_config->mfg_capacity) {
        LOG_INFO("Detected: %s", nor_config->name);
    } else {
        LOG_ERR("Unknown memory: manufacturer: %02x Device Type %02x, Capacity: %02x",
                cmd_buf[0],
                cmd_buf[1],
                cmd_buf[2]);
        return -PB_ERR_NOT_FOUND;
    }

    bio_dev_t d = bio_allocate(0,
                               nor_config->capacity / nor_config->block_size - 1,
                               nor_config->block_size,
                               nor_config->uuid,
                               nor_config->name);

    if (d < 0)
        return d;

    bio_set_flags(d, BIO_FLAG_WRITABLE | BIO_FLAG_VISIBLE | BIO_FLAG_ERASE_BEFORE_WRITE);

    rc = bio_set_ios(d, imx_flexspi_read, imx_flexspi_write);

    if (rc != 0)
        return rc;

    rc = bio_set_ios_erase(d, imx_flexspi_erase_part);

    if (rc != 0)
        return rc;

    rc = bio_set_private(d, (uintptr_t)nor_config);
    return PB_OK;
}

/*
 * The FlexSPI controller initialization sequence is shown as follows:
 *
 *   1. Enable controller clocks (AHB clock, IP bus clock, or serial root clock) at system level.
 *   2. Set MCR0[MDIS] to 0x1 (make sure that the FlexSPI controller is configured in module stop
 * mode).
 *   3. Configure module control registers: MCR0, MCR1, MCR2. (do not change MCR0[MDIS]).
 *   4. Configure AHB bus control register (AHBCR) and AHB RX Buffer control register (AHBRXBUFxCR0)
 * optionally when AHB command is used.
 *   5. Configure Flash control registers (FLSHxCR0,FLSHxCR1,or FLSHxCR2) according to external
 * device type.
 *   6. Configure DLL control register (DLLxCR) according to sample clock source selection.
 *   7. set MCR0[MDIS] to 0x0 (exit module stop mode).
 *   8. Configure LUT as needed (for AHB command or IP command).
 *   9. Reset controller optionally (by set MCR0[SWRESET] to 0x1).
 */
int imx_flexspi_init(const struct flexspi_core_config *config)
{
    int rc;
    cfg = config;

    LOG_INFO("FlexSPI init 0x%lx", cfg->base);

    /* Reset */
    mmio_clrbits_32(cfg->base + FLEXSPI_MCR0, FLEXSPI_MCR0_MDIS);
    mmio_setbits_32(cfg->base + FLEXSPI_MCR0, FLEXSPI_MCR0_SWRESET);

    /* Wait for reset */
    rc = imx_flexspi_wait_reset();

    if (rc != PB_OK)
        return rc;

    /*
     * Configure flexspi block. There are a bunch of hard coded things here
     * for now, some should probably be set through the config struct.
     */
    mmio_write_32(cfg->base + FLEXSPI_MCR0,
                  FLEXSPI_MCR0_MDIS | FLEXSPI_MCR0_RXCLKSRC(0) | FLEXSPI_MCR0_DOZEEN |
                      FLEXSPI_MCR0_IPGRANTWAIT(0xFF) | FLEXSPI_MCR0_AHBGRANTWAIT(0xFF));

    /* The longest timeouts */
    mmio_write_32(cfg->base + FLEXSPI_MCR1,
                  FLEXSPI_MCR1_SEQWAIT(0xFFFF) | FLEXSPI_MCR1_AHBBUSWAIT(0xFFFF));

    mmio_clrsetbits_32(cfg->base + FLEXSPI_MCR2,
                       FLEXSPI_MCR2_RESUMEWAIT_MASK | FLEXSPI_MCR2_SCKBDIFFOPT |
                           FLEXSPI_MCR2_SAMEDEVICEEN | FLEXSPI_MCR2_CLRAHBBUFOPT,

                       FLEXSPI_MCR2_RESUMEWAIT(20));

    /* AHB control */
    mmio_clrsetbits_32(cfg->base + FLEXSPI_AHBCR,
                       FLEXSPI_AHBCR_CACHABLEEN | FLEXSPI_AHBCR_BUFFERABLEEN |
                           FLEXSPI_AHBCR_PREFETCHEN | FLEXSPI_AHBCR_READADDROPT,

                       FLEXSPI_AHBCR_CACHABLEEN | FLEXSPI_AHBCR_BUFFERABLEEN |
                           FLEXSPI_AHBCR_PREFETCHEN | FLEXSPI_AHBCR_READADDROPT);

    /* AHB buffers */
    for (int i = 0; i < 4; i++) {
        mmio_write_32(cfg->base + FLEXSPI_AHBRXBUFCR0(i), FLEXSPI_AHBRXBUFCR0_BUFSZ(256));
    }

    /* Reset size of all attached flashes */
    for (int i = 0; i < 4; i++) {
        mmio_write_32(cfg->base + FLEXSPI_FLSHCR0(i), 0);
    }

    /* Configure DLL. */
    mmio_write_32(cfg->base + FLEXSPI_DLLACR, cfg->dllacr);
    mmio_write_32(cfg->base + FLEXSPI_DLLBCR, cfg->dllbcr);

    /* Configure memories */
    for (unsigned int i = 0; i < cfg->mem_length; i++) {
        if (cfg->mem[i]->capacity % cfg->mem[i]->block_size != 0)
            return -PB_ERR_ALIGN;
        rc = imx_flexspi_mem_config(cfg->mem[i]);

        if (rc != 0)
            return rc;
    }

    /* Configure write masks for Port A and B. */
    mmio_write_32(cfg->base + FLEXSPI_FLSHCR4, cfg->cr4);

    /* Exit stop mode. */
    mmio_clrsetbits_32(cfg->base + FLEXSPI_MCR0, FLEXSPI_MCR0_MDIS, 0);

    /* TX&RX fifos */
    mmio_clrsetbits_32(
        cfg->base + FLEXSPI_IPRXFCR, FLEXSPI_IPRXFCR_RXWMRK_MASK, FLEXSPI_IPRXFCR_RXWMRK(8));

    mmio_clrsetbits_32(
        cfg->base + FLEXSPI_IPTXFCR, FLEXSPI_IPTXFCR_TXWMRK_MASK, FLEXSPI_IPTXFCR_TXWMRK(8));

    /* Update LUT */
    rc = imx_flexspi_config_lut();

    if (rc != PB_OK)
        return rc;

    /* Reset again, a SW Reset does not reset any registers */
    mmio_setbits_32(cfg->base + FLEXSPI_MCR0, FLEXSPI_MCR0_SWRESET);

    /* Wait for reset */
    rc = imx_flexspi_wait_reset();

    if (rc != PB_OK)
        return rc;

    /* Probe memories */
    for (unsigned int i = 0; i < cfg->mem_length; i++) {
        rc = imx_flexspi_mem_probe(cfg->mem[i]);

        if (rc != 0)
            return rc;
    }

    return PB_OK;
}
