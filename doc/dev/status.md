# Status

_Last updated: 2026-10-22_

## Current focus

Modernization, on branch `claude-work`. **Phase 0 (reproducible dev loop) is in progress.**
The dev container works locally: `scripts/pbdev test` runs all 43 QEMU tests (all pass) and
`make gcovr` reports 54% line coverage.

Nothing on this branch is committed yet.

## Done in Phase 0 so far

- `docker/Dockerfile`, `scripts/pbdev`, `scripts/ci/test.sh` (see `build-and-test.md`).
- Fixed what the newer toolchain exposed: F-012 (A/B state NULL cfg crash), F-013 and F-014 (gcov
  format for GCC 12+ and 14), and the click >= 8.2 exit codes (F-046). gcovr pinned to 8.3.

## Next steps

1. Commit the bootstrap and Phase 0 work (ask Jonas how to split it).
2. Switch `.github/workflows/build.yml` to build and use the container (F-040). Decide whether to
   publish the image (ghcr.io) or build it in CI each time.
3. Re-enable the skipped tests (F-034).
4. Then Phase 1: security fixes F-001..F-005.

## Open questions (for Jonas)

- Is open command mode in SLC EOL intended? (F-002)
- Which boards are still in use? (F-033)
- Mbed TLS: 3.6 LTS or 4.x?
- Keep Make + Kconfiglib?
