/*
 * Copyright (c) 2017-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <pb.h>
#include <stdarg.h>


static void string_print(char **s, size_t n, size_t *chars_printed,
			 const char *str)
{
	while (*str != '\0') {
		if (*chars_printed < n) {
			*(*s) = *str;
			(*s)++;
		}

		(*chars_printed)++;
		str++;
	}
}

static void unsigned_dec_print(char **s, size_t n, size_t *chars_printed,
			       unsigned int unum)
{
	/* Enough for a 32-bit unsigned decimal integer (4294967295). */
	char num_buf[10];
	int i = 0;
	unsigned int rem;

	do {
		rem = unum % 10U;
		num_buf[i++] = '0' + rem;
		unum /= 10U;
	} while (unum > 0U);

	while (--i >= 0) {
		if (*chars_printed < n) {
			*(*s) = num_buf[i];
			(*s)++;
		}

		(*chars_printed)++;
	}
}

static int unsigned_num_print(char **s, 
                              unsigned long long int unum, 
                              size_t *chars_printed,
                              unsigned int radix,
			                  char padc, 
                              int padn)
{
	/* Just need enough space to store 64 bit decimal integer */
	char num_buf[20];
	int i = 0, count = 0;
	unsigned int rem;

	do {
		rem = unum % radix;
		if (rem < 0xa)
			num_buf[i] = '0' + rem;
		else
			num_buf[i] = 'A' + (rem - 0xa);
		i++;
		unum /= radix;
	} while (unum > 0U);

	if (padn > 0) {
		while (i < padn) {
			*(*s) = padc;
            (*s)++;
            (*chars_printed)++;
			count++;
			padn--;
		}
	}

	while (--i >= 0) {
		*(*s) = num_buf[i];
        (*s)++;
        (*chars_printed)++;
		count++;
	}

	return count;
}
/*******************************************************************
 * Reduced snprintf to be used for Trusted firmware.
 * The following type specifiers are supported:
 *
 * %d or %i - signed decimal format
 * %s - string format
 * %u - unsigned decimal format
 *
 * The function panics on all other formats specifiers.
 *
 * It returns the number of characters that would be written if the
 * buffer was big enough. If it returns a value lower than n, the
 * whole string has been written.
 *******************************************************************/
int snprintf(char *s, size_t n, const char *fmt, ...)
{
	va_list args;
	int num;
	unsigned int unum;
	char *str;
	size_t chars_printed = 0U;
	char padc = '\0'; /* Padding character */
	int padn; /* Number of characters to pad */

	if (n == 0U) {
		/* There isn't space for anything. */
	} else if (n == 1U) {
		/* Buffer is too small to actually write anything else. */
		*s = '\0';
		n = 0U;
	} else {
		/* Reserve space for the terminator character. */
		n--;
	}

	va_start(args, fmt);
	while (*fmt != '\0') {
        
		if (*fmt == '%') {
			fmt++;
            padn = 0;
snprintf_loop:
			/* Check the format specifier. */
			switch (*fmt) {
			case 'i':
			case 'd':
				num = va_arg(args, int);

				if (num < 0) {
					if (chars_printed < n) {
						*s = '-';
						s++;
					}
					chars_printed++;

					unum = (unsigned int)-num;
				} else {
					unum = (unsigned int)num;
				}

				unsigned_dec_print(&s, n, &chars_printed, unum);
				break;
			case 's':
				str = va_arg(args, char *);
				string_print(&s, n, &chars_printed, str);
				break;
			case 'u':
				unum = va_arg(args, unsigned int);
				unsigned_dec_print(&s, n, &chars_printed, unum);
				break;
            case 'x':
				unum = va_arg(args, unsigned int);
                unsigned_num_print(&s, unum, &chars_printed,16, padc,padn);
            break;
            case '0':
                padc = '0';
                padn = 0;
                fmt++;

                for (;;) {
                    char ch = *fmt;
                    if ((ch < '0') || (ch > '9')) {
                        goto snprintf_loop;
                    }
                    padn = (padn * 10) + (ch - '0');
                    fmt++;
                }
                while(1)
                    __asm__ ("nop");
			default:
				/* Panic on any other format specifier. */
				LOG_ERR("snprintf: specifier with ASCII code '%d' not supported.",
				      *fmt);
                while(1); // TODO: add plat_panic
			}
			fmt++;
			continue;
		}

		if (chars_printed < n) {
			*s = *fmt;
			s++;
		}

		fmt++;
		chars_printed++;
	}

	va_end(args);

	if (n > 0U)
		*s = '\0';

	return (int)chars_printed;
}
