# Status

_Last updated: 2026-10-22_

## Current focus

**New host-native test setup** (D-004, D-005; details in `testing.md`). The skeleton works end to end:
- punchboot runs as a Linux process (`pb-host`)
- CMocka unit tests (`test_gpt`, 7 cases)
- pytest integration tests (`test_part.py`, 4 cases), run through the real `punchboot` package
- ASan/UBSan clean, native gcov coverage
- a full run takes under 1 s

`scripts/pbdev host-test` runs it all.

The old QEMU suite still exists (`scripts/pbdev test`) and stays until the porting is done.
`build.yml` is deliberately unchanged for now (Jonas).

## Next steps

1. Port the remaining 42 bash tests to pytest (tracked in `testing.md`). Group them by area:
   boot/bpak, A/B and rollback, GPT corruption, auth/SLC/revoke, board, part read/write/verify.
2. Add unit tests for `ab_state`, `image_helpers`, `wire`, `der_helpers`, `rot`/`slc`.
3. Wire the host tests into CI, then retire the QEMU suite and the gcov hack.
4. Then Phase 1: security fixes F-001..F-005, each with a test.

## Open questions (for Jonas)

- Is open command mode in SLC EOL intended? (F-002)
- Which boards are still in use? (F-033)
- Mbed TLS: 3.6 LTS or 4.x?
