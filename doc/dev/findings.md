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
| F-006 | `src/drivers/partition/gpt.c:~411` | Loop over `p->first_lba` has no 128-entry limit. Primary and backup tables are never compared. | Reported | open |
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
| F-032 | Deprecated commands still handled: `PART_TBL_INSTALL`, `PART_RESIZE`, `AUTH_SET_PASSWORD`. `pb_wire_command_string`/`pb_wire_slc_string` are stubs. | open |
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
