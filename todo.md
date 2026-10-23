# Todo

Ordered by priority. "Reported" findings are unverified; check them before fixing.

1. Security: F-005, `cmd_auth` (pre-auth) passes the host-supplied size along unchecked, and ignores the `cm_read()` result.
2. Security: F-004, the imx8 example boards accept any password prefix.
3. Security: F-063, `cmd_board` passes `request_size` to the board unbounded.
4. Security: F-055 + F-075, `bio_erase` has no range or writable check and ignores `first_lba`; pre-2.0 hosts get a silent no-op erase. Fix together.
5. Security: F-001, `slc_set_eol()` checks `set_configuration_locked` but calls `set_eol` (NULL call).
6. Bug: F-081, the cache invalidate loses dirty data on shared lines (CAAM output on jiffy).
7. Bug: F-008, return values of the `mbedtls_sha*` calls are ignored (reported).
8. Bug: F-007, `ab_state` does I/O with a negative handle when a state partition is missing, and more (reported).
9. Bug: F-070, a command-mode boot can use a stale `header_ptr` after a failed in-memory boot.
10. Bug: F-066, `der_ec_public_key_data` compares with a data-supplied length, and `s -= 2` can underflow.
11. Security: F-065, boot-path hardening for signed input: overlap wrap, load regions, DTB size, anti-rollback (reported).
12. Bug: F-006, GPT primary and backup tables are never compared (reported).
13. Bug: F-064, GPT header `last_lba` is `last - 32`, should be `last - 33`.
14. Bug: F-089, vendored `snprintf` `%x` overflows small buffers.
15. Bug: F-071, pb adds missing DTB properties (roadmap Phase 1).
16. Bug: F-074, flag checks pass on the error code after a failed STREAM_INITIALIZE (harmless).
17. Bug: F-068, `bpak_del_meta` writes past `meta[]` (fix upstream).
18. Bug: F-011, `bpak_add_transport_meta` uses the wrong size (fixed upstream in bpak v0.9.2, latent).
19. Board: F-083, `punchboot part read` fails with an I/O error on jiffy.
20. Board: F-084, `plat_get_us_tick()` runs 2.64x fast on jiffy (measured over JTAG), so every delay and timeout is short.
21. Board: F-079/F-078, eMMC reset is on the wrong pin on jiffy, and SRST falls back to the serial downloader.
22. Board: F-090, imx8dxlevk RWX link warning.
23. Compat: F-076, honour `dont-hash` (has an xfail test).
24. Compat: F-087, a bad payload is a generic error on the wire.
25. Compat: F-077, the host doesn't handle `chunk_transfer_max_bytes == 0` from old devices.
26. Host: F-009, `log_cb` ignores `PyObject_CallMethod` failure, and the GIL is held during USB I/O (reported).
27. Host: F-010, `pbstate` CMake sets `BACKUP_PART` from the primary part (reported).
28. Tests: F-067, SIGPIPE, orphaned pb-host, `-m32`.
29. CI: Check the Coverity job on master (PRs #61 and #62 are merged).
30. CI: Merge PR #64 (`jiffy-fixes` → master), which also carries #63 (`jtag-debugging`).
31. CI: Consider `-Werror` for the host build now that F-049 is fixed.
32. Compat: Guard 3, old PyPI wheels against pb-host.
33. Compat: Guard 4, recorded transcripts.
34. Compat: Guard 5, CODEOWNERS.
35. Compat: Golden images from older bpak CLI releases.
36. Repo: Split the jiffy HAB signing commits off `compat-guards` onto their own branch.
37. Ethernet: Evaluate the ethernet PoC (branch `enet-poc`).
38. Decide: Which boards are still in use? Only jiffy and imx8dxlevk build (F-051, F-033).
39. Decide: Keep the virtio drivers for a future QEMU smoke tier? (F-052)
40. Decide: Mbed TLS 3.6 LTS or 4.x?
41. Decide: The compat floor, the oldest released host and firmware the guards test against.
