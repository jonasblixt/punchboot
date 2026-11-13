/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A model of the NXP CAAM as punchboot uses it: one job ring, the message
 * digest accelerator (MDHA) and the ECDSA signature verification protocol
 * on the built-in curves. Written from the CAAM chapters of the i.MX6UL and
 * the i.MX8M Mini security reference manuals (SRM), which are under NDA:
 * they are named here, never quoted.
 *
 * The test routes fake_mmio_read/write (fakes/mmio) for the job ring's page
 * here. The model takes jobs from the input ring, runs their descriptors and
 * puts their results in the output ring, and logs what a driver may not do
 * as violations (bench.h):
 *
 *  - more jobs added than the input ring has slots, more removed than the
 *    output ring holds
 *  - the rings' base and size written while a job is in them
 *  - a descriptor or the data of a job changed before the job was done:
 *    the engine reads them at any time until then (drivers.md 4.2)
 *  - a hash that is continued with data that isn't whole blocks, or without
 *    its running message length
 *  - what the model doesn't know: other commands, algorithms and registers
 *
 * Time is simulated (bench.h): a job takes 20 us in the engine unless the
 * test says otherwise, and every register read costs 1 us. The engine runs
 * one job at a time, in the order they were added. A job's descriptor is
 * read when it leaves the input ring; its data is read, and its results are
 * written, when it is done: the last moment the manual allows, so that a
 * driver that touches a job's memory before it saw the completion is caught
 * with both the violation and the effect. A completion waits for a free
 * entry of the output ring, and JRSTAR holds the status of the last
 * completion that was written there.
 *
 * Digests and verdicts are computed with Mbed TLS.
 *
 * Where the model follows the silicon, not the manual:
 *
 *  - A hash that starts (AS = INIT) with no data is run. The manual asks
 *    for a non-zero number of blocks; punchboot starts every hash of less
 *    than a block that way, on every board
 *  - The verification's PDB is the one punchboot and NXP's OP-TEE driver
 *    use: PD in bit 22 of the first word, then the pointers to the key, the
 *    hashed message, c, d and the temporary buffer, then the message
 *    length. The i.MX8M Mini SRM's table puts PD elsewhere and has no
 *    temporary buffer
 *
 * Not known, and so not modeled: what the engine does with a public key
 * that isn't a point of its curve (here: the signature is invalid), and how
 * a running context without its message length continues (HASH_MD5_BROKEN;
 * here: from a length of 0, with a violation).
 *
 * Not modeled: the RNG (test_imx_caam.c has a model of its own), the job
 * ring's flush and reset, interrupts, scatter/gather tables, shared
 * descriptors, more than one job ring.
 *
 * The addresses in a ring and in a descriptor are 32 bits: tests link with
 * -no-pie and use static buffers.
 */

#ifndef TESTS_UNIT_MODELS_CAAM_MODEL_H
#define TESTS_UNIT_MODELS_CAAM_MODEL_H

#include <stdbool.h>
#include <stdint.h>

/* Job termination status words */
#define CAAM_STATUS_INVALID_SIG  0x40000086u /* DECO: the signature is invalid */
#define CAAM_STATUS_DECO_WDOG    0x4000001cu /* DECO: watchdog timeout */
#define CAAM_STATUS_MDHA_MODE    0x20000041u /* CCB, MDHA: mode error */
#define CAAM_STATUS_MDHA_SIZE    0x20000042u /* CCB, MDHA: data size error */
#define CAAM_STATUS_INDEX(index) ((uint32_t)(index) << 8) /* The descriptor's word */

struct caam_model_config {
    uintptr_t jr_base; /* The job ring's page, 4 KiB */
    unsigned int era;
    bool lp256; /* The low power MDHA, as on the i.MX6UL: no SHA-384/512 */
};

/* A CAAM after reset: no ring, no job */
void caam_model_init(const struct caam_model_config *cfg);

bool caam_model_owns(uintptr_t addr);
uint32_t caam_model_read(uintptr_t addr);
void caam_model_write(uintptr_t addr, uint32_t value);

/* How long a job is in the engine */
void caam_model_job_time(uint32_t us);

/* While set no job is taken or completed, as when the security state
 * machine has left 'Trusted' */
void caam_model_hang(bool hang);

/* The next job that is run does nothing and ends with this status */
void caam_model_fail_next_job(uint32_t status);

/* A completion that isn't of a job the driver added: one of an earlier user
 * of the job ring, whose job ends now. It goes into the output ring behind
 * the completions of the jobs that are in the engine, when there is room */
void caam_model_foreign_completion(uint32_t status);

/* An earlier stage used the job ring, with an output ring of one entry at
 * 'ring' (two words), and left a completion in it. Before the driver's init */
void caam_model_earlier_stage(uint32_t *ring, uint32_t status);

/* Jobs that are done, since caam_model_init() */
unsigned int caam_model_jobs_done(void);

/* What the driver did, as the engine saw it, for a test that pins it: the
 * job ring's register writes and the descriptor of each job the engine took,
 * in order. A word that is an address is marked, so that a test can name the
 * buffer instead of the number */
#define CAAM_MODEL_DESC_WORDS 64

struct caam_model_event {
    bool job; /* A job's descriptor, else a register write */
    uint32_t reg; /* A write: the register's offset in the job ring's page */
    uint32_t value; /* A write: the value. A job: the descriptor's address */
    unsigned int len; /* A job: the descriptor's length, in words */
    uint32_t words[CAAM_MODEL_DESC_WORDS];
    uint64_t addresses; /* Bit n: word n is an address. A write: bit 0, the value */
};

/* Starts recording into 'events', 'max' of them; NULL stops */
void caam_model_trace(struct caam_model_event *events, unsigned int max);

/* Events recorded since caam_model_trace(). More than 'max' fails the test */
unsigned int caam_model_traced(void);

/* No job is in the input ring or in the engine, and no completion is in
 * the output ring */
bool caam_model_idle(void);

#endif // TESTS_UNIT_MODELS_CAAM_MODEL_H
