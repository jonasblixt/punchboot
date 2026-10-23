# Todo

Ordered by priority. "Reported" findings are unverified; check them before fixing.

1. Security: F-004, the imx8 example boards accept any password prefix.
2. Security: F-055 + F-075, `bio_erase` has no range or writable check and ignores `first_lba`; pre-2.0 hosts get a silent no-op erase. Fix together.
3. Security: F-001, `slc_set_eol()` checks `set_configuration_locked` but calls `set_eol` (NULL call).
4. Bug: F-081, the cache invalidate loses dirty data on shared lines (CAAM output on jiffy).
5. Bug: F-095, `cmd_board` ignores the host's `response_buffer_size`; an oversized response desyncs the host.
6. Bug: F-008, return values of the `mbedtls_sha*` calls are ignored (reported).
7. Bug: F-007, `ab_state` does I/O with a negative handle when a state partition is missing, and more (reported).
8. Bug: F-070, a command-mode boot can use a stale `header_ptr` after a failed in-memory boot.
9. Bug: F-066, `der_ec_public_key_data` compares with a data-supplied length, and `s -= 2` can underflow.
10. Security: F-065, boot-path hardening for signed input: overlap wrap, load regions, DTB size, anti-rollback (reported).
11. Bug: F-006, GPT primary and backup tables are never compared (reported).
12. Bug: F-064, GPT header `last_lba` is `last - 32`, should be `last - 33`.
13. Bug: F-089, vendored `snprintf` `%x` overflows small buffers.
14. Bug: F-071, pb adds missing DTB properties (roadmap Phase 1).
15. Bug: F-074, flag checks pass on the error code after a failed STREAM_INITIALIZE (harmless).
16. Bug: F-068, `bpak_del_meta` writes past `meta[]` (fix upstream).
17. Bug: F-011, `bpak_add_transport_meta` uses the wrong size (fixed upstream in bpak v0.9.2, latent).
18. Board: F-083, `punchboot part read` fails with an I/O error on jiffy.
19. Board: F-084, measure the GPT tick on the imx8x EVK (imx8dxlevk), expect 1.000 ticks/us.
20. Board: F-084, measure the GPT tick on the imx8m EVK (imx8mevk doesn't build yet, F-033).
21. Board: F-084, measure the GPT tick on the i.MX RT EVK (imxrt1060evk doesn't build yet, F-051), checks the perclk change in e93a767.
22. Board: F-079/F-078, eMMC reset is on the wrong pin on jiffy, and SRST falls back to the serial downloader.
23. Board: F-090, imx8dxlevk RWX link warning.
24. Compat: F-076, honour `dont-hash` (has an xfail test).
25. Compat: F-087, a bad payload is a generic error on the wire.
26. Compat: F-077, the host doesn't handle `chunk_transfer_max_bytes == 0` from old devices.
27. Host: F-009, `log_cb` ignores `PyObject_CallMethod` failure, and the GIL is held during USB I/O (reported).
28. Host: F-010, `pbstate` CMake sets `BACKUP_PART` from the primary part (reported).
29. Tests: F-067, SIGPIPE, orphaned pb-host, `-m32`.
30. CI: Check the Coverity job on master (PRs #61 and #62 are merged).
31. CI: Merge PR #64 (`jiffy-fixes` → master), which also carries #63 (`jtag-debugging`).
32. CI: Consider `-Werror` for the host build now that F-049 is fixed.
33. Compat: Guard 3, old PyPI wheels against pb-host.
34. Compat: Guard 4, recorded transcripts.
35. Compat: Guard 5, CODEOWNERS.
36. Compat: Golden images from older bpak CLI releases.
37. Repo: Split the jiffy HAB signing commits off `compat-guards` onto their own branch.
38. Ethernet: Evaluate the ethernet PoC (branch `enet-poc`).
39. Decide: Which boards are still in use? Only jiffy and imx8dxlevk build (F-051, F-033).
40. Decide: Keep the virtio drivers for a future QEMU smoke tier? (F-052)
41. Decide: Mbed TLS 3.6 LTS or 4.x?
42. Decide: The compat floor, the oldest released host and firmware the guards test against.
