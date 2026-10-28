# Punchboot

Secure bootloader for embedded ARM (i.MX6UL/8M/8X/RT) plus host tooling
(C library + Python package/CLI `punchboot`, `pbstate`). Images use the bpak
format; a vendored subset lives in `src/lib/bpak.c` + `include/bpak/`
(upstream: `../bpak`).

**Never break backward compatibility of the wire protocol or the bpak image
format** (D-008). Old hosts must work with new devices, new hosts with old
devices, and every image an old release accepted must still be accepted.
Read `doc/dev/compatibility.md` before changing either.

## Context: load on demand

Do not read these up front. Open only what the task needs.

| File | Read when |
|---|---|
| `doc/dev/status.md` | **Start of every session.** Current focus, open threads, next steps |
| `doc/dev/roadmap.md` | Planning or picking the next piece of work |
| `doc/dev/findings.md` | Fixing bugs / tech debt (IDs like `F-012`) |
| `doc/dev/decisions.md` | Before changing direction on something already decided |
| `doc/dev/arch-firmware.md` | Working in `src/`, `include/` |
| `doc/dev/arch-host-tools.md` | Working in `tools/`, wire protocol, Python package |
| `doc/dev/testing.md` | Writing or running tests (host unit/integration tests) |
| `doc/dev/build-and-test.md` | Building firmware, the dev container, CI, releasing |
| `doc/dev/imx8m-ddr.md` | i.MX8M DRAM init (uMCTL2, DWC LPDDR4 PHY), its golden trace test |
| `doc/dev/bpak.md` | Anything touching bpak or image signing/verification |
| `doc/dev/compatibility.md` | **Any change to `wire.h`/`error.h`, command mode, the host API, bpak, or image handling** |
| `doc/dev/log/` | Only the newest entry, and only if `status.md` is not enough |

Skip unless the task is about them: `pki/` (test keys), vendored code
(`scripts/kconfiglib.py`, `src/lib/{fdt,uuid,libc}`, `src/plat/imx8x/sci`,
`src/boot/atf-interface`, `src/drivers/fs/lfs.c`, `include/xlat_tables`),
`doc/source/` (Sphinx docs), binary files under `doc/`.

## Essentials

- Firmware: C99 freestanding, return `PB_OK` or `-PB_ERR_*`, log via `LOG_ERR/INFO/DBG`.
- Format: clang-format (C), ruff (Python, `select = ["ALL"]`). `pre-commit run --all-files` before committing.
- Tests: `scripts/pbdev test` (~10 s once built). Firmware: `scripts/pbdev firmware jiffy`.
- Release: bump `version.txt`, then tag `vX.Y.Z` on master. The tag publishes to PyPI.
- Author email is `jonas@blxt.se`: commits and the copyright header of new files
  (`Copyright (C) <year> Jonas Blixt <jonas@blxt.se>`). Don't copy the old `jonpe960@gmail.com`
  from existing headers.
- Commits that fix a bug start with `bugfix:` (`bugfix: drivers: timer: ...`). Keep the fix
  minimal and in its own commit; refactoring, cosmetics and new functionality go in separate
  commits after it, and each board or platform fix gets its own commit.

## Keeping context current

When a piece of work ends, or before the session ends:
1. Update `doc/dev/status.md` (keep it under ~60 lines; move history to the log).
2. Append to `doc/dev/log/YYYY-MM-DD.md`: what changed, why, what's left.
3. Record decisions in `decisions.md`, and add or close items in `findings.md`.
