/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Synopsys DesignWare DDR PHY: firmware loading, PMU training and mailbox.
 *
 * Clean-room implementation from doc/dev/spec-dwc-ddrphy.md.
 */

#include <drivers/ddr/dwc_ddrphy.h>
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
 * Wait for the PMU to post a message, read it (16 or 32 bit) and hand the
 * mailbox back to the PMU.
 */
static uint32_t mailbox_get(const struct dwc_ddrphy *phy, bool wide)
{
    uint32_t msg;

    while (!mailbox_pending(phy))
        ;

    msg = dwc_ddrphy_read(phy, DDRPHY_UCTWRITEONLYSHADOW) & 0xffff;
    if (wide)
        msg |= (dwc_ddrphy_read(phy, DDRPHY_UCTDATWRITEONLYSHADOW) & 0xffff) << 16;

    dwc_ddrphy_write(phy, DDRPHY_DCTWRITEPROT, DCTWRITEPROT_ACK);
    while (mailbox_pending(phy))
        ;
    dwc_ddrphy_write(phy, DDRPHY_DCTWRITEPROT, DCTWRITEPROT_RELEASE);

    return msg;
}

/* Consume a streaming message: a header with the argument count, then the arguments */
static void consume_streaming_msg(const struct dwc_ddrphy *phy)
{
    uint32_t header = mailbox_get(phy, true);
    unsigned int nargs = header & 0xffff;

    LOG_DBG("PHY stream 0x%08x (%u args)", header, nargs);

    for (unsigned int i = 0; i < nargs; i++) {
        uint32_t arg = mailbox_get(phy, true);

        LOG_DBG("PHY stream arg %u: 0x%08x", i, arg);
        (void)arg;
    }
}

/* Follow the PMU's messages until training ends. Returns true on success. */
static bool wait_training_result(const struct dwc_ddrphy *phy)
{
    for (;;) {
        uint32_t major = mailbox_get(phy, false);

        switch (major) {
        case DDRPHY_MAIL_TRAINING_DONE:
            return true;
        case DDRPHY_MAIL_TRAINING_FAILED:
            return false;
        case DDRPHY_MAIL_STREAMING_MSG:
            consume_streaming_msg(phy);
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

    if (!wait_training_result(phy)) {
        LOG_ERR("PHY training failed at %u MT/s", fsp->drate);
        return -PB_ERR_IO;
    }

    dwc_ddrphy_write(phy, DDRPHY_MICRORESET, MICRORESET_STALL);
    dwc_ddrphy_write(phy, DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);
    dwc_ddrphy_write(phy, DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);

    LOG_DBG("PHY training done, %u MT/s", fsp->drate);
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
