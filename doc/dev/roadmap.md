# Roadmap (DRAFT, under discussion)

Guiding idea: first make it cheap and safe to change things (a reproducible build and
test loop that Claude can run locally), then fix, then modernize. The bpak format is
frozen (D-001).

## Phase 0: Reproducible dev loop (enabler)

- A container (Dockerfile/devcontainer) with arm-none-eabi + aarch64 toolchains, QEMU,
  pinned Mbed TLS sources, the bpak CLI and Python deps.
- One command: `scripts/dev test` (or similar) that builds and runs `make check` inside it.
- Make the tests parallel-safe and re-enable the skipped ones (F-045, F-034).
- Fix `build.yml` (F-040) to use the same container, so CI and local runs match.

## Phase 1: Security and correctness fixes

- F-001..F-008, each with a regression test in the QEMU suite where possible.
- Decide the SLC EOL semantics (F-002).

## Phase 2: Dependencies

- Mbed TLS 3.4 → 3.6 LTS (F-020); trim `mbedtls_config.h`.
- Re-sync vendored bpak to v0.9.3 without changing the format (F-011, F-021). Add a sync script and record the version.
- Record the version of each vendored component in a manifest (F-022).

## Phase 3: Host tools

- PEP 621 `pyproject.toml` with `[build-system]` (F-042).
- `_punchboot.pyi` stubs, mypy in CI, ruff-format (F-043).
- pytest unit tests for the Python layer, and a wheel smoke test in cibuildwheel (F-041).
- Fix the socket transport (F-003) and the wrapper issues (F-009).
- Remove the duplicated constants and exception lists (F-035), possibly by generating them from `wire.h`/`error.h`.
- Factor out the repeated command boilerplate in the C API.

## Phase 4: Firmware cleanup

- Remove dead libc/xlat files and stale include paths (F-030, F-031).
- Prune or finish boards and defconfigs (F-033). Put every board in the Kconfig `choice`.
- Retire the deprecated commands (F-032), keeping the enum numbering stable.

## Phase 5: Docs

- Refresh the Sphinx docs or replace them (F-044). Document the CLI, Python API, wire protocol and porting guide.

## Later / to discuss

- Build system: keep Make + Kconfiglib, or move to CMake/Meson?
- Wire protocol version field and capability negotiation.
- Anti-rollback version counter (fuse-backed).
- PyPI trusted publishing. Linux aarch64 wheels.
- bpak format changes (explicitly deferred).
