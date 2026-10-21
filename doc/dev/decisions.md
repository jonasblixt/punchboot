# Decisions

Short records of decisions and why they were made. Add new ones at the bottom,
and never edit old ones; supersede them with a new entry instead.

## D-001: Keep the bpak file format unchanged for now (2026-10-22)

The modernization works around the bpak format. Refactoring how punchboot
*uses* bpak is fine, as long as the on-disk and on-wire `struct bpak_header`
stays compatible.

## D-002: AI working context lives in `doc/dev/` (2026-10-22)

`CLAUDE.md` stays small and points to files that are loaded on demand.
`doc/source/` remains the user-facing Sphinx documentation. `doc/dev/` is
developer and AI context: status, roadmap, findings, decisions, architecture
notes and a dated log.

## D-003: Release flow (2026-10-22)

Releases are made from master. Bump `version.txt` in a PR, then create a GitHub
release with tag `vX.Y.Z` and generated notes. The tag triggers
`punchboot-tools.yml`, which uploads to PyPI.

## D-004: Host-native test setup replaces the QEMU suite (2026-10-22)

Tests run on the host: CMocka unit tests, plus pytest integration tests against punchboot built
as a native process (`plat/host`, `board/host`). They are built with CMake/CTest, while the
firmware for targets stays on Make + Kconfig. The 43 bash/QEMU tests are ported one by one, and
the QEMU suite is removed when everything is covered. A small QEMU smoke tier for
arch-specific code may come back later.
Why: the QEMU suite was slow (~15 min), serial, and broke with toolchain upgrades (the gcov hack).
It also couldn't use sanitizers or a debugger.

## D-005: Tooling for the host tests (2026-10-22)

CMake + CTest, CMocka for C unit tests, and pytest driving the real `punchboot` Python package for
integration tests (this also tests the host library and the Python bindings).

## D-006: CI runs the host tests in the dev container; QEMU suite removed (2026-10-22)

`build.yml` runs everything through `scripts/pbdev`, the same as locally: the host tests (default
and sanitizers), coverage (codecov action), a compile check of the firmware for the boards that
build (jiffy, imx8dxlevk), and Coverity on master. The bash/QEMU suite, `src/board/test`,
`src/plat/qemu` and `configs/test_defconfig` were removed once all tests were ported. The virtio
drivers are kept for now; they would be needed for a QEMU smoke tier.
