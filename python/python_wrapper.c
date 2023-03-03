#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <Python.h>
#include <bpak/bpak.h>
#include <bpak/utils.h>
#include <pb-tools/api.h>
#include <pb-tools/error.h>
#include <pb-tools/usb.h>
#include <pb-tools/wire.h>
#include <uuid/uuid.h>
#include "crc.h"
#include "sha256.h"

#if PY_MAJOR_VERSION < 3
#error "Only python3 supported"
#endif

static PyObject* pb_base_exception = NULL;

static const char *pb_error_to_python_exception_name(int result)
{
#define PRE "punchboot."
    switch(result)
    {
        case -PB_RESULT_ERROR:
            return PRE"Error";
        case -PB_RESULT_AUTHENTICATION_FAILED:
            return PRE"AuthenticationFailed";
        case -PB_RESULT_NOT_AUTHENTICATED:
            return PRE"NotAuthenticated";
        case -PB_RESULT_NOT_SUPPORTED:
            return PRE"NotSupported";
        case -PB_RESULT_INVALID_ARGUMENT:
            return PRE"InvalidArgument";
        case -PB_RESULT_INVALID_COMMAND:
            return PRE"InvalidCommand";
        case -PB_RESULT_PART_VERIFY_FAILED:
            return PRE"PartitionVerifyFailed";
        case -PB_RESULT_PART_NOT_BOOTABLE:
            return PRE"PartitionNotBootable";
        case -PB_RESULT_NO_MEMORY:
            return PRE"NoMemory";
        case -PB_RESULT_TRANSFER_ERROR:
            return PRE"TransferError";
        case -PB_RESULT_NOT_FOUND:
            return PRE"NotFound";
        case -PB_RESULT_STREAM_NOT_INITIALIZED:
            return PRE"StreamNotInitialized";
        case -PB_RESULT_TIMEOUT:
            return PRE"TimeoutError";
        case -PB_RESULT_KEY_REVOKED:
            return PRE"KeyRevoked";
        case -PB_RESULT_SIGNATURE_ERROR:
            return PRE"SignatureError";
        case -PB_RESULT_MEM_ERROR:
            return PRE"MemoryError";
        case -PB_RESULT_IO_ERROR:
            return PRE"IOError";
        default:
            return "";
    }
#undef PRE
}

static PyObject* PbErr_FromErrorCode(int err, const char* msg)
{
    PyObject* exc =
        PyErr_NewException(pb_error_to_python_exception_name(err), pb_base_exception, NULL);
    if (msg) {
        PyErr_SetString(exc, msg);
    } else {
        PyErr_SetString(exc, "");
    }
    return NULL;
}

static int init_transport(const char* uuid, struct pb_context** ctx)
{
    int ret = 0;
    struct pb_context* local_ctx = NULL;

    ret = pb_api_create_context(&local_ctx, NULL);
    if (ret != PB_RESULT_OK) {
        return ret;
    }
    ret = pb_usb_transport_init(local_ctx, uuid);
    if (ret != PB_RESULT_OK) {
        goto err_free_ctx;
    }
    ret = local_ctx->connect(local_ctx);
    if (ret != PB_RESULT_OK) {
        goto err_free_ctx;
    }
    *ctx = local_ctx;
    return PB_RESULT_OK;

err_free_ctx:
    pb_api_free_context(local_ctx);
    return ret;
}

struct pb_session {
    PyObject_HEAD
    struct pb_context *ctx;
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
    static char *kwlist[] = {"uuid", NULL};
    const char* uuid = NULL;
    int ret;

    /* If 'uuid' is not provided (== None), this will pick any valid device */
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|z", kwlist, &uuid)) {
        return -1;
    }

    // TODO: If init is called again, do we reset the session or return an error?

    ret = init_transport(uuid, &self->ctx);
    if (ret != PB_RESULT_OK) {
        PbErr_FromErrorCode(ret, "Failed to connect to device");
        return -1;
    }

    return 0;
}

static void PbSession_dealloc(struct pb_session *self)
{
    if (self->ctx) {
        pb_api_free_context(self->ctx);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* authenticate(PyObject* self, PyObject* args, PyObject* kwds)
{
    struct pb_session* session = (struct pb_session*)self;
    static char *kwlist[] = {"password", NULL};
    const char* password;
    int ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &password)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    ret = pb_api_authenticate_password(session->ctx, (uint8_t*)password, strlen(password));
    if (ret != 0) {
        return PbErr_FromErrorCode(ret, "Authentication failed");
    }
    Py_RETURN_NONE;
}

static PyObject* device_reset(PyObject* self, PyObject* Py_UNUSED(args))
{
    struct pb_session* session = (struct pb_session*)self;
    int ret;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    ret = pb_api_device_reset(session->ctx);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Could not reset device");
    }
    pb_api_free_context(session->ctx);
    session->ctx = NULL;

    Py_RETURN_NONE;
}

static PyObject* device_get_punchboot_version(PyObject* self, PyObject* Py_UNUSED(args))
{
    struct pb_session* session = (struct pb_session*)self;
    char version[64];
    int ret;

    memset(version, 0, sizeof(version));

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    ret = pb_api_bootloader_version(session->ctx, version, sizeof(version));
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Failed to read punchboot version");
    }

    return Py_BuildValue("s", version);
}

static int get_uuid(struct pb_context* ctx, uuid_t* uuid)
{
    uuid_t device_uu;
    char board_name[17];
    int ret;

    /* Arguments to this API are not optional */
    ret = pb_api_device_read_identifier(ctx, device_uu, sizeof(device_uu),
                                           board_name, sizeof(board_name));
    if (ret != PB_RESULT_OK) {
        return ret;
    }

    memcpy(uuid, &device_uu, sizeof(device_uu));

    return PB_RESULT_OK;
}

static PyObject* device_get_uuid(PyObject* self, PyObject* Py_UNUSED(args))
{
    struct pb_session* session = (struct pb_session*)self;
    uuid_t device_uu;
    char uuid_str[37];
    int ret;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    /* Arguments to this API are not optional */
    ret = get_uuid(session->ctx, &device_uu);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Failed to read device UUID");
    }

    uuid_unparse(device_uu, uuid_str);

    return Py_BuildValue("s", uuid_str);
}

static PyObject* device_get_boardname(PyObject* self, PyObject* Py_UNUSED(args))
{
    struct pb_session* session = (struct pb_session*)self;
    uuid_t device_uu;
    char board_name[17];
    int ret = 0;

    memset(board_name, 0, sizeof(board_name));

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    /* Arguments to this API are not optional */
    ret = pb_api_device_read_identifier(session->ctx, device_uu, sizeof(device_uu),
                                           board_name, sizeof(board_name));
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Failed to read boardname");
    }

    return Py_BuildValue("s", board_name);
}

static PyObject* slc_set_configuration(PyObject* self, PyObject* Py_UNUSED(args))
{
    struct pb_session* session = (struct pb_session*)self;
    int ret;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    ret = pb_api_slc_set_configuration(session->ctx);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Could not set SLC configuration");
    }
    Py_RETURN_NONE;
}

static PyObject* slc_lock_device(PyObject* self, PyObject* Py_UNUSED(args))
{
    struct pb_session* session = (struct pb_session*)self;
    int ret;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    ret = pb_api_slc_set_configuration_lock(session->ctx);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Coult not lock SLC configuration");
    }
    Py_RETURN_NONE;
}

static PyObject* slc_revoke_key(PyObject* self, PyObject* args, PyObject* kwds)
{
    struct pb_session* session = (struct pb_session*)self;
    static char *kwlist[] = {"key_id", NULL};
    const char* key = NULL;
    char* end;
    uint32_t key_id;

    int ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &key)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    errno = 0;
    key_id = strtoul(key, &end, 16);
    if (key_id == 0 || errno == ERANGE || key != end || *end != '\0') {
        PyErr_SetString(PyExc_ValueError, "Invalid Key ID");
        return NULL;
    }

    ret = pb_api_slc_revoke_key(session->ctx, key_id);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Failed to revoke key");
    }
    Py_RETURN_NONE;
}


static PyObject* slc_get_lifecycle(PyObject* self, PyObject* Py_UNUSED(args))
{
    struct pb_session* session = (struct pb_session*)self;
    int ret;

    uint8_t slc;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    ret = pb_api_slc_read(session->ctx, &slc, NULL, NULL);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Failed to read SLC");
    }
    return Py_BuildValue("i", (int)slc);
}

static PyObject* slc_get_active_keys(PyObject* self, PyObject* Py_UNUSED(args))
{
    struct pb_session* session = (struct pb_session*)self;
    int ret;

    /* slc argument is currently not optional to pb_api_slc_read */
    uint8_t slc;
    uint32_t active_keys[16];
    memset(active_keys, 0, sizeof(active_keys));

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    ret = pb_api_slc_read(session->ctx, &slc, (uint8_t *) active_keys, NULL);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Failed to read active keys");
    }

    /* The API doesn't tell us how many keys there are, but we do know that 0 is not a valid ID */
    int key_count;
    for (key_count = 0; active_keys[key_count] != 0; key_count++) {
        ; /* empty on purpose, we're counting keys */
    }
    PyObject* keylist = PyList_New(key_count);
    if (!keylist) {
        return PyErr_NoMemory();
    }

    for (int i = 0; i < key_count; i++) {
        PyList_SET_ITEM(keylist, i, PyUnicode_FromFormat("0x%x", active_keys[i]));
    }

    return keylist;
}

static PyObject* slc_get_revoked_keys(PyObject* self, PyObject* Py_UNUSED(args))
{
    struct pb_session* session = (struct pb_session*)self;
    int ret;

    /* slc argument is currently not optional to pb_api_slc_read */
    uint8_t slc;
    uint32_t revoked_keys[16];
    memset(revoked_keys, 0, sizeof(revoked_keys));

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    ret = pb_api_slc_read(session->ctx, &slc, NULL, (uint8_t *) revoked_keys);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Failed to read revoked keys");
    }

    /* The API doesn't tell us how many keys there are, but we do know that 0 is not a valid ID */
    int key_count;
    for (key_count = 0; revoked_keys[key_count] != 0; key_count++) {
        ; /* empty on purpose, we're counting keys */
    }
    PyObject* keylist = PyList_New(key_count);
    if (!keylist) {
        return PyErr_NoMemory();
    }

    for (int i = 0; i < key_count; i++) {
        PyList_SET_ITEM(keylist, i, PyUnicode_FromFormat("0x%x", revoked_keys[i]));
    }

    return keylist;
}

static int read_part_table(struct pb_context* ctx, struct pb_partition_table_entry** table, int* entries)
{
    struct pb_partition_table_entry *tbl;
    int read_entries = 128;
    int ret;

    tbl = malloc(sizeof(*tbl) * read_entries);
    if (!tbl) {
        return -PB_RESULT_NO_MEMORY;
    }

    ret = pb_api_partition_read_table(ctx, tbl, &read_entries);
    if (ret != PB_RESULT_OK) {
        free(tbl);
        return ret;
    }

    if (read_entries == 0) {
        free(tbl);
        return -PB_RESULT_ERROR;  // TODO: Better error?
    }

    *table = tbl;
    *entries = read_entries;
    return 0;
}

static PyObject* part_list_partitions(PyObject* self, PyObject* Py_UNUSED(args))
{
    struct pb_session* session = (struct pb_session*)self;
    int ret = 0;
    int i;

    PyObject *part_dict = NULL;
    struct pb_partition_table_entry *tbl;
    int entries;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    ret = read_part_table(session->ctx, &tbl, &entries);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Failed to read partition table");
    }

    part_dict = PyDict_New();
    if (!part_dict) {
        PyErr_NoMemory();
        goto err_free_table;
    }

    for (i = 0; i < entries; i++) {
        char uuid_str[37];
        char entryname[10];
        bool use_description = tbl[i].description &&
                               strlen(tbl[i].description) > 0;

        memset(uuid_str, 0, sizeof(uuid_str));
        uuid_unparse(tbl[i].uuid, uuid_str);

        if (!use_description) {
            snprintf(entryname, sizeof(entryname), "Part%d", i );
        }

        ret = PyDict_SetItemString(part_dict,
                                   use_description ? tbl[i].description :
                                                     entryname,
                                   PyUnicode_FromFormat("%s", uuid_str));
        if (ret != 0) {
            goto err_free_dict;
        }
    }

    free(tbl);
    return part_dict;

err_free_dict:
    Py_CLEAR(part_dict);  // TODO: Do we need to de-allocate all the members ourselves?
err_free_table:
    free(tbl);
    return NULL;
}

static PyObject* part_table_install(PyObject* self, PyObject* Py_UNUSED(args))
{
    struct pb_session* session = (struct pb_session*)self;
    int ret;

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    ret = pb_api_partition_install_table(session->ctx);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Failed installing parition table");
    }
    Py_RETURN_NONE;
}

static int stream_data(struct pb_context* ctx, uint8_t buf_id, void* data, uint32_t data_size, uint32_t offset)
{
    int ret = pb_api_stream_prepare_buffer(ctx, buf_id, data, data_size);
    if (ret == PB_RESULT_OK)
    {
        ret = pb_api_stream_write_buffer(ctx, buf_id, offset, data_size);
    }
    return ret;
}

static PyObject* part_resize(PyObject* self, PyObject* args, PyObject* kwds)
{
    struct pb_session* session = (struct pb_session*)self;
    static char *kwlist[] = {"block_count", "uuid", NULL};
    const char* uuid;
    uuid_t uuid_part;
    size_t block_count;
    int ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "ks", kwlist, &block_count, &uuid)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    uuid_parse(uuid, uuid_part);
    ret = pb_api_partition_resize(session->ctx, uuid_part, block_count);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Failed to resize partition");
    }

    Py_RETURN_NONE;
}

static PyObject* part_write(PyObject* self, PyObject* args, PyObject* kwds)
{
    struct pb_session* session = (struct pb_session*)self;
    static char *kwlist[] = {"file", "uuid", NULL};
    PyObject* file = NULL;
    const char* uuid = NULL;
    struct pb_partition_table_entry *tbl;
    int tbl_entries;
    struct pb_device_capabilities caps;
    size_t chunk_size = 0;
    uint8_t *chunk_buffer = NULL;
    uint8_t buffer_id = 0;
    struct bpak_header header;
    size_t part_size = 0;
    uuid_t uuid_part;
    int file_fd;
    int ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Os", kwlist, &file, &uuid)) {
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

    if (lseek(file_fd, 0, SEEK_SET) == (off_t) -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    if (uuid_parse(uuid, uuid_part) != 0) {
        PyErr_SetString(PyExc_TypeError, "Failed to parse UUID");
        return NULL;
    }

    ret = pb_api_device_read_caps(session->ctx, &caps);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Failed to read device caps");
    }

    chunk_size = caps.chunk_transfer_max_bytes;
    chunk_buffer = malloc(chunk_size + 1);
    if (!chunk_buffer) {
        return PyErr_NoMemory();
    }

    ret = read_part_table(session->ctx, &tbl, &tbl_entries);
    if (ret != PB_RESULT_OK) {
        PbErr_FromErrorCode(ret, "Failed to read partition table");
        goto err_free_buf;
    }

    for (int i = 0; i < tbl_entries; i++) {
        if (uuid_compare(tbl[i].uuid, uuid_part) == 0) {
            part_size = (tbl[i].last_block - tbl[i].first_block + 1) * tbl[i].block_size;
        }
    }
    free(tbl);
    if (part_size == 0) {
        PyErr_SetString(PyExc_ValueError, "UUID not found in partition table");
        goto err_free_buf;
    }

    ret = pb_api_stream_init(session->ctx, uuid_part);
    if (ret != PB_RESULT_OK) {
        PbErr_FromErrorCode(ret, "Failed to initialize stream");
        goto err_free_buf;
    }

    size_t offset = 0;
    ssize_t read_bytes = 0;
    bool bpak_file = false;

    read_bytes = read(file_fd, &header, sizeof(header));

    if (read_bytes < 0) {
        return PyErr_SetFromErrno(PyExc_IOError);
    } else if (read_bytes == sizeof(header) &&
               bpak_valid_header(&header) == BPAK_OK) {
        bpak_file = true;
    } else {
        lseek(file_fd, 0, SEEK_SET);
    }

    if (bpak_file) {
        offset = part_size  - sizeof(header);
        ret = stream_data(session->ctx, buffer_id, &header, sizeof(header), offset);

        if (ret != PB_RESULT_OK) {
            PbErr_FromErrorCode(ret, "Failed to write header");
            goto err_free_buf;
        }

        buffer_id = (buffer_id + 1) % caps.stream_no_of_buffers;
        offset = 0;
    }

    while ((read_bytes = read(file_fd, chunk_buffer, chunk_size)) > 0) {
        ret = stream_data(session->ctx, buffer_id, chunk_buffer, read_bytes, offset);
        if (ret != PB_RESULT_OK) {
            PbErr_FromErrorCode(ret, "Failed to write partition");
            break;
        }

        buffer_id = (buffer_id + 1) % caps.stream_no_of_buffers;
        offset += read_bytes;
    }
    if (read_bytes < 0) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    Py_RETURN_NONE;

err_free_buf:
    pb_api_stream_finalize(session->ctx);
    free(chunk_buffer);
    return NULL;
}

static PyObject* part_verify(PyObject*self, PyObject* args, PyObject *kwds)
{
    struct pb_session* session = (struct pb_session*)self;
    static char *kwlist[] = {"file", "uuid", NULL};
    const char* uuid;
    uuid_t uuid_part;
    PyObject* file;
    int file_fd;
    struct bpak_header header;
    mbedtls_sha256_context sha256;
    unsigned char* chunk_buffer;
    unsigned char hash_data[32];
    int ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Os", kwlist, &file, &uuid)) {
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

    if (lseek(file_fd, 0, SEEK_SET) == (off_t) -1) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }

    uuid_parse(uuid, uuid_part);
    mbedtls_sha256_init(&sha256);
    mbedtls_sha256_starts_ret(&sha256, 0);

    size_t total_read = 0;
    ssize_t read_bytes = read(file_fd, &header, sizeof(header));
    if (read_bytes > 0) {
        mbedtls_sha256_update_ret(&sha256, (unsigned char*)&header, read_bytes);
    } else {
        PyErr_SetNone(PyExc_IOError);
        return NULL;
    }
    total_read = read_bytes;

    if (read_bytes != sizeof(header)) {
        memset(&header, 0, sizeof(header));
    }

    chunk_buffer = malloc(128 * 1024);
    if (!chunk_buffer) {
        return PyErr_NoMemory();
    }

    do {
        read_bytes = read(file_fd, chunk_buffer, 128 * 1024);
        if (read_bytes > 0) {
            mbedtls_sha256_update_ret(&sha256, chunk_buffer, read_bytes);
            total_read = read_bytes;
        }
    } while (read_bytes > 0 || (read_bytes == -1 && errno == EINTR));
    free(chunk_buffer);

    if (read_bytes == -1) {
        PyErr_SetNone(PyExc_IOError);
        return NULL;
    }

    mbedtls_sha256_finish_ret(&sha256, hash_data);

    ret = pb_api_partition_verify(session->ctx, uuid_part, hash_data, total_read, bpak_valid_header(&header) == BPAK_OK);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Verification failed");
    }

    Py_RETURN_NONE;
}

static PyObject* boot_set_boot_part(PyObject* self, PyObject* args, PyObject *kwds)
{
    struct pb_session* session = (struct pb_session*)self;
    static char *kwlist[] = {"uuid", NULL};
    const char* uuid;
    uuid_t uuid_boot;
    int ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &uuid)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    if (strcmp(uuid, "none") == 0) {
        memset(uuid_boot, 0, sizeof(uuid_boot));
    } else {
        if (uuid_parse(uuid, uuid_boot) != 0) {
            PyErr_SetString(PyExc_TypeError, "Failed to parse UUID");
            return NULL;
        }
    }

    ret = pb_api_boot_activate(session->ctx, uuid_boot);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Could not set active boot partition");
    }
    Py_RETURN_NONE;
}

static PyObject* boot_partition(PyObject* self, PyObject* args, PyObject* kwds)
{
    struct pb_session* session = (struct pb_session*)self;
    static char *kwlist[] = {"uuid", NULL};
    const char* uuid;
    uuid_t uuid_boot;
    int ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &uuid)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    if (uuid_parse(uuid, uuid_boot) != 0) {
        PyErr_SetString(PyExc_TypeError, "Failed to parse UUID");
        return NULL;
    }

    ret = pb_api_boot_part(session->ctx, uuid_boot, true);
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Could not boot partition");
    }

    pb_api_free_context(session->ctx);
    session->ctx = NULL;

    Py_RETURN_NONE;
}

static PyObject* board_run_command(PyObject *self, PyObject* args, PyObject* kwds)
{
    struct pb_session* session = (struct pb_session*)self;
    // TODO: Invalidates_ctx could be removed if the API could inform the user
    // about the properties of the command that is about to be run.
    static char *kwlist[] = {"cmd", "args", "invalidates_ctx", NULL};
    const char* cmd;
    const char* cmd_args = NULL;
    bool invalidates_ctx = false;
    size_t cmd_args_len = 0;
    int cmd_enc;
    char response[4096];
    int ret;

    /* Allow passing None for args */
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|sp", kwlist, &cmd, &cmd_args, &invalidates_ctx)) {
        return NULL;
    }

    if (validate_pb_session(session) != 0) {
        return NULL;
    }

    if (cmd_args) {
        cmd_args_len = strlen(cmd_args);
    }

    cmd_enc = pb_crc32(0, (unsigned char*)cmd, strlen(cmd));

    ret = pb_api_board_command(session->ctx, cmd_enc, cmd_args, cmd_args_len,
                                response, sizeof(response));
    if (ret != PB_RESULT_OK) {
        return PbErr_FromErrorCode(ret, "Failed to run board command");
    }
    if (invalidates_ctx) {
        pb_api_free_context(session->ctx);
        session->ctx = NULL;
    }

    return Py_BuildValue("s", response);
}

static PyMethodDef PbSession_methods[] = {
    /* Password API */
    {"authenticate", (PyCFunction)(void(*)(void))authenticate, METH_VARARGS | METH_KEYWORDS, "Authenticate towards punhcboot"},
    /* Device API */
    {"device_reset", device_reset, METH_NOARGS, "Resets Device"},
    {"device_get_punchboot_version", device_get_punchboot_version, METH_NOARGS, "Shows punchboot version on device"},
    {"device_get_uuid", device_get_uuid, METH_NOARGS, "Shows device UUID"},
    {"device_get_boardname", device_get_boardname, METH_NOARGS, "Shows device board name"},
    /* SLC API */
    {"slc_set_configuration", slc_set_configuration, METH_NOARGS, "Set SLC configuration, including burning fuses."},
    {"slc_lock_device", slc_lock_device, METH_NOARGS, "Lock SLC configuration"},
    {"slc_revoke_key", (PyCFunction)(void(*)(void))slc_revoke_key, METH_VARARGS | METH_KEYWORDS, "Revoke secure boot keys"},
    {"slc_get_lifecycle", slc_get_lifecycle, METH_NOARGS, "Reads SLC configuration"},
    {"slc_get_active_keys", slc_get_active_keys, METH_NOARGS, "Reads active secure boot Key IDs"},
    {"slc_get_revoked_keys", slc_get_revoked_keys, METH_NOARGS, "Reads revoked secure boot Key IDs"},
    /* Part API */
    {"part_list_partitions", part_list_partitions, METH_NOARGS, "Return the partition table as a dictionary, names as keys"},
    {"part_table_install", part_table_install, METH_NOARGS, "Install partition table"},
    {"part_resize", (PyCFunction)(void(*)(void))part_resize, METH_VARARGS | METH_KEYWORDS, "Resize partition"},
    {"part_write", (PyCFunction)(void(*)(void))part_write, METH_VARARGS | METH_KEYWORDS, "Write file to partition"},
    {"part_verify", (PyCFunction)(void(*)(void))part_verify, METH_VARARGS | METH_KEYWORDS, "Verify that partition is flashed with specified file"},
    /* Boot API */
    {"boot_set_boot_part", (PyCFunction)(void(*)(void))boot_set_boot_part, METH_VARARGS | METH_KEYWORDS, "Set active boot partition"},
    {"boot_partition", (PyCFunction)(void(*)(void))boot_partition, METH_VARARGS | METH_KEYWORDS, "Boot partition"},
    /* Board API */
    {"board_run_command", (PyCFunction)(void(*)(void))board_run_command, METH_VARARGS | METH_KEYWORDS, "Run board specific command"},
    { NULL },
};

static PyTypeObject PbSession = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "punchboot.Session",
    .tp_doc = PyDoc_STR("Punchboot session object towards one board"),
    .tp_basicsize = sizeof(struct pb_session),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc) PbSession_init,
    .tp_dealloc = (destructor) PbSession_dealloc,
    .tp_methods = PbSession_methods,
};

static PyObject* wait_for_device(PyObject* self, PyObject* args, PyObject* kwds)
{
    static char *kwlist[] = {"timeout", NULL};
    int64_t timeout = -1;
    struct pb_context *ctx;
    uuid_t uuid;
    int ret;
    (void) self;

    /* Allow passing None for args */
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|k", kwlist, &timeout)) {
        return NULL;
    }

    while (true) {
        ret = init_transport(NULL, &ctx);
        if (ret != PB_RESULT_OK) {
            return PbErr_FromErrorCode(ret, "Failed to init transport");
        }

        ret = get_uuid(ctx, &uuid);
        pb_api_free_context(ctx);
        if (ret != PB_RESULT_OK) {
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

static PyMethodDef Punchboot_methods[] = {
    {"wait_for_device", (PyCFunction)(void(*)(void))wait_for_device, METH_VARARGS | METH_KEYWORDS, "Wait for a device, optional timeout in seconds"},
//    {"get_device_uuid", get_device_uuid, METH_NOARGS, "Get device UUID."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef Punchboot = {
   PyModuleDef_HEAD_INIT,
   .m_name = "punchboot",
   .m_doc = NULL,
   .m_size = -1,
   .m_methods = Punchboot_methods,
};


PyMODINIT_FUNC PyInit_punchboot(void)
{
    if (PyType_Ready(&PbSession) < 0) {
        return NULL;
    }

    pb_base_exception = PyErr_NewException("punchboot.Error", NULL, NULL);
    if (pb_base_exception == NULL) {
        return NULL;
    }

    PyObject* mod = PyModule_Create(&Punchboot);
    if (mod == NULL) {
        return NULL;
    }

    Py_INCREF(&PbSession);
    if (PyModule_AddObject(mod, "Session", (PyObject*) &PbSession) < 0) {
        Py_DECREF(&PbSession);
        Py_DECREF(mod);
        return NULL;
    }


    if (PyModule_AddObject(mod, "Error", (PyObject*) pb_base_exception) < 0) {
        Py_DECREF(&PbSession);
        Py_DECREF(mod);
        return NULL;
    }

    return mod;
}
