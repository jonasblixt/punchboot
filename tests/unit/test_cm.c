/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Command mode (src/cm/cm_main.c) over a scripted fake transport. The boot
 * flow, root of trust, SLC and crypto providers are fakes, so each command's
 * error paths can be reached. The integration tests cover the commands
 * against the real modules and the punchboot host library.
 *
 * Command mode is a task (cm_main_task(), board.md 4.6): cm_run() here calls
 * it as main()'s loop does, round after round with a kick of the watchdog
 * and another task in each, until it returns an error.
 *
 * A test queues the host's side of the conversation (commands and data) with
 * host_*(), runs cm_run() until the queue is empty, and then checks what the
 * device sent back with expect_*(). When the queue runs dry the transport
 * reports a disconnect, the task re-initializes the transport, and the fake
 * fails that, which ends cm_run() with -PB_ERR_IO. host_lost() and
 * t.lose_write lose a transfer on the way, as a host that went away.
 * host_stops() and t.stop_write leave one pending, as a host that stopped.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <boot/boot.h>
#include <bpak/bpak.h>
#include <pb-tools/error.h>
#include <pb-tools/wire.h>
#include <pb/bio.h>
#include <pb/cm.h>
#include <pb/crc.h>
#include <pb/crypto.h>
#include <pb/device_uuid.h>
#include <pb/log_source.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/ramlog.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <stdlib.h>
#include <string.h>

#include "fakes/plat.h"
#include "fakes/ramdisk.h"

#define BUF_SIZE    (CONFIG_CM_BUF_SIZE_KiB * 1024)
#define LOG_SIZE    (CONFIG_RAMLOG_SIZE_KiB * 1024)
#define DISK_BLOCKS 64

#define UU_DISK \
    (const unsigned char *)"\x1e\xac\xed\xf3\x37\x90\x48\xc7\x8e\xd8\x91\x88\xff\x49\x67\x2b"
/* Readable, writable and visible: blocks 10 to 29 */
#define UU_RW \
    (const unsigned char *)"\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62\x01\x4c\xfa\x73\x5c\xe0"
/* Visible only: blocks 30 to 39 */
#define UU_RO \
    (const unsigned char *)"\xc0\x46\xcc\xd8\x0f\x2e\x40\x36\x98\x4d\x76\xc1\x4d\xc7\x39\x92"
/* Not visible: blocks 40 to 49 */
#define UU_HIDDEN \
    (const unsigned char *)"\xff\x4d\xdc\x6c\xad\x7a\x47\xe8\x87\x73\x67\x29\x39\x2d\xd1\xb5"
#define UU_MISSING \
    (const unsigned char *)"\x44\xac\xdc\xbe\xdc\xb0\x4d\x89\xb0\xad\x8f\x96\x96\x7f\x8c\x95"

#define RW_FIRST_LBA        10
#define RW_BLOCKS           20
#define DEVICE_UUID         "02e49231-756e-35ee-a982-378e5ba866a9"
#define KEY_ID              0xa90f9680
#define FAKE_SIG_OK         0x5a
#define BOARD_ECHO          1
#define BOARD_RETURN        2
#define BOARD_REBOOT        3
#define BOARD_FILL          4
#define BOARD_FILL_SIZE     64
#define BOARD_PREFIX        "echo: "
/* The response_buffer_size every released front end sends: the 1.x CLI, the
 * 2.x Python wrapper and the Python host */
#define HOST_RESPONSE_SIZE  4096
/* fake_us_tick_step: 1 ms per plat_get_us_tick() */
#define TICK_STEP_US        1000
/* A failed authentication's result is sent 1 s after the data (3.3), and the
 * watchdog is kicked every 100 ms meanwhile */
#define AUTH_WAIT_US        1000000
#define AUTH_KICK_US        100000
#define TRANSFER_TIMEOUT_US (CONFIG_CM_TRANSFER_TIMEOUT * 1000000U)
/* A stopped transfer polled this often was waited for past the timeout */
#define STOPPED_POLLS_MAX   (2 * TRANSFER_TIMEOUT_US / TICK_STEP_US)

/* The fake transport: the cm_transport_*() functions that command mode links */
static struct {
    uint8_t in[32 * 1024]; /* Host to device */
    size_t in_len;
    size_t in_pos;
    uint8_t out[32 * 1024]; /* Device to host */
    size_t out_len;
    size_t out_pos;

    void *read_buf;
    size_t read_len;
    bool read_pending;

    int complete_rcs[8]; /* Returned by complete() before it does anything */
    int n_complete_rcs;
    size_t lost_at[4]; /* A read that starts at this input position is lost, */
    int lost_rc[4]; /* complete() returns this for it, once */
    int n_lost;
    int lose_write; /* The write with this number (from 1) is lost */
    int write_calls;
    bool write_lost;
    int fail_reads_after; /* -1: never, n: read() fails after n calls */
    int fail_writes_after;
    /* The host stops: a read that starts at input position stop_at, or the
     * write with the number stop_write, stays pending until the transport is
     * started again */
    bool stop_reads;
    size_t stop_at;
    int stop_write;
    bool write_stopped;
    bool stopped;
    unsigned int stopped_polls;

    int max_inits;
    int init_calls;
    int connect_rc; /* Returned by connect() until connect_ok_after calls */
    int connect_ok_after;
    int connect_calls;
    int disconnect_calls;
    int complete_calls;
    int rounds; /* Rounds of the tasks: calls of the other task */

    /* fake_us_tick when each read completed and each write was sent */
    unsigned int read_ticks[64];
    size_t n_reads;
    unsigned int write_ticks[64];
    size_t n_writes;
    struct ramdisk *heal; /* The other task ends heal's write faults */
    int heal_at; /* in this round */
} t;

int cm_transport_init(void)
{
    if (t.stopped) {
        /* The next host */
        t.stopped = false;
        t.stop_reads = false;
        t.write_stopped = false;
        t.stopped_polls = 0;
    }
    t.init_calls++;
    return (t.init_calls > t.max_inits) ? -PB_ERR_IO : PB_OK;
}

int cm_transport_connect(void)
{
    t.connect_calls++;
    if (t.connect_ok_after >= 0 && t.connect_calls > t.connect_ok_after)
        return PB_OK;
    return t.connect_rc;
}

int cm_transport_disconnect(void)
{
    t.disconnect_calls++;
    return PB_OK;
}

int cm_transport_read(void *buf, size_t length)
{
    if (t.fail_reads_after == 0)
        return -PB_ERR_IO;
    if (t.fail_reads_after > 0)
        t.fail_reads_after--;
    t.read_buf = buf;
    t.read_len = length;
    t.read_pending = true;
    return PB_OK;
}

int cm_transport_write(const void *buf, size_t length)
{
    if (t.fail_writes_after == 0)
        return -PB_ERR_IO;
    if (t.fail_writes_after > 0)
        t.fail_writes_after--;
    if (++t.write_calls == t.lose_write) {
        t.write_lost = true; /* complete() fails it, nothing arrives */
        return PB_OK;
    }
    if (t.write_calls == t.stop_write) {
        t.write_stopped = true; /* complete() never ends it */
        return PB_OK;
    }

    assert_true(t.out_len + length <= sizeof(t.out));
    memcpy(&t.out[t.out_len], buf, length);
    t.out_len += length;
    if (t.n_writes < ARRAY_SIZE(t.write_ticks))
        t.write_ticks[t.n_writes++] = fake_us_tick;
    return PB_OK;
}

int cm_transport_complete(void)
{
    t.complete_calls++;

    if (t.n_complete_rcs > 0) {
        int rc = t.complete_rcs[0];
        memmove(&t.complete_rcs[0], &t.complete_rcs[1], sizeof(int) * --t.n_complete_rcs);
        return rc;
    }

    if (t.write_lost) {
        t.write_lost = false;
        return -PB_ERR_IO;
    }

    if (t.write_stopped || (t.read_pending && t.stop_reads && t.in_pos == t.stop_at)) {
        t.stopped = true;
        /* pb gives up after its timeout instead of polling for ever */
        assert_true(++t.stopped_polls < STOPPED_POLLS_MAX);
        return -PB_ERR_AGAIN;
    }

    if (t.read_pending && t.n_lost > 0 && t.in_pos == t.lost_at[0]) {
        int rc = t.lost_rc[0];

        t.n_lost--;
        memmove(&t.lost_at[0], &t.lost_at[1], sizeof(t.lost_at[0]) * t.n_lost);
        memmove(&t.lost_rc[0], &t.lost_rc[1], sizeof(t.lost_rc[0]) * t.n_lost);
        t.read_pending = false;
        return rc;
    }

    if (t.read_pending) {
        if (t.in_pos + t.read_len > t.in_len)
            return -PB_ERR_IO; /* The host is gone */
        memcpy(t.read_buf, &t.in[t.in_pos], t.read_len);
        t.in_pos += t.read_len;
        t.read_pending = false;
        if (t.n_reads < ARRAY_SIZE(t.read_ticks))
            t.read_ticks[t.n_reads++] = fake_us_tick;
    }

    return PB_OK;
}

/* Another task of the build, which runs once per round whatever command
 * mode's task waits for (board.md 4.6) */
static int other_task(void)
{
    t.rounds++;
    if (t.heal != NULL && t.rounds == t.heal_at)
        t.heal->fail_writes_after = -1;
    return PB_OK;
}

/* The board */
static int board_init_rc;
static int board_status(uint8_t *buf, size_t *length);
static int
board_command(uint32_t cmd, uint8_t *bfr, size_t bfr_length, uint8_t *rsp, size_t *rsp_size);
static int password_auth(const char *password, size_t length);

static const struct cm_config cfg_default = {
    .name = "unit test",
    .status = board_status,
    .command = board_command,
};

static const struct cm_config cfg_all_callbacks = {
    .name = "unit test",
    .status = board_status,
    .command = board_command,
    .password_auth = password_auth,
};

static const struct cm_config *board_cfg;
static int board_init_calls;

int cm_board_init(void)
{
    board_init_calls++;
    if (board_init_rc != PB_OK)
        return board_init_rc;
    return cm_init(board_cfg);
}

/* The platform's part of the set up: the USB port (platform.md 3.10) */
static struct {
    int rc;
    int calls;
    int board_inits_before; /* cm_board_init() calls when it was called */
    int transport_inits_before;
} plat_cm;

int plat_cm_init(void)
{
    plat_cm.calls++;
    plat_cm.board_inits_before = board_init_calls;
    plat_cm.transport_inits_before = t.init_calls;
    return plat_cm.rc;
}

static int board_status_rc;

static int board_status(uint8_t *buf, size_t *length)
{
    *length = snprintf((char *)buf, *length, "status ok");
    return board_status_rc;
}

/* The room for a response that the board was given, per call */
static size_t board_rsp_room[8];
static unsigned int board_command_calls;

static int
board_command(uint32_t cmd, uint8_t *bfr, size_t bfr_length, uint8_t *rsp, size_t *rsp_size)
{
    if (board_command_calls < ARRAY_SIZE(board_rsp_room))
        board_rsp_room[board_command_calls] = *rsp_size;
    board_command_calls++;

    switch (cmd) {
    case BOARD_FILL: /* A board that doesn't look at the room it was given */
        memset(rsp, 0x42, BOARD_FILL_SIZE);
        *rsp_size = BOARD_FILL_SIZE;
        return PB_OK;
    case BOARD_ECHO:
        *rsp_size = snprintf((char *)rsp, *rsp_size, BOARD_PREFIX "%.*s", (int)bfr_length, bfr);
        return PB_OK;
    case BOARD_RETURN: /* Returns the error code given in the request */
        *rsp_size = 0;
        return *(int32_t *)bfr;
    case BOARD_REBOOT:
        *rsp_size = 0;
        cm_board_cmd_request_reboot_on_success();
        return PB_OK;
    default:
        *rsp_size = 0;
        return -PB_ERR;
    }
}

static int password_auth(const char *password, size_t length)
{
    return (length == 6 && memcmp(password, "secret", 6) == 0) ? PB_OK
                                                               : -PB_ERR_AUTHENTICATION_FAILED;
}

/* Fake boot, rot, slc and device UUID */
static struct {
    int load_rc;
    int load_calls;
    uuid_t load_uu;
    int jump_calls;
    int set_part_rc;
    uuid_t set_part_uu;
    uint32_t flags;
    enum boot_source source;
    bool reads_header; /* boot_load() reads the header and answers it */
    boot_read_cb_t read_f;
    boot_result_cb_t result_f;
} fake_boot;

void boot_clear_set_flags(uint32_t clear, uint32_t set)
{
    fake_boot.flags = (fake_boot.flags & ~clear) | set;
}

int boot_set_source(enum boot_source source)
{
    fake_boot.source = source;
    return PB_OK;
}

void boot_configure_load_cb(boot_read_cb_t read_f, boot_result_cb_t result_f)
{
    fake_boot.read_f = read_f;
    fake_boot.result_f = result_f;
}

int boot_load(uuid_t boot_part_override_uu)
{
    fake_boot.load_calls++;
    uuid_copy(fake_boot.load_uu, boot_part_override_uu);

    if (fake_boot.reads_header) {
        static struct bpak_header header;
        int rc = fake_boot.read_f(-(int64_t)sizeof(header), sizeof(header), &header);

        if (rc != PB_OK)
            return rc;
        fake_boot.result_f(fake_boot.load_rc);
    }

    return fake_boot.load_rc;
}

int boot_jump(void)
{
    fake_boot.jump_calls++;
    return -PB_ERR; /* The jump returned */
}

int boot_set_boot_partition(uuid_t part_uu)
{
    uuid_copy(fake_boot.set_part_uu, part_uu);
    return fake_boot.set_part_rc;
}

void boot_get_boot_partition(uuid_t part_uu)
{
    uuid_copy(part_uu, UU_RW);
}

static struct {
    slc_t slc;
    int set_rc;
    int key_status_rc;
    int get_key_rc;
    dsa_t key_kind;
    uint32_t revoked; /* Bit per key index */
    uint32_t key_io_error; /* Bit per key index */
    int revoke_rc;
    key_id_t revoked_id;
    size_t no_of_keys; /* 0: two keys */
} sec;

slc_t slc_read_status(void)
{
    return sec.slc;
}

int slc_set_configuration(void)
{
    if (sec.set_rc == PB_OK)
        sec.slc = SLC_CONFIGURATION;
    return sec.set_rc;
}

int slc_set_configuration_locked(void)
{
    if (sec.set_rc == PB_OK)
        sec.slc = SLC_CONFIGURATION_LOCKED;
    return sec.set_rc;
}

int slc_set_eol(void)
{
    if (sec.set_rc == PB_OK)
        sec.slc = SLC_EOL;
    return sec.set_rc;
}

int rot_read_key_status(key_id_t id)
{
    (void)id;
    return sec.key_status_rc;
}

int rot_get_dsa_key(key_id_t id, dsa_t *dsa_kind, const uint8_t **der_data, size_t *der_data_length)
{
    static const uint8_t der[] = { 0x30 };
    (void)id;
    *dsa_kind = sec.key_kind;
    *der_data = der;
    *der_data_length = sizeof(der);
    return sec.get_key_rc;
}

size_t rot_no_of_keys(void)
{
    return sec.no_of_keys ? sec.no_of_keys : 2;
}

int rot_read_key_status_by_idx(unsigned int index)
{
    if (sec.key_io_error & (1u << index))
        return -PB_ERR_IO;
    return (sec.revoked & (1u << index)) ? -PB_ERR_KEY_REVOKED : PB_OK;
}

key_id_t rot_key_idx_to_id(unsigned int index)
{
    return 0x1000 + index;
}

int rot_revoke_key(key_id_t id)
{
    sec.revoked_id = id;
    return sec.revoke_rc;
}

int device_uuid(uuid_t uu)
{
    return uuid_parse(DEVICE_UUID, uu);
}

/* Fake crypto providers, the digest is a running sum */
static struct {
    int init_rc;
    int update_rc;
    int update_rc_after; /* update_rc applies after this many updates */
    int updates;
    int final_rc;
    uint32_t sum;
    hash_t alg;
    hash_t verify_alg;
    bool verify_ok;
    unsigned int verify_us; /* What a verification takes on the fake clock */
    unsigned int update_kicks[8]; /* fake_wdog_kicks at each update */
} crypto;

static int hash_fake_init(hash_t alg)
{
    crypto.alg = alg;
    crypto.sum = 0;
    crypto.updates = 0;
    return crypto.init_rc;
}

static int hash_fake_update(const void *buf, size_t length)
{
    const uint8_t *p = buf;

    if (crypto.updates < (int)ARRAY_SIZE(crypto.update_kicks))
        crypto.update_kicks[crypto.updates] = fake_wdog_kicks;
    if (crypto.updates++ >= crypto.update_rc_after && crypto.update_rc != PB_OK)
        return crypto.update_rc;
    for (size_t i = 0; i < length; i++)
        crypto.sum = crypto.sum * 31 + p[i];
    return PB_OK;
}

static void sum_digest(uint8_t *out, size_t length, uint32_t sum)
{
    for (size_t i = 0; i < length; i++)
        out[i] = (uint8_t)(sum >> (8 * (i % 4)));
}

static int hash_fake_final(uint8_t *digest_out, size_t length)
{
    if (crypto.final_rc != PB_OK)
        return crypto.final_rc;
    sum_digest(digest_out, length, crypto.sum);
    return PB_OK;
}

static const struct hash_ops fake_hash_ops = {
    .name = "fake-hash",
    .alg_bits = HASH_SHA256 | HASH_SHA384 | HASH_SHA512,
    .init = hash_fake_init,
    .update = hash_fake_update,
    .final = hash_fake_final,
};

static int dsa_fake_verify(const uint8_t *der_signature,
                           size_t signature_length,
                           const uint8_t *der_key,
                           size_t key_length,
                           hash_t md_alg,
                           uint8_t *md,
                           size_t md_length,
                           bool *verified)
{
    (void)signature_length;
    (void)der_key;
    (void)key_length;
    (void)md;
    (void)md_length;
    crypto.verify_alg = md_alg;
    fake_us_tick += crypto.verify_us;
    *verified = crypto.verify_ok && der_signature[0] == FAKE_SIG_OK;
    return PB_OK;
}

static const struct dsa_ops fake_dsa_ops = {
    .name = "fake-dsa",
    .alg_bits = DSA_EC_SECP256r1 | DSA_EC_SECP384r1 | DSA_EC_SECP521r1,
    .verify = dsa_fake_verify,
};

/* The host's side */
static void host_send(const void *data, size_t length)
{
    assert_true(t.in_len + length <= sizeof(t.in));
    memcpy(&t.in[t.in_len], data, length);
    t.in_len += length;
}

static void host_cmd(uint8_t command, const void *request, size_t length)
{
    struct pb_command cmd;

    if (request != NULL)
        pb_wire_init_command2(&cmd, command, (void *)request, length);
    else
        pb_wire_init_command(&cmd, command);
    host_send(&cmd, sizeof(cmd));
}

/* An authentication token or password, always 1024 bytes on the wire */
static void host_auth(uint8_t method, const void *data, uint16_t size, uint32_t key_id)
{
    struct pb_command_authenticate auth = { .method = method, .size = size, .key_id = key_id };
    uint8_t buf[1024] = { 0 };

    host_cmd(PB_CMD_AUTHENTICATE, &auth, sizeof(auth));
    memcpy(buf, data, size);
    host_send(buf, sizeof(buf));
}

static void host_auth_token(void)
{
    static const uint8_t token[] = { FAKE_SIG_OK, 0x01 };
    host_auth(PB_AUTH_ASYM_TOKEN, token, sizeof(token), KEY_ID);
}

/*
 * The host goes away here: the device's next read fails with 'rc', the way
 * the transport reports an unplug, a bus reset (usb.md 2.4) or a client
 * that closed the socket (2.6). When the device waits for a command, cm_run()
 * starts the transport again, and what is queued after this is the next
 * host's.
 */
static void host_lost(int rc)
{
    assert_true(t.n_lost < (int)ARRAY_SIZE(t.lost_at));
    t.lost_at[t.n_lost] = t.in_len;
    t.lost_rc[t.n_lost] = rc;
    t.n_lost++;
    t.max_inits++;
}

/*
 * The host stops here, the connection still up: the device's next read
 * stays pending. Once the transport is started again, what is queued after
 * this is the next host's.
 */
static void host_stops(void)
{
    t.stop_reads = true;
    t.stop_at = t.in_len;
    t.max_inits++;
}

/* The loop of the tasks, as main() runs it while pb is stopped (board.md
 * 4.6): the watchdog is kicked once per round, and each task is called in
 * turn until one returns an error */
static int cm_run(void)
{
    int rc;

    do {
        plat_wdog_kick();
        rc = other_task();

        if (rc == PB_OK)
            rc = cm_main_task();
    } while (rc == PB_OK);

    return rc;
}

/* Runs command mode until the host is done */
static void run(void)
{
    assert_int_equal(cm_run(), -PB_ERR_IO);
}

static const struct pb_result *expect_result(int code)
{
    const struct pb_result *result = (const struct pb_result *)&t.out[t.out_pos];

    assert_true(t.out_pos + sizeof(*result) <= t.out_len);
    assert_int_equal(result->magic, PB_WIRE_MAGIC);
    assert_int_equal(result->result_code, code);
    t.out_pos += sizeof(*result);
    return result;
}

static const uint8_t *expect_data(size_t length)
{
    const uint8_t *data = &t.out[t.out_pos];

    assert_true(t.out_pos + length <= t.out_len);
    t.out_pos += length;
    return data;
}

static void expect_end(void)
{
    assert_int_equal(t.out_pos, t.out_len);
}

struct fixture {
    struct ramdisk *disk;
    bio_dev_t rw;
};

static int group_setup(void **state)
{
    (void)state;
    if (hash_add_ops(&fake_hash_ops) != PB_OK)
        return -1;
    if (dsa_add_ops(&fake_dsa_ops) != PB_OK)
        return -1;
    return 0;
}

static int setup(void **state)
{
    struct fixture *f = calloc(1, sizeof(*f));
    assert_non_null(f);
    f->disk = ramdisk_new(DISK_BLOCKS);
    assert_non_null(f->disk);

    bio_reset();
    bio_dev_t disk = ramdisk_attach(f->disk, UU_DISK);
    assert_true(disk >= 0);
    assert_int_equal(bio_set_flags(disk, 0), PB_OK);
    f->rw = bio_allocate_parent(disk, 10, 29, RAMDISK_BLOCK_SIZE, UU_RW, "Read write");
    bio_dev_t ro = bio_allocate_parent(disk, 30, 39, RAMDISK_BLOCK_SIZE, UU_RO, "Visible");
    bio_dev_t hidden = bio_allocate_parent(disk, 40, 49, RAMDISK_BLOCK_SIZE, UU_HIDDEN, "Hidden");
    assert_true(f->rw >= 0 && ro >= 0 && hidden >= 0);
    assert_int_equal(bio_set_flags(f->rw, BIO_FLAG_VISIBLE | BIO_FLAG_READABLE | BIO_FLAG_WRITABLE),
                     PB_OK);
    assert_int_equal(bio_set_flags(ro, BIO_FLAG_VISIBLE), PB_OK);

    memset(&t, 0, sizeof(t));
    t.max_inits = 1;
    t.fail_reads_after = -1;
    t.fail_writes_after = -1;
    t.connect_ok_after = -1;
    memset(&fake_boot, 0, sizeof(fake_boot));
    memset(&sec, 0, sizeof(sec));
    sec.slc = SLC_CONFIGURATION;
    sec.key_kind = DSA_EC_SECP256r1;
    memset(&crypto, 0, sizeof(crypto));
    crypto.verify_ok = true;
    board_cfg = &cfg_default;
    board_init_rc = PB_OK;
    board_init_calls = 0;
    board_command_calls = 0;
    memset(&plat_cm, 0, sizeof(plat_cm));
    board_status_rc = PB_OK;
    /* The clock runs: a failed authentication waits on it (F-276) */
    fake_us_tick_step = TICK_STEP_US;

    *state = f;
    return 0;
}

static int teardown(void **state)
{
    struct fixture *f = *state;
    ramdisk_free(f->disk);
    free(f);
    fake_us_tick = FAKE_US_TICK_DEFAULT;
    fake_us_tick_step = 0;
    return 0;
}

static void test_basic_commands(void **state)
{
    (void)state;

    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    host_cmd(PB_CMD_DEVICE_READ_CAPS, NULL, 0);
    host_cmd(PB_CMD_DEVICE_IDENTIFIER_READ, NULL, 0);
    run();

    const struct pb_result *r = expect_result(PB_RESULT_OK);
    assert_string_equal((const char *)r->response, PB_VERSION);

    r = expect_result(PB_RESULT_OK);
    const struct pb_result_device_caps *caps = (const void *)r->response;
    assert_int_equal(caps->stream_no_of_buffers, 2);
    assert_int_equal(caps->stream_buffer_size, BUF_SIZE);
    /* pb's transfer timeout; no bound on an erase (command-mode.md 4.6) */
    assert_int_equal(caps->operation_timeout_ms, CONFIG_CM_TRANSFER_TIMEOUT * 1000);
    assert_int_equal(caps->part_erase_timeout_ms, 0);

    r = expect_result(PB_RESULT_OK);
    const struct pb_result_device_identifier *id = (const void *)r->response;
    assert_string_equal(id->board_id, "unit test");
    uuid_t uu;
    uuid_parse(DEVICE_UUID, uu);
    assert_memory_equal(id->device_uuid, uu, 16);
    expect_end();
}

/* The longest name a board may have (board.md 3: cm_board_init() in
 * src/board.c lets 1 to 16 characters through) fills the wire's board_id:
 * its 16 bytes and no terminator. The name is copied with its own length,
 * so the check is what keeps it inside the field. */
static void test_board_id_fills_the_field(void **state)
{
    static const struct cm_config cfg_longest_name = {
        .name = "16 characters...",
        .status = board_status,
        .command = board_command,
    };
    (void)state;

    board_cfg = &cfg_longest_name;
    host_cmd(PB_CMD_DEVICE_IDENTIFIER_READ, NULL, 0);
    run();

    const struct pb_result *r = expect_result(PB_RESULT_OK);
    const struct pb_result_device_identifier *id = (const void *)r->response;
    assert_int_equal(sizeof(id->board_id), 16);
    assert_int_equal(strlen(cfg_longest_name.name), sizeof(id->board_id));
    assert_memory_equal(id->board_id, "16 characters...", sizeof(id->board_id));
    uuid_t uu;
    uuid_parse(DEVICE_UUID, uu);
    assert_memory_equal(id->device_uuid, uu, 16);
    expect_end();
}

/* command-mode.md 4.3 (F-190): a result's reserved bytes and every byte the
 * command didn't produce are zero. The identifier is answered before
 * authentication, and the buffer then holds what the last host sent: here a
 * token that didn't verify. */
static void test_identifier_sends_only_the_identifier(void **state)
{
    static const uint8_t zeros[PB_RESULT_RESPONSE_MAX_SIZE];
    uint8_t token[1024];
    (void)state;

    memset(token, 0xa5, sizeof(token));
    sec.slc = SLC_CONFIGURATION_LOCKED;
    host_auth(PB_AUTH_ASYM_TOKEN, token, sizeof(token), KEY_ID);
    host_cmd(PB_CMD_DEVICE_IDENTIFIER_READ, NULL, 0);
    run();

    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_AUTHENTICATION_FAILED);
    const struct pb_result *r = expect_result(PB_RESULT_OK);
    const struct pb_result_device_identifier *id = (const void *)r->response;
    uuid_t uu;
    uuid_parse(DEVICE_UUID, uu);
    assert_memory_equal(id->device_uuid, uu, 16);
    assert_memory_equal(id->board_id, "unit test\0\0\0\0\0\0\0", sizeof(id->board_id));
    assert_memory_equal(id->rz, zeros, sizeof(id->rz));
    assert_memory_equal(r->rz, zeros, sizeof(r->rz));
    assert_memory_equal(r->response + sizeof(*id), zeros, sizeof(r->response) - sizeof(*id));
    expect_end();
}

static void test_invalid_command(void **state)
{
    struct pb_command cmd;
    (void)state;

    pb_wire_init_command(&cmd, PB_CMD_BOOTLOADER_VERSION_READ);
    cmd.magic = 0;
    host_send(&cmd, sizeof(cmd));
    pb_wire_init_command(&cmd, PB_CMD_END);
    host_send(&cmd, sizeof(cmd));
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();

    expect_result(-PB_RESULT_INVALID_COMMAND);
    expect_result(-PB_RESULT_INVALID_COMMAND);
    expect_result(PB_RESULT_OK);
    expect_end();
}

static void test_deprecated_commands(void **state)
{
    (void)state;

    host_cmd(PB_CMD_AUTH_SET_PASSWORD, NULL, 0);
    host_cmd(PB_CMD_PART_RESIZE, NULL, 0);
    run();

    expect_result(-PB_RESULT_NOT_SUPPORTED);
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    expect_end();
}

static void test_part_table_read(void **state)
{
    (void)state;

    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    run();

    const struct pb_result *r = expect_result(PB_RESULT_OK);
    const struct pb_result_part_table_read *tbl = (const void *)r->response;
    /* The disk and the hidden partition are left out */
    assert_int_equal(tbl->no_of_entries, 2);

    const struct pb_result_part_table_entry *e = (const void *)expect_data(2 * sizeof(*e));
    assert_memory_equal(e[0].uuid, UU_RW, 16);
    assert_string_equal(e[0].description, "Read write");
    assert_int_equal(e[0].first_block, 10);
    assert_int_equal(e[0].last_block, 29);
    assert_int_equal(e[0].block_size, RAMDISK_BLOCK_SIZE);
    assert_int_equal(e[0].flags, BIO_FLAG_VISIBLE | BIO_FLAG_READABLE | BIO_FLAG_WRITABLE);
    assert_memory_equal(e[1].uuid, UU_RO, 16);

    expect_result(PB_RESULT_OK);
    expect_end();
}

static void test_part_table_install(void **state)
{
    struct pb_command_install_part_table req = { .variant = 1 };
    (void)state;

    /* The device a partition table is on has only VISIBLE (F-265); a hidden
     * one isn't there for the host (F-272) */
    assert_int_equal(bio_set_flags(bio_get_part_by_uu(UU_DISK), BIO_FLAG_VISIBLE), PB_OK);
    memcpy(req.uu, UU_MISSING, 16);
    host_cmd(PB_CMD_PART_TBL_INSTALL, &req, sizeof(req));
    memcpy(req.uu, UU_DISK, 16);
    host_cmd(PB_CMD_PART_TBL_INSTALL, &req, sizeof(req));
    run();

    expect_result(-PB_RESULT_NOT_FOUND);
    /* The ramdisk has no partition table support */
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    expect_end();
}

static void test_part_activate(void **state)
{
    struct pb_command_activate_part req = { 0 };
    (void)state;

    memcpy(req.uuid, UU_RW, 16);
    host_cmd(PB_CMD_PART_ACTIVATE, &req, sizeof(req));
    fake_boot.set_part_rc = PB_OK;
    run();

    expect_result(PB_RESULT_OK);
    assert_memory_equal(fake_boot.set_part_uu, UU_RW, 16);
    expect_end();
}

static void test_boot_status(void **state)
{
    (void)state;

    host_cmd(PB_CMD_BOOT_STATUS, NULL, 0);
    run();

    const struct pb_result *r = expect_result(PB_RESULT_OK);
    const struct pb_result_boot_status *status = (const void *)r->response;
    assert_memory_equal(status->uuid, UU_RW, 16);
    expect_end();
}

static void test_board_status(void **state)
{
    (void)state;

    host_cmd(PB_CMD_BOARD_STATUS_READ, NULL, 0);
    run();

    const struct pb_result *r = expect_result(PB_RESULT_OK);
    const struct pb_result_board_status *status = (const void *)r->response;
    assert_int_equal(status->size, strlen("status ok"));
    assert_memory_equal(expect_data(status->size), "status ok", status->size);
    expect_result(PB_RESULT_OK);
    expect_end();
}

/* A status that fails is answered with the error alone: released hosts
 * stop at it (command-mode.md 4.4, F-274) */
static void test_board_status_fails(void **state)
{
    (void)state;

    board_status_rc = -PB_ERR_IO;
    host_cmd(PB_CMD_BOARD_STATUS_READ, NULL, 0);
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();

    const struct pb_result *r = expect_result(-PB_RESULT_IO_ERROR);
    assert_int_equal(((const struct pb_result_board_status *)r->response)->size, 0);
    expect_result(PB_RESULT_OK);
    expect_end();
}

static void test_board_command(void **state)
{
    struct pb_command_board req = { .command = BOARD_ECHO,
                                    .request_size = 5,
                                    .response_buffer_size = HOST_RESPONSE_SIZE };
    uint8_t arg[1024] = "hello";
    (void)state;

    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    host_send(arg, sizeof(arg));
    /* Unknown command: an error without a response */
    req.command = 0xdead;
    req.request_size = 0;
    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    run();

    expect_result(PB_RESULT_OK);
    const struct pb_result *r = expect_result(PB_RESULT_OK);
    const struct pb_result_board *board = (const void *)r->response;
    const char *expected = BOARD_PREFIX "hello";
    assert_int_equal(board->size, strlen(expected));
    assert_memory_equal(expect_data(board->size), expected, board->size);
    expect_result(PB_RESULT_OK);

    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_ERROR);
    expect_end();
}

/* F-063: the request comes in one 1024 byte transfer. A larger size is
 * refused without calling the board, after the transfer, which 2.x hosts
 * send without checking the first result. */
static void test_board_command_request_size(void **state)
{
    static const uint32_t bad_sizes[] = { 1025, 0xffffffff };
    struct pb_command_board req = { .command = BOARD_ECHO,
                                    .request_size = 1024,
                                    .response_buffer_size = HOST_RESPONSE_SIZE };
    uint8_t arg[1024];
    (void)state;

    memset(arg, 'a', sizeof(arg));
    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    host_send(arg, sizeof(arg));
    for (size_t i = 0; i < ARRAY_SIZE(bad_sizes); i++) {
        req.request_size = bad_sizes[i];
        host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
        host_send(arg, sizeof(arg));
    }
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();

    expect_result(PB_RESULT_OK);
    const struct pb_result *r = expect_result(PB_RESULT_OK);
    const struct pb_result_board *board = (const void *)r->response;
    assert_int_equal(board->size, strlen(BOARD_PREFIX) + sizeof(arg));
    expect_data(board->size);
    expect_result(PB_RESULT_OK);
    for (size_t i = 0; i < ARRAY_SIZE(bad_sizes); i++) {
        expect_result(PB_RESULT_OK);
        r = expect_result(-PB_RESULT_INVALID_ARGUMENT);
        board = (const void *)r->response;
        assert_int_equal(board->size, 0);
    }
    expect_result(PB_RESULT_OK);
    expect_end();
}

/* command-mode.md 5.6 (F-095): the response is at most what the host said it
 * can take (response_buffer_size) and at most a buffer. The board is given
 * that as its room, and a board that reports more is cut there: a host
 * leaves a larger response on the link. A host that can take nothing gets
 * no response transfer, and the exchange stays in step. */
static void test_board_response_fits_the_hosts_buffer(void **state)
{
    struct pb_command_board req = { .command = BOARD_ECHO,
                                    .request_size = 5,
                                    .response_buffer_size = 8 };
    uint8_t arg[1024] = "hello";
    uint8_t fill[BOARD_FILL_SIZE];
    (void)state;

    memset(fill, 0x42, sizeof(fill));

    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    host_send(arg, sizeof(arg));
    req.command = BOARD_FILL;
    req.request_size = 0;
    req.response_buffer_size = 16;
    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    req.response_buffer_size = 0;
    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    /* More than a buffer: the board has a buffer */
    req.response_buffer_size = 0xffffffff;
    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();

    /* snprintf() wrote 7 characters and the NUL, and returned 11 */
    expect_result(PB_RESULT_OK);
    const struct pb_result *r = expect_result(PB_RESULT_OK);
    assert_int_equal(((const struct pb_result_board *)r->response)->size, 8);
    assert_memory_equal(expect_data(8), "echo: h", 8);
    expect_result(PB_RESULT_OK);
    assert_int_equal(board_rsp_room[0], 8);

    expect_result(PB_RESULT_OK);
    r = expect_result(PB_RESULT_OK);
    assert_int_equal(((const struct pb_result_board *)r->response)->size, 16);
    assert_memory_equal(expect_data(16), fill, 16);
    expect_result(PB_RESULT_OK);
    assert_int_equal(board_rsp_room[1], 16);

    /* No response: the second result is the last */
    expect_result(PB_RESULT_OK);
    r = expect_result(PB_RESULT_OK);
    assert_int_equal(((const struct pb_result_board *)r->response)->size, 0);
    assert_int_equal(board_rsp_room[2], 0);

    expect_result(PB_RESULT_OK);
    r = expect_result(PB_RESULT_OK);
    assert_int_equal(((const struct pb_result_board *)r->response)->size, BOARD_FILL_SIZE);
    assert_memory_equal(expect_data(BOARD_FILL_SIZE), fill, BOARD_FILL_SIZE);
    expect_result(PB_RESULT_OK);
    assert_int_equal(board_rsp_room[3], BUF_SIZE);

    expect_result(PB_RESULT_OK);
    expect_end();
}

/* Every pb error code maps to a wire result */
static void test_error_codes(void **state)
{
    static const struct {
        int32_t error;
        int result;
    } map[] = {
        { PB_OK, PB_RESULT_OK },
        { -PB_ERR, -PB_RESULT_ERROR },
        { -PB_ERR_TIMEOUT, -PB_RESULT_TIMEOUT },
        { -PB_ERR_KEY_REVOKED, -PB_RESULT_KEY_REVOKED },
        { -PB_ERR_SIGNATURE, -PB_RESULT_SIGNATURE_ERROR },
        { -PB_ERR_CHECKSUM, -PB_RESULT_ERROR },
        { -PB_ERR_MEM, -PB_RESULT_MEM_ERROR },
        { -PB_ERR_IO, -PB_RESULT_IO_ERROR },
        { -PB_ERR_NOT_FOUND, -PB_RESULT_NOT_FOUND },
        { -PB_ERR_NOT_AUTHENTICATED, -PB_RESULT_NOT_AUTHENTICATED },
        { -PB_ERR_AUTHENTICATION_FAILED, -PB_RESULT_AUTHENTICATION_FAILED },
        { -PB_ERR_INVALID_ARGUMENT, -PB_RESULT_INVALID_ARGUMENT },
        { -PB_ERR_INVALID_COMMAND, -PB_RESULT_INVALID_COMMAND },
        { -PB_ERR_PART_NOT_FOUND, -PB_RESULT_NOT_FOUND },
        { -PB_ERR_PART_NOT_BOOTABLE, -PB_RESULT_PART_NOT_BOOTABLE },
        { -PB_ERR_PARAM, -PB_RESULT_INVALID_ARGUMENT },
        { -PB_ERR_NOT_SUPPORTED, -PB_RESULT_NOT_SUPPORTED },
        { -PB_ERR_BAD_HEADER, -PB_RESULT_ERROR },
    };
    struct pb_command_board req = { .command = BOARD_RETURN, .request_size = 4 };
    uint8_t arg[1024] = { 0 };
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(map); i++) {
        host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
        memcpy(arg, &map[i].error, sizeof(map[i].error));
        host_send(arg, sizeof(arg));
    }
    run();

    for (size_t i = 0; i < ARRAY_SIZE(map); i++) {
        expect_result(PB_RESULT_OK);
        expect_result(map[i].result);
    }
    expect_end();
}

static void test_board_command_requests_reboot(void **state)
{
    struct pb_command_board req = { .command = BOARD_REBOOT };
    (void)state;

    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    assert_int_equal(cm_run(), -PB_ERR_ABORT);

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    /* The next command is never read */
    expect_end();
    /* The device leaves the bus before the reset (command-mode.md 2.3) */
    assert_int_equal(t.disconnect_calls, 1);
}

static void test_device_reset(void **state)
{
    (void)state;

    host_cmd(PB_CMD_DEVICE_RESET, NULL, 0);
    assert_int_equal(cm_run(), -PB_ERR_ABORT);
    expect_result(PB_RESULT_OK);
    expect_end();
    /* The result is sent, then the transport is disconnected, then pb
     * resets (command-mode.md 2.3) */
    assert_int_equal(t.disconnect_calls, 1);
}

static void test_locked_device_requires_auth(void **state)
{
    (void)state;

    sec.slc = SLC_CONFIGURATION_LOCKED;
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    /* Open commands */
    host_cmd(PB_CMD_DEVICE_IDENTIFIER_READ, NULL, 0);
    host_auth_token();
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();

    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_end();
    assert_int_equal(crypto.alg, HASH_SHA256);
}

/*
 * A board init that failed before slc_init(), or an SLC that can't be read:
 * the device may be locked, so command mode must not be open. A board
 * without SLC support stays open.
 */
static void test_unknown_slc_requires_auth(void **state)
{
    static const slc_t unknown[] = { -PB_ERR_STATE, -PB_ERR_IO };
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(unknown); i++) {
        sec.slc = unknown[i];
        host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
        host_cmd(PB_CMD_DEVICE_IDENTIFIER_READ, NULL, 0);
        run();
        expect_result(-PB_RESULT_NOT_AUTHENTICATED);
        expect_result(PB_RESULT_OK);
        expect_end();
        t.init_calls = 0;
    }

    sec.slc = -PB_ERR_NOT_SUPPORTED;
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();
    expect_result(PB_RESULT_OK);
    expect_end();
}

static void test_auth_token_key_kinds(void **state)
{
    static const struct {
        dsa_t kind;
        hash_t hash;
    } kinds[] = {
        { DSA_EC_SECP384r1, HASH_SHA384 },
        { DSA_EC_SECP521r1, HASH_SHA512 },
    };

    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(kinds); i++) {
        sec.key_kind = kinds[i].kind;
        host_auth_token();
        run();
        expect_result(PB_RESULT_OK);
        expect_result(PB_RESULT_OK);
        assert_int_equal(crypto.verify_alg, kinds[i].hash);

        /* Next session */
        t.init_calls = 0;
    }
}

static void test_auth_token_errors(void **state)
{
    static const uint8_t bad_token[] = { 0x00 };
    (void)state;

    sec.slc = SLC_CONFIGURATION_LOCKED;

    sec.key_status_rc = -PB_ERR_KEY_REVOKED;
    host_auth_token();
    run();
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_KEY_REVOKED);
    sec.key_status_rc = PB_OK;

    t.init_calls = 0;
    sec.get_key_rc = -PB_ERR_KEY_NOT_FOUND;
    host_auth_token();
    run();
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_ERROR);
    sec.get_key_rc = PB_OK;

    t.init_calls = 0;
    sec.key_kind = 0;
    host_auth_token();
    run();
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    sec.key_kind = DSA_EC_SECP256r1;

    t.init_calls = 0;
    crypto.init_rc = -PB_ERR_NOT_SUPPORTED;
    host_auth_token();
    run();
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    crypto.init_rc = PB_OK;

    t.init_calls = 0;
    crypto.update_rc = -PB_ERR_IO;
    host_auth_token();
    run();
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_IO_ERROR);
    crypto.update_rc = PB_OK;

    t.init_calls = 0;
    crypto.final_rc = -PB_ERR_IO;
    host_auth_token();
    run();
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_IO_ERROR);
    crypto.final_rc = PB_OK;

    /* A bad signature, and the device stays locked */
    t.init_calls = 0;
    host_auth(PB_AUTH_ASYM_TOKEN, bad_token, sizeof(bad_token), KEY_ID);
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    run();
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_AUTHENTICATION_FAILED);
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_end();
}

static void test_auth_password(void **state)
{
    (void)state;

    sec.slc = SLC_CONFIGURATION_LOCKED;

    /* Not supported by the default board */
    host_auth(PB_AUTH_PASSWORD, "secret", 6, 0);
    run();
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    /* The 1024 bytes of password data are read as two frames, with a wrong
     * magic, which the closed device refuses before authentication (F-275) */
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_end();

    t.init_calls = 0;
    t.out_len = t.out_pos = 0;
    t.in_len = t.in_pos = 0;
    board_cfg = &cfg_all_callbacks;
    t.connect_ok_after = 0;
    host_auth(PB_AUTH_PASSWORD, "wrong!", 6, 0);
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    host_auth(PB_AUTH_PASSWORD, "secret", 6, 0);
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    struct pb_command_authenticate invalid = { .method = PB_AUTH_INVALID };
    host_cmd(PB_CMD_AUTHENTICATE, &invalid, sizeof(invalid));
    run();

    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_AUTHENTICATION_FAILED);
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    expect_end();
}

/* F-005: hosts send the data in one 1024 byte transfer, and none for size 0.
 * A bad size is refused before the data and changes nothing (F-278): an
 * authenticated session stays so, and one that isn't stays locked. */
static void test_auth_data_size(void **state)
{
    static const uint8_t methods[] = { PB_AUTH_ASYM_TOKEN, PB_AUTH_PASSWORD };
    static const uint16_t bad_sizes[] = { 0, 1025, 0xffff };
    uint8_t token[1024] = { FAKE_SIG_OK };
    (void)state;

    sec.slc = SLC_CONFIGURATION_LOCKED;
    board_cfg = &cfg_all_callbacks;
    t.connect_ok_after = 0;

    /* The largest token */
    host_auth(PB_AUTH_ASYM_TOKEN, token, sizeof(token), KEY_ID);
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);

    for (size_t m = 0; m < ARRAY_SIZE(methods); m++) {
        for (size_t s = 0; s < ARRAY_SIZE(bad_sizes); s++) {
            struct pb_command_authenticate auth = {
                .method = methods[m],
                .size = bad_sizes[s],
                .key_id = KEY_ID,
            };
            host_cmd(PB_CMD_AUTHENTICATE, &auth, sizeof(auth));
            host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
        }
    }
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    for (size_t i = 0; i < ARRAY_SIZE(methods) * ARRAY_SIZE(bad_sizes); i++) {
        expect_result(-PB_RESULT_INVALID_ARGUMENT);
        expect_result(PB_RESULT_OK);
    }
    expect_end();

    /* cm_run() again: not authenticated */
    t.init_calls = 0;
    t.out_len = t.out_pos = 0;
    t.in_len = t.in_pos = 0;
    for (size_t m = 0; m < ARRAY_SIZE(methods); m++) {
        struct pb_command_authenticate auth = { .method = methods[m], .size = 0 };
        host_cmd(PB_CMD_AUTHENTICATE, &auth, sizeof(auth));
        host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    }
    run();

    for (size_t m = 0; m < ARRAY_SIZE(methods); m++) {
        expect_result(-PB_RESULT_INVALID_ARGUMENT);
        expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    }
    expect_end();
}

/* command-mode.md 3.3 (F-278): a refusal before the data (an unknown method,
 * a board without a password check, a bad size) tried nothing and leaves an
 * authenticated session so. A failed attempt with data ends it. */
static void test_auth_refusal_keeps_session(void **state)
{
    static const uint8_t bad_token[] = { 0x00 };
    struct pb_command_authenticate invalid = { .method = PB_AUTH_INVALID, .size = 6 };
    struct pb_command_authenticate unknown = { .method = 0x7f, .size = 6 };
    struct pb_command_authenticate password = { .method = PB_AUTH_PASSWORD, .size = 6 };
    struct pb_command_authenticate token_size = { .method = PB_AUTH_ASYM_TOKEN,
                                                  .size = 1025,
                                                  .key_id = KEY_ID };
    struct pb_command_authenticate password_size = { .method = PB_AUTH_PASSWORD, .size = 0 };
    (void)state;

    sec.slc = SLC_CONFIGURATION_LOCKED;

    /* The default board has no password check: no data follows */
    host_auth_token();
    host_cmd(PB_CMD_AUTHENTICATE, &password, sizeof(password));
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    expect_result(PB_RESULT_OK);
    expect_end();

    t.init_calls = 0;
    t.out_len = t.out_pos = 0;
    t.in_len = t.in_pos = 0;
    board_cfg = &cfg_all_callbacks;
    t.connect_ok_after = 0;
    host_auth(PB_AUTH_PASSWORD, "secret", 6, 0);
    host_cmd(PB_CMD_AUTHENTICATE, &invalid, sizeof(invalid));
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    host_cmd(PB_CMD_AUTHENTICATE, &unknown, sizeof(unknown));
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    host_cmd(PB_CMD_AUTHENTICATE, &token_size, sizeof(token_size));
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    host_cmd(PB_CMD_AUTHENTICATE, &password_size, sizeof(password_size));
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    /* Data that arrived and didn't verify: no longer authenticated */
    host_auth(PB_AUTH_PASSWORD, "wrong!", 6, 0);
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    host_auth_token();
    host_auth(PB_AUTH_ASYM_TOKEN, bad_token, sizeof(bad_token), KEY_ID);
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_INVALID_ARGUMENT);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_INVALID_ARGUMENT);
    expect_result(PB_RESULT_OK);

    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_AUTHENTICATION_FAILED);
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_AUTHENTICATION_FAILED);
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_end();
}

/* F-005: when the data doesn't arrive, the device must not check whatever the
 * buffer held from the last authentication. The transfer failed, so nothing
 * more is sent and the transport is restarted (command-mode.md 4.4) */
static void test_auth_data_read_fails(void **state)
{
    struct pb_command_authenticate token = { .method = PB_AUTH_ASYM_TOKEN,
                                             .size = 2,
                                             .key_id = KEY_ID };
    struct pb_command_authenticate password = { .method = PB_AUTH_PASSWORD, .size = 6 };
    (void)state;

    host_auth_token();
    host_cmd(PB_CMD_AUTHENTICATE, &token, sizeof(token));
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_end();

    t.init_calls = 0;
    t.out_len = t.out_pos = 0;
    t.in_len = t.in_pos = 0;
    board_cfg = &cfg_all_callbacks;
    t.connect_ok_after = 0;
    host_auth(PB_AUTH_PASSWORD, "secret", 6, 0);
    host_cmd(PB_CMD_AUTHENTICATE, &password, sizeof(password));
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_end();
}

/* Forgets what the last cm_run() read and sent, for the next one */
static void clear_transport(void)
{
    t.init_calls = 0;
    t.out_len = t.out_pos = 0;
    t.in_len = t.in_pos = 0;
    t.n_reads = t.n_writes = 0;
}

/* The time from the authentication data's arrival (the second read) to its
 * result (the second write) */
static unsigned int auth_answer_us(void)
{
    assert_true(t.n_reads >= 2 && t.n_writes >= 2);
    return t.write_ticks[1] - t.read_ticks[1];
}

/* command-mode.md 3.3 (F-276): the result of a failed attempt is sent 1 s
 * after the data arrived, with every method, and the watchdog is kicked
 * meanwhile */
static void test_auth_failure_is_delayed(void **state)
{
    static const uint8_t bad_token[] = { 0x00 };

    static const struct {
        int key_status_rc;
        int get_key_rc;
        int result;
    } token_failures[] = {
        { PB_OK, PB_OK, -PB_RESULT_AUTHENTICATION_FAILED },
        { -PB_ERR_KEY_REVOKED, PB_OK, -PB_RESULT_KEY_REVOKED },
        { PB_OK, -PB_ERR_KEY_NOT_FOUND, -PB_RESULT_ERROR },
    };

    (void)state;

    sec.slc = SLC_CONFIGURATION_LOCKED;
    board_cfg = &cfg_all_callbacks;
    t.connect_ok_after = 0;

    for (size_t i = 0; i <= ARRAY_SIZE(token_failures); i++) {
        unsigned int kicks = fake_wdog_kicks;
        int result = -PB_RESULT_AUTHENTICATION_FAILED;

        clear_transport();
        fake_wdog_kick_tick = fake_us_tick;
        fake_wdog_max_gap_us = 0;

        if (i < ARRAY_SIZE(token_failures)) {
            sec.key_status_rc = token_failures[i].key_status_rc;
            sec.get_key_rc = token_failures[i].get_key_rc;
            result = token_failures[i].result;
            host_auth(PB_AUTH_ASYM_TOKEN, bad_token, sizeof(bad_token), KEY_ID);
        } else {
            host_auth(PB_AUTH_PASSWORD, "wrong!", 6, 0);
        }
        run();

        expect_result(PB_RESULT_OK);
        expect_result(result);
        expect_end();
        assert_in_range(auth_answer_us(), AUTH_WAIT_US, AUTH_WAIT_US + 2 * TICK_STEP_US);
        /* At once, then every 100 ms */
        assert_true(fake_wdog_kicks - kicks >= AUTH_WAIT_US / AUTH_KICK_US + 1);
        assert_true(fake_wdog_max_gap_us <= AUTH_KICK_US + 2 * TICK_STEP_US);
    }
}

/* The delay counts from the data's arrival: a slow check shortens the rest of
 * it, one that took longer than the delay isn't held, and the clock may wrap
 * meanwhile */
static void test_auth_failure_delay_counts_from_the_data(void **state)
{
    static const uint8_t bad_token[] = { 0x00 };
    (void)state;

    sec.slc = SLC_CONFIGURATION_LOCKED;

    /* A check of 300 ms leaves 700 ms */
    crypto.verify_us = 300000;
    host_auth(PB_AUTH_ASYM_TOKEN, bad_token, sizeof(bad_token), KEY_ID);
    run();
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_AUTHENTICATION_FAILED);
    assert_in_range(auth_answer_us(), AUTH_WAIT_US, AUTH_WAIT_US + 2 * TICK_STEP_US);

    /* A check of 1.5 s is answered when it ends */
    clear_transport();
    crypto.verify_us = 1500000;
    host_auth(PB_AUTH_ASYM_TOKEN, bad_token, sizeof(bad_token), KEY_ID);
    run();
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_AUTHENTICATION_FAILED);
    assert_in_range(auth_answer_us(), 1500000, 1500000 + 2 * TICK_STEP_US);

    /* The tick wraps half a second after the data */
    clear_transport();
    crypto.verify_us = 0;
    fake_us_tick = UINT32_MAX - 500000;
    host_auth(PB_AUTH_ASYM_TOKEN, bad_token, sizeof(bad_token), KEY_ID);
    run();
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_AUTHENTICATION_FAILED);
    assert_true(t.read_ticks[1] > t.write_ticks[1]);
    assert_in_range(auth_answer_us(), AUTH_WAIT_US, AUTH_WAIT_US + 2 * TICK_STEP_US);
}

/* A success, and a refusal before the data, are answered at once */
static void test_auth_not_delayed(void **state)
{
    struct pb_command_authenticate bad_size = { .method = PB_AUTH_ASYM_TOKEN, .size = 0 };
    struct pb_command_authenticate unknown = { .method = 0x7f, .size = 6 };
    struct pb_command_authenticate password = { .method = PB_AUTH_PASSWORD, .size = 6 };
    (void)state;

    sec.slc = SLC_CONFIGURATION_LOCKED;

    host_auth_token();
    run();
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    assert_true(auth_answer_us() <= 2 * TICK_STEP_US);

    clear_transport();
    board_cfg = &cfg_all_callbacks;
    t.connect_ok_after = 0;
    host_auth(PB_AUTH_PASSWORD, "secret", 6, 0);
    run();
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    assert_true(auth_answer_us() <= 2 * TICK_STEP_US);

    /* From the command to its only result: a bad size, an unknown method,
     * and the password on a board without a check */
    clear_transport();
    host_cmd(PB_CMD_AUTHENTICATE, &bad_size, sizeof(bad_size));
    host_cmd(PB_CMD_AUTHENTICATE, &unknown, sizeof(unknown));
    run();
    expect_result(-PB_RESULT_INVALID_ARGUMENT);
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    expect_end();
    assert_true(t.write_ticks[0] - t.read_ticks[0] <= 2 * TICK_STEP_US);
    assert_true(t.write_ticks[1] - t.read_ticks[1] <= 2 * TICK_STEP_US);

    clear_transport();
    board_cfg = &cfg_default;
    host_cmd(PB_CMD_AUTHENTICATE, &password, sizeof(password));
    run();
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    expect_end();
    assert_true(t.write_ticks[0] - t.read_ticks[0] <= 2 * TICK_STEP_US);
}

static void test_slc_commands(void **state)
{
    struct pb_command_revoke_key revoke = { .key_id = 0x1001 };
    (void)state;

    sec.revoked = 1u << 0;
    host_cmd(PB_CMD_SLC_READ, NULL, 0);
    host_cmd(PB_CMD_SLC_SET_CONFIGURATION, NULL, 0);
    host_cmd(PB_CMD_SLC_REVOKE_KEY, &revoke, sizeof(revoke));
    host_cmd(PB_CMD_SLC_SET_CONFIGURATION_LOCK, NULL, 0);
    /* Locked from now on */
    host_cmd(PB_CMD_SLC_SET_EOL, NULL, 0);
    host_auth_token();
    host_cmd(PB_CMD_SLC_SET_EOL, NULL, 0);
    run();

    const struct pb_result *r = expect_result(PB_RESULT_OK);
    assert_int_equal(((const struct pb_result_slc *)r->response)->slc, SLC_CONFIGURATION);
    const struct pb_result_slc_key_status *keys = (const void *)expect_data(sizeof(*keys));
    assert_int_equal(keys->revoked[0], 0x1000);
    assert_int_equal(keys->active[0], 0);
    assert_int_equal(keys->active[1], 0x1001);
    expect_result(PB_RESULT_OK);

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    assert_int_equal(sec.revoked_id, 0x1001);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    assert_int_equal(sec.slc, SLC_EOL);
    expect_end();
}

static void test_slc_errors(void **state)
{
    (void)state;

    sec.set_rc = -PB_ERR_IO;
    host_cmd(PB_CMD_SLC_SET_EOL, NULL, 0);
    run();
    expect_result(-PB_RESULT_IO_ERROR);
    assert_int_equal(sec.slc, SLC_CONFIGURATION);

    /* An SLC that can't be read counts as locked */
    t.init_calls = 0;
    sec.slc = -PB_ERR_IO;
    host_auth_token();
    host_cmd(PB_CMD_SLC_READ, NULL, 0);
    run();
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_IO_ERROR);
    expect_end();
}

static void test_slc_read_key_status(void **state)
{
    (void)state;

    /* A revoked last key is not an error (F-072) */
    sec.revoked = 1u << 1;
    host_cmd(PB_CMD_SLC_READ, NULL, 0);
    run();
    expect_result(PB_RESULT_OK);
    const struct pb_result_slc_key_status *keys = (const void *)expect_data(sizeof(*keys));
    assert_int_equal(keys->active[0], 0x1000);
    assert_int_equal(keys->revoked[1], 0x1001);
    expect_result(PB_RESULT_OK);
    expect_end();

    /* A key that can't be read is an error, but the others are still
     * reported */
    t.init_calls = 0;
    t.out_len = t.out_pos = 0;
    sec.key_io_error = 1u << 0;
    sec.revoked = 1u << 1;
    host_cmd(PB_CMD_SLC_READ, NULL, 0);
    run();
    expect_result(PB_RESULT_OK);
    keys = (const void *)expect_data(sizeof(*keys));
    assert_int_equal(keys->active[0], 0);
    assert_int_equal(keys->revoked[0], 0);
    assert_int_equal(keys->revoked[1], 0x1001);
    expect_result(-PB_RESULT_IO_ERROR);
    expect_end();
}

/* command-mode.md 5.2 (F-190): SLC_READ lists 16 keys at most, the slots
 * the wire has. A keystore with more keys fills them and nothing after
 * them: the 17th id was written where the revoked list starts, and the
 * revoked ones past the end of the lists. */
static void test_slc_read_lists_16_keys_at_most(void **state)
{
    (void)state;

    sec.no_of_keys = 20;
    sec.revoked = (1u << 1) | (1u << 17);
    host_cmd(PB_CMD_SLC_READ, NULL, 0);
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();

    expect_result(PB_RESULT_OK);
    const struct pb_result_slc_key_status *keys = (const void *)expect_data(sizeof(*keys));
    for (unsigned int i = 0; i < 16; i++) {
        assert_int_equal(keys->active[i], (i == 1) ? 0 : 0x1000 + i);
        assert_int_equal(keys->revoked[i], (i == 1) ? 0x1001 : 0);
    }
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_end();
}

static void test_bpak_read(void **state)
{
    struct fixture *f = *state;
    struct pb_command_read_bpak req = { 0 };
    struct bpak_header *hdr = (struct bpak_header *)ramdisk_block(f->disk, 10 + RW_BLOCKS - 8);

    bpak_init_header(hdr);
    hdr->key_id = 0x1234;
    memcpy(req.uuid, UU_RW, 16);
    host_cmd(PB_CMD_PART_BPAK_READ, &req, sizeof(req));
    run();

    expect_result(PB_RESULT_OK);
    const struct bpak_header *read_hdr = (const void *)expect_data(sizeof(*read_hdr));
    assert_int_equal(read_hdr->magic, BPAK_HEADER_MAGIC);
    assert_int_equal(read_hdr->key_id, 0x1234);
    expect_result(PB_RESULT_OK);
    expect_end();
}

static void test_bpak_read_errors(void **state)
{
    struct fixture *f = *state;
    struct pb_command_read_bpak req = { 0 };
    (void)state;

    memcpy(req.uuid, UU_MISSING, 16);
    host_cmd(PB_CMD_PART_BPAK_READ, &req, sizeof(req));
    /* No bpak header */
    memcpy(req.uuid, UU_RW, 16);
    host_cmd(PB_CMD_PART_BPAK_READ, &req, sizeof(req));
    run();
    expect_result(-PB_RESULT_NOT_FOUND);
    expect_result(-PB_RESULT_NOT_FOUND);
    expect_end();

    t.init_calls = 0;
    f->disk->fail_reads_after = 0;
    host_cmd(PB_CMD_PART_BPAK_READ, &req, sizeof(req));
    run();
    expect_result(-PB_RESULT_IO_ERROR);
}

/* A partition below 4 KiB has no room for a bpak header (command-mode.md
 * 5.3, F-279): blocks 50 to 56, 3.5 KiB */
#define UU_SMALL \
    (const unsigned char *)"\x6e\x2b\x1f\x0c\x47\x95\x4a\x3e\x9d\x81\x5c\x22\xe7\x40\xb3\x18"

static void add_small_partition(void)
{
    bio_dev_t disk = bio_get_part_by_uu(UU_DISK);
    bio_dev_t small = bio_allocate_parent(disk, 50, 56, RAMDISK_BLOCK_SIZE, UU_SMALL, "Small");

    assert_true(small >= 0);
    assert_int_equal(bio_set_flags(small, BIO_FLAG_VISIBLE | BIO_FLAG_READABLE | BIO_FLAG_WRITABLE),
                     PB_OK);
}

static void test_bpak_read_small_partition(void **state)
{
    struct pb_command_read_bpak req = { 0 };
    (void)state;

    add_small_partition();
    memcpy(req.uuid, UU_SMALL, 16);
    host_cmd(PB_CMD_PART_BPAK_READ, &req, sizeof(req));
    run();

    expect_result(-PB_RESULT_NOT_FOUND);
    expect_end();
}

static struct pb_command_verify_part verify_req(size_t size, bool bpak, const uint8_t *data)
{
    struct pb_command_verify_part req = { .size = size, .bpak = bpak };

    memcpy(req.uuid, UU_RW, 16);
    uint32_t sum = 0;
    for (size_t i = 0; i < size; i++)
        sum = sum * 31 + data[i];
    sum_digest(req.sha256, sizeof(req.sha256), sum);
    return req;
}

static void test_part_verify(void **state)
{
    struct fixture *f = *state;
    uint8_t *part = ramdisk_block(f->disk, RW_FIRST_LBA);
    const size_t part_size = RW_BLOCKS * RAMDISK_BLOCK_SIZE;
    (void)state;

    for (size_t i = 0; i < part_size; i++)
        part[i] = (uint8_t)(i * 13);

    /* More than one buffer */
    struct pb_command_verify_part req = verify_req(BUF_SIZE + 1024, false, part);
    host_cmd(PB_CMD_PART_VERIFY, &req, sizeof(req));
    req.sha256[0] ^= 1;
    host_cmd(PB_CMD_PART_VERIFY, &req, sizeof(req));
    run();

    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_PART_VERIFY_FAILED);
    expect_end();
}

static void test_part_verify_bpak(void **state)
{
    struct fixture *f = *state;
    uint8_t *part = ramdisk_block(f->disk, RW_FIRST_LBA);
    uint8_t image[BUF_SIZE + 1024];
    (void)state;

    /* A bpak image is the header (from the end of the partition) followed by
     * the payload (from the start) */
    for (size_t i = 0; i < 1024; i++)
        part[i] = (uint8_t)(i * 3);
    struct bpak_header *hdr = (struct bpak_header *)ramdisk_block(f->disk, RW_FIRST_LBA + 12);
    memset(hdr, 0x77, sizeof(*hdr));
    memcpy(image, hdr, sizeof(*hdr));
    memcpy(image + sizeof(*hdr), part, 1024);

    struct pb_command_verify_part req = verify_req(sizeof(image), true, image);
    host_cmd(PB_CMD_PART_VERIFY, &req, sizeof(req));
    run();

    expect_result(PB_RESULT_OK);
    expect_end();
}

/* command-mode.md 4.6 (F-189): part verify kicks the watchdog once per
 * chunk, after the chunk is read and its hash started, the bpak header
 * included: one kick between two updates of the hash */
static void test_part_verify_kicks_per_chunk(void **state)
{
    struct fixture *f = *state;
    uint8_t *part = ramdisk_block(f->disk, RW_FIRST_LBA);
    const size_t part_size = RW_BLOCKS * RAMDISK_BLOCK_SIZE;
    uint8_t image[2 * BUF_SIZE + 1024];
    (void)state;

    for (size_t i = 0; i < part_size; i++)
        part[i] = (uint8_t)(i * 7);

    /* Three chunks, the last a short one */
    struct pb_command_verify_part req = verify_req(2 * BUF_SIZE + 1024, false, part);
    host_cmd(PB_CMD_PART_VERIFY, &req, sizeof(req));
    run();

    expect_result(PB_RESULT_OK);
    expect_end();
    assert_int_equal(crypto.updates, 3);
    assert_int_equal(crypto.update_kicks[1] - crypto.update_kicks[0], 1);
    assert_int_equal(crypto.update_kicks[2] - crypto.update_kicks[1], 1);

    /* A bpak image: the header, then two chunks of payload */
    t.init_calls = 0;
    t.out_len = t.out_pos = 0;
    t.in_len = t.in_pos = 0;
    crypto.updates = 0;
    crypto.sum = 0;
    memcpy(image, ramdisk_block(f->disk, RW_FIRST_LBA + RW_BLOCKS - 8), sizeof(struct bpak_header));
    memcpy(image + sizeof(struct bpak_header), part, sizeof(image) - sizeof(struct bpak_header));
    req = verify_req(sizeof(image), true, image);
    host_cmd(PB_CMD_PART_VERIFY, &req, sizeof(req));
    run();

    expect_result(PB_RESULT_OK);
    expect_end();
    assert_int_equal(crypto.updates, 3);
    assert_int_equal(crypto.update_kicks[1] - crypto.update_kicks[0], 1);
    assert_int_equal(crypto.update_kicks[2] - crypto.update_kicks[1], 1);
}

static void test_part_verify_errors(void **state)
{
    struct fixture *f = *state;
    uint8_t *part = ramdisk_block(f->disk, RW_FIRST_LBA);
    struct pb_command_verify_part req = verify_req(BUF_SIZE + 1024, true, part);
    struct pb_command_verify_part missing = req;
    (void)state;

    memcpy(missing.uuid, UU_MISSING, 16);
    host_cmd(PB_CMD_PART_VERIFY, &missing, sizeof(missing));
    run();
    expect_result(-PB_RESULT_NOT_FOUND);

    static const struct {
        int init_rc;
        int update_rc;
        int update_rc_after;
        int final_rc;
        int fail_reads_after;
        int result;
    } cases[] = {
        { -PB_ERR_NOT_SUPPORTED, 0, 0, 0, -1, -PB_RESULT_NOT_SUPPORTED },
        /* The bpak header can't be read or hashed */
        { 0, 0, 0, 0, 0, -PB_RESULT_IO_ERROR },
        { 0, -PB_ERR_IO, 0, 0, -1, -PB_RESULT_PART_VERIFY_FAILED },
        /* The payload can't be read or hashed */
        { 0, 0, 0, 0, 1, -PB_RESULT_PART_VERIFY_FAILED },
        { 0, -PB_ERR_IO, 1, 0, -1, -PB_RESULT_IO_ERROR },
        { 0, 0, 0, -PB_ERR_IO, -1, -PB_RESULT_IO_ERROR },
    };

    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        t.init_calls = 0;
        crypto.init_rc = cases[i].init_rc;
        crypto.update_rc = cases[i].update_rc;
        crypto.update_rc_after = cases[i].update_rc_after;
        crypto.final_rc = cases[i].final_rc;
        f->disk->fail_reads_after = cases[i].fail_reads_after;
        host_cmd(PB_CMD_PART_VERIFY, &req, sizeof(req));
        run();
        expect_result(cases[i].result);
    }
    expect_end();
}

/* With bpak the size is at least 4 KiB inside the partition (command-mode.md
 * 5.3, F-279): a partition below 4 KiB takes none, and that is checked before
 * the hash starts. Without bpak its bytes are hashed */
static void test_part_verify_small_partition(void **state)
{
    struct fixture *f = *state;
    uint8_t *part = ramdisk_block(f->disk, 50);
    (void)state;

    add_small_partition();
    struct pb_command_verify_part req = verify_req(1024, false, part);
    memcpy(req.uuid, UU_SMALL, 16);
    req.bpak = 1;
    host_cmd(PB_CMD_PART_VERIFY, &req, sizeof(req));
    run();

    expect_result(-PB_RESULT_INVALID_ARGUMENT);
    expect_end();
    assert_int_equal(crypto.alg, 0);

    t.init_calls = 0;
    req.bpak = 0;
    host_cmd(PB_CMD_PART_VERIFY, &req, sizeof(req));
    run();
    expect_result(PB_RESULT_OK);
    expect_end();
}

/* With bpak the size is at least 4 KiB (command-mode.md 5.3, F-190): the
 * header is part of it. A smaller size is refused before anything is read or
 * hashed; it used to wrap, and the partition was read to its end. A header
 * alone is a size. */
static void test_part_verify_bpak_size_below_a_header(void **state)
{
    static const uint32_t bad_sizes[] = { 0, 1, 1024, sizeof(struct bpak_header) - 1 };
    struct fixture *f = *state;
    uint8_t *header = ramdisk_block(f->disk, RW_FIRST_LBA + RW_BLOCKS - 8);
    struct pb_command_verify_part req;

    memset(header, 0x3c, sizeof(struct bpak_header));

    for (size_t i = 0; i < ARRAY_SIZE(bad_sizes); i++) {
        req = verify_req(bad_sizes[i], true, header);
        host_cmd(PB_CMD_PART_VERIFY, &req, sizeof(req));
    }
    run();

    for (size_t i = 0; i < ARRAY_SIZE(bad_sizes); i++)
        expect_result(-PB_RESULT_INVALID_ARGUMENT);
    expect_end();
    assert_int_equal(crypto.alg, 0);
    assert_int_equal(crypto.updates, 0);

    t.init_calls = 0;
    req = verify_req(sizeof(struct bpak_header), true, header);
    host_cmd(PB_CMD_PART_VERIFY, &req, sizeof(req));
    run();
    expect_result(PB_RESULT_OK);
    expect_end();
    assert_int_equal(crypto.updates, 1);
}

/* Hosts send the partition's first block from the partition table (10 for
 * UU_RW) plus the blocks already erased (F-055) */
static void test_part_erase(void **state)
{
    struct pb_command_erase_part req = { .start_lba = 10, .block_count = 1 };
    (void)state;

    memcpy(req.uuid, UU_MISSING, 16);
    host_cmd(PB_CMD_PART_ERASE, &req, sizeof(req));
    /* The ramdisk can't erase */
    memcpy(req.uuid, UU_RW, 16);
    host_cmd(PB_CMD_PART_ERASE, &req, sizeof(req));
    /* Before the partition */
    req.start_lba = 9;
    host_cmd(PB_CMD_PART_ERASE, &req, sizeof(req));
    /* No blocks, what hosts older than 2.0 send (F-075): erases nothing, and
     * the ramdisk still can't erase */
    req.start_lba = 0;
    req.block_count = 0;
    host_cmd(PB_CMD_PART_ERASE, &req, sizeof(req));
    run();

    expect_result(-PB_RESULT_NOT_FOUND);
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    expect_result(-PB_RESULT_INVALID_ARGUMENT);
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    expect_end();
}

/* Erase needs WRITABLE (command-mode.md 5.3, F-188), the answer a stream
 * write gets. The ramdisk can't erase, so NOT_SUPPORTED says the flags let
 * the erase through */
static void test_part_erase_needs_writable(void **state)
{
    struct pb_command_erase_part req = { .start_lba = 30, .block_count = 1 };
    bio_dev_t disk = bio_get_part_by_uu(UU_DISK);
    (void)state;

    /* The device a partition table is on has only VISIBLE (F-265) */
    assert_true(disk >= 0);
    assert_int_equal(bio_set_flags(disk, BIO_FLAG_VISIBLE), PB_OK);

    memcpy(req.uuid, UU_RO, 16);
    host_cmd(PB_CMD_PART_ERASE, &req, sizeof(req));
    /* Refused before the range is checked */
    req.start_lba = 0;
    host_cmd(PB_CMD_PART_ERASE, &req, sizeof(req));
    memcpy(req.uuid, UU_DISK, 16);
    host_cmd(PB_CMD_PART_ERASE, &req, sizeof(req));
    /* A count of 0 erases nothing and answers as it did (F-075), for any
     * partition */
    req.block_count = 0;
    host_cmd(PB_CMD_PART_ERASE, &req, sizeof(req));
    memcpy(req.uuid, UU_RW, 16);
    req.start_lba = 10;
    req.block_count = 1;
    host_cmd(PB_CMD_PART_ERASE, &req, sizeof(req));
    run();

    expect_result(-PB_RESULT_IO_ERROR);
    expect_result(-PB_RESULT_IO_ERROR);
    expect_result(-PB_RESULT_IO_ERROR);
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    expect_end();
}

static void stream_init(const unsigned char *uu)
{
    struct pb_command_stream_initialize req = { 0 };

    memcpy(req.part_uuid, uu, 16);
    host_cmd(PB_CMD_STREAM_INITIALIZE, &req, sizeof(req));
}

static void stream_prepare(uint8_t id, uint32_t size, const void *data)
{
    struct pb_command_stream_prepare_buffer req = { .size = size, .id = id };

    host_cmd(PB_CMD_STREAM_PREPARE_BUFFER, &req, sizeof(req));
    if (data)
        host_send(data, size);
}

static void stream_write(uint8_t id, uint32_t size, uint64_t offset)
{
    struct pb_command_stream_write_buffer req = { .size = size, .offset = offset, .buffer_id = id };
    host_cmd(PB_CMD_STREAM_WRITE_BUFFER, &req, sizeof(req));
}

static void stream_read(uint8_t id, uint32_t size, uint64_t offset)
{
    struct pb_command_stream_read_buffer req = { .size = size, .offset = offset, .buffer_id = id };
    host_cmd(PB_CMD_STREAM_READ_BUFFER, &req, sizeof(req));
}

/* command-mode.md 4.3 (F-190): every byte the command didn't produce is
 * zero. An entry's reserved bytes and what follows its description are not
 * what a host left in the buffers. */
static void test_part_table_sends_only_the_table(void **state)
{
    static const uint8_t zeros[sizeof(struct pb_result_part_table_entry)];
    static uint8_t filled[BUF_SIZE];
    (void)state;

    memset(filled, 0xa5, sizeof(filled));
    stream_prepare(0, sizeof(filled), filled);
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    const struct pb_result_part_table_entry *e = (const void *)expect_data(2 * sizeof(*e));
    expect_result(PB_RESULT_OK);
    expect_end();

    for (int i = 0; i < 2; i++) {
        size_t len = strlen(i == 0 ? "Read write" : "Visible");

        assert_memory_equal(e[i].uuid, i == 0 ? UU_RW : UU_RO, 16);
        assert_memory_equal(e[i].description, i == 0 ? "Read write" : "Visible", len);
        assert_memory_equal(e[i].description + len, zeros, sizeof(e[i].description) - len);
        assert_memory_equal(e[i].rz, zeros, sizeof(e[i].rz));
    }
}

/* A hidden partition is internal to pb (command-mode.md 5.3, D-058, F-272):
 * every command that names it answers NOT_FOUND, whatever its other flags
 * allow. BOOT_BPAK's UUID is a label and reaches no partition. */
static void test_hidden_partition(void **state)
{
    struct fixture *f = *state;
    bio_dev_t hidden = bio_get_part_by_uu(UU_HIDDEN);
    struct pb_command_erase_part erase = { .start_lba = 40, .block_count = 1 };
    struct pb_command_read_bpak bpak_read = { 0 };
    struct pb_command_boot_part boot = { 0 };
    struct pb_command_activate_part activate = { 0 };
    struct pb_command_install_part_table install = { .variant = 0 };
    struct pb_command_bpak_boot bpak_boot = { 0 };
    struct pb_command_verify_part verify = verify_req(1024, false, ramdisk_block(f->disk, 40));

    assert_true(hidden >= 0);
    assert_int_equal(
        bio_set_flags(hidden, BIO_FLAG_READABLE | BIO_FLAG_WRITABLE | BIO_FLAG_BOOTABLE), PB_OK);
    bpak_init_header((struct bpak_header *)ramdisk_block(f->disk, 50 - 8));

    stream_init(UU_HIDDEN);
    memcpy(erase.uuid, UU_HIDDEN, 16);
    host_cmd(PB_CMD_PART_ERASE, &erase, sizeof(erase));
    memcpy(verify.uuid, UU_HIDDEN, 16);
    host_cmd(PB_CMD_PART_VERIFY, &verify, sizeof(verify));
    memcpy(bpak_read.uuid, UU_HIDDEN, 16);
    host_cmd(PB_CMD_PART_BPAK_READ, &bpak_read, sizeof(bpak_read));
    memcpy(boot.uuid, UU_HIDDEN, 16);
    host_cmd(PB_CMD_BOOT_PART, &boot, sizeof(boot));
    memcpy(activate.uuid, UU_HIDDEN, 16);
    host_cmd(PB_CMD_PART_ACTIVATE, &activate, sizeof(activate));
    memcpy(install.uu, UU_HIDDEN, 16);
    host_cmd(PB_CMD_PART_TBL_INSTALL, &install, sizeof(install));
    memcpy(bpak_boot.uuid, UU_HIDDEN, 16);
    host_cmd(PB_CMD_BOOT_BPAK, &bpak_boot, sizeof(bpak_boot));
    run();

    for (int i = 0; i < 7; i++)
        expect_result(-PB_RESULT_NOT_FOUND);
    expect_result(PB_RESULT_OK);
    expect_end();
    /* Neither the boot nor the policy heard of it, but BOOT_BPAK's label */
    assert_int_equal(fake_boot.load_calls, 1);
    assert_memory_equal(fake_boot.load_uu, UU_HIDDEN, 16);
    assert_memory_equal(fake_boot.set_part_uu, (uint8_t[16]){ 0 }, 16);
}

static void test_stream(void **state)
{
    struct fixture *f = *state;
    uint8_t data[1024];
    (void)state;

    for (size_t i = 0; i < sizeof(data); i++)
        data[i] = (uint8_t)(i * 5);

    stream_init(UU_RW);
    stream_prepare(1, sizeof(data), data);
    stream_write(1, sizeof(data), 2 * RAMDISK_BLOCK_SIZE);
    stream_read(0, sizeof(data), 2 * RAMDISK_BLOCK_SIZE);
    host_cmd(PB_CMD_STREAM_FINALIZE, NULL, 0);
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    assert_memory_equal(expect_data(sizeof(data)), data, sizeof(data));
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_end();
    assert_memory_equal(ramdisk_block(f->disk, RW_FIRST_LBA + 2), data, sizeof(data));
}

/* command-mode.md 5.4 (F-182): hosts send the size as it is. A write whose
 * size isn't a multiple of the block writes its last block padded with
 * zeros, and a read sends 'size' bytes. The RAM disk takes whole blocks
 * only, as a driver does */
static void test_stream_size_that_isnt_whole_blocks(void **state)
{
    struct fixture *f = *state;
    uint8_t data[RAMDISK_BLOCK_SIZE + 200];
    uint8_t *medium = ramdisk_block(f->disk, RW_FIRST_LBA + 2);

    for (size_t i = 0; i < sizeof(data); i++)
        data[i] = (uint8_t)(i * 5 + 1);
    memset(medium, 0xff, 3 * RAMDISK_BLOCK_SIZE);

    stream_init(UU_RW);
    stream_prepare(1, sizeof(data), data);
    stream_write(1, sizeof(data), 2 * RAMDISK_BLOCK_SIZE);
    stream_read(0, sizeof(data), 2 * RAMDISK_BLOCK_SIZE);
    stream_read(0, 100, 3 * RAMDISK_BLOCK_SIZE);
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    assert_memory_equal(expect_data(sizeof(data)), data, sizeof(data));
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    assert_memory_equal(expect_data(100), data + RAMDISK_BLOCK_SIZE, 100);
    expect_result(PB_RESULT_OK);
    expect_end();

    assert_memory_equal(medium, data, sizeof(data));
    for (size_t i = sizeof(data); i < 2 * RAMDISK_BLOCK_SIZE; i++)
        assert_int_equal(medium[i], 0);
    for (size_t i = 2 * RAMDISK_BLOCK_SIZE; i < 3 * RAMDISK_BLOCK_SIZE; i++)
        assert_int_equal(medium[i], 0xff);
}

/* command-mode.md 5.3 (F-182): 'size' is any number of bytes inside the
 * partition, and exactly those bytes are hashed */
static void test_part_verify_size_that_isnt_whole_blocks(void **state)
{
    struct fixture *f = *state;
    uint8_t *part = ramdisk_block(f->disk, RW_FIRST_LBA);
    const size_t part_size = RW_BLOCKS * RAMDISK_BLOCK_SIZE;

    for (size_t i = 0; i < part_size; i++)
        part[i] = (uint8_t)(i * 13 + 5);

    /* Less than a block, and more than a buffer */
    struct pb_command_verify_part small = verify_req(300, false, part);
    struct pb_command_verify_part large = verify_req(BUF_SIZE + 1000, false, part);
    host_cmd(PB_CMD_PART_VERIFY, &small, sizeof(small));
    host_cmd(PB_CMD_PART_VERIFY, &large, sizeof(large));
    /* The last byte is hashed, the one after it isn't */
    part[BUF_SIZE + 1000] ^= 1;
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_end();
}

static void test_part_verify_size_that_isnt_whole_blocks_last_byte(void **state)
{
    struct fixture *f = *state;
    uint8_t *part = ramdisk_block(f->disk, RW_FIRST_LBA);
    struct pb_command_verify_part req = verify_req(BUF_SIZE + 1000, false, part);

    part[BUF_SIZE + 999] ^= 1;
    host_cmd(PB_CMD_PART_VERIFY, &req, sizeof(req));
    run();

    expect_result(-PB_RESULT_PART_VERIFY_FAILED);
    expect_end();
}

/* PART_VERIFY changes nothing, the stream neither (command-mode.md 5.3,
 * F-186): a write after a verify of another partition, and after one that
 * found none, goes to the stream's partition */
static void test_part_verify_keeps_the_stream(void **state)
{
    struct fixture *f = *state;
    uint8_t data[RAMDISK_BLOCK_SIZE];
    struct pb_command_verify_part verify = verify_req(1024, false, ramdisk_block(f->disk, 30));
    struct pb_command_verify_part missing = verify;
    (void)state;

    memset(data, 0x5a, sizeof(data));
    memcpy(verify.uuid, UU_RO, 16);
    memcpy(missing.uuid, UU_MISSING, 16);

    stream_init(UU_RW);
    stream_prepare(0, sizeof(data), data);
    host_cmd(PB_CMD_PART_VERIFY, &verify, sizeof(verify));
    stream_write(0, sizeof(data), 0);
    host_cmd(PB_CMD_PART_VERIFY, &missing, sizeof(missing));
    stream_write(0, sizeof(data), RAMDISK_BLOCK_SIZE);
    host_cmd(PB_CMD_STREAM_FINALIZE, NULL, 0);
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_NOT_FOUND);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_end();
    assert_memory_equal(ramdisk_block(f->disk, RW_FIRST_LBA), data, sizeof(data));
    assert_memory_equal(ramdisk_block(f->disk, RW_FIRST_LBA + 1), data, sizeof(data));
}

static void test_stream_errors(void **state)
{
    struct fixture *f = *state;
    static uint8_t frames[BUF_SIZE + 1];
    (void)state;

    /* Data that are frames, were they read as commands */
    for (size_t i = 0; i + sizeof(struct pb_command) <= sizeof(frames);
         i += sizeof(struct pb_command))
        pb_wire_init_command((struct pb_command *)&frames[i], PB_CMD_BOOTLOADER_VERSION_READ);

    /* The stream_check() bounds are covered by test_stream.py */
    stream_init(UU_MISSING);
    /* Without a partition the stream is not initialized */
    stream_write(0, RAMDISK_BLOCK_SIZE, 0);
    /* A size or an id the buffers don't have: hosts send the data anyway,
     * the device reads it and answers twice (command-mode.md 4.4, F-274) */
    stream_prepare(0, BUF_SIZE + 1, frames);
    stream_prepare(2, 16, frames);
    stream_init(UU_RO);
    stream_write(0, RAMDISK_BLOCK_SIZE, 0);
    stream_read(0, RAMDISK_BLOCK_SIZE, 0);
    run();

    expect_result(-PB_RESULT_NOT_FOUND);
    expect_result(-PB_RESULT_STREAM_NOT_INITIALIZED);
    expect_result(-PB_RESULT_NO_MEMORY);
    expect_result(-PB_RESULT_NO_MEMORY);
    expect_result(-PB_RESULT_NO_MEMORY);
    expect_result(-PB_RESULT_NO_MEMORY);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_IO_ERROR);
    expect_result(-PB_RESULT_IO_ERROR);
    expect_end();

    t.init_calls = 0;
    f->disk->fail_reads_after = 0;
    stream_init(UU_RW);
    stream_read(0, RAMDISK_BLOCK_SIZE, 0);
    run();
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_IO_ERROR);
    expect_end();
}

/* An offset inside a block is refused, not rounded down to the block's start
 * (command-mode.md 5.4, F-273) */
static void test_stream_offset_inside_a_block(void **state)
{
    struct fixture *f = *state;
    uint8_t data[RAMDISK_BLOCK_SIZE];
    uint8_t before[2 * RAMDISK_BLOCK_SIZE];

    memset(data, 0xa5, sizeof(data));
    memcpy(before, ramdisk_block(f->disk, RW_FIRST_LBA + 2), sizeof(before));

    stream_init(UU_RW);
    stream_prepare(0, sizeof(data), data);
    stream_write(0, sizeof(data), 2 * RAMDISK_BLOCK_SIZE + 1);
    stream_write(0, sizeof(data), 3 * RAMDISK_BLOCK_SIZE - 1);
    stream_read(1, sizeof(data), RAMDISK_BLOCK_SIZE / 2);
    /* The stream is still there */
    stream_read(1, sizeof(data), 0);
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_INVALID_ARGUMENT);
    expect_result(-PB_RESULT_INVALID_ARGUMENT);
    expect_result(-PB_RESULT_INVALID_ARGUMENT);
    expect_result(PB_RESULT_OK);
    expect_data(sizeof(data));
    expect_result(PB_RESULT_OK);
    expect_end();
    assert_memory_equal(ramdisk_block(f->disk, RW_FIRST_LBA + 2), before, sizeof(before));
}

static unsigned int write_done_calls;
static bio_dev_t write_done_dev;
static int write_done_rc;

static int fake_write_done(bio_dev_t dev, int param)
{
    (void)param;
    write_done_calls++;
    write_done_dev = dev;
    return write_done_rc;
}

/* Writes one block of 'buffer_id' in a new stream, without finalizing it */
static void stream_write_block(uint8_t buffer_id)
{
    static const uint8_t data[RAMDISK_BLOCK_SIZE] = { 0xaa };

    stream_init(UU_RW);
    stream_prepare(buffer_id, sizeof(data), data);
    stream_write(buffer_id, sizeof(data), 0);
}

/* Only a finalized stream where every write succeeded is a complete write (F-082) */
static void test_stream_write_done(void **state)
{
    struct fixture *f = *state;
    static const uint8_t too_large[BUF_SIZE + 1];

    write_done_calls = 0;
    write_done_dev = -1;
    write_done_rc = PB_OK;
    assert_int_equal(bio_set_write_done_cb(f->rw, fake_write_done), PB_OK);

    /* A complete write, and a second finalize without a new stream */
    stream_write_block(0);
    host_cmd(PB_CMD_STREAM_FINALIZE, NULL, 0);
    host_cmd(PB_CMD_STREAM_FINALIZE, NULL, 0);
    /* A read only stream */
    stream_init(UU_RW);
    stream_read(0, RAMDISK_BLOCK_SIZE, 0);
    host_cmd(PB_CMD_STREAM_FINALIZE, NULL, 0);
    /* A stream with a failed buffer, then a good write */
    stream_init(UU_RW);
    stream_prepare(0, BUF_SIZE + 1, too_large);
    stream_prepare(1, RAMDISK_BLOCK_SIZE, ramdisk_block(f->disk, 0));
    stream_write(1, RAMDISK_BLOCK_SIZE, 0);
    host_cmd(PB_CMD_STREAM_FINALIZE, NULL, 0);
    run();

    expect_result(PB_RESULT_OK); /* init */
    expect_result(PB_RESULT_OK); /* prepare */
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK); /* write */
    expect_result(PB_RESULT_OK); /* finalize */
    expect_result(PB_RESULT_OK); /* finalize */
    expect_result(PB_RESULT_OK); /* init */
    expect_result(PB_RESULT_OK); /* read */
    expect_data(RAMDISK_BLOCK_SIZE);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK); /* finalize */
    expect_result(PB_RESULT_OK); /* init */
    expect_result(-PB_RESULT_NO_MEMORY); /* prepare, its data read anyway */
    expect_result(-PB_RESULT_NO_MEMORY);
    expect_result(PB_RESULT_OK); /* prepare */
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK); /* write */
    expect_result(PB_RESULT_OK); /* finalize */
    expect_end();
    assert_int_equal(write_done_calls, 1);
    assert_int_equal(write_done_dev, f->rw);

    /* A failed write, then a good one in the same stream: the stream is the
     * session's (command-mode.md 3.1), so both are in one. The disk works
     * again from the fourth command on. */
    t.init_calls = 0;
    t.out_len = t.out_pos = 0;
    write_done_calls = 0;
    board_cfg = &cfg_all_callbacks;
    t.connect_ok_after = 0;
    f->disk->fail_writes_after = 0;
    t.heal = f->disk;
    /* The first round sets up and starts the read of the first command, so
     * the fourth command is handled in the fifth round */
    t.heal_at = t.rounds + 5;
    stream_write_block(0);
    stream_prepare(1, RAMDISK_BLOCK_SIZE, ramdisk_block(f->disk, 0));
    stream_write(1, RAMDISK_BLOCK_SIZE, 0);
    host_cmd(PB_CMD_STREAM_FINALIZE, NULL, 0);
    /* The driver's error is the finalize result */
    write_done_rc = -PB_ERR_IO;
    stream_write_block(0);
    host_cmd(PB_CMD_STREAM_FINALIZE, NULL, 0);
    run();
    expect_result(PB_RESULT_OK); /* init */
    expect_result(PB_RESULT_OK); /* prepare */
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_IO_ERROR); /* write */
    expect_result(PB_RESULT_OK); /* prepare */
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK); /* write */
    expect_result(PB_RESULT_OK); /* finalize */
    expect_result(PB_RESULT_OK); /* init */
    expect_result(PB_RESULT_OK); /* prepare */
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK); /* write */
    expect_result(-PB_RESULT_IO_ERROR); /* finalize */
    expect_end();
    assert_int_equal(write_done_calls, 1);
}

static void test_boot_part(void **state)
{
    struct pb_command_boot_part req = { .verbose = 1 };
    (void)state;

    memcpy(req.uuid, UU_RW, 16);
    host_cmd(PB_CMD_BOOT_PART, &req, sizeof(req));
    fake_boot.load_rc = -PB_ERR_SIGNATURE;
    run();
    expect_result(-PB_RESULT_SIGNATURE_ERROR);
    assert_int_equal(fake_boot.source, BOOT_SOURCE_BIO);
    assert_int_equal(fake_boot.flags, BOOT_FLAG_CMD | BOOT_FLAG_VERBOSE);
    assert_memory_equal(fake_boot.load_uu, UU_RW, 16);
    assert_int_equal(fake_boot.jump_calls, 0);

    /* A failed boot keeps the transport up for the next command */
    t.init_calls = 0;
    board_cfg = &cfg_all_callbacks;
    t.connect_ok_after = 0;
    host_cmd(PB_CMD_BOOT_PART, &req, sizeof(req));
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();
    expect_result(-PB_RESULT_SIGNATURE_ERROR);
    expect_result(PB_RESULT_OK);
    expect_end();
    assert_int_equal(t.disconnect_calls, 0);

    /* The fake jump returns, which is a command error */
    t.init_calls = 0;
    fake_boot.load_rc = PB_OK;
    host_cmd(PB_CMD_BOOT_PART, &req, sizeof(req));
    run();
    expect_result(PB_RESULT_OK);
    assert_int_equal(fake_boot.jump_calls, 1);
    assert_true(t.disconnect_calls >= 1);
}

static void test_boot_bpak(void **state)
{
    struct pb_command_bpak_boot req = { .verbose = 0 };
    static struct bpak_header header;
    (void)state;

    memcpy(req.uuid, UU_RW, 16);
    fake_boot.load_rc = -PB_ERR_SIGNATURE;
    fake_boot.reads_header = true;
    host_cmd(PB_CMD_BOOT_BPAK, &req, sizeof(req));
    host_send(&header, sizeof(header));
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();
    /* The image transfer and its results are handled by the boot flow, and
     * the header isn't read again */
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_SIGNATURE_ERROR);
    expect_result(PB_RESULT_OK);
    expect_end();
    assert_int_equal(fake_boot.source, BOOT_SOURCE_CB);
    assert_int_equal(fake_boot.flags, BOOT_FLAG_CMD);

    t.init_calls = 0;
    fake_boot.load_rc = PB_OK;
    fake_boot.reads_header = false;
    board_cfg = &cfg_all_callbacks;
    t.connect_ok_after = 0;
    host_cmd(PB_CMD_BOOT_BPAK, &req, sizeof(req));
    run();
    assert_int_equal(fake_boot.jump_calls, 1);
    assert_true(t.disconnect_calls >= 1);

    /* The first result can't be sent */
    t.init_calls = 0;
    t.fail_writes_after = 0;
    fake_boot.load_calls = 0;
    host_cmd(PB_CMD_BOOT_BPAK, &req, sizeof(req));
    run();
    assert_int_equal(fake_boot.load_calls, 0);
}

/* A boot refused before the header is read (command-mode.md 5.5, D-058,
 * F-128): the host sends the header after the first OK, the device reads it
 * and the header's result carries the refusal. The header is a frame, were it
 * read as one. */
static void test_boot_bpak_refused_before_the_header(void **state)
{
    struct pb_command_bpak_boot req = { .verbose = 0 };
    static uint8_t header[sizeof(struct bpak_header)];
    (void)state;

    pb_wire_init_command((struct pb_command *)header, PB_CMD_BOOTLOADER_VERSION_READ);
    fake_boot.load_rc = -PB_ERR_NOT_SUPPORTED;
    host_cmd(PB_CMD_BOOT_BPAK, &req, sizeof(req));
    host_send(header, sizeof(header));
    host_cmd(PB_CMD_DEVICE_IDENTIFIER_READ, NULL, 0);
    run();

    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_NOT_SUPPORTED);
    expect_result(PB_RESULT_OK);
    expect_end();
    assert_int_equal(fake_boot.jump_calls, 0);
}

static void test_run_errors(void **state)
{
    (void)state;

    board_init_rc = -PB_ERR_IO;
    assert_int_equal(cm_run(), -PB_ERR_IO);
    assert_int_equal(t.init_calls, 0);
    board_init_rc = PB_OK;

    t.max_inits = 0;
    assert_int_equal(cm_run(), -PB_ERR_IO);
    assert_int_equal(t.init_calls, 1);

    /* The transport can't read */
    t.max_inits = 1;
    t.init_calls = 0;
    t.fail_reads_after = 0;
    board_cfg = &cfg_all_callbacks;
    t.connect_ok_after = 0;
    assert_int_equal(cm_run(), -PB_ERR_IO);
    assert_int_equal(t.disconnect_calls, 1);
}

/* The set up starts with the platform's (command-mode.md 2.1, platform.md
 * 3.10): plat_cm_init(), before the board's data is checked and before the
 * transport is started, and once: not again when the transport is restarted,
 * neither after a host that was lost while pb waited for a command nor after
 * a transfer that was lost inside one */
static void test_setup_platform_first(void **state)
{
    struct pb_command_board req = { .command = BOARD_ECHO, .request_size = 5 };
    (void)state;

    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    /* Lost while waiting for a command */
    host_lost(-PB_ERR_IO);
    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    /* Lost inside a command: the board command's request */
    host_lost(-PB_ERR_IO);
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK); /* The board command's first, no second */
    expect_result(PB_RESULT_OK);
    expect_end();
    assert_int_equal(t.init_calls, 4);
    assert_int_equal(plat_cm.calls, 1);
    assert_int_equal(plat_cm.board_inits_before, 0);
    assert_int_equal(plat_cm.transport_inits_before, 0);
    assert_int_equal(board_init_calls, 1);
}

/* A platform that can't set up the port: no command mode (platform.md 3.10).
 * Nothing after it is set up, and the error is what pb resets on */
static void test_setup_platform_fails(void **state)
{
    (void)state;

    plat_cm.rc = -PB_ERR_TIMEOUT;
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    assert_int_equal(cm_run(), -PB_ERR_TIMEOUT);
    assert_int_equal(plat_cm.calls, 1);
    assert_int_equal(board_init_calls, 0);
    assert_int_equal(t.init_calls, 0);
    expect_end();
}

/*
 * board.md 4.6, command-mode.md 2: command mode is a task, and a call never
 * waits for the host. Its first call sets up and starts the transport;
 * without a session a call polls the transport once (2.2), with one it polls
 * the read of the next command once (usb.md 2.4), and each returns PB_OK to
 * be called again. It doesn't kick the watchdog while it waits: the core
 * does, once per round. A command that has come runs to its end in the call,
 * and the read of the next one is started.
 */
static void test_task_polls_once_per_call(void **state)
{
    unsigned int kicks = fake_wdog_kicks;
    (void)state;

    t.connect_rc = -PB_ERR_AGAIN;
    t.connect_ok_after = 2;
    t.complete_rcs[0] = -PB_ERR_AGAIN;
    t.complete_rcs[1] = -PB_ERR_AGAIN;
    t.n_complete_rcs = 2;
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);

    /* Its first call: the set up, the transport started, one poll */
    assert_int_equal(cm_main_task(), PB_OK);
    assert_int_equal(plat_cm.calls, 1);
    assert_int_equal(board_init_calls, 1);
    assert_int_equal(t.init_calls, 1);
    assert_int_equal(t.connect_calls, 1);

    assert_int_equal(cm_main_task(), PB_OK);
    assert_int_equal(t.connect_calls, 2);
    assert_false(t.read_pending);

    /* Ready: the session starts with the read of its first command */
    assert_int_equal(cm_main_task(), PB_OK);
    assert_int_equal(t.connect_calls, 3);
    assert_true(t.read_pending);
    assert_int_equal(t.complete_calls, 0);

    /* No command yet */
    assert_int_equal(cm_main_task(), PB_OK);
    assert_int_equal(t.complete_calls, 1);
    assert_int_equal(cm_main_task(), PB_OK);
    assert_int_equal(t.complete_calls, 2);
    assert_int_equal(t.out_len, 0);
    assert_int_equal(fake_wdog_kicks, kicks);
    /* The set up ran once, the transport was started once */
    assert_int_equal(plat_cm.calls, 1);
    assert_int_equal(t.init_calls, 1);

    /* The command has come */
    assert_int_equal(cm_main_task(), PB_OK);
    expect_result(PB_RESULT_OK);
    expect_end();
    assert_true(t.read_pending);

    /* The host goes, and the fake has no second one: the task ends the loop */
    run();
    expect_end();
}

static void test_run_connect(void **state)
{
    (void)state;

    /* Not ready twice, then connected */
    board_cfg = &cfg_all_callbacks;
    t.connect_rc = -PB_ERR_AGAIN;
    t.connect_ok_after = 2;
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();
    assert_int_equal(t.connect_calls, 3);
    expect_result(PB_RESULT_OK);
    /* The other task ran while the transport wasn't ready (usb.md 2.2) */
    assert_true(t.rounds >= 3);

    /* A connect error */
    t.init_calls = 0;
    t.connect_calls = 0;
    t.connect_rc = -PB_ERR_IO;
    t.connect_ok_after = -1;
    assert_int_equal(cm_run(), -PB_ERR_IO);
    assert_int_equal(t.connect_calls, 1);

    /* Never ready: times out on the fake clock */
    t.init_calls = 0;
    t.connect_calls = 0;
    t.connect_rc = -PB_ERR_AGAIN;
    fake_us_tick = 0;
    fake_us_tick_step = 1000000;
    assert_int_equal(cm_run(), -PB_ERR_TIMEOUT);
    assert_true(t.connect_calls >= CONFIG_CM_TRANSPORT_READY_TIMEOUT - 1);
}

static void test_run_retries(void **state)
{
    (void)state;

    /* The command read completes after a timeout and a retry: -PB_ERR_TIMEOUT
     * while pb waits for a command is polled on, like -PB_ERR_AGAIN, and is
     * not a lost transport (F-287). The fake starts once (max_inits is 1), so
     * a restart on the timeout would end command mode with the command
     * unanswered */
    t.complete_rcs[0] = -PB_ERR_TIMEOUT;
    t.complete_rcs[1] = -PB_ERR_AGAIN;
    t.n_complete_rcs = 2;
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();
    expect_result(PB_RESULT_OK);
    expect_end();
    /* The start, and the restart when the host was gone after its command */
    assert_int_equal(t.init_calls, 2);
}

static void test_transport_errors_in_commands(void **state)
{
    struct pb_command_board req = { .command = BOARD_ECHO, .request_size = 5 };
    (void)state;

    /* The request data never arrives */
    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    run();
    expect_result(PB_RESULT_OK);

    /* The data read can't be started: a failed transfer, so nothing more is
     * sent and the transport is restarted (command-mode.md 4.4) */
    t.init_calls = 0;
    t.out_len = t.out_pos = 0;
    t.read_pending = false;
    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    t.fail_reads_after = 1;
    run();
    expect_result(PB_RESULT_OK);
    expect_end();
    assert_int_equal(t.init_calls, 2);
}

/* A gated command that a locked device runs once the session is
 * authenticated: the result, two partitions and the last result */
static void expect_part_table(void)
{
    const struct pb_result *r = expect_result(PB_RESULT_OK);
    const struct pb_result_part_table_read *tbl = (const void *)r->response;

    assert_int_equal(tbl->no_of_entries, 2);
    expect_data(2 * sizeof(struct pb_result_part_table_entry));
    expect_result(PB_RESULT_OK);
}

/*
 * command-mode.md 3.1: the session ends with the transport. A host that
 * comes after an unplug or a bus reset (usb.md 2.4), or after a client
 * closed the socket (2.6), finds a locked device not authenticated. Every
 * error from complete() other than AGAIN is lost; TIMEOUT is still polled
 * (F-287).
 */
static void test_session_ends_with_the_transport(void **state)
{
    (void)state;

    sec.slc = SLC_CONFIGURATION_LOCKED;
    board_cfg = &cfg_all_callbacks;
    t.connect_ok_after = 0;

    host_auth_token();
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    host_lost(-PB_ERR_IO);
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    host_auth_token();
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    host_lost(-PB_ERR);
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_part_table();
    /* The next host */
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_part_table();
    /* And the one after it */
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_end();
    /* The transport was started for each host, and waited for each */
    assert_int_equal(t.init_calls, 4);
    assert_int_equal(t.connect_calls, 3);
}

/* command-mode.md 3.1, 4.4: a transfer inside a command that fails ends the
 * session too, a read or a write. Nothing more is sent on that transport,
 * also where the command ignores the failure, and the transport is
 * restarted: the next command is the next host's, which isn't
 * authenticated. */
static void test_session_ends_with_a_lost_transfer(void **state)
{
    struct pb_command_board req = { .command = BOARD_ECHO, .request_size = 5 };
    (void)state;

    sec.slc = SLC_CONFIGURATION_LOCKED;

    /* The board command's request is lost: no second result */
    host_auth_token();
    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    host_lost(-PB_ERR_IO);
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_end();
    assert_int_equal(t.init_calls, 3);

    /* A result is lost: the third write, the version's */
    t.init_calls = 0;
    t.out_len = t.out_pos = 0;
    t.write_calls = 0;
    t.lose_write = 3;
    host_auth_token();
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_end();
    assert_int_equal(t.init_calls, 3);

    /* The partition table's data is lost, a write whose result the command
     * doesn't look at: its last result isn't sent */
    t.init_calls = 0;
    t.out_len = t.out_pos = 0;
    t.write_calls = 0;
    t.lose_write = 4;
    host_auth_token();
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK); /* the table's first */
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_end();
    assert_int_equal(t.write_calls, 5);
    assert_int_equal(t.init_calls, 3);
}

/*
 * command-mode.md 4.6 (F-277): a host that stops in the middle of an
 * exchange, here an AUTHENTICATE without its data. pb waits
 * CONFIG_CM_TRANSFER_TIMEOUT for the transfer and kicks the watchdog while
 * it waits; then the session ends and the transport is restarted, so the
 * next host is answered and isn't authenticated.
 */
static void test_transfer_timeout_read(void **state)
{
    struct pb_command_authenticate auth = { .method = PB_AUTH_ASYM_TOKEN,
                                            .size = 2,
                                            .key_id = KEY_ID };
    (void)state;

    sec.slc = SLC_CONFIGURATION_LOCKED;

    host_auth_token();
    host_cmd(PB_CMD_AUTHENTICATE, &auth, sizeof(auth));
    host_stops();
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    fake_wdog_kick_tick = fake_us_tick;
    fake_wdog_max_gap_us = 0;
    run();

    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK); /* the second AUTHENTICATE's first */
    /* The next host */
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_end();
    assert_int_equal(t.init_calls, 3);

    /* Reads: the token's command and data, the second command, the next
     * host's command. Between the last two: the timeout, and little else */
    assert_int_equal(t.n_reads, 4);
    assert_true(t.read_ticks[3] - t.read_ticks[2] >= TRANSFER_TIMEOUT_US);
    assert_true(t.read_ticks[3] - t.read_ticks[2] <= TRANSFER_TIMEOUT_US + 100 * TICK_STEP_US);
    assert_true(fake_wdog_max_gap_us <= 10 * TICK_STEP_US);
}

/* command-mode.md 4.6 (F-277): a host that stops reading. The partition
 * table's data isn't taken: pb waits the timeout once, doesn't send the last
 * result, and restarts the transport. */
static void test_transfer_timeout_write(void **state)
{
    (void)state;

    t.stop_write = 2;
    t.max_inits = 2;
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    fake_wdog_kick_tick = fake_us_tick;
    fake_wdog_max_gap_us = 0;
    run();

    expect_result(PB_RESULT_OK); /* the table's first */
    /* The next host */
    expect_result(PB_RESULT_OK);
    expect_end();
    assert_int_equal(t.write_calls, 3);
    assert_int_equal(t.init_calls, 3);
    assert_int_equal(t.n_writes, 2);
    assert_true(t.write_ticks[1] - t.write_ticks[0] >= TRANSFER_TIMEOUT_US);
    assert_true(t.write_ticks[1] - t.write_ticks[0] <= TRANSFER_TIMEOUT_US + 100 * TICK_STEP_US);
    assert_true(fake_wdog_max_gap_us <= 10 * TICK_STEP_US);
}

/* command-mode.md 3.1, 5.4: the stream is the session's. After the
 * transport was lost there is no stream: a write and a read without a new
 * STREAM_INITIALIZE answer STREAM_NOT_INITIALIZED, and a finalize has
 * nothing written to complete. */
static void test_session_stream_ends_with_the_transport(void **state)
{
    struct fixture *f = *state;
    uint8_t before[RAMDISK_BLOCK_SIZE];

    write_done_calls = 0;
    write_done_rc = PB_OK;
    assert_int_equal(bio_set_write_done_cb(f->rw, fake_write_done), PB_OK);

    stream_write_block(0);
    host_lost(-PB_ERR_IO);
    stream_write(0, RAMDISK_BLOCK_SIZE, RAMDISK_BLOCK_SIZE);
    stream_read(0, RAMDISK_BLOCK_SIZE, 0);
    host_cmd(PB_CMD_STREAM_FINALIZE, NULL, 0);
    memcpy(before, ramdisk_block(f->disk, RW_FIRST_LBA + 1), sizeof(before));
    run();

    expect_result(PB_RESULT_OK); /* init */
    expect_result(PB_RESULT_OK); /* prepare */
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK); /* write */
    /* The next host */
    expect_result(-PB_RESULT_STREAM_NOT_INITIALIZED);
    expect_result(-PB_RESULT_STREAM_NOT_INITIALIZED);
    expect_result(PB_RESULT_OK); /* finalize */
    expect_end();
    assert_int_equal(write_done_calls, 0);
    assert_memory_equal(ramdisk_block(f->disk, RW_FIRST_LBA + 1), before, sizeof(before));
}

/*
 * What doesn't end the session (command-mode.md 3.1, 3.3): a command that
 * fails, and an AUTHENTICATE with a method the build doesn't have, leave it
 * as it was. A failed authentication leaves it not authenticated and keeps
 * the stream. The transport that is lost then ends it.
 */
static void test_session_survives_failures(void **state)
{
    struct fixture *f = *state;
    struct pb_command_board fail = { .command = BOARD_RETURN, .request_size = 4 };
    int32_t fail_rc = -PB_ERR_IO;
    uint8_t fail_req[1024] = { 0 };
    struct pb_command_authenticate no_method = { .method = 0xee, .size = 2, .key_id = KEY_ID };
    static const uint8_t bad_token[] = { 0x00 };
    static const uint8_t data[RAMDISK_BLOCK_SIZE] = { 0x5c };

    write_done_calls = 0;
    write_done_rc = PB_OK;
    assert_int_equal(bio_set_write_done_cb(f->rw, fake_write_done), PB_OK);
    memcpy(fail_req, &fail_rc, sizeof(fail_rc));
    sec.slc = SLC_CONFIGURATION_LOCKED;

    host_auth_token();
    stream_init(UU_RW);
    stream_prepare(1, sizeof(data), data);
    /* A board command that fails, a method that isn't in the build */
    host_cmd(PB_CMD_BOARD_COMMAND, &fail, sizeof(fail));
    host_send(fail_req, sizeof(fail_req));
    host_cmd(PB_CMD_AUTHENTICATE, &no_method, sizeof(no_method));
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    /* A token that doesn't verify */
    host_auth(PB_AUTH_ASYM_TOKEN, bad_token, sizeof(bad_token), KEY_ID);
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    /* Authenticated again, the stream is still there */
    host_auth_token();
    stream_write(1, sizeof(data), 0);
    host_cmd(PB_CMD_STREAM_FINALIZE, NULL, 0);
    host_lost(-PB_ERR_IO);
    host_cmd(PB_CMD_PART_TBL_READ, NULL, 0);
    run();

    expect_result(PB_RESULT_OK); /* auth */
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK); /* init */
    expect_result(PB_RESULT_OK); /* prepare */
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK); /* board command */
    expect_result(-PB_RESULT_IO_ERROR);
    expect_result(-PB_RESULT_NOT_SUPPORTED); /* the method, no data read */
    expect_part_table();
    expect_result(PB_RESULT_OK); /* the bad token */
    expect_result(-PB_RESULT_AUTHENTICATION_FAILED);
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_result(PB_RESULT_OK); /* auth */
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK); /* write */
    expect_result(PB_RESULT_OK); /* finalize */
    /* The next host */
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_end();
    assert_int_equal(write_done_calls, 1);
    assert_memory_equal(ramdisk_block(f->disk, RW_FIRST_LBA), data, sizeof(data));
}

/* The unit tests print with the system printf, so only what a test puts in
 * the RAM log is there */
static void ramlog_puts(const char *s)
{
    while (*s)
        ramlog_putc(*s++);
}

static void host_log_read_from(uint32_t log, uint32_t position, uint32_t size)
{
    struct pb_command_log_read log_cmd = { .position = position, .size = size, .log = log };
    host_cmd(PB_CMD_LOG_READ, &log_cmd, sizeof(log_cmd));
}

static void host_log_read(uint32_t position, uint32_t size)
{
    host_log_read_from(0, position, size);
}

static const struct pb_result_log_read *expect_log_result(void)
{
    return (const struct pb_result_log_read *)expect_result(PB_RESULT_OK)->response;
}

static void test_log_read(void **state)
{
    const struct pb_result_log_read *lr;
    (void)state;

    ramlog_init();
    ramlog_puts("hello world");
    host_log_read(0, 100);
    host_log_read(6, 3);
    host_log_read(11, 100);
    host_log_read(1000, 100);
    host_log_read(0, 0);
    run();

    lr = expect_log_result();
    assert_int_equal(lr->position, 0);
    assert_int_equal(lr->size, 11);
    assert_int_equal(lr->head, 11);
    assert_memory_equal(expect_data(11), "hello world", 11);
    expect_result(PB_RESULT_OK);

    lr = expect_log_result();
    assert_int_equal(lr->position, 6);
    assert_int_equal(lr->size, 3);
    assert_memory_equal(expect_data(3), "wor", 3);
    expect_result(PB_RESULT_OK);

    /* At or past the end: no data, and the result is the only one */
    lr = expect_log_result();
    assert_int_equal(lr->position, 11);
    assert_int_equal(lr->size, 0);
    assert_int_equal(lr->head, 11);
    lr = expect_log_result();
    assert_int_equal(lr->position, 11);
    assert_int_equal(lr->size, 0);

    /* Size 0 only asks for the head */
    lr = expect_log_result();
    assert_int_equal(lr->position, 0);
    assert_int_equal(lr->size, 0);
    assert_int_equal(lr->head, 11);
    expect_end();
}

/* Positions that were overwritten start at the oldest byte, and a read is at
 * most one command buffer */
static void test_log_read_wrapped(void **state)
{
    const struct pb_result_log_read *lr;
    const size_t total = LOG_SIZE + 100;
    const uint8_t *data;
    (void)state;

    ramlog_init();
    for (size_t i = 0; i < total; i++)
        ramlog_putc((char)(i % 251));
    host_log_read(0, 0xffffffff);
    run();

    lr = expect_log_result();
    assert_int_equal(lr->position, 100);
    assert_int_equal(lr->size, MIN(BUF_SIZE, LOG_SIZE));
    assert_int_equal(lr->head, total);
    data = expect_data(lr->size);
    for (size_t i = 0; i < lr->size; i++)
        assert_int_equal(data[i], (100 + i) % 251);
    expect_result(PB_RESULT_OK);
    expect_end();
}

/* A log that doesn't change, like the boot ROM's */
static const char static_text[] = "static text";

static uint32_t static_head(void)
{
    return sizeof(static_text) - 1;
}

static size_t static_length(void)
{
    return sizeof(static_text) - 1;
}

static size_t static_read(size_t offset, void *buf, size_t length)
{
    if (offset >= sizeof(static_text) - 1)
        return 0;
    length = MIN(length, sizeof(static_text) - 1 - offset);
    memcpy(buf, &static_text[offset], length);
    return length;
}

static const struct log_source static_log = {
    .name = "test",
    .description = "A test log",
    .head = static_head,
    .length = static_length,
    .read = static_read,
};

static uint32_t log_id(const char *name)
{
    return crc32(0, (const uint8_t *)name, (uint32_t)strlen(name));
}

/* Logs are selected by pb_id() of their name, 0 is pb's, and the result says
 * which one was read */
static void test_log_select(void **state)
{
    const struct pb_result_log_read *lr;
    (void)state;

    assert_int_equal(log_id("pb"), PB_LOG_ID_PB);
    assert_int_equal(log_id("list"), PB_LOG_ID_LIST);
    assert_int_equal(log_source_register(&static_log), PB_OK);
    assert_int_equal(log_source_register(&static_log), PB_OK);

    ramlog_init();
    ramlog_puts("hello");
    host_log_read_from(0, 0, 100);
    host_log_read_from(PB_LOG_ID_PB, 0, 100);
    host_log_read_from(log_id("test"), 7, 100);
    host_log_read_from(log_id("nope"), 0, 100);
    host_log_read_from(PB_LOG_ID_LIST, 0, 100);
    run();

    for (int i = 0; i < 2; i++) {
        lr = expect_log_result();
        assert_int_equal(lr->log, PB_LOG_ID_PB);
        assert_int_equal(lr->size, 5);
        assert_memory_equal(expect_data(5), "hello", 5);
        expect_result(PB_RESULT_OK);
    }

    /* A static log: positions are offsets, its head is its length */
    lr = expect_log_result();
    assert_int_equal(lr->log, log_id("test"));
    assert_int_equal(lr->position, 7);
    assert_int_equal(lr->head, 11);
    assert_int_equal(lr->size, 4);
    assert_memory_equal(expect_data(4), "text", 4);
    expect_result(PB_RESULT_OK);

    expect_result(-PB_RESULT_NOT_FOUND);

    static const char list[] = "pb pb's console output\ntest A test log\n";
    lr = expect_log_result();
    assert_int_equal(lr->log, PB_LOG_ID_LIST);
    assert_int_equal(lr->size, sizeof(list) - 1);
    assert_memory_equal(expect_data(sizeof(list) - 1), list, sizeof(list) - 1);
    expect_result(PB_RESULT_OK);
    expect_end();
}

static void test_log_source_full(void **state)
{
    static const struct log_source more[] = {
        { .name = "a", .description = "" },
        { .name = "b", .description = "" },
        { .name = "c", .description = "" },
        { .name = "d", .description = "" },
    };
    int rc = PB_OK;
    (void)state;

    /* 'static_log' from test_log_select takes one of the four places */
    for (size_t i = 0; i < ARRAY_SIZE(more) && rc == PB_OK; i++)
        rc = log_source_register(&more[i]);
    assert_int_equal(rc, -PB_ERR_MEM);
}

static void test_log_read_requires_auth(void **state)
{
    (void)state;

    sec.slc = SLC_CONFIGURATION_LOCKED;
    host_log_read(0, 100);
    run();

    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_end();
}

/*
 * The gate over every frame (command-mode.md 3.2, 4.1, 4.5, F-275): each
 * command id the wire's byte can carry, with the right magic and with a wrong
 * one, in each kind of session. Each frame is a session of its own with a
 * zeroed request, and the first result is the one checked.
 */
enum gate_session {
    GATE_OPEN, /* CONFIGURATION: the device is open */
    GATE_AUTHENTICATED, /* Locked, after a token */
    GATE_REFUSED, /* Locked, not authenticated */
    GATE_REFUSED_UNKNOWN, /* The life cycle can't be read, not authenticated */
};

#define WRONG_MAGIC 0x30424c50 /* 'PBL0' with its bytes the other way round */

static int gate_first_result(enum gate_session kind, uint32_t magic, unsigned int command)
{
    struct pb_command frame;

    memset(&t, 0, sizeof(t));
    t.max_inits = 1;
    t.fail_reads_after = -1;
    t.fail_writes_after = -1;
    t.connect_ok_after = -1;
    memset(&fake_boot, 0, sizeof(fake_boot));
    memset(&sec, 0, sizeof(sec));
    sec.key_kind = DSA_EC_SECP256r1;
    if (kind == GATE_OPEN)
        sec.slc = SLC_CONFIGURATION;
    else if (kind == GATE_REFUSED_UNKNOWN)
        sec.slc = -PB_ERR_IO;
    else
        sec.slc = SLC_CONFIGURATION_LOCKED;

    if (kind == GATE_AUTHENTICATED)
        host_auth_token();

    memset(&frame, 0, sizeof(frame));
    frame.magic = magic;
    frame.command = (uint8_t)command;
    host_send(&frame, sizeof(frame));
    (void)cm_run(); /* Some commands reset, jump or wait for data */

    if (kind == GATE_AUTHENTICATED) {
        expect_result(PB_RESULT_OK);
        expect_result(PB_RESULT_OK);
    }

    const struct pb_result *r = (const struct pb_result *)expect_data(sizeof(struct pb_result));
    assert_int_equal(r->magic, PB_WIRE_MAGIC);
    return r->result_code;
}

static void expect_gate(enum gate_session kind, uint32_t magic, unsigned int command, int expected)
{
    int got = gate_first_result(kind, magic, command);

    if (got != expected)
        fail_msg("Session %d, magic %08x, command %u: %d, expected %d",
                 kind,
                 (unsigned int)magic,
                 command,
                 got,
                 expected);
}

/*
 * What an open or authenticated session gets with a zeroed request, pinned
 * before F-275 was fixed. The command's own answer to that request: a
 * partition with the nil UUID isn't found, method 0 isn't in the build.
 */
static const int8_t gate_open_results[PB_CMD_END] = {
    [PB_CMD_INVALID] = -PB_RESULT_INVALID_COMMAND,
    [PB_CMD_DEVICE_RESET] = PB_RESULT_OK,
    [PB_CMD_DEVICE_IDENTIFIER_READ] = PB_RESULT_OK,
    [PB_CMD_DEVICE_READ_CAPS] = PB_RESULT_OK,
    [PB_CMD_SLC_SET_CONFIGURATION] = PB_RESULT_OK,
    [PB_CMD_SLC_SET_CONFIGURATION_LOCK] = PB_RESULT_OK,
    [PB_CMD_SLC_SET_EOL] = PB_RESULT_OK,
    [PB_CMD_SLC_REVOKE_KEY] = PB_RESULT_OK,
    [PB_CMD_SLC_READ] = PB_RESULT_OK,
    [PB_CMD_BOOTLOADER_VERSION_READ] = PB_RESULT_OK,
    [PB_CMD_PART_TBL_READ] = PB_RESULT_OK,
    [PB_CMD_PART_TBL_INSTALL] = -PB_RESULT_NOT_FOUND,
    [PB_CMD_PART_VERIFY] = -PB_RESULT_NOT_FOUND,
    [PB_CMD_PART_ACTIVATE] = PB_RESULT_OK,
    [PB_CMD_PART_BPAK_READ] = -PB_RESULT_NOT_FOUND,
    [PB_CMD_PART_ERASE] = -PB_RESULT_NOT_FOUND,
    [PB_CMD_AUTHENTICATE] = -PB_RESULT_NOT_SUPPORTED,
    [PB_CMD_AUTH_SET_PASSWORD] = -PB_RESULT_NOT_SUPPORTED,
    [PB_CMD_STREAM_INITIALIZE] = -PB_RESULT_NOT_FOUND,
    [PB_CMD_STREAM_PREPARE_BUFFER] = PB_RESULT_OK,
    [PB_CMD_STREAM_WRITE_BUFFER] = -PB_RESULT_STREAM_NOT_INITIALIZED,
    [PB_CMD_STREAM_FINALIZE] = PB_RESULT_OK,
    [PB_CMD_BOOT_PART] = PB_RESULT_OK,
    [PB_CMD_BOOT_BPAK] = PB_RESULT_OK,
    [PB_CMD_BOARD_COMMAND] = PB_RESULT_OK,
    [PB_CMD_BOARD_STATUS_READ] = PB_RESULT_OK,
    [PB_CMD_STREAM_READ_BUFFER] = -PB_RESULT_STREAM_NOT_INITIALIZED,
    [PB_CMD_PART_RESIZE] = -PB_RESULT_NOT_SUPPORTED,
    [PB_CMD_BOOT_STATUS] = PB_RESULT_OK,
    [PB_CMD_LOG_READ] = PB_RESULT_OK,
};

/* An open or authenticated session: what it got before F-275, in the same
 * order of checks. A frame that isn't a command, and a number this release
 * doesn't define, are INVALID_COMMAND (4.5). */
static void test_gate_open_sessions(void **state)
{
    static const enum gate_session kinds[] = { GATE_OPEN, GATE_AUTHENTICATED };

    (void)state;

    for (size_t k = 0; k < ARRAY_SIZE(kinds); k++) {
        for (unsigned int c = 0; c < 256; c++) {
            int expected = (c < PB_CMD_END) ? gate_open_results[c] : -PB_RESULT_INVALID_COMMAND;

            expect_gate(kinds[k], PB_WIRE_MAGIC, c, expected);
            expect_gate(kinds[k], WRONG_MAGIC, c, -PB_RESULT_INVALID_COMMAND);
            expect_gate(kinds[k], 0, c, -PB_RESULT_INVALID_COMMAND);
        }
    }
}

/* A session the gate refuses: NOT_AUTHENTICATED and nothing else for every
 * frame but the two commands, also a number pb doesn't define and a frame
 * whose magic is wrong, so that it doesn't tell which commands it has
 * (D-058). The two run as in an open session. */
static void test_gate_refused_sessions(void **state)
{
    static const enum gate_session kinds[] = { GATE_REFUSED, GATE_REFUSED_UNKNOWN };

    (void)state;

    for (size_t k = 0; k < ARRAY_SIZE(kinds); k++) {
        for (unsigned int c = 0; c < 256; c++) {
            if (c == PB_CMD_DEVICE_IDENTIFIER_READ || c == PB_CMD_AUTHENTICATE) {
                expect_gate(kinds[k], PB_WIRE_MAGIC, c, gate_open_results[c]);
            } else {
                expect_gate(kinds[k], PB_WIRE_MAGIC, c, -PB_RESULT_NOT_AUTHENTICATED);
                /* Hosts send their data anyway: the second result (4.4) */
                if (c == PB_CMD_BOARD_COMMAND || c == PB_CMD_STREAM_PREPARE_BUFFER)
                    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
                expect_end();
            }

            expect_gate(kinds[k], WRONG_MAGIC, c, -PB_RESULT_NOT_AUTHENTICATED);
            expect_end();
            expect_gate(kinds[k], 0, c, -PB_RESULT_NOT_AUTHENTICATED);
            expect_end();
        }
    }
}

/* The gate refuses BOARD_COMMAND, and hosts send its request whatever the
 * first result says (command-mode.md 3.2, 4.4, F-187): the device reads the
 * request, answers twice and runs nothing. With no request it answers twice
 * as well. The session stays in step. */
static void test_gate_refuses_board_command(void **state)
{
    struct pb_command_board req = { .command = BOARD_ECHO, .request_size = 5 };
    uint8_t arg[1024] = { 0 };
    (void)state;

    /* A request that is a frame the gate lets through, were it read as one */
    pb_wire_init_command((struct pb_command *)arg, PB_CMD_DEVICE_IDENTIFIER_READ);
    sec.slc = SLC_CONFIGURATION_LOCKED;
    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    host_send(arg, sizeof(arg));
    req.request_size = 0;
    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    host_cmd(PB_CMD_DEVICE_IDENTIFIER_READ, NULL, 0);
    run();

    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    const struct pb_result *r = expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    assert_int_equal(((const struct pb_result_board *)r->response)->size, 0);
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_result(PB_RESULT_OK);
    expect_end();
}

/* The gate refuses STREAM_PREPARE_BUFFER, and hosts send its data whatever
 * the first result says (command-mode.md 3.2, 4.4, F-274): the device reads
 * the data, answers twice and fills no buffer. */
static void test_gate_refuses_stream_prepare(void **state)
{
    struct pb_command frame;
    (void)state;

    /* Data that is a frame the gate lets through, were it read as one */
    pb_wire_init_command(&frame, PB_CMD_DEVICE_IDENTIFIER_READ);
    sec.slc = SLC_CONFIGURATION_LOCKED;
    stream_prepare(0, sizeof(frame), &frame);
    host_cmd(PB_CMD_DEVICE_IDENTIFIER_READ, NULL, 0);
    run();

    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_result(PB_RESULT_OK);
    expect_end();
}

/* One session: refused frames leave it able to authenticate, and after that
 * the same frames get what an open device answers. */
static void test_gate_refused_then_authenticated(void **state)
{
    struct pb_command bad_magic;
    (void)state;

    pb_wire_init_command(&bad_magic, PB_CMD_DEVICE_IDENTIFIER_READ);
    bad_magic.magic = WRONG_MAGIC;

    sec.slc = SLC_CONFIGURATION_LOCKED;
    host_send(&bad_magic, sizeof(bad_magic));
    host_cmd(PB_CMD_END, NULL, 0);
    host_cmd(PB_CMD_INVALID, NULL, 0);
    host_cmd(PB_CMD_DEVICE_IDENTIFIER_READ, NULL, 0);
    host_auth_token();
    host_send(&bad_magic, sizeof(bad_magic));
    host_cmd(PB_CMD_END, NULL, 0);
    host_cmd(PB_CMD_INVALID, NULL, 0);
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    run();

    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_INVALID_COMMAND);
    expect_result(-PB_RESULT_INVALID_COMMAND);
    expect_result(-PB_RESULT_INVALID_COMMAND);
    expect_result(PB_RESULT_OK);
    expect_end();
}

#define CM_TEST(f) cmocka_unit_test_setup_teardown(f, setup, teardown)

int main(void)
{
    const struct CMUnitTest tests[] = {
        CM_TEST(test_basic_commands),
        CM_TEST(test_board_id_fills_the_field),
        CM_TEST(test_identifier_sends_only_the_identifier),
        CM_TEST(test_invalid_command),
        CM_TEST(test_deprecated_commands),
        CM_TEST(test_part_table_read),
        CM_TEST(test_part_table_sends_only_the_table),
        CM_TEST(test_part_table_install),
        CM_TEST(test_part_activate),
        CM_TEST(test_boot_status),
        CM_TEST(test_board_status),
        CM_TEST(test_board_status_fails),
        CM_TEST(test_board_command),
        CM_TEST(test_board_command_request_size),
        CM_TEST(test_board_response_fits_the_hosts_buffer),
        CM_TEST(test_error_codes),
        CM_TEST(test_board_command_requests_reboot),
        CM_TEST(test_device_reset),
        CM_TEST(test_locked_device_requires_auth),
        CM_TEST(test_auth_token_key_kinds),
        CM_TEST(test_auth_token_errors),
        CM_TEST(test_auth_password),
        CM_TEST(test_auth_data_size),
        CM_TEST(test_auth_refusal_keeps_session),
        CM_TEST(test_auth_data_read_fails),
        CM_TEST(test_auth_failure_is_delayed),
        CM_TEST(test_auth_failure_delay_counts_from_the_data),
        CM_TEST(test_auth_not_delayed),
        CM_TEST(test_slc_commands),
        CM_TEST(test_slc_errors),
        CM_TEST(test_unknown_slc_requires_auth),
        CM_TEST(test_slc_read_key_status),
        CM_TEST(test_slc_read_lists_16_keys_at_most),
        CM_TEST(test_bpak_read),
        CM_TEST(test_bpak_read_errors),
        CM_TEST(test_bpak_read_small_partition),
        CM_TEST(test_part_verify),
        CM_TEST(test_part_verify_bpak),
        CM_TEST(test_part_verify_kicks_per_chunk),
        CM_TEST(test_part_verify_errors),
        CM_TEST(test_part_verify_small_partition),
        CM_TEST(test_part_verify_bpak_size_below_a_header),
        CM_TEST(test_part_erase),
        CM_TEST(test_part_erase_needs_writable),
        CM_TEST(test_hidden_partition),
        CM_TEST(test_stream),
        CM_TEST(test_stream_size_that_isnt_whole_blocks),
        CM_TEST(test_part_verify_size_that_isnt_whole_blocks),
        CM_TEST(test_part_verify_size_that_isnt_whole_blocks_last_byte),
        CM_TEST(test_part_verify_keeps_the_stream),
        CM_TEST(test_stream_errors),
        CM_TEST(test_stream_offset_inside_a_block),
        CM_TEST(test_stream_write_done),
        CM_TEST(test_boot_part),
        CM_TEST(test_boot_bpak),
        CM_TEST(test_boot_bpak_refused_before_the_header),
        CM_TEST(test_run_errors),
        CM_TEST(test_setup_platform_first),
        CM_TEST(test_setup_platform_fails),
        CM_TEST(test_task_polls_once_per_call),
        CM_TEST(test_run_connect),
        CM_TEST(test_run_retries),
        CM_TEST(test_transport_errors_in_commands),
        CM_TEST(test_session_ends_with_the_transport),
        CM_TEST(test_session_ends_with_a_lost_transfer),
        CM_TEST(test_transfer_timeout_read),
        CM_TEST(test_transfer_timeout_write),
        CM_TEST(test_session_stream_ends_with_the_transport),
        CM_TEST(test_session_survives_failures),
        CM_TEST(test_log_read),
        CM_TEST(test_log_read_wrapped),
        CM_TEST(test_log_read_requires_auth),
        CM_TEST(test_log_select),
        CM_TEST(test_log_source_full),
        CM_TEST(test_gate_open_sessions),
        CM_TEST(test_gate_refused_sessions),
        CM_TEST(test_gate_refuses_board_command),
        CM_TEST(test_gate_refuses_stream_prepare),
        CM_TEST(test_gate_refused_then_authenticated),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}
