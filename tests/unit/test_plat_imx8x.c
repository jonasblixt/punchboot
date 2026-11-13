/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8X port (src/plat/imx8x/plat.c) against the platform contract
 * (doc/interfaces/platform.md), with the SoC's registers in a register file
 * (fakes/regs.h). The SCFW is a model behind its messaging unit: it takes
 * the RPC messages that the port's SCFW client (src/plat/imx8x/sci) writes
 * and answers them. The GPT's driver is the real one; the fuses, the RoT
 * and the SLC helpers and the MMU are fakes.
 *
 * The port keeps the SCFW's handle in a static: the tests of the time before
 * plat_early_init() run first.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/console.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <plat/imx8x/sci/rpc.h>
#include <plat/imx8x/sci/svc/pm/api.h>
#include <string.h>

#include "fakes/regs.h"
#include "fakes/xlat.h"

/* The messaging unit, the port's side (src/plat/imx8x/sci/imx8_mu.h) */
#define MU_ATR(n)              (SC_IPC_BASE + 0x00 + 4 * (n))
#define MU_ARR(n)              (SC_IPC_BASE + 0x10 + 4 * (n))
#define MU_ASR                 (SC_IPC_BASE + 0x20)
#define MU_ACR                 (SC_IPC_BASE + 0x24)
#define MU_ASR_TE_ALL          (0xfu << 20)
#define MU_ASR_RF_ALL          (0xfu << 24)

/* The functions' indexes (src/plat/imx8x/sci/svc/pm/rpc.h) */
#define PM_FUNC_SET_CLOCK_RATE 5
#define PM_FUNC_RESET_REASON   10
#define PM_FUNC_RESET          13

/* The GPT's control register and its software reset */
#define GPT_CR                 0x0
#define GPT_CR_SWR             BIT(15)

#define MAX_CALLS              64

static struct {
    /* The message that is being written */
    uint32_t words[SC_RPC_MAX_MSG];
    unsigned int n_words;

    /* Every message the SCFW got */
    struct {
        uint8_t svc;
        uint8_t func;
        uint32_t arg; /* The first data word */
        size_t at; /* The register log's length when it was complete */
    } calls[MAX_CALLS];

    size_t n_calls;
    uint8_t reset_reason;
    uint8_t reset_reason_err; /* The SCFW's error for the question, 0: none */
} scfw;

/* A reset leaves the code under test through here */
static jmp_buf soc_reset;

void imx8x_fuse_init(sc_ipc_t ipc)
{
    (void)ipc;
}

void imx8x_rot_helpers_init(sc_ipc_t ipc)
{
    (void)ipc;
}

void imx8x_slc_helpers_init(sc_ipc_t ipc)
{
    (void)ipc;
}

void log_time_start(void)
{
}

void log_prefix(const char *level, const char *func)
{
    (void)level;
    (void)func;
}

/* The SCFW got a whole message: note it, act and answer. The answer's header
 * has the error in the place of the function (0, SC_ERR_NONE) and is
 * followed by three data words */
static void scfw_call(void)
{
    uint8_t svc = (scfw.words[0] >> 16) & 0xff;
    uint8_t func = (scfw.words[0] >> 24) & 0xff;
    uint32_t data = 0;
    uint32_t err = SC_ERR_NONE;

    assert_true(scfw.n_calls < MAX_CALLS);
    scfw.calls[scfw.n_calls].svc = svc;
    scfw.calls[scfw.n_calls].func = func;
    scfw.calls[scfw.n_calls].arg = scfw.words[1];
    scfw.calls[scfw.n_calls].at = fake_reg_log_len;
    scfw.n_calls++;
    scfw.n_words = 0;

    if (svc == SC_RPC_SVC_PM && func == PM_FUNC_RESET)
        longjmp(soc_reset, 1);

    if (svc == SC_RPC_SVC_PM && func == PM_FUNC_SET_CLOCK_RATE)
        data = scfw.words[1]; /* The rate asked for */
    else if (svc == SC_RPC_SVC_PM && func == PM_FUNC_RESET_REASON) {
        data = scfw.reset_reason;
        err = scfw.reset_reason_err;
    }

    fake_regs_set(MU_ARR(0), (err << 24) | (SC_RPC_SVC_RETURN << 16) | (4 << 8) | SC_RPC_VERSION);
    fake_regs_set(MU_ARR(1), data);
    fake_regs_set(MU_ARR(2), 0);
    fake_regs_set(MU_ARR(3), 0);
}

/* What the SoC does with a write */
static uint64_t soc_on_write(uintptr_t addr, uint64_t value)
{
    if (addr >= MU_ATR(0) && addr <= MU_ATR(3)) {
        assert_true(scfw.n_words < SC_RPC_MAX_MSG);
        scfw.words[scfw.n_words++] = (uint32_t)value;

        /* The size, in words, is the header's second byte */
        if (scfw.n_words == ((scfw.words[0] >> 8) & 0xff))
            scfw_call();
    }

    if (addr == IMX_GPT_BASE + GPT_CR)
        return value & ~GPT_CR_SWR;

    return value;
}

static int setup(void **state)
{
    (void)state;
    memset(&scfw, 0, sizeof(scfw));
    fake_regs_reset();
    fake_regs_on_write = soc_on_write;
    /* The unit can always send, and an answer is always there */
    fake_regs_set(MU_ASR, MU_ASR_TE_ALL | MU_ASR_RF_ALL);
    fake_mmu_enables = 0;
    return 0;
}

/* platform.md 3.3: plat_wdog_kick() is safe at any time, also before the
 * watchdog is armed. Before plat_early_init() there is no channel to the
 * SCFW: nothing is sent and nothing is waited for. Runs first */
static void test_wdog_kick_before_early_init(void **state)
{
    (void)state;

    plat_wdog_kick();
    assert_int_equal(scfw.n_calls, 0);
    assert_int_equal(fake_reg_log_len, 0);
}

/* platform.md 3.1: plat_reset() is valid from the first instruction after
 * arch_init(), also before plat_early_init(): the arch's exception handlers
 * call it. The 8X resets through the SCFW, whose channel plat_early_init()
 * opens. Runs before any plat_early_init() */
static void test_plat_reset_before_early_init(void **state)
{
    (void)state;

    if (setjmp(soc_reset) == 0) {
        plat_reset();
        fail_msg("plat_reset() returned");
    }

    /* One message: a board reset. The messaging unit was set up first */
    assert_int_equal(scfw.n_calls, 1);
    assert_int_equal(scfw.calls[0].svc, SC_RPC_SVC_PM);
    assert_int_equal(scfw.calls[0].func, PM_FUNC_RESET);
    assert_int_equal(scfw.calls[0].arg & 0xff, SC_PM_RESET_TYPE_BOARD);
    assert_true(fake_regs_first_write(MU_ACR) < fake_regs_first_write(MU_ATR(0)));
}

/* platform.md 3.1: and after it, with the channel that is open */
static void test_plat_reset(void **state)
{
    size_t acr_writes;
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    scfw.n_calls = 0;
    acr_writes = fake_regs_writes(MU_ACR);

    if (setjmp(soc_reset) == 0) {
        plat_reset();
        fail_msg("plat_reset() returned");
    }

    assert_int_equal(scfw.n_calls, 1);
    assert_int_equal(scfw.calls[0].svc, SC_RPC_SVC_PM);
    assert_int_equal(scfw.calls[0].func, PM_FUNC_RESET);
    assert_int_equal(scfw.calls[0].arg & 0xff, SC_PM_RESET_TYPE_BOARD);
    /* The channel isn't opened again */
    assert_int_equal(fake_regs_writes(MU_ACR), acr_writes);
}

/* platform.md 3.4: before plat_early_init() nothing was asked, and the
 * reason is no power on by default. Runs before any plat_early_init() */
static void test_boot_reason_before_early_init(void **state)
{
    struct pb_boot_reason reason;
    (void)state;

    memset(&reason, 0xff, sizeof(reason));
    assert_int_equal(plat_get_boot_reason(&reason), -PB_ERR_IO);
    assert_int_equal(reason.kind, PB_BOOT_REASON_UNKNOWN);
    assert_string_equal(reason.name, "Unknown");
    assert_int_equal(scfw.n_calls, 0);
}

/* platform.md 3.4: every reason the SCFW has gets a kind and a name, and raw
 * is the SCFW's value (plat/imx8x/imx8x.h has the table). 13 and 14 were
 * "Unknown" (F-412) */
static void test_boot_reason_kinds(void **state)
{
    static const struct {
        enum pb_boot_reason_kind kind;
        const char *name;
    } reasons[] = {
        [SC_PM_RESET_REASON_POR] = { PB_BOOT_REASON_POR, "POR" },
        [SC_PM_RESET_REASON_JTAG] = { PB_BOOT_REASON_DEBUG, "JTAG" },
        [SC_PM_RESET_REASON_SW] = { PB_BOOT_REASON_SW, "SW" },
        [SC_PM_RESET_REASON_WDOG] = { PB_BOOT_REASON_WDOG, "WDOG" },
        [SC_PM_RESET_REASON_LOCKUP] = { PB_BOOT_REASON_FAULT, "LOCKUP" },
        [SC_PM_RESET_REASON_SNVS] = { PB_BOOT_REASON_FAULT, "SNVS" },
        [SC_PM_RESET_REASON_TEMP] = { PB_BOOT_REASON_FAULT, "TEMP" },
        [SC_PM_RESET_REASON_MSI] = { PB_BOOT_REASON_UNKNOWN, "MSI" },
        [SC_PM_RESET_REASON_UECC] = { PB_BOOT_REASON_FAULT, "UECC" },
        [SC_PM_RESET_REASON_SCFW_WDOG] = { PB_BOOT_REASON_WDOG, "SCFW WDOG" },
        [SC_PM_RESET_REASON_ROM_WDOG] = { PB_BOOT_REASON_WDOG, "ROM WDOG" },
        [SC_PM_RESET_REASON_SECO] = { PB_BOOT_REASON_FAULT, "SECO" },
        [SC_PM_RESET_REASON_SCFW_FAULT] = { PB_BOOT_REASON_FAULT, "SCFW Fault" },
        [SC_PM_RESET_REASON_V2X_DEBUG] = { PB_BOOT_REASON_UNKNOWN, "V2X DEBUG" },
        [SC_PM_RESET_REASON_BOOT_STAGE] = { PB_BOOT_REASON_UNKNOWN, "BOOT STAGE" },
        /* One the SCFW adds later */
        [15] = { PB_BOOT_REASON_UNKNOWN, "Unknown" },
    };
    struct pb_boot_reason reason;

    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(reasons); i++) {
        memset(&reason, 0xff, sizeof(reason));
        scfw.reset_reason = (uint8_t)i;
        scfw.n_calls = 0;
        assert_int_equal(plat_early_init(), PB_OK);
        assert_int_equal(plat_get_boot_reason(&reason), PB_OK);
        assert_int_equal(reason.kind, reasons[i].kind);
        assert_int_equal(reason.raw, i);
        assert_string_equal(reason.name, reasons[i].name);
    }
}

/* platform.md 3.4: the SCFW refused the question. An error, and a reason
 * that is filled in all the same: the banner prints it */
static void test_boot_reason_scfw_error(void **state)
{
    struct pb_boot_reason reason;
    (void)state;

    memset(&reason, 0xff, sizeof(reason));
    scfw.reset_reason = SC_PM_RESET_REASON_WDOG;
    scfw.reset_reason_err = SC_ERR_NOACCESS;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_get_boot_reason(&reason), -PB_ERR_IO);
    assert_int_equal(reason.kind, PB_BOOT_REASON_UNKNOWN);
    assert_int_equal(reason.raw, 0);
    assert_string_equal(reason.name, "Unknown");
}

/* platform.md 3.6, 3.7: the platform's UUID namespace never changes.
 * aeda39be-792b-4de5-858a-4c357b9b6302 */
static void test_platform_ns_uuid(void **state)
{
    static const uint8_t ns[16] = { 0xae, 0xda, 0x39, 0xbe, 0x79, 0x2b, 0x4d, 0xe5,
                                    0x85, 0x8a, 0x4c, 0x35, 0x7b, 0x9b, 0x63, 0x02 };
    (void)state;

    assert_memory_equal(platform_ns_uuid, ns, sizeof(ns));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_wdog_kick_before_early_init, setup),
        cmocka_unit_test_setup(test_plat_reset_before_early_init, setup),
        cmocka_unit_test_setup(test_boot_reason_before_early_init, setup),
        cmocka_unit_test_setup(test_plat_reset, setup),
        cmocka_unit_test_setup(test_boot_reason_kinds, setup),
        cmocka_unit_test_setup(test_boot_reason_scfw_error, setup),
        cmocka_unit_test_setup(test_platform_ns_uuid, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
