# Punchboot

Secure bootloader for embedded ARM (i.MX6UL/8M/8X/RT) plus host tooling
(C library + Python package/CLI `punchboot`, `pbstate`). Images use the bpak
format; a vendored subset lives in `src/lib/bpak.c` + `include/bpak/`
(upstream: `../bpak`).

**Never break backward compatibility of the wire protocol or the bpak image
format** (D-008). Old hosts must work with new devices, new hosts with old
devices, and every image an old release accepted must still be accepted.
Only security comes before it, with a decision entry (`doc/principles.md`).
Read `doc/system/compatibility.md` before changing either.

## Context: load on demand

Do not read these up front. Open only what the task needs.

| File                               | Read when                                                                  |
| ---------------------------------- | -------------------------------------------------------------------------- |
| `doc/dev/status.md`                | **Start of every session.** Current focus, open threads, next steps        |
| `doc/README.md`                    | The map of the documents: the levels, the folders, each document's state   |
| `doc/process.md`                   | **Before writing or changing a contract**, and to find where a change      |
|                                    | starts                                                                     |
| `doc/principles.md`                | Designing or reviewing anything, and when goals conflict: what wins        |
| `doc/system/security.md`           | **Anything security rests on**: the threat model and the rules the         |
|                                    | contracts cite                                                             |
| `doc/system/system.md`             | pb's outside interfaces, the life cycle states, what pb promises           |
| `doc/system/compatibility.md`      | **Any change to `wire.h`/`error.h`, command mode, the host API, bpak,**    |
|                                    | **or image handling**                                                      |
| `doc/architecture/boot.md`         | **The boot flow**: boot source policies (A/B), load and verify, handovers  |
|                                    | (`src/boot/`)                                                              |
| `doc/architecture/command-mode.md` | **Command mode**: the session, the gate, each command and its exchange     |
|                                    | (`src/cm/`, `wire.h`'s behaviour)                                          |
| `doc/interfaces/platform.md`       | **Changing or adding a platform** (`src/plat/`, `include/plat/`, `plat.h`, |
|                                    | link scripts)                                                              |
| `doc/interfaces/board.md`          | **Changing or adding a board** (`src/board/`, `board.h`, `src/board.c`,    |
|                                    | out-of-tree boards)                                                        |
| `doc/interfaces/drivers.md`        | **Changing or adding any driver** (`src/drivers/`): set up and stop,       |
|                                    | timeouts, errors, the DMA and cache rule                                   |
| `doc/interfaces/bio.md`            | Block devices: `bio`, the storage drivers, the partition tables            |
| `doc/interfaces/crypto.md`         | Hashing and signature verification: `crypto.h`, the CAAM, Mbed TLS         |
| `doc/interfaces/usb.md`            | Command mode's transport, the USB stack and the device controllers         |
| `doc/ports/plat-imx.md`            | Working on an i.MX port: its SoC facts, sources, conformance to            |
|                                    | `platform.md`                                                              |
| `doc/ports/imx8m-ddr.md`           | i.MX8M DRAM init (uMCTL2, DWC LPDDR4 PHY), its golden trace test           |
| `doc/guides/testing.md`            | Writing or running tests (host unit/integration tests)                     |
| `doc/guides/build-and-test.md`     | Building firmware, the dev container, CI, releasing                        |
| `doc/guides/bpak.md`               | Anything touching bpak or image signing/verification                       |
| `testbench/README.md`              | The Buildroot Linux test bench (boot/root images for the boards), adding a |
|                                    | board, boot time                                                           |
| `testbench/simple/README.md`       | The simple target test bench (bare metal test application, pytest runner), |
|                                    | MCU boards                                                                 |
| `doc/dev/roadmap.md`               | Planning or picking the next piece of work                                 |
| `doc/dev/findings.md`              | Fixing bugs / tech debt (IDs like `F-012`); the fixed ones are in          |
|                                    | `findings-closed.md`                                                       |
| `doc/dev/decisions.md`             | Before changing direction on something already decided                     |
| `doc/dev/log/`                     | Only the newest entry, and only if `status.md` is not enough               |

Skip unless the task is about them: `pki/` (test keys), vendored code
(`scripts/kconfiglib.py`, `src/lib/{fdt,uuid,libc}`, `src/plat/imx8x/sci`,
`src/boot/atf-interface`, `include/xlat_tables`), binary files under `doc/`.

## Essentials

- Firmware: C99 freestanding, return `PB_OK` or `-PB_ERR_*`, log via `LOG_ERR/INFO/DBG`.
- Format: clang-format (C), ruff (Python, `select = ["ALL"]`). `pre-commit run --all-files` before committing.
- Keep the boot path short (D-023): what only command mode uses is set up on the first call of
  `cm_main_task()`, not in `main()` or the platform/board init. Defer whatever can wait.
- Tests: `scripts/pbdev test` (~10 s once built). Firmware: `scripts/pbdev firmware jiffy`.
- Release: bump `version.txt`, then tag `vX.Y.Z` on master. The tag publishes to PyPI.
- Author email is `jonas@blxt.se`: commits and the copyright header of new files
  (`Copyright (C) <year> Jonas Blixt <jonas@blxt.se>`).
- A new board that runs Linux (or a Linux board new to the test bench) gets a test bench defconfig,
  and both lists in `testbench/README.md`, "Adding a board", are done for it without being asked:
  bring-up, then boot time (measure, slim kernel and DTS, `initcall_debug`, compression), each step measured
  and recorded.
- Commits that fix a bug start with `bugfix:` (`bugfix: drivers: timer: ...`). Keep the fix
  minimal and in its own commit; refactoring, cosmetics and new functionality go in separate
  commits after it, and each board or platform fix gets its own commit.

## Keeping context current

When a piece of work ends, or before the session ends:
1. Update `doc/dev/status.md` (keep it under ~60 lines; move history to the log).
2. Append to `doc/dev/log/YYYY-MM-DD.md`: what changed, why, what's left.
3. Record decisions in `decisions.md`, and add or close items in `findings.md`.
