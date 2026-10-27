# Build, CI, release

Tests are covered in `testing.md`.

## Firmware build

```sh
scripts/pbdev firmware jiffy          # or imx8dxlevk: the boards that build today (F-051)
```

By hand (in `scripts/pbdev shell`):

```sh
make BOARD=src/board/<x> KCONFIG_CONFIG=configs/<x>_defconfig CROSS_COMPILE=<arm-none-eabi-|aarch64-linux-gnu-> build-<x>/pb.bin
```

- Kconfig is handled by the vendored Kconfiglib. `scripts/genconfig.py` runs on **every** make
  invocation and writes `build-<board>/config.h`. `make menuconfig` and `make oldconfig` are available.
  Without `KCONFIG_CONFIG`, the config is read from `.config`.
- Kbuild-like variables (`src-y`, `asm-y`, `cflags-y`, `blobs-y`, `plat-y`) are collected from `src/*/makefile.mk` plus `$(BOARD)/makefile.mk`.
- `keystore.c` is generated with the host `bpak` CLI (`$(BPAK) generate keystore`) from `CONFIG_KEYSTORE`,
  default `pki/internal_keystore.bpak`. It can be overridden on the make command line (`CONFIG_KEYSTORE=...`).
- Outputs go to `build-<board>/`: `pb`, `pb.bin` and `pb.map`.
- The default `all` target also builds the platform images (`.imx`, signed with NXP `cst`).
  `mkimage` (u-boot-tools) and `cst` (imx-code-signing-tool 3.4.1) are in the container. jiffy
  builds and signs with the HAB test keys in `pki/`. The other boards also need vendor blobs that
  aren't in the container.
- Signed jiffy (HAB test keys): `scripts/pbdev run make BOARD=src/board/jiffy
  KCONFIG_CONFIG=configs/jiffy_defconfig CROSS_COMPILE=arm-none-eabi- all` gives
  `build-jiffy/pb_signed.imx` (boot media, 1 KiB zero pad) and `pb_signed_uuu.imx` (serial download).
- NXP's Python SPSDK (`nxpimage`) is not a replacement for `cst` on i.MX6UL/8M: its HAB support is
  i.MX RT only. It covers AHAB (i.MX8DXL/8QXP/8ULP/9).
- MBEDTLS_DIR must point to Mbed TLS 3.4.0 sources (set in the container).

## Dev container

`docker/Dockerfile` (Debian trixie) contains:
- GCC 14 toolchains: arm-none-eabi and aarch64-linux-gnu
- Mbed TLS 3.4.0 sources (`MBEDTLS_DIR`)
- the bpak v0.9.3 CLI
- gdisk and CMocka
- `file`, which Coverity's cov-build needs (F-097)
- a Python venv with click >= 8.2, semver, pytest and gcovr 8.3 (pinned: 8.7 crashes)

```sh
scripts/pbdev test [cmake args]   # host build + all tests, see testing.md
scripts/pbdev firmware <board>    # scripts/ci/build-firmware.sh
scripts/pbdev shell               # interactive
scripts/pbdev run <cmd...>
```

- The image tag is a hash of `docker/`, so it rebuilds automatically when the Dockerfile changes.
- The repo is mounted at the same path as on the host, and commands run as your uid.
- `BUILD_DIR` and `CTEST_ARGS` are passed through into the container.
- If Docker says "permission denied", the shell predates being added to the `docker` group.
  Use `sg docker -c "scripts/pbdev ..."` or log in again.

## JTAG debugging

```sh
scripts/pbdebug jiffy [elf]          # OpenOCD + gdb-multiarch, attached to the running target
scripts/pbdebug --load jiffy [elf]   # load elf into RAM over JTAG, halted at its entry point
```

- Runs on the host (needs `openocd` and `gdb-multiarch`), since OpenOCD needs the USB adapter.
  The adapter defaults to the Keil ULINK v1 (`interface/ulink.cfg`, OpenULINK firmware from the
  Debian openocd package). Set `PB_OPENOCD_ADAPTER` for another one, `PB_GDB` for another GDB.
- GDB halts the target on attach. Symbols come from `build-<board>/pb_unstripped` and are only
  right if the board runs that build. Set `CONFIG_DEBUG_SYMBOLS=y` (and a lower `CONFIG_OPTIMIZE`)
  for source level debugging. Quitting GDB resumes the target.
- Configs: `src/plat/imx6ul/openocd.cfg` (chip: upstream `target/imx6ul.cfg` with DAP r5
  `0x5ba00477`, the upstream default r4 doesn't match the silicon) and
  `src/board/<board>/openocd.cfg` (board). The board config sources the platform one.
- `--load` needs the core in the boot ROM with the MMU and D-cache off, e.g. the serial
  downloader (boot mode buttons, or any reset after JTAG activity on a closed board, F-078).
  `pb_load_prepare` (platform config) refuses otherwise, because pb doesn't invalidate the caches
  before enabling them. It turns off the I-cache, sets ARM state and SVC mode, and calls the board's
  `pb_board_load_prepare` (jiffy: DDR from the DCD). GDB `load` then takes about 27 s for pb at 3
  KB/s. Nothing is flashed, and on a closed board the image isn't HAB authenticated, so the CAAM
  keeps working with JTAG attached (unlike F-080).
- Breakpoints are hardware breakpoints (`gdb_breakpoint_override hard`, 6 on the A7): pb maps
  `.text` read-only with LPAE tables, so OpenOCD can't remove a software breakpoint once the MMU is
  on. A breakpoint hit during a USB command can make the host time out (`Error: Transfer`).
- `monitor jiffy_ddr_init` runs the DCD from `src/board/jiffy/imximage.cfg` over JTAG, e.g. when
  halted in the boot ROM's serial downloader. Checked by writing and reading back DDR.
- No `reset_config` on jiffy: an SRST reset from the adapter lands in the serial downloader (F-078).
- The watchdog only pauses while the core is halted if pb sets WDBG
  (`CONFIG_DRIVERS_IMX_WDOG_DEBUG`, on in `jiffy_defconfig`). Without it, a halt longer than the
  5 s timeout resets the board, into the serial downloader (F-078). Checked on the board: an 8 s
  halt with WDBG set (`WCR` reads `0x0a3e`) survives.
- On a closed board (HAB booted), any JTAG connection, even OpenOCD `init`, sets SNVS SV1 and drops
  the SSM to Non-Secure. Until the next power cycle the CAAM takes no jobs, so `auth token` and
  image verification fail with a timeout (F-080), and warm resets probably fail HAB and land in the serial
  downloader (F-078). Power cycle after debugging before trusting crypto again.
- The first run after plugging in the ULINK loads its firmware, and OpenOCD exits before the
  adapter re-enumerates. `pbdebug` retries.

## Lint

`pre-commit run --all-files` runs pre-commit-hooks, mypy, ruff-check and clang-format.
pre-commit.ci runs the same hooks on PRs and autoupdates them weekly. With `select = ["ALL"]`,
a ruff bump can break the check (it happened in 2.7.0).

## CI (`.github/workflows/`)

- `build.yml` (D-006). All jobs run through `scripts/pbdev`, so the image is built in every job:
  - **host-tests:** default and sanitizer builds.
  - **coverage:** the gcovr summary in the job log, and the line by line HTML report as the
    `coverage` workflow artifact (D-012). Not a gate.
  - **firmware:** jiffy and imx8dxlevk `pb.bin`.
  - **coverity:** cov-build of jiffy inside the container. Only on pushes to master in
    jonasblixt/punchboot, because it needs `COVERITY_SCAN_TOKEN`.
- `punchboot-tools.yml`: cibuildwheel v4.3.3 builds wheels for cp310–cp315 on Linux x86_64
  (manylinux_2_28), Windows amd64 and macOS x86_64/arm64, plus an sdist. Free-threaded builds are skipped.
  - `workflow_dispatch` publishes to TestPyPI.
  - A `v*` tag publishes to PyPI with the `PYPI_API_TOKEN` secret. Trusted publishing is not set up yet.
  - libusb comes from vcpkg on Windows and pinned Homebrew bottles (1.0.27) on macOS. The version is hardcoded in 3 places.

## Release

See D-003 in `decisions.md` and `doc/release-checklist.txt`.
