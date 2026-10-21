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
- **Integration tests.** The `device` fixture gives you a running device in command mode on a blank
  disk. Use `device.session()` (only one at a time), `device.reboot()` and `device.boot_status`.
  When a test fails, the pb-host log is printed.

## Porting status

| Bash test | pytest | Notes |
|---|---|---|
| `test_part.sh` | `test_part.py` | Also checks the table after a reboot |
| all others | — | To do (42 tests) |

## Findings from the first runs

- `Session.boot_set_boot_part(None)` raised `TypeError`, so the documented API never worked. Fixed (F-047).
- UBSan: signed-shift overflow in `gpt_prng`. Fixed (F-048).
- The 64-bit host build shows `%llu`-with-`uint64_t` format bugs (F-049).
