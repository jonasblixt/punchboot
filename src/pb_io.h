#ifndef __PB_IO_H_
#define __PB_IO_H_

#include <pb_types.h>

static inline u32 pb_readl(void * addr)
{
	return *((u32 *)addr);
}

static inline void pb_writel(u32 data, void * addr)
{
	*((u32 *)addr) = data;
}

static inline u16 pb_readw(void * addr)
{
	return *((u16 *)addr);
}

static inline void pb_writew(u16 data, void * addr)
{
	*((u16 *)addr) = data;
}

static inline u8 pb_readb(void * addr)
{
	return *((u8 *)addr);
}

static inline void pb_writeb(u8 data, void * addr)
{
	*((u8 *)addr) = data;
}

#endif

