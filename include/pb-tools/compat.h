#ifndef INCLUDE_PB_TOOLS_COMPAT_H

#ifdef __GNUC__
#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK(__Declaration__) __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
#include <basetsd.h>
typedef SSIZE_T ssize_t;
#endif // _SSIZE_T_DEFINED
#define sleep(x) Sleep(x * 1000)
#endif // _MSC_VER

#endif // INCLUDE_PB_TOOLS_COMPAT_H
