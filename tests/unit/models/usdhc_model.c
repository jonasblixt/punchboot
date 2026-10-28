/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * See usdhc_model.h. Violations cite the register or table of the uSDHC
 * chapter; it has the same layout in the i.MX6UL and i.MX8M RMs.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <stdio.h>
#include <string.h>

#include "bench.h"
#include "usdhc_model.h"

#define RM                    "uSDHC RM "

#define BLK_ATT               0x04
#define CMD_ARG               0x08
#define CMD_XFR_TYP           0x0c
#define CMD_RSP0              0x10
#define CMD_RSP1              0x14
#define CMD_RSP2              0x18
#define CMD_RSP3              0x1c
#define PRES_STATE            0x24
#define PROT_CTRL             0x28
#define SYS_CTRL              0x2c
#define INT_STATUS            0x30
#define INT_STATUS_EN         0x34
#define INT_SIGNAL_EN         0x38
#define AUTOCMD12_ERR_STATUS  0x3c
#define HOST_CTRL_CAP         0x40
#define WTMK_LVL              0x44
#define MIX_CTRL              0x48
#define ADMA_ERR_STATUS       0x54
#define ADMA_SYS_ADDR         0x58
#define DLL_CTRL              0x60
#define DLL_STATUS            0x64
#define STROBE_DLL_STATUS     0x74
#define VEND_SPEC             0xc0
#define VEND_SPEC2            0xc8
#define TUNING_CTRL           0xcc
#define REG_SPACE             0x160

/* BLK_ATT */
#define BLKCNT(v)             ((v) >> 16)
#define BLKSIZE(v)            ((v) & 0x1fff)

/* CMD_XFR_TYP */
#define XFR_CMDINX(v)         (((v) >> 24) & 0x3f)
#define XFR_CMDTYP(v)         (((v) >> 22) & 3)
#define XFR_DPSEL             (1u << 21)
#define XFR_CICEN             (1u << 20)
#define XFR_CCCEN             (1u << 19)
#define XFR_RSPTYP(v)         (((v) >> 16) & 3)
#define RSPTYP_NONE           0
#define RSPTYP_136            1
#define RSPTYP_48             2
#define RSPTYP_48_BUSY        3

/* PRES_STATE */
#define PS_DAT0               (1u << 24)
#define PS_DLSL_MASK          0xff000000u
#define PS_CLSL               (1u << 23)
#define PS_WPSPL              (1u << 19)
#define PS_CDPL               (1u << 18)
#define PS_CINST              (1u << 16)
#define PS_TSCD               (1u << 15)
#define PS_SDOFF              (1u << 7)
#define PS_SDSTB              (1u << 3)
#define PS_DLA                (1u << 2)
#define PS_CDIHB              (1u << 1)
#define PS_CIHB               (1u << 0)

/* PROT_CTRL */
#define PROT_RSVD_23_21       (7u << 21)
#define PROT_RSVD_23_21_VALUE (4u << 21) /* "Always write as 100" */
#define PROT_DMASEL(v)        (((v) >> 8) & 3)
#define DMASEL_ADMA2          2
#define PROT_EMODE(v)         (((v) >> 4) & 3)
#define EMODE_LE              2
#define PROT_DTW(v)           (((v) >> 1) & 3)

/* SYS_CTRL */
#define SYS_RSTT              (1u << 28)
#define SYS_INITA             (1u << 27)
#define SYS_RSTD              (1u << 26)
#define SYS_RSTC              (1u << 25)
#define SYS_RSTA              (1u << 24)
#define SYS_DTOCV(v)          (((v) >> 16) & 0xf)
#define SYS_SDCLKFS(v)        (((v) >> 8) & 0xff)
#define SYS_DVS(v)            (((v) >> 4) & 0xf)
#define SYS_CLK_MASK          0x0000fff0u
#define SYS_RSVD_3_0          0xfu /* "Always write as 1" */

/* INT_STATUS */
#define INT_DMAE              (1u << 28)
#define INT_AC12E             (1u << 24)
#define INT_DCE               (1u << 21)
#define INT_DTOE              (1u << 20)
#define INT_CIE               (1u << 19)
#define INT_CCE               (1u << 17)
#define INT_CTOE              (1u << 16)
#define INT_TC                (1u << 1)
#define INT_CC                (1u << 0)

/* AUTOCMD12_ERR_STATUS */
#define AC12_IE               (1u << 4)
#define AC12_CE               (1u << 3)
#define AC12_TOE              (1u << 1)
#define AC12_NE               (1u << 0)

/* MIX_CTRL */
#define MIX_RSVD_31           (1u << 31) /* "Always write as 1" */
#define MIX_AC23EN            (1u << 7)
#define MIX_MSBSEL            (1u << 5)
#define MIX_DTDSEL            (1u << 4)
#define MIX_DDR_EN            (1u << 3)
#define MIX_AC12EN            (1u << 2)
#define MIX_BCEN              (1u << 1)
#define MIX_DMAEN             (1u << 0)

/* ADMA_ERR_STATUS */
#define ADMA_DCE              (1u << 3)
#define ADMA_LME              (1u << 2)

/* VEND_SPEC */
#define VS_RSVD_29            (1u << 29) /* Write as 1 */
#define VS_CARD_CLK_SOFT_EN   (1u << 14) /* i.MX6UL */
#define VS_SOFT_EN_MASK       (0xfu << 11) /* Reserved on the i.MX8M */
#define VS_FRC_SDCLK_ON       (1u << 8)
#define VS_AC12_WR_CHKBUSY_EN (1u << 3)
#define VS_VSELECT            (1u << 1)

/* ADMA2 descriptor attributes */
#define ADMA2_VALID           (1u << 0)
#define ADMA2_END             (1u << 1)
#define ADMA2_ACT(a)          (((a) >> 4) & 3)
#define ACT_TRAN              2
#define ACT_LINK              3
#define ADMA2_MAX_DESCS       4096

/* The SDSTB low time after a divider change, in register reads */
#define SDSTB_READS           3

struct adma_seg {
    uint8_t *ptr;
    size_t len;
};

static struct {
    struct usdhc_model_config cfg;
    uint32_t regs[REG_SPACE / 4];
    struct bus_cfg bus;
    unsigned int sdstb_wait;
    bool inita_done;
    bool cmd_seen;
    bool busy_pending; /* TC follows when DAT0 is released */
    bool data_pending; /* ... after a data transfer */
    uint64_t busy_deadline_us;
    unsigned int cmds;
    bool fail_dma;
    struct adma_seg segs[ADMA2_MAX_DESCS];
} m;

static uint32_t reg(unsigned int off)
{
    return m.regs[off / 4];
}

static void set_reg(unsigned int off, uint32_t v)
{
    m.regs[off / 4] = v;
}

static void latch(uint32_t bits)
{
    /* A status bit is set only if its INT_STATUS_EN bit is */
    m.regs[INT_STATUS / 4] |= bits & reg(INT_STATUS_EN);
}

static void reset_all(void)
{
    bool imx6 = m.cfg.soc == USDHC_IMX6UL;

    memset(m.regs, 0, sizeof(m.regs));
    set_reg(BLK_ATT, 0x00010000);
    set_reg(PROT_CTRL, 0x08800020);
    set_reg(SYS_CTRL, imx6 ? 0x8080800f : 0x0080800f);
    set_reg(HOST_CTRL_CAP, 0x07f3b407);
    set_reg(WTMK_LVL, 0x08100810);
    set_reg(MIX_CTRL, 0x80000000);
    set_reg(DLL_CTRL, imx6 ? 0x00000200 : 0);
    set_reg(DLL_STATUS, imx6 ? 0 : 0x00000200);
    if (!imx6)
        set_reg(STROBE_DLL_STATUS, 0x00000200);
    set_reg(VEND_SPEC, imx6 ? 0x20007809 : 0x30007809);
    set_reg(VEND_SPEC2, imx6 ? 0x00000006 : 0x00019006);
    set_reg(TUNING_CTRL, 0x00212800);
    m.busy_pending = false;
    m.data_pending = false;
    m.sdstb_wait = 0;
}

/* The prescaler, SYS_CTRL.SDCLKFS: one hot, doubled when DDR_EN is set */
static unsigned int prescaler(void)
{
    unsigned int fs = SYS_SDCLKFS(reg(SYS_CTRL));
    unsigned int p = 1;

    if (fs != 0) {
        while (!(fs & 1)) {
            fs >>= 1;
            p *= 2;
        }
        p *= 2;
    }
    if (reg(MIX_CTRL) & MIX_DDR_EN)
        p *= 2;
    return p;
}

static bool clock_gated(void)
{
    /* i.MX6UL: VEND_SPEC.CARD_CLK_SOFT_EN gates the card clock */
    return m.cfg.soc == USDHC_IMX6UL && !(reg(VEND_SPEC) & VS_CARD_CLK_SOFT_EN);
}

static void update_bus(void)
{
    static const unsigned int widths[] = { 1, 4, 8, 8 };
    struct bus_cfg b = { 0 };
    unsigned int div = prescaler() * (SYS_DVS(reg(SYS_CTRL)) + 1);

    b.clk_hz = clock_gated() ? 0 : m.cfg.base_clk_hz / div;
    b.width = widths[PROT_DTW(reg(PROT_CTRL))];
    b.ddr = reg(MIX_CTRL) & MIX_DDR_EN;
    b.vdd_mv = 3300;
    if (m.cfg.vselect_wired)
        b.vccq_mv = (reg(VEND_SPEC) & VS_VSELECT) ? 1800 : 3300;
    else
        b.vccq_mv = m.cfg.vccq_mv;

    if (memcmp(&b, &m.bus, sizeof(b)) != 0) {
        m.bus = b;
        m.cfg.card->ops->set_bus(m.cfg.card, &m.bus);
    }
}

static bool card_busy(void)
{
    return m.cfg.card->ops->busy(m.cfg.card);
}

/* Data timeout from SYS_CTRL.DTOCV, in SD clocks */
static uint64_t data_timeout_clocks(void)
{
    unsigned int n = SYS_DTOCV(reg(SYS_CTRL));

    if (m.cfg.soc == USDHC_IMX6UL)
        return 1ULL << (13 + n);
    if (n < 2)
        return 1ULL << (32 + n);
    return 1ULL << (16 + n);
}

static void start_busy(bool data)
{
    uint64_t clk = m.bus.clk_hz ? m.bus.clk_hz : 1;

    m.busy_pending = true;
    m.data_pending = data;
    m.busy_deadline_us = bench_now_us() + data_timeout_clocks() * 1000000ULL / clk;
}

/* TC once the card releases DAT0, DTOE if it takes longer than DTOCV */
static void update_busy(void)
{
    if (!m.busy_pending)
        return;

    if (!card_busy()) {
        m.busy_pending = false;
        m.data_pending = false;
        latch(INT_TC);
    } else if (bench_now_us() > m.busy_deadline_us) {
        m.busy_pending = false;
        m.data_pending = false;
        latch(INT_DTOE);
    }
}

static uint32_t pres_state(void)
{
    uint32_t v = PS_CLSL | PS_TSCD | (PS_DLSL_MASK & ~PS_DAT0);
    bool present = m.cfg.card->ops->present(m.cfg.card);

    update_busy();
    if (!card_busy())
        v |= PS_DAT0;
    if (m.busy_pending)
        v |= PS_DLA | PS_CDIHB;
    if (m.sdstb_wait == 0)
        v |= PS_SDSTB;
    else
        m.sdstb_wait--;
    if (present)
        v |= PS_CINST | PS_CDPL;
    if (!m.cfg.write_protect)
        v |= PS_WPSPL;
    if (!(reg(VEND_SPEC) & VS_FRC_SDCLK_ON))
        v |= PS_SDOFF;
    return v;
}

static void write_sys_ctrl(uint32_t v)
{
    uint32_t old = reg(SYS_CTRL);

    if ((v & SYS_RSVD_3_0) != SYS_RSVD_3_0)
        bench_violation(
            RM "SYS_CTRL", "Bits 3:0 written as 0x%x, they are always written as 1", v & 0xf);

    if (v & SYS_RSTA) {
        if (m.data_pending)
            bench_violation(RM "SYS_CTRL", "RSTA during a data transfer");
        reset_all();
        update_bus();
        return;
    }
    if (v & SYS_RSTC)
        m.regs[INT_STATUS / 4] &= ~INT_CC;
    if (v & SYS_RSTD) {
        if (m.data_pending)
            bench_violation(RM "SYS_CTRL", "RSTD during a data transfer");
        m.busy_pending = false;
        m.data_pending = false;
        m.regs[INT_STATUS / 4] &= ~INT_TC;
    }
    if (v & SYS_INITA) {
        if (m.busy_pending) {
            bench_violation(RM "SYS_CTRL", "INITA while CDIHB is set, it is ignored");
        } else {
            m.inita_done = true;
            if (m.bus.clk_hz > 400000)
                bench_violation(RM "init",
                                "INITA at %u Hz, the 80 clocks are sent at about 400 kHz",
                                m.bus.clk_hz);
        }
    }

    if ((old ^ v) & SYS_CLK_MASK) {
        unsigned int fs = SYS_SDCLKFS(v);

        if (m.sdstb_wait != 0)
            bench_violation(RM "PRES_STATE", "SDCLKFS/DVS changed before SDSTB was set");
        if (reg(VEND_SPEC) & VS_FRC_SDCLK_ON)
            bench_violation(RM "VEND_SPEC", "SDCLKFS/DVS changed with FRC_SDCLK_ON set");
        if (fs & (fs - 1))
            bench_violation(RM "SYS_CTRL", "SDCLKFS 0x%02x has more than one bit set", fs);
        m.sdstb_wait = SDSTB_READS;
    }

    /* The reset and INITA bits clear themselves */
    set_reg(SYS_CTRL, v & ~(SYS_RSTT | SYS_INITA | SYS_RSTD | SYS_RSTC | SYS_RSTA));
    update_bus();
}

static void write_prot_ctrl(uint32_t v)
{
    if ((v & PROT_RSVD_23_21) != PROT_RSVD_23_21_VALUE)
        bench_violation(RM "PROT_CTRL",
                        "Bits 23:21 written as %u, they are always written as 100b",
                        (v >> 21) & 7);
    if (PROT_EMODE(v) != EMODE_LE)
        bench_violation(
            RM "PROT_CTRL", "EMODE %u, the model only does little endian", PROT_EMODE(v));
    if (PROT_DTW(v) == 3)
        bench_violation(RM "PROT_CTRL", "DTW 11b is reserved");
    set_reg(PROT_CTRL, v);
    update_bus();
}

static void write_mix_ctrl(uint32_t v)
{
    if (!(v & MIX_RSVD_31))
        bench_violation(RM "MIX_CTRL", "Bit 31 written as 0, it is always written as 1");
    if (m.data_pending)
        bench_violation(RM "MIX_CTRL", "Written during a data transfer");
    set_reg(MIX_CTRL, v);
    update_bus();
}

static void write_vend_spec(uint32_t v)
{
    if (!(v & VS_RSVD_29))
        bench_violation(RM "VEND_SPEC", "Bit 29 written as 0, it is written as 1");
    if (m.cfg.soc == USDHC_IMX8M && (v & VS_SOFT_EN_MASK) != VS_SOFT_EN_MASK)
        bench_violation(RM "VEND_SPEC", "Reserved bits 14:11 changed to 0x%x", (v >> 11) & 0xf);
    set_reg(VEND_SPEC, v);
    update_bus();
}

/* Collects the ADMA2 descriptor list. Returns the total length, -1 on a descriptor error */
static long adma_walk(size_t *n_segs)
{
    uint32_t addr = reg(ADMA_SYS_ADDR);
    long total = 0;
    size_t n = 0;

    for (int i = 0; i < ADMA2_MAX_DESCS; i++) {
        const uint32_t *d = (const uint32_t *)(uintptr_t)addr;
        uint32_t attr;
        uint32_t len;

        if (addr == 0 || (addr & 3)) {
            bench_violation(RM "ADMA_SYS_ADDR", "Descriptor address 0x%08x", addr);
            return -1;
        }
        attr = d[0] & 0xffff;
        len = d[0] >> 16;

        if (!(attr & ADMA2_VALID)) {
            set_reg(ADMA_ERR_STATUS, ADMA_DCE);
            return -1;
        }

        if (ADMA2_ACT(attr) == ACT_TRAN) {
            if (len == 0)
                bench_violation(RM "ADMA2", "Descriptor length 0, its meaning is not defined");
            if ((d[1] & 3) || (len & 3))
                bench_violation(RM "ADMA2",
                                "Descriptor 0x%08x + %u is not word aligned and a multiple of 4",
                                d[1],
                                len);
            m.segs[n].ptr = (uint8_t *)(uintptr_t)d[1];
            m.segs[n].len = len;
            total += len;
            n++;
        }

        if (attr & ADMA2_END) {
            *n_segs = n;
            return total;
        }
        addr = ADMA2_ACT(attr) == ACT_LINK ? d[1] : addr + 8;
    }

    fail_msg("ADMA2 descriptor list without an end");
    return -1;
}

/* Copies between a block and the scatter list at byte offset 'off' */
static void seg_copy(size_t n_segs, size_t off, void *block, size_t len, bool to_mem)
{
    uint8_t *b = block;

    for (size_t i = 0; i < n_segs && len > 0; i++) {
        if (off >= m.segs[i].len) {
            off -= m.segs[i].len;
            continue;
        }
        size_t chunk = m.segs[i].len - off;

        if (chunk > len)
            chunk = len;
        if (to_mem)
            memcpy(m.segs[i].ptr + off, b, chunk);
        else
            memcpy(b, m.segs[i].ptr + off, chunk);
        b += chunk;
        len -= chunk;
        off = 0;
    }
}

static enum bus_rsp_type bus_rsp(unsigned int rsptyp)
{
    switch (rsptyp) {
    case RSPTYP_136:
        return BUS_RSP_136;
    case RSPTYP_48:
        return BUS_RSP_48;
    case RSPTYP_48_BUSY:
        return BUS_RSP_48_BUSY;
    default:
        return BUS_RSP_NONE;
    }
}

static void store_response(const struct bus_rsp *rsp, unsigned int rsptyp)
{
    if (rsptyp == RSPTYP_136) {
        /* R[127:8] in {RSP3[23:0], RSP2, RSP1, RSP0} */
        set_reg(CMD_RSP0, (rsp->r[0] >> 8) | (rsp->r[1] << 24));
        set_reg(CMD_RSP1, (rsp->r[1] >> 8) | (rsp->r[2] << 24));
        set_reg(CMD_RSP2, (rsp->r[2] >> 8) | (rsp->r[3] << 24));
        set_reg(CMD_RSP3, rsp->r[3] >> 8);
    } else {
        set_reg(CMD_RSP0, rsp->r[0]);
    }
}

/* Auto CMD12 after the last block, the response goes to CMD_RSP3 */
static void auto_cmd12(bool write)
{
    struct bus_cmd cmd = {
        .idx = 12,
        .rsp = write && (reg(VEND_SPEC) & VS_AC12_WR_CHKBUSY_EN) ? BUS_RSP_48_BUSY : BUS_RSP_48,
    };
    struct bus_rsp rsp;

    /* The status of the last auto CMD12 */
    set_reg(AUTOCMD12_ERR_STATUS, 0);
    m.cmds++;
    m.cfg.card->ops->cmd(m.cfg.card, &cmd, &rsp);
    if (rsp.timeout) {
        set_reg(AUTOCMD12_ERR_STATUS, reg(AUTOCMD12_ERR_STATUS) | AC12_TOE);
        latch(INT_AC12E);
        return;
    }
    if (rsp.crc_error)
        set_reg(AUTOCMD12_ERR_STATUS, reg(AUTOCMD12_ERR_STATUS) | AC12_CE);
    if (rsp.idx != 12)
        set_reg(AUTOCMD12_ERR_STATUS, reg(AUTOCMD12_ERR_STATUS) | AC12_IE);
    if (reg(AUTOCMD12_ERR_STATUS))
        latch(INT_AC12E);
    set_reg(CMD_RSP3, rsp.r[0]);
}

static void run_data(void)
{
    uint32_t mix = reg(MIX_CTRL);
    uint32_t blk = reg(BLK_ATT);
    bool read = mix & MIX_DTDSEL;
    bool multi = mix & MIX_MSBSEL;
    size_t bs = BLKSIZE(blk);
    size_t n_segs = 0;
    long total;
    size_t blocks;
    static uint8_t block[4096];

    if (!(mix & MIX_DMAEN) || PROT_DMASEL(reg(PROT_CTRL)) != DMASEL_ADMA2) {
        bench_violation(RM "MIX_CTRL", "Data command without ADMA2, the model only does ADMA2");
        return;
    }

    if (m.fail_dma) {
        m.fail_dma = false;
        set_reg(ADMA_ERR_STATUS, ADMA_DCE);
        latch(INT_DMAE);
        return;
    }

    total = adma_walk(&n_segs);
    if (total < 0) {
        latch(INT_DMAE);
        return;
    }

    if (!multi)
        blocks = 1;
    else if (mix & MIX_BCEN)
        blocks = BLKCNT(blk);
    else
        blocks = (size_t)total / bs;

    if ((size_t)total != blocks * bs) {
        set_reg(ADMA_ERR_STATUS, ADMA_LME);
        bench_violation(RM "ADMA_ERR_STATUS",
                        "Descriptors hold %ld bytes, the transfer is %zu x %zu",
                        total,
                        blocks,
                        bs);
        latch(INT_DMAE);
        return;
    }

    for (size_t b = 0; b < blocks; b++) {
        enum bus_data_status st;

        if (read) {
            st = m.cfg.card->ops->read(m.cfg.card, block, bs);
            if (st == BUS_DATA_OK)
                seg_copy(n_segs, b * bs, block, bs, true);
        } else {
            /* The controller waits for DAT0 before the next block */
            while (card_busy())
                bench_advance_us(1);
            seg_copy(n_segs, b * bs, block, bs, false);
            st = m.cfg.card->ops->write(m.cfg.card, block, bs);
        }

        if (st != BUS_DATA_OK) {
            latch(st == BUS_DATA_CRC ? INT_DCE : INT_DTOE);
            if (multi && (mix & MIX_AC12EN)) {
                /* An error before the last block: no auto CMD12 */
                set_reg(AUTOCMD12_ERR_STATUS, AC12_NE);
                latch(INT_AC12E);
            }
            return;
        }
    }

    if (multi && (mix & MIX_AC12EN))
        auto_cmd12(!read);

    if (read)
        latch(INT_TC);
    else
        start_busy(true);
}

static void run_command(uint32_t xfr)
{
    unsigned int idx = XFR_CMDINX(xfr);
    unsigned int rsptyp = XFR_RSPTYP(xfr);
    bool data = xfr & XFR_DPSEL;
    struct bus_cmd cmd = { .idx = idx, .arg = reg(CMD_ARG), .rsp = bus_rsp(rsptyp) };
    struct bus_rsp rsp;

    update_busy();
    set_reg(CMD_XFR_TYP, xfr);

    if (!m.inita_done && !m.cmd_seen)
        bench_violation(JESD84 "10.1", "First command without the initialization clocks (INITA)");
    m.cmd_seen = true;

    if (data && m.busy_pending) {
        bench_violation(
            RM "CMD_XFR_TYP", "Data command CMD%u while CDIHB is set, it is ignored", idx);
        return;
    }
    if (data && BLKSIZE(reg(BLK_ATT)) == 0) {
        bench_violation(RM "CMD_XFR_TYP", "Data command CMD%u with BLKSIZE 0, it is ignored", idx);
        return;
    }
    if (data && !(reg(MIX_CTRL) & MIX_DTDSEL) && m.cfg.write_protect)
        return; /* Write protected, dropped */

    if (m.bus.clk_hz == 0) {
        latch(INT_CTOE);
        return;
    }

    m.cmds++;
    m.cfg.card->ops->cmd(m.cfg.card, &cmd, &rsp);

    if (rsptyp == RSPTYP_NONE) {
        latch(INT_CC);
        return;
    }
    if (rsp.timeout) {
        latch(INT_CTOE);
        return;
    }

    /* R3 has no CRC and no index, they read as all ones */
    bool r3 = rsptyp == RSPTYP_48 && rsp.idx == 0x3f;

    if ((xfr & XFR_CCCEN) && (rsp.crc_error || r3))
        latch(INT_CCE);
    if ((xfr & XFR_CICEN) && rsp.idx != idx)
        latch(INT_CIE);

    /* Table 10-47: R1, R1b, R5, R6, R7 check both, R2 the CRC only */
    if (rsptyp != RSPTYP_136 && !r3 && (xfr & (XFR_CICEN | XFR_CCCEN)) != (XFR_CICEN | XFR_CCCEN))
        bench_violation(RM "Table 10-47", "CMD%u response without index and CRC checks", idx);
    if (rsptyp == RSPTYP_136 && !(xfr & XFR_CCCEN))
        bench_violation(RM "Table 10-47", "CMD%u R2 response without the CRC check", idx);

    store_response(&rsp, rsptyp);
    latch(INT_CC);

    if (data)
        run_data();
    else if (rsptyp == RSPTYP_48_BUSY)
        start_busy(false);
}

void usdhc_model_init(const struct usdhc_model_config *cfg)
{
    memset(&m, 0, sizeof(m));
    m.cfg = *cfg;
    if (m.cfg.vccq_mv == 0)
        m.cfg.vccq_mv = 3300;
    reset_all();
    update_bus();
}

bool usdhc_model_owns(uintptr_t addr)
{
    return addr >= m.cfg.base && addr < m.cfg.base + REG_SPACE;
}

uint32_t usdhc_model_read(uintptr_t addr)
{
    unsigned int off = addr - m.cfg.base;

    bench_advance_us(1);
    bench_check_stuck("polling the uSDHC");

    switch (off) {
    case PRES_STATE:
        return pres_state();
    case INT_STATUS:
        update_busy();
        return reg(INT_STATUS);
    default:
        return reg(off);
    }
}

void usdhc_model_write(uintptr_t addr, uint32_t value)
{
    unsigned int off = addr - m.cfg.base;

    switch (off) {
    case INT_STATUS:
        update_busy();
        m.regs[INT_STATUS / 4] &= ~value;
        break;
    case SYS_CTRL:
        write_sys_ctrl(value);
        break;
    case PROT_CTRL:
        write_prot_ctrl(value);
        break;
    case MIX_CTRL:
        write_mix_ctrl(value);
        break;
    case VEND_SPEC:
        write_vend_spec(value);
        break;
    case CMD_XFR_TYP:
        run_command(value);
        break;
    case BLK_ATT:
        if (m.data_pending)
            break; /* Ignored during a transfer */
        set_reg(BLK_ATT, value);
        break;
    case WTMK_LVL:
        if ((value & 0xff) == 0 || (value & 0xff) > 128 || ((value >> 16) & 0xff) == 0 ||
            ((value >> 16) & 0xff) > 128)
            bench_violation(RM "WTMK_LVL", "Watermark 0x%08x out of 1..128", value);
        set_reg(WTMK_LVL, value);
        break;
    case PRES_STATE:
    case CMD_RSP0:
    case CMD_RSP1:
    case CMD_RSP2:
    case CMD_RSP3:
    case HOST_CTRL_CAP:
    case ADMA_ERR_STATUS:
    case DLL_STATUS:
    case STROBE_DLL_STATUS:
        break;
    default:
        set_reg(off, value);
        break;
    }
}

const struct bus_cfg *usdhc_model_bus(void)
{
    return &m.bus;
}

unsigned int usdhc_model_cmds(void)
{
    return m.cmds;
}

void usdhc_model_fail_dma(void)
{
    m.fail_dma = true;
}
