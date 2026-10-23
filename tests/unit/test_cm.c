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
 * A test queues the host's side of the conversation (commands and data) with
 * host_*(), runs cm_run() until the queue is empty, and then checks what the
 * device sent back with expect_*(). When the queue runs dry the transport
 * reports a disconnect, cm_run() re-initializes the transport, and the fake
 * fails that, which ends cm_run() with -PB_ERR_IO.
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
#include <pb/crypto.h>
#include <pb/device_uuid.h>
#include <pb/pb.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <stdlib.h>
#include <string.h>

#include "fakes/plat.h"
#include "fakes/ramdisk.h"

#define BUF_SIZE    (CONFIG_CM_BUF_SIZE_KiB * 1024)
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

#define RW_FIRST_LBA 10
#define RW_BLOCKS    20
#define DEVICE_UUID  "02e49231-756e-35ee-a982-378e5ba866a9"
#define KEY_ID       0xa90f9680
#define FAKE_SIG_OK  0x5a
#define BOARD_ECHO   1
#define BOARD_RETURN 2
#define BOARD_REBOOT 3
#define BOARD_PREFIX "echo: "

/* The fake transport */
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
    int fail_reads_after; /* -1: never, n: read() fails after n calls */
    int fail_writes_after;

    int max_inits;
    int init_calls;
    int connect_rc; /* Returned by connect() until connect_ok_after calls */
    int connect_ok_after;
    int connect_calls;
    int disconnect_calls;
    int process_calls;
} t;

static int fake_init(void)
{
    t.init_calls++;
    return (t.init_calls > t.max_inits) ? -PB_ERR_IO : PB_OK;
}

static int fake_connect(void)
{
    t.connect_calls++;
    if (t.connect_ok_after >= 0 && t.connect_calls > t.connect_ok_after)
        return PB_OK;
    return t.connect_rc;
}

static int fake_disconnect(void)
{
    t.disconnect_calls++;
    return PB_OK;
}

static int fake_read(void *buf, size_t length)
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

static int fake_write(const void *buf, size_t length)
{
    if (t.fail_writes_after == 0)
        return -PB_ERR_IO;
    if (t.fail_writes_after > 0)
        t.fail_writes_after--;

    assert_true(t.out_len + length <= sizeof(t.out));
    memcpy(&t.out[t.out_len], buf, length);
    t.out_len += length;
    return PB_OK;
}

static int fake_complete(void)
{
    if (t.n_complete_rcs > 0) {
        int rc = t.complete_rcs[0];
        memmove(&t.complete_rcs[0], &t.complete_rcs[1], sizeof(int) * --t.n_complete_rcs);
        return rc;
    }

    if (t.read_pending) {
        if (t.in_pos + t.read_len > t.in_len)
            return -PB_ERR_IO; /* The host is gone */
        memcpy(t.read_buf, &t.in[t.in_pos], t.read_len);
        t.in_pos += t.read_len;
        t.read_pending = false;
    }

    return PB_OK;
}

static void fake_process(void)
{
    t.process_calls++;
}

/* The board */
static int board_init_rc;
static int board_status(uint8_t *buf, size_t *length);
static int
board_command(uint32_t cmd, uint8_t *bfr, size_t bfr_length, uint8_t *rsp, size_t *rsp_size);
static int password_auth(const char *password, size_t length);

#define TOPS .init = fake_init, .read = fake_read, .write = fake_write, .complete = fake_complete

static const struct cm_config cfg_default = {
    .name = "unit test",
    .status = board_status,
    .command = board_command,
    .tops = { TOPS },
};

static const struct cm_config cfg_all_callbacks = {
    .name = "unit test",
    .status = board_status,
    .command = board_command,
    .password_auth = password_auth,
    .process = fake_process,
    .tops = { TOPS, .connect = fake_connect, .disconnect = fake_disconnect },
};

static const struct cm_config *board_cfg;

int cm_board_init(void)
{
    if (board_init_rc != PB_OK)
        return board_init_rc;
    return cm_init(board_cfg);
}

static int board_status(uint8_t *buf, size_t *length)
{
    *length = snprintf((char *)buf, *length, "status ok");
    return PB_OK;
}

static int
board_command(uint32_t cmd, uint8_t *bfr, size_t bfr_length, uint8_t *rsp, size_t *rsp_size)
{
    switch (cmd) {
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
    (void)read_f;
    (void)result_f;
}

int boot_load(uuid_t boot_part_override_uu)
{
    fake_boot.load_calls++;
    uuid_copy(fake_boot.load_uu, boot_part_override_uu);
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
    return 2;
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
    fake_us_tick_step = 0;

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

    r = expect_result(PB_RESULT_OK);
    const struct pb_result_device_identifier *id = (const void *)r->response;
    assert_string_equal(id->board_id, "unit test");
    uuid_t uu;
    uuid_parse(DEVICE_UUID, uu);
    assert_memory_equal(id->device_uuid, uu, 16);
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

static void test_board_command(void **state)
{
    struct pb_command_board req = { .command = BOARD_ECHO, .request_size = 5 };
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
    struct pb_command_board req = { .command = BOARD_ECHO, .request_size = 1024 };
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
}

static void test_device_reset(void **state)
{
    (void)state;

    host_cmd(PB_CMD_DEVICE_RESET, NULL, 0);
    assert_int_equal(cm_run(), -PB_ERR_ABORT);
    expect_result(PB_RESULT_OK);
    expect_end();
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
    /* The 1024 bytes of password data are read as two commands, which are
     * invalid */
    expect_result(-PB_RESULT_INVALID_COMMAND);
    expect_result(-PB_RESULT_INVALID_COMMAND);
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
 * A bad size is refused before the data, and leaves the device locked. */
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
        expect_result(-PB_RESULT_NOT_AUTHENTICATED);
    }
    expect_end();
}

/* F-005: when the data doesn't arrive, the device must not check whatever the
 * buffer held from the last authentication */
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
    expect_result(-PB_RESULT_IO_ERROR);
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
    expect_result(-PB_RESULT_IO_ERROR);
    expect_end();
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

    t.init_calls = 0;
    sec.slc = -PB_ERR_IO;
    host_cmd(PB_CMD_SLC_READ, NULL, 0);
    run();
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

static void test_part_erase(void **state)
{
    struct pb_command_erase_part req = { .start_lba = 0, .block_count = 1 };
    (void)state;

    memcpy(req.uuid, UU_MISSING, 16);
    host_cmd(PB_CMD_PART_ERASE, &req, sizeof(req));
    /* The ramdisk can't erase */
    memcpy(req.uuid, UU_RW, 16);
    host_cmd(PB_CMD_PART_ERASE, &req, sizeof(req));
    run();

    expect_result(-PB_RESULT_NOT_FOUND);
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

static void test_stream_errors(void **state)
{
    struct fixture *f = *state;
    (void)state;

    /* The stream_check() bounds are covered by test_stream.py */
    stream_init(UU_MISSING);
    /* Without a partition the stream is not initialized */
    stream_write(0, RAMDISK_BLOCK_SIZE, 0);
    stream_prepare(0, BUF_SIZE + 1, NULL);
    stream_prepare(2, 16, NULL);
    stream_init(UU_RO);
    stream_write(0, RAMDISK_BLOCK_SIZE, 0);
    stream_read(0, RAMDISK_BLOCK_SIZE, 0);
    run();

    expect_result(-PB_RESULT_NOT_FOUND);
    expect_result(-PB_RESULT_STREAM_NOT_INITIALIZED);
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
    stream_prepare(0, BUF_SIZE + 1, NULL);
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
    expect_result(-PB_RESULT_NO_MEMORY); /* prepare */
    expect_result(PB_RESULT_OK); /* prepare */
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK); /* write */
    expect_result(PB_RESULT_OK); /* finalize */
    expect_end();
    assert_int_equal(write_done_calls, 1);
    assert_int_equal(write_done_dev, f->rw);

    /* A failed write, then a good one */
    t.init_calls = 0;
    write_done_calls = 0;
    f->disk->fail_writes_after = 0;
    stream_write_block(0);
    run();
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_IO_ERROR);
    expect_end();

    t.init_calls = 0;
    f->disk->fail_writes_after = -1;
    stream_prepare(1, RAMDISK_BLOCK_SIZE, ramdisk_block(f->disk, 0));
    stream_write(1, RAMDISK_BLOCK_SIZE, 0);
    host_cmd(PB_CMD_STREAM_FINALIZE, NULL, 0);
    /* The driver's error is the finalize result */
    write_done_rc = -PB_ERR_IO;
    stream_write_block(0);
    host_cmd(PB_CMD_STREAM_FINALIZE, NULL, 0);
    run();
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
    (void)state;

    memcpy(req.uuid, UU_RW, 16);
    fake_boot.load_rc = -PB_ERR_SIGNATURE;
    host_cmd(PB_CMD_BOOT_BPAK, &req, sizeof(req));
    run();
    /* The image transfer and its results are handled by the boot flow */
    expect_result(PB_RESULT_OK);
    expect_end();
    assert_int_equal(fake_boot.source, BOOT_SOURCE_CB);
    assert_int_equal(fake_boot.flags, BOOT_FLAG_CMD);

    t.init_calls = 0;
    fake_boot.load_rc = PB_OK;
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
    assert_true(t.process_calls >= 1);

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

    /* The command read completes after a timeout and a retry */
    t.complete_rcs[0] = -PB_ERR_TIMEOUT;
    t.complete_rcs[1] = -PB_ERR_AGAIN;
    t.n_complete_rcs = 2;
    host_cmd(PB_CMD_BOOTLOADER_VERSION_READ, NULL, 0);
    /* The host reconnects */
    t.max_inits = 2;
    run();
    expect_result(PB_RESULT_OK);
    assert_int_equal(t.init_calls, 3);
}

static void test_transport_errors_in_commands(void **state)
{
    struct pb_command_board req = { .command = BOARD_ECHO, .request_size = 5 };
    (void)state;

    /* The request data never arrives */
    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    run();
    expect_result(PB_RESULT_OK);

    /* The data read can't be started, and neither can the next command's */
    t.init_calls = 0;
    t.out_len = t.out_pos = 0;
    t.read_pending = false;
    host_cmd(PB_CMD_BOARD_COMMAND, &req, sizeof(req));
    t.fail_reads_after = 1;
    run();
    expect_result(PB_RESULT_OK);
    expect_result(-PB_RESULT_IO_ERROR);
    expect_end();
}

#define CM_TEST(f) cmocka_unit_test_setup_teardown(f, setup, teardown)

int main(void)
{
    const struct CMUnitTest tests[] = {
        CM_TEST(test_basic_commands),
        CM_TEST(test_invalid_command),
        CM_TEST(test_deprecated_commands),
        CM_TEST(test_part_table_read),
        CM_TEST(test_part_table_install),
        CM_TEST(test_part_activate),
        CM_TEST(test_boot_status),
        CM_TEST(test_board_status),
        CM_TEST(test_board_command),
        CM_TEST(test_board_command_request_size),
        CM_TEST(test_error_codes),
        CM_TEST(test_board_command_requests_reboot),
        CM_TEST(test_device_reset),
        CM_TEST(test_locked_device_requires_auth),
        CM_TEST(test_auth_token_key_kinds),
        CM_TEST(test_auth_token_errors),
        CM_TEST(test_auth_password),
        CM_TEST(test_auth_data_size),
        CM_TEST(test_auth_data_read_fails),
        CM_TEST(test_slc_commands),
        CM_TEST(test_slc_errors),
        CM_TEST(test_slc_read_key_status),
        CM_TEST(test_bpak_read),
        CM_TEST(test_bpak_read_errors),
        CM_TEST(test_part_verify),
        CM_TEST(test_part_verify_bpak),
        CM_TEST(test_part_verify_errors),
        CM_TEST(test_part_erase),
        CM_TEST(test_stream),
        CM_TEST(test_stream_errors),
        CM_TEST(test_stream_write_done),
        CM_TEST(test_boot_part),
        CM_TEST(test_boot_bpak),
        CM_TEST(test_run_errors),
        CM_TEST(test_run_connect),
        CM_TEST(test_run_retries),
        CM_TEST(test_transport_errors_in_commands),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}
