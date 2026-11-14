/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * See caam_model.h. Violations cite the SRM's CAAM chapter by its section's
 * or its register's name, which is the same in the i.MX6UL's and the
 * i.MX8M Mini's manual.
 */

/* The hash contexts' running state is restored from the CAAM's */
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/md5.h>
#include <mbedtls/platform.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <stdlib.h>
#include <string.h>

#include "bench.h"
#include "caam_model.h"
#include "fakes/dcache.h"

#define SRM               "CAAM SRM "
#define DMA               "drivers.md 4.2"

/* The job ring's registers. The 64 bit ones have their least significant
 * word at +4 */
#define IRBAR_MS          0x000
#define IRBAR_LS          0x004
#define IRSR              0x00c
#define IRSAR             0x014
#define IRJAR             0x01c
#define ORBAR_MS          0x020
#define ORBAR_LS          0x024
#define ORSR              0x02c
#define ORJRR             0x034
#define ORSFR             0x03c
#define JRSTAR            0x044
#define JRINTR            0x04c
#define JRCFGR_MS         0x050
#define JRCFGR_LS         0x054
#define CCBVID            0xfe4
#define CHAVID_LS         0xfec
#define CAAMVID_MS        0xff8
#define PAGE_SIZE         0x1000

#define RING_SIZE_MASK    0x3ff

/* Commands, CTYPE in bits 31-27 */
#define CTYPE(word)       ((word) >> 27)
#define CTYPE_LOAD        0x02
#define CTYPE_FIFO_LOAD   0x04
#define CTYPE_STORE       0x0a
#define CTYPE_OPERATION   0x10
#define CTYPE_JOB_HEADER  0x16
#define CMD_CLASS(word)   (((word) >> 25) & 3)
#define CMD_SGF           (1u << 24)
#define CMD_IMM           (1u << 23)

/* Job descriptor header */
#define HDR_ONE           (1u << 23)
#define HDR_START(word)   (((word) >> 16) & 0x3f)
#define HDR_LEN(word)     ((word) & 0x7f)
#define HDR_KNOWN         (0xf8000000u | HDR_ONE | (0x3fu << 16) | 0x7f)

/* LOAD and STORE */
#define LS_REG(word)      (((word) >> 16) & 0x7f)
#define LS_OFFSET(word)   (((word) >> 8) & 0xff)
#define LS_LENGTH(word)   ((word) & 0xff)
#define REG_CONTEXT       0x20
#define CONTEXT_SIZE      128

/* FIFO LOAD */
#define FL_EXT            (1u << 22)
#define FL_TYPE(word)     (((word) >> 16) & 0x3f)
#define FL_MSG_LAST2      0x14 /* Message data, the last for class 2 */

/* OPERATION */
#define OP_TYPE(word)     (((word) >> 24) & 7)
#define OP_TYPE_UNI       0
#define OP_TYPE_CLASS2    4
#define OP_ALG(word)      (((word) >> 16) & 0xff)
#define OP_AAI(word)      (((word) >> 4) & 0x1ff)
#define OP_AS(word)       (((word) >> 2) & 3)
#define AS_UPDATE         0
#define AS_INIT           1
#define AS_FINAL          2
#define AS_INIT_FINAL     3
#define PROTID_DSAVERIFY  0x16
#define PROT_MES_REP(w)   (((w) >> 10) & 3)
#define MES_REP_HASHED    2
#define PROT_ECC          (1u << 1)

/* The verification's PDB, with a built-in domain */
#define PDB_PD            (1u << 22)
#define PDB_ECDSEL(word)  (((word) >> 7) & 0x7f)
#define PDB_WORDS         7
#define EC_MAX            66 /* P-521's L and N, bytes */

/* Status words: the source, the descriptor's word, the code */
#define DECO(index, code) (0x40000000u | CAAM_STATUS_INDEX(index) | (code))
#define DECO_INVALID_CMD  0x04
#define DECO_LOAD         0x07
#define DECO_STORE        0x08
#define DECO_OPERATION    0x09
#define DECO_FIFO_LOAD    0x0a
#define DECO_HEADER       0x13
#define DECO_CONTEXT_LEN  0x15
#define DECO_PROTOCOL     0x81

#define JOBS_MAX          4 /* The job queue controller's job IDs */
#define DESC_WORDS        CAAM_MODEL_DESC_WORDS
#define RANGES            6
#define OUTS              2
#define OUT_MAX           (2 * EC_MAX)
#define JOB_US            20

struct alg {
    uint8_t sel;
    const char *name;
    uint32_t block;
    uint32_t running; /* The running digest in the context register */
};

static const struct alg algs[] = {
    { 0x40, "MD5", 64, 16 },
    { 0x43, "SHA-256", 64, 32 },
    { 0x44, "SHA-384", 128, 64 },
    { 0x45, "SHA-512", 128, 64 },
};

/* Memory a job reads, and what was in it when the job was taken */
struct range {
    uint32_t addr;
    uint32_t len;
    uint64_t sum;
};

/* What a job writes when it is done */
struct out {
    uint32_t addr;
    uint32_t len;
    uint8_t data[OUT_MAX];
};

struct job {
    uint32_t addr;
    uint32_t desc[DESC_WORDS];
    unsigned int len;
    bool bad; /* Not a descriptor */
    struct range in[RANGES];
    unsigned int n_in;
    struct out out[OUTS];
    unsigned int n_out;
    uint32_t status;
    uint64_t taken_at;
    uint64_t done_at;
    bool started;
    bool done;
};

/* The class 2 registers of the job that runs */
struct ccb {
    const struct alg *alg;
    unsigned int as;
    uint8_t ctx[CONTEXT_SIZE];
    unsigned int ctx_loaded;
};

/* A command with the words that belong to it */
struct cmd {
    unsigned int index;
    uint32_t word;
    uint32_t pointer;
    uint32_t length;
};

struct pdb {
    mbedtls_ecp_group_id id;
    uint32_t l; /* The field's and the subgroup's size, bytes */
    uint32_t w, f, c, d, tmp, ml;
};

static struct {
    struct caam_model_config cfg;
    uint32_t irbar, irsr, irsar;
    uint32_t orbar, orsr, orsfr;
    bool earlier_ring; /* The output ring is the stage's before pb */
    uint32_t jrstar;
    uint32_t jrcfgr[2];
    uint32_t in_ring; /* Jobs added and not taken yet */
    uint32_t rindex, windex;
    struct job jobs[JOBS_MAX]; /* In the engine, the oldest first */
    unsigned int n_jobs;
    uint64_t free_at; /* When the engine was done with the job before */
    uint32_t job_us;
    bool hang;
    bool fail_next;
    uint32_t fail_status;
    unsigned int jobs_done;
    struct caam_model_event *trace;
    unsigned int trace_max;
    unsigned int traced;
} m;

/* The engine reads and writes RAM, behind the driver's cache (fakes/dcache.h) */
static void ram_read(uint32_t addr, void *dst, uint32_t len)
{
    dcache_dev_read(addr, dst, len);
}

/* FNV-1a, of what the driver has there: its cache. A job's data isn't
 * written while the engine has it, cleaned or not */
static uint64_t sum(uint32_t addr, uint32_t len)
{
    const uint8_t *p = (const void *)(uintptr_t)addr;
    uint64_t h = 0xcbf29ce484222325ull;

    for (uint32_t i = 0; i < len; i++)
        h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}

static uint64_t get_be(const uint8_t *p, unsigned int bytes)
{
    uint64_t v = 0;

    for (unsigned int i = 0; i < bytes; i++)
        v = (v << 8) | p[i];
    return v;
}

static void put_be(uint8_t *p, uint64_t v, unsigned int bytes)
{
    for (unsigned int i = 0; i < bytes; i++)
        p[i] = (uint8_t)(v >> (8 * (bytes - 1 - i)));
}

static uint32_t next_word(const struct job *job, unsigned int *i)
{
    return (*i < job->len) ? job->desc[(*i)++] : 0;
}

static bool next_cmd(const struct job *job, unsigned int *i, struct cmd *c)
{
    if (*i >= job->len)
        return false;

    c->index = *i;
    c->word = job->desc[(*i)++];
    c->pointer = 0;
    c->length = 0;

    switch (CTYPE(c->word)) {
    case CTYPE_LOAD:
    case CTYPE_STORE:
        c->length = LS_LENGTH(c->word);
        if (c->word & CMD_IMM)
            *i += (c->length + 3) / 4;
        else
            c->pointer = next_word(job, i);
        break;
    case CTYPE_FIFO_LOAD:
        if (c->word & CMD_IMM) {
            *i += ((c->word & 0xffff) + 3) / 4;
            break;
        }
        c->pointer = next_word(job, i);
        c->length = (c->word & FL_EXT) ? next_word(job, i) : (c->word & 0xffff);
        break;
    default:
        break;
    }
    return true;
}

static unsigned int first_cmd(const struct job *job)
{
    unsigned int start = HDR_START(job->desc[0]);

    return start ? start : 1;
}

/* The PDB is between the header and the first command */
static bool dsa_pdb(const struct job *job, struct pdb *p)
{
    uint32_t w1 = job->desc[1];

    if (first_cmd(job) != 1 + PDB_WORDS || job->len <= PDB_WORDS)
        return false;
    if (!(w1 & PDB_PD) || (w1 & ~(PDB_PD | (0x7fu << 7))))
        return false;

    switch (PDB_ECDSEL(w1)) {
    case 2:
        p->id = MBEDTLS_ECP_DP_SECP256R1;
        p->l = 32;
        break;
    case 3:
        p->id = MBEDTLS_ECP_DP_SECP384R1;
        p->l = 48;
        break;
    case 4:
        p->id = MBEDTLS_ECP_DP_SECP521R1;
        p->l = 66;
        break;
    default:
        return false;
    }

    p->w = job->desc[2];
    p->f = job->desc[3];
    p->c = job->desc[4];
    p->d = job->desc[5];
    p->tmp = job->desc[6];
    p->ml = job->desc[7];
    return true;
}

static bool is_dsa_verify(uint32_t word)
{
    return CTYPE(word) == CTYPE_OPERATION && OP_TYPE(word) == OP_TYPE_UNI &&
           OP_ALG(word) == PROTID_DSAVERIFY;
}

static void add_in(struct job *job, uint32_t addr, uint32_t len)
{
    if (len == 0)
        return;
    assert_true(job->n_in < RANGES);
    job->in[job->n_in++] = (struct range){ addr, len, sum(addr, len) };
}

/* What the job reads: its descriptor first */
static void job_inputs(struct job *job)
{
    unsigned int i = first_cmd(job);
    struct cmd c;
    struct pdb p;

    add_in(job, job->addr, job->len * 4);

    while (next_cmd(job, &i, &c)) {
        if (c.pointer != 0)
            add_in(job, c.pointer, CTYPE(c.word) == CTYPE_STORE ? 0 : c.length);

        if (is_dsa_verify(c.word) && dsa_pdb(job, &p)) {
            add_in(job, p.w, 2 * p.l);
            add_in(job, p.f, p.ml);
            add_in(job, p.c, p.l);
            add_in(job, p.d, p.l);
        }
    }
}

/* Which of the descriptor's words are addresses: a command's pointer, and
 * the verification PDB's five */
static uint64_t job_addresses(const struct job *job)
{
    unsigned int i = first_cmd(job);
    uint64_t addresses = 0;
    struct cmd c;
    struct pdb p;

    while (next_cmd(job, &i, &c)) {
        unsigned int type = CTYPE(c.word);

        if ((type == CTYPE_LOAD || type == CTYPE_STORE || type == CTYPE_FIFO_LOAD) &&
            !(c.word & CMD_IMM))
            addresses |= 1ull << (c.index + 1);

        if (is_dsa_verify(c.word) && dsa_pdb(job, &p))
            addresses |= 0x7cull; /* Words 2 to 6 */
    }
    return addresses;
}

static struct caam_model_event *trace_next(void)
{
    if (m.trace == NULL)
        return NULL;
    if (m.traced >= m.trace_max)
        fail_msg("More than %u events traced", m.trace_max);
    return &m.trace[m.traced++];
}

static void job_check(const struct job *job)
{
    for (unsigned int n = 0; n < job->n_in; n++) {
        const struct range *r = &job->in[n];

        if (sum(r->addr, r->len) != r->sum)
            bench_violation(DMA,
                            "a job's %s was changed before the job was done",
                            (n == 0) ? "descriptor" : "data");
    }
}

static void job_out(struct job *job, uint32_t addr, const uint8_t *data, uint32_t len)
{
    struct out *o;

    assert_true(job->n_out < OUTS && len <= OUT_MAX);
    o = &job->out[job->n_out++];
    o->addr = addr;
    o->len = len;
    memcpy(o->data, data, len);
}

static uint32_t not_modeled(const struct cmd *c, uint32_t code)
{
    bench_violation(SRM "commands", "not modeled: 0x%08x at word %u", c->word, c->index);
    return DECO(c->index, code);
}

/* The MDHA: the context register is the running digest and, behind it, the
 * running message length. The length's coding is the model's own, bytes
 * and big endian: a driver saves and restores it and never reads it */
static uint32_t mdha(struct ccb *ccb, const struct cmd *c)
{
    const struct alg *a = ccb->alg;
    uint8_t *data;
    bool init = (ccb->as == AS_INIT || ccb->as == AS_INIT_FINAL);
    bool final = (ccb->as == AS_FINAL || ccb->as == AS_INIT_FINAL);
    uint8_t *ctx = ccb->ctx;
    uint64_t total;
    int rc = 0;

    if (a == NULL)
        return not_modeled(c, DECO_FIFO_LOAD);

    if (!final && (c->length % a->block) != 0) {
        bench_violation(SRM "MDHA",
                        "%s: %u bytes continue a hash, which takes whole blocks",
                        a->name,
                        c->length);
        return CAAM_STATUS_MDHA_SIZE | CAAM_STATUS_INDEX(c->index);
    }

    if (!init && ccb->ctx_loaded < a->running + 8)
        bench_violation(SRM "MDHA", "%s is continued without its running message length", a->name);

    total = get_be(&ctx[a->running], 8);

    data = malloc(c->length + 1);
    assert_non_null(data);
    ram_read(c->pointer, data, c->length);

    switch (a->sel) {
    case 0x40: {
        mbedtls_md5_context md5;

        mbedtls_md5_init(&md5);
        rc = mbedtls_md5_starts(&md5);
        if (!init) {
            for (unsigned int i = 0; i < 4; i++)
                md5.state[i] = ctx[4 * i] | (ctx[4 * i + 1] << 8) | (ctx[4 * i + 2] << 16) |
                               ((uint32_t)ctx[4 * i + 3] << 24);
            md5.total[0] = (uint32_t)total;
            md5.total[1] = (uint32_t)(total >> 32);
        }
        rc |= mbedtls_md5_update(&md5, data, c->length);
        if (final) {
            rc |= mbedtls_md5_finish(&md5, ctx);
        } else {
            for (unsigned int i = 0; i < 16; i++)
                ctx[i] = (uint8_t)(md5.state[i / 4] >> (8 * (i % 4)));
            total = md5.total[0] | ((uint64_t)md5.total[1] << 32);
        }
        break;
    }
    case 0x43: {
        mbedtls_sha256_context sha;

        mbedtls_sha256_init(&sha);
        rc = mbedtls_sha256_starts(&sha, 0);
        if (!init) {
            for (unsigned int i = 0; i < 8; i++)
                sha.state[i] = (uint32_t)get_be(&ctx[4 * i], 4);
            sha.total[0] = (uint32_t)total;
            sha.total[1] = (uint32_t)(total >> 32);
        }
        rc |= mbedtls_sha256_update(&sha, data, c->length);
        if (final) {
            rc |= mbedtls_sha256_finish(&sha, ctx);
        } else {
            for (unsigned int i = 0; i < 8; i++)
                put_be(&ctx[4 * i], sha.state[i], 4);
            total = sha.total[0] | ((uint64_t)sha.total[1] << 32);
        }
        break;
    }
    default: {
        mbedtls_sha512_context sha;

        mbedtls_sha512_init(&sha);
        rc = mbedtls_sha512_starts(&sha, a->sel == 0x44);
        if (!init) {
            for (unsigned int i = 0; i < 8; i++)
                sha.state[i] = get_be(&ctx[8 * i], 8);
            sha.total[0] = total;
            sha.total[1] = 0;
        }
        rc |= mbedtls_sha512_update(&sha, data, c->length);
        if (final) {
            rc |= mbedtls_sha512_finish(&sha, ctx);
        } else {
            for (unsigned int i = 0; i < 8; i++)
                put_be(&ctx[8 * i], sha.state[i], 8);
            total = sha.total[0];
        }
        break;
    }
    }

    free(data);
    assert_int_equal(rc, 0);

    if (!final)
        put_be(&ctx[a->running], total, 8);
    return 0;
}

static uint32_t dsa_verify(struct job *job, const struct cmd *c)
{
    static const uint8_t scratch[2 * EC_MAX] = { 0xa5 };
    uint8_t point[1 + 2 * EC_MAX];
    uint8_t c_d[2][EC_MAX];
    uint8_t f[CONTEXT_SIZE];
    mbedtls_ecp_group grp;
    mbedtls_ecp_point q;
    mbedtls_mpi r, s;
    struct pdb p;
    int rc;

    if (!(c->word & PROT_ECC) || PROT_MES_REP(c->word) != MES_REP_HASHED || !dsa_pdb(job, &p))
        return not_modeled(c, DECO_PROTOCOL);

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&q);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    /* The key is Wx and Wy, L bytes each */
    point[0] = 0x04;
    ram_read(p.w, &point[1], 2 * p.l);
    ram_read(p.c, c_d[0], p.l);
    ram_read(p.d, c_d[1], p.l);
    assert_true(p.ml <= sizeof(f));
    ram_read(p.f, f, p.ml);

    assert_int_equal(mbedtls_ecp_group_load(&grp, p.id), 0);
    assert_int_equal(mbedtls_mpi_read_binary(&r, c_d[0], p.l), 0);
    assert_int_equal(mbedtls_mpi_read_binary(&s, c_d[1], p.l), 0);

    rc = mbedtls_ecp_point_read_binary(&grp, &q, point, 1 + 2 * p.l);

    if (rc == 0)
        rc = mbedtls_ecp_check_pubkey(&grp, &q);

    /* The leftmost bits of the hashed message, as FIPS 186-4 6.4 has it */
    if (rc == 0)
        rc = mbedtls_ecdsa_verify(&grp, f, p.ml, &q, &r, &s);

    mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&r);
    mbedtls_ecp_point_free(&q);
    mbedtls_ecp_group_free(&grp);

    /* The engine works in the temporary buffer, 2L bytes */
    job_out(job, p.tmp, scratch, 2 * p.l);

    return (rc == 0) ? 0 : (CAAM_STATUS_INVALID_SIG | CAAM_STATUS_INDEX(c->index));
}

static uint32_t operation(struct job *job, struct ccb *ccb, const struct cmd *c)
{
    if (is_dsa_verify(c->word))
        return dsa_verify(job, c);

    if (OP_TYPE(c->word) != OP_TYPE_CLASS2 || OP_AAI(c->word) != 0)
        return not_modeled(c, DECO_OPERATION);

    ccb->alg = NULL;

    for (size_t n = 0; n < sizeof(algs) / sizeof(algs[0]); n++) {
        if (algs[n].sel == OP_ALG(c->word))
            ccb->alg = &algs[n];
    }

    if (ccb->alg == NULL)
        return not_modeled(c, DECO_OPERATION);

    /* The low power MDHA has no SHA-384 and no SHA-512 */
    if (m.cfg.lp256 && ccb->alg->block == 128)
        return CAAM_STATUS_MDHA_MODE | CAAM_STATUS_INDEX(c->index);

    ccb->as = OP_AS(c->word);
    return 0;
}

/* LOAD into and STORE from the class 2 context register */
static uint32_t context(struct job *job, struct ccb *ccb, const struct cmd *c)
{
    bool load = (CTYPE(c->word) == CTYPE_LOAD);
    uint32_t offset = LS_OFFSET(c->word);

    if (CMD_CLASS(c->word) != 2 || (c->word & (CMD_SGF | CMD_IMM)) ||
        LS_REG(c->word) != REG_CONTEXT)
        return not_modeled(c, load ? DECO_LOAD : DECO_STORE);

    if (offset + c->length > CONTEXT_SIZE)
        return DECO(c->index, DECO_CONTEXT_LEN);

    if (load) {
        ram_read(c->pointer, &ccb->ctx[offset], c->length);
        ccb->ctx_loaded = offset + c->length;
    } else {
        job_out(job, c->pointer, &ccb->ctx[offset], c->length);
    }
    return 0;
}

static void job_run(struct job *job)
{
    struct ccb ccb;
    unsigned int i = first_cmd(job);
    struct cmd c;

    memset(&ccb, 0, sizeof(ccb));
    job->status = 0;

    if (m.fail_next) {
        m.fail_next = false;
        job->status = m.fail_status;
        return;
    }

    if (job->bad) {
        job->status = DECO(0, DECO_HEADER);
        return;
    }

    while (job->status == 0 && next_cmd(job, &i, &c)) {
        switch (CTYPE(c.word)) {
        case CTYPE_OPERATION:
            job->status = operation(job, &ccb, &c);
            break;
        case CTYPE_LOAD:
        case CTYPE_STORE:
            job->status = context(job, &ccb, &c);
            break;
        case CTYPE_FIFO_LOAD:
            if (CMD_CLASS(c.word) != 2 || (c.word & (CMD_SGF | CMD_IMM)) ||
                FL_TYPE(c.word) != FL_MSG_LAST2)
                job->status = not_modeled(&c, DECO_FIFO_LOAD);
            else
                job->status = mdha(&ccb, &c);
            break;
        default:
            job->status = not_modeled(&c, DECO_INVALID_CMD);
        }
    }

    /* A job that failed stores nothing */
    if (job->status != 0)
        job->n_out = 0;
}

/* The job leaves the engine: now its data is read and its results are
 * written, see caam_model.h */
static void job_complete(struct job *job)
{
    job_check(job);
    job_run(job);

    for (unsigned int n = 0; n < job->n_out; n++) {
        const struct out *o = &job->out[n];

        dcache_dev_write(o->addr, o->data, o->len);

        /* What a later job reads of it is that job's data as it is now */
        for (struct job *later = job + 1; later < &m.jobs[m.n_jobs]; later++) {
            for (unsigned int k = 1; k < later->n_in; k++) {
                struct range *r = &later->in[k];

                if (r->addr < o->addr + o->len && o->addr < r->addr + r->len)
                    r->sum = sum(r->addr, r->len);
            }
        }
    }

    job->done = true;
    m.jobs_done++;
}

/* A descriptor's address leaves the input ring, and the descriptor is read */
static void job_take(uint64_t now)
{
    struct job *job = &m.jobs[m.n_jobs++];
    uint32_t hdr;

    memset(job, 0, sizeof(*job));
    job->addr = dcache_dev_read_32(m.irbar + 4 * m.rindex);
    job->taken_at = now;
    m.rindex = (m.rindex + 1) % m.irsr;
    m.in_ring--;
    m.irsar++;

    hdr = (job->addr != 0) ? dcache_dev_read_32(job->addr) : 0;
    job->len = HDR_LEN(hdr);

    if (CTYPE(hdr) != CTYPE_JOB_HEADER || !(hdr & HDR_ONE) || (hdr & ~HDR_KNOWN) || job->len < 2 ||
        job->len > DESC_WORDS || HDR_START(hdr) >= job->len) {
        bench_violation(SRM "job descriptor",
                        "0x%08x at 0x%08x is no job descriptor header the model knows",
                        hdr,
                        job->addr);
        job->bad = true;
        job->len = 0;
        return;
    }

    ram_read(job->addr, job->desc, job->len * 4);
    job_inputs(job);

    struct caam_model_event *e = trace_next();

    if (e != NULL) {
        memset(e, 0, sizeof(*e));
        e->job = true;
        e->value = job->addr;
        e->len = job->len;
        memcpy(e->words, job->desc, job->len * 4);
        e->addresses = job_addresses(job);
    }
}

static void advance(void)
{
    uint64_t now = bench_now_us();
    bool progress;

    if (m.hang)
        return;

    do {
        progress = false;

        while (m.in_ring > 0 && m.n_jobs < JOBS_MAX) {
            job_take(now);
            progress = true;
        }

        /* The engine has one job at a time, the oldest that isn't done */
        for (unsigned int n = 0; n < m.n_jobs; n++) {
            struct job *job = &m.jobs[n];

            if (job->done)
                continue;

            if (!job->started) {
                job->started = true;
                job->done_at = (m.free_at > job->taken_at ? m.free_at : job->taken_at) + m.job_us;
            }

            if (now >= job->done_at) {
                job_complete(job);
                m.free_at = job->done_at;
                progress = true;
            }
            break;
        }

        /* A completion waits for a free entry of the output ring */
        if (m.n_jobs > 0 && m.jobs[0].done && m.orsfr < m.orsr) {
            uint32_t entry = m.orbar + 8 * m.windex;

            if (m.earlier_ring) {
                /* Not the driver's memory, nor its cache's business */
                ((uint32_t *)(uintptr_t)entry)[0] = m.jobs[0].addr;
                ((uint32_t *)(uintptr_t)entry)[1] = m.jobs[0].status;
            } else {
                dcache_dev_write_32(entry, m.jobs[0].addr);
                dcache_dev_write_32(entry + 4, m.jobs[0].status);
            }
            m.jrstar = m.jobs[0].status;
            m.windex = (m.windex + 1) % m.orsr;
            m.orsfr++;
            m.n_jobs--;
            memmove(&m.jobs[0], &m.jobs[1], m.n_jobs * sizeof(m.jobs[0]));
            /* An empty engine starts the next job when it is taken, also
             * after the test set the clock back (bench_reset()) */
            if (m.n_jobs == 0)
                m.free_at = 0;
            progress = true;
        }
    } while (progress);
}

void caam_model_init(const struct caam_model_config *cfg)
{
    memset(&m, 0, sizeof(m));
    m.cfg = *cfg;
    m.job_us = JOB_US;
    /* pb's Mbed TLS configuration has no allocator of its own */
    mbedtls_platform_set_calloc_free(calloc, free);
}

bool caam_model_owns(uintptr_t addr)
{
    return addr >= m.cfg.jr_base && addr - m.cfg.jr_base < PAGE_SIZE;
}

uint32_t caam_model_read(uintptr_t addr)
{
    uint32_t off = (uint32_t)(addr - m.cfg.jr_base);

    bench_advance_us(1);
    bench_check_stuck("the CAAM is polled");
    advance();

    switch (off) {
    case IRBAR_MS:
    case ORBAR_MS:
    case JRINTR:
        return 0;
    case IRBAR_LS:
        return m.irbar;
    case IRSR:
        return m.irsr;
    case IRSAR:
        return m.irsar;
    case ORBAR_LS:
        return m.orbar;
    case ORSR:
        return m.orsr;
    case ORSFR:
        return m.orsfr;
    case JRSTAR:
        return m.jrstar;
    case JRCFGR_MS:
    case JRCFGR_LS:
        return m.jrcfgr[(off - JRCFGR_MS) / 4];
    case CCBVID:
        return (uint32_t)m.cfg.era << 24;
    case CHAVID_LS:
        /* MDVID: the low power MDHA up to SHA-256, or the high performance one */
        return (m.cfg.lp256 ? 0u : 3u) << 12;
    case CAAMVID_MS:
        return 0x0a160300;
    default:
        bench_violation(SRM "registers", "not modeled: read of 0x%03x", off);
        return 0;
    }
}

void caam_model_write(uintptr_t addr, uint32_t value)
{
    uint32_t off = (uint32_t)(addr - m.cfg.jr_base);
    struct caam_model_event *e;

    advance();

    e = trace_next();

    if (e != NULL) {
        memset(e, 0, sizeof(*e));
        e->reg = off;
        e->value = value;
        e->addresses = (off == IRBAR_LS || off == ORBAR_LS) ? 1 : 0;
    }

    switch (off) {
    case IRBAR_MS:
    case ORBAR_MS:
        if (value != 0)
            bench_violation(SRM "registers", "not modeled: a ring above 4 GiB");
        break;
    case IRBAR_LS:
    case IRSR:
    case ORBAR_LS:
    case ORSR:
        /* JRINTR's error types: an invalid write, a bad base address */
        if (m.in_ring || m.n_jobs || m.orsfr)
            bench_violation(SRM "JRINTR", "a ring's base or size written with a job in the ring");
        if ((off == IRBAR_LS || off == ORBAR_LS) && (value & 3))
            bench_violation(SRM "JRINTR", "a ring's base isn't on a 4 byte boundary");

        if (off == IRBAR_LS) {
            m.irbar = value;
        } else if (off == ORBAR_LS) {
            m.orbar = value;
            m.earlier_ring = false;
        } else if (off == IRSR) {
            m.irsr = value & RING_SIZE_MASK;
            m.irsar = m.irsr;
            m.rindex = 0;
        } else {
            m.orsr = value & RING_SIZE_MASK;
            m.windex = 0;
        }
        break;
    case IRJAR:
        if (value > m.irsar) {
            bench_violation(SRM "JRINTR",
                            "%u jobs added to an input ring with %u slots available",
                            value,
                            m.irsar);
            value = m.irsar;
        }
        m.irsar -= value;
        m.in_ring += value;
        break;
    case ORJRR:
        if (value > m.orsfr) {
            bench_violation(
                SRM "JRINTR", "%u jobs removed from an output ring that holds %u", value, m.orsfr);
            value = m.orsfr;
        }
        m.orsfr -= value;
        break;
    case JRINTR:
        break;
    case JRCFGR_MS:
    case JRCFGR_LS:
        m.jrcfgr[(off - JRCFGR_MS) / 4] = value;
        break;
    default:
        bench_violation(SRM "registers", "not modeled: write of 0x%08x to 0x%03x", value, off);
    }

    advance();
}

void caam_model_job_time(uint32_t us)
{
    m.job_us = us;
}

void caam_model_hang(bool hang)
{
    m.hang = hang;
}

void caam_model_fail_next_job(uint32_t status)
{
    m.fail_next = true;
    m.fail_status = status;
}

void caam_model_foreign_completion(uint32_t status)
{
    /* Its descriptor is nobody's now */
    static uint32_t descriptor[2];
    struct job *job;

    assert_true(m.n_jobs < JOBS_MAX);
    job = &m.jobs[m.n_jobs++];
    memset(job, 0, sizeof(*job));
    job->addr = (uint32_t)(uintptr_t)descriptor;
    job->status = status;
    job->started = true;
    job->done = true;
    advance();
}

void caam_model_earlier_stage(uint32_t *ring, uint32_t status)
{
    m.irsr = 1;
    m.irsar = 1;
    m.orbar = (uint32_t)(uintptr_t)ring;
    m.earlier_ring = true;
    m.orsr = 1;
    m.windex = 0;
    caam_model_foreign_completion(status);
}

void caam_model_trace(struct caam_model_event *events, unsigned int max)
{
    m.trace = events;
    m.trace_max = max;
    m.traced = 0;
}

unsigned int caam_model_traced(void)
{
    return m.traced;
}

unsigned int caam_model_jobs_done(void)
{
    return m.jobs_done;
}

bool caam_model_idle(void)
{
    advance();
    return m.in_ring == 0 && m.n_jobs == 0 && m.orsfr == 0;
}
