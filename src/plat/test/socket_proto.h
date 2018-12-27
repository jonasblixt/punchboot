#ifndef __PB_SOCKET_PROTO_H__
#define __PB_SOCKET_PROTO_H__

#include <stdint.h>

struct pb_socket_header
{
    uint32_t ep;
    uint32_t sz;
};

#endif
