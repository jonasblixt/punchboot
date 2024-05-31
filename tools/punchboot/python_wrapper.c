/* Python 3.10 and newer must set PY_SSIZE_T_CLEAN when using # variant
 *  when parsing arguments */
#define PY_SSIZE_T_CLEAN
#include "python_wrapper.h"
#include "api.h"
#include "socket.h"
#include "usb.h"
#include <pb-tools/compat.h>
#include <pb-tools/error.h>
#include <pb-tools/wire.h>
#include <Python.h>
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#include <windows.h>
#else
#include <unistd.h>
#endif

#if PY_MAJOR_VERSION < 3
#error "Only python3 supported"
#endif

static int log_cb(struct pb_context *ctx, int level, const char *fmt, ...)
{
    static PyObject *logging = NULL;
    static PyObject *log_msg = NULL;
    char msg_buf[1024];
    int rc;
    (void)ctx;
    (void)level;

    if (logging == NULL) {
        logging = PyImport_ImportModuleNoBlock("logging");
        if (logging == NULL) {
            PyErr_SetString(PyExc_ImportError, "Could not import module 'logging'");
        }
    }

    va_list args;

    va_start(args, fmt);
    rc = vsnprintf(msg_buf, sizeof(msg_buf) - 1, fmt, args);

    if (rc > 0) {
        if (msg_buf[rc - 1] == '\n')
            msg_buf[rc - 1] = 0;

        log_msg = Py_BuildValue("s", msg_buf);
        PyObject_CallMethod(logging, "debug", "O", log_msg);
        Py_DECREF(log_msg);
    }
    va_end(args);

    return 0;
}

static int init_transport(const char *uuid, const char *socket_path, struct pb_context **ctx)
{
    int rc = 0;
    struct pb_context *local_ctx = NULL;

    rc = pb_api_create_context(&local_ctx, log_cb);
    if (rc != PB_RESULT_OK) {
        return rc;
    }

    if (socket_path) {
#ifdef __linux__
        rc = pb_socket_transport_init(local_ctx, socket_path);
#else
        rc = -PB_RESULT_NOT_SUPPORTED;
#endif
    } else {
        rc = pb_usb_transport_init(local_ctx, uuid);
    }
    if (rc != PB_RESULT_OK) {
        goto err_free_ctx;
    }

    rc = local_ctx->connect(local_ctx);
    if (rc != PB_RESULT_OK) {
        goto err_free_ctx;
    }
    *ctx = local_ctx;
    return PB_RESULT_OK;

err_free_ctx:
    pb_api_free_context(local_ctx);
    return rc;
}

struct pb_session {
    PyObject_HEAD struct pb_context *ctx;
};

static int validate_pb_session(struct pb_session *s)
{
    if (s->ctx == NULL) {
        PyErr_SetString(PyExc_IOError, "Session is invalidated, must re-init");
        return -1;
    }
    return 0;
}

static int PbSession_init(struct pb_session *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = { "uuid", "socket_path", NULL };
    char *device_uuid_str = NULL;
    char *socket_path = NULL;
    int rc;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|zz", kwlist, &device_uuid_str, &socket_path)) {
        return -1;
    }

    rc = init_transport((const char *)device_uuid_str, socket_path, &self->ctx);
    if (rc != PB_RESULT_OK) {
        pb_exception_from_rc(rc);
        return -1;
    }

    return 0;
}

static void close_pb_session(struct pb_session *self)
{
    if (self->ctx) {
        pb_api_free_context(self->ctx);
    }
    self->ctx = NULL;
}

static void PbSession_dealloc(struct pb_session *self)
{
    close_pb_session(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *session_close(PyObject *self, PyObject *Py_UNUSED(args))
{
    struct pb_session *session = (struct pb_session *)self;
    close_pb_session(session);
    Py_RETURN_NONE;
}

static PyObject *authenticate(PyObject *self, PyObject *args, PyObject *kwds)
{
    struct pb_session *session = (struct pb_session *)self;
    static char *kwlist[] = { "password", NULL };
    const char *password;
    int rc;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &password)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_authenticate_password(session->ctx, (uint8_t *)password, strlen(password));
    if (rc != 0) {
        return pb_exception_from_rc(rc);
    }
    Py_RETURN_NONE;
}

static PyObject *auth_set_password(PyObject *self, PyObject *args, PyObject *kwds)
{
    struct pb_session *session = (struct pb_session *)self;
    static char *kwlist[] = { "password", NULL };
    const char *password;
    int rc;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &password)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_auth_set_password(session->ctx, password, strlen(password));
    if (rc != 0) {
        return pb_exception_from_rc(rc);
    }
    Py_RETURN_NONE;
}

static PyObject *authenticate_dsa_token(PyObject *self, PyObject *args, PyObject *kwds)
{
    struct pb_session *session = (struct pb_session *)self;
    static char *kwlist[] = { "password", "key_id", NULL };
    uint8_t *token_data = NULL;
    size_t token_data_length = 0;
    unsigned int key_id = -1;
    int rc;

    if (!PyArg_ParseTupleAndKeywords(
            args, kwds, "y#I", kwlist, &token_data, &token_data_length, &key_id)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_authenticate_key(session->ctx, key_id, token_data, token_data_length);
    if (rc != 0) {
        return pb_exception_from_rc(rc);
    }
    Py_RETURN_NONE;
}

static PyObject *device_reset(PyObject *self, PyObject *Py_UNUSED(args))
{
    struct pb_session *session = (struct pb_session *)self;
    int rc;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_device_reset(session->ctx);
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }
    pb_api_free_context(session->ctx);
    session->ctx = NULL;

    Py_RETURN_NONE;
}

static PyObject *device_get_punchboot_version(PyObject *self, PyObject *Py_UNUSED(args))
{
    struct pb_session *session = (struct pb_session *)self;
    char version[64];
    int rc;

    memset(version, 0, sizeof(version));

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_bootloader_version(session->ctx, version, sizeof(version));
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }

    return Py_BuildValue("s", version);
}

static int get_uuid(struct pb_context *ctx, uint8_t *uuid)
{
    uint8_t device_uu[16];
    char board_name[17];
    int rc;

    /* Arguments to this API are not optional */
    rc = pb_api_device_read_identifier(
        ctx, device_uu, sizeof(device_uu), board_name, sizeof(board_name));
    if (rc != PB_RESULT_OK) {
        return rc;
    }

    memcpy(uuid, &device_uu, sizeof(device_uu));

    return PB_RESULT_OK;
}

static PyObject *device_get_uuid(PyObject *self, PyObject *Py_UNUSED(args))
{
    struct pb_session *session = (struct pb_session *)self;
    uint8_t device_uu[16];
    int rc;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    /* Arguments to this API are not optional */
    rc = get_uuid(session->ctx, device_uu);
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }

    return Py_BuildValue("y#", device_uu, 16);
}

static PyObject *device_get_boardname(PyObject *self, PyObject *Py_UNUSED(args))
{
    struct pb_session *session = (struct pb_session *)self;
    uint8_t device_uu[16];
    char board_name[17];
    int rc = 0;

    memset(board_name, 0, sizeof(board_name));

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    /* Arguments to this API are not optional */
    rc = pb_api_device_read_identifier(
        session->ctx, device_uu, sizeof(device_uu), board_name, sizeof(board_name));
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }

    return Py_BuildValue("s", board_name);
}

static PyObject *slc_set_configuration(PyObject *self, PyObject *Py_UNUSED(args))
{
    struct pb_session *session = (struct pb_session *)self;
    int rc;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_slc_set_configuration(session->ctx);
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }
    Py_RETURN_NONE;
}

static PyObject *slc_set_configuration_locked(PyObject *self, PyObject *Py_UNUSED(args))
{
    struct pb_session *session = (struct pb_session *)self;
    int rc;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_slc_set_configuration_lock(session->ctx);
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }
    Py_RETURN_NONE;
}

static PyObject *slc_set_end_of_life(PyObject *self, PyObject *Py_UNUSED(args))
{
    struct pb_session *session = (struct pb_session *)self;
    int rc;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_slc_set_end_of_life(session->ctx);
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }
    Py_RETURN_NONE;
}

static PyObject *slc_revoke_key(PyObject *self, PyObject *args, PyObject *kwds)
{
    struct pb_session *session = (struct pb_session *)self;
    static char *kwlist[] = { "key_id", NULL };
    unsigned int key_id;

    int rc;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "I", kwlist, &key_id)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_slc_revoke_key(session->ctx, key_id);
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }
    Py_RETURN_NONE;
}

static PyObject *slc_get_lifecycle(PyObject *self, PyObject *Py_UNUSED(args))
{
    struct pb_session *session = (struct pb_session *)self;
    int rc;

    uint8_t slc;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_slc_read(session->ctx, &slc, NULL, NULL);
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }
    return Py_BuildValue("i", (int)slc);
}

#define PB_MAX_KEYS 16

static PyObject *slc_get_active_keys(PyObject *self, PyObject *Py_UNUSED(args))
{
    struct pb_session *session = (struct pb_session *)self;
    int rc;
    uint8_t slc;
    uint32_t active_keys[PB_MAX_KEYS];
    memset(active_keys, 0, sizeof(active_keys));

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_slc_read(session->ctx, &slc, (uint8_t *)active_keys, NULL);
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }

    PyObject *keylist = PyList_New(0);
    if (!keylist) {
        return PyErr_NoMemory();
    }

    for (int i = 0; i < PB_MAX_KEYS; i++) {
        if (active_keys[i]) {
            PyList_Append(keylist, PyLong_FromUnsignedLong(active_keys[i]));
        }
    }

    return keylist;
}

static PyObject *slc_get_revoked_keys(PyObject *self, PyObject *Py_UNUSED(args))
{
    struct pb_session *session = (struct pb_session *)self;
    int rc;
    uint8_t slc;
    uint32_t revoked_keys[PB_MAX_KEYS];
    memset(revoked_keys, 0, sizeof(revoked_keys));

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_slc_read(session->ctx, &slc, NULL, (uint8_t *)revoked_keys);
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }

    PyObject *keylist = PyList_New(0);
    if (!keylist) {
        return PyErr_NoMemory();
    }

    for (int i = 0; i < PB_MAX_KEYS; i++) {
        if (revoked_keys[i]) {
            PyList_Append(keylist, PyLong_FromUnsignedLong(revoked_keys[i]));
        }
    }

    return keylist;
}

static int read_part_table(struct pb_context *ctx,
                           struct pb_partition_table_entry **table,
                           int *entries)
{
    struct pb_partition_table_entry *tbl;
    int read_entries = 128;
    int rc;

    tbl = malloc(sizeof(*tbl) * read_entries);
    if (!tbl) {
        return -PB_RESULT_NO_MEMORY;
    }

    rc = pb_api_partition_read_table(ctx, tbl, &read_entries);
    if (rc != PB_RESULT_OK) {
        free(tbl);
        return rc;
    }

    if (read_entries == 0) {
        free(tbl);
        return -PB_RESULT_ERROR; // TODO: Better error?
    }

    *table = tbl;
    *entries = read_entries;
    return 0;
}

static PyObject *part_get_partitions(PyObject *self, PyObject *Py_UNUSED(args))
{
    struct pb_session *session = (struct pb_session *)self;
    PyObject *part_list = NULL;
    int rc = 0;
    int i;
    struct pb_partition_table_entry *tbl;
    int entries;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = read_part_table(session->ctx, &tbl, &entries);
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }

    part_list = PyList_New(entries);

    if (!part_list) {
        PyErr_NoMemory();
        goto err_free_tbl_out;
    }

    for (i = 0; i < entries; i++) {
        PyObject *part_tpl = PyTuple_New(6);
        PyTuple_SetItem(part_tpl, 0, Py_BuildValue("y#", tbl[i].uuid, 16));
        PyTuple_SetItem(part_tpl, 1, Py_BuildValue("s", tbl[i].description));
        PyTuple_SetItem(part_tpl, 2, Py_BuildValue("i", tbl[i].first_block));
        PyTuple_SetItem(part_tpl, 3, Py_BuildValue("i", tbl[i].last_block));
        PyTuple_SetItem(part_tpl, 4, Py_BuildValue("i", tbl[i].block_size));
        PyTuple_SetItem(part_tpl, 5, Py_BuildValue("i", tbl[i].flags));

        rc = PyList_SetItem(part_list, i, part_tpl);
        if (rc != 0) {
            goto err_free_list;
        }
    }
    free(tbl);
    return part_list;

err_free_list:
    Py_CLEAR(part_list);
err_free_tbl_out:
    free(tbl);
    return NULL;
}

static PyObject *part_table_install(PyObject *self, PyObject *args, PyObject *kwds)
{
    struct pb_session *session = (struct pb_session *)self;
    static char *kwlist[] = { "part", "variant", NULL };
    int rc;
    uint8_t *part_uu = NULL;
    size_t part_uu_len = 0;
    unsigned int variant = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "y#|I", kwlist, &part_uu, &part_uu_len, &variant)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_partition_install_table(session->ctx, part_uu, variant);
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }
    Py_RETURN_NONE;
}

static PyObject *part_write(PyObject *self, PyObject *args, PyObject *kwds)
{
    struct pb_session *session = (struct pb_session *)self;
    static char *kwlist[] = { "file", "uuid", NULL };
    PyObject *file = NULL;
    int file_fd = -1;
    uint8_t *part_uu = NULL;
    size_t part_uu_len = 0;
    int rc;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oy#", kwlist, &file, &part_uu, &part_uu_len)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    file_fd = PyObject_AsFileDescriptor(file);
    if (file_fd == -1) {
        PyErr_SetString(PyExc_TypeError, "Invalid file descriptor");
        return NULL;
    }

    rc = pb_api_partition_write(session->ctx, file_fd, part_uu);

    if (rc != 0) {
        return pb_exception_from_rc(rc);
    }

    Py_RETURN_NONE;
}

static PyObject *part_read(PyObject *self, PyObject *args, PyObject *kwds)
{
    struct pb_session *session = (struct pb_session *)self;
    static char *kwlist[] = { "file", "uuid", NULL };
    PyObject *file = NULL;
    int file_fd = -1;
    uint8_t *part_uu = NULL;
    size_t part_uu_len = 0;
    int rc;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oy#", kwlist, &file, &part_uu, &part_uu_len)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    file_fd = PyObject_AsFileDescriptor(file);
    if (file_fd == -1) {
        PyErr_SetString(PyExc_TypeError, "Invalid file descriptor");
        return NULL;
    }

    rc = pb_api_partition_read(session->ctx, file_fd, part_uu);

    if (rc != 0) {
        return pb_exception_from_rc(rc);
    }

    Py_RETURN_NONE;
}

static PyObject *part_erase(PyObject *self, PyObject *args, PyObject *kwds)
{
    struct pb_session *session = (struct pb_session *)self;
    static char *kwlist[] = { "uuid", "start_lba", "count", NULL };
    uint8_t *part_uu = NULL;
    size_t part_uu_len = 0;
    unsigned int start_lba = 0;
    unsigned int block_count = 0;
    int rc;

    if (!PyArg_ParseTupleAndKeywords(
            args, kwds, "y#II", kwlist, &part_uu, &part_uu_len, &start_lba, &block_count)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_partition_erase(session->ctx, part_uu, start_lba, block_count);

    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }

    Py_RETURN_NONE;
}

static PyObject *part_verify(PyObject *self, PyObject *args, PyObject *kwds)
{
    struct pb_session *session = (struct pb_session *)self;
    static char *kwlist[] = { "uuid", "sha256_digest", "data_length", "bpak", NULL };
    uint8_t *uu_part;
    size_t uu_part_len = 0;
    uint8_t *sha256_digest = NULL;
    size_t sha256_digest_len = 0;
    int bpak_file = 0;
    unsigned int data_length = 0;
    int rc;

    if (!PyArg_ParseTupleAndKeywords(args,
                                     kwds,
                                     "y#y#I|p",
                                     kwlist,
                                     &uu_part,
                                     &uu_part_len,
                                     &sha256_digest,
                                     &sha256_digest_len,
                                     &data_length,
                                     &bpak_file)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_partition_verify(session->ctx, uu_part, sha256_digest, data_length, bpak_file);

    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }

    Py_RETURN_NONE;
}

static PyObject *boot_set_boot_part(PyObject *self, PyObject *args, PyObject *kwds)
{
    struct pb_session *session = (struct pb_session *)self;
    static char *kwlist[] = { "uuid", NULL };
    uint8_t *boot_uu = NULL;
    size_t boot_uu_len = 0;
    int rc;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "y#", kwlist, &boot_uu, &boot_uu_len)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_boot_activate(session->ctx, boot_uu);

    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }

    Py_RETURN_NONE;
}

static PyObject *boot_bpak(PyObject *self, PyObject *args, PyObject *kwds)
{
    struct pb_session *session = (struct pb_session *)self;
    static char *kwlist[] = { "bpak_data", "pretend_uuid", "verbose", NULL };
    uint8_t *bpak_data = NULL;
    size_t bpak_data_len = 0;
    uint8_t *pretend_uu = NULL;
    size_t pretend_uu_len = 0;
    int verbose_boot = 0;
    int rc;

    if (!PyArg_ParseTupleAndKeywords(args,
                                     kwds,
                                     "y#y#I",
                                     kwlist,
                                     &bpak_data,
                                     &bpak_data_len,
                                     &pretend_uu,
                                     &pretend_uu_len,
                                     &verbose_boot)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }
    rc = pb_api_boot_bpak(session->ctx, bpak_data, pretend_uu, verbose_boot);

    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }

    Py_RETURN_NONE;
}

static PyObject *boot_partition(PyObject *self, PyObject *args, PyObject *kwds)
{
    struct pb_session *session = (struct pb_session *)self;
    static char *kwlist[] = { "uuid", "verbose_boot", NULL };
    uint8_t *boot_uu = NULL;
    size_t boot_uu_len = 0;
    int verbose_boot = 0;
    int rc;

    if (!PyArg_ParseTupleAndKeywords(
            args, kwds, "y#|i", kwlist, &boot_uu, &boot_uu_len, &verbose_boot)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_boot_part(session->ctx, boot_uu, verbose_boot);
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }

    pb_api_free_context(session->ctx);
    session->ctx = NULL;

    Py_RETURN_NONE;
}

static PyObject *boot_status(PyObject *self, PyObject *Py_UNUSED(args))
{
    struct pb_session *session = (struct pb_session *)self;
    char status_msg[1024];
    uint8_t boot_uu[16];
    int rc;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_boot_status(session->ctx, boot_uu, status_msg, sizeof(status_msg));
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }

    PyObject *result = PyTuple_New(2);
    PyTuple_SetItem(result, 0, Py_BuildValue("y#", boot_uu, 16));
    PyTuple_SetItem(result, 1, Py_BuildValue("s", status_msg));

    return result;
}

static PyObject *board_read_status(PyObject *self, PyObject *Py_UNUSED(args))
{
    struct pb_session *session = (struct pb_session *)self;
    int rc;
    char status_bfr[1024] = { 0 };

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    rc = pb_api_board_status(session->ctx, status_bfr, sizeof(status_bfr));

    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }

    return Py_BuildValue("s", status_bfr);
}

static PyObject *board_run_command(PyObject *self, PyObject *args, PyObject *kwds)
{
    struct pb_session *session = (struct pb_session *)self;
    static char *kwlist[] = { "cmd", "args", NULL };
    unsigned int cmd;
    uint8_t *cmd_args = NULL;
    size_t cmd_args_len = 0;
    char response[4096];
    size_t response_size = sizeof(response);
    int rc;

    /* Allow passing None for args */
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|y#", kwlist, &cmd, &cmd_args, &cmd_args_len)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    memset(response, 0, sizeof(response));

    rc = pb_api_board_command(session->ctx, cmd, cmd_args, cmd_args_len, response, &response_size);
    if (rc != PB_RESULT_OK) {
        return pb_exception_from_rc(rc);
    }

    return Py_BuildValue("y#", response, response_size);
}

static PyMethodDef PbSession_methods[] = {
    { "close",
      session_close,
      METH_NOARGS,
      "Close the current session (invalidates the session object)" },
    /* Password API */
    {
        "authenticate",
        (PyCFunction)(void (*)(void))authenticate,
        METH_VARARGS | METH_KEYWORDS,
        "Authenticate towards punchboot",
    },
    {
        "auth_set_password",
        (PyCFunction)(void (*)(void))auth_set_password,
        METH_VARARGS | METH_KEYWORDS,
        "Write password",
    },
    {
        "authenticate_dsa_token",
        (PyCFunction)(void (*)(void))authenticate_dsa_token,
        METH_VARARGS | METH_KEYWORDS,
        "Authenticate towards punchboot",
    },
    /* Device API */
    {
        "device_reset",
        device_reset,
        METH_NOARGS,
        "Resets Device",
    },
    {
        "device_get_punchboot_version",
        device_get_punchboot_version,
        METH_NOARGS,
        "Shows punchboot version on device",
    },
    {
        "device_get_uuid",
        device_get_uuid,
        METH_NOARGS,
        "Shows device UUID",
    },
    {
        "device_get_boardname",
        device_get_boardname,
        METH_NOARGS,
        "Shows device board name",
    },
    /* SLC API */
    {
        "slc_get_lifecycle",
        slc_get_lifecycle,
        METH_NOARGS,
        "Reads SLC configuration",
    },
    {
        "slc_set_configuration",
        slc_set_configuration,
        METH_NOARGS,
        "Set SLC configuration, including burning fuses.",
    },
    {
        "slc_set_configuration_lock",
        slc_set_configuration_locked,
        METH_NOARGS,
        "Lock SLC configuration",
    },
    {
        "slc_set_end_of_life",
        slc_set_end_of_life,
        METH_NOARGS,
        "Set SLC to end of life",
    },
    {
        "slc_revoke_key",
        (PyCFunction)(void (*)(void))slc_revoke_key,
        METH_VARARGS | METH_KEYWORDS,
        "Revoke secure boot keys",
    },
    {
        "slc_get_active_keys",
        slc_get_active_keys,
        METH_NOARGS,
        "Reads active secure boot Key IDs",
    },
    {
        "slc_get_revoked_keys",
        slc_get_revoked_keys,
        METH_NOARGS,
        "Reads revoked secure boot Key IDs",
    },
    /* Part API */
    {
        "part_get_partitions",
        part_get_partitions,
        METH_NOARGS,
        "Return available partitions",
    },
    {
        "part_table_install",
        (PyCFunction)(void (*)(void))part_table_install,
        METH_VARARGS | METH_KEYWORDS,
        "Install partition table",
    },
    {
        "part_write",
        (PyCFunction)(void (*)(void))part_write,
        METH_VARARGS | METH_KEYWORDS,
        "Write file to partition",
    },
    {
        "part_read",
        (PyCFunction)(void (*)(void))part_read,
        METH_VARARGS | METH_KEYWORDS,
        "Write file to partition",
    },
    {
        "part_verify",
        (PyCFunction)(void (*)(void))part_verify,
        METH_VARARGS | METH_KEYWORDS,
        "Verify that partition is flashed with specified file",
    },
    {
        "part_erase",
        (PyCFunction)(void (*)(void))part_erase,
        METH_VARARGS | METH_KEYWORDS,
        "Erase the contents of a partition",
    },
    /* Boot API */
    {
        "boot_set_boot_part",
        (PyCFunction)(void (*)(void))boot_set_boot_part,
        METH_VARARGS | METH_KEYWORDS,
        "Set active boot partition",
    },
    {
        "boot_partition",
        (PyCFunction)(void (*)(void))boot_partition,
        METH_VARARGS | METH_KEYWORDS,
        "Boot partition",
    },
    {
        "boot_bpak",
        (PyCFunction)(void (*)(void))boot_bpak,
        METH_VARARGS | METH_KEYWORDS,
        "Boot partition",
    },
    {
        "boot_status",
        boot_status,
        METH_NOARGS,
        "Read board status",
    },
    /* Board API */
    {
        "board_run_command",
        (PyCFunction)(void (*)(void))board_run_command,
        METH_VARARGS | METH_KEYWORDS,
        "Run board specific command",
    },
    {
        "board_read_status",
        board_read_status,
        METH_NOARGS,
        "Read board status",
    },
    { NULL },
};

static PyTypeObject PbSession = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "punchboot.Session",
    .tp_doc = PyDoc_STR("Punchboot session object towards one board"),
    .tp_basicsize = sizeof(struct pb_session),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc)PbSession_init,
    .tp_dealloc = (destructor)PbSession_dealloc,
    .tp_methods = PbSession_methods,
};

static PyObject *wait_for_device(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = { "timeout", NULL };
    int64_t timeout = -1;
    struct pb_context *ctx;
    uint8_t uuid[16];
    int rc;
    (void)self;

    /* Allow passing None for args */
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|k", kwlist, &timeout)) {
        return NULL;
    }

    while (true) {
        rc = init_transport(NULL, NULL, &ctx);
        if (rc != PB_RESULT_OK) {
            PyErr_SetString(PyExc_TimeoutError, "No device found");
            return NULL;
        }

        rc = get_uuid(ctx, uuid);
        pb_api_free_context(ctx);
        if (rc != PB_RESULT_OK) {
            if (timeout > 0) {
                sleep(1);
                timeout--;
            } else {
                PyErr_SetString(PyExc_TimeoutError, "No device found");
                return NULL;
            }
        } else {
            break;
        }
    };

    Py_RETURN_NONE;
}

static void add_list_entry(const char *device_uuid, void *priv)
{
    PyObject *list = (PyObject *)priv;
    PyList_Append(list, Py_BuildValue("s", device_uuid));
}

static PyObject *list_usb_devices(PyObject *Py_UNUSED(self), PyObject *Py_UNUSED(args))
{
    int rc = 0;
    PyObject *result = NULL;
    struct pb_context *local_ctx = NULL;

    result = PyList_New(0);

    if (!result) {
        return PyErr_NoMemory();
    }

    rc = pb_api_create_context(&local_ctx, log_cb);
    if (rc != PB_RESULT_OK) {
        return result;
    }

    rc = pb_usb_transport_init(local_ctx, NULL);
    if (rc != PB_RESULT_OK) {
        return result;
    }

    rc = pb_api_list_devices(local_ctx, add_list_entry, (void *)result);
    if (rc != PB_RESULT_OK) {
        return result;
    }

    pb_api_free_context(local_ctx);

    return result;
}

static PyMethodDef Punchboot_methods[] = {
    {
        "wait_for_device",
        (PyCFunction)(void (*)(void))wait_for_device,
        METH_VARARGS | METH_KEYWORDS,
        "Wait for a device, optional timeout in seconds",
    },
    {
        "list_usb_devices",
        list_usb_devices,
        METH_NOARGS,
        "Find all punchboot USB devices",
    },
    { NULL, NULL, 0, NULL } /* Sentinel */
};

static struct PyModuleDef Punchboot = {
    PyModuleDef_HEAD_INIT, .m_name = "_punchboot",         .m_doc = NULL,
    .m_size = -1,          .m_methods = Punchboot_methods,
};

PyMODINIT_FUNC PyInit__punchboot(void)
{
    if (PyType_Ready(&PbSession) < 0) {
        return NULL;
    }

    PyObject *mod = PyModule_Create(&Punchboot);
    if (mod == NULL) {
        return NULL;
    }

    Py_INCREF(&PbSession);
    if (PyModule_AddObject(mod, "Session", (PyObject *)&PbSession) < 0) {
        Py_DECREF(&PbSession);
        Py_DECREF(mod);
        return NULL;
    }

    if (pb_exceptions_init(mod) != 0) {
        Py_DECREF(&PbSession);
        Py_DECREF(mod);
        return NULL;
    }

    return mod;
}
