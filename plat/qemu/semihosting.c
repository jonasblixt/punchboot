/*
 * Copyright (c) 2013-2014, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <plat/test/semihosting.h>
#include <string.h>

#ifndef SEMIHOSTING_SUPPORTED
#define SEMIHOSTING_SUPPORTED  1
#endif

long semihosting_call(unsigned long operation,
            void *system_block_address);

typedef struct {
    const char *file_name;
    unsigned long mode;
    size_t name_length;
} smh_file_open_block_t;

typedef struct {
    long handle;
    uintptr_t buffer;
    size_t length;
} smh_file_read_write_block_t;

typedef struct {
    long handle;
    long location;
} smh_file_seek_block_t;

typedef struct {
    char *command_line;
    size_t command_length;
} smh_system_block_t;

long semihosting_connection_supported(void)
{
    return SEMIHOSTING_SUPPORTED;
}

long semihosting_file_open(const char *file_name, size_t mode)
{
    smh_file_open_block_t open_block;

    open_block.file_name = file_name;
    open_block.mode = mode;
    open_block.name_length = strlen(file_name);

    return semihosting_call(SEMIHOSTING_SYS_OPEN,
                (void *) &open_block);
}

long semihosting_file_seek(long file_handle, long offset)
{
    smh_file_seek_block_t seek_block;
    long result;

    seek_block.handle = file_handle;
    seek_block.location = offset;

    result = semihosting_call(SEMIHOSTING_SYS_SEEK,
                  (void *) &seek_block);

    if (result)
        result = semihosting_call(SEMIHOSTING_SYS_ERRNO, 0);

    return result;
}

long semihosting_file_read(long file_handle, size_t *length, uintptr_t buffer)
{
    smh_file_read_write_block_t read_block;
    long result = -1;

    if ((length == NULL) || (buffer == (uintptr_t)NULL))
        return result;

    read_block.handle = file_handle;
    read_block.buffer = buffer;
    read_block.length = *length;

    result = semihosting_call(SEMIHOSTING_SYS_READ,
                  (void *) &read_block);

    if (result == (long) *length) {
        return -1;
    } else if (result < (long) *length) {
        *length -= result;
        return 0;
    } else {
        return result;
    }
}

long semihosting_file_write(long file_handle,
                size_t *length,
                const uintptr_t buffer)
{
    smh_file_read_write_block_t write_block;
    long result = -1;

    if ((length == NULL) || (buffer == (uintptr_t)NULL))
        return -1;

    write_block.handle = file_handle;
    write_block.buffer = (uintptr_t)buffer; /* cast away const */
    write_block.length = *length;

    result = semihosting_call(SEMIHOSTING_SYS_WRITE,
                   (void *) &write_block);

    *length = result;

    return (result == 0) ? 0 : -1;
}

long semihosting_file_close(long file_handle)
{
    return semihosting_call(SEMIHOSTING_SYS_CLOSE,
                (void *) &file_handle);
}

long semihosting_file_length(long file_handle)
{
    return semihosting_call(SEMIHOSTING_SYS_FLEN,
                (void *) &file_handle);
}

char semihosting_read_char(void)
{
    return semihosting_call(SEMIHOSTING_SYS_READC, NULL);
}

void semihosting_write_char(char character)
{
    semihosting_call(SEMIHOSTING_SYS_WRITEC, (void *) &character);
}

void semihosting_write_string(char *string)
{
    semihosting_call(SEMIHOSTING_SYS_WRITE0, (void *) string);
}

long semihosting_system(char *command_line)
{
    smh_system_block_t system_block;

    system_block.command_line = command_line;
    system_block.command_length = strlen(command_line);

    return semihosting_call(SEMIHOSTING_SYS_SYSTEM,
                (void *) &system_block);
}

long semihosting_get_flen(const char *file_name)
{
    long file_handle;
    long length;


    file_handle = semihosting_file_open(file_name, FOPEN_MODE_RB);
    if (file_handle == -1)
        return file_handle;

    /* Find the length of the file */
    length = semihosting_file_length(file_handle);

    return semihosting_file_close(file_handle) ? -1 : length;
}

void semihosting_sys_exit(uint8_t error)
{
    if (error == 0)
        semihosting_call(SEMIHOSTING_SYS_EXIT, (void *) 0x00020026);
    else
        semihosting_call(SEMIHOSTING_SYS_EXIT, 0);
}

long semihosting_download_file(const char *file_name,
                  size_t buf_size,
                  uintptr_t buf)
{
    long ret = -1;
    size_t length;
    long file_handle;

    /* Null pointer check */
    if (!buf)
        return ret;


    file_handle = semihosting_file_open(file_name, FOPEN_MODE_RB);
    if (file_handle == -1)
        return ret;

    /* Find the actual length of the file */

    long status = semihosting_file_length(file_handle);
    if (status == -1)
        goto semihosting_fail;

    length = (size_t) status;
    /* Signal error if we do not have enough space for the file */
    if (length > buf_size)
        goto semihosting_fail;

    /*
     * A successful read will return 0 in which case we pass back
     * the actual number of bytes read. Else we pass a negative
     * value indicating an error.
     */
    ret = semihosting_file_read(file_handle, &length, buf);
    if (ret)
        goto semihosting_fail;
    else
        ret = length;

semihosting_fail:
    semihosting_file_close(file_handle);
    return ret;
}
