/*
 * Copyright (c) 2012-2017 Roberto E. Vargas Caballero
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Portions copyright (c) 2018, ARM Limited and Contributors.
 * All rights reserved.
 */

#ifndef INCLUDE_PB_LIBC_STRING_H_
#define INCLUDE_PB_LIBC_STRING_H_

#include <string_.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

void *memcpy(void *dst, const void *src, size_t len);
void *memmove(void *dst, const void *src, size_t len);
int memcmp(const void *s1, const void *s2, size_t len);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
void *memchr(const void *src, int c, size_t len);
char *strchr(const char *s, int c);
void *memset(void *dst, int val, size_t count);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
char *strrchr(const char *p, int ch);
size_t strlcpy(char *dst, const char *src, size_t dsize);
unsigned long strtoul(const char *nptr, char **endptr, int base);
char *strncpy(char *dest, const char *src, size_t n);
size_t strspn(const char *s, const char *accept);
char *strcpy(char *dest, const char *src);
size_t strcspn(const char *s, const char *reject);

#endif // INCLUDE_PB_LIBC_STRING_H_
