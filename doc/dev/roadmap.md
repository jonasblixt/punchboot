# Roadmap (DRAFT, under discussion)

Guiding idea: first make it cheap and safe to change things (a reproducible build and
test loop that an agent can run locally), then fix, then modernize. The bpak format is
frozen (D-001).

## Phase 0: Reproducible dev loop (enabler)

- A container (Dockerfile/devcontainer) with arm-none-eabi + aarch64 toolchains, QEMU,
  pinned Mbed TLS sources, the bpak CLI and Python deps.
- One command: `scripts/dev test` (or similar) that builds and runs `make check` inside it.
- Make the tests parallel-safe and re-enable the skipped ones (F-045, F-034).
- Fix `build.yml` (F-040) to use the same container, so CI and local runs match.

## Phase 1: Security and correctness fixes

- **Compatibility guards first (D-008, `compatibility.md`)**, before fixes that touch the wire or
  image handling (F-055, F-075, the bpak re-sync). In order of value per effort:
  1. Done: frozen layout file `tests/compat/abi.txt`, only-grows check in CI and pre-commit,
     `_Static_assert`s on the struct sizes (`compatibility.md`, Guards).
  2. Done for bpak 0.9.3 and 1.0.1: golden images in `tests/compat/bpak/`, booted from RAM and from a
     partition by `test_compat.py`, plus pinned ids and the Python mirrors. Still open: images
     from older bpak CLIs (0.5-0.8 need Mbed TLS 2.x to build).
  3. Old hosts against the current device: a CI job installs released `punchboot` wheels from PyPI
     (2.0.0 and up, as available) and runs a read-mostly subset of the integration tests against
     today's pb-host.
  4. Recorded transcripts: raw command/response bytes for each command, captured once, replayed
     against pb-host (old host → new device) and served by a fake socket device to the current
     host library (new host → old device). From the first release after this branch, also keep
     each release's pb-host to test the current wheel against it.
  5. CODEOWNERS + a PR template checkbox for the files listed in `compatibility.md`.
- F-001..F-008, each with a regression test in the QEMU suite where possible.
- ~~Decide the SLC EOL semantics (F-002).~~ Intended: command mode is open in EOL (D-011).
- ~~Device tree without the pb properties (F-071, wanted by Jonas).~~ Done (`1e8f5601`, `39276b9c`):
  `CONFIG_BOOT_DTB_HEADROOM`, the README's "Booting Linux: the device tree"; the boot drivers' shared
  code waits for a review of the boot setup (Jonas). Today the board DTBs must already
  have `/chosen` and `pb,slc-available-keys`, and enough free space for the properties pb sets.
  Instead, pb should add whatever is missing:
  - Create `/chosen` and the `pb,*` properties if they don't exist, and treat a missing
    `pb,slc-available-keys` as nothing to delete.
  - Reserve spare RAM after the DTB and grow it there (`fdt_open_into` with the extra size) before
    patching. The board config says how much. The grown DTB must not overlap the other parts or pb's
    own memory, and `fdt_totalsize` must be checked against the part size (F-065).
  - Cost: growing the DTB moves its structure and strings blocks, which adds boot time. Measure it on
    jiffy and document it in the user docs (porting guide / Linux boot), including how to avoid it by
    building the DTB with the properties and padding (`dtc -p`).
  - Tests: unit tests in `test_linux.c` (no `/chosen`, no pb properties, no free space), and at
    least one test on a jiffy board: boot a DTB without the pb properties and check
    `/proc/device-tree/chosen/pb,*` from Linux. There is no hardware test setup in the repo yet, so
    this one is manual unless we add one.

## Phase 2: Dependencies

- Mbed TLS 3.4 → 3.6 LTS (F-020); trim `mbedtls_config.h`.
- Done (2026-11-08, D-024): vendored bpak re-synced to v1.0.1 with `scripts/sync-bpak` (F-011, F-021).
- Record the version of each vendored component in a manifest (F-022).

## Phase 3: Host tools

- mypy in CI, ruff-format (F-043).
- pytest unit tests for the Python layer (F-041).
- Remove the duplicated constants (F-035): `pbtest.py`'s `PB_CMD_*` can use `punchboot.wire`.
- Done in 3.0.0 (D-021): the host is pure Python, which removed the C wrapper issues (F-009),
  the command boilerplate and the duplicated exception lists, and gave a wheel smoke test.
- `pbstate` (tool and library) for every boot source policy, not only A/B (Jonas, 2026-11-11):
  one freestanding core with I/O callbacks shared with pb's firmware (as `src/wire.c` is with the
  host), used on Linux, Zephyr and bare metal, the simple target's copy replaced; extensions from
  `rz[]` where zero is today's behaviour (D-008); Arm's FWU metadata as a model for the concepts,
  not the format. Not Linux specific, it never was. The name stays: scripts and products call the
  tool and link `libpbstate`, and "pb's state" is still what it is (decided 2026-11-11).

## Phase 4: Firmware cleanup

- Remove dead libc/xlat files and stale include paths (F-030, F-031).
- Prune or finish boards and defconfigs (F-033). Put every board in the Kconfig `choice`.
- Retire the deprecated commands (F-032), keeping the enum numbering stable.

## Phase 5: Docs

- User docs (the Sphinx tree is gone, D-020): document the CLI, Python API, wire protocol and porting guide, starting from the README (F-044).

## From the level 1 review (2026-11-11, D-035)

To do (Jonas):

- **Anti-rollback**: a version floor, fuse-backed. A version below the floor fails verification, so
  the boot source policies don't change; it may change what a fallback may boot (a CVE).
- **Examples for `password_auth()`**: a few good ones, such as a secret in the RPMB. A board's own
  implementation stays allowed (`board.md` 4.5, F-004).
- **The compatibility floor is v1.0.0**: the guards test hosts and firmware from the 1.0.0 release
  on (Phase 1, item 3 said 2.0.0).

To discuss (Jonas):

- **EOL** (`system.md` Open (1)): the SoC's end of life state (NXP's field return), behind Kconfig;
  a product may not allow it at all.
- **Encrypted images** (`security.md` 2): confidentiality isn't protected today.
- **Authentication methods** (`security.md` 4.7): authentication is pluggable. A token limited in
  time, on platforms with secure time. Capabilities: a token for a return analysis that only
  allows reading certain logs.
- **Boot failure records** (`system.md` 5.4): counters for failed boots that the system reads
  later, and error logs written to flash in some cases, for the analysis of returned devices.
- **Another image format** (`system.md` 3): bpak solves more than pb needs; a simpler format of
  pb's own, loosely coupled, and whether one build accepts several formats. With it: the keystore
  without the round trip through the bpak CLI at build time (key ids unchanged, the bpak input
  still accepted). The parked `pbi-image-format` branch belongs here. What the boot flow needs
  from a format is `boot.md` 4.4 (D-044); with the format come its interface in the code, the part
  ids a board names and the host's command.
- **Erase on the wire**: Jonas may want to change it again (F-075 is accepted, no product used it).
- pb's own update with a rollback (`security.md` 5; my addition).

## Later / to discuss

- Build system: keep Make + Kconfiglib, or move to CMake/Meson?
- Wire protocol version field and capability negotiation. Only next to `PBL0` and the caps
  command, never replacing them (D-008).
- PyPI trusted publishing.
- bpak format extensions. Only backward compatible ones (D-008).
- Low prio: move the whole repo to `jonas@blxt.se`. Simple part: the copyright headers that still
  say `jonpe960@gmail.com` (one commit). Rewriting the history of the ~1000 old commits would
  change every hash and tag (the PyPI releases point at tags), so decide that separately. The
  Coverity upload in `build.yml` also uses the old email and the `jonpe960/punchboot` project.
