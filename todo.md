# Todo

1. CI: Check the Coverity job on master (PRs #61 and #62 are merged).
2. CI: Merge PR #64 (`jiffy-fixes` → master), which also carries #63 (`jtag-debugging`).
3. CI: Consider `-Werror` for the host build now that F-049 is fixed.
4. Security: F-001, `slc_set_eol()` checks `set_configuration_locked` but calls `set_eol` (NULL call).
5. Security: F-002, every command is open in SLC EOL (intended?).
6. Security: F-004, the imx8 example boards accept any password prefix.
7. Security: F-005, `cmd_auth` passes the host-supplied size along unchecked.
8. Security: F-063, `cmd_board` passes `request_size` to the board unbounded.
9. Security: F-055, `bio_erase` has no range or writable check and ignores `first_lba`.
10. Compat: Guard 3, old PyPI wheels against pb-host.
11. Compat: Guard 4, recorded transcripts.
12. Compat: Guard 5, CODEOWNERS.
13. Compat: Golden images from older bpak CLI releases.
14. Compat: F-075, pre-2.0 hosts get a silent no-op erase.
15. Compat: F-076, honour `dont-hash` (has an xfail test).
16. Compat: F-077, the host doesn't handle `chunk_transfer_max_bytes == 0` from old devices.
17. Compat: F-087, a bad payload is a generic error on the wire.
18. Core: F-070, a command-mode boot can use a stale `header_ptr` after a failed in-memory boot.
19. Core: F-071, pb adds missing DTB properties (roadmap Phase 1).
20. Core: F-066, the rest of the `der_helpers.c` fixes.
21. Core: F-089, vendored `snprintf` `%x` overflows small buffers.
22. Tests: F-067, SIGPIPE, orphaned pb-host, `-m32`.
23. Board: F-090, imx8dxlevk RWX link warning.
24. Board: F-083, `punchboot part read` fails with an I/O error on jiffy.
25. Board: F-078/F-079, jiffy SRST falls back to the serial downloader and eMMC reset is on the wrong pin.
26. Board: F-081, the cache invalidate loses dirty data on shared lines.
27. Board: F-084, `plat_get_us_tick()` runs at least 2x fast on jiffy.
28. Repo: Split the jiffy HAB signing commits off `compat-guards` onto their own branch.
29. Ethernet: Evaluate the ethernet PoC (branch `enet-poc`).
30. Decide: Which boards are still in use? Only jiffy and imx8dxlevk build (F-051, F-033).
31. Decide: Keep the virtio drivers for a future QEMU smoke tier? (F-052)
32. Decide: Mbed TLS 3.6 LTS or 4.x?
33. Decide: The compat floor, the oldest released host and firmware the guards test against.
