#ifndef __STRING_H__
#define __STRING_H__

#include <pb_types.h>


void * memcpy (void *dest, const void *src, size_t n);
void * memset (void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
 
#endif
