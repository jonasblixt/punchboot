/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Synopsys DesignWare DDR PHY: firmware loading, PMU training and mailbox.
 *
 * Clean-room implementation from doc/ports/spec-dwc-ddrphy.md.
 */

#include <drivers/ddr/dwc_ddrphy.h>
#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <stdbool.h>

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

/*
 * Load one part of a firmware image into a PMU memory that starts at CSR
 * 'start'. Each 32 bit word occupies two 16 bit CSRs, low half first. The
 * whole part is written, then read back and verified.
 */
static int load_pmu_mem(const struct dwc_ddrphy *phy,
                        uint32_t start,
                        const uint32_t *words,
                        const uint32_t *words_end,
                        const char *name)
{
    size_t count = (size_t)(words_end - words);

    for (size_t i = 0; i < count; i++) {
        uint32_t csr = start + 2 * (uint32_t)i;

        mmio_write_16(csr_addr(phy, csr), (uint16_t)(words[i] & 0xffff));
        mmio_write_16(csr_addr(phy, csr + 1), (uint16_t)(words[i] >> 16));
    }

    for (size_t i = 0; i < count; i++) {
        uint32_t csr = start + 2 * (uint32_t)i;
        uint16_t lo = mmio_read_16(csr_addr(phy, csr));
        uint16_t hi = mmio_read_16(csr_addr(phy, csr + 1));
        uint32_t readback = ((uint32_t)hi << 16) | lo;

        if (readback != words[i]) {
            LOG_ERR("PHY %s verify failed at word %u: wrote 0x%08x, read 0x%08x",
                    name,
                    (unsigned int)i,
                    words[i],
                    readback);
            return -PB_ERR_IO;
        }
    }

    return PB_OK;
}

static bool mailbox_pending(const struct dwc_ddrphy *phy)
{
    return (dwc_ddrphy_read(phy, DDRPHY_UCTSHADOWREGS) & UCTSHADOWREGS_UCTWRITEPROTSHADOW) == 0;
}

/*
 * Every wait of the PHY's is bounded (F-102, drivers.md 3). On the 8MP EVK
 * (4000 MT/s, 1D and 2D; log 2026-11-16) the PMU's next message took at
 * most 15.1 ms, the ack and the calibration 1 us: 1 s and 10 ms
 */
#define DDRPHY_MAIL_TIMEOUT_us     1000000
#define DDRPHY_TIMEOUT_us          10000
/* A whole training run: 15 to 30 ms each on the 8MP EVK. A PMU that goes on
 * posting messages and never ends is bounded too (the code review) */
#define DDRPHY_TRAINING_TIMEOUT_us 2000000

/* Wait until the mailbox is 'pending' */
static int mailbox_wait(const struct dwc_ddrphy *phy, bool pending, uint32_t timeout_us)
{
    struct pb_timeout to;

    pb_timeout_init_us(&to, timeout_us);

    while (mailbox_pending(phy) != pending) {
        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("PHY: %s", pending ? "no message from the PMU" : "the PMU didn't take the ack");
            return -PB_ERR_TIMEOUT;
        }
    }

    return PB_OK;
}

/*
 * Wait for the PMU to post a message, read it (16 or 32 bit) and hand the
 * mailbox back to the PMU.
 */
static int mailbox_get(const struct dwc_ddrphy *phy, bool wide, uint32_t *msg)
{
    int rc;

    rc = mailbox_wait(phy, true, DDRPHY_MAIL_TIMEOUT_us);
    if (rc != PB_OK)
        return rc;

    *msg = dwc_ddrphy_read(phy, DDRPHY_UCTWRITEONLYSHADOW) & 0xffff;
    if (wide)
        *msg |= (dwc_ddrphy_read(phy, DDRPHY_UCTDATWRITEONLYSHADOW) & 0xffff) << 16;

    dwc_ddrphy_write(phy, DDRPHY_DCTWRITEPROT, DCTWRITEPROT_ACK);
    rc = mailbox_wait(phy, false, DDRPHY_TIMEOUT_us);
    /* The mailbox goes back to the PMU also when it didn't take the ack */
    dwc_ddrphy_write(phy, DDRPHY_DCTWRITEPROT, DCTWRITEPROT_RELEASE);

    return rc;
}

/* Consume a streaming message: a header with the argument count, then the arguments */
static int consume_streaming_msg(const struct dwc_ddrphy *phy)
{
    uint32_t header;
    unsigned int nargs;
    int rc;

    rc = mailbox_get(phy, true, &header);
    if (rc != PB_OK)
        return rc;

    nargs = header & 0xffff;
    LOG_DBG("PHY stream 0x%08x (%u args)", header, nargs);

    for (unsigned int i = 0; i < nargs; i++) {
        uint32_t arg;

        rc = mailbox_get(phy, true, &arg);
        if (rc != PB_OK)
            return rc;

        LOG_DBG("PHY stream arg %u: 0x%08x", i, arg);
    }

    return PB_OK;
}

/* Follow the PMU's messages until training ends: PB_OK when it trained,
 * -PB_ERR_IO when it failed, or a wait's timeout */
static int wait_training_result(const struct dwc_ddrphy *phy)
{
    struct pb_timeout to;

    pb_timeout_init_us(&to, DDRPHY_TRAINING_TIMEOUT_us);

    for (;;) {
        uint32_t major;
        int rc = mailbox_get(phy, false, &major);

        if (rc != PB_OK)
            return rc;

        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("PHY: the training doesn't end (0x%04x)", (unsigned int)major);
            return -PB_ERR_TIMEOUT;
        }

        switch (major) {
        case DDRPHY_MAIL_TRAINING_DONE:
            return PB_OK;
        case DDRPHY_MAIL_TRAINING_FAILED:
            return -PB_ERR_IO;
        case DDRPHY_MAIL_STREAMING_MSG:
            rc = consume_streaming_msg(phy);
            if (rc != PB_OK)
                return rc;
            break;
        default:
            LOG_DBG("PHY training progress 0x%04x", major);
            break;
        }
    }
}

int dwc_ddrphy_train(const struct dwc_ddrphy *phy, const struct dram_fsp_msg *fsp)
{
    const struct dwc_ddrphy_fw *fw;
    int rc;

    if (fsp->fw_type != FW_1D_IMAGE && fsp->fw_type != FW_2D_IMAGE) {
        LOG_ERR("Invalid PHY firmware type %d", (int)fsp->fw_type);
        return -PB_ERR_PARAM;
    }

    fw = phy->fw[fsp->fw_type];
    if (fw == NULL) {
        LOG_ERR("No %s PHY firmware", fsp->fw_type == FW_2D_IMAGE ? "2D" : "1D");
        return -PB_ERR_PARAM;
    }

    LOG_DBG("PHY training, %u MT/s, %s firmware",
            fsp->drate,
            fsp->fw_type == FW_2D_IMAGE ? "2D" : "1D");

    dwc_ddrphy_write(phy, DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);

    rc = load_pmu_mem(phy, DDRPHY_IMEM, fw->imem, fw->imem_end, "IMEM");
    if (rc != PB_OK)
        return rc;

    rc = load_pmu_mem(phy, DDRPHY_DMEM, fw->dmem, fw->dmem_end, "DMEM");
    if (rc != PB_OK)
        return rc;

    dwc_ddrphy_write_cfg(phy, fsp->fsp_cfg, fsp->fsp_cfg_num);

    dwc_ddrphy_write(phy, DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);
    dwc_ddrphy_write(phy, DDRPHY_MICRORESET, MICRORESET_HOLD);
    dwc_ddrphy_write(phy, DDRPHY_MICRORESET, MICRORESET_STALL);
    dwc_ddrphy_write(phy, DDRPHY_MICRORESET, MICRORESET_RUN);

    rc = wait_training_result(phy);
    if (rc != PB_OK) {
        LOG_ERR("PHY training failed at %u MT/s (%i)", fsp->drate, rc);
        /* The PMU doesn't go on running */
        dwc_ddrphy_write(phy, DDRPHY_MICRORESET, MICRORESET_STALL);
        return rc;
    }

    dwc_ddrphy_write(phy, DDRPHY_MICRORESET, MICRORESET_STALL);
    dwc_ddrphy_write(phy, DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);
    dwc_ddrphy_write(phy, DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);

    LOG_DBG("PHY training done, %u MT/s", fsp->drate);
    return PB_OK;
}

int dwc_ddrphy_wait_calibration(const struct dwc_ddrphy *phy)
{
    struct pb_timeout to;

    pb_timeout_init_us(&to, DDRPHY_TIMEOUT_us);

    while (dwc_ddrphy_read(phy, DDRPHY_CALBUSY) & CALBUSY_BUSY) {
        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("PHY: the calibration doesn't end");
            return -PB_ERR_TIMEOUT;
        }
    }

    return PB_OK;
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
