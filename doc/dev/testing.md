# Testing

All tests run on the host (D-004): punchboot is built as a native process, and there are CMocka
unit tests and pytest integration tests. The old QEMU/bash suite has been ported and removed, and
CI runs these tests (D-006).

## Layout

| Path | What |
|---|---|
| `CMakeLists.txt` | Host build only. The firmware for targets is still built by the `Makefile`. |
| `configs/host_defconfig` | Kconfig for the host build (`ARCH_HOST`, `PLAT_HOST`) |
| `src/arch/host/` | No-op cache/MMU. `arch_jump` exits. |
| `src/plat/host/` | Process entry (`main.c`, CLI options), RAM, file-backed disk, unix-socket command-mode transport |
| `src/board/host/` | Test board (partitions, keys, board commands, fuses on disk) |
| `tests/unit/` | CMocka unit tests, one per module, and the shared fakes (`fakes/ramdisk.c`, `fakes/plat.c`) |
| `tests/integration/` | pytest tests that drive `pb-host` through the `punchboot` Python package |
| `scripts/ci/host-test.sh` | In-container entry point: pip install, cmake, ctest |

## Running

```sh
scripts/pbdev test                           # build + all tests (~1 min cold, ~10 s warm)
scripts/pbdev test -DPB_SANITIZE=ON          # ASan + UBSan
BUILD_DIR=build-host-cov scripts/pbdev test -DPB_COVERAGE=ON
scripts/pbdev run gcovr -r . build-host-cov --filter src/
CTEST_ARGS="-L unit" scripts/pbdev test      # only unit tests
```

Coverage (2026-10-22, all host tests): 99.2% of lines (2337/2356), 100% of functions and 96.3% of
branches in `src/`, excluding vendored libs and the host platform. It was 59% before the unit tests
below. The 19 lines left are unreachable or behind a known bug:

- Unreachable: `gpt.c` 298 and 383-389 (after recovery the primary is valid), `cm_main.c` 653 and
  the `default:` case (every command id is handled), `rot.c` 114 (`rot_init` checks the key ids) and
  126-127 (the keystore has only EC keys), `main.c` 51, `mbedtls_pb.c` 252 (needs a full DSA table
  with room for a hash provider; both tables hold one entry).
- Behind a bug: `linux.c` 76-77 (F-059), 176-177 (F-071).
- `main.c` 29: `plat_init()` failing on the host.

The coverage counts only what the host build compiles: the drivers and platform code for the
targets are not in it.

A single integration test, inside `scripts/pbdev shell`:
`pip install --no-build-isolation --no-deps . && pytest tests/integration -k install`.

## How the host platform works

- **Entry point.** `main.c` in `src/` is compiled with `-Dmain=pb_main`, and `src/plat/host/main.c`
  parses the options (`--disk --socket --boot-status --command-mode`) and calls `pb_main()`.
- **RAM.** 512 MiB is `mmap`ed at `BOARD_RAM_BASE` (0x40000000), so `pb-load-addr` values work
  unchanged. It stays below 0x7fff8000, where ASan's shadow memory starts.
- **pb's own memory regions.** These normally come from the linker script. On the host they are
  set with `--defsym` (`-no-pie`) at the start of RAM, so the image overlap checks keep working.
- **Transport.** A unix socket server. `complete()` does the blocking I/O. When a client disconnects,
  `complete()` fails, `cm_run` calls `init()` again, and `init()` accepts the next client, so each
  CLI or `Session` is one connection.
  The socket is bound to `<path>.tmp` and renamed to `<path>` after `listen()`, so tests can treat
  the path appearing as "command mode is ready".
- **Disk.** A file. The fuses live in a partition on it (test fuse driver), so device state
  persists across reboots within a test.
- **Reset and boot.** `plat_reset()` calls `exit(0)`, which also writes gcov data. On boot, the
  board's `late_boot` writes the partition name to `--boot-status` and then resets.
- **System libc.** Host builds use glibc instead of the firmware libc. `src/plat/host/compat.h` is
  force-included and provides the BSD attribute macros (`__section`, `__aligned`, ...).

## Writing tests

- **Unit tests.** Use `pb_unit_test(name SOURCES <modules under test> LIBS <libraries>)` in
  `tests/unit/CMakeLists.txt`. Link only the modules under test, and fake what they call in the
  test file (D-007). Call `bio_reset()` between cases (see `test_gpt.c`).
  - `fakes/ramdisk.h`: set `fail_reads_after`/`fail_writes_after` to n to let n operations succeed
    and fail the rest with `-PB_ERR_IO`.
  - `fakes/plat.h`: `fake_us_tick` and `fake_us_tick_step` drive `plat_get_us_tick()`. Reset them
    in teardown; `test_gpt.c` depends on the default.
  - Modules keep their config in statics that can't be reset (`rot`, `slc`, `crypto`, `ab_state`,
    `boot`). Tests for the uninitialized state run first, and `cmocka_unit_test` order matters.
  - The crypto API holds one hash and one DSA provider (`CONFIG_CRYPTO_MAX_*_OPS=1`), so fake and
    Mbed TLS providers are in separate test programs.
  - `test_cm.c` scripts the host side with `host_*()`, runs `cm_run()` until the input runs out, and
    checks the device's output with `expect_*()`.
  - A path that hits an open bug is left out, with a comment naming the finding.
- **Integration tests.** Helpers live in `pbtest.py`: the `Device` class, `make_bpak()` (bpak CLI)
  and `sgdisk_verify()`. Fixtures:
  - `blank_device` starts in command mode on a blank disk.
  - `device` also has the default partition table installed.

  Use `device.session()` (only one at a time) together with `reboot()`, `boot()` (power on without
  command mode, returns 'A'/'B'/'?'), `read_blocks()`/`write_blocks()` (on the disk image) and
  `pbstate()`. When a test fails, the pb-host log is printed.
- **Raw wire protocol.** `device.wire()` gives a `Wire` connection for requests the punchboot
  package never sends (bad buffer ids, sizes, offsets). See `test_stream.py`.
- **Teardown checks.** The fixture fails the test if pb-host exits non-zero, doesn't exit on reset,
  or its log contains an ASan/LeakSanitizer/UBSan report. A locked, unauthenticated device can't be
  reset, so it is killed without a failure.

## Porting status

All 43 bash tests in the QEMU run list were ported (63 pytest cases plus 7 CMocka cases, about 9 s
in total). The bash suite, `src/board/test`, `src/plat/qemu` (including the semihosting gcov hack)
and `configs/test_defconfig` were then removed.

| pytest module | Ported from |
|---|---|
| `test_part.py` | test_part, test_part_flash, test_part_verify_fh, test_verify_bpak, test_part_dump, test_part_dump2 (+ size limits, F-050) |
| `test_gpt.py` | test_corrupt_gpt{,2,3,4,5}, test_corrupt_backup_gpt, test_gpt_variants |
| `test_boot.py` | test_boot_bpak{,2..10}, test_invalid_key_index, test_overlapping_region, test_all_sig_formats, test_gpt_boot_activate{,_step2..5} |
| `test_ab_state.py` | test_switch, test_rollback, test_corrupt_primary_config, test_corrupt_backup_config, test_board_regs |
| `test_security.py` | test_device_setup, test_authentication, test_revoke_key |
| `test_board.py` | test_board, test_board_status |
| `test_cli.py` | test_cli, test_reset |
| `test_stream.py` | new: stream bounds (F-053, F-054) |
| `test_commands.py` | new: boot status, erase, end of life, bpak read, F-072 (xfail) |

Unit tests, by module under test:

| Test | Module | Fakes |
|---|---|---|
| `test_bio.c`, `test_gpt.c`, `test_fuse_bio.c` | `bio.c`, `gpt.c`, `test_fuse_bio.c` | ramdisk |
| `test_bpak.c`, `test_der.c`, `test_wire.c`, `test_delay.c` | `bpak.c`, `der_helpers.c`, `wire.c`, `delay.c` | clock (`test_delay.c`) |
| `test_crypto.c`, `test_mbedtls.c` | `crypto.c`, `mbedtls_pb.c` | providers / none (real keys and the auth token) |
| `test_rot.c`, `test_device_uuid.c` | `rot.c`, `slc.c`, `device_uuid.c` | board callbacks, unique id |
| `test_ab_state.c`, `test_boot.c` | `ab_state.c`, `boot.c` | ramdisk, image helpers, boot driver |
| `test_image_helpers.c` | `image_helpers.c` | hash, DSA, root of trust (memory regions via `--defsym`) |
| `test_linux.c` | `linux.c` | arch, SLC, root of trust, device UUID; builds DTBs with libfdt |
| `test_cm.c` | `cm_main.c` | transport, boot, root of trust, SLC, crypto providers |

Where the ports differ from the bash tests:
- **Self-contained.** The bash tests depended on disk state and on the `/tmp/pb_force_command_mode`
  flag left behind by earlier tests. Each pytest test starts from a blank disk (`blank_device`) or a
  freshly installed partition table (`device`).
- **Stronger checks.**
  - GPT and A/B state recovery check that the corrupt copy is rewritten.
  - Boots check which partition was booted.
  - Negative boot tests check that the device didn't boot.
- **Fixed intent.**
  - test_corrupt_gpt3/4 used dd `skip` instead of `seek`, so they corrupted the start of the disk.
    The header and partition-entry cases are now tested properly.
  - test_boot_bpak9 never wrote the oversized image it was about.
  - The test_gpt_boot_activate steps always ran in command mode and never checked a boot.
- **Python API instead of the CLI.** `test_cli.py` covers the CLI itself.

Not ported, and removed with the suite, because they use tools and commands that no longer
exist (`config`, `pbi`, `part show`, `--offset`, old `slc`/`auth` flags) and were not in the run
list: test_config, test_invalid_key_index2, test_setup_lock, test_bpak_show, test_part_offset_write.

## Findings from the first runs

- `Session.boot_set_boot_part(None)` raised `TypeError`, so the documented API never worked. Fixed (F-047).
- UBSan: signed-shift overflow in `gpt_prng`. Fixed (F-048).
- The 64-bit host build shows `%llu`-with-`uint64_t` format bugs (F-049).
- `part_write` accepted bpak images whose payload overlapped the header (written to the end of the
  partition), and corrupted them silently. Fixed in the host library (F-050).
- Authentication lasts until reset, not per connection. That is by design: separate CLI invocations
  authenticate and then run commands. `test_authentication_lasts_until_reset` documents it.
