
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pb/pb.h>
#include <pb/recovery.h>
#include <plat/test/socket_proto.h>
#include "transport.h"

static int fd;
static struct sockaddr_un addr;

int transport_init(uint8_t *usb_path, uint8_t usb_path_count)
{
    UNUSED(usb_path);
    UNUSED(usb_path_count);

    if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket error");
        return -1;
    }

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/pb_sock", sizeof(addr.sun_path)-1);
    bind(fd, (struct sockaddr*)&addr, sizeof(addr));

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect error");
        return -1;
    }

    return 0;
}

uint32_t pb_read_result_code(void)
{
    uint32_t result_code = PB_ERR;

    if (pb_read((uint8_t *) &result_code, sizeof(uint32_t)) != PB_OK)
        result_code = PB_ERR;

    return result_code;
}

int pb_write(uint32_t cmd, uint32_t arg0,
                           uint32_t arg1,
                           uint32_t arg2,
                           uint32_t arg3,
                           uint8_t *bfr, int sz)
{
    size_t tx_bytes;
    struct pb_socket_header hdr;
    struct pb_cmd_header cmd_hdr;
    uint8_t status;
    uint32_t result_code = PB_OK;

    memset(&cmd_hdr, 0, sizeof(struct pb_cmd_header));

    cmd_hdr.cmd = cmd;
    cmd_hdr.size = sz;
    cmd_hdr.arg0 = arg0;
    cmd_hdr.arg1 = arg1;
    cmd_hdr.arg2 = arg2;
    cmd_hdr.arg3 = arg3;

    hdr.ep = 4;
    hdr.sz = sz + sizeof(struct pb_cmd_header);

    tx_bytes = write(fd, &hdr, sizeof(struct pb_socket_header));

    if (tx_bytes != sizeof(struct pb_socket_header))
    return -1;

    if (read(fd, &status, 1) != 1)
    {
       return PB_ERR;
    }

    tx_bytes = write(fd, &cmd_hdr, sizeof(struct pb_cmd_header));

    // printf("cmd: tx_bytes = %li\n",tx_bytes);

    if (tx_bytes != sizeof(struct pb_cmd_header))
        return -1;

    if (read(fd, &status, 1) != 1)
        return PB_ERR;

    if (bfr && sz)
    {
        tx_bytes = write(fd, bfr, sz);

        if (tx_bytes != sz)
            return -1;

        if (read(fd, &status, 1) != 1)
            return PB_ERR;
    }

    return result_code;
}

int pb_read(uint8_t *bfr, int sz)
{
    size_t rx_bytes;
    uint32_t remaining = sz;
    uint32_t read_count = 0;
    uint32_t chunk;
    uint32_t count = 0;

    while (remaining)
    {
        if (remaining > 4096)
        {
            chunk = 4096;
        }
        else
        {
            chunk = remaining;
        }

        rx_bytes = read(fd, &bfr[read_count], chunk);
        count++;
        remaining -= rx_bytes;
        read_count += rx_bytes;
    }

    if (read_count != sz)
        return -1;

    return 0;
}

int pb_write_bulk(uint8_t *bfr, int sz, int *sz_tx)
{
    uint32_t remaining = sz;
    uint32_t chunk = 0;
    uint32_t pos = 0;

    if (bfr && sz)
    {
        while (remaining)
        {
            if (remaining > 4096)
            {
                chunk = 4096;
            }
            else
            {
                chunk = remaining;
            }

            *sz_tx = write(fd, &bfr[pos], chunk);

            if (*sz_tx != chunk) {
                printf("Error sending data\n");
                return -1;
            }
            remaining -= chunk;
            pos += chunk;
        }
    }

    return 0;
}

void transport_exit(void)
{
    close(fd);
}
