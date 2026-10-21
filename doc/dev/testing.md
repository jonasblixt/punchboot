# Testing

The host-native test setup replaces the QEMU/bash suite (D-004). It is being built up
alongside the old suite, and the bash tests are ported one at a time. The QEMU suite is
removed once everything is covered.

## Layout

| Path | What |
|---|---|
| `CMakeLists.txt` | Host build only. The firmware for targets is still built by the `Makefile`. |
| `configs/host_defconfig` | Kconfig for the host build (`ARCH_HOST`, `PLAT_HOST`) |
| `src/arch/host/` | No-op cache/MMU. `arch_jump` exits. |
| `src/plat/host/` | Process entry (`main.c`, CLI options), RAM, file-backed disk, unix-socket command-mode transport |
| `src/board/host/` | Test board, mirroring `src/board/test` (same partitions, keys, board commands, fuses) |
| `tests/unit/` | CMocka unit tests and their fakes (`fakes/ramdisk.c`, `fakes/plat.c`) |
| `tests/integration/` | pytest tests that drive `pb-host` through the `punchboot` Python package |
| `scripts/ci/host-test.sh` | In-container entry point: pip install, cmake, ctest |

## Running

```sh
scripts/pbdev host-test                      # build + all tests (~1 min cold, seconds warm)
scripts/pbdev host-test -DPB_SANITIZE=ON     # ASan + UBSan
BUILD_DIR=build-host-cov scripts/pbdev host-test -DPB_COVERAGE=ON
scripts/pbdev run gcovr -r . build-host-cov --filter src/
CTEST_ARGS="-L unit" scripts/pbdev host-test # only unit tests
```

Coverage baseline (2026-10-22, all host tests): 58% of lines in `src/`, excluding vendored libs and
the host platform. Lowest coverage: `linux.c` 13%, `bpak.c` 17%, `der_helpers.c` 0%, `delay.c` 0%.

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
- **Disk.** A file. The fuses live in a partition on it (test fuse driver), so device state
  persists across reboots within a test.
- **Reset and boot.** `plat_reset()` calls `exit(0)`, which also writes gcov data. On boot, the
  board's `late_boot` writes the partition name to `--boot-status` and then resets.
- **System libc.** Host builds use glibc instead of the firmware libc. `src/plat/host/compat.h` is
  force-included and provides the BSD attribute macros (`__section`, `__aligned`, ...).

## Writing tests

- **Unit tests.** Use `pb_unit_test(name SOURCES <modules under test>)` in `tests/unit/CMakeLists.txt`.
  Link only the modules under test plus fakes. Call `bio_reset()` between cases (see `test_gpt.c`).
- **Integration tests.** Helpers live in `pbtest.py`: the `Device` class, `make_bpak()` (bpak CLI)
  and `sgdisk_verify()`. Fixtures:
  - `blank_device` starts in command mode on a blank disk.
  - `device` also has the default partition table installed.

  Use `device.session()` (only one at a time) together with `reboot()`, `boot()` (power on without
  command mode, returns 'A'/'B'/'?'), `read_blocks()`/`write_blocks()` (on the disk image) and
  `pbstate()`. When a test fails, the pb-host log is printed.

## Porting status

All 43 bash tests in the QEMU run list are ported: 63 pytest cases plus 7 CMocka cases, about 9 s
in total. The bash/QEMU suite stays until CI is switched over (`build.yml` is untouched for now).

| pytest module | Ported from |
|---|---|
| `test_part.py` | test_part, test_part_flash, test_part_verify_fh, test_verify_bpak, test_part_dump, test_part_dump2 (+ size limits, F-050) |
| `test_gpt.py` | test_corrupt_gpt{,2,3,4,5}, test_corrupt_backup_gpt, test_gpt_variants |
| `test_boot.py` | test_boot_bpak{,2..10}, test_invalid_key_index, test_overlapping_region, test_all_sig_formats, test_gpt_boot_activate{,_step2..5} |
| `test_ab_state.py` | test_switch, test_rollback, test_corrupt_primary_config, test_corrupt_backup_config, test_board_regs |
| `test_security.py` | test_device_setup, test_authentication, test_revoke_key |
| `test_board.py` | test_board, test_board_status |
| `test_cli.py` | test_cli, test_reset |

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

Not ported, because they use tools and commands that no longer exist (`config`, `pbi`,
`part show`, `--offset`, old `slc`/`auth` flags) and were not in the run list: test_config,
test_invalid_key_index2, test_setup_lock, test_bpak_show, test_part_offset_write.

## Findings from the first runs

- `Session.boot_set_boot_part(None)` raised `TypeError`, so the documented API never worked. Fixed (F-047).
- UBSan: signed-shift overflow in `gpt_prng`. Fixed (F-048).
- The 64-bit host build shows `%llu`-with-`uint64_t` format bugs (F-049).
- `part_write` accepted bpak images whose payload overlapped the header (written to the end of the
  partition), and corrupted them silently. Fixed in the host library (F-050).
- Authentication lasts until reset, not per connection. That is by design: separate CLI invocations
  authenticate and then run commands. `test_authentication_lasts_until_reset` documents it.
