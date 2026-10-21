# Punchboot

Secure bootloader for embedded ARM (i.MX6UL/8M/8X/RT, QEMU) plus host tooling
(C library + Python package/CLI `punchboot`, `pbstate`). Images use the bpak
format; a vendored subset lives in `src/lib/bpak.c` + `include/bpak/`
(upstream: `../bpak`). The bpak file format is out of scope for now.

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
| `doc/dev/build-and-test.md` | Building, running tests, CI, releasing |
| `doc/dev/bpak.md` | Anything touching bpak or image signing/verification |
| `doc/dev/log/` | Only the newest entry, and only if `status.md` is not enough |

Skip unless the task is about them: `pki/` (test keys), vendored code
(`scripts/kconfiglib.py`, `src/lib/{fdt,uuid,libc}`, `src/plat/imx8x/sci`,
`src/boot/atf-interface`, `src/drivers/fs/lfs.c`, `include/xlat_tables`),
`doc/source/` (Sphinx docs), binary files under `doc/`.

## Essentials

- Firmware: C99 freestanding, return `PB_OK` or `-PB_ERR_*`, log via `LOG_ERR/INFO/DBG`.
- Format: clang-format (C), ruff (Python, `select = ["ALL"]`). `pre-commit run --all-files` before committing.
- Build/test in the dev container: `scripts/pbdev test` (~10–15 min, log to a file). Details in `doc/dev/build-and-test.md`.
- Release: bump `version.txt`, then tag `vX.Y.Z` on master. The tag publishes to PyPI.

## Keeping context current

When a piece of work ends, or before the session ends:
1. Update `doc/dev/status.md` (keep it under ~60 lines; move history to the log).
2. Append to `doc/dev/log/YYYY-MM-DD.md`: what changed, why, what's left.
3. Record decisions in `decisions.md`, and add or close items in `findings.md`.
