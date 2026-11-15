/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M SPL's fastboot (D-089), its commands. What a host sends is
 * read here before anything checked it (security.md 1): a command is bytes
 * of a length, never a string, and every number in it is bounded.
 *
 *   getvar:<name>      version, product, max-download-size
 *   download:<hex>     the size of what follows, at most the room
 *   boot, continue     start what was downloaded (fastboot's "boot" and
 *                      "continue"; uuu's "FB: boot", which sends "boot:")
 *
 * Nothing of the device but that it is pb's SPL is told (security.md 4.7).
 */

#include <stdbool.h>
#include <string.h>

#include "fb_proto.h"

#define FB_PROTOCOL_VERSION "0.4"
#define FB_PRODUCT          "punchboot-spl"

/* The command is 'word' and nothing else */
static bool is_word(const char *cmd, size_t len, const char *word)
{
    return len == strlen(word) && memcmp(cmd, word, len) == 0;
}

/* The command is 'word' without an argument: the word, or the word and the
 * ':' an argument would follow, as uuu sends every command */
static bool is_word_no_arg(const char *cmd, size_t len, const char *word)
{
    if (len > 0 && cmd[len - 1] == ':')
        len--;

    return is_word(cmd, len, word);
}

/* The command starts with 'prefix': what follows it, and its length */
static bool has_prefix(const char *cmd, size_t len, const char *prefix, const char **arg, size_t *n)
{
    size_t plen = strlen(prefix);

    if (len < plen || memcmp(cmd, prefix, plen) != 0)
        return false;

    *arg = cmd + plen;
    *n = len - plen;
    return true;
}

/* "OKAY", "FAIL" or "DATA" and a text, cut to what a reply holds */
static void answer(char *reply, const char *status, const char *text)
{
    size_t n = strlen(text);

    if (n > FB_REPLY_MAX - 4)
        n = FB_REPLY_MAX - 4;

    memcpy(reply, status, 4);
    memcpy(reply + 4, text, n);
    reply[4 + n] = '\0';
}

/* 'value' as eight hex digits, as fastboot writes a size */
static void hex32(char *out, uint32_t value)
{
    static const char digits[] = "0123456789abcdef";

    for (unsigned int i = 0; i < 8; i++)
        out[i] = digits[(value >> (28 - 4 * i)) & 0xf];

    out[8] = '\0';
}

/* One to eight hex digits and nothing else */
static bool parse_hex32(const char *text, size_t n, uint32_t *value)
{
    uint32_t v = 0;

    if (n == 0 || n > 8)
        return false;

    for (size_t i = 0; i < n; i++) {
        char c = text[i];
        uint32_t d;

        if (c >= '0' && c <= '9')
            d = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f')
            d = (uint32_t)(c - 'a') + 10;
        else if (c >= 'A' && c <= 'F')
            d = (uint32_t)(c - 'A') + 10;
        else
            return false;

        v = (v << 4) | d;
    }

    *value = v;
    return true;
}

static void getvar(const struct fb_state *s, const char *name, size_t n, char *reply)
{
    char text[11] = "0x";

    if (is_word(name, n, "version")) {
        answer(reply, "OKAY", FB_PROTOCOL_VERSION);
    } else if (is_word(name, n, "product")) {
        answer(reply, "OKAY", FB_PRODUCT);
    } else if (is_word(name, n, "max-download-size")) {
        /* A size is 32 bits on the wire */
        hex32(text + 2, s->room > UINT32_MAX ? UINT32_MAX : (uint32_t)s->room);
        answer(reply, "OKAY", text);
    } else {
        answer(reply, "FAIL", "unknown variable");
    }
}

enum fb_next fb_command(struct fb_state *s, const char *cmd, size_t len, char *reply)
{
    const char *arg;
    size_t n;
    uint32_t size;
    char text[9];

    if (len > FB_CMD_MAX) {
        answer(reply, "FAIL", "command too long");
    } else if (has_prefix(cmd, len, "getvar:", &arg, &n)) {
        getvar(s, arg, n, reply);
    } else if (has_prefix(cmd, len, "download:", &arg, &n)) {
        /* A new download replaces the last, also one that is refused */
        s->expect = 0;
        s->downloaded = 0;

        if (!parse_hex32(arg, n, &size) || size == 0) {
            answer(reply, "FAIL", "bad size");
        } else if (size > s->room) {
            answer(reply, "FAIL", "too large");
        } else {
            s->expect = size;
            hex32(text, size);
            answer(reply, "DATA", text);
            return FB_NEXT_DATA;
        }
    } else if (is_word_no_arg(cmd, len, "boot") || is_word_no_arg(cmd, len, "continue")) {
        if (s->downloaded == 0) {
            answer(reply, "FAIL", "nothing downloaded");
        } else {
            reply[0] = '\0';
            return FB_NEXT_BOOT;
        }
    } else {
        answer(reply, "FAIL", "unknown command");
    }

    return FB_NEXT_REPLY;
}

void fb_data_done(struct fb_state *s, int ok, char *reply)
{
    if (ok && s->expect > 0) {
        s->downloaded = s->expect;
        answer(reply, "OKAY", "");
    } else {
        s->downloaded = 0;
        answer(reply, "FAIL", "download failed");
    }

    s->expect = 0;
}
