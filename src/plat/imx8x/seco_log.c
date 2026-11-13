/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/log_source.h>
#include <pb/pb.h>
#include <plat/imx8x/sci/svc/seco/api.h>
#include <plat/imx8x/seco_log.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*
 * An event is the SECO's response code to a request (SECO API Reference
 * Guide, "Responses"): the request in bits 23:16, the indication (why) in
 * bits 15:8, the status in bits 7:0. Codes without a name print as numbers.
 */
#define EVENT_COMMAND(ev)    (((ev) >> 16) & 0xff)
#define EVENT_INDICATION(ev) (((ev) >> 8) & 0xff)
#define EVENT_STATUS(ev)     ((ev) & 0xff)

struct code_name {
    uint32_t code;
    const char *name;
};

static const struct code_name commands[] = {
    { 0x03, "attest" },
    { 0x85, "assign job ring" },
    { 0x87, "authenticate container" },
    { 0x88, "verify image" },
    { 0x89, "release container" },
    { 0x8b, "write SNVS parameter" },
    { 0x8c, "read SNVS parameter" },
    { 0x90, "load key" },
    { 0x91, "write secure fuse" },
    { 0x94, "enable debug" },
    { 0x95, "forward life cycle" },
    { 0x96, "write fuse" },
    { 0x9d, "get SECO FW version" },
    { 0xa0, "return life cycle" },
    { 0xa1, "get chip info" },
    { 0xa2, "get event" },
    { 0xa3, "start RNG" },
    { 0xa4, "set attest mode" },
    { 0xa5, "attest verify" },
    { 0xa6, "get attest signature" },
    { 0xa7, "get attest public key" },
    { 0xa8, "commit" },
    { 0xaa, "assign SECO MU" },
    { 0xab, "get MP public key" },
    { 0xac, "update MPMR" },
    { 0xad, "get MP signature" },
    { 0xae, "manage SNVS" },
    { 0xaf, "generate key blob" },
};

static const struct code_name indications[] = {
    { 0x00, "none" },
    { 0xa1, "BID already allocated" },
    { 0xa2, "DID doesn't match the BID" },
    { 0xa3, "unknown BID" },
    { 0xa6, "payload not 64-bit aligned" },
    { 0xa7, "size not a multiple of 64 bits" },
    { 0xaa, "fuse programming failed" },
    { 0xab, "fuse locked" },
    { 0xac, "fuse double error" },
    { 0xad, "invalid fuse index" },
    { 0xae, "ADM not ready" },
    { 0xaf, "wrong life cycle transition" },
    { 0xb0, "time out" },
    { 0xb1, "bad payload" },
    { 0xb3, "KMEK generation failed" },
    { 0xb4, "wrong address" },
    { 0xb5, "DMA failure" },
    { 0xb6, "disabled feature" },
    { 0xb7, "must attest" },
    { 0xb8, "RNG not started" },
    { 0xb9, "OCOTP CRC error" },
    { 0xba, "blob generation failed" },
    { 0xbb, "authentication skipped or failed" },
    { 0xbc, "inconsistent parameters" },
    { 0xbd, "RNG instantiation failed" },
    { 0xbe, "bad id" },
    { 0xc0, "invalid operation" },
    { 0xc1, "SNVS not secure" },
    { 0xe0, "signed message required" },
    { 0xee, "no authentication" },
    { 0xef, "NXP SRK expected" },
    { 0xf0, "bad signature" },
    { 0xf1, "bad hash" },
    { 0xf2, "invalid life cycle" },
    { 0xf3, "permission denied" },
    { 0xf4, "invalid message" },
    { 0xf5, "bad value" },
    { 0xf6, "bad fuse id" },
    { 0xf7, "bad container" },
    { 0xf8, "bad version" },
    { 0xf9, "invalid key" },
    { 0xfa, "bad key hash" },
    { 0xfb, "no valid container" },
    { 0xfc, "bad certificate" },
    { 0xfd, "bad UID" },
    { 0xfe, "bad monotonic counter" },
    { 0xff, "SECO fatal failure" },
};

static const struct code_name statuses[] = {
    { 0x00, "success" },
    { 0x29, "failure" },
};

static const struct code_name life_cycles[] = {
    { 0x001, "pristine" },          { 0x002, "fab" },        { 0x008, "open" },
    { 0x020, "NXP closed" },        { 0x080, "OEM closed" }, { 0x100, "partial field return" },
    { 0x200, "full field return" }, { 0x400, "no return" },
};

static const char *code_name(const struct code_name *names, size_t n, uint32_t code)
{
    for (size_t i = 0; i < n; i++) {
        if (names[i].code == code)
            return names[i].name;
    }

    return "unknown";
}

#define NAME(table, value) code_name(table, ARRAY_SIZE(table), value)

struct text {
    char *buf;
    size_t size;
    size_t len;
    bool truncated;
};

/* Whole lines only: a line that doesn't fit, and those after it, are left
 * out. The firmware's vsnprintf() has %s, %i, %u, %x and %0<width>x */
static void put_line(struct text *t, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void put_line(struct text *t, const char *fmt, ...)
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
        t->buf[t->len] = '\0';
        t->truncated = true;
        return;
    }

    t->len += (size_t)n;
}

int imx8x_seco_log_decode(int32_t lc,
                          const uint32_t *events,
                          size_t n_events,
                          char *out,
                          size_t *out_len)
{
    struct text t = { .buf = out, .size = *out_len };

    if (t.size == 0)
        return -PB_ERR_BUF_TOO_SMALL;
    out[0] = '\0';

    if (lc < 0) {
        put_line(&t, "SECO life cycle: not read (%i)\n", (int)lc);
    } else {
        put_line(&t,
                 "SECO life cycle: %s (0x%04x)\n",
                 NAME(life_cycles, (uint32_t)lc),
                 (unsigned int)lc);
    }

    if (n_events == 0)
        put_line(&t, "No SECO events\n");

    for (size_t i = 0; i < n_events; i++) {
        uint32_t ev = events[i];

        put_line(&t,
                 "Event %u: 0x%08x, command: %s (0x%02x), indication: %s (0x%02x), "
                 "status: %s (0x%02x)\n",
                 (unsigned int)i,
                 (unsigned int)ev,
                 NAME(commands, EVENT_COMMAND(ev)),
                 (unsigned int)EVENT_COMMAND(ev),
                 NAME(indications, EVENT_INDICATION(ev)),
                 (unsigned int)EVENT_INDICATION(ev),
                 NAME(statuses, EVENT_STATUS(ev)),
                 (unsigned int)EVENT_STATUS(ev));
    }

    *out_len = t.len;
    return t.truncated ? -PB_ERR_BUF_TOO_SMALL : PB_OK;
}

static sc_ipc_t seco_ipc;
/* A header and IMX8X_SECO_LOG_MAX_EVENTS lines of at most ~140 characters */
static char seco_text[2560];
static size_t seco_text_len;
static bool seco_decoded;

/* Asks the SCFW at the first read: the events stay until the next reset */
static size_t seco_length(void)
{
    uint32_t events[IMX8X_SECO_LOG_MAX_EVENTS];
    size_t n_events = 0;
    uint16_t lc;
    int32_t lc_read;
    sc_err_t err;

    if (seco_decoded)
        return seco_text_len;

    err = sc_seco_chip_info(seco_ipc, &lc, NULL, NULL, NULL);
    lc_read = (err == SC_ERR_NONE) ? (int32_t)lc : -(int32_t)err;

    /* Index 0 takes the SECO's events, the rest read them; an error is
     * the end of them (SCFW API, sc_seco_get_event()) */
    while (n_events < ARRAY_SIZE(events) &&
           sc_seco_get_event(seco_ipc, (uint8_t)n_events, &events[n_events]) == SC_ERR_NONE)
        n_events++;

    seco_text_len = sizeof(seco_text);
    /* A truncated log still has what fits */
    (void)imx8x_seco_log_decode(lc_read, events, n_events, seco_text, &seco_text_len);
    seco_decoded = true;

    return seco_text_len;
}

static uint32_t seco_head(void)
{
    return (uint32_t)seco_length();
}

static size_t seco_read(size_t offset, void *buf, size_t length)
{
    size_t size = seco_length();

    if (offset >= size)
        return 0;

    length = MIN(length, size - offset);
    memcpy(buf, &seco_text[offset], length);
    return length;
}

static const struct log_source seco_log = {
    .name = "seco",
    .description = "The SECO's life cycle and events",
    .head = seco_head,
    .length = seco_length,
    .read = seco_read,
};

int imx8x_seco_log_init(sc_ipc_t ipc)
{
    seco_ipc = ipc;
    seco_decoded = false;
    return log_source_register(&seco_log);
}
