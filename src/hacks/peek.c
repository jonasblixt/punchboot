/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The peek hack (CONFIG_HACK_PEEK): a named command that reads memory, for
 * finding what a ROM leaves behind. Never in a product's build.
 *
 *   punchboot cmd peek "0x960 16"
 *
 * The request is text: an address and a length in bytes, each decimal or
 * hex with 0x, both multiples of 4; without a length, 4. The response is
 * text, 16 bytes a line as 32-bit words, as many lines as fit it:
 *
 *   00000960: 0090cae0 00000000 0090cbe0 00000000
 */

#include <pb/command.h>
#include <pb/pb.h>
#include <stdio.h>
#include <string.h>

#ifdef CONFIG_LIB_XLAT_TBLS
#include <pb/mmu.h>
#endif

/* A line: the address, four words, the newline */
#define PEEK_LINE_MAX 48

/* A number in the request at *p, decimal or 0x hex; *p moves past it */
static int parse_number(const char **p, const char *end, uint64_t *out)
{
    const char *s = *p;
    unsigned int base = 10;
    uint64_t v = 0;
    bool digits = false;

    while (s < end && (*s == ' ' || *s == '\t'))
        s++;

    if (end - s > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    }

    for (; s < end; s++) {
        unsigned int d;

        if (*s >= '0' && *s <= '9')
            d = (unsigned int)(*s - '0');
        else if (base == 16 && *s >= 'a' && *s <= 'f')
            d = (unsigned int)(*s - 'a' + 10);
        else if (base == 16 && *s >= 'A' && *s <= 'F')
            d = (unsigned int)(*s - 'A' + 10);
        else
            break;

        if (d >= base || v > (UINT64_MAX - d) / base)
            return -PB_ERR_PARAM;
        v = v * base + d;
        digits = true;
    }

    if (!digits)
        return -PB_ERR_PARAM;

    *p = s;
    *out = v;
    return PB_OK;
}

static int peek(const uint8_t *request, size_t request_size, uint8_t *response, size_t *response_size)
{
    const char *p = (const char *)request;
    const char *end = p + request_size;
    char *text = (char *)response;
    size_t room = *response_size;
    size_t pos = 0;
    uint64_t addr;
    uint64_t len = 4;

    *response_size = 0;

    /* The request may end in a NUL or white space */
    while (end > p && (end[-1] == '\0' || end[-1] == ' ' || end[-1] == '\n'))
        end--;

    if (parse_number(&p, end, &addr) != PB_OK)
        return -PB_ERR_PARAM;

    while (p < end && (*p == ' ' || *p == '\t'))
        p++;

    if (p < end && parse_number(&p, end, &len) != PB_OK)
        return -PB_ERR_PARAM;

    if (p != end || len == 0 || len % 4 != 0 || addr % 4 != 0 || addr > UINTPTR_MAX - len)
        return -PB_ERR_PARAM;

    /* As many lines as fit the response, with its NUL */
    if (room < PEEK_LINE_MAX + 1)
        return -PB_ERR_PARAM;
    len = MIN(len, (uint64_t)(room - 1) / PEEK_LINE_MAX * 16);

#ifdef CONFIG_LIB_XLAT_TBLS
    /* What isn't mapped faults: only what pb maps */
    if (!pb_mmu_mapped((uintptr_t)addr, (size_t)len))
        return -PB_ERR_PARAM;
#endif

    for (uint64_t n = 0; n < len; n += 4) {
        uint32_t word = *(const volatile uint32_t *)(uintptr_t)(addr + n);
        uint32_t a = (uint32_t)(addr + n);

        if (n % 16 == 0) {
            if (n != 0)
                text[pos++] = '\n';
            if ((addr + n) >> 32)
                pos += snprintf(&text[pos], room - pos, "%x", (unsigned int)((addr + n) >> 32));
            pos += snprintf(&text[pos], room - pos, "%08x:", (unsigned int)a);
        }

        pos += snprintf(&text[pos], room - pos, " %08x", (unsigned int)word);
    }

    text[pos++] = '\n';
    text[pos++] = '\0';
    *response_size = pos;
    return PB_OK;
}

DEFINE_COMMAND("peek", "Hack: words of memory, \"<address> [<length>]\"", peek);
