# Testing

All tests run on the host (D-004): punchboot is built as a native process, and there are CMocka
unit tests and pytest integration tests. The old QEMU/bash suite has been ported and removed, and
CI runs these tests (D-006).

## Layout

| Path                      | What                                                                                   |
| ------------------------- | -------------------------------------------------------------------------------------- |
| `CMakeLists.txt`          | Host build only. The firmware for targets is still built by the `Makefile`.            |
| `configs/host_defconfig`  | Kconfig for the host build (`ARCH_HOST`, `PLAT_HOST`)                                  |
| `src/arch/host/`          | No-op cache/MMU. `arch_jump` exits.                                                    |
| `src/plat/host/`          | Process entry (`main.c`, CLI options), RAM, file-backed disk, unix-socket command-mode |
|                           | transport                                                                              |
| `src/board/host/`         | Test board (partitions, keys, named commands, the fuses' layout)                       |
| `tests/unit/`             | CMocka unit tests, one per module, and the shared fakes (`fakes/ramdisk.c`,            |
|                           | `fakes/plat.c`)                                                                        |
| `tests/integration/`      | pytest tests that drive `pb-host` through the `punchboot` Python package               |
| `tests/compat/`           | Compatibility guards (D-008): `abi.txt` + `check_abi.py`, golden bpak images in        |
|                           | `bpak/`, wire transcripts in `transcripts/`. See `compatibility.md`                    |
| `tests/doc/`              | The document lint (`process.md` 7): `check_docs.py`, the test `doc.lint`               |
| `tests/make/`             | The firmware build: `check_rebuild.py` (`make.rebuild`) runs `make` for jiffy and      |
|                           | checks that a changed keystore, SRK fuse file or version is made again (F-183);        |
|                           | `check_config.py` (`make.config`), its configuration (F-229, F-147);                   |
|                           | `check_include_dirs.py` (`make.include_dirs`), that each `-I` exists (F-031)           |
| `scripts/ci/host-test.sh` | In-container entry point: pip install, cmake, ctest                                    |

## Running

```sh
scripts/pbdev test                           # build + all tests (~1 min cold, ~10 s warm)
scripts/pbdev test -DPB_SANITIZE=ON          # ASan + UBSan
BUILD_DIR=build-host-cov scripts/pbdev test -DPB_COVERAGE=ON
scripts/pbdev run gcovr -r . build-host-cov --filter src/
CTEST_ARGS="-L unit" scripts/pbdev test      # only unit tests
CTEST_ARGS="-L compat" scripts/pbdev test    # only the frozen layout check (compatibility.md)
CTEST_ARGS="-L doc" scripts/pbdev test       # only the document lint (process.md 7)
CTEST_ARGS="-L make" scripts/pbdev test      # only the firmware build's checks
python3 tests/doc/check_docs.py              # the same, without the container
```

Coverage (2026-10-22, all host tests): 99.2% of lines (2337/2356), 100% of functions and 96.3% of
branches in `src/`, excluding vendored libs and the host platform. It was 59% before the unit tests
below. After `jiffy-fixes` 18 lines are left, unreachable or behind a known bug:

- Unreachable: `gpt.c`'s lines then (after recovery the primary is valid; that code is gone with
  the GPT's repair, F-195), `cm_main.c` 663 and
  the `default:` case (every command id is handled), `boot.c` 96 (the boot device is valid), `rot.c` 114 (`rot_init` checks the key ids) and
  126-127 (the keystore has only EC keys), `main.c` 51, `mbedtls_pb.c` 252 (needs a full DSA table
  with room for a hash provider; both tables hold one entry).
- Behind a bug: `image.c` 175-176 (F-071).
- `main.c` 29: `plat_init()` failing on the host.

The coverage counts only what the host build compiles: the drivers and platform code for the
targets are not in it.

A single integration test, inside `scripts/pbdev shell`:
`pip install --no-build-isolation --no-deps . && pytest tests/integration -k install`.

## How the host platform works

- **Entry point.** `main.c` in `src/` is compiled with `-Dmain=pb_main`, and `src/plat/host/main.c`
  parses the options (`--disk --fuses --nor --socket --boot-status --command-mode`) and calls
  `pb_main()`.
- **RAM.** 512 MiB is `mmap`ed at `BOARD_RAM_BASE` (0x40000000), so `pb-load-addr` values work
  unchanged. It stays below 0x7fff8000, where ASan's shadow memory starts.
- **pb's own memory regions.** These normally come from the linker script. On the host they are
  set with `--defsym` (`-no-pie`) at the start of RAM, so the image overlap checks keep working.
- **Transport.** A unix socket server. `complete()` polls: it moves what the socket has, waits at
  most 10 ms for more and returns AGAIN until the transfer is whole, so command mode's transfer
  timeout and kicks run (`command-mode.md` 4.6). When a client disconnects,
  `complete()` fails, command mode's task calls `init()` again, and `init()` accepts the next
  client, so each
  CLI or `Session` is one connection, and one session (`command-mode.md` 3.1): a locked device
  asks each for authentication.
  The socket is bound to `<path>.tmp` and renamed to `<path>` after `listen()`, so tests can treat
  the path appearing as "command mode is ready".
- **Disk.** A file.
- **Fuses.** A file of their own (`--fuses PATH`, default `fuses.img`), not on the disk: the
  test fuse driver reads it in `plat_init()`, so the life cycle and the revoked keys don't depend
  on the partition table, as a SoC's fuses don't (F-217). Device state persists across reboots
  within a test. The disk's "Fuse array" partition is left unused, so the layout stays.
- **Flash.** With `--nor PATH`: a memory mapped flash for the in place source (`boot.md` 4.1),
  512 KiB at 0x70000000 (`BOARD_NOR_BASE`), a file that is erased (0xff) when new. pb has it
  mapped read only and writes it through its bio device, so a write through the map faults as
  it would be lost on a NOR. The host board gives it a static table: "NOR In place" (booted in
  place), "NOR Loaded" (loaded, like a disk's partition) and "NOR Not bootable". The host
  board's boot source policy is A/B on the disk; its init also marks the first two bootable,
  so `boot part` boots them and the policy never hands them out. Without the
  option pb-host has no flash and lists none.
- **Reset and boot.** `plat_reset()` calls `exit(0)`, which also writes gcov data. On boot, the
  board's `late_boot` writes the partition name to `--boot-status` and then resets: `A` or `B`,
  `X` for "NOR In place" and `N` for "NOR Loaded".
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
  - `test_cm.c` scripts the host side with `host_*()`, calls command mode's task
    (`cm_main_task()`) round after round, as `main()`'s loop does, until the input runs out,
    and checks the device's output with `expect_*()`.
  - A path that hits an open bug is left out, with a comment naming the finding.
  - Drivers with fixed register addresses: put `fakes/mmio` first on the include path (see
    `test_imx8m_ddr` in `CMakeLists.txt`); every `mmio_*` call then goes to `fake_mmio_read/write`
    in the test. `test_imx8m_ddr.c` uses this for a golden register trace (D-013,
    `imx8m-ddr.md`).
  - `fakes/regs.h`: a register file behind `fakes/mmio` for code that needs no model of a
    device. A register reads what was last written or set, every write is logged, and
    `fake_regs_on_write` is what the hardware does with a write (a bit that clears itself, a
    reset: it may leave with a `longjmp`). The ports' tests (`test_plat_*.c`) use it:
    `pb_plat_test()` in `CMakeLists.txt` builds a port's `plat.c` for the host.
- **MMC/SD test bench** (`tests/unit/models/`, D-017). Card models behind a bus interface
  (`mmc_bus.h`): `emmc_model.c` from JESD84-B51 and `sd_model.c` from the SD Physical Layer
  spec (written from memory, it wasn't at hand: violations name the command, not a section).
  They act like the card and log every host action the standard doesn't allow, citing the
  section (`bench.h`); `assert_bench_clean()` fails the test on any. Two ways in:
  - `hal_model.c`: an `mmc_hal` that behaves like a correct controller, for `mmc_core` alone
    (`test_mmc_emmc.c`).
  - `usdhc_model.c`: a register model of the uSDHC (i.MX6UL and i.MX8M RMs) behind
    `fakes/mmio`, for `imx_usdhc.c` + `mmc_core` with the boards' settings (`test_imx_usdhc.c`).
    It runs commands, ADMA2 and auto CMD12, latches INT_STATUS and logs RM violations. DMA
    addresses are 32 bits: `-no-pie` and static buffers.

  Time is simulated: `plat_get_us_tick()` advances 1 us per call (the real `delay.c` runs),
  every uSDHC register read costs 1 us, and polling for more than 10 s fails as stuck.
  Faults: no response or bad CRC on a command, SWITCH_ERROR on a field, data CRC/timeout,
  status error bits, power loss after n blocks; `emmc_model_rom_handover()` starts from where a
  boot ROM leaves the card. `test_mmc_models.c` checks the models themselves: a scripted host
  runs the standard sequences (DDR52, HS400 after tuning, SD high speed, SDR104 with CMD11)
  clean, and known mistakes give the expected violation. Not modeled: signal timing (DLL taps,
  sampling), boot operation, erase, write protection, RPMB authentication, CQE.
  Where the silicon differs from the RM, the model follows the silicon and says so (F-126: no
  TC at the end of busy without data).
- **Integration tests.** Helpers live in `pbtest.py`: the `Device` class, `make_bpak()` (bpak CLI)
  and `sgdisk_verify()`. Fixtures:
  - `blank_device` starts in command mode on a blank disk.
  - `device` also has the default partition table installed.
  - `nor_device` is `device` with the flash (`--nor`). `read_nor()` and `write_nor()` reach its
    file. An in place image's `pb-load-addr` is `NOR_BASE` plus the partition's offset plus the
    part's offset in the payload.

  Use `device.session()` (only one at a time) together with `reboot()`, `boot()` (power on without
  command mode, returns 'A'/'B'/'?'), `read_blocks()`/`write_blocks()` (on the disk image) and
  `pbstate()`. When a test fails, the pb-host log is printed.
- **pb-host's console.** The host platform sends stdout through `console_putc()` (`fopencookie()`),
  so with `CONFIG_RAMLOG` (on in `host_defconfig`) the RAM log holds the same bytes as the pb-host
  log, as on target with a UART.
- **Raw wire protocol.** `device.wire()` gives a `Wire` connection for requests the punchboot
  package never sends (bad buffer ids, sizes, offsets). See `test_stream.py`.
- **Teardown checks.** The fixture fails the test if pb-host exits non-zero, doesn't exit on reset,
  or its log contains an ASan/LeakSanitizer/UBSan report. The reset authenticates with the
  development key when a locked device asks for it (`pbtest.device_reset()`); a device that
  refuses that key can't be reset, so it is killed without a failure.

## Porting status

All 43 bash tests in the QEMU run list were ported (63 pytest cases plus 7 CMocka cases, about 9 s
in total). The bash suite, `src/board/test`, `src/plat/qemu` (including the semihosting gcov hack)
and `configs/test_defconfig` were then removed.

| pytest module         | Ported from                                                                             |
| --------------------- | --------------------------------------------------------------------------------------- |
| `test_part.py`        | test_part, test_part_flash, test_part_verify_fh, test_verify_bpak, test_part_dump,      |
|                       | test_part_dump2 (+ size limits, F-050)                                                  |
| `test_gpt.py`         | test_corrupt_gpt{,2,3,4,5}, test_corrupt_backup_gpt, test_gpt_variants                  |
| `test_boot.py`        | test_boot_bpak{,2..10}, test_invalid_key_index, test_overlapping_region,                |
|                       | test_all_sig_formats, test_gpt_boot_activate{,_step2..5}                                |
| `test_ab_state.py`    | test_switch, test_rollback, test_corrupt_primary_config,                                |
|                       | test_corrupt_backup_config, test_board_regs                                             |
| `test_security.py`    | test_device_setup, test_authentication, test_revoke_key                                 |
| `test_board.py`       | test_board, test_board_status (+ a failed step of `board_setup()`, F-178)               |
| `test_cli.py`         | test_cli, test_reset                                                                    |
| `test_stream.py`      | new: stream bounds (F-053, F-054)                                                       |
| `test_commands.py`    | new: boot status, erase, end of life, bpak read, F-072                                  |
| `test_log.py`         | new: `PB_CMD_LOG_READ` and `punchboot log` (in chunks, wrapped)                         |
| `test_compat.py`      | new: golden bpak images, bpak ids, Python mirrors of wire values (D-008), F-076 refused |
| `test_transcripts.py` | new: recorded wire transcripts served to the current host (D-008, guard 4)              |
| `test_in_place.py`    | new: the in place source on the host's flash: boot, placement, verification (F-253)     |
| `test_pbstate.py`     | new: the pbstate tool's command line on pb-host's disk, its library's symbols, a build  |
|                       | with default partitions (F-221, F-222, F-010)                                           |

Unit tests, by module under test:

| Test                                          | Module                       | Fakes                            |
| --------------------------------------------- | ---------------------------- | -------------------------------- |
| `test_bio.c`, `test_gpt.c`, `test_fuse_bio.c` | `bio.c`, `gpt.c`,            | ramdisk                          |
|                                               | `test_fuse_bio.c`            |                                  |
| `test_bpak.c`, `test_der.c`, `test_wire.c`,   | `bpak.c`, `der_helpers.c`,   | clock (`test_delay.c`)           |
|                                               | `wire.c`,                    |                                  |
| `test_delay.c`                                | `delay.c`                    |                                  |
| `test_crypto.c`, `test_mbedtls.c`             | `crypto.c`, `mbedtls_pb.c`   | providers / none (real keys and  |
|                                               |                              | the auth token)                  |
| `test_rot.c`, `test_device_uuid.c`            | `rot.c`, `slc.c`,            | board callbacks, unique id       |
|                                               | `device_uuid.c`              |                                  |
| `test_ab_state.c`, `test_boot.c`              | `ab_state.c`, `boot.c`       | ramdisk, image helpers, boot     |
|                                               |                              | driver. In place: `static.c`,    |
|                                               |                              | the ramdisk's memory as the map  |
| `test_pbstate.c`                              | `tools/pbstate`'s            | `sync()`, which counts; two      |
|                                               | `pbstate.c` (libpbstate)     | files in a temporary directory   |
|                                               |                              | are the copies                   |
| `test_static_ptbl.c`                          | `static.c`                   | ramdisk                          |
| `test_policy.c`                               | `policy.c`,                  | ramdisk                          |
|                                               | `policy_fixed.c`,            |                                  |
|                                               | `policy_chain.c`             |                                  |
| `test_fh.c`                                   | `fh.c`, `dtb.c` (not in      | arch (the cache); a receiver of  |
|                                               | pb-host), once per register  | the test's own reads the list in |
|                                               | convention                   | a region the test maps           |
| `test_tfa.c`                                  | `atf.c` (not in pb-host),    | arch (ARMv8-A's names, the ID    |
|                                               | twice: the `bl_params` list  | register); pins what BL31 reads, |
|                                               | in pb's data and in the      | field by field                   |
|                                               | platform's memory            |                                  |
| `test_tfa_image.c`                            | `image.c`, `dtb.c`, `atf.c`: | as `test_image.c` and            |
|                                               | the 8MP EVK's and the MEK's  | `test_tfa.c`                     |
|                                               | boot                         |                                  |
| `test_image_helpers.c`                        | `image_helpers.c`            | hash, DSA, root of trust (memory |
|                                               |                              | regions via `--defsym`)          |
| `test_image.c`                                | `image.c`                    | arch, SLC, root of trust, device |
|                                               |                              | UUID; builds DTBs with libfdt    |
| `test_cm.c`                                   | `cm_main.c`                  | transport, boot, root of trust,  |
|                                               |                              | SLC, crypto providers            |
| `test_usb_transport.c`                        | `pb_dev_cls.c`, `usbd.c`:    | the USB device controller        |
|                                               | the transport on USB         |                                  |
| `test_main.c`                                 | `main.c`: the loop over the  | platform, board set up, boot;    |
|                                               | tasks, twice: with three     | three tasks                      |
|                                               | tasks and with none; the     |                                  |
|                                               | reset after a platform       |                                  |
|                                               | failure                      |                                  |
| `test_plat_imxrt.c`                           | `plat/imxrt/plat.c`,         | the SoC's registers in a         |
|                                               | `imx_gpt.c`, `imx_wdog.c`:   | register file; the fuses, the    |
|                                               | the port against             | HAB, Mbed TLS                    |
|                                               | `platform.md`                |                                  |
| `test_plat_imx8x.c`                           | `plat/imx8x/plat.c`, its     | a model of the SCFW behind its   |
|                                               | SCFW client (`sci/`),        | messaging unit; the fuses, the   |
|                                               | `imx_gpt.c`                  | RoT and SLC helpers, the MMU     |
|                                               |                              | (`fakes/xlat.c`)                 |
| `test_imx8x_dry_run.c`                        | `plat/imx8x/`'s fuse, RoT    | the SCFW's fuse and SECO calls   |
|                                               | and SLC helpers, with the    |                                  |
|                                               | fuse dry run                 |                                  |
| `test_mmc_core.c`                             | `mmc_core.c` (boot partition | fake card                        |
|                                               | updates, power loss)         |                                  |
| `test_mmc_emmc.c`                             | `mmc_core.c`, `mmc_emmc.c`   | e.MMC model, `hal_model`         |
| `test_mmc_sd.c`                               | `mmc_core.c`, `mmc_sd.c`     | SD card model, `hal_model`       |
| `test_imx_usdhc.c`                            | `imx_usdhc.c`, `mmc_core.c`  | uSDHC model, e.MMC and SD models |
| `test_mmc_models.c`                           | the card models              | scripted host                    |

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
- Authentication lasted until reset, not per connection (F-185, fixed). The session ends with the
  transport (`command-mode.md` 3.1), and on the host's socket a client that closes is lost
  (`usb.md` 2.6), so each connection starts without authentication and without a stream.
  `test_authentication_ends_with_the_connection` checks it, and the bench's reset authenticates
  on a locked device. On USB, separate CLI invocations still share a session: closing and opening
  the device is no unplug and no bus reset.
