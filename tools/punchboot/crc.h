#ifndef __CRC_H__
#define __CRC_H__

#include <sys/types.h>

u_int32_t crc32(u_int32_t crc, const void *buf, size_t size);


#endif
