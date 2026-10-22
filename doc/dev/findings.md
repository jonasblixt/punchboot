# Findings

Bugs and tech debt from the 2026-10-22 repo scan. **Verified** means a human or
Claude read the code and confirmed it. **Reported** means it came from a scan
agent and has not been checked yet; check before acting on it.

Status: `open` / `fixed (<commit>)` / `wontfix (<reason>)`.

## Security / correctness

| ID | Where | Finding | Check | Status |
|---|---|---|---|---|
| F-001 | `src/slc.c:45` | `slc_set_eol()` checks `set_configuration_locked` but calls `set_eol`. A board without `set_eol` makes a NULL call. | Verified | open |
| F-002 | `src/cm/cm_main.c:688` | The auth gate only applies when `slc == PB_SLC_CONFIGURATION_LOCKED`. In `PB_SLC_EOL` every command is open. It also compares the core `slc_t` with the wire enum (the values happen to match). | Verified | open: intended? |
| F-003 | `tools/punchboot/socket.c:47-80` | `read()` and `write()` loops pass the full `sz` on every iteration. `write` also re-sends the original `bfr`. A short read overruns the buffer, and EOF (0) loops forever. Affects the socket transport used by the host tests. | Verified | fixed (claude-work) |
| F-004 | `src/board/imx8{qxmek,dxlevk}/board.c:~288` | `strncmp(expected, password, length)` with `length` taken from the host accepts any prefix ("i"). `LOG_DBG` prints a password that is not NUL-terminated. Example boards, but the pattern gets copied. | Verified | open |
| F-005 | `src/cm/cm_main.c:283` `cmd_auth` | Reads 1024 bytes, then passes the host-supplied `auth_cmd->size` along without bounds-checking it. | Partly verified: the bound in the callee is not checked yet | open |
| F-006 | `src/drivers/partition/gpt.c:~411` | Loop over `p->first_lba` has no 128-entry limit. Primary and backup tables are never compared. | Reported | partly fixed (claude-work): entry loop bounded by `no_of_parts` (F-056); tables still not compared |
| F-007 | `src/boot/ab_state.c` | If a state partition is missing, it still does I/O with a negative handle. The B→A rollback branch is not symmetric with A→B. `validate()` zeroes `crc` as a side effect. `init` sets `cfg` before validating, so a failed init leaves it half set up. | Reported | open (NULL-`cfg` part split out as F-012) |
| F-008 | `src/drivers/crypto/mbedtls/mbedtls_pb.c` | Return values of the `mbedtls_sha*` calls are ignored. | Reported | open |
| F-009 | `tools/punchboot/python_wrapper.c` `log_cb` | Ignores `PyObject_CallMethod` failure and leaks the result. The GIL is never released during USB I/O. | Reported | open |
| F-010 | `tools/pbstate/CMakeLists.txt` | `BACKUP_PART` is set from `PB_STATE_PRIMARY_PART` (copy-paste). The target is listed twice in `install`. | Reported | open |
| F-012 | `src/boot/ab_state.c` + boards | If GPT init fails (blank disk), boards skip `boot_ab_state_init()` but still register its callbacks. `boot disable` dereferenced a NULL `cfg`, and a commit would have written to bio dev 0 (the raw disk). It crashed `test_part` under QEMU 10. | Verified | fixed (claude-work): guards on every entry point |
| F-013 | `src/plat/qemu/gcov.h` | GCC 14 added a 9th gcov counter type, so the struct layout was wrong and the code read a garbage `n_functions`. | Verified | fixed, then removed with `plat/qemu` (D-006) |
| F-014 | `src/plat/qemu/gcov.c` | Since GCC 12, the counter record length must be in bytes; it was written in words, so `.gcda` files were unreadable and showed 0% coverage. The merge loop also advanced past inactive counters. | Verified | fixed, then removed with `plat/qemu` (D-006) |
| F-047 | `tools/punchboot/punchboot/session.py` `boot_set_boot_part` | `None`/`"none"` passed `None` to the C extension, which requires bytes, so it raised `TypeError`. The CLI worked around it with a null UUID. | Verified | fixed (claude-work) |
| F-048 | `src/drivers/partition/gpt.c` `gpt_prng` | xorshift on a signed `int`, a signed-shift overflow (UB), found by UBSan. | Verified | fixed (claude-work): `uint32_t` |
| F-049 | `gpt.c:239,269`, `cm_main.c:336,371` | `%llu` used for `uint64_t`, wrong on 64-bit (use `PRIu64`). Other host `-Wextra` warnings: unused params in `wire.c` and `image_helpers.c`, an always-true check in `rot.c:98`. | Verified | open |
| F-050 | `tools/punchboot/api_partition.c` `pb_api_partition_write` | A bpak image is written payload-first from offset 0 and header-last at the end of the partition, with no size check. When payload + header > partition (e.g. a 512 KiB payload into a 512 KiB partition), the last payload chunk overwrote the header area: the write "succeeded" and left a corrupt image. Larger files only failed partway, with IOError. | Verified | fixed (claude-work): rejects `file size > partition size` with `ArgumentError` before writing |
| F-015 | `src/board/test/board.c:~245` | `if (readable_part)` should be `>= 0`; a negative error code is truthy. Harmless because the bio functions check the handle. | Verified | fixed in `board/host`; `board/test` removed |
| F-011 | `include/bpak/bpak.h` (vendored) | `bpak_add_transport_meta` uses the wrong size. Fixed upstream in bpak v0.9.2. Latent: punchboot never calls it. | Reported | open |
| F-053 | `src/cm/cm_main.c:348,387` `cmd_stream_read/write` | `buffer_id` (0-255) and `size` (u32) are not bounded; only `stream_prep` checks them. READ_BUFFER writes disk data past the 2×4 KiB CM buffer (memory corruption), WRITE_BUFFER copies arbitrary RAM into a partition (disclosure). No auth needed unless CONFIGURATION_LOCKED. | Verified (code) | fixed (claude-work): `stream_check()` bounds buffer id, size and offset; `test_stream.py` |
| F-054 | `src/bio.c:179` `check_lba_range` | `first_lba + lba` is computed in 32-bit `lba_t` and wraps, so a large LBA passes the check and reaches any absolute block. Stream commands truncate the u64 byte offset to `lba_t`, so the host can read/write the whole disk through any readable/writable partition. | Verified (code) | fixed (claude-work): 64-bit range check, `bio_write` returns `-PB_ERR_PARAM` like `bio_read`; `test_bio.c`, `test_stream.py` |
| F-055 | `src/bio.c:218` `bio_erase`, `cm_main.c:502` | No range check, `first_lba` is not added (partition-relative LBA passed as absolute), no `BIO_FLAG_WRITABLE` check. On imxrt, erasing any partition at LBA 0 erases the start of NOR. The host disk has no erase op, so it's untested. | Verified (code) | open: keep `start_lba` partition-relative as hosts send it; see F-075 (D-008) |
| F-056 | `src/drivers/partition/gpt.c:111,250,355,386` | `no_of_parts` from disk is never bounded to 128 (extends F-006). The array CRC reads past `primary`/`backup`, and recovery `memcpy`s write past them. `hdr_sz`/`part_entry_sz` are not checked. Memory corruption from a crafted disk, before any image is verified. | Verified (code) | fixed (claude-work): header rejected unless ≤ 128 entries of 128 bytes; `test_gpt.c` |
| F-057 | `src/drivers/crypto/caam/imx_caam.c:209,220` `caam_der_ecsig_to_rs` | Always copies `sz` bytes (from the signature) into `r`/`s`, so r/s longer than `rs_length` overflow; `sig_length` and the SEQUENCE length are ignored. Runs before verification on every CAAM board (jiffy, imx8x, imx8m). | Verified (code) | open |
| F-058 | `src/boot/image_helpers.c:113` | Passes `sizeof(header_digest)` (64) as the digest length. mbedtls truncates to the curve size, so sha256+P-384/P-521 and sha384+P-521 hash trailing garbage and valid images are rejected (fails closed). | Verified (code) | open |
| F-059 | `src/boot/linux.c:69-71` | Ramdisk load-addr meta is dereferenced before `rc` is checked; `mh` is NULL on failure. NULL deref for an image without a ramdisk part on boards with `ramdisk_bpak_id`. | Verified (code) | open |
| F-060 | `src/boot/boot.c:311` `boot_load` | Success falls through to `err_out`, which clears `boot_flags`, so `late_boot` never sees `BOOT_FLAG_CMD` (imxrt `board_late_boot` depends on it). | Verified (code) | open |
| F-061 | `src/boot/image_helpers.c:280` + `cm_main.c:558` | `rc = result_f(rc)` replaces a read/hash error with `cm_write`'s result, so a failed part reads as OK; only the final payload hash catches it. | Verified (code) | open |
| F-062 | `src/boot/image_helpers.c:258` | Payload LBA is computed with a hard-coded `/512`, while the header read uses `bio_block_size()`. Wrong data on any non-512 block device. Latent. | Verified (code) | open |
| F-063 | `cm_main.c:207,221` `cmd_board` + `board/host/board.c:435` | `request_size` from the host is passed to the board unbounded (cm always reads 1024). The host board does `arg[size] = 0`, an out-of-bounds write. Same pattern as F-005. | Verified (code) | open |
| F-064 | `src/drivers/partition/gpt.c:152` | Header `last_lba` (last usable) is `last - 32`, which is the first backup-entry block; should be `last - 33`. pb's own allocation is unaffected; other tools could overlap the backup array. | Verified (code) | open |
| F-065 | `src/boot/*`, `utils_def.h:150` | Hardening, signed input only: `CHECK_OVERLAP` can wrap; no check that load regions are in RAM or that parts don't overlap each other; load-addr meta read as u64 without a size check; `linux.c` doesn't check `fdt_totalsize <= dtb part`; `keystore_id` never compared; no anti-rollback counter. | Reported | open |
| F-066 | `src/lib/der_helpers.c` | `der_ecsig_to_rs` has no callers and doesn't left-pad short r/s. `der_ec_public_key_data` memcmp with a data-supplied length (0 matches anything, and an OID longer than the reference reads past it); `s -= 2` can underflow. | Reported | open: `test_der.c` covers the rest |
| F-068 | `src/lib/bpak.c` `bpak_del_meta` (vendored, same upstream) | When the last meta slot is in use, the slot isn't cleared and the final `memset` writes the 16 bytes after `meta[]`, i.e. `parts[0]`. punchboot never deletes meta. | Verified (code) | open: fix upstream |
| F-069 | `src/crypto.c:33`, `src/cm/cm_main.c:538` | Positive error codes: `hash_init` returned `PB_ERR_NOT_SUPPORTED` when no provider matched; `cmd_part_verify` sets `rc = PB_ERR_CHECKSUM` (the host still gets PART_VERIFY_FAILED). | Verified (test) | fixed (claude-work) |
| F-070 | `src/boot/boot.c` | BIO and CB boots load into `header`, but `prepare`/`late_boot` get `header_ptr`, which an IN_MEM or CUSTOM boot repoints. After a failed in-memory boot, a command-mode boot would prepare (jump address, dtb) from the stale, unverified header. Latent: no board uses IN_MEM/CUSTOM. | Verified (code) | open |
| F-071 | `src/boot/linux.c:173` | `fdt_delprop("pb,slc-available-keys")` returns `-FDT_ERR_NOTFOUND` if `/chosen` doesn't have the property, which fails the boot. A DTB without `/chosen` fails too, and new properties only fit if the DTB was built with free space. The jiffy, imx8dxlevk and imx8qxmek DTBs have the properties (Jonas), so no board is affected today. | Verified (code, libfdt) | open: planned, pb should add what's missing (roadmap, Phase 1) |
| F-072 | `src/cm/cm_main.c` `cmd_slc_read` | Returns the status of the last key in the key map, which becomes the final result. If the last key is revoked the host gets KEY_REVOKED, and `slc_get_*` raise `KeyRevokedError`. | Verified (test: `test_commands.py`, xfail) | open |
| F-073 | `src/bio.c:256` `bio_set_hal_flags` | `flags << 24` on a promoted `uint8_t`: signed overflow (UB) when bit 7 is set. Found by UBSan in `test_bio.c`. | Verified (test) | fixed (claude-work) |
| F-074 | `src/cm/cm_main.c` stream read/write | After a failed STREAM_INITIALIZE, `block_dev` is an error code, and `bio_get_flags()` of it has the flag bits set, so the readable/writable checks pass. `stream_check()` then fails on the block size, so it's harmless. | Verified (test) | open |
| F-075 | `cm_main.c` `cmd_part_erase`, `struct pb_command_erase_part` | Wire compat (D-008): `start_lba`/`block_count` were taken from `rz` in 2309738 (v2.0.0). A pre-2.0 host sends zeros, so a current device erases 0 blocks and answers OK: a silent no-op instead of a full erase. Fix: `block_count == 0` means the whole partition. Coordinate with F-055. | Verified (code) | open |
| F-076 | `src/boot/image_helpers.c` (payload hash and load) | bpak compat (D-008): punchboot hashes every part, while upstream `verify.c` skips parts with `BPAK_FLAG_EXCLUDE_FROM_HASH` (`bpak add --exclude-from-hash`). Such images verify with the bpak CLI but fail to boot in punchboot. Supporting the flag only accepts more images, so it is a compatible fix. | Verified (code) | open |
| F-077 | `tools/punchboot/api_partition.c`, `api_boot.c` | Wire compat, new host + old device: the host uses `caps.chunk_transfer_max_bytes` without a zero check. A device that predates the field (before 0.7, 2020) reports 0: partition write sends the bpak header and no payload and reports success, and RAM boot loops forever. Fall back to `stream_buffer_size`. Low: such devices are probably gone. | Verified (code) | open |

## Dependencies / vendored code

| ID | Finding | Status |
|---|---|---|
| F-020 | Mbed TLS pinned to 3.4.0 (EOL). The file list in `src/drivers/crypto/mbedtls/makefile.mk` is tied to that release. `mbedtls_config.h` enables MD5/BASE64/PKCS1_V21, which are unused. | open |
| F-021 | Vendored bpak is ~v0.9.0, the CI CLI is v0.9.3, and there is no sync procedure. | open |
| F-022 | Vendored code has no version record: Kconfiglib, libfdt, libuuid, TF-A xlat_tables_v2 + libc, NXP SCFW, littlefs. | open |

## Dead code / cleanup

| ID | Finding | Status |
|---|---|---|
| F-030 | libc files never compiled: `memcpy.c`, `strncmp.c`, `strlcpy.c`, `strnlen.c`, `strrchr.c`. Also `xlat_tables_v2/aarch32/nonlpae_tables.c`. | open |
| F-031 | Stale `-I src/libc/include/...` in the armv7a/armv8a makefiles. The gcovr excludes still name `bearssl` and `src/fdt`. | open |
| F-032 | Deprecated commands still handled: `PART_TBL_INSTALL`, `PART_RESIZE`, `AUTH_SET_PASSWORD`. `pb_wire_command_string`/`pb_wire_slc_string` are stubs. | open: removing them breaks old hosts (D-008); at most answer `NOT_SUPPORTED`, never reuse the slots |
| F-033 | imx8mevk unfinished (`board_init` returns error, no Kconfig). Orphan defconfigs `bebop`, `pico8ml`. imxrt untouched since 2023. | open: ask Jonas |
| F-034 | Bash tests that exist but don't run: `test_config`, `test_invalid_key_index2`, `test_setup_lock`, `test_bpak_show`, `test_part_offset_write`. They use commands and tools that no longer exist. | removed with the bash suite |
| F-035 | The same data is defined in several places: exception names (C ×1, Python ×3), partition flags, SLC enum, and the crc32 id table (`helpers.py`, could be `zlib.crc32`). | open |

## Build / CI / docs

| ID | Finding | Status |
|---|---|---|
| F-040 | `build.yml`: `actions/checkout@v1`, deprecated codecov bash uploader, Coverity steps fail without the secret (fork PRs). | fixed (D-006) |
| F-041 | Wheels are never tested (no cibuildwheel `test-command`). There are no Python unit tests. | open |
| F-042 | `pyproject.toml` has no `[build-system]`/`[project]`; all metadata is in `setup.py`. The old author email is still in it. | open |
| F-043 | No `ruff-format` hook despite the format config. mypy only runs through pre-commit.ci. There is no `_punchboot.pyi`. | open |
| F-044 | Sphinx docs are mostly stubs, with version 1.0.0 in `conf.py`, broken header paths, and `sphinx<7`. The README test instructions are wrong (no `BOARD`). `doc/release-checklist.txt` is stale. | open |
| F-046 | CI's `build.yml` installs neither click nor gcovr versions; it uses whatever the runner has. CI passed with old click, while PyPI users get click >= 8.2 (subgroups without a command exit with 2). `test_cli` was updated and the requirement is now `click>=8.2`. | fixed (claude-work); CI still to move to the container |
| F-045 | `tests/common.sh` hardcodes `/tmp/pb.sock`, `/tmp/disk` and `build-test/`, so tests cannot run in parallel. | fixed by the host tests (per-test working directory) |
| F-051 | Most boards don't build: imx8qxmek uses an old SCI API (`sci_seco_api.h`, `sc_pm_*`, `sc_pad_set`); imx8dxlevk needs a `keystore.bpak` that isn't in the repo (it builds with the internal keystore); imx8mevk needs proprietary DDR blobs; imxrt1060evk has an implicit `ts_print`. Only the QEMU board was ever built in CI. | Verified | open: CI builds jiffy + imx8dxlevk; ask Jonas which boards to keep (F-033) |
| F-052 | `src/drivers/virtio/*` has no user since `plat/qemu` was removed. | Verified | open: keep for a QEMU smoke tier, or remove |
| F-067 | Host test infra: `pbtest.py` `stop()` ignores pb-host's exit code, so sanitizer/LeakSanitizer failures at teardown are lost. The socket file appears at `bind()`, before `listen()` (startup race). No client timeouts, no pytest-timeout/ctest `TIMEOUT`. pb-host `write()` can die from SIGPIPE. Orphaned pb-host if pytest is killed. `fakes/ramdisk.c` computes `lba * 512` in 32 bits. 64-bit host hides 32-bit overflows (F-054); consider a `-m32` job. | Verified (read) | partly fixed (claude-work): teardown fails on non-zero exit and sanitizer reports; ramdisk overflow fixed; the socket path appears only after `listen()`; ctest `TIMEOUT` (60 s per unit test, 600 s for the integration suite). Client timeouts in the C library, SIGPIPE, orphans and `-m32` open |
