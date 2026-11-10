# Findings

Bugs and tech debt from the 2026-10-22 repo scan. **Verified** means a human or
Claude read the code and confirmed it. **Reported** means it came from a scan
agent and has not been checked yet; check before acting on it.

Status: `open` / `fixed (<commit>)` / `wontfix (<reason>)`.

## Security / correctness

| ID | Where | Finding | Check | Status |
|---|---|---|---|---|
| F-001 | `src/slc.c:45` | `slc_set_eol()` checks `set_configuration_locked` but calls `set_eol`. A board without `set_eol` makes a NULL call. | Verified | fixed (work-3.0.0) |
| F-002 | `src/cm/cm_main.c:688` | The auth gate only applies when `slc == PB_SLC_CONFIGURATION_LOCKED`. In `PB_SLC_EOL` every command is open. It also compares the core `slc_t` with the wire enum (the values happen to match; `_Static_assert`s in `cm_main.c` now pin that). | Verified | wontfix (intended, D-011) |
| F-003 | `tools/punchboot/socket.c:47-80` | `read()` and `write()` loops pass the full `sz` on every iteration. `write` also re-sends the original `bfr`. A short read overruns the buffer, and EOF (0) loops forever. Affects the socket transport used by the host tests. | Verified | fixed (claude-work) |
| F-004 | `src/board/imx8{qxmek,dxlevk}/board.c:~288` | `strncmp(expected, password, length)` with `length` taken from the host accepts any prefix ("i"). `LOG_DBG` prints a password that is not NUL-terminated. Example boards, but the pattern gets copied. | Verified | open |
| F-005 | `src/cm/cm_main.c:283` `cmd_auth` | Reads 1024 bytes, then passes the host-supplied `auth_cmd->size` along without bounds-checking it. | Verified: up to 65535 bytes reach `dsa_verify`/`password_auth`, past both command buffers on a 4 KiB build. A failed read was ignored too, so the stale buffer (e.g. the last good token) was checked | fixed (5370eb7): size 0 or over 1024 refused with INVALID_ARGUMENT before the first result, read errors fail the authentication; `test_cm.c` |
| F-006 | `src/drivers/partition/gpt.c:~411` | Loop over `p->first_lba` has no 128-entry limit. Primary and backup tables are never compared. | Reported | partly fixed (claude-work): entry loop bounded by `no_of_parts` (F-056); tables still not compared |
| F-007 | `src/boot/ab_state.c` | If a state partition is missing, it still does I/O with a | Verified | open |
| | | negative handle. The B→A rollback branch is not symmetric with A→B. `validate()` | | |
| | | zeroes `crc` as a side effect. `init` sets `cfg` before validating, so a failed init | | |
| | | leaves it half set up. NULL-`cfg` part split out as F-012. Checked 2026-11-09: `bio` | | |
| | | rejects the negative handle (`-PB_ERR_PARAM`) and init then fails on the commit; the | | |
| | | B→A branch ends in the same state as A→B. Read errors: F-173. | | |
| F-008 | `src/drivers/crypto/mbedtls/mbedtls_pb.c` | Return values of the `mbedtls_sha*` calls are ignored. | Reported | open |
| F-009 | `tools/punchboot/python_wrapper.c` `log_cb` | Ignores `PyObject_CallMethod` failure and leaks the result. The GIL is never released during USB I/O. | Reported | fixed (D-021): the extension is gone; pyusb releases the GIL during transfers |
| F-010 | `tools/pbstate/CMakeLists.txt` | `BACKUP_PART` is set from `PB_STATE_PRIMARY_PART` (copy-paste). The target is listed twice in `install`. | Reported | open |
| F-012 | `src/boot/ab_state.c` + boards | If GPT init fails (blank disk), boards skip `boot_ab_state_init()` but still register its callbacks. `boot disable` dereferenced a NULL `cfg`, and a commit would have written to bio dev 0 (the raw disk). It crashed `test_part` under QEMU 10. | Verified | fixed (claude-work): guards on every entry point |
| F-013 | `src/plat/qemu/gcov.h` | GCC 14 added a 9th gcov counter type, so the struct layout was wrong and the code read a garbage `n_functions`. | Verified | fixed, then removed with `plat/qemu` (D-006) |
| F-014 | `src/plat/qemu/gcov.c` | Since GCC 12, the counter record length must be in bytes; it was written in words, so `.gcda` files were unreadable and showed 0% coverage. The merge loop also advanced past inactive counters. | Verified | fixed, then removed with `plat/qemu` (D-006) |
| F-047 | `tools/punchboot/session.py` `boot_set_boot_part` | `None`/`"none"` passed `None` to the C extension, which requires bytes, so it raised `TypeError`. The CLI worked around it with a null UUID. | Verified | fixed (claude-work) |
| F-048 | `src/drivers/partition/gpt.c` `gpt_prng` | xorshift on a signed `int`, a signed-shift overflow (UB), found by UBSan. | Verified | fixed (claude-work): `uint32_t` |
| F-049 | `gpt.c:239,269`, `cm_main.c:336,371` | `%llu` used for `uint64_t`, wrong on 64-bit (use `PRIu64`). Other host `-Wextra` warnings: unused params in `wire.c` and `image_helpers.c`, an always-true check in `rot.c:98`. | Verified | fixed (`jiffy-fixes`): `PRIu64` (the vendored aarch64 `inttypes_.h` had `"l"` for its `unsigned long long` `uint64_t`, now `"ll"`), `rot.c:98` check removed, `image_helpers.c` uses its sizes (F-088). Host, jiffy and imx8dxlevk build without warnings |
| F-050 | `tools/punchboot/api_partition.c` `pb_api_partition_write` | A bpak image is written payload-first from offset 0 and header-last at the end of the partition, with no size check. When payload + header > partition (e.g. a 512 KiB payload into a 512 KiB partition), the last payload chunk overwrote the header area: the write "succeeded" and left a corrupt image. Larger files only failed partway, with IOError. | Verified | fixed (claude-work): rejects `file size > partition size` with `ArgumentError` before writing |
| F-015 | `src/board/test/board.c:~245` | `if (readable_part)` should be `>= 0`; a negative error code is truthy. Harmless because the bio functions check the handle. | Verified | fixed in `board/host`; `board/test` removed |
| F-011 | `include/bpak/bpak.h` (vendored) | `bpak_add_transport_meta` uses the wrong size. Fixed upstream in bpak v0.9.2. Latent: punchboot never calls it. | Reported | fixed 2026-11-08: bpak 1.0.1 vendored (D-024), `test_add_transport_meta` checks the size |
| F-053 | `src/cm/cm_main.c:348,387` `cmd_stream_read/write` | `buffer_id` (0-255) and `size` (u32) are not bounded; only `stream_prep` checks them. READ_BUFFER writes disk data past the 2×4 KiB CM buffer (memory corruption), WRITE_BUFFER copies arbitrary RAM into a partition (disclosure). No auth needed unless CONFIGURATION_LOCKED. | Verified (code) | fixed (claude-work): `stream_check()` bounds buffer id, size and offset; `test_stream.py` |
| F-054 | `src/bio.c:179` `check_lba_range` | `first_lba + lba` is computed in 32-bit `lba_t` and wraps, so a large LBA passes the check and reaches any absolute block. Stream commands truncate the u64 byte offset to `lba_t`, so the host can read/write the whole disk through any readable/writable partition. | Verified (code) | fixed (claude-work): 64-bit range check, `bio_write` returns `-PB_ERR_PARAM` like `bio_read`; `test_bio.c`, `test_stream.py` |
| F-055 | `src/bio.c:218` `bio_erase`, `cm_main.c:502` | No range check, `first_lba` is not added (partition-relative LBA passed as absolute), no `BIO_FLAG_WRITABLE` check. On imxrt, erasing any partition at LBA 0 erases the start of NOR. The host disk has no erase op, so it's untested. | Verified (code) | fixed (`6a1a0e23`): `bio_erase()` is relative to the partition with a range check, `cmd_part_erase()` converts the absolute LBA that hosts send (the 2.x transcript); a count of 0 stays as before (F-075) |
| F-056 | `src/drivers/partition/gpt.c:111,250,355,386` | `no_of_parts` from disk is never bounded to 128 (extends F-006). The array CRC reads past `primary`/`backup`, and recovery `memcpy`s write past them. `hdr_sz`/`part_entry_sz` are not checked. Memory corruption from a crafted disk, before any image is verified. | Verified (code) | fixed (claude-work): header rejected unless ≤ 128 entries of 128 bytes; `test_gpt.c` |
| F-057 | `src/drivers/crypto/caam/imx_caam.c:209,220` `caam_der_ecsig_to_rs` | Always copies `sz` bytes (from the signature) into `r`/`s`, so r/s longer than `rs_length` overflow; `sig_length` and the SEQUENCE length are ignored. Runs before verification on every CAAM board (jiffy, imx8x, imx8m). | Verified (code) | fixed (`jiffy-fixes`): `der_ecsig_to_rs` in `src/lib` reads nothing past `sig_length`, drops leading zeros, pads to the curve size; the CAAM uses it (`test_der.c`) |
| F-058 | `src/boot/image_helpers.c:113` | Passes `sizeof(header_digest)` (64) as the digest length. mbedtls truncates to the curve size, so sha256+P-384/P-521 and sha384+P-521 hash trailing garbage and valid images are rejected (fails closed). | Verified (code) | fixed (`jiffy-fixes`): the digest length is passed; `test_boot.py` boots sha256/P-384, sha384/P-521, sha512/P-256. The CAAM now gets 32 instead of 64 bytes for sha256/P-256. Checked on jiffy: a sha256/P-256 header verifies (`boot bpak`) |
| F-059 | `src/boot/linux.c:69-71` | Ramdisk load-addr meta is dereferenced before `rc` is checked; `mh` is NULL on failure. NULL deref for an image without a ramdisk part on boards with `ramdisk_bpak_id`. | Verified (code) | fixed (`jiffy-fixes`): checked first, still `BAD_META` (`test_linux.c`) |
| F-060 | `src/boot/boot.c:311` `boot_load` | Success falls through to `err_out`, which clears `boot_flags`, so `late_boot` never sees `BOOT_FLAG_CMD` (imxrt `board_late_boot` depends on it). | Verified (code) | fixed (`jiffy-fixes`): the flags are cleared when `boot_load` or `boot_jump` fails, and kept for `late_boot` otherwise |
| F-061 | `src/boot/image_helpers.c:280` + `cm_main.c:558` | `rc = result_f(rc)` replaces a read/hash error with `cm_write`'s result, so a failed part reads as OK; only the final payload hash catches it. | Verified (code) | fixed (`jiffy-fixes`): the first hash error is kept; with a streaming host the rest is still read, with a result per part and a final one (D-008). The CB boot also sends the final result when the load fails |
| F-062 | `src/boot/image_helpers.c:258` | Payload LBA is computed with a hard-coded `/512`, while the header read uses `bio_block_size()`. Wrong data on any non-512 block device. Latent. | Verified (code) | fixed (`jiffy-fixes`): `read_f` takes a byte offset, `boot_bio_read` converts it with the block size (`-PB_ERR_ALIGN` if unaligned) |
| F-063 | `cm_main.c:207,221` `cmd_board` + `board/host/board.c:435` | `request_size` from the host is passed to the board unbounded (cm always reads 1024). The host board does `arg[size] = 0`, an out-of-bounds write. Same pattern as F-005. | Verified: `request_size` 0xffffffff crashes pb-host (SEGV under ASan) | fixed (e405444): over 1024 refused with INVALID_ARGUMENT after the request transfer (2.x hosts send it without checking the first result), the board isn't called; `test_cm.c`, `test_board.py` |
| F-064 | `src/drivers/partition/gpt.c:152` | Header `last_lba` (last usable) is `last - 32`, which is the first backup-entry block; should be `last - 33`. pb's own allocation is unaffected; other tools could overlap the backup array. | Verified (code) | open |
| F-065 | `src/boot/*`, `utils_def.h:150` | Hardening, signed input only: `CHECK_OVERLAP` can wrap; no check that load regions are in RAM or that parts don't overlap each other; load-addr meta read as u64 without a size check; `linux.c` doesn't check `fdt_totalsize <= dtb part`; `keystore_id` never compared; no anti-rollback counter. | Reported | open; the `fdt_totalsize` <= part check is done (`39276b9c`) |
| F-066 | `src/lib/der_helpers.c` | `der_ecsig_to_rs` has no callers and doesn't left-pad short r/s. `der_ec_public_key_data` memcmp with a data-supplied length (0 matches anything, and an OID longer than the reference reads past it); `s -= 2` can underflow. | Reported | partly fixed (`jiffy-fixes`): `der_ecsig_to_rs` now pads short r/s and is used by the CAAM (F-057). `der_ec_public_key_data` is still open |
| F-068 | `src/lib/bpak.c` `bpak_del_meta` (vendored, same upstream) | When the last meta slot is in use, the slot isn't cleared and the final `memset` writes the 16 bytes after `meta[]`, i.e. `parts[0]`. punchboot never deletes meta. | Verified (code) | fixed upstream (bpak 1.0.0), vendored 2026-11-08 (D-024), `test_del_meta_all_slots` |
| F-069 | `src/crypto.c:33`, `src/cm/cm_main.c:538` | Positive error codes: `hash_init` returned `PB_ERR_NOT_SUPPORTED` when no provider matched; `cmd_part_verify` sets `rc = PB_ERR_CHECKSUM` (the host still gets PART_VERIFY_FAILED). | Verified (test) | fixed (claude-work) |
| F-070 | `src/boot/boot.c` | BIO and CB boots load into `header`, but `prepare`/`late_boot` get `header_ptr`, which an IN_MEM or CUSTOM boot repoints. After a failed in-memory boot, a command-mode boot would prepare (jump address, dtb) from the stale, unverified header. Latent: no board uses IN_MEM/CUSTOM. | Verified (code) | open |
| F-071 | `src/boot/linux.c:173` | `fdt_delprop("pb,slc-available-keys")` returns `-FDT_ERR_NOTFOUND` if `/chosen` doesn't have the property, which fails the boot. A DTB without `/chosen` fails too, and new properties only fit if the DTB was built with free space. The jiffy, imx8dxlevk and imx8qxmek DTBs have the properties (Jonas), so no board is affected today. | Verified (code, libfdt) | fixed: `1e8f5601` (missing key list), `39276b9c` (`boot_dtb_grow()` grows the DTB by `CONFIG_BOOT_DTB_HEADROOM`, `/chosen` added). jiffy boots a DTS without the placeholders (the test bench), the 8MP EVK's upstream DTB is next (F-150) |
| F-072 | `src/cm/cm_main.c` `cmd_slc_read` | Returns the status of the last key in the key map, which becomes the final result. If the last key is revoked the host gets KEY_REVOKED, and `slc_get_*` raise `KeyRevokedError`. | Verified (test: `test_commands.py`, xfail) | fixed (`jiffy-fixes`): the result is OK, or the first error reading a key status; the xfail is gone |
| F-073 | `src/bio.c:256` `bio_set_hal_flags` | `flags << 24` on a promoted `uint8_t`: signed overflow (UB) when bit 7 is set. Found by UBSan in `test_bio.c`. | Verified (test) | fixed (claude-work) |
| F-074 | `src/cm/cm_main.c` stream read/write | After a failed STREAM_INITIALIZE, `block_dev` is an error code, and `bio_get_flags()` of it has the flag bits set, so the readable/writable checks pass. `stream_check()` then fails on the block size, so it's harmless. | Verified (test) | open |
| F-075 | `cm_main.c` `cmd_part_erase`, `struct pb_command_erase_part` | Wire compat (D-008): `start_lba`/`block_count` were taken from `rz` in 2309738 (v2.0.0). A pre-2.0 host sends zeros, so a current device erases 0 blocks and answers OK: a silent no-op instead of a full erase. Fix: `block_count == 0` means the whole partition. Coordinate with F-055. | Verified (code) | open |
| F-076 | `src/boot/image_helpers.c` (payload hash and load) | bpak compat (D-008): punchboot hashes every part, while upstream `verify.c` skips parts with `BPAK_FLAG_EXCLUDE_FROM_HASH` (`bpak add --set-flag dont-hash`). Such images verify with the bpak CLI but fail to boot in punchboot. Supporting the flag only accepts more images, so it is a compatible fix. | Verified (code) | open |
| F-077 | `tools/punchboot/api_partition.c`, `api_boot.c` (now `session.py`) | Wire compat, new host + old device: the host uses `caps.chunk_transfer_max_bytes` without a zero check. A device that predates the field (before 0.7, 2020) reports 0: partition write sends the bpak header and no payload and reports success, and RAM boot loops forever. Fall back to `stream_buffer_size`. Low: such devices are probably gone. | Verified (code) | fixed: falls back to `stream_buffer_size` and one buffer, `NotSupportedError` if both sizes are 0; `test_host_caps.py` |
| F-078 | jiffy (hardware / boot ROM) | An SRST reset from the JTAG adapter (OpenOCD `reset`, SRSR = 1, POR) makes the boot ROM fall back to the serial downloader (`15a2:007d`) within a second, instead of booting pb from eMMC. A second clean SRST pulse did the same. A watchdog reset does too (SRSR = 0x11, after pb without WDBG was halted over JTAG, and after the `auth token` hang below). Earlier resets when the ULINK was plugged in did reboot into pb, and a power cycle does. Cause unknown. Suspect: pb leaves the eMMC in 8-bit DDR52, and a warm reset doesn't reset the eMMC. Other options: HAB, or the reset itself. `src/board/jiffy/openocd.cfg` has no `reset_config` because of this. | Seen on the board (2026-10-22) | open: a power cycle boots pb. Forcing serial download with the boot mode buttons and resetting works (uuu loads `pb_signed_uuu.imx`), but in the downloader the ROM falls back to after a failed boot, every SDP command with a data phase (write, dcd) hangs. Hardware (jiffy R1 netlist): JTAG SRST is the `RST_N` net (POR_B, LP3470 supervisor, button PB2). The eMMC RST_n is a separate net from pad NAND_WP_B with a 46.4k pull-up, so a POR_B reset doesn't reset the eMMC (see F-079) | Likely cause: the same SNVS SV1/Non-Secure state as F-080 (every warm reset after JTAG activity landed in the downloader, the one without JTAG since power-on booted; a direct test is blocked because `dev reset` needs `auth`). SNVS survives a warm reset, so HAB fails on the next boot and the ROM falls back to the downloader, where data writes then hang. A power cycle clears it. Fits every reset seen so far except one SRST at 12:26.
| F-079 | `src/board/jiffy/board.c` `usdhc_emmc_setup` | Muxes NAND_DQS (ball E6) as the eMMC reset, but on jiffy R1 NAND_DQS isn't connected. The eMMC RST_n is on NAND_WP_B (ball D5, `eMMC_RST_N`, 46.4k pull-up), which pb never configures, so the eMMC never gets a hardware reset. Possibly related to F-078. | Verified (netlist) | open: check NAND_WP_B's USDHC1_RESET_B alt in the RM, and whether the eMMC has RST_n_FUNCTION enabled |
| F-080 | `src/drivers/crypto/caam/imx_caam.c:158` `caam_wait_for_job` | On the closed jiffy, any JTAG connection (even OpenOCD `init` alone, no halt) sets SNVS SV1 (HPSVSR 0x80000002) and the SSM drops from Trusted to Non-Secure (HPSR 0x8000bb00). After that the CAAM, left in trusted mode by HAB (CSTA MOO 3), never fetches new jobs (IRSAR 0, ORSFR 0). pb spins in `caam_wait_for_job` (no timeout, no watchdog kick) on the next hash, e.g. in `auth_token`, until the watchdog resets the board. The host sees `Error: Transfer`. Confirmed: power cycle, `auth` works, OpenOCD `init`, `auth` hangs. pb started over JTAG (not HAB booted, CSTA MOO 0) doesn't hang. The hardware behaviour is expected on a closed part. The bug is that pb hangs instead of failing. | Confirmed on the board (2026-10-22) | fixed (`jtag-debugging`): `caam_wait_for_job` times out after 1 s with `-PB_ERR_TIMEOUT`, and `caam_hash_final` checks the INIT job. Checked on the board: after a JTAG attach `auth token` fails with `Error: Timeout` and pb keeps running. The CAAM stays unusable until a power cycle (expected) |
| F-081 | `src/arch/armv7a/cp15.S` `arch_invalidate_cache_range`, callers in `imx_caam.c` | The invalidate rounds the start down to a cache line and uses DCIMVAC (invalidate, no clean), so dirty CPU data sharing the first or last line is lost. `caam_wait_for_job` invalidates `caam.output` (0x8001a20c in the jiffy debug build), whose line also holds the tail of `struct caam` (`init`, `async_hashing`, `hash_align_buf_len`...), `wpb`, another `base`, `mmc_current_part` and `mmc_current_part_config`. Not shown to cause F-080. | Verified (code, symbol map) | open: clean+invalidate (DCCIMVAC) partial lines, or keep DMA buffers in their own cache-line-aligned sections |
| F-082 | `src/drivers/mmc/mmc_core.c` `mmc_part_switch` | `mmc_part_switch` wrote the whole PARTITION_CONFIG: accessing BOOT0 enabled BOOT1 for boot and the other way round, and the user area and RPMB enabled BOOT0. After `part write` to BOOT0, a `dev reset` boots BOOT1, i.e. the other (older) bootloader. Seen twice on jiffy (2026-10-22). Worse: after a power loss in the middle of a BOOT0 write, the ROM boots BOOT1 once, pb reads the GPT and enables the half-written BOOT0, and the next boot ends in the serial downloader (the ROM has no fallback, RM 8.5.3.2). A read of BOOT0 also enabled BOOT1, and pb undid a boot partition selected from Linux. | Seen on the board | fixed (`jiffy-fixes`): `mmc_part_switch` only changes PARTITION_ACCESS; a write to the enabled boot partition first enables the other one, and a STREAM_FINALIZE after error-free writes enables the written one (`bio_write_done()`). An interrupted write boots the other partition until a write completes (`test_mmc_core.c`). Holds once both partitions run the fix: from an older pb, write BOOT1 first. Checked on jiffy: half a BOOT0 write without STREAM_FINALIZE, then two power cycles both boot BOOT1, and the next full BOOT0 write boots BOOT0 |
| F-083 | `punchboot part read` | Fails with `Error: I/O error` on jiffy, both for BOOT1 and the 512 B PB State Primary partition, in an authenticated session. Cause unknown. | Seen on the board (2026-10-22) | open |
| F-084 | `src/drivers/timer/imx_gpt.c` `imx_gpt_init`, `src/plat/imx6ul/plat.c` | `plat_get_us_tick()` runs 2.64x fast on jiffy, so every `pb_delay_*`/`pb_timeout_*` is 2.64x short (the 1 s CAAM timeout is ~0.38 s). The driver selects CLKSRC 010 (`ipg_clk_highfreq` = PERCLK_CLK_ROOT, GPR5 bit 28 = 0) and writes `PR = input_clock_Hz / MHz(1)`, but the counter divides by PR + 1 (RM §28.6.2), and perclk isn't 24 MHz: pb never sets `CSCMR1[PERCLK_CLK_SEL]`, and the ROM leaves it on IPG = PLL2 PFD2 396 MHz / AHB 3 / IPG 2 = 66 MHz. 66 / 25 = 2.64 MHz. The same driver is used by imx8x, imx8m and imxrt (their perclk not checked; the PR off-by-one applies to all). | Verified (JTAG, 2026-10-23): CSCMR1 `0x04900000`, CBCDR `0x00018900`, CBCMR `0x24a69324`, PFD_528 `0x5058505b`, GPR5 0, GPT1 CR `0x681`, PR `0x18`; GPT1_CNT advanced 26518521 in 10.044 s of host time (2.6401 ticks/us) | fixed (fc337d5 PR + 1, all platforms; 002a08d imx6ul runs from the 24 MHz crystal via `imx_gpt_init_24m()`; e93a767 imxrt passes perclk). Checked on jiffy after the fix (flashed, 2026-10-23): GPT1 CR `0x743`, PR `0x7002`, 10046344 ticks in 10.046 s of host time (1.0001 ticks/us). imx8x and imx8m keep perclk and now get 1.00 MHz instead of 0.96 MHz if their perclk is 24 MHz (not checked) |
| F-085 | `src/cm/cm_main.c` `PB_CMD_BOOT_PART` | Since ac58e89 (2024-09) the handler calls `tops.disconnect()` before checking `boot_load`'s result. When the load fails, `cm_run` logs `Command error` and waits for the next command on a stopped USB controller, so the device drops off the bus until a power cycle. Seen on jiffy: `boot partition` of an image that fails to verify; over JTAG pb loops in `cm_run`/`imx_ci_udc_xfer_complete` with USBCMD 0x00080000 (RS = 0). | Confirmed on the board (2026-10-22) | fixed (`jiffy-fixes`): disconnect only before `boot_jump`; `test_boot_part` sends a command after a failed boot. Checked on jiffy: the device stays on USB and `boot status` works after the failed boot |
| F-086 | `src/drivers/crypto/caam/imx_caam.c` `imx_caam_init` | The CAAM registers SHA-384/512 whatever the MDHA is. The i.MX6UL has the LP256 MDHA (CHAVID_LS 0x10040103, MD VID 0), which only does up to SHA-256. A SHA-512 bpak (sys_a on jiffy) fails the header hash with JR status 0x20000541 (CCB, MDHA, mode error, descriptor index 5, the ST of the INIT job; descriptor read over JTAG) instead of `NOT_SUPPORTED`. jiffy has no software hash, so such images can't boot there. | Confirmed on the board (2026-10-22) | fixed (`jiffy-fixes`): SHA-384/512 dropped when CCBVID era is 1..9 and the MD VID is LP256, other CAAMs unchanged. Checked on jiffy: the SHA-512 sys_a image fails with NOT_SUPPORTED (-19), no CAAM job error |
| F-087 | `src/cm/cm_main.c` `error_to_wire` | `-PB_ERR_BAD_PAYLOAD` (payload hash mismatch) has no wire result, so the host reports "Generic error". `include/pb-tools/error.h` has no payload code. | Seen on jiffy (2026-10-22) | open: a new result code is a wire change, see `compatibility.md` (old hosts must handle an unknown code) |
| F-088 | `src/boot/image_helpers.c` `boot_image_copy_and_hash` | Ignores `source_size` and `destination_size`, so a part past the end of either buffer is copied anyway, out of bounds. Latent: only the unit tests call it. Found through the `-Wunused-parameter` warning (F-049). | Verified (code) | fixed (`jiffy-fixes`): a part that doesn't fit returns `-PB_ERR_BUF_TOO_SMALL` before it's copied (`test_image_helpers.c`) |
| F-089 | `src/lib/libc/snprintf.c` (vendored) `unsigned_num_print` | `%x` writes the digits and padding without checking the buffer size `n`, so `snprintf` with `%x` can overflow a small buffer (`%d`/`%u`/`%s` are bounded). Unsupported specifiers (`%zu`, `%2u`, `%c`, ...) spin forever instead of failing. | Verified (code) | open: `imx_rom_log.c` works around it (only writes a line with 128 bytes left) |
| F-090 | `src/plat/imx8x` link, imx8dxlevk | `.text` grew past 64 KiB (0x11000 after `plat_get_boot_source`), so `.data` no longer starts on a 64 KiB boundary (ld's aarch64 max page size), and ld puts `.text` through `.bss` in one RWX LOAD segment: `warning: build-imx8dxlevk/pb has a LOAD segment with RWX permissions`. imx8mpevk (2026-10-28) too: `.text` starts at 0x920000 and ends inside the same 64 KiB page as `.data`. Only the ELF headers; `pb.bin` and the MMU setup (linker symbols) are the same. | Verified (readelf) | open: `-z max-page-size=0x1000` for the aarch64 link, or align `.data` in the linker script |
| F-094 | `tools/punchboot/api_authentication.c` `internal_pb_api_authenticate` | The host copied the auth data into a 1024 byte stack buffer without a size check, and the Python wrapper passes any length: a token over 1024 bytes overflowed the host's stack (64 KiB: SIGSEGV), and the size went on the wire as a truncated uint16. A size of 0 sent no data and reported the device's first OK as success, while the device waited for the data. Same pattern as F-005, host side. | Verified (test, fake device) | fixed (bb1a1f6): size 0 or over 1024 is INVALID_ARGUMENT before anything is sent; `test_host_auth.py` |
| F-095 | `src/cm/cm_main.c` `cmd_board`, `tools/punchboot/api_board.c` | The device ignores `response_buffer_size` and lets the board fill the whole command buffer. When the response is larger than the host's buffer, the host returns an error without reading the response, which stays on the link and is read as the next results. 2.x hosts also don't check the first result (the check is commented out), so they send the request transfer even after an error. | Verified (code) | open: pass `min(response_buffer_size, CM_BUF_SIZE)` to the board, but check first whether any released host sends 0 there (D-008) |
| F-096 | `src/plat/imx6ul/plat.c`, `src/board/jiffy/imximage.cfg` | The TZASC (TZC-380) was bypassed on the i.MX6UL: TZASC1_BYP in IOMUXC_GPR9 is cleared by a power-on reset and nothing set it, so no DDR access was checked. OP-TEE's TZASC regions had no effect: on jiffy Linux read OP-TEE's memory at 0xbe000000 with devmem. Out of bypass, OP-TEE takes a data abort reading the DTB through its non-secure mapping before its TZASC driver has run, because region 0 only lets secure accesses through. | Seen on the board (2026-10-26) | fixed (`imx6ul-tzasc`): new TZC-380 driver (`src/drivers/tzc`, selected by `PLAT_IMX6UL`); `plat_init()` opens the DDR window to both worlds in region 1, which OP-TEE replaces, and logs an error when the DCD left the TZASC bypassed (not fatal, command mode needs `plat_board_init()`). The jiffy DCD sets the bit after the MMDC setup. A warm reset keeps the bit, so the check only catches a missing DCD line after a power-on reset. Needs OP-TEE with `CFG_TZC380`, or DDR stays open in region 1 |
| F-098 | `src/plat/imx8m/umctl2_lp4.c` `umctl2_init` (now `ddr.c`, `drivers/ddr/dwc_ddrphy.c`) | The LPDDR4 init ignored the firmware readback check (which also returned a positive `PB_ERR`, F-069) and the PHY firmware's "training failed" mail, carried on with untrained DRAM and returned `PB_OK`; `plat_init()` ignored the result anyway. | Verified (host model, `test_imx8m_ddr`) | fixed (`imx8m-dram-cleanup`): `-PB_ERR_IO`, `plat_init()` fails |
| F-099 | `include/plat/imx8m/clock.h` | `FRAC_PLL_LOCK_MASK` and `SSCG_PLL_LOCK_MASK` were `(1 << 31)`, signed overflow (UB). Found by UBSan in `test_imx8m_ddr`. | Verified | fixed (`imx8m-dram-cleanup`) |
| F-100 | `src/plat/imx8m/ddr.c` `dram_save_timing` | The timing copy for TF-A in `.umctl2` (NOLOAD, 0x40000000) never writes `fsp_msg`/`fsp_msg_num`, so TF-A gets whatever DRAM held. The copy's size (~12.5 KiB for the EVK) is not checked against the 128 KiB region, and nothing pins `struct dram_timing_info` to TF-A's layout. | Verified (read) | partly fixed (`imx8m-dram-cleanup`): the copy is a 128 KiB `struct dram_timing_save` and too large tables fail with `-PB_ERR_MEM`; `fsp_msg` is never read by upstream TF-A (only `fsp_table` and the tables), so harmless there; the layout pin is open |
| F-101 | `src/board/imx8mevk/lpddr4_timing.c` `umctl2_ddrc_cfg` | The last four entries write `FREQ1_DERATEEN/DERATEINT/RFSHCTL0/RFSHTMG` again with other values, overwriting the 667 MT/s settings a few lines up, while the 3200 MT/s `DERATEEN`, `DERATEINT` and `RFSHCTL0` are never written (reset values). Looks like FREQ0 values pasted onto FREQ1 names. Also `DBG1`, `SWCTL`, `PCTRL_0` in the table are written again by the code. | Verified (read); hardware effect unknown | fixed (`imx8m-dram-cleanup`, `c129bbf9`): the EVK timing is now NXP DDR tool output, which sets the FREQ0 registers and writes FREQ1 once |
| F-102 | `src/drivers/ddr/{umctl2,dwc_ddrphy}.c`, `src/plat/imx8m/ddr_imx8mq.c` | Every poll in the DDR init (STAT, PHY mailbox, CalBusy, DFISTAT, SWSTAT, PLL lock) spins forever. A stuck PHY or bad board table hangs the board without a log. | Verified (read) | open: timeouts need a measurement on hardware |
| F-104 | `src/drivers/ddr/dwc_ddrphy.c` `dwc_ddrphy_train` | The training results in the message block are never read. U-Boot (all i.MX8M) reads the trained critical delay differences after each 1D run and widens the rank-to-rank gaps (`DRAMTMG2` rd2wr/wr2rd, `RANKCTL`, per P-state) before the DFI init. The imx8mevk has two ranks (`MSTR.active_ranks` 3). TF-A reads these registers back at boot and restores them after retention, so it expects the bootloader to have applied the workaround. | Verified (read, and in the differential trace: U-Boot rewrites `DRAMTMG2`, `FREQ1_DRAMTMG2` and `RANKCTL` before `dfi_init_start`) | open: needs the message block layout and an imx8mevk |
| F-105 | `src/plat/imx8m/ddr.c`, `ddr_imx8mq.c` | The DDR init never sets the CCM `DRAM_APB_CLK_ROOT` and `DRAM_SEL_CFG`. NXP's current code (U-Boot master) sets the DRAM APB clock to sys PLL1 / 4 before configuring the controller, and after every DRAM PLL change selects the PLL as DRAM clock (`DRAM_SEL_CFG` source 0) and the APB clock to / 5. Punchboot relies on the reset/ROM state. | Verified (differential trace against U-Boot, 2026-10-28) | fixed (`imx8m-dram-cleanup`, `410f767c`): `ddr_imx8mq.c` sets DRAM_APB_CLK_ROOT and DRAM_SEL_CFG as NXP's DDR tool does, and DRAM_ALT_CLK_ROOT for PHY bypass |
| F-106 | `src/drivers/ddr/umctl2.c` | Low power/refresh values differ from NXP's current sequence: during init `RFSHCTL3` 0x11 and `PWRCTL` 0xa8 (U-Boot: 0x1 and 0xa0, its comment says `en_dfi_dram_clk_disable = 0`; `refresh_mode` 1 is a DDR4 fine granularity setting); at the end `PWRCTL` 0x08 and `RFSHCTL3` 0x10 (U-Boot: clears only `selfref_sw`, 0x80, then `RFSHCTL3` 0 and `selfref_en`, 0x81). Punchboot also writes `DFIPHYMSTR` twice, which the board table already sets. | Verified (differential trace) | open: needs an imx8mevk |
| F-107 | `src/plat/imx8m/ddr.c` `dram_save_timing` | The trained PHY CSRs for TF-A are read after the DFI init and port enable; NXP reads them right after loading the PIE image, before the calibration wait and the DFI init. | Verified (differential trace) | open: needs an imx8mevk |
| F-110 | `src/plat/imx8m/ddr.c` `imx8m_ddr_init` (GPC) | The DDR power-up writes `GPC_PGC_CPU_0_1_MAPPING` (0x303A00EC) and `GPC_PU_PGC_SW_PUP_REQ` bit 5 (0x303A00F8), the i.MX8MQ/8MM layout, and never waits for the request to self-clear. On the i.MX8M Plus 0x303A00EC is the read-only `GPC_LPCR_CM4_AD` and 0x303A00F8 is not in the map; its PU power-up trigger is 0x303A00D8 with `DDRMIX_SW_PUP_REQ` at bit 19 (RM 5.2.10.37). NXP's 8MP DDR tool script writes the same two addresses, and the RM's own example code uses them. Harmless on the 8MP EVK: DDRMIX is powered after reset and the DRAM trains (2026-10-28). | Verified (RM, board) | open: per-SoC power-up in `ddr_imx8m{q,p}.c`, with the self-clear wait |
| F-112 | `src/drivers/usb/dwc3_udc.c` | The DWC3 device driver had never run (imx8mevk isn't in the lab) and didn't follow the RM's device programming model: every EP0 OUT TRB was Control-Setup (also the status stage), data stages were Normal TRBs, status stages started without waiting for XferNotReady(Status), a USB reset left the control transfer hanging (no timeouts: the watchdog reset the 8MP EVK during enumeration), EP0/1 used FIFO 1 and 64 bytes before connect done, connect done didn't reconfigure EP0, DCFG.DEVSPD was never set, End Transfer had no transfer resource index (never saved) nor ForceRM/CmdIOC, the FIFO sizes were overwritten with magic values, IN endpoints used the physical number as TX FIFO, field macros didn't parenthesize their argument, TRBs shared cache lines. | Verified (RM 11.2.2.5, board) | fixed (`imx8m-dram-cleanup`): rewritten from RM 11.2.2.5/11.2.2.7, named fields in `dwc3_udc_private.h`. On the 8MP EVK: enumeration, `dev show`, `part list`, 4 MiB writes with device-side verify. Not exercised yet: a USB reset during a bulk transfer, EP0 stall, full speed hosts |
| F-113 | `src/drivers/mmc/imx_usdhc.c`, `src/board/imx8mpevk/board.c` (i.MX8M Plus uSDHC3) | eMMC in DDR52 (8-bit DDR) fails at the first data read after the switch (CMD8, INT status 0x208003: DCE, a data CRC error); HS200 works. 2026-10-28, after the DDR_EN fix (F-123): with the board's fixed DLL override tap 35 the CMD8 still fails the same way; with tap 0 init gets through DDR52 (EXT_CSD reads work), but the first CMD18 (GPT read) fails: the auto CMD12 response in CMD_RSP3 is 0x00d00f00 (CC_ERROR, COM_CRC_ERROR, ILLEGAL_COMMAND, state prg), which doesn't fit a read. HS200 with tap 0 fails the same way (2026-10-29) and works with tap 35: a sampling error, the DLL override is in the read path in SDR too. The right tap depends on board and silicon (tuning). | Verified (board) | open: imx8mpevk uses HS200. Next: what CMD_RSP3 holds after auto CMD12 in DDR, and a DLL setting for DDR instead of the fixed override |
| F-114 | `src/plat/imx8m/plat.c` | The OCOTP clock gate (CCGR34) was never opened: on the i.MX8M Plus EVK the unique ID read as 0, so the device UUID was all zeros and the USB serial garbage. | Verified (board) | fixed (`imx8m-dram-cleanup`): opened before `imx_ocotp_init()` (reading only) |
| F-115 | `src/board/imx8mpevk/board.c` `cm_board_init` | USB1 relies on the ROM having powered HSIOMIX and the USB1 PHY (GPC_PU_PGC_SW_PUP_REQ 0x303A00D8 bits 17 and 2) and set the PHY reference clock, which it does after a serial download boot. Booting from eMMC it doesn't. | Verified (RM), seen on the board (2026-10-29) | fixed: `usb1_power_up()`: the HSIO and USB clocks and reference clock roots (CCM slices 98/99), GPC A53 mapping and power-up of HSIOMIX and the USB1 PHY, the NoC handshake (acked only once HSIO BLK_CTRL USB_CLOCK_MODULE_EN is set), PHY_CTRL0 FSEL 24 MHz, PHY_CTRL6 bits 1:0 cleared (reset 0b11, the alternate PHY clock; without it the first DWC3 endpoint command times out). The EVK boots pb from eMMC with USB command mode |
| F-116 | `src/drivers/mmc/mmc_core.c` `mmc_setup` (HS400) | HS400 without Enhanced Strobe selects HS_TIMING 3 without tuning in HS200 first (JESD84-B51 6.6.2.3 steps 5-6). With `MMC_BUS_WIDTH_8BIT_DDR_STROBE` the card is switched to BUS_WIDTH 0x86, then `imx_usdhc_set_bus_width()` rejects the width. No board uses HS400. | Verified (bench, `test_mmc_emmc` `test_hs400`) | open |
| F-117 | `src/drivers/mmc/mmc_core.c` `mmc_setup` | The bus mode is switched without reading DEVICE_TYPE (or the VCCQ the mode needs) first, JESD84-B51 6.6.2.2: on a device without HS200, or at 3.3 V, the CMD6 fails with SWITCH_ERROR. EXT_CSD is first read after the switch. | Verified (bench, `test_hs200_not_supported`, `test_hs200_at_3v3`) | fixed (`imx8m-dram-cleanup`): `fa4c3d49`, DEVICE_TYPE read before the mode switch. HS200 at 3.3 V is a board configuration error, the core can't know VCCQ |
| F-118 | `src/drivers/mmc/mmc_core.c` `select_part`, `mmc_part_switch` | A failed partition switch is ignored: `select_part()` returns void and `mmc_current_part` is set before the CMD6, so the read or write goes to the partition still selected (e.g. user LBA 0, the GPT). | Verified (bench, `test_part_switch_error`) | fixed (`imx8m-dram-cleanup`): `5c912810` |
| F-119 | `src/drivers/mmc/mmc_core.c` | Card status error bits other than SWITCH_ERROR are never checked (Table 68): the R1 of CMD17/18/23/24/25 is discarded, and `mmc_device_state()` only looks at SWITCH_ERROR, so ADDRESS_OUT_OF_RANGE, WP_VIOLATION, ERROR or DEVICE_ECC_FAILED reach the host as success. | Verified (bench, `test_read_status_error`) | fixed (`imx8m-dram-cleanup`): `74f3e3b6`; COM_CRC_ERROR/ILLEGAL_COMMAND left out of the mask, they belong to the previous command (`3825305e`) |
| F-120 | `src/drivers/mmc/mmc_core.c` `mmc_power_off` | After POWER_OFF_LONG the core polls CMD13, and a command after a power off notification restores POWERED_ON (JESD84-B51 6.6.36): power is then cut without a notification. The busy end must be waited for on DAT0 (R1b). | Verified (bench, `test_power_off`) | fixed (`imx8m-dram-cleanup`): `ec4a3714` |
| F-121 | `src/drivers/mmc/mmc_core.c` `mmc_setup`, `mmc_device_state` | GENERIC_CMD6_TIME, PARTITION_SWITCH_TIME or POWER_OFF_LONG_TIME of 0 (not defined, 7.4.31/56) makes `mmc_device_state(0)` fail every CMD6 at once, so init stops at the BOOT_BUS_CONDITIONS update. The timeouts are also kept from a previous `mmc_init()`. The timeout counts loop iterations, not ms. | Verified (bench, `test_cmd6_times_not_defined`) | fixed (`imx8m-dram-cleanup`): `b8336127` |
| F-122 | `src/drivers/mmc/imx_usdhc.c` `imx_usdhc_send_cmd` | R1b goes out as RSPTYP 10 (48 bit, no busy): the busy branch can't be reached, so the driver never waits for TC after CMD6/CMD12 and the core's CMD13 runs during busy. Index check (CICEN) only for busy responses, so R1 responses have no index check (RM Table 10-47). The data and direction of a command are guessed from its index, which can't work for SD (CMD6, CMD8, ACMD13/51). | Verified (RM, bench) | fixed (`imx8m-dram-cleanup`): `f60f503a` (response types); data from the request, not the index (`05e92cc2`); busy end from DAT0, F-126 (`3af5b056`) |
| F-123 | `src/drivers/mmc/imx_usdhc.c` (DDR) | `set_bus_clock()` computes the divider for DDR (doubled), but MIX_CTRL.DDR_EN is set only on data commands and cleared on all others, so every command without data runs at twice the clock: 99 MHz in HS (limit 52) on jiffy's DDR52. Each data command also changes the clock without waiting for SDSTB. `bus_ddr_enable` isn't reset by `imx_usdhc_setup()`. | Verified (RM, bench `test_jiffy_emmc`) | fixed (`imx8m-dram-cleanup`): `7692b04d` |
| F-124 | `src/drivers/mmc/imx_usdhc.c` (CMD12) | After a data error there is no CMD12 (the RM: no auto CMD12 after an error in a non-last block), so the card stays in data/rcv and the next command is illegal. Auto CMD12 is also sent after a CMD23-counted transfer (RPMB), which is illegal (JESD84-B51 6.6.7.1). | Verified (bench, `test_rpmb_read`, `test_read_crc_error`) | fixed (`imx8m-dram-cleanup`): `6033ac72` |
| F-125 | `src/drivers/mmc/imx_usdhc.c`, `imx_usdhc_private.h` (programming) | Against the uSDHC RM (6UL, 8M Plus): MIX_CTRL written with bit 31 = 0 and PROT_CTRL with bits 23:21 = 0 (both "always write as"); VEND_SPEC bits 14:11 toggled, reserved on the i.MX8M; INITA right after RSTA at about 780 kHz, before the 400 kHz clock; no SDSTB check before a divider change; no timeouts on any poll; DMA and auto CMD12 errors not enabled in INT_STATUS_EN, so an ADMA error hangs the TC wait; a fixed 1 ms delay before every command; magic constants (PROT_CTRL DMASEL and bit 27, watermark, DTOCV, DLL_CTRL); the "Actual bus rate" print divides by zero at /1 (crashes the host build). | Verified (RM reviews, bench) | fixed (`imx8m-dram-cleanup`): `640e034e`, `d029efe1`, `8e53eed4`, `b3ec3920`; the 1 ms delay `e87a83f8`, named fields `96a3a743`. Open: DTOCV is always 0xF (longer than the 5 s busy timeout), ADMA alignment unchecked |
| F-126 | `src/drivers/mmc/imx_usdhc.c` (R1b) | The uSDHC sets no TC when the busy of a command without data ends (i.MX8M Plus EVK: CMD6 with RSPTYP 11, INT_STATUS stays 0x1 for 5 s), though both RMs say TC marks the end of busy. The bench's model followed the RM and passed. | Verified (board) | fixed (`imx8m-dram-cleanup`, `3af5b056`): the driver watches DAT0, the model sets no TC there. The i.MX6UL is unverified |
| F-127 | `src/drivers/crypto/caam/imx_caam.c` `caam_wait_for_job` | The job status is read from JRSTAR after the output ring entry is removed, not from the entry's status word, and a mismatching descriptor address is logged as "Job failed". JRSTAR only holds the status of the last job that completed, so this works only while one job is in flight (true today). | Verified (SRM, code) | open (low) |
| F-128 | `src/cm/cm_main.c` `cmd_boot_bpak`, `src/board/imx8mpevk` | The imx8mpevk has no boot driver, so `boot_load()` returns `-PB_ERR_PARAM` before reading the image. `cmd_boot_bpak` has already answered OK, so the host keeps sending the image, pb parses it as commands (`Invalid command: 0`) and the board resets into the serial downloader. The host sees `Error: Transfer`. | Seen on the board (2026-10-29) | partly fixed: the EVK has a boot driver (2026-10-29), so `boot_load()` gets as far as the image. Open: how the protocol recovers when `boot_load()` fails before the image has been read |
| F-129 | `src/link.lds`, `src/link_flash.lds` | `.self_test_fn` is collected without `KEEP()`, so `--gc-sections` drops every entry: with `CONFIG_SELF_TEST=y` (jiffy) pb prints "All 0 tests passed". The four SHA-256 known-answer tests in `crypto.c` never run. | Seen on the board (2026-10-29), `.self_test_fn` is 0x20 bytes in `crypto.o`, `_self_test_fn_start == _self_test_fn_end` in `pb` | wontfix (self tests removed, D-019) |
| F-130 | `src/drivers/usb/imx_ci_udc.c`, `src/cm/cm_main.c` `cm_write` | Intermittently (alternating `punchboot log` and `auth`: within 6-18 iterations, once not in 120) a bulk IN reply on EP1 never completes: the dTD stays Active while the host waits for the data. `cm_write` polls `xfer_complete` without a timeout or a watchdog kick, so the watchdog resets jiffy after ~5 s and the host reports `Error: Transfer`. Seen as the glitch during the boot time measurements. Cause unknown; suspect the cached dQH: `xfer_start` writes `qh->next` into a possibly stale cache line and cleans the whole 64-byte line, writing old overlay fields back over the controller's. The prime/flush/completion loops have no timeouts. | Seen on the board (2026-10-29), located with breadcrumbs | fixed (`3746bc6c`): usbmon showed the host's 1024 byte OUT stage end after 512 bytes, the dump an EP2 overlay of the previous 512 byte command transfer into `align_buffer`: `xfer_start` wrote `qh->next` into a cached dQH line filled while the endpoint was primed and its clean wrote the old active overlay back, which the controller resumed. Now the line is invalidated first and the overlay token cleared (RM 54.4.6.6.3). 0 hangs in 1000 `log`+`auth` and 1000 `log`+board command rounds (8-11 in 500 before). F-132/F-133 and SDIS were not it |
| F-131 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_xfer_start` | For a buffer that isn't 4 KiB aligned, page 0 of the dTD is `align_buffer` at offset 0 with only `align_length` bytes valid, but the controller fills page 0 up to its 4 KiB boundary before page 1, so an unaligned transfer longer than `align_length` sends or receives the wrong bytes. pb's large buffers are aligned, the small ones fit in one page, so nothing hits it today. | Verified (code) | open: point page 0 at `align_buffer + 4096 - align_length` |
| F-132 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_set_configuration` | An IN endpoint is enabled with bit 6 (RXR) set instead of bit 22 (TXR), so the IN data toggle is never reset on SET_CONFIGURATION. 6UL RM ENDPTCTRL1: "Whenever a configuration event is received for this Endpoint, software must write a one to this bit". | Verified (code, RM 54.6.40) | fixed (`40f82aac`), not yet run on the board |
| F-133 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_set_configuration` | ENDPTCTRLn is written whole for one direction, leaving the other as type 00 (control): EP1 OUT and EP2 IN on pb's class. 6UL RM ENDPTCTRL1 note: "the unused direction type must be changed from the default control-type to any other type… leaving an unconfigured endpoint control causes undefined behavior for the data pid tracking on the active endpoint/direction". The write also clobbers the other direction if both are used (the RM asks for read-modify-write). A candidate for F-130. | Verified (code, RM 54.6.40, 54.4.6.3.2) | fixed (`afaa9c87`): read-modify-write, an unused direction is bulk; not yet run on the board |
| F-134 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_xfer_complete` | For an OUT transfer into a buffer that isn't 4 KiB aligned, the data is copied from `align_buffer` into the buffer and then the buffer is invalidated (DCIMVAC), which architecturally discards the copy while the lines are dirty. Every command read takes this path (`cmd` at 0x81826200 in the jiffy build), yet commands work on jiffy, and the D-cache does hold dirty lines there (the F-130 breadcrumbs in `.no_init` were lost on reset without a clean, log 2026-10-29). Not understood: perhaps the Cortex-A7 performs DCIMVAC as clean and invalidate (unchecked; that would also hide F-081 on jiffy). | Verified (code, symbol map); why it works is open | open: invalidate before the copy, not after; check DCIMVAC on the board (dirty a line, invalidate, read back) |
| F-135 | `src/arch/armv7a`, `src/plat/imx6ul/plat.c` `plat_mmu_init` | Nothing sets ACTLR.SMP before the MMU and caches are enabled; as I recall it, the Cortex-A7 MPCore TRM asks for that. What it changes on jiffy is unknown: the 2026-10-29 breadcrumbs show write-back caching of `.no_init` works without it, so it doesn't simply leave Shareable memory uncached (first guess in the USB review, withdrawn). | Reported (TRM from memory, not in `~/work/manuals`) | open: read ACTLR on jiffy (`mrc p15, 0, r0, c1, c0, 1`), get the TRM |
| F-136 | `src/drivers/usb/imx_ci_udc.c` (prime, flush, bus reset) | Against 6UL RM 54.4.6: the prime loop waits for ENDPTPRIME to clear but never checks that ENDPTSTAT got set (PRIME 0 and STAT 0 is a failed prime, 54.4.6.4.2.2; the dTD then stays Active for ever, F-130's symptom). `imx_ci_udc_flush_ep` doesn't check ENDPTSTAT and repeat a refused flush (54.4.6.6.5). The EP0 flush in `poll_setup_pkt` doesn't wait before EP0 is primed again. The bus reset handler waits `while (FLUSH == 0x00FF00FF)`, which ends when the first bit clears, and flushes without waiting for ENDPTPRIME to clear (54.4.6.2.1). None of the loops has a timeout. | Verified (code, RM) | open |
| F-137 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_xfer_complete` | Only the Active bit of the last dTD is checked. Halted, Data Buffer Error and Transaction Error are ignored (an RX overflow halts the endpoint and returns `PB_OK`), and the bytes left in Total Bytes are not reported (54.4.6.6.4). A short packet in an earlier dTD of a multi-dTD read retires that dTD and leaves the last one Active. pb's exact-size protocol avoids the last today. | Verified (code, RM 54.4.6.6.4) | open |
| F-138 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_poll_setup_pkt`, `src/drivers/usb/usbd.c` | Control transfers against 6UL RM 54.4.6.4.2: the setup packet is copied under the tripwire and then acked in ENDPTSETUPSTAT; the RM acks first, so a second SETUP arriving between the two is acked unread. After priming an EP0 data or status stage nothing checks ENDPTSETUPSTAT, so a SETUP that aborts the control transfer leaves `wait_for_completion`/`imx_ci_udc_xfer_zlp` spinning until the watchdog. Unsupported requests are never STALLed (54.4.6.3.2): the host waits out its control timeout (e.g. `lsusb -v` asking for the BOS descriptor) and `usbd_xfer_complete` fails the transfer in progress. `USB_SET_IDLE` primes an OUT status stage a no-data request doesn't have. EP0 IN has ZLT disabled, so a reply that is a multiple of 64 bytes and shorter than wLength would need a ZLP it never gets (no current descriptor is). | Verified (code, RM) | open |
| F-139 | `src/drivers/usb/imx_usb2_phy.c`, `src/board/jiffy/board.c` `cm_board_init` | 6UL clocks and PHY: `imx_usb2_phy_init` writes PLL_SIC at USBPHY+0xa0, which the 6UL USBPHY doesn't have (its map ends at VERSION, 0x80; the PLL is CCM_ANALOG_PLL_USB1). jiffy only sets EN_USB_CLKS in PLL_USB1 (reset 0x00012000: BYPASS set, POWER clear) and relies on the ROM having powered, unbypassed and locked PLL3. The charger detector is on after reset (USB_ANALOG_USB1_CHRG_DETECT EN_B = CHK_CHRG_B = 0) and RM Fig. 55-3 turns it off before D+ is pulled up; pb doesn't. Works on the board. | Verified (code, RM 18.7.2, 55.3, 55.2.7) | open |
| F-140 | `src/drivers/usb/imx_ci_udc.c` | USBMODE.SDIS is set; the RM: in high speed every OUT packet is then answered with NYET, and it "substantially limits" throughput. Worth measuring for flashing speed. The init also writes ENDPOINTLISTADDR before USBMODE.CM, the RM (54.4.6.1) sets the mode first. `CMD_ITC(x)` cast to `uint16_t` after the shift, so ITC was always 0 (immediate; fine for a polling driver). | Verified (code, RM 54.6.33, 54.4.6.1) | open (ITC: `USBCMD_ITC(0)` since `5bd1d20e`, same value) |
| F-141 | `tools/punchboot/usb.c` `pb_usb_read`, `pb_usb_write` | The actual length (`rx_sz`) of a bulk transfer is never checked, so a short read or write counts as complete and the host and pb can get out of step without an error. | Verified (code) | fixed (D-021): `UsbTransport` raises `TransferError` on a short transfer |
| F-142 | `src/cm`, `punchboot part read` | `part read` of eMMC BOOT0 or BOOT1 on jiffy fails with `Error: I/O error` (2.8.0, 2026-10-29). Not looked into. | Seen on the board | open |
| F-143 | `src/drivers/mmc/mmc_core.c:168` | `#ifdef MMC_CORE_DEBUG_IOS` lacks the `CONFIG_` prefix, so the Kconfig option `MMC_CORE_DEBUG_IOS` never has an effect. | Verified (code) | open |
| F-144 | `src/plat/imx8x/plat.c` `imx8x_wdog_init` | With `CONFIG_ENABLE_WATCHDOG` off the function has no `return` (UB; the caller gets garbage). | Verified (code) | open |
| F-148 | `src/plat/imxrt/plat.c` `plat_init` | The clock roots depend on how the chip was reset: pb sets the ARM PLL and the periph clock muxes but not all dividers, so it inherits them. On the RT1060 EVK after a power-on boot (loaded over JTAG after the ROM) it reports AHB 150, IPG 75, PER 37 MHz; booted from the NOR after a software reset, AHB 600, IPG 200, PER 100 MHz. 200 MHz is above the IPG root's 150 MHz maximum (RT1060 data sheet). Set AHB_PODF, IPG_PODF and PERCLK_PODF explicitly. | Verified (board) | open |
| F-149 | `src/board/imxrt1060evk/imximage.cfg` | The DCD writes IOMUXC_GPR17 (0x400ac044 = 0x55555555) and sets GPR16 bit 2 to make all of the FlexRAM OCRAM. HAB rejects the write: `punchboot log hab` on the RT1060 EVK has a failure, invalid address (0x22) in a CSF/DCD command, for that write. So the FlexRAM keeps its fused split (256 KiB OCRAM); pb fits, since its buffers end at 0x202bf000. On an open part the ROM goes on; on a closed one the boot fails. Drop the DCD or set GPR17 from pb (the ROM's DCD address list doesn't allow IOMUXC_GPR). | Verified (board, HAB log) | open |
| F-150 | `src/board/imx8mpevk/board.c` | The board booted with the Linux boot driver, which jumps to `kernel` at EL3; arm64 Linux needs TF-A's BL31. TF-A v2.12's 8MP BL31 takes BL33 from the `bl_params` in x0 only from OCRAM/TCM and swaps base and size in that check (below 0x960000 then), else BL31 jumps to 0x40200000 without arguments. | Code (TF-A v2.12 source) | fixed: `ac10a592` (ATF boot driver), `cb1092bb` (`bl_params` in `.data`, 0x936168), `f4f01c5b` (the system counter: BL31 hung in `udelay()` without it), `875ccea8` (no SLC), F-071. Boots to the login prompt on the EVK |
| F-151 | jiffy on the bench (2026-10-29) | Linux froze a few seconds into the boot (no kernel message) and jiffy power-on-reset about 2.5 s later (`POR (1)`), with pb 3.0.0 and v2.7.1, the test bench and the old OP-TEE image, with or without kickstart, network or pbstate. Cause: jiffy's USB (command mode) was on the VIA VL813 hub. Plugged into the host directly, the test bench boots to the login prompt (dm-verity root, `pb,active-system` A, verified). Why the hub resets jiffy isn't known (port power switching after pb's USB disconnect?). Bisecting software ruled out: cpuidle, CAAM, usbotg1, watchdog, network, pbstate, kickstart. Userspace `devmem` aborts on every peripheral (AIPS-TZ defaults, privileged only), unrelated. | Verified (board) | closed: bench, not software |
| F-152 | `src/drivers/mmc/mmc_sd.c:303` | `dev->rca << RCA_SHIFT_OFFSET`: the 16-bit RCA is promoted to `int` and shifted by 16, which overflows for an RCA from 0x8000 (UBSan in `test_mmc_sd` and `test_imx_usdhc` with `PB_SANITIZE=ON`, RCA 0xAAAA). The sanitizer CI run should fail on it. | Verified (UBSan) | open |
| F-153 | Linux 6.18 + TF-A v2.12 on the i.MX8M Plus | Linux powers the audio mix through the GPC itself, which leaves its AIPS5 bridge at the reset defaults (TF-A configures AIPS5 only when its own SiP call powers the domain): the first access (audio mix clock driver, MU2) takes an SError. Not pb; U-Boot doesn't set AIPS5 either. Newer kernels have `drivers/bus/imx-aipstz.c` (MPR0 = 0x77777777 on probe and runtime resume) with a node in `imx8mp.dtsi`. | Verified (EVK) | worked around: the test bench's DTS disables AIPS5 (`59569a10`) |
| F-154 | `src/drivers/crypto/caam/imx_caam.c`, i.MX8M | pb never instantiated the CAAM's RNG4. On jiffy Linux runs secure and did it itself; on the 8MP Linux runs non-secure behind TF-A and can't (`caam: failed to acquire DECO 0`, `failed to instantiate RNG`), so there was no `rng-caam` and the entropy pool filled slowly. | Verified (EVK) | fixed 2026-10-30: `imx_caam_rng_init()` instantiates both state handles with prediction resistance (Linux tears down a handle without, which needs DECO 0) and generates the secure keys; jiffy, imx8mevk, imx8mpevk call it. Both boards: `rng-caam`, RDSTA 0x40000033, crng ready at 0.15 s (8MP) / 0.27 s (jiffy). 27 ms per boot on both; every reset kind clears the CAAM. imx8mevk not tried |
| F-155 | i.MX8M Plus EVK, `30be0000.ethernet` (FEC) | The FEC's PHY (address 1, reset `gpio4 2`) doesn't answer on MDIO (`MDIO device at address 1 is missing`, `Unable to connect to phy`), with the upstream DTS. The EQOS port probes. No cable was attached. | Seen on the board | open |
| F-156 | Linux 6.18 on the i.MX8M Plus EVK, `drivers/regulator/pca9450-regulator.c` | The PCA9450 driver registers a restart handler at priority 130, above PSCI's 129, that sends the PMIC SW_RST 0x14 (a cold reset). Every `reboot` and every panic restart (`panic=5`) power cycles the DRAM and loses ramoops, whatever `reboot=` says; pb's `log linux` then shows an older record. Only a watchdog reset (WDOG_B, warm with `nxp,wdog_b-warm-reset` in the test bench DTS) keeps it: a hang, which panics Linux on the watchdog pretimeout 2 s before the reset. Not pb. jiffy has no PMIC handler and keeps it on every path. | Verified (EVK, pb fill/check and Linux pstore) | open: a test bench kernel patch that lowers the handler below PSCI, or a product DTS/board that doesn't need it |
| F-157 | `src/boot/image_helpers.c` `boot_image_verify_parts`, `src/boot/dtb.c` `overlaps_pb` | The check that a part doesn't load over pb lists code, data, stack, bss and `.no_init`, but not `.ro_data` (keystore included; `image_helpers.c` imports `_ro_data_region_*` and never uses them), `.ramlog` or the i.MX8M's `.umctl`. A signed image with a part on any of them passes. Two hand-kept lists of the same regions. | Verified (code, jiffy/8MP maps) | open: one list, or two linker ranges for all of `ram` and all of `buffers` |
| F-158 | `src/plat/{imx6ul,imx8m,imx8x}/plat.c` `plat_mmu_init` | `rw_nox` maps `[_zero_region_start, _no_init_end)` as one RW Normal region, which assumes `ram` and `buffers` are next to each other. On the 8MP that's 0x944000..0x40885000, about 1 GiB: unpopulated address space as Normal memory (speculative accesses allowed), the AIPS/GIC/DDRC windows (they still get Device attributes only because the innermost region wins) and a partial overlap with the static OCRAM entry, which `mmap_add_region` would reject with `ENABLE_ASSERTIONS`. On jiffy and the 8X the gap is DRAM. | Verified (code, 8MP map) | open: map `ram` and `buffers` as two regions |
| F-159 | `tools/punchboot/transport.py` `_usb_serial`, pb 2.0.0 on the i.MX8QXP MEK | pb 2.0.0's cdns3 driver overflows the USB serial number string descriptor (`Errno 75`, `lsusb` shows it empty). The host skips devices whose serial it can't read, so `punchboot list` doesn't show the board and `-u` can't select it; without `-u` the first device is used, which works only when it is the only one. New host + old device (D-008). | Seen on the board (2026-11-08) | fixed (work-3.0.0-plat): EP0 is 64 byte packets with ERDY and ISP as Linux (`bee9508b`); the host asks a device without a readable serial for its identifier (`84cf2137`). Was open. Cause, pb 3.0.0 too: `cdns3_udc_core.c` configures EP0 with 512 byte packets (SuperSpeed) at high speed, so the 74 byte string goes out as one packet (babble). With 64 (Linux's value, the descriptor's `bMaxPacketSize0`) every transfer up to 64 bytes works but a longer one never completes, also with EP0 configured again on connect and reset and with a TRB burst length (tried on the MEK, the board then hangs in enumeration). Effects: the host takes ~22 s to enumerate the MEK after a reset (the kernel retries the string), and a CLI run that reads the serial number of every device (`punchboot list`) once ended in a watchdog reset of the MEK. Host side: list such devices by `PB_CMD_DEVICE_IDENTIFIER_READ` |
| F-160 | `src/cm/cm_main.c` `cmd_bpak_read` | Returns `bpak_valid_header()`'s bpak error code to `cm_run()`, which logs it as a pb error (`Command error -7` for `-BPAK_NOT_SUPPORTED`). The host gets the right `NOT_FOUND`, only the log is wrong. | Seen on the RT1060 EVK (2026-11-08) | open |
| F-161 | `src/plat/imx6ul/rot_helpers.c` | `imx6ul_read_key_status()` read bank 5 word 5, | Verified | fixed |
| | | revocation writes word 7 (`OCOTP_SRK_REVOKE`, RM 35.5.58). A revoked key | | (work-3.0.0) |
| | | still read as valid, and the bits of word 5 (boot config) as revoked keys. | | |
| F-162 | `src/plat/imx8x/slc_helpers.c` | `sc_seco_chip_info()`'s error was ignored, and the life | Verified | fixed |
| | | cycle compared uninitialised. A failed read could report CONFIGURATION on a | | (work-3.0.0) |
| | | closed chip, and `slc_set_configuration_locked()` act on it. | | |
| F-163 | `src/plat/imx8x/slc_helpers.c` | Only life cycle 0x80 (closed with NXP firmware) read | Verified | fixed |
| | | as CONFIGURATION_LOCKED. Closed (0x40), field return (0x100, 0x200) and no | | (work-3.0.0) |
| | | return (0x400) read as CONFIGURATION: secure boot on, command mode open. | | |
| F-164 | `src/plat/imx8x/rot_helpers.c` | `imx8x_revoke_key()` ignored the key: it committed | Verified | fixed |
| | | whatever the AHAB header revokes and returned `PB_OK`, also when no fuse | | (work-3.0.0) |
| | | changed. The host was told a key was revoked that wasn't. | | |
| F-165 | `src/plat/{imx6ul,imx8x}/rot_helpers.c` | No bound on the key's revoke slot. | Verified | fixed |
| | | The 6UL revokes SRK0-2 (`SRK_REVOKE[2:0]`, RM Table 5-9), the keystore has | | |
| | | 4 keys: revoking key 3 burns bit 3, which the fuse map doesn't define, and | | |
| | | pb then rejects key 3 although the ROM still accepts it. On the 8X a key | | |
| | | past the four OEM SRKs reads as valid forever. The platform should say how | | |
| | | many slots it has. `work-3.0.0-plat`: `imx_hab_fuses.revocable`, the 8X | | |
| | | refuses past its four (F-164). The slot was the key's place in the | | |
| | | keystore: dropping a revoked key moved the next onto its burnt bit. Now | | |
| | | `CONFIG_KEYSTORE_SLOTS` lists them (`test_rot_slots`). | | |
| F-166 | `src/plat/*/slc_helpers.c` | The SLC state changes are checked differently per | Verified | partly fixed |
| | | platform: the 6UL locks only from CONFIGURATION, the 8X from any state. No | | |
| | | platform reads the fuses back after writing them. The rules belong in | | |
| | | `src/slc.c`. `work-3.0.0-plat`: they are; still no read-back (the 8X's | | |
| | | life cycle may only change on reset). | | |
| F-167 | `src/plat/imxrt/rot_helpers.c` | The RT1060 reports every key as valid and can't | Verified | partly fixed |
| | | revoke one, although HAB has `SRK_REVOKE` there too. `work-3.0.0-plat`: it | | |
| | | reads `SRK_REVOKE` (`imx_hab_fuses.c`); the EVK board doesn't revoke or lock. | | |
| F-168 | `src/cm/cm_main.c`, `src/slc.c` | Command mode asked for authentication only when the | Verified | fixed |
| | | SLC read CONFIGURATION_LOCKED. When the board init failed before `slc_init()` | | (work-3.0.0) |
| | | (eMMC init on jiffy, DDR training on the i.MX8M) or the read failed, the read | | |
| | | was an error and every command was open on a locked device. Now an unset SLC | | |
| | | is `-PB_ERR_STATE`, and any error but `-PB_ERR_NOT_SUPPORTED` is locked. | | |
| F-169 | `src/plat/imx8m/plat.c` | The unique id, and so the device UUID, is fuse words 1 and 2 | Verified | fixed |
| | | (0x410, 0x420) on the 8MP too. There 0x410 is lock fuses and UNIQUE_ID[63:0] is | | |
| | | 0x420 and 0x430 (8MP RM Table 6-35; Linux reads them, the EVK's serial number | | |
| | | ends in its words 3 and 2). The EVK's UUID 884651b6-... is MD5 of words 1, 2: it | | |
| | | changes when a GP lock fuse is burnt, and UID[63:32] is unused. Words 2, 3 give | | |
| | | 9438ef3f-...; fixing it changes the 8MP's device UUID (no 8MP release yet). | | |
| | | Fixed (`work-3.0.0-plat`): the 8MP reads words 2 and 3, the EVK is 9438ef3f-... | | |
| F-170 | `src/board/{imx8qxmek,imx8dxlevk}/board.c` | The example configuration fuses BOOT0 | Verified | fixed |
| | | (0x920) to 0x2, the eMMC boot mode, without BT_FUSE_SEL (0x920[6]). The ROM | | |
| | | takes the boot mode from the fuses only when the boot pins are 0 and BT_FUSE_SEL | | |
| | | is set (QXP RM 5.3.5, Table 7-2; DXL RM Table 5-1): with the pins at 0 it goes | | |
| | | to serial download. jiffy sets the 6UL's BT_FUSE_SEL. Fixing it burns one more | | |
| | | fuse on devices that ran slc set-configuration. Fixed (`work-3.0.0-plat`): BOOT0 | | |
| | | is BT_FUSE_SEL and the eMMC boot mode, 0x42. Not run on hardware. | | |
| F-171 | `testbench/simple/src/main.c` | The boot time test reported one boot of 3262 s (it fails | Plausible | open |
| | | over 1 s) and passed when run again. The time is the clock at entry minus the | | |
| | | reset time kept in the RT1060's persistent words. A series that didn't end (no | | |
| | | magic cleared) leaves them set: the next series' first boot takes the old reset | | |
| | | time. 3262 s is about the time since the previous boot time run. Not verified. | | |
| F-172 | `src/drivers/usb/cdns3_udc_core.c` (the MEK) | Writes reset the MEK now and then: a | Verified | open |
| | | small `part write` after a large one, or the first write right after the board | | |
| | | comes up ("Generic error", then the board resets, a watchdog reset into the ROM | | |
| | | loader in serial download mode). Seen with and without the EP0 fix (F-159), from | | |
| | | RAM (uuu) and from eMMC. Waiting ~5 s after it enumerates and a `dev show` before | | |
| | | the write helped. The cdns3 bulk path's TODOs (TRB count, burst, TRBERR) are the | | |
| | | suspects. A reset with garbage in the enabled boot partition leaves the MEK dead | | |
| | | until SW2 is set to serial download. | | |
| F-173 | `src/boot/ab_state.c` | `init` ignores `bio_read()`'s result: a copy that can't | Verified | open |
| | | be read counts as corrupt. When both reads fail and writes work, it writes the defaults | | |
| | | (nothing enabled) over a good state and returns `PB_OK`: the next boot has no active | | |
| | | partition and stays in command mode. Shown on the ramdisk (`fail_reads_after`). Install | | |
| | | the defaults only when both copies were read and are bad, else return the read error. | | |
| F-174 | `src/boot/boot.c`, `ab_state.c` | A slot that the A/B state picks and that then fails | Verified | open |
| | | to load or authenticate isn't reported back. A verified slot is picked again on every | | |
| | | boot (command mode, the 10 s transport timeout, reset), for good: the other verified | | |
| | | slot is never tried. An unverified slot loses an attempt each time and rolls back. | | |
| | | Decision for Jonas: on such a failure clear the slot's verified bit (or its attempts) | | |
| | | so the next boot rolls back, at the risk of a downgrade after a transient read error. | | |
| F-175 | `src/boot/ab_state.c` | `boot activate` with an unknown UUID clears `enable`, | Verified | open |
| | | `verified` and `error` in RAM, then returns `-PB_ERR_PART_NOT_BOOTABLE`. For the rest of | | |
| | | command mode `boot status` says none and booting the active slot fails, and the next | | |
| | | commit writes it (on the host board a board register write: nothing to boot after the | | |
| | | reset). `test_set_boot_partition` asserts the RAM change. Return before the change. | | |
| F-176 | `src/boot/ab_state.c` | `init` keeps two valid copies that differ (power lost | Verified | open |
| | | between the two writes, by pb or `pbstate`). A verified system never commits, so the | | |
| | | stale backup can stay, and it comes back when the primary goes bad (shown: B verified | | |
| | | in the primary, the old trial state in the backup, a bad primary rolls back to A). | | |
| | | Commit when they differ: the primary is always written first. | | |
| F-177 | `src/boot/ab_state.c` | Speculative rollback sets the fallback's attempts to 1 | Verified | open |
| | | and boots it at once without using one: it gets two boots (`pbstate switch B 3` gives | | |
| | | three). In the ping-pong, `error =` drops the other rollback bit. No board uses the | | |
| | | speculative mode; `test_speculative_rollback_from_a` asserts B, B, A. Intended? | | |
| F-178 | `src/board.c` | When `boot_ab_state_init()` fails (a repair write, a state partition | Verified | open |
| | | missing from the table), `board_setup()` returns before `late_init` and the boot | | |
| | | driver: in command mode `boot activate` is `-PB_ERR_NOT_SUPPORTED` and `boot partition` | | |
| | | `-PB_ERR_PARAM`, the commands that would repair it. The GPT failure path above sets up | | |
| | | the driver for that reason. | | |
| F-179 | `src/boot/ab_state.c` | Minor. With both `enable` bits set `get()` boots A, | Verified | open |
| | | `boot_ab_state_get_boot_partition()` reports none and `pbstate` both active (no writer | | |
| | | sets both today; `validate()` could reject bits outside A and B). Normal mode with no | | |
| | | verified fallback logs "Rollback to B system", then fails with `-PB_ERR`. A rollback to | | |
| | | a verified slot fails when its commit fails (read-only eMMC); booting it would be safe, | | |
| | | the next boot picks it again. `boot activate` leaves `remaining_boot_attempts` as it | | |
| | | was. `pb_boot_state_defaults(&boot_state_backup)` does nothing: commit writes only | | |
| | | `boot_state`. | | |
| F-180 | `tools/pbstate/src/pbstate.c` | `pbstate_is_system_verified(NONE)` returns | Verified | open |
| | | `!!enable` (a copy of `is_system_active`). `pbstate_set_system_verified(X)` zeroes the | | |
| | | attempts also when X isn't the enabled system: it ends the other system's trial, and | | |
| | | the next boot rolls back. | | |
| F-181 | `imx_caam.c`, `cm_main.c`, `image_helpers.c` | High. An async hash job that is | Reported (probe) | open |
| | | abandoned (a read error in `boot_image_load_and_hash()`, a failed `part verify`: no `hash_final`) stays | | |
| | | in the CAAM's one-slot output ring. Every later `caam_wait_for_job()` returns the previous job's | | |
| | | completion, so `caam_ecda_verify()` reports verified from the hash job's status. In a model of the ring, | | |
| | | `auth token` then accepted any well-formed signature on a locked device. Not run on hardware. Same root | | |
| | | as F-127. Drain the ring in `caam_hash_init()` and before a verify, take the status from the ring entry, | | |
| | | finish the hash on every error path. | | |
| F-182 | `src/drivers/mmc/mmc_core.c:163`, `cm_main.c` | High. `mmc_rw()` uses `blocks = length / | Verified | open |
| | | 512` while bio's range check rounds up: a `part write` whose size isn't a block multiple loses its tail | | |
| | | and returns OK; under 512 bytes CMD24 goes out with no data and the card stays in rcv. `part verify` of | | |
| | | such a size hashes stale buffer bytes. The host disk and the ramdisk write exact byte counts, so no test | | |
| | | sees it. In cm: zero-pad the tail and round up (hosts send raw sizes, D-008). | | |
| F-183 | `Makefile:124-134` | High. `keystore.o` has no prerequisites and `srk_fuse.o` depends | Verified | open |
| | | only on the file: a changed `CONFIG_KEYSTORE`/`CONFIG_SRK_FUSE` isn't regenerated in an existing build | | |
| | | directory (shown with an out-of-tree jiffy build). pb keeps the test keys and the test SRK hash, which | | |
| | | `slc set-configuration` would fuse. `PB_VERSION` has the same gap. Depend on the input file and the | | |
| | | config. | | |
| F-184 | `src/drivers/usb/cdns3_udc_core.c:263` | The clean covers `desc_count` TRBs, not the | Verified | open |
| | | terminating one at `trb[desc_count]`: for some counts its flags word is in a cache line that isn't | | |
| | | cleaned, and a stale TRB from a larger transfer stays in memory. The effect on the controller is | | |
| | | unverified; it fits F-172 (a small write after a large one). Clean `desc_count + 1`. | | |
| F-185 | `src/cm/cm_main.c`, `usbd.c` | `authenticated` survives a USB replug, a bus reset and | Reported | open |
| | | `restart_command_mode`: the next host on a powered, locked device is authenticated. No idle timeout. | | |
| | | Clear it on bus reset and transport restart. | | |
| F-186 | `cm_main.c` `cmd_part_verify`, `cmd_bpak_read` | Neither checks | Reported | open |
| | | `BIO_FLAG_READABLE`/`VISIBLE`: `part verify` with size 1, 2, ... and hash guesses reads any partition | | |
| | | byte by byte, hidden ones too. It also retargets the stream's `block_dev`. | | |
| F-187 | `cm_main.c:881` | The auth gate sends one result for `board command`, but hosts send the | Reported | open |
| | | request anyway (F-095): the device parses it as commands and answers INVALID_COMMAND twice; with an | | |
| | | empty request the host hangs. Drain the request and send the second result. | | |
| F-188 | `cm_main.c` `cmd_part_erase` | No `BIO_FLAG_WRITABLE` check. F-055 listed it and is | Reported | open |
| | | marked fixed, but `6a1a0e23` fixed only the range. | | |
| F-189 | `cm_main.c`, `usbd.c` | The watchdog is kicked only between commands: a `part verify` or | Reported | open |
| | | `boot part` of a few hundred MiB against a 5 s watchdog resets the board (timing not measured). | | |
| | | `cm_read`/`cm_write` spin on AGAIN without a kick or timeout. | | |
| F-190 | `cm_main.c` | Low. IDENTIFIER_READ sends 23 bytes of `buffer[0]` unauthenticated (`rz` | Reported | open |
| | | not cleared), the partition table 57 of every 128. `boot_jump()` can return after `disconnect()`. | | |
| | | `bytes_to_verify -= 4096` underflows for bpak with size < 4096. A failed data read in | | |
| | | `cmd_stream_prep_buffer` still answers OK. `block_dev` starts at 0, a valid bio. No build-time bounds | | |
| | | (buffer size, 16 keys in `cmd_slc_read`, board name). No real board sets `BIO_FLAG_READABLE`: the cause | | |
| | | of F-083 and F-142. | | |
| F-191 | `src/drivers/partition/gpt.c:353,412` | A valid GPT with fewer than 128 entries: | Reported (probe) | open |
| | | the backup array is read at `last-32` and the repair writes 33 blocks. Every boot rewrites the PMBR and | | |
| | | primary, fails on the backup and registers no partitions. | | |
| F-192 | `gpt.c:433` | The entry loop stops at the first entry with `first_lba == 0`: | Reported (probe) | open |
| | | with entry 0 deleted (`sgdisk -d 1`) the later partitions aren't registered and init returns OK. Skip | | |
| | | empty entries. | | |
| F-193 | `gpt.c:263,446` | LBAs from the disk aren't validated: an entry at LBA 1 becomes | Reported (probe) | open |
| | | a writable partition over the GPT, a 64-bit `last_lba` is truncated, a primary with MyLBA 5000 makes the | | |
| | | repair write there. | | |
| F-194 | `gpt.c:296`, `src/bio.c` | Installing a table from command mode doesn't update | Reported (probe) | open |
| | | the bio pool: until a reset the old partitions are listed and written at their old LBAs. Not documented | | |
| | | either. | | |
| F-195 | `gpt.c:385,416` | Low. Repairing one copy rewrites PMBR, primary and backup: a power | Reported | open |
| | | loss while the good copy is rewritten leaves no table. Write only the bad copy. | | |
| F-196 | `src/drivers/fuse/imx_ocotp.c:73` | The fuse timing is hard-coded for a 66 MHz ipg_clk | Reported | open |
| | | (STROBE_PROG 0x299). The RT1060 uses the driver at 75-200 MHz (F-148): a 3-9 us program strobe instead | | |
| | | of 10 us on an irreversible write. Checked against both RMs, not on hardware. Compute TIMING from the | | |
| | | ipg rate. | | |
| F-197 | `imx_ocotp.c:69,91-104,121` | Against the 6UL RM: ERROR is cleared by a plain CTRL | Reported | open |
| | | write, not CTRL_CLR (35.5.1), so after one failed access everything is `-PB_ERR_IO` until reset; no 2 us | | |
| | | postamble after a write (35.3.1.4); the index is masked with 0x7F, not checked; a read doesn't check | | |
| | | ERROR (a locked word reads 0xBADABADA with OK). | | |
| F-198 | `src/drivers/memc/imx_flexspi.c` | The erase starts with the largest block without | Reported | open |
| | | checking the address is aligned to it, rounds a short tail up to a full block, ignores write-enable and | | |
| | | busy-wait results. Latent with the EVK's layout. IDATSZ is 16 bits: a read of 64 KiB or more is | | |
| | | truncated and waited for forever. No loop has a timeout or a watchdog kick. | | |
| F-199 | `src/drivers/mmc/imx_usdhc.c:338` | A command timeout (CTOE) returns | Reported (probe) | open |
| | | `-PB_ERR_IO`; `mmc_sd.c:281` detects SD 1.x cards by `-PB_ERR_TIMEOUT` on CMD8, so a 1.x card fails init | | |
| | | on uSDHC. `test_sdsc_v1` runs only on the HAL model. | | |
| F-200 | storage, low | `imx_hab_fuses.c:159`: a partly programmed SRK word is refused as already | Reported | open |
| | | fused. `mmc_core.c`: `mmc_wait_state()` and `mmc_bio_xfer()` (`max_chunk_bytes == 0`) can loop forever; | | |
| | | a zero BOOT_MULT gives a 2^32 block bio. `imx_usdhc.c`: ADMA2 addresses truncated to 32 bits. | | |
| | | `plat_get_unique_id()` (6UL, 8M, RT) ignores `imx_ocotp_read()` errors: the device UUID comes from | | |
| | | uninitialised stack. | | |
| F-201 | `src/drivers/usb/usbd.c:28` | `enumerated` is never cleared: on the 8MP EVK a replug or | Reported | open |
| | | bus reset ends in a read on an unconfigured EP2, `cm_run` exits and the board resets. Clear it in | | |
| | | `usbd_init()` and on bus reset. | | |
| F-202 | `src/plat/imx8m/plat.c:225`, `main.c:42` | After a failed DDR init command mode runs | Reported | open |
| | | with `cmd`, `result`, `buffer` and dwc3's TRBs in untrained DRAM (`.no_init`, 0x40062000-). Reset on a | | |
| | | DDR failure, or keep them in OCRAM. | | |
| F-203 | `usbd.c:340-349` | An EP0 request during a bulk transfer cancels it and restarts it from | Reported | open |
| | | offset 0: a `punchboot list` or `lsusb -v` during a `part write` corrupts the chunk and swallows the | | |
| | | next command. | | |
| F-204 | `cdns3_udc_core.c:507` | `cdns3_udc_core_stop()` is a stub: after `disconnect()` the MEK | Reported | open |
| | | stays enumerated with EP0 armed at pb's `setup_pkt`, a SETUP after the handover writes 8 bytes into the | | |
| | | OS's memory. | | |
| F-205 | USB, low | cdns3: `setup_pkt` shares its cache line with boot.c's callbacks and digest | Reported | open |
| | | (invalidated per SETUP, F-081 class); `trb[128]` and `align_buffer` bounds; four waits without timeout. | | |
| | | dwc3 rounds an OUT TRB up to the max packet: up to 511 bytes past a part in a RAM boot. | | |
| | | `imx_ci_udc.c:193`: `align_length` never cleared (F-131). `imx_salvo_phy.c`: register table from U-Boot | | |
| | | (GPL-2.0+) in a BSD file (F-108 class). | | |
| F-206 | `src/drivers/wdog/imx_wdog.c`, `plat/imx6ul/plat.c:385` | `CONFIG_ENABLE_WATCHDOG=n`: | Reported | open |
| | | `base` stays 0, kicks and `imx_wdog_reset_now()` write at address 0 and `plat_reset()` spins (every | | |
| | | defconfig has it on). WT is `delay_s * 2` unmasked: 5.5 s for 5, 128 s wraps. `reset_now` also clears | | |
| | | WDA. | | |
| F-207 | `src/arch/*/entry*` | The D-cache is enabled without invalidating it first (v7a, v7m, | Reported | open |
| | | v8a). The 6UL and RT ROMs use the caches and only disable them (RMs 8.4.4, 9.5.4); a stale valid line | | |
| | | would serve old data. Plausible, jiffy works. Invalidate by set/way before enabling. | | |
| F-208 | `src/plat/imx8m/plat.c` | The 8M never enabled its TZASC while | Verified (on the 8MP EVK) | fixed (8M) |
| | | `boot/atf.c` supports a BL32: the normal world could read secure DRAM (the 8M's F-096). TF-A doesn't enable | | |
| | | it: `bl31_tzc380_setup()` returns when GPR10's TZASC_EN is clear. Fixed: GPR10's TZASC_EN, ID_SWAP_BYPASS | | |
| | | and both locks before `imx8m_ddr_init()`, the DDR window in region 1, region 0 left secure. On the EVK: 16 | | |
| | | regions, 33 bit addresses; an NS read of a region made secure only is refused (FAIL_ADDRESS 0x20000000 for | | |
| | | PA 0x60000000: addresses from the DDR base), and passes again without it. GPR10 alone is enough, no | | |
| | | TZASC_ENABLE fuse (8MP RM 4.9.1.2 asks for one; none written). Linux boots through BL31 | | |
| F-209 | `src/link.lds:60-67` | The stack is directly above `.keystore_key`; on the RT1060 (no | Reported | open |
| | | MPU region, 4 KiB stack, software ECDSA) an overflow rewrites a root-of-trust key silently. | | |
| | | `.vector_handlers` is an orphan section placed after `_code_end`, outside the overlap check (F-157). | | |
| F-210 | arch/plat, low | v7m: CACR.FORCEWT never cleared at the handover, VTOR never set to pb's | Reported | open |
| | | table. 6UL: PLL1 raised without writing CACRR.ARM_PODF (F-148 class), SRSR never cleared (stale reset | | |
| | | reason). Exception policy differs per arch, v8 handlers run on the faulting stack and hang by default. | | |
| | | 8X `BOARD_RAM_END` maps non-RAM. `armv7a/arch.c:37`: clean then invalidate of the live stack in C. | | |
| | | `imx_uart.h`: `UFCR_RFDIV_1` is divide by 2. | | |
| F-211 | `src/delay.c:33-47` | Low. Timeouts compare with an absolute end tick: if the 32-bit us | Verified | open |
| | | counter wraps between `ts_end` and the next poll the timeout takes another 71 minutes. Callers ignore | | |
| | | `pb_timeout_init_us()`'s result. Use `(now - start) >= timeout`. | | |
| F-212 | `src/board/imx8dxlevk` | The DXL board offers `slc lock`, but its build can't sign pb | Reported | open |
| | | (`imx8xl_sign_image`: not supported): a locked DXL rejects the only image there is. Drop | | |
| | | `set_configuration_locked` until signing exists. | | |
| F-213 | `configs/*`, board `makefile.mk` | The SRK hash that is fused (`CONFIG_SRK_FUSE`) and | Reported | open |
| | | the SRK table pb is signed with are independent settings: a product that changes one fuses a hash that | | |
| | | closes the device to its own images. D-027 and `srk_fuse.h` claim they match. Derive or check at build | | |
| | | time. | | |
| F-214 | `src/board/imxrt1060evk` | `BOARD_IMXRT1060EVK_REV_C` only swaps the FCB; the NOR config | Reported | open |
| | | stays IS25WP064 (ID, 0xD7 erase, 8 MiB), so the probe fails on an EVKC's W25Q128JW. Untried on hardware. | | |
| F-215 | `boot.c:114`, `ab_state.c:108`, `board.h` | `.ab = NULL` is accepted, but such a board | Reported | open |
| | | can't boot from storage: no `get_boot_bio_device`, and BOOTABLE is only set in `boot_ab_state_init()`. | | |
| | | Only imx8mevk today. A `boot_part_uu` member. | | |
| F-216 | `src/board.c:122-158` | F-178 is one of three: a failing `init()` or `late_init()` also | Reported | open |
| | | returns before `setup_boot_driver()`, so a RAM boot of a rescue image isn't possible when the eMMC | | |
| | | fails. | | |
| F-217 | `src/board/host/board.c`, `test_fuse_bio.c` | The host board's fuses are on a GPT | Reported | open |
| | | partition and load in `late_init`: with a bad GPT a locked host device reads as open, the opposite of | | |
| | | real boards, so the D-026 order can't be tested end to end. | | |
| F-218 | boards, low | `IMX8X_FUSE_DRY_RUN` doesn't gate | Reported | open |
| | | `sc_seco_commit()`/`forward_lifecycle()`. The shared `default_command` changed the 8X boards' answer | | |
| | | from OK to NOT_SUPPORTED (not in `compatibility.md`). Board data isn't validated (`cm_transport` NULL, a | | |
| | | name over 16 characters, `.slc = NULL`). Dead Kconfig symbols (`ARMV8_EL3`, `choice BOARD`, | | |
| | | `IMXRT_FLASH`), `CM_AUTH_PASSWORD=y` without a callback. GPT tables can't align partitions. 8X | | |
| | | `early_boot()` ignores SCU results. | | |
| F-219 | `src/boot/simple.c`, `boot.c`, `board.c` | The XIP / `BOOT_SOURCE_IN_MEM` path can't be | Reported | open |
| | | reached (nothing sets `get_in_mem_image`), and if wired up it hashes memory at `pb-load-addr` but jumps | | |
| | | to `image_base_address + offset`. Extends F-070. | | |
| F-220 | security core, low | `caam_hash_input()` refills the alignment buffer while its async | Reported | open |
| | | job may read it (wrong digest, fails closed). `rot.c:117,134` return error codes through unsigned types: | | |
| | | `slc read` loops ~2^32 times after a failed `rot_init()`. Parts are 512-aligned, a 4 KiB block device | | |
| | | fails mid-load on a second part. `mbedtls_pb.c:183` skips `mbedtls_pk_free()`. `slc_set_configuration()` | | |
| | | goes on when the state read is NOT_SUPPORTED. | | |
| F-221 | `tools/pbstate/src/main.c` | `-s` with anything but a/b switches to NONE and | Reported (probe) | open |
| | | exits 0. `counter` is `uint8_t`: `-c 256` is 0, which means verified. `-w 0 zzz` writes 0. `strtol` | | |
| | | fails from 0x80000000 on 32-bit. Read-only calls end in `sync()`. | | |
| F-222 | `tools/pbstate/CMakeLists.txt`, `src/lib/crc.c` | `libpbstate.so` exports a | Reported (probe) | open |
| | | global `crc32` that isn't zlib's: with zlib loaded first every state fails the CRC check. Hide or rename | | |
| | | it. | | |
| F-223 | `tools/punchboot/session.py:242` | `_stream_finalize()` never checks the result. | Reported (probe) | open |
| | | FINALIZE enables the written eMMC boot partition (F-082): if that fails, `part write` exits 0 and the | | |
| | | board boots the old one. | | |
| F-224 | `tools/punchboot/__main__.py:567` | `boot enable` without its argument sends the | Reported (probe) | open |
| | | null UUID: the device disables boot, exit 0. Make it required. | | |
| F-225 | `__main__.py`, `session.py`, `partition.py` | The CLI replaces every exception's | Reported (probe) | open |
| | | message with a fixed string (no udev rule reads "Generic error"). Unknown partition flag bits or SLC | | |
| | | values from a newer device raise `ValueError`, which the CLI doesn't catch. Tracebacks for a missing | | |
| | | file, a binary board response, a non-semver version. | | |
| F-226 | `tools/punchboot/transport.py:39` | One fixed 10 s USB timeout; the device's caps | Reported | open |
| | | timeouts are ignored. After a timeout the session stays usable and reads the late result as the next | | |
| | | command's. Several error paths leave transfers unread the same way. | | |
| F-227 | host tools, low | `session.py:556` masks the verify size to 32 bits. With several | Reported | open |
| | | devices and no `-u` the first is used, also for `slc` commands. Three different is-this-bpak checks. The | | |
| | | CI's only-grows guards compare with the merge-base, which is HEAD on a push to master. | | |
| F-228 | `.github/workflows`, `docker/`, `scripts/ci` | Any `v*` tag publishes to PyPI with no | Reported | open |
| | | check against `version.txt`, master or the tests. Actions are pinned by tag, the publish action gets a | | |
| | | long-lived token. The dev image is partly pinned and rebuilt per job. CI builds only `pb.bin` (never the | | |
| | | signed images or `testbench/simple`), warnings don't fail. | | |
| F-229 | `Makefile:31` | The result of `genconfig.py` is ignored: a failure builds with | Reported (probe) | open |
| | | the old `.config`, a mistyped defconfig name builds the Kconfig defaults, both with exit 0. | | |
| F-230 | `doc/dev/testing.md`, `status.md` | Line coverage is 91.0% (4445/4886), not | Reported (probe) | open |
| | | 99.2%. `imx_caam.c` 38%: `caam_ecda_verify` and the hash path 0%. Never compiled by a host test: | | |
| | | `boot/atf.c`, `boot/simple.c`, `usbd.c`, `pb_dev_cls.c`, `partition/static.c`. No tests: `imx_ocotp.c`, | | |
| | | `imx6ul/hab.c`, the 8X RoT/SLC helpers, the UDCs, `imx_flexspi.c`. | | |
| F-231 | build, low | Pre-release builds identify as 3.0.0. `blobs/` is shared in the source root | Reported | open |
| | | (pb relinks every make). pbstate is built without sanitizers. `-fno-stack-protector`, no `-Wundef`. | | |
| | | `.gitignore` ignores `*.bin` globally, matching tracked pki files. | | |
| F-232 | docs | The README (also the PyPI page) describes removed things (PBI, C parts and binary | Reported | open |
| | | wheels, the auth cookie, Bebop/PICO) and lacks the 8MP EVK and RT1060. Missing: a porting guide for | | |
| | | 3.0.0, firmware build and image signing, the A/B update flow and pbstate, the SLC and fusing procedure, | | |
| | | a changelog, SECURITY.md. `arch-firmware.md` predates D-026 and has nothing on storage, USB, memory | | |
| | | layout or exceptions; `roadmap.md` phases 0-1 are done; `testing.md` lacks 12 unit tests; `imx8m-ddr.md` | | |
| | | step 4. | | |
| F-233 | `plat/imx8x/plat.c` | `imx8x_wdog_init()`: Without `CONFIG_ENABLE_WATCHDOG` the | Verified (code) | open |
| | | function has no `return`: `wdog_rc` is undefined, `plat_init()` may log a failed watchdog. | | |
| F-234 | `drivers/wdog/imx_wdog.c` | Also `src/plat/{imx6ul,imx8m,imxrt}/plat.c`: `base` is | Verified (code) | open |
| | | 0 until `imx_wdog_init()`. `plat_reset()` (`imx_wdog_reset_now()`) and `plat_wdog_kick()` write to address | | |
| | | 0 without `CONFIG_ENABLE_WATCHDOG`, and `plat_reset()` from an exception before `plat_early_init()` doesn't | | |
| | | reset. Every defconfig enables the watchdog today. The base is a constant: set it, arm on the option | | |
| | | (`platform.md` 3) | | |
| F-235 | `src/main.c` | After a failed `plat_early_init()` or `plat_init()` command mode | Verified (code) | open |
| | | runs without what it needs: no console or tick, no MMU, no fuses, on the 8M the buffers in untrained DRAM | | |
| | | (F-202). `platform.md` 2: reset. The RT's `plat_init()` also returns `mbedtls_pb_init()`'s error | | |
| F-236 | `plat_boot_reason` | `src/plat/*/plat.c`: No common meaning: the 6UL's own enum, | Verified (code) | open |
| | | the 8X's SCFW value, the RT's raw WRSR bitmask (several bits set: "?"), the 8M not implemented, the host 0. | | |
| | | `platform.md` 3: `plat_get_boot_reason()` with a kind, the raw value and a name | | |
| F-237 | `include/pb/plat.h` | Also `src/device_uuid.c`: `plat_get_us_tick()` is | Verified (code) | open |
| | | `unsigned int` in the header, `uint32_t` in three ports. `plat_reset()` isn't `noreturn`. | | |
| | | `platform_ns_uuid` is only picked up with `extern` in `device_uuid.c`, and nothing says it must never | | |
| | | change (`platform.md` 3.6) | | |
| F-238 | `plat/{imx8m,imxrt}/plat.c` | The tick and `log_time_start()` are set up in | Verified (code) | open |
| | | `plat_init()`, after `board.console_init()`: before it there is no `pb_delay_*()` and the log time is 0. | | |
| | | The 6UL and the 8X do it in `plat_early_init()` | | |
| F-239 | link scripts | `PB_ENTRY` repeated as a constant in | Verified (code) | open |
| | | `src/plat/{imx6ul,imx8x}/link.lds` and `src/board/imxrt1060evk/link.lds` (0x80001000, 0x80020000, | | |
| | | 0x20249000); the RT's `MEMORY` is in its board. `buffers` fixed in the platforms, `BOARD_RAM_*` only `-D` | | |
| | | flags, `PB_RAM_LENGTH` set by hand although it follows from the ROM's load window and `PB_ENTRY`. Nothing | | |
| | | checks at link time that pb is where the ROM may load it, `buffers` in working memory, or that they don't | | |
| | | overlap. `platform.md` 5.1: Kconfig values, `BOARD_DRAM_*`, the `pb` region, `ASSERT`s | | |
| F-240 | `src/board/*/makefile.mk` | The SoC's packaging and signing are in the boards: | Verified (code) | open |
| | | jiffy holds the 6UL CSF recipe (IVT at `PB_ENTRY - 0xC00`, uuu's 0x00910000), the MEK and DXL repeat the 8X | | |
| | | recipe. `FINAL_OUTPUT` is set with `$(eval)` in recipes. Signing inputs are Kconfig on the 6UL, make | | |
| | | variables on the 8X. `platform.md` 6.2 | | |
| F-241 | `plat/imx6ul/plat.c` | `tzasc_init()`: A failure is only logged: nothing tells | Verified (code, RM) | open |
| | | the boot driver that the TZASC is bypassed. Region 1 gets the physical DDR base, while 6UL RM 52.3 says the | | |
| | | TZASC sees the DDR controller's "local addresses": check with OP-TEE that its memory is protected. GPR9 has | | |
| | | no lock bit (RM p. 1277), so the secure OS's CSU policy decides whether the normal world can bypass it; the | | |
| | | TZASC_ENABLE fuse (RM p. 209) makes the ROM enable it. GPR1 TZASC1_BOOT_LOCK unread (TZC-380 TRM, DDI0431, | | |
| | | not on the share) | | |
| F-242 | `src/arch` | `src/arch/armv8a/Kconfig`, `src/arch/{armv7a,armv8a}/asm_macros.S`: | Reported | open |
| | | Leftovers: the Kconfig help mentions sunxi FEL (no sunxi platform), a TF-A stack macro calls | | |
| | | `plat_my_core_pos`, which no platform defines (the macro seems unused) | | |
| F-243 | `image_helpers.c` | `load_and_hash()`: The boot path never kicks the watchdog, | Verified (code) | open |
| | | only command mode does: loading and verifying must fit in `CONFIG_WATCHDOG_TIMEOUT` (5 s) from arming. | | |
| | | `platform.md` 3: kick once per chunk and before the jump, never in `pb_delay`/`pb_timeout` | | |
| F-244 | `plat/imx6ul/pb.csf.template` | On a closed 6UL nothing locked SRK | Verified (on jiffy) | fixed (6UL) |
| | | revocation before the jump: the CSF unlocks SRKREVOKE so that pb can revoke keys, and pb never set | | |
| | | OCOTP_SW_STICKY's SRK_REVOKE_LOCK (6UL RM 35.5.6). On jiffy the SRK_REVOKE shadow took writes while Linux | | |
| | | could run. Fixed: `plat_lockdown()` before the jump, `imx_ocotp_lock()`: SW_STICKY 0x1e after it and the | | |
| | | shadow write refused (HAB had locked field return and SCS already). A watchdog reset clears the locks, | | |
| | | revocation still works after a reboot. Open: the 8X (SECO) and RT, unchecked; the 8M once it has HAB | | |
| | | (`plat-imx.md` 4.3) | | |
| F-245 | `drivers/wdog/imx_wdog.c` | Each WDOG's power-down counter runs out of | Verified (on jiffy) | fixed (6UL) |
| | | every reset and drives WDOG_B low after 16 s unless WMCR.PDE is cleared (6UL RM 57.5.3). pb cleared it for | | |
| | | WDOG1 only, only with `CONFIG_ENABLE_WATCHDOG` (`imx_wdog_init()`); U-Boot clears WDOG1-3 on the 6UL and | | |
| | | 8M. A board with a WDOG_B on a pad to the PMIC resets after 16 s. Fixed on the 6UL: | | |
| | | `imx_wdog_disable_powerdown()` for WDOG1-3 in `plat_early_init()`; on jiffy all three enter pb with PDE set | | |
| | | (the ROM clears none) and read 0 after. Open: the 8M, the RT unchecked | | |
| F-246 | `src/plat/imx6ul/plat.c` | The AIPS-TZ bridges are never programmed: the 6UL RM | Reported (RM) | open |
| | | (14.6) says they "should be programmed before use", reset MPR 0x77000000 and OPACR 0x44444444 (supervisor | | |
| | | only, masters beyond the first two untrusted). U-Boot opens them (`init_aips()`); OP-TEE's code read | | |
| | | doesn't. Check on jiffy what Linux misses (user-mode or DMA access to peripherals), then set them in | | |
| | | `plat_init()` (`platform.md` 4.5) | | |
| F-247 | `src/board/*/board.c` | The boards wire the SoC's RoT and SLC functions | Verified (code) | open |
| | | (`board.rot`, the `slc_config` callbacks) and set up the crypto engine and its RNG (`imx_caam_init()`, the | | |
| | | RNG4 instantiation), while the RT's `plat_init()` sets up mbedtls. D-030: `plat_rot`, `plat_slc` and the | | |
| | | crypto engine in `plat_init()` behind a Kconfig option (`platform.md` 2.2, 4.1, 4.2) | | |
| F-248 | `src/boot/boot.c`, `src/plat/*/plat.c` | A failed security precondition only logs | Verified (code) | open |
| | | and the boot goes on: a bypassed 6UL TZASC (`tzasc_init()`), an 8X watchdog that didn't arm. Nothing | | |
| | | records them for the boot or command mode. `platform.md` 2.3: `plat_get_status()`, checked by `boot()`, no | | |
| | | silent boot; the policy is open (Jonas) | | |
| F-108 | `src/board/imx8mevk/lpddr4_timing.c`, `tests/unit/data/imx8m_ddr_init.trace` | The EVK timing's comments and 83% of its register/value pairs match NXP's 2018 `imx8mq_evk/lpddr4_timing.c` in U-Boot (GPL-2.0+, Copyright NXP), under a BSD-3-Clause header without NXP's notice, since 2019. The rest of the 2019 DDR code also followed NXP's U-Boot code; on `imx8m-dram-cleanup` the controller sequence is based on NXP's BSD-3-Clause TF-A code and the PHY driver is a clean-room implementation from `doc/dev/spec-dwc-ddrphy.md` (`imx8m-ddr.md`, "Licences and provenance"). | Verified (compared with NXP's uboot-imx 2017/2018 branches and U-Boot master) | fixed (`imx8m-dram-cleanup`): EVK timing replaced by NXP DDR Tool v4.2.0 output (BSD-3-Clause, `c129bbf9`), golden trace regenerated from it; controller sequence based on NXP's BSD-3-Clause TF-A code; clean-room PHY driver (`ee093be7`); `lpddr4.h` removed |
ed; add a timeout to `caam_wait_for_job`; consider one INIT_FINAL job when nothing was submitted |

## Dependencies / vendored code

| ID | Finding | Status |
|---|---|---|
| F-020 | Mbed TLS pinned to 3.4.0 (EOL). The file list in `src/drivers/crypto/mbedtls/makefile.mk` is tied to that release. `mbedtls_config.h` enables MD5/BASE64/PKCS1_V21, which are unused. | open |
| F-021 | Vendored bpak is ~v0.9.0, the CI CLI is v0.9.3, and there is no sync procedure. | fixed 2026-11-08: both 1.0.1, `scripts/sync-bpak` (D-024) |
| F-022 | Vendored code has no version record: Kconfiglib, libfdt, libuuid, TF-A xlat_tables_v2 + libc, NXP SCFW (littlefs was removed). | open |

## Dead code / cleanup

| ID | Finding | Status |
|---|---|---|
| F-030 | libc files never compiled: `memcpy.c`, `strncmp.c`, `strlcpy.c`, `strnlen.c`, `strrchr.c`. Also `xlat_tables_v2/aarch32/nonlpae_tables.c`. | open |
| F-031 | Stale `-I src/libc/include/...` in the armv7a/armv8a makefiles. The gcovr excludes still name `bearssl` and `src/fdt`. | open |
| F-032 | Deprecated commands still handled: `PART_TBL_INSTALL`, `PART_RESIZE`, `AUTH_SET_PASSWORD`. `pb_wire_command_string`/`pb_wire_slc_string` are stubs. | open: removing them breaks old hosts (D-008); at most answer `NOT_SUPPORTED`, never reuse the slots |
| F-033 | imx8mevk unfinished (`board_init` returns error, no Kconfig). Orphan defconfigs `bebop`, `pico8ml`. imxrt untouched since 2023. | open: ask Jonas |
| F-034 | Bash tests that exist but don't run: `test_config`, `test_invalid_key_index2`, `test_setup_lock`, `test_bpak_show`, `test_part_offset_write`. They use commands and tools that no longer exist. | removed with the bash suite |
| F-035 | The same data is defined in several places: exception names (C ×1, Python ×3, before D-021), partition flags, SLC enum, and the crc32 id table (`helpers.py`, could be `zlib.crc32`). | partly fixed (D-021): exception names are defined once (`errors.py`), and `wire.py` mirrors the wire values checked against `abi.txt`. Left: `pbtest.py`'s `PB_CMD_*`, the crc32 table |
| F-145 | De-duplication left from the 2026-10-29 slimming review (D-020): board boilerplate (default GPT table and `partitions.h` byte-identical in 5 boards, the same `board_init` sequence and 3 dev keys in all, cm `.tops` in 6), ~900 lines; the write/read/check/print block 42 times in `tools/punchboot/api_*.c` and the validate/call/exception pattern 28 times in `python_wrapper.c`, ~550; `plat_mmu_init` and `plat_get_unique_id` near-identical in imx6ul/imx8m/imx8x; the bpak hash-kind switch 4 times in `image_helpers.c`; one-provider crypto ops tables; `test_mmc_core.c`'s own fake card vs the card models. Porting API: `arch_init()` empty everywhere, single-member `console_ops`, `board_init(struct imx*_platform *)` (imx8m struct empty), cm `command`/`status` stubs in 4 boards. | open; the host tools part is gone with the C library (D-021) |
| F-146 | `src/boot/atf.c` (+ `atf-interface/`, ~1,050 lines) is never compiled: no defconfig sets `BOOT_ATF`. Its `/chosen` patching duplicates `linux.c` (with F-071). Keep for an 8MP TF-A handover, or remove. | closed: kept, the 8MP EVK boots with it (`ac10a592`), warnings fixed (`8ceeea5a`); the duplicated `/chosen` code waits for Jonas' boot review |

## Build / CI / docs

| ID | Finding | Status |
|---|---|---|
| F-111 | `scripts/pbdev` | `IMX8M_FW_DIR` (`build-firmware.sh`, `imx8m-ddr.md`) was not passed into the container, so `scripts/pbdev firmware imx8mevk` always linked the zero filled placeholder firmware. The directory must also be inside the repository, the only mount. | fixed (`imx8m-dram-cleanup`): `-e IMX8M_FW_DIR`; documented in `build-firmware.sh` |
| F-040 | `build.yml`: `actions/checkout@v1`, deprecated codecov bash uploader, Coverity steps fail without the secret (fork PRs). | fixed (D-006) |
| F-041 | Wheels are never tested (no cibuildwheel `test-command`, before D-021). There are no Python unit tests. | partly fixed (D-021): `punchboot-tools.yml` installs and runs the wheel on Linux, Windows and macOS. No Python unit tests yet |
| F-042 | `pyproject.toml` has no `[build-system]`/`[project]`; all metadata is in `setup.py`. The old author email is still in it. | fixed: PEP 621 metadata in `pyproject.toml` (setuptools backend, version from `version.txt`), `setup.py` removed, author `jonas@blxt.se` |
| F-043 | No `ruff-format` hook despite the format config. mypy only runs through pre-commit.ci. There is no `_punchboot.pyi`. | open (`_punchboot.pyi` is moot since D-021) |
| F-044 | Sphinx docs are mostly stubs, with version 1.0.0 in `conf.py`, broken header paths, and `sphinx<7`. The README test instructions are wrong (no `BOARD`). `doc/release-checklist.txt` is stale. | partly fixed: Sphinx/RTD removed (D-020); README and the checklist still open |
| F-046 | CI's `build.yml` installs neither click nor gcovr versions; it uses whatever the runner has. CI passed with old click, while PyPI users get click >= 8.2 (subgroups without a command exit with 2). `test_cli` was updated and the requirement is now `click>=8.2`. | fixed (claude-work); CI still to move to the container |
| F-045 | `tests/common.sh` hardcodes `/tmp/pb.sock`, `/tmp/disk` and `build-test/`, so tests cannot run in parallel. | fixed by the host tests (per-test working directory) |
| F-051 | Most boards don't build: imx8qxmek uses an old SCI API (`sci_seco_api.h`, `sc_pm_*`, `sc_pad_set`); imx8dxlevk needs a `keystore.bpak` that isn't in the repo (it builds with the internal keystore); imx8mevk needs proprietary DDR blobs; imxrt1060evk had an implicit `ts_print` (gone with the timestamps, 2026-10-29: it builds, with four unused-code warnings in `board.c`, and runs on the EVK). Only the QEMU board was ever built in CI. | Verified | open: CI builds jiffy + imx8dxlevk; ask Jonas which boards to keep (F-033) |
| F-052 | `src/drivers/virtio/*` has no user since `plat/qemu` was removed. | Verified | open: keep for a QEMU smoke tier, or remove |
| F-067 | Host test infra: `pbtest.py` `stop()` ignores pb-host's exit code, so sanitizer/LeakSanitizer failures at teardown are lost. The socket file appears at `bind()`, before `listen()` (startup race). No client timeouts, no pytest-timeout/ctest `TIMEOUT`. pb-host `write()` can die from SIGPIPE. Orphaned pb-host if pytest is killed. `fakes/ramdisk.c` computes `lba * 512` in 32 bits. 64-bit host hides 32-bit overflows (F-054); consider a `-m32` job. | Verified (read) | partly fixed (claude-work): teardown fails on non-zero exit and sanitizer reports; ramdisk overflow fixed; the socket path appears only after `listen()`; ctest `TIMEOUT` (60 s per unit test, 600 s for the integration suite). Client timeouts in the C library, SIGPIPE, orphans and `-m32` open |
| F-097 | The Coverity job in `build.yml` has failed on every push to master since it was added (#61): `cov-build` runs in the dev container, which has no `file` ("Cannot find '/usr/bin/file' command"). Nothing was ever uploaded. | fixed (`ci-coverity-file`): `file` is in the container; only a push to master shows whether the upload works |
| F-103 | imx8mevk is never compiled in CI (F-051: needs NXP's DDR training blobs). With placeholder blobs it compiles (`imx8m-ddr.md`, Testing), which would at least catch build breaks. | fixed (`imx8m-dram-cleanup`): CI builds it with placeholder blobs (`build-firmware.sh`, `IMX8M_FW_DIR` for the real ones) |
| F-109 | `include/pb/log_def.h`: a disabled LOG macro expands to nothing, so `else LOG_DBG(...);` is an empty body and warns (`-Wempty-body`) at `CONFIG_LOGLEVEL=1` (seen in `src/self_test.c`, since removed). Expanding to `do { } while (0)` fixes it. Verified (build). | open |
| F-147 | `Makefile` `menuconfig oldconfig:` runs `scripts/$@.py`, but `scripts/oldconfig.py` doesn't exist (there is `olddefconfig.py`, unreachable); `.PHONY` names `olddefconfig` and `keystore`, which have no rules. `README.md:208` puts `createtoken.sh` in "the tools folder" (it's `scripts/`) and passes `-sha256` (the script adds the `-`). `build.yml:75` gives `upload-artifact` a `fail_ci_if_error` left from Codecov. | open |
