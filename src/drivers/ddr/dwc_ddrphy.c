/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Synopsys DesignWare DDR4/LPDDR4 PHY with firmware training: firmware
 * load, PMU mailbox and training runs. See drivers/ddr/dwc_ddrphy.h.
 *
 * The CSR numbers, the mailbox handshake and the message codes are how the
 * PHY and its training firmware work, documented in dwc_ddrphy.h. The
 * CalBusy wait is also in NXP's BSD-3-Clause TF-A code
 * (plat/imx/imx8m/ddr); the firmware load and the training are not.
 */

#include <drivers/ddr/dwc_ddrphy.h>
#include <pb/mmio.h>
#include <pb/pb.h>

static uintptr_t csr_addr(const struct dwc_ddrphy *phy, uint32_t csr)
{
    return phy->base + (uintptr_t)phy->csr_stride * csr;
}

void dwc_ddrphy_write(const struct dwc_ddrphy *phy, uint32_t csr, uint32_t val)
{
    mmio_write_32(csr_addr(phy, csr), val);
}

uint32_t dwc_ddrphy_read(const struct dwc_ddrphy *phy, uint32_t csr)
{
    return mmio_read_32(csr_addr(phy, csr));
}

void dwc_ddrphy_write_cfg(const struct dwc_ddrphy *phy,
                          const struct dram_cfg_param *cfg,
                          unsigned int n)
{
    for (unsigned int i = 0; i < n; i++)
        dwc_ddrphy_write(phy, cfg[i].reg, cfg[i].val);
}

/* Write a firmware image to PMU memory at 'csr', two CSRs per word, and read it back */
static int
load_mem(const struct dwc_ddrphy *phy, uint32_t csr, const uint32_t *start, const uint32_t *end)
{
    uintptr_t to = csr_addr(phy, csr);

    for (const uint32_t *p = start; p < end; p++) {
        mmio_write_16(to, *p & 0xffff);
        mmio_write_16(to + phy->csr_stride, *p >> 16);
        to += 2 * phy->csr_stride;
    }

    to = csr_addr(phy, csr);

    for (const uint32_t *p = start; p < end; p++) {
        uint32_t word = mmio_read_16(to) | ((uint32_t)mmio_read_16(to + phy->csr_stride) << 16);

        if (word != *p)
            return -PB_ERR_IO;
        to += 2 * phy->csr_stride;
    }

    return PB_OK;
}

static int load_training_firmware(const struct dwc_ddrphy *phy, enum fw_type type)
{
    const struct dwc_ddrphy_fw *fw = phy->fw[type];
    int rc;

    LOG_INFO("Loading image from %p, sz %lu bytes",
             (void *)fw->imem,
             (unsigned long)((uintptr_t)fw->imem_end - (uintptr_t)fw->imem));

    rc = load_mem(phy, DDRPHY_IMEM, fw->imem, fw->imem_end);
    if (rc == PB_OK)
        rc = load_mem(phy, DDRPHY_DMEM, fw->dmem, fw->dmem_end);

    return rc;
}

/* Mailbox: wait for a message from the PMU, read it, then ack it */
static void wait_mail(const struct dwc_ddrphy *phy)
{
    while (dwc_ddrphy_read(phy, DDRPHY_UCTSHADOWREGS) & UCTSHADOWREGS_UCTWRITEPROTSHADOW)
        ;
}

static void ack_mail(const struct dwc_ddrphy *phy)
{
    dwc_ddrphy_write(phy, DDRPHY_DCTWRITEPROT, DCTWRITEPROT_ACK);

    while (!(dwc_ddrphy_read(phy, DDRPHY_UCTSHADOWREGS) & UCTSHADOWREGS_UCTWRITEPROTSHADOW))
        ;

    dwc_ddrphy_write(phy, DDRPHY_DCTWRITEPROT, DCTWRITEPROT_RELEASE);
}

/* A major message: 16 bits */
static uint32_t get_mail(const struct dwc_ddrphy *phy)
{
    uint32_t mail;

    wait_mail(phy);
    mail = dwc_ddrphy_read(phy, DDRPHY_UCTWRITEONLYSHADOW);
    ack_mail(phy);

    return mail;
}

/* A word of a streaming message: 32 bits */
static uint32_t get_stream_word(const struct dwc_ddrphy *phy)
{
    uint32_t lo, hi;

    wait_mail(phy);
    lo = dwc_ddrphy_read(phy, DDRPHY_UCTWRITEONLYSHADOW);
    hi = dwc_ddrphy_read(phy, DDRPHY_UCTDATWRITEONLYSHADOW);
    ack_mail(phy);

    return (hi << 16) | lo;
}

/*
 * A streaming message is a string index (into a string table that comes
 * with the firmware) whose low 16 bits are the number of arguments that
 * follow.
 */
static void decode_streaming_message(const struct dwc_ddrphy *phy)
{
    uint32_t string_index = get_stream_word(phy);

    for (unsigned int i = 0; i < (string_index & 0xffff); i++) {
        uint32_t arg = get_stream_word(phy);
        LOG_DBG("    arg[%u] = 0x%x", i, arg);
    }
}

static int wait_training_complete(const struct dwc_ddrphy *phy)
{
    while (1) {
        uint32_t mail = get_mail(phy);

        if (mail == DDRPHY_MAIL_STREAMING_MSG) {
            decode_streaming_message(phy);
        } else if (mail == DDRPHY_MAIL_TRAINING_DONE) {
            LOG_INFO("Training PASS");
            return PB_OK;
        } else if (mail == DDRPHY_MAIL_TRAINING_FAILED) {
            LOG_ERR("Training FAILED");
            return -PB_ERR_IO;
        }
    }
}

int dwc_ddrphy_train(const struct dwc_ddrphy *phy, const struct dram_fsp_msg *fsp)
{
    int rc;

    LOG_DBG("Training %u MT/s, %s", fsp->drate, fsp->fw_type == FW_2D_IMAGE ? "2D" : "1D");

    dwc_ddrphy_write(phy, DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);

    rc = load_training_firmware(phy, fsp->fw_type);
    if (rc != PB_OK) {
        LOG_ERR("Training firmware verify failed");
        return rc;
    }

    /* Message block (DMEM) */
    dwc_ddrphy_write_cfg(phy, fsp->fsp_cfg, fsp->fsp_cfg_num);

    dwc_ddrphy_write(phy, DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);
    dwc_ddrphy_write(phy, DDRPHY_MICRORESET, MICRORESET_HOLD);
    dwc_ddrphy_write(phy, DDRPHY_MICRORESET, MICRORESET_STALL);
    dwc_ddrphy_write(phy, DDRPHY_MICRORESET, MICRORESET_RUN);

    rc = wait_training_complete(phy);
    if (rc != PB_OK)
        return rc;

    /* Halt the microcontroller */
    dwc_ddrphy_write(phy, DDRPHY_MICRORESET, MICRORESET_STALL);

    /* The message block results could be read here, with APB access (F-104) */
    dwc_ddrphy_write(phy, DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);
    dwc_ddrphy_write(phy, DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);

    return PB_OK;
}

void dwc_ddrphy_wait_calibration(const struct dwc_ddrphy *phy)
{
    while (dwc_ddrphy_read(phy, DDRPHY_CALBUSY) & CALBUSY_BUSY)
        ;
}

void dwc_ddrphy_read_csrs(const struct dwc_ddrphy *phy, struct dram_cfg_param *csrs, unsigned int n)
{
    dwc_ddrphy_write(phy, DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);
    dwc_ddrphy_write(
        phy, DDRPHY_UCCLKHCLKENABLES, UCCLKHCLKENABLES_HCLKEN | UCCLKHCLKENABLES_UCCLKEN);

    for (unsigned int i = 0; i < n; i++)
        csrs[i].val = dwc_ddrphy_read(phy, csrs[i].reg);

    dwc_ddrphy_write(phy, DDRPHY_UCCLKHCLKENABLES, UCCLKHCLKENABLES_HCLKEN);
    dwc_ddrphy_write(phy, DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);
}
