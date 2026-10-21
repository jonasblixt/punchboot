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
