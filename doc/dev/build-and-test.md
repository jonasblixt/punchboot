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

## Lint

`pre-commit run --all-files` runs pre-commit-hooks, mypy, ruff-check and clang-format.
pre-commit.ci runs the same hooks on PRs and autoupdates them weekly. With `select = ["ALL"]`,
a ruff bump can break the check (it happened in 2.7.0).

## CI (`.github/workflows/`)

- `build.yml` (D-006). All jobs run through `scripts/pbdev`, so the image is built in every job:
  - **host-tests:** default and sanitizer builds.
  - **coverage:** gcovr XML, uploaded with `codecov/codecov-action` using OIDC (`id-token: write`),
    so no `CODECOV_TOKEN` secret is needed. The upload is non-fatal: check the job log if Codecov
    looks stale.
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
