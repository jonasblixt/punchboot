
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pb/recovery.h>
#include <plat/test/socket_proto.h>
#include "transport.h"

static int fd;
static struct sockaddr_un addr;

int transport_init(void)
{

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

int pb_write(uint32_t cmd, uint8_t *bfr, int sz)
{
	size_t tx_bytes;
	struct pb_socket_header hdr;
    struct pb_cmd_header cmd_hdr;
    uint8_t status;

    cmd_hdr.cmd = cmd;
    cmd_hdr.size = sz;

	hdr.ep = 2;
	hdr.sz = sz + sizeof(struct pb_cmd_header);

	tx_bytes = write(fd, &hdr, sizeof(struct pb_socket_header));

	//printf("hdr: tx_bytes = %li\n",tx_bytes);

	if (tx_bytes != sizeof(struct pb_socket_header))
		return -1;

    read(fd, &status, 1);

	tx_bytes = write(fd, &cmd_hdr, sizeof(struct pb_cmd_header));

	//printf("cmd: tx_bytes = %li\n",tx_bytes);

	if (tx_bytes != sizeof(struct pb_cmd_header))
		return -1;

    read(fd, &status, 1);

    if (bfr && sz) 
    { 
        tx_bytes = write(fd, bfr, sz);

        if (tx_bytes != sz)
            return -1;

        read(fd, &status, 1);
    }
	return 0;
}

int pb_read(uint8_t *bfr, int sz)
{
	size_t rx_bytes;

	rx_bytes = read(fd, bfr, sz);

    //printf ("Read %lu bytes (of %i)\n",rx_bytes,sz);
	if (rx_bytes != sz)
		return -1;

	return 0;
}

int pb_write_bulk(uint8_t *bfr, int sz, int *sz_tx)
{
    uint8_t status = 0;

    if (bfr && sz) 
    {
        *sz_tx = write(fd, bfr, sz);

        if (*sz_tx != sz) {
            printf ("Error sending data\n");
            return -1;
        }

        read(fd, &status, 1);
    }
	return 0;
}

void transport_exit(void)
{
}
