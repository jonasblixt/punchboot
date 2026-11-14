# The crypto backend contract

**State: reviewed (Jonas, 2026-11-12), D-064, D-071, D-073.** Level 3 (`process.md`).

What pb's hashing and signature verification (`include/pb/crypto.h`, `src/crypto.c`) give their
users, and what a backend that implements them provides: a SoC's crypto engine or a software
library. Image verification and command mode's token rest on it (`security.md` 4.1 to 4.3,
4.7), so its first rule is that a verdict is never wrong in the direction of yes. It builds on
`drivers.md`. Vendor neutral: where each backend stands is section 9.

Changing `crypto`, a backend or a user of either: read this, `drivers.md` and `security.md` 4,
then go through the checklist (section 8).

**Not yet in the code (F-nnn)** marks what the code doesn't do yet. The commit that fixes the
finding removes the marker and updates section 9.

**No open items.** The draft's five, as decided in Jonas's review (D-071) and written into
the text:

| Item | Rule                                                                              | By    |
| ---- | --------------------------------------------------------------------------------- | ----- |
| 1    | No abort to call: `hash_init()` starts from an idle engine, whatever was left     | Jonas |
|      | (3.1)                                                                             |       |
| 2    | The verdict's two channels mean the same: PB_OK if and only if `*verified`. A     | Jonas |
|      | signature that doesn't verify is `-PB_ERR_SIGNATURE` (4)                          |       |
| 3    | `dsa_verify()` while a hash is open is refused, `-PB_ERR_STATE` (4)               | Jonas |
| 4    | A build may have several backends, each algorithm from one of them: a hardware    | Jonas |
|      | backend for the hashes and a software one for the signatures. No fallback at run  |       |
|      | time (2)                                                                          |       |
| 5    | `HASH_MD5_BROKEN` stays (5)                                                       | Jonas |

## 1. Words and scope

| Word    | Means                                                                                |
| ------- | ------------------------------------------------------------------------------------ |
| Backend | An implementation of the hash operations, of signature verification, or of both      |
| Context | The one hash that is open, from `hash_init()` to `hash_final()`                      |
| Verdict | Whether a signature verifies, for one key and one digest                             |
| User    | Whoever calls `hash_*()` or `dsa_verify()`: the boot, command mode, the device's     |
|         | identity                                                                             |

What pb uses it for:

| Use                   | Hash                               | Signature          | Where                 |
| --------------------- | ---------------------------------- | ------------------ | --------------------- |
| An image's header     | The header's own: SHA-256, -384 or | The keystore key's | `boot.md` 4.2         |
|                       | -512                               | curve              |                       |
| An image's payload    | The same, over every part as it is |                    | `boot.md` 4.2         |
|                       | loaded                             |                    |                       |
| Command mode's token  | The key's curve gives it: SHA-256, | The keystore key's | `command-mode.md` 3.3 |
|                       | -384, -512                         | curve              |                       |
| `part verify`         | SHA-256                            |                    | `command-mode.md` 5.3 |
| The device's identity | MD5, as a UUID version 3 needs it  |                    | `platform.md` 3.7     |

Not here: which key is used and whether it is revoked (`rot.c`, `security.md` 4.3, the
core's), the keystore's format, the image format's layout (`boot.md` 4.4), the hardware RNG
(the platform's duty to the next stage, `platform.md` 4.5; verification needs no random numbers),
encryption, which pb doesn't do.

pb verifies and never signs: a backend holds no secret, and nothing here needs constant time.

## 2. Backends

A backend registers its operations when it is set up: `hash_add_ops()`, `dsa_add_ops()`, each
with the algorithms it has (`alg_bits`).

- **Set up by the platform**, in `plat_init()`, each behind its Kconfig option
  (`platform.md` 2.2). The board's defconfig says which backends the build has.
- **A backend that failed isn't registered.** An algorithm no backend has is
  `-PB_ERR_NOT_SUPPORTED` from `hash_init()` and `dsa_verify()`, and a check that can't be made
  is a no (`security.md` 4.4). A failed verification engine refuses the boot before the load
  (`platform.md` 2.3, `security.md` 4.5).
- **A build may have several backends, and each algorithm comes from one of them** (Jonas,
  D-071). The hashes and the signatures are registered apart, so an engine can give the
  accelerated hashes while a software backend verifies the signatures, or the other way
  round. A backend's options say which of its algorithms it registers. Today no build in
  the tree has two backends.
- **No fallback at run time** (Jonas): an algorithm has one backend, chosen by the build. A
  fallback would be a second implementation that must verify the same, and `security.md` 4.5
  asks that of one before it may stand in. An algorithm that two backends register is a
  mistake of the build: the second registration is refused.
- **Together the build's backends have every algorithm its keys and images use**: the
  header's hash, the keystore keys' curves, SHA-256 for `part verify`, MD5 for the identity.
  No single backend needs them all. One that lacks an algorithm on some hardware (an engine
  variant without the long hashes) doesn't register it there, so that the answer is
  `-PB_ERR_NOT_SUPPORTED` and not a wrong digest.
- **Software is a backend like any other** and passes the same tests (section 9).

## 3. Hashing

One context at a time. `hash_init()` opens it, `hash_final()` closes it, and between the two
the updates may be of any kind and mixed. A call without a context is `-PB_ERR_STATE`.

### 3.1 Function : hash_init() [mandatory]

```
Argument : hash_t alg
Return   : int, PB_OK or -PB_ERR_*
```

Opens a context for one algorithm.

- **It always starts clean** (Jonas, D-071). Whatever was open is ended: a job that is in
  flight is waited for or cancelled, and nothing of it completes into the new context or into
  a later verification (F-181). A user that gives a hash up on an error calls nothing; the
  next `hash_init()` is the cleanup. Until then the context counts as open, and a
  verification is refused (section 4), which is a no; every verification hashes first, so it
  starts with a `hash_init()`.
- **When it fails there is no context**: the next update is `-PB_ERR_STATE`.
- `-PB_ERR_NOT_SUPPORTED`: no backend has the algorithm. `-PB_ERR_PARAM`: `alg` isn't one
  algorithm.

### 3.2 Function : hash_update() [mandatory]

```
Argument : const void *buf, size_t length
Return   : int, PB_OK or -PB_ERR_*
```

Adds `length` bytes to the hash. When it returns, the backend needs `buf` no more.

- Any length, 0 too, and any alignment: a backend that hashes whole blocks keeps the rest
  itself. The digest is that of the bytes in the order of the calls, however they were split.
- The buffer is read only (`drivers.md` 4.3, out).

### 3.3 Function : hash_update_async() [optional]

```
Argument : const void *buf, size_t length
Return   : int, PB_OK or -PB_ERR_*
```

As `hash_update()`, but it may return before the bytes are hashed, so that the boot reads the
next chunk while the engine hashes this one. Without it the core calls `update`.

- **`buf` stays valid and unchanged until the next `hash_*()` call returns**, any of them,
  `hash_final()` and the next `hash_init()` included. Each of them waits for the job first.
- **An error of the job is returned by the call that waits for it**, and the context is then
  failed: every later update and the final return an error, never a digest.
- The image's memory is the usual buffer: a chunk is hashed where it was loaded, and the next
  chunk goes behind it.

### 3.4 Function : hash_copy_update() [optional]

```
Argument : const void *src, void *dest, size_t length
Return   : int, PB_OK or -PB_ERR_*
```

Copies `length` bytes from `src` to `dest` and hashes them: what is hashed is what was
written to `dest`. Without it the core copies, then hashes `dest`. No backend has it and
nothing in the tree calls it; it stays (`principles.md` 7).

### 3.5 Function : hash_final() [mandatory]

```
Argument : uint8_t *digest_out, size_t length
Return   : int, PB_OK or -PB_ERR_*
```

Waits for what is in flight, writes the digest and closes the context, whatever it returns.

- **`length` is the room at `digest_out`.** Less than the algorithm's digest is
  `-PB_ERR_BUF_TOO_SMALL` and nothing is written; with more, the digest's bytes are written
  and the rest is left. `CRYPTO_MD_MAX_SZ` (64) fits every algorithm.
- **The digest reaches `digest_out` by a copy**, not by the engine's own write: users have it
  on the stack (`drivers.md` 4.3).
- **On an error `digest_out` is not a digest** and the user doesn't compare it.

## 4. Function : dsa_verify() [mandatory]

```
Argument : dsa_t alg, const uint8_t *der_signature, size_t signature_length,
           const uint8_t *der_key, size_t key_length,
           hash_t md_alg, uint8_t *md, size_t md_length, bool *verified
Return   : int, PB_OK or -PB_ERR_*
```

Verifies an ECDSA signature over a digest with a public key.

**The verdict** (Jonas, D-071).

- **`*verified` is false until the signature verified.** It is set to false before anything
  else, and to true in one place: where this call's own computation said the signature is
  valid for this key and this digest.
- **PB_OK if and only if `*verified` is true.** Users check both (`principles.md` 2).
- **A signature that doesn't verify is `-PB_ERR_SIGNATURE`.** Everything else is the code of
  what failed: `-PB_ERR_ASN1` for a signature or a key that can't be parsed,
  `-PB_ERR_NOT_SUPPORTED` for a curve no backend has, `-PB_ERR_TIMEOUT` and `-PB_ERR_IO` for
  the engine. A user treats every one of them as "no"; the difference is for the log.
- **The verdict is this call's.** A backend whose engine reports completions never takes a
  completion that another job left behind as this job's: it starts from an idle engine and
  matches the completion to what it submitted (`drivers.md` 5).

The arguments:

- **The signature** is DER, an `ECDSA-Sig-Value` (two INTEGERs), as the image's header and the
  token carry it. It comes from the image or the host: every length in it is checked against
  `signature_length`, and against the curve's size (`drivers.md` 5).
- **The key** is DER, a `SubjectPublicKeyInfo`, from the keystore, which is part of pb. It is
  parsed inside `key_length` all the same (`principles.md` 2).
- **`alg` is the key's curve as the keystore names it.** A key whose own curve is another one
  is refused, `-PB_ERR_PARAM`: the keystore and the key must agree. **Not yet in the code
  (F-286)**: `alg` only selects the backend.
- **`md` and `md_length`** are the digest and its length. `md_length` is exactly the digest
  size of `md_alg`; anything else is `-PB_ERR_PARAM`, from the core (Jonas, D-073). A backend
  uses the leftmost bits the curve takes, as ECDSA defines. `md` is read only and may be
  anywhere. **Not yet in the code (F-286)**: nothing checks the length, and command mode's
  token passes 64 whatever the hash. The two are fixed in one commit, and the tokens that
  verified are tried on both backends first: which ones verify is fixed
  (`command-mode.md` 7).
- **Not while a hash is open** (Jonas, D-071): `-PB_ERR_STATE`, from the core, whichever
  backends the build has.

## 5. Algorithms

| Algorithm                   | For                               | Note                               |
| --------------------------- | --------------------------------- | ---------------------------------- |
| SHA-256, SHA-384, SHA-512   | Images, the token, `part verify`  | The image format's hash kinds      |
|                             |                                   | (`boot.md` 4.4)                    |
| ECDSA on P-256, P-384 and   | Images, the token                 | The keystore's key kinds           |
| P-521                       |                                   |                                    |
| MD5                         | The device's identity             | Not a security use: a UUID version |
|                             | (`platform.md` 3.7)               | 3 is defined with it               |
| `HASH_MD5_BROKEN`           | The identity of products in the   | Stays                              |
|                             | field                             |                                    |

- **MD5 is in every build**: the device's UUID is computed with it, and the UUID never changes
  (`system.md` 5.5). No verification uses it, and no image or key can ask for it: the image
  format's hash kinds don't name it.
- **`HASH_MD5_BROKEN` stays** (Jonas, D-071). The engine's backend has a second MD5 with a
  shortened running context, "compatible with some versions already in the field". Nothing
  in the tree asks for it: a product whose devices got their UUIDs from it needs it for good
  (`principles.md` 7).
- **A new algorithm is added**, with its bit in `hash_t` or `dsa_t`; none is changed or
  reused. Which algorithms an image may name is the image format's (`compatibility.md`).

## 6. The engine at the jump

- **No job is in flight** when the boot jumps. The boot's last use of a backend is a
  verification that ended, and `hash_final()` and `dsa_verify()` return only when their work
  is done. A hash that was given up before that was ended by the `hash_init()` of the
  verification that followed (3.1).
- **The engine holds no address in pb's memory that it would write to** (`drivers.md` 2.3). A
  backend with rings or queues the engine keeps has a `stop` that takes them away.
- **The engine is handed over as the next stage expects it** (`platform.md` 4.5): which of its
  interfaces belong to the secure world and which to the normal one is the platform's, in its
  port's document.

## 7. Fixed (compatibility)

- What a digest and a verdict are is the standards': FIPS 180-4, FIPS 186-4, RFC 1321. Every
  backend gives the same answer for the same input, so every image and every token a release
  accepted is accepted (D-008).
- The device's identity: MD5 over the same bytes (`platform.md` 3.7), and the broken variant
  where a product has it.
- The formats `dsa_verify()` takes: they are the image's and the keystore's
  (`compatibility.md`).

`struct hash_ops`, `struct dsa_ops` and the functions are the porting API: they change with a
major release at most (D-018).

## 8. Review checklist

For a new backend, and for a change to one, to `crypto.c` or to a user of it:

- [ ] `drivers.md` 7.
- [ ] `*verified` is set to false first and to true in one place, and PB_OK is returned only
      there.
- [ ] No completion, status or digest of an earlier operation can be read as this one's:
      `init` and `verify` start from an idle engine.
- [ ] After any error the next operation gives the right answer (the tests give a hash up at
      every step, then verify).
- [ ] Every length in a signature is checked against its buffer and the curve.
- [ ] `final` writes no more than `length` and the digest only; the digest is copied to the
      user.
- [ ] An asynchronous update is waited for by every later call; its buffer isn't touched
      before.
- [ ] An algorithm the hardware lacks isn't registered.
- [ ] The known answer tests pass for every algorithm the backend registers (section 9).
- [ ] A user checks the return value and `*verified`, and uses no digest of a failed
      `hash_final()`.

## 9. Conformance

Where each backend is (2026-11-12). Read in the code, not run, unless a test is named.

| Requirement                          | CAAM                    | Mbed TLS             | Finding      |
| ------------------------------------ | ----------------------- | -------------------- | ------------ |
| Set up by the platform               | yes (`test_plat_*`)     | yes (`test_plat_*`)  |              |
| `hash_init()` starts clean           | yes                     | yes                  |              |
|                                      | (`test_caam_crypto`)    |                      |              |
| Any length and split                 | yes (the suite)         | yes (the suite)      |              |
| The asynchronous update is waited    | yes                     | n/a (none)           |              |
| for                                  | (`test_caam_crypto`)    |                      |              |
| `final` respects `length`            | yes                     | yes (`test_mbedtls`) |              |
|                                      | (`test_caam_crypto`)    |                      |              |
| The digest is copied to the user     | yes                     | yes (no DMA)         |              |
|                                      | (`test_caam_crypto`)    |                      |              |
| `*verified` false first              | yes                     | yes                  |              |
| A bad signature is                   | yes                     | yes (`test_mbedtls`) |              |
| `-PB_ERR_SIGNATURE`                  | (`test_caam_crypto`)    |                      |              |
| The verdict is this call's           | yes                     | yes                  |              |
|                                      | (`test_caam_crypto`)    |                      |              |
| The signature's lengths checked      | partly: the outer       | yes (the library)    | F-286        |
|                                      | length is ignored       |                      |              |
| The key parsed inside its length     | yes (`test_der`)        | yes (the library)    |              |
| The key's curve is `alg`             | no                      | no                   | F-286        |
| Every wait bounded                   | yes (1 s)               | n/a                  |              |
| Nothing left at the jump             | yes (`test_plat_*`),    | n/a                  | F-286        |
|                                      | not run on a board      |                      |              |
| The library's results checked        | n/a                     | yes                  |              |
|                                      |                         | (`test_mbedtls`)     |              |
| An algorithm the hardware lacks      | yes (the short engine)  | yes (Kconfig)        |              |
| isn't registered                     |                         |                      |              |
| Known answer tests                   | yes (the suite)         | yes (the suite)      |              |

The core:

| Requirement                                      | Today                             | Finding |
| ------------------------------------------------ | --------------------------------- | ------- |
| An algorithm is registered once                  | yes (`test_crypto`)               |         |
| No context after a failed `hash_init()`          | yes (`test_crypto`)               |         |
| `dsa_verify()` is refused while a hash is open   | yes (`test_crypto`)               |         |
| `md_length` is the digest size of `md_alg`       | no: not checked                   | F-286   |

**The conformance suite** (`process.md` 7, `tests/unit/conformance/crypto_conformance.h`): one
set of tests for sections 3 and 4, run against every backend: known answers for each algorithm (the
standards' vectors, lengths around the block size, every split), the verdict's cases (a valid
signature, one bit changed in the signature, in the digest and in the key, malformed DER, a
wrong curve), and the states (a hash given up at each step, then a verification that must
fail and one that must pass). Mbed TLS runs it on the host. The CAAM runs it on the host
against a model of its job ring that executes the descriptors, and on the bench through the
simple target (D-025): F-181 was found in such a model, not on hardware. **Not yet in the code
(F-286)**: the run on the bench.

## 10. Not yet in the code

| Finding      | What                                                                            |
| ------------ | ------------------------------------------------------------------------------- |
| F-286        | The core and the backends against sections 3, 4 and 6; the suite on the bench   |
