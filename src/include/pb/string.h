/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __STRING_H__
#define __STRING_H__

#include <pb.h>

void * memcpy (void *dest, const void *src, size_t n);
int strcmp(const char *s1, const char *s2);
void * memset (void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memmove(void *dest, const void *src, size_t n);
size_t strlen(const char *s);
void *memchr(const void *s, int c, size_t n);

#endif
