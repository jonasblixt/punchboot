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
 * and the SLC helpers, the MMU and the crypto backends (the CAAM and Mbed
 * TLS: the test's build has both options) are fakes.
 *
 * The port keeps the SCFW's handle in a static: the tests of the time before
 * plat_early_init() run first.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/crypto/imx_caam.h>
#include <drivers/crypto/mbedtls.h>
#include <pb/command.h>
#include <pb/console.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <plat/imx8x/sci/rpc.h>
#include <plat/imx8x/sci/svc/misc/api.h>
#include <plat/imx8x/sci/svc/pm/api.h>
#include <string.h>

#include "fakes/regs.h"
#include "fakes/xlat.h"

/* The messaging unit, the port's side (src/plat/imx8x/sci/imx8_mu.h) */
#define MU_ATR(n)               (SC_IPC_BASE + 0x00 + 4 * (n))
#define MU_ARR(n)               (SC_IPC_BASE + 0x10 + 4 * (n))
#define MU_ASR                  (SC_IPC_BASE + 0x20)
#define MU_ACR                  (SC_IPC_BASE + 0x24)
#define MU_ASR_TE_ALL           (0xfu << 20)
#define MU_ASR_RF_ALL           (0xfu << 24)

/* The functions' indexes (src/plat/imx8x/sci/svc/pm/rpc.h) */
#define PM_FUNC_SET_POWER_MODE  3
#define PM_FUNC_SET_CLOCK_RATE  5
#define PM_FUNC_RESET_REASON    10
#define PM_FUNC_RESET           13
/* src/plat/imx8x/sci/svc/misc/rpc.h */
#define MISC_FUNC_GET_TEMP      13
#define MISC_FUNC_BUILD_INFO    15
#define MISC_FUNC_GET_BOOT_TYPE 33

/* The GPT's control register and its software reset */
#define GPT_CR                  0x0
#define GPT_CR_SWR              BIT(15)

#define MAX_CALLS               64

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
    uint32_t scfw_build; /* sc_misc_build_info()'s build, its commit is 0 */
    uint32_t temp; /* sc_misc_get_temp()'s word: celsius in 15:0, tenths in 23:16 */
    uint8_t temp_err;
    uint8_t timer_err; /* ... for every call of its timer service */
    uint8_t boot_type; /* What the ROM booted from, SC_MISC_BT_* */
    uint8_t boot_type_err; /* The SCFW's error for the question, 0: none */
} scfw;

/* A reset leaves the code under test through here */
static jmp_buf soc_reset;

/* The crypto backends: what their set up returns and how often it was
 * called, what the CAAM's got, and the number of the SCFW's calls, the
 * register log's length and the MMU's state when it was called */
static struct {
    int caam_rc, mbedtls_rc;
    unsigned int caam_inits, mbedtls_inits;
    uintptr_t caam_base;
    size_t caam_scfw_calls, caam_at, mbedtls_at;
    unsigned int caam_mmu_enables;
} crypto;

int imx_caam_init(uintptr_t base)
{
    crypto.caam_inits++;
    crypto.caam_base = base;
    crypto.caam_scfw_calls = scfw.n_calls;
    crypto.caam_at = fake_reg_log_len;
    crypto.caam_mmu_enables = fake_mmu_enables;
    return crypto.caam_rc;
}

int mbedtls_pb_init(void)
{
    crypto.mbedtls_inits++;
    crypto.mbedtls_at = fake_reg_log_len;
    return crypto.mbedtls_rc;
}

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
    } else if (svc == SC_RPC_SVC_MISC && func == MISC_FUNC_BUILD_INFO) {
        data = scfw.scfw_build;
    } else if (svc == SC_RPC_SVC_MISC && func == MISC_FUNC_GET_TEMP) {
        data = scfw.temp;
        err = scfw.temp_err;
    } else if (svc == SC_RPC_SVC_TIMER) {
        err = scfw.timer_err;
    } else if (svc == SC_RPC_SVC_MISC && func == MISC_FUNC_GET_BOOT_TYPE) {
        data = scfw.boot_type;
        err = scfw.boot_type_err;
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
    memset(&crypto, 0, sizeof(crypto));
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

/* What plat_get_status() answers, every field set by the port */
static struct plat_status status_now(void)
{
    struct plat_status status;

    memset(&status, 0xff, sizeof(status));
    plat_get_status(&status);
    return status;
}

/* platform.md 3.8: what the port recorded. The DRAM filter is a later
 * stage's (BL31, through the SCFW), the watchdog is armed (the test is built
 * with the option), the crypto backends the platform set up work, and it
 * saves none of the ROM's logs */
static void test_status(void **state)
{
    struct plat_status status;
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    status = status_now();

    assert_int_equal(status.tz_dram_filter, PLAT_TZ_DRAM_LATER_STAGE);
    assert_true(status.wdog_armed);
    assert_true(status.crypto_engine);
    assert_false(status.rom_log);
}

/* platform.md 2.2, 2.3 (F-248): a watchdog the SCFW didn't arm isn't the
 * platform's error. It is recorded, for the boot to report */
static void test_status_wdog_not_armed(void **state)
{
    (void)state;

    scfw.timer_err = SC_ERR_NOACCESS;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_false(status_now().wdog_armed);

    /* And it is the state of this plat_early_init(), not of an earlier one */
    scfw.timer_err = SC_ERR_NONE;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_true(status_now().wdog_armed);
}

/* How often the SCFW was asked for the boot type */
static unsigned int boot_type_calls(void)
{
    unsigned int calls = 0;

    for (size_t i = 0; i < scfw.n_calls; i++) {
        if (scfw.calls[i].svc == SC_RPC_SVC_MISC && scfw.calls[i].func == MISC_FUNC_GET_BOOT_TYPE)
            calls++;
    }

    return calls;
}

/* platform.md 3.9 (F-251): where pb was loaded from is the platform's to
 * tell, from the SCFW's boot type, which the boards read themselves in
 * early_boot(). Serial is the ROM's serial downloader: USB. The other types
 * are a boot from storage, whose partition isn't named: unknown */
static void test_boot_source(void **state)
{
    static const struct {
        uint8_t type;
        enum pb_boot_source_kind kind;
    } types[] = {
        { SC_MISC_BT_PRIMARY, PB_BOOT_SOURCE_UNKNOWN },
        { SC_MISC_BT_SECONDARY, PB_BOOT_SOURCE_UNKNOWN },
        { SC_MISC_BT_RECOVERY, PB_BOOT_SOURCE_UNKNOWN },
        { SC_MISC_BT_MANUFACTURE, PB_BOOT_SOURCE_UNKNOWN },
        { SC_MISC_BT_SERIAL, PB_BOOT_SOURCE_USB },
        /* One the SCFW adds later */
        { 5, PB_BOOT_SOURCE_UNKNOWN },
    };
    struct pb_boot_source src;

    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    /* Not asked on the way: plat_early_init() is on every boot */
    assert_int_equal(boot_type_calls(), 0);

    for (size_t i = 0; i < ARRAY_SIZE(types); i++) {
        memset(&src, 0xff, sizeof(src));
        scfw.boot_type = types[i].type;
        scfw.n_calls = 0;
        assert_int_equal(plat_get_boot_source(&src), PB_OK);
        assert_int_equal(src.kind, types[i].kind);
        assert_int_equal(boot_type_calls(), 1);
        assert_int_equal(scfw.n_calls, 1);
    }
}

/* An SCFW that refuses the question: the platform can't tell this time,
 * which is "unknown" with PB_OK, whatever the answer's data holds */
static void test_boot_source_scfw_error(void **state)
{
    struct pb_boot_source src;
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    memset(&src, 0xff, sizeof(src));
    scfw.boot_type = SC_MISC_BT_SERIAL;
    scfw.boot_type_err = SC_ERR_NOACCESS;
    assert_int_equal(plat_get_boot_source(&src), PB_OK);
    assert_int_equal(src.kind, PB_BOOT_SOURCE_UNKNOWN);
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

/* The platform's named command 'info' (command-mode.md 5.6), from the
 * linker's list */
static const struct cm_command *find_command(const char *name)
{
    for (const struct cm_command *c = __start_pb_commands; c < __stop_pb_commands; c++) {
        if (strcmp(c->name, name) == 0)
            return c;
    }
    return NULL;
}

static size_t run_info(uint8_t *response, size_t room)
{
    const struct cm_command *info = find_command("info");
    size_t size = room;

    assert_non_null(info);
    assert_int_equal(info->run(NULL, 0, response, &size), PB_OK);
    return size;
}

static void test_info_command(void **state)
{
    static const char text[] = "SCFW 5665 (0)\n"
                               "SECO 0 (0)\n"
                               "SoC id 0, rev 0\n"
                               "Temperature 31.5 C\n";
    uint8_t response[256];
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    scfw.scfw_build = 5665;
    scfw.temp = 31 | (5u << 16);

    /* The text and its NUL */
    memset(response, 0xa5, sizeof(response));
    assert_int_equal(run_info(response, sizeof(response)), sizeof(text));
    assert_memory_equal(response, text, sizeof(text));

    /* Below zero: one sign */
    scfw.temp = 0xfffd | (0xfbu << 16);
    run_info(response, sizeof(response));
    assert_non_null(strstr((char *)response, "Temperature -3.5 C\n"));

    scfw.temp_err = SC_ERR_UNAVAILABLE;
    run_info(response, sizeof(response));
    assert_non_null(strstr((char *)response, "Temperature not read\n"));

    /* Cut to the room, with its NUL */
    memset(response, 0xa5, sizeof(response));
    assert_int_equal(run_info(response, 8), 8);
    assert_memory_equal(response, "SCFW 56\0", 8);
    assert_int_equal(response[8], 0xa5);
}

/* platform.md 2.2, crypto.md 2 (F-247): plat_init() sets up the build's
 * crypto backends, each behind its driver's option: the CAAM on job ring 2,
 * which the SCFW powers first (the ring and its output), and Mbed TLS.
 *
 * Until F-247 the boards' init() did the SCFW's two calls and the CAAM's as
 * its first step, which was the first thing after plat_init(): they are
 * plat_init()'s last now, with the MMU on, and neither the SCFW nor a
 * register of the platform's hears anything after them. So the SoC sees the
 * same calls and writes in the same order */
static void test_crypto_set_up_last_in_plat_init(void **state)
{
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(crypto.caam_inits, 0);
    assert_int_equal(crypto.mbedtls_inits, 0);

    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(crypto.caam_inits, 1);
    assert_int_equal(crypto.mbedtls_inits, 1);
    assert_int_equal(crypto.caam_base, IMX_CAAM_JR2_BASE);
    assert_int_equal(crypto.caam_mmu_enables, 1);

    /* The SCFW's last two calls, before the CAAM is touched */
    assert_true(scfw.n_calls >= 2);
    assert_int_equal(crypto.caam_scfw_calls, scfw.n_calls);

    for (size_t i = 0; i < 2; i++) {
        static const uint32_t resources[] = { SC_R_CAAM_JR2, SC_R_CAAM_JR2_OUT };
        size_t n = scfw.n_calls - 2 + i;

        assert_int_equal(scfw.calls[n].svc, SC_RPC_SVC_PM);
        assert_int_equal(scfw.calls[n].func, PM_FUNC_SET_POWER_MODE);
        assert_int_equal(scfw.calls[n].arg & 0xffff, resources[i]);
        assert_int_equal((scfw.calls[n].arg >> 16) & 0xff, SC_PM_PW_MODE_ON);
    }

    /* Nothing is written after it, and Mbed TLS comes after the CAAM */
    assert_int_equal(crypto.caam_at, fake_reg_log_len);
    assert_int_equal(crypto.mbedtls_at, fake_reg_log_len);
}

/* platform.md 2.2: a crypto engine that fails isn't plat_init()'s error:
 * command mode can start without it, and main() would reset */
static void test_plat_init_succeeds_without_the_crypto_engine(void **state)
{
    (void)state;

    crypto.caam_rc = -PB_ERR_IO;
    crypto.mbedtls_rc = -PB_ERR_PARAM;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(crypto.caam_inits, 1);
    assert_int_equal(crypto.mbedtls_inits, 1);
}

/* platform.md 2.3, 3.8 (F-247, F-248): the platform sets the build's crypto
 * backends up, so their result is the platform's status, and the boot's
 * preconditions refuse the boot when it is false, before a candidate is
 * tried (test_boot.c) */
static void test_status_crypto_engine(void **state)
{
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_true(status_now().crypto_engine);

    /* The CAAM, which the boards verify with */
    setup(NULL);
    crypto.caam_rc = -PB_ERR_IO;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_false(status_now().crypto_engine);

    /* Each algorithm has one backend (crypto.md 2): a build with two needs
     * both */
    setup(NULL);
    crypto.mbedtls_rc = -PB_ERR_MEM;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_false(status_now().crypto_engine);

    /* It is the state of this plat_init(), not of an earlier one */
    setup(NULL);
    assert_int_equal(plat_init(), PB_OK);
    assert_true(status_now().crypto_engine);
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
        cmocka_unit_test_setup(test_status, setup),
        cmocka_unit_test_setup(test_status_wdog_not_armed, setup),
        cmocka_unit_test_setup(test_boot_source, setup),
        cmocka_unit_test_setup(test_boot_source_scfw_error, setup),
        cmocka_unit_test_setup(test_platform_ns_uuid, setup),
        cmocka_unit_test_setup(test_info_command, setup),
        cmocka_unit_test_setup(test_crypto_set_up_last_in_plat_init, setup),
        cmocka_unit_test_setup(test_plat_init_succeeds_without_the_crypto_engine, setup),
        cmocka_unit_test_setup(test_status_crypto_engine, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
