#ifndef __PB_IO_H_
#define __PB_IO_H_

#include <pb_types.h>

static inline u32 pb_readl(__iomem addr)
{
	return *(( u32 *)addr);
}

static inline void pb_writel(u32 data, __iomem addr)
{
	*(( u32 *)addr) = data;
}


static inline void pb_write(u16 data, __iomem addr)
{
	*(( u16 *)addr) = data;
}

#endif

