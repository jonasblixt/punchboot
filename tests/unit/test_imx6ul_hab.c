/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX6UL's HAB helpers (src/plat/imx6ul/hab.c) and what they decide for
 * the RoT and the SLC with the 6UL's fuse map (plat.c's hab_fuses, the HABv4
 * driver src/drivers/fuse/imx_hab_fuses.c): the life cycle and the key state
 * a 6UL reports, and the revocation that HAB's status allows.
 *
 * The OCOTP is a fake of 8 banks of 8 words. The ROM's vector table is the
 * test's (fakes/imx6ul_hab): report_status() and report_event() as the HAB4
 * API describes them, an event found by its status and its index among the
 * events of that status. The ROM's functions are called through 32 bit
 * pointers, so the test is linked -no-pie.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/fuse/imx_hab_fuses.h>
#include <drivers/fuse/imx_ocotp.h>
#include <pb/pb.h>
#include <plat/imx6ul/fusebox.h>
#include <plat/imx6ul/hab.h>
#include <string.h>

#define WORDS_PER_BANK 8

/* The fuses */
static uint32_t otp[8 * WORDS_PER_BANK];
static unsigned int writes;
static int read_rc;

#define SEC_CONFIG (otp[0 * WORDS_PER_BANK + 6])
#define SRK(n)     (otp[3 * WORDS_PER_BANK + (n)])
#define REVOKE     (otp[5 * WORDS_PER_BANK + 7])

int imx_ocotp_read(uint32_t bank, uint32_t row, uint32_t *value)
{
    assert_true(bank * WORDS_PER_BANK + row < ARRAY_SIZE(otp));

    if (read_rc != PB_OK)
        return read_rc;

    *value = otp[bank * WORDS_PER_BANK + row];
    return PB_OK;
}

/* An OTP write only blows bits */
int imx_ocotp_write(uint32_t bank, uint32_t row, uint32_t value)
{
    assert_true(bank * WORDS_PER_BANK + row < ARRAY_SIZE(otp));
    writes++;
    otp[bank * WORDS_PER_BANK + row] |= value;
    return PB_OK;
}

/* The build's SRK hash, pb/srk_fuse.h */
const uint32_t srk_fuse[IMX_HAB_SRK_HASH_WORDS] = {
    0x5020c7d7, 0xbb62b945, 0xdd97c8be, 0xdc6710dd, 0x2756b777, 0xef43bc0a, 0x7185604b, 0x3f335991,
};
const size_t srk_fuse_words = IMX_HAB_SRK_HASH_WORDS;

/* The ROM: its state and its event log */
struct event {
    enum hab_status status;
    uint8_t data[8];
};

static struct {
    enum hab_status status;
    enum hab_config config;
    enum hab_state state;
    struct event events[4];
    unsigned int nr_events;

    /* report_event()'s calls, in order */
    struct {
        enum hab_status status;
        uint32_t index;
    } calls[16];

    unsigned int nr_calls;
} rom;

static enum hab_status rom_report_status(enum hab_config *config, enum hab_state *state)
{
    *config = rom.config;
    *state = rom.state;
    return rom.status;
}

/* The index-th event with the status, HAB_STS_ANY matching every event */
static enum hab_status
rom_report_event(enum hab_status status, uint32_t index, uint8_t *event, size_t *bytes)
{
    unsigned int seen = 0;

    assert_true(rom.nr_calls < ARRAY_SIZE(rom.calls));
    rom.calls[rom.nr_calls].status = status;
    rom.calls[rom.nr_calls].index = index;
    rom.nr_calls++;

    for (unsigned int n = 0; n < rom.nr_events; n++) {
        if (status != HAB_STS_ANY && rom.events[n].status != status)
            continue;

        if (seen++ != index)
            continue;

        assert_true(*bytes >= sizeof(rom.events[n].data));
        memcpy(event, rom.events[n].data, sizeof(rom.events[n].data));
        *bytes = sizeof(rom.events[n].data);
        return HAB_SUCCESS;
    }

    return HAB_FAILURE;
}

/* The vector table holds 32 bit pointers */
uint32_t fake_hab_rvt_read(uintptr_t offset)
{
    uintptr_t fn;

    switch (offset) {
    case 0x20:
        fn = (uintptr_t)rom_report_event;
        break;
    case 0x24:
        fn = (uintptr_t)rom_report_status;
        break;
    default:
        fail_msg("The helpers read the RVT at 0x%lx", (unsigned long)offset);
        return 0;
    }

    assert_true(fn <= UINT32_MAX);
    return (uint32_t)fn;
}

static void add_event(enum hab_status status, uint8_t first)
{
    struct event *e = &rom.events[rom.nr_events++];

    e->status = status;
    memset(e->data, first, sizeof(e->data));
}

/* The 6UL's configuration, as src/plat/imx6ul/plat.c has it */
static const struct imx_hab_fuses hab_fuses = {
    .sec_config_bank = IMX6UL_FUSE_SEC_CONFIG_BANK,
    .sec_config_word = IMX6UL_FUSE_SEC_CONFIG_WORD,
    .closed = IMX6UL_FUSE_SEC_CONFIG_CLOSED,
    .srk_bank = IMX6UL_FUSE_SRK_BANK,
    .srk_word = IMX6UL_FUSE_SRK_WORD,
    .revoke_bank = IMX6UL_FUSE_REVOKE_BANK,
    .revoke_word = IMX6UL_FUSE_REVOKE_WORD,
    .revocable = IMX6UL_FUSE_REVOCABLE_SRKS,
    .revoke_allowed = hab_has_no_errors,
};

static int setup(void **state)
{
    (void)state;
    memset(otp, 0, sizeof(otp));
    writes = 0;
    read_rc = PB_OK;
    memset(&rom, 0, sizeof(rom));
    /* An open part that booted a good image */
    rom.status = HAB_SUCCESS;
    rom.config = HAB_CFG_OPEN;
    rom.state = HAB_STATE_NONSECURE;
    imx_hab_fuses_init(&hab_fuses);
    return 0;
}

/* RM Table 5-9 (the fuse map): SEC_CONFIG[1] at 0x460, SRK_HASH at
 * 0x580..0x5f0, SRK_REVOKE at 0x6f0. A word's address is 0x400 + 0x10 per
 * word, 8 words a bank */
static void test_fuse_map(void **state)
{
    (void)state;

#define FUSE_ADDR(bank, word) (0x400 + ((bank) * WORDS_PER_BANK + (word)) * 0x10)
    assert_int_equal(FUSE_ADDR(IMX6UL_FUSE_SEC_CONFIG_BANK, IMX6UL_FUSE_SEC_CONFIG_WORD), 0x460);
    assert_int_equal(IMX6UL_FUSE_SEC_CONFIG_CLOSED, 0x02);
    assert_int_equal(FUSE_ADDR(IMX6UL_FUSE_SRK_BANK, IMX6UL_FUSE_SRK_WORD), 0x580);
    assert_int_equal(FUSE_ADDR(IMX6UL_FUSE_SRK_BANK, IMX6UL_FUSE_SRK_WORD + 7), 0x5f0);
    assert_int_equal(FUSE_ADDR(IMX6UL_FUSE_REVOKE_BANK, IMX6UL_FUSE_REVOKE_WORD), 0x6f0);
    assert_int_equal(IMX6UL_FUSE_REVOCABLE_SRKS, 3);
#undef FUSE_ADDR
}

/* What the ROM's status gives: only HAB_SUCCESS is no error, whatever the
 * part's configuration and state */
static void test_status(void **state)
{
    (void)state;

    assert_int_equal(hab_has_no_errors(), PB_OK);

    rom.config = HAB_CFG_CLOSED;
    rom.state = HAB_STATE_TRUSTED;
    assert_int_equal(hab_has_no_errors(), PB_OK);

    rom.status = HAB_WARNING;
    assert_int_equal(hab_has_no_errors(), -PB_ERR);

    rom.status = HAB_FAILURE;
    rom.state = HAB_STATE_FAIL_SOFT;
    assert_int_equal(hab_has_no_errors(), -PB_ERR);

    /* Not a status the API has */
    rom.status = HAB_STS_ANY;
    assert_int_equal(hab_has_no_errors(), -PB_ERR);

    /* The events don't change the answer, only the status does */
    rom.status = HAB_SUCCESS;
    add_event(HAB_WARNING, 0xaa);
    assert_int_equal(hab_has_no_errors(), PB_OK);
}

/* The events are logged: the warnings, then the failures, each from index
 * 0: report_event() counts the index among the events of the status asked
 * for. The failures' index started where the warnings' ended, and a failure
 * after a warning was never asked for nor logged (F-430) */
static void test_events(void **state)
{
    (void)state;

    /* No events: one call each */
    assert_int_equal(hab_has_no_errors(), PB_OK);
    assert_int_equal(rom.nr_calls, 2);
    assert_int_equal(rom.calls[0].status, HAB_WARNING);
    assert_int_equal(rom.calls[0].index, 0);
    assert_int_equal(rom.calls[1].status, HAB_FAILURE);
    assert_int_equal(rom.calls[1].index, 0);

    /* A failure alone is asked for at index 0 */
    rom.nr_calls = 0;
    rom.status = HAB_FAILURE;
    add_event(HAB_FAILURE, 0x33);
    assert_int_equal(hab_has_no_errors(), -PB_ERR);
    assert_int_equal(rom.nr_calls, 3);
    assert_int_equal(rom.calls[1].status, HAB_FAILURE);
    assert_int_equal(rom.calls[1].index, 0);
    assert_int_equal(rom.calls[2].index, 1);

    /* A warning and then the failure: the failure is asked for at 0, and
     * found */
    memset(&rom.events, 0, sizeof(rom.events));
    rom.nr_events = 0;
    rom.nr_calls = 0;
    add_event(HAB_WARNING, 0x69);
    add_event(HAB_FAILURE, 0x33);
    assert_int_equal(hab_has_no_errors(), -PB_ERR);
    assert_int_equal(rom.nr_calls, 4);
    assert_int_equal(rom.calls[0].status, HAB_WARNING);
    assert_int_equal(rom.calls[0].index, 0);
    assert_int_equal(rom.calls[1].status, HAB_WARNING);
    assert_int_equal(rom.calls[1].index, 1);
    assert_int_equal(rom.calls[2].status, HAB_FAILURE);
    assert_int_equal(rom.calls[2].index, 0);
    assert_int_equal(rom.calls[3].status, HAB_FAILURE);
    assert_int_equal(rom.calls[3].index, 1);

    /* Two warnings and two failures, interleaved: each is asked for once */
    memset(&rom.events, 0, sizeof(rom.events));
    rom.nr_events = 0;
    rom.nr_calls = 0;
    add_event(HAB_FAILURE, 0x31);
    add_event(HAB_WARNING, 0x61);
    add_event(HAB_FAILURE, 0x32);
    add_event(HAB_WARNING, 0x62);
    assert_int_equal(hab_has_no_errors(), -PB_ERR);
    assert_int_equal(rom.nr_calls, 6);
    assert_int_equal(rom.calls[3].status, HAB_FAILURE);
    assert_int_equal(rom.calls[3].index, 0);
    assert_int_equal(rom.calls[4].index, 1);
    assert_int_equal(rom.calls[5].index, 2);
}

/* The life cycle a 6UL reports, from its fuses */
static void test_life_cycle(void **state)
{
    (void)state;

    assert_int_equal(imx_hab_slc_read_status(), SLC_NOT_CONFIGURED);

    /* The boot fuses don't change it */
    SEC_CONFIG = IMX6UL_FUSE_BT_FUSE_SEL;
    assert_int_equal(imx_hab_slc_read_status(), SLC_NOT_CONFIGURED);

    assert_int_equal(imx_hab_slc_set_configuration(), PB_OK);
    assert_int_equal(writes, IMX_HAB_SRK_HASH_WORDS);
    assert_int_equal(SRK(0), srk_fuse[0]);
    assert_int_equal(SRK(7), srk_fuse[7]);
    assert_int_equal(imx_hab_slc_read_status(), SLC_CONFIGURATION);

    assert_int_equal(imx_hab_slc_set_configuration_locked(), PB_OK);
    assert_int_equal(SEC_CONFIG, IMX6UL_FUSE_BT_FUSE_SEL | IMX6UL_FUSE_SEC_CONFIG_CLOSED);
    assert_int_equal(imx_hab_slc_read_status(), SLC_CONFIGURATION_LOCKED);

    /* Closed without an SRK hash is closed */
    memset(otp, 0, sizeof(otp));
    SEC_CONFIG = IMX6UL_FUSE_SEC_CONFIG_CLOSED;
    assert_int_equal(imx_hab_slc_read_status(), SLC_CONFIGURATION_LOCKED);
}

/* The key state a 6UL reports: SRK_REVOKE's bit per slot */
static void test_key_status(void **state)
{
    struct rot_key key = { .id = 0xa90f9680, .slot = 0 };
    (void)state;

    for (key.slot = 0; key.slot < 4; key.slot++)
        assert_int_equal(imx_hab_read_key_status(&key), PB_OK);

    REVOKE = 0x05;
    key.slot = 0;
    assert_int_equal(imx_hab_read_key_status(&key), -PB_ERR_KEY_REVOKED);
    key.slot = 1;
    assert_int_equal(imx_hab_read_key_status(&key), PB_OK);
    key.slot = 2;
    assert_int_equal(imx_hab_read_key_status(&key), -PB_ERR_KEY_REVOKED);
}

/* A revocation on the 6UL asks the HAB first: an error refuses it */
static void test_revoke_asks_the_hab(void **state)
{
    struct rot_key key = { .id = 0xa90f9680, .slot = 2 };
    (void)state;

    rom.status = HAB_WARNING;
    assert_int_equal(imx_hab_revoke_key(&key), -PB_ERR_STATE);
    assert_int_equal(writes, 0);
    /* Asked: the status and the events */
    assert_true(rom.nr_calls > 0);

    rom.status = HAB_FAILURE;
    assert_int_equal(imx_hab_revoke_key(&key), -PB_ERR_STATE);
    assert_int_equal(writes, 0);

    rom.status = HAB_SUCCESS;
    assert_int_equal(imx_hab_revoke_key(&key), PB_OK);
    assert_int_equal(REVOKE, 0x04);
    assert_int_equal(imx_hab_read_key_status(&key), -PB_ERR_KEY_REVOKED);

    /* SRK3 is past what HAB revokes: refused before the HAB is asked */
    rom.nr_calls = 0;
    rom.status = HAB_SUCCESS;
    key.slot = 3;
    assert_int_equal(imx_hab_revoke_key(&key), -PB_ERR_PARAM);
    assert_int_equal(rom.nr_calls, 0);
    assert_int_equal(writes, 1);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_fuse_map, setup),
        cmocka_unit_test_setup(test_status, setup),
        cmocka_unit_test_setup(test_events, setup),
        cmocka_unit_test_setup(test_life_cycle, setup),
        cmocka_unit_test_setup(test_key_status, setup),
        cmocka_unit_test_setup(test_revoke_asks_the_hab, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
