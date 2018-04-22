#ifndef __PB_TYPES_H__
#define __PB_TYPES_H__

#define NULL (void *) 0
#define true 1
#define false 0

#define PB_OK   0
#define PB_ERR  1

typedef char s8;
typedef short s16;
typedef int s32;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned int size_t;
typedef unsigned int bool;
typedef unsigned long long u64;

typedef u32 __iomem;

#define __no_bss __attribute__((section (".bigbuffer")))
#define __a4k  __attribute__ ((aligned(4096)))

#endif


