/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/rom_log/imx_hab_log.h>
#include <pb/log_source.h>
#include <pb/pb.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* HABv4 values, as in include/plat/imx6ul/hab.h */
#define HAB_TAG_RVT       0xdd
#define HAB_TAG_EVT       0xdb
#define HAB_STS_ANY       0x00
#define HAB_SUCCESS       0xf0

/* RVT: a 4 byte header, then pointers; on AArch64 they start 8 bytes in */
#define RVT_REPORT_EVENT  7
#define RVT_REPORT_STATUS 8

/* Bytes of one event: header, status, reason, context, engine, data */
#define EVENT_MAX         128
#define TEXT_SIZE         4096

struct name {
    uint8_t value;
    const char *name;
};

static const struct name statuses[] = {
    { 0xf0, "success" },
    { 0x69, "warning" },
    { 0x33, "failure" },
};

static const struct name configs[] = {
    { 0xf0, "open" },
    { 0xcc, "closed" },
    { 0x33, "field return" },
};

static const struct name states[] = {
    { 0x33, "initial" },    { 0x55, "check" },
    { 0x66, "non-secure" }, { 0x99, "trusted" },
    { 0xaa, "secure" },     { 0xcc, "soft fail" },
    { 0xff, "hard fail" },  { 0xf0, "no security state machine" },
};

static const struct name reasons[] = {
    { 0x30, "engine failure" },
    { 0x22, "invalid address" },
    { 0x0c, "invalid assertion" },
    { 0x28, "function called out of sequence" },
    { 0x21, "invalid certificate" },
    { 0x06, "invalid command" },
    { 0x11, "invalid CSF" },
    { 0x27, "invalid DCD" },
    { 0x0f, "invalid index" },
    { 0x05, "invalid IVT" },
    { 0x1d, "invalid key" },
    { 0x1e, "failed callback" },
    { 0x18, "invalid signature" },
    { 0x17, "invalid data size" },
    { 0x2e, "memory failure" },
    { 0x2b, "expired poll count" },
    { 0x2d, "exhausted storage" },
    { 0x12, "unsupported algorithm" },
    { 0x03, "unsupported command" },
    { 0x0a, "unsupported engine" },
    { 0x24, "unsupported configuration item" },
    { 0x1b, "unsupported key" },
    { 0x14, "unsupported protocol" },
    { 0x09, "unsuitable state" },
};

static const struct name contexts[] = {
    { 0xff, "FAB test" },
    { 0xe1, "entry" },
    { 0x33, "check target" },
    { 0x0a, "authenticate image" },
    { 0xdd, "DCD" },
    { 0xcf, "CSF" },
    { 0xc0, "CSF/DCD command" },
    { 0xdb, "authenticated data" },
    { 0xa0, "assert" },
    { 0xee, "exit" },
};

static const struct name engines[] = {
    { 0x00, "any" },  { 0x03, "SCC" }, { 0x05, "RTIC" }, { 0x06, "SAHARA" }, { 0x0a, "CSU" },
    { 0x0c, "SRTC" }, { 0x1b, "DCP" }, { 0x1d, "CAAM" }, { 0x1e, "SNVS" },   { 0x21, "OCOTP" },
    { 0x22, "DTCP" }, { 0x36, "ROM" }, { 0x24, "HDCP" }, { 0x77, "RTL" },    { 0xff, "software" },
};

static const char *find_name(const struct name *names, size_t n, uint32_t value)
{
    for (size_t i = 0; i < n; i++) {
        if (names[i].value == value)
            return names[i].name;
    }

    return "unknown";
}

#define NAME(table, value) find_name(table, ARRAY_SIZE(table), value)

struct text {
    char *buf;
    size_t size;
    size_t len;
    bool truncated;
};

/* Add to the text; what doesn't fit is cut, and the text stays terminated.
 * The firmware's vsnprintf() has %s, %i, %u, %x and %0<width>x */
static void put(struct text *t, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void put(struct text *t, const char *fmt, ...)
{
    size_t left = t->size - t->len;
    va_list args;
    int n;

    if (t->truncated)
        return;

    va_start(args, fmt);
    n = vsnprintf(&t->buf[t->len], left, fmt, args);
    va_end(args);

    if (n < 0 || (size_t)n >= left) {
        t->len = t->size - 1;
        t->truncated = true;
        return;
    }

    t->len += (size_t)n;
}

/* "label: name (0xNN)" */
static void put_named(struct text *t, const char *label, const char *name, uint32_t value)
{
    put(t, "%s: %s (0x%02x)", label, name, (unsigned int)value);
}

int imx_hab_log_decode(imx_hab_report_status_t *report_status,
                       imx_hab_report_event_t *report_event,
                       char *out,
                       size_t *out_len)
{
    struct text t = { .buf = out, .size = *out_len };
    uint32_t config = 0, state = 0;
    uint32_t status;
    static uint8_t event[EVENT_MAX];

    if (t.size == 0)
        return -PB_ERR_BUF_TOO_SMALL;
    out[0] = '\0';

    status = report_status(&config, &state);
    put_named(&t, "HAB status", NAME(statuses, status), status);
    put_named(&t, ", config", NAME(configs, config), config);
    put_named(&t, ", state", NAME(states, state), state);
    put(&t, "\n");

    for (uint32_t i = 0;; i++) {
        size_t bytes = sizeof(event);

        if (report_event(HAB_STS_ANY, i, event, &bytes) != HAB_SUCCESS)
            break;

        bytes = MIN(bytes, sizeof(event));
        put(&t, "Event %u", (unsigned int)i);
        if (bytes >= 8 && event[0] == HAB_TAG_EVT) {
            put_named(&t, ": status", NAME(statuses, event[4]), event[4]);
            put_named(&t, ", reason", NAME(reasons, event[5]), event[5]);
            put_named(&t, ", context", NAME(contexts, event[6]), event[6]);
            put_named(&t, ", engine", NAME(engines, event[7]), event[7]);
        } else {
            put(&t, ": not an event record");
        }
        put(&t, "\n ");
        for (size_t j = 0; j < bytes; j++) {
            put(&t, " %02x", (unsigned int)event[j]);
            if ((j % 16) == 15 && j + 1 < bytes)
                put(&t, "\n ");
        }
        put(&t, "\n");
    }

    *out_len = t.len;
    return t.truncated ? -PB_ERR_BUF_TOO_SMALL : PB_OK;
}

static const struct imx_hab_log_config *hab_cfg;
static char hab_text[TEXT_SIZE];
static size_t hab_text_len;
static bool hab_decoded;

static uintptr_t rvt_entry(unsigned int n)
{
    /* The RVT is in ROM at a low address, which GCC takes for a NULL
     * dereference; the empty asm hides the constant from it */
    uintptr_t addr = hab_cfg->rvt + (hab_cfg->entry_size == 8 ? 8 : 4) + n * hab_cfg->entry_size;

    __asm__("" : "+r"(addr));
    if (hab_cfg->entry_size == 8)
        return (uintptr_t)*(const volatile uint64_t *)addr;
    return (uintptr_t)*(const volatile uint32_t *)addr;
}

/* Calls the ROM at the first read */
static size_t hab_length(void)
{
    if (!hab_decoded) {
        hab_text_len = sizeof(hab_text);
        /* A truncated log still has what fits */
        (void)imx_hab_log_decode((imx_hab_report_status_t *)rvt_entry(RVT_REPORT_STATUS),
                                 (imx_hab_report_event_t *)rvt_entry(RVT_REPORT_EVENT),
                                 hab_text,
                                 &hab_text_len);
        hab_decoded = true;
    }

    return hab_text_len;
}

static uint32_t hab_head(void)
{
    return (uint32_t)hab_length();
}

static size_t hab_read(size_t offset, void *buf, size_t length)
{
    size_t size = hab_length();

    if (offset >= size)
        return 0;

    length = MIN(length, size - offset);
    memcpy(buf, &hab_text[offset], length);
    return length;
}

static const struct log_source hab_log = {
    .name = "hab",
    .description = "The boot ROM's HAB status and events",
    .head = hab_head,
    .length = hab_length,
    .read = hab_read,
};

int imx_hab_log_init(const struct imx_hab_log_config *cfg)
{
    uintptr_t addr = cfg->rvt;
    uint8_t tag;

    __asm__("" : "+r"(addr));
    tag = *(const volatile uint8_t *)addr;
    if (tag != HAB_TAG_RVT) {
        LOG_ERR("No HAB RVT at 0x%x (tag 0x%x)", (unsigned int)cfg->rvt, tag);
        return -PB_ERR_NOT_FOUND;
    }

    hab_cfg = cfg;
    hab_decoded = false;
    return log_source_register(&hab_log);
}
