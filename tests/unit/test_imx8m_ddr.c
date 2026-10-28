/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX8M LPDDR4 init (src/plat/imx8m/ddr.c) against a model of the
 * uMCTL2 controller, the DWC DDR PHY and its training firmware mailbox.
 *
 * imx8m_ddr_init() can only be tested on hardware, and the imx8mevk is not in
 * the lab. This test records every register write it makes, plus the timing
 * data it hands over to TF-A, and compares that to a golden trace. Changes
 * to the init code that are meant to be cosmetic must keep the trace
 * identical. A change that is meant to alter the sequence updates the golden
 * trace in the same commit:
 *
 *   PB_UPDATE_GOLDEN=1 build-host/tests/unit/test_imx8m_ddr
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/delay.h>
#include <pb/pb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <plat/imx8m/dram.h>

/* Everything the init code touches lives in 0x3000_0000 - 0x3fff_ffff */
#define DEV_START               0x30000000UL
#define DEV_END                 0x40000000UL

#define DDRC_BASE               0x3d400000UL
#define DDRC_STAT_REG           (DDRC_BASE + 0x004)
#define DDRC_PWRCTL_REG         (DDRC_BASE + 0x030)
#define DDRC_DFISTAT_REG        (DDRC_BASE + 0x1bc)
#define DDRC_SWSTAT_REG         (DDRC_BASE + 0x324)
#define MODEL_PWRCTL_SELFREF_SW (1u << 5)
#define DRAM_PLL_CFG0_REG       0x30360060UL
#define PLL_LOCK                (1u << 31)

#define PHY_BASE                0x3c000000UL
#define PHY_REG(csr)            (PHY_BASE + 4UL * (csr))
#define PHY_UCT_SHADOW_REGS     PHY_REG(0xd0004)
#define PHY_DCT_WRITE_PROT      PHY_REG(0xd0031)
#define PHY_UCT_WRITE_ONLY      PHY_REG(0xd0032)
#define PHY_UCT_DAT_WRITE       PHY_REG(0xd0034)
#define PHY_CAL_BUSY            PHY_REG(0x20097)
#define PHY_IMEM                PHY_REG(0x50000)

#define TRACE_MAX               (1024 * 1024)
#define STORE_SIZE              (1 << 16)

/* Training firmware images, normally linked in with objcopy */
#define FAKE_BLOB(name, a, b, c, d)                                                  \
    __asm__(".section .rodata\n.balign 4\n.globl _binary_blobs_" name "_bin_start\n" \
            "_binary_blobs_" name "_bin_start:\n.long " #a "," #b "," #c "," #d "\n" \
            ".globl _binary_blobs_" name "_bin_end\n_binary_blobs_" name "_bin_end:\n.text\n")

FAKE_BLOB("lpddr4_pmu_train_1d_imem", 0x11111111, 0x01234567, 0x89abcdef, 0xdeadbeef);
FAKE_BLOB("lpddr4_pmu_train_1d_dmem", 0x22222222, 0x76543210, 0xfedcba98, 0xcafef00d);
FAKE_BLOB("lpddr4_pmu_train_2d_imem", 0x33333333, 0x0badc0de, 0x13579bdf, 0x2468ace0);
FAKE_BLOB("lpddr4_pmu_train_2d_dmem", 0x44444444, 0xa5a5a5a5, 0x5a5a5a5a, 0x00ff00ff);

/* Register file: values written, everything else reads as a pattern */
struct reg {
    uintptr_t addr;
    uint32_t val;
    int used;
};

static struct reg store[STORE_SIZE];
static char trace[TRACE_MAX];
static size_t trace_len;

/*
 * Mailbox script for one training run, (UctWriteOnlyShadow,
 * UctDatWriteOnlyShadow) pairs: a streaming message with two arguments,
 * then 'training complete'.
 */
static const uint16_t mail_script[][2] = {
    { 0x0008, 0x0000 }, { 0x0002, 0x00ab }, { 0x1111, 0x2222 },
    { 0x3333, 0x4444 }, { 0x0007, 0x0000 },
};
static size_t mail_pos;
static uint16_t mail_hi;
/* Mail that ends training run n (0-based) instead of 'complete', or 0 */
static uint16_t fail_mail;
static unsigned int fail_run;
/* Firmware readback from the PHY IMEM is corrupted */
static int corrupt_imem;

static void trace_add(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    int n = vsnprintf(&trace[trace_len], TRACE_MAX - trace_len, fmt, ap);
    va_end(ap);
    assert_true(n > 0 && (size_t)n < TRACE_MAX - trace_len);
    trace_len += (size_t)n;
}

static struct reg *reg_lookup(uintptr_t addr, int create)
{
    size_t i = (addr >> 1) % STORE_SIZE;

    for (size_t n = 0; n < STORE_SIZE; n++) {
        struct reg *r = &store[(i + n) % STORE_SIZE];
        if (r->used && r->addr == addr)
            return r;
        if (!r->used) {
            if (!create)
                return NULL;
            r->used = 1;
            r->addr = addr;
            return r;
        }
    }
    fail_msg("register store full");
    return NULL;
}

static uint32_t reg_get(uintptr_t addr)
{
    struct reg *r = reg_lookup(addr, 0);
    /* Unwritten registers, e.g. the trained CSRs, read as a pattern */
    return r ? r->val : (uint32_t)(addr * 2654435761u) & 0xffff;
}

uint64_t fake_mmio_read(uintptr_t addr, unsigned int width)
{
    if (addr < DEV_START || addr >= DEV_END) {
        /* Not a register: the firmware image in RAM */
        switch (width) {
        case 16:
            return *(volatile uint16_t *)addr;
        case 32:
            return *(volatile uint32_t *)addr;
        default:
            fail_msg("unexpected %u bit read of RAM", width);
        }
    }

    switch (addr) {
    case DDRC_STAT_REG:
        /* Self refresh (by software) while selfref_sw is set, else normal */
        return (reg_get(DDRC_PWRCTL_REG) & MODEL_PWRCTL_SELFREF_SW) ? 0x223 : 0x001;
    case DDRC_DFISTAT_REG: /* dfi_init_complete */
    case DDRC_SWSTAT_REG: /* sw_done_ack */
        return 1;
    case PHY_CAL_BUSY:
        return 0;
    case DRAM_PLL_CFG0_REG:
        return reg_get(addr) | PLL_LOCK;
    case PHY_UCT_SHADOW_REGS:
        /* UctWriteProtShadow: 0 = message pending, 1 after the ack (DctWriteProt = 0) */
        return reg_get(PHY_DCT_WRITE_PROT) == 0 ? 1 : 0;
    case PHY_UCT_WRITE_ONLY: {
        size_t run = mail_pos / ARRAY_SIZE(mail_script);
        const uint16_t *m = mail_script[mail_pos++ % ARRAY_SIZE(mail_script)];
        mail_hi = m[1];
        if (m[0] == 0x0007 && fail_mail != 0 && run == fail_run)
            return fail_mail;
        return m[0];
    }
    case PHY_UCT_DAT_WRITE:
        return mail_hi;
    default:
        if (corrupt_imem && addr == PHY_IMEM)
            return reg_get(addr) ^ 1;
        return width == 16 ? reg_get(addr) & 0xffff : reg_get(addr);
    }
}

void fake_mmio_write(uintptr_t addr, uint64_t value, unsigned int width)
{
    assert_true(addr >= DEV_START && addr < DEV_END);
    assert_true(width == 16 || width == 32);

    trace_add("w%u %08lx %08lx\n", width, (unsigned long)addr, (unsigned long)value);
    reg_lookup(addr, 1)->val = (uint32_t)value;
}

void pb_delay_ms(uint32_t delay)
{
    trace_add("delay_ms %u\n", delay);
}

void pb_delay_us(uint32_t delay)
{
    trace_add("delay_us %u\n", delay);
}

/* The copy for TF-A, with the pointers as offsets from the start of the copy */
static void trace_timing_copy(void)
{
    const struct dram_timing_info *dt = &dram_timing_info_copy.info;
    const uint8_t *base = (const uint8_t *)dt;

    const struct {
        const char *name;
        const struct dram_cfg_param *p;
        unsigned int n;
    } tables[] = {
        { "ddrc_cfg", dt->ddrc_cfg, dt->ddrc_cfg_num },
        { "ddrphy_cfg", dt->ddrphy_cfg, dt->ddrphy_cfg_num },
        { "ddrphy_trained_csr", dt->ddrphy_trained_csr, dt->ddrphy_trained_csr_num },
        { "ddrphy_pie", dt->ddrphy_pie, dt->ddrphy_pie_num },
    };

    trace_add("fsp_table %u %u %u %u\n",
              dt->fsp_table[0],
              dt->fsp_table[1],
              dt->fsp_table[2],
              dt->fsp_table[3]);

    for (size_t t = 0; t < ARRAY_SIZE(tables); t++) {
        trace_add("%s +%ld %u\n",
                  tables[t].name,
                  (long)((const uint8_t *)tables[t].p - base),
                  tables[t].n);
        for (unsigned int i = 0; i < tables[t].n; i++)
            trace_add("  %08x %08x\n", tables[t].p[i].reg, tables[t].p[i].val);
    }
}

static char *read_file(const char *path, size_t *len)
{
    FILE *fp = fopen(path, "rb");
    char *buf;

    if (fp == NULL)
        return NULL;
    fseek(fp, 0, SEEK_END);
    *len = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = malloc(*len + 1);
    assert_non_null(buf);
    assert_int_equal(fread(buf, 1, *len, fp), *len);
    buf[*len] = 0;
    fclose(fp);
    return buf;
}

static void report_first_difference(const char *golden, const char *actual)
{
    unsigned int line = 1;
    const char *g = golden, *a = actual;

    while (*g && *g == *a) {
        if (*g == '\n')
            line++;
        g++;
        a++;
    }
    print_error("First difference at line %u of %s\n", line, IMX8M_DDR_GOLDEN);
    print_error("  golden: %.60s\n", g);
    print_error("  actual: %.60s\n", a);
}

static int reset_model(void **state)
{
    (void)state;
    memset(store, 0, sizeof(store));
    trace_len = 0;
    trace[0] = 0;
    mail_pos = 0;
    fail_mail = 0;
    fail_run = 0;
    corrupt_imem = 0;
    return 0;
}

static void test_init_trace(void **state)
{
    (void)state;
    size_t golden_len = 0;
    char *golden;

    int rc = imx8m_ddr_init();
    trace_add("return %d\n", rc);
    trace_timing_copy();

    if (getenv("PB_UPDATE_GOLDEN") != NULL) {
        FILE *fp = fopen(IMX8M_DDR_GOLDEN, "wb");
        assert_non_null(fp);
        assert_int_equal(fwrite(trace, 1, trace_len, fp), trace_len);
        fclose(fp);
        print_message("Wrote %s\n", IMX8M_DDR_GOLDEN);
        return;
    }

    golden = read_file(IMX8M_DDR_GOLDEN, &golden_len);
    assert_non_null(golden);

    if (golden_len != trace_len || memcmp(golden, trace, trace_len) != 0) {
        report_first_difference(golden, trace);
        free(golden);
        fail_msg("Register trace differs from %s", IMX8M_DDR_GOLDEN);
    }

    free(golden);
}

/* The firmware reports a failed training: stop there */
static void test_training_failed(void **state)
{
    (void)state;

    for (unsigned int run = 0; run < 3; run++) {
        reset_model(NULL);
        fail_mail = 0x00ff;
        fail_run = run;
        assert_int_equal(imx8m_ddr_init(), -PB_ERR_IO);
        /* Training run n + 1 is never started */
        assert_int_equal(mail_pos, (run + 1) * ARRAY_SIZE(mail_script));
    }
}

/* The firmware image does not read back from the PHY as written */
static void test_firmware_verify_failed(void **state)
{
    (void)state;

    corrupt_imem = 1;
    assert_int_equal(imx8m_ddr_init(), -PB_ERR_IO);
    /* Training is never started */
    assert_int_equal(mail_pos, 0);
}

/* A training run at a data rate the DRAM PLL has no setting for */
static void test_unsupported_data_rate(void **state)
{
    (void)state;
    unsigned int drate = dram_timing.fsp_msg[1].drate;

    dram_timing.fsp_msg[1].drate = 1234;
    assert_int_equal(imx8m_ddr_init(), -PB_ERR_NOT_SUPPORTED);
    dram_timing.fsp_msg[1].drate = drate;
    /* Only the first training run happened */
    assert_int_equal(mail_pos, ARRAY_SIZE(mail_script));
}

/* Board tables that don't fit in the TF-A copy are refused, not copied past its end */
static void test_timing_copy_too_large(void **state)
{
    (void)state;
    static struct dram_cfg_param pie[ARRAY_SIZE(dram_timing_info_copy.cfg) + 1];
    struct dram_cfg_param *saved_pie = dram_timing.ddrphy_pie;
    unsigned int saved_pie_num = dram_timing.ddrphy_pie_num;
    unsigned int others =
        dram_timing.ddrc_cfg_num + dram_timing.ddrphy_cfg_num + dram_timing.ddrphy_trained_csr_num;

    for (size_t i = 0; i < ARRAY_SIZE(pie); i++)
        pie[i].reg = 0x90000;

    /* One entry more than fits */
    dram_timing.ddrphy_pie = pie;
    dram_timing.ddrphy_pie_num = ARRAY_SIZE(dram_timing_info_copy.cfg) + 1 - others;
    assert_int_equal(imx8m_ddr_init(), -PB_ERR_MEM);
    dram_timing.ddrphy_pie = saved_pie;
    dram_timing.ddrphy_pie_num = saved_pie_num;
}

/* The PHY driver refuses an invalid firmware type or a missing image before touching the PHY */
static void test_phy_bad_firmware(void **state)
{
    (void)state;
    static const uint32_t word = 0x12345678;
    const struct dwc_ddrphy_fw fw = { &word, &word + 1, &word, &word + 1 };
    const struct dwc_ddrphy no_2d = {
        .base = PHY_BASE,
        .csr_stride = 4,
        .fw = { [FW_1D_IMAGE] = &fw, [FW_2D_IMAGE] = NULL },
    };
    struct dram_fsp_msg fsp = { .drate = 3200, .fw_type = FW_2D_IMAGE };

    assert_int_equal(dwc_ddrphy_train(&no_2d, &fsp), -PB_ERR_PARAM);
    fsp.fw_type = (enum fw_type)7;
    assert_int_equal(dwc_ddrphy_train(&no_2d, &fsp), -PB_ERR_PARAM);
    assert_int_equal(trace_len, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_init_trace, reset_model),
        cmocka_unit_test_setup(test_training_failed, reset_model),
        cmocka_unit_test_setup(test_firmware_verify_failed, reset_model),
        cmocka_unit_test_setup(test_unsupported_data_rate, reset_model),
        cmocka_unit_test_setup(test_timing_copy_too_large, reset_model),
        cmocka_unit_test_setup(test_phy_bad_firmware, reset_model),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
