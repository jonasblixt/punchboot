# Findings, closed

The findings that are fixed, rejected (`wontfix`) or closed without a fix, moved out of
`findings.md` so that it lists what is open. Same sections and columns. A finding keeps its id
for good: documents, commits and the log cite F-nnn wherever the entry is.

## Security / correctness

| ID | Where | Finding | Check | Status |
|---|---|---|---|---|
| F-001 | `src/slc.c:45` | `slc_set_eol()` checks `set_configuration_locked` but calls `set_eol`. A board without `set_eol` makes a NULL call. | Verified | fixed (work-3.0.0) |
| F-002 | `src/cm/cm_main.c:688` | The auth gate only applies when `slc == PB_SLC_CONFIGURATION_LOCKED`. In `PB_SLC_EOL` every command is open. It also compares the core `slc_t` with the wire enum (the values happen to match; `_Static_assert`s in `cm_main.c` now pin that). | Verified | wontfix (intended, D-011) |
| F-003 | `tools/punchboot/socket.c:47-80` | `read()` and `write()` loops pass the full `sz` on every iteration. `write` also re-sends the original `bfr`. A short read overruns the buffer, and EOF (0) loops forever. Affects the socket transport used by the host tests. | Verified | fixed (claude-work) |
| F-005 | `src/cm/cm_main.c:283` `cmd_auth` | Reads 1024 bytes, then passes the host-supplied `auth_cmd->size` along without bounds-checking it. | Verified: up to 65535 bytes reach `dsa_verify`/`password_auth`, past both command buffers on a 4 KiB build. A failed read was ignored too, so the stale buffer (e.g. the last good token) was checked | fixed (5370eb7): size 0 or over 1024 refused with INVALID_ARGUMENT before the first result, read errors fail the authentication; `test_cm.c` |
| F-009 | `tools/punchboot/python_wrapper.c` `log_cb` | Ignores `PyObject_CallMethod` failure and leaks the result. The GIL is never released during USB I/O. | Reported | fixed (D-021): the extension is gone; pyusb releases the GIL during transfers |
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
| F-068 | `src/lib/bpak.c` `bpak_del_meta` (vendored, same upstream) | When the last meta slot is in use, the slot isn't cleared and the final `memset` writes the 16 bytes after `meta[]`, i.e. `parts[0]`. punchboot never deletes meta. | Verified (code) | fixed upstream (bpak 1.0.0), vendored 2026-11-08 (D-024), `test_del_meta_all_slots` |
| F-069 | `src/crypto.c:33`, `src/cm/cm_main.c:538` | Positive error codes: `hash_init` returned `PB_ERR_NOT_SUPPORTED` when no provider matched; `cmd_part_verify` sets `rc = PB_ERR_CHECKSUM` (the host still gets PART_VERIFY_FAILED). | Verified (test) | fixed (claude-work) |
| F-071 | `src/boot/linux.c:173` | `fdt_delprop("pb,slc-available-keys")` returns `-FDT_ERR_NOTFOUND` if `/chosen` doesn't have the property, which fails the boot. A DTB without `/chosen` fails too, and new properties only fit if the DTB was built with free space. The jiffy, imx8dxlevk and imx8qxmek DTBs have the properties (Jonas), so no board is affected today. | Verified (code, libfdt) | fixed: `1e8f5601` (missing key list), `39276b9c` (`boot_dtb_grow()` grows the DTB by `CONFIG_BOOT_DTB_HEADROOM`, `/chosen` added). jiffy boots a DTS without the placeholders (the test bench), the 8MP EVK's upstream DTB is next (F-150) |
| F-072 | `src/cm/cm_main.c` `cmd_slc_read` | Returns the status of the last key in the key map, which becomes the final result. If the last key is revoked the host gets KEY_REVOKED, and `slc_get_*` raise `KeyRevokedError`. | Verified (test: `test_commands.py`, xfail) | fixed (`jiffy-fixes`): the result is OK, or the first error reading a key status; the xfail is gone |
| F-073 | `src/bio.c:256` `bio_set_hal_flags` | `flags << 24` on a promoted `uint8_t`: signed overflow (UB) when bit 7 is set. Found by UBSan in `test_bio.c`. | Verified (test) | fixed (claude-work) |
| F-077 | `tools/punchboot/api_partition.c`, `api_boot.c` (now `session.py`) | Wire compat, new host + old device: the host uses `caps.chunk_transfer_max_bytes` without a zero check. A device that predates the field (before 0.7, 2020) reports 0: partition write sends the bpak header and no payload and reports success, and RAM boot loops forever. Fall back to `stream_buffer_size`. Low: such devices are probably gone. | Verified (code) | fixed: falls back to `stream_buffer_size` and one buffer, `NotSupportedError` if both sizes are 0; `test_host_caps.py` |
| F-080 | `src/drivers/crypto/caam/imx_caam.c:158` `caam_wait_for_job` | On the closed jiffy, any JTAG connection (even OpenOCD `init` alone, no halt) sets SNVS SV1 (HPSVSR 0x80000002) and the SSM drops from Trusted to Non-Secure (HPSR 0x8000bb00). After that the CAAM, left in trusted mode by HAB (CSTA MOO 3), never fetches new jobs (IRSAR 0, ORSFR 0). pb spins in `caam_wait_for_job` (no timeout, no watchdog kick) on the next hash, e.g. in `auth_token`, until the watchdog resets the board. The host sees `Error: Transfer`. Confirmed: power cycle, `auth` works, OpenOCD `init`, `auth` hangs. pb started over JTAG (not HAB booted, CSTA MOO 0) doesn't hang. The hardware behaviour is expected on a closed part. The bug is that pb hangs instead of failing. | Confirmed on the board (2026-10-22) | fixed (`jtag-debugging`): `caam_wait_for_job` times out after 1 s with `-PB_ERR_TIMEOUT`, and `caam_hash_final` checks the INIT job. Checked on the board: after a JTAG attach `auth token` fails with `Error: Timeout` and pb keeps running. The CAAM stays unusable until a power cycle (expected) |
| F-082 | `src/drivers/mmc/mmc_core.c` `mmc_part_switch` | `mmc_part_switch` wrote the whole PARTITION_CONFIG: accessing BOOT0 enabled BOOT1 for boot and the other way round, and the user area and RPMB enabled BOOT0. After `part write` to BOOT0, a `dev reset` boots BOOT1, i.e. the other (older) bootloader. Seen twice on jiffy (2026-10-22). Worse: after a power loss in the middle of a BOOT0 write, the ROM boots BOOT1 once, pb reads the GPT and enables the half-written BOOT0, and the next boot ends in the serial downloader (the ROM has no fallback, RM 8.5.3.2). A read of BOOT0 also enabled BOOT1, and pb undid a boot partition selected from Linux. | Seen on the board | fixed (`jiffy-fixes`): `mmc_part_switch` only changes PARTITION_ACCESS; a write to the enabled boot partition first enables the other one, and a STREAM_FINALIZE after error-free writes enables the written one (`bio_write_done()`). An interrupted write boots the other partition until a write completes (`test_mmc_core.c`). Holds once both partitions run the fix: from an older pb, write BOOT1 first. Checked on jiffy: half a BOOT0 write without STREAM_FINALIZE, then two power cycles both boot BOOT1, and the next full BOOT0 write boots BOOT0 |
| F-084 | `src/drivers/timer/imx_gpt.c` `imx_gpt_init`, `src/plat/imx6ul/plat.c` | `plat_get_us_tick()` runs 2.64x fast on jiffy, so every `pb_delay_*`/`pb_timeout_*` is 2.64x short (the 1 s CAAM timeout is ~0.38 s). The driver selects CLKSRC 010 (`ipg_clk_highfreq` = PERCLK_CLK_ROOT, GPR5 bit 28 = 0) and writes `PR = input_clock_Hz / MHz(1)`, but the counter divides by PR + 1 (RM §28.6.2), and perclk isn't 24 MHz: pb never sets `CSCMR1[PERCLK_CLK_SEL]`, and the ROM leaves it on IPG = PLL2 PFD2 396 MHz / AHB 3 / IPG 2 = 66 MHz. 66 / 25 = 2.64 MHz. The same driver is used by imx8x, imx8m and imxrt (their perclk not checked; the PR off-by-one applies to all). | Verified (JTAG, 2026-10-23): CSCMR1 `0x04900000`, CBCDR `0x00018900`, CBCMR `0x24a69324`, PFD_528 `0x5058505b`, GPR5 0, GPT1 CR `0x681`, PR `0x18`; GPT1_CNT advanced 26518521 in 10.044 s of host time (2.6401 ticks/us) | fixed (fc337d5 PR + 1, all platforms; 002a08d imx6ul runs from the 24 MHz crystal via `imx_gpt_init_24m()`; e93a767 imxrt passes perclk). Checked on jiffy after the fix (flashed, 2026-10-23): GPT1 CR `0x743`, PR `0x7002`, 10046344 ticks in 10.046 s of host time (1.0001 ticks/us). imx8x and imx8m keep perclk and now get 1.00 MHz instead of 0.96 MHz if their perclk is 24 MHz (not checked) |
| F-085 | `src/cm/cm_main.c` `PB_CMD_BOOT_PART` | Since ac58e89 (2024-09) the handler calls `tops.disconnect()` before checking `boot_load`'s result. When the load fails, `cm_run` logs `Command error` and waits for the next command on a stopped USB controller, so the device drops off the bus until a power cycle. Seen on jiffy: `boot partition` of an image that fails to verify; over JTAG pb loops in `cm_run`/`imx_ci_udc_xfer_complete` with USBCMD 0x00080000 (RS = 0). | Confirmed on the board (2026-10-22) | fixed (`jiffy-fixes`): disconnect only before `boot_jump`; `test_boot_part` sends a command after a failed boot. Checked on jiffy: the device stays on USB and `boot status` works after the failed boot |
| F-086 | `src/drivers/crypto/caam/imx_caam.c` `imx_caam_init` | The CAAM registers SHA-384/512 whatever the MDHA is. The i.MX6UL has the LP256 MDHA (CHAVID_LS 0x10040103, MD VID 0), which only does up to SHA-256. A SHA-512 bpak (sys_a on jiffy) fails the header hash with JR status 0x20000541 (CCB, MDHA, mode error, descriptor index 5, the ST of the INIT job; descriptor read over JTAG) instead of `NOT_SUPPORTED`. jiffy has no software hash, so such images can't boot there. | Confirmed on the board (2026-10-22) | fixed (`jiffy-fixes`): SHA-384/512 dropped when CCBVID era is 1..9 and the MD VID is LP256, other CAAMs unchanged. Checked on jiffy: the SHA-512 sys_a image fails with NOT_SUPPORTED (-19), no CAAM job error |
| F-088 | `src/boot/image_helpers.c` `boot_image_copy_and_hash` | Ignores `source_size` and `destination_size`, so a part past the end of either buffer is copied anyway, out of bounds. Latent: only the unit tests call it. Found through the `-Wunused-parameter` warning (F-049). | Verified (code) | fixed (`jiffy-fixes`): a part that doesn't fit returns `-PB_ERR_BUF_TOO_SMALL` before it's copied (`test_image_helpers.c`) |
| F-094 | `tools/punchboot/api_authentication.c` `internal_pb_api_authenticate` | The host copied the auth data into a 1024 byte stack buffer without a size check, and the Python wrapper passes any length: a token over 1024 bytes overflowed the host's stack (64 KiB: SIGSEGV), and the size went on the wire as a truncated uint16. A size of 0 sent no data and reported the device's first OK as success, while the device waited for the data. Same pattern as F-005, host side. | Verified (test, fake device) | fixed (bb1a1f6): size 0 or over 1024 is INVALID_ARGUMENT before anything is sent; `test_host_auth.py` |
| F-096 | `src/plat/imx6ul/plat.c`, `src/board/jiffy/imximage.cfg` | The TZASC (TZC-380) was bypassed on the i.MX6UL: TZASC1_BYP in IOMUXC_GPR9 is cleared by a power-on reset and nothing set it, so no DDR access was checked. OP-TEE's TZASC regions had no effect: on jiffy Linux read OP-TEE's memory at 0xbe000000 with devmem. Out of bypass, OP-TEE takes a data abort reading the DTB through its non-secure mapping before its TZASC driver has run, because region 0 only lets secure accesses through. | Seen on the board (2026-10-26) | fixed (`imx6ul-tzasc`): new TZC-380 driver (`src/drivers/tzc`, selected by `PLAT_IMX6UL`); `plat_init()` opens the DDR window to both worlds in region 1, which OP-TEE replaces, and logs an error when the DCD left the TZASC bypassed (not fatal, command mode needs `plat_board_init()`). The jiffy DCD sets the bit after the MMDC setup. A warm reset keeps the bit, so the check only catches a missing DCD line after a power-on reset. Needs OP-TEE with `CFG_TZC380`, or DDR stays open in region 1 |
| F-098 | `src/plat/imx8m/umctl2_lp4.c` `umctl2_init` (now `ddr.c`, `drivers/ddr/dwc_ddrphy.c`) | The LPDDR4 init ignored the firmware readback check (which also returned a positive `PB_ERR`, F-069) and the PHY firmware's "training failed" mail, carried on with untrained DRAM and returned `PB_OK`; `plat_init()` ignored the result anyway. | Verified (host model, `test_imx8m_ddr`) | fixed (`imx8m-dram-cleanup`): `-PB_ERR_IO`, `plat_init()` fails |
| F-099 | `include/plat/imx8m/clock.h` | `FRAC_PLL_LOCK_MASK` and `SSCG_PLL_LOCK_MASK` were `(1 << 31)`, signed overflow (UB). Found by UBSan in `test_imx8m_ddr`. | Verified | fixed (`imx8m-dram-cleanup`) |
| F-101 | `src/board/imx8mevk/lpddr4_timing.c` `umctl2_ddrc_cfg` | The last four entries write `FREQ1_DERATEEN/DERATEINT/RFSHCTL0/RFSHTMG` again with other values, overwriting the 667 MT/s settings a few lines up, while the 3200 MT/s `DERATEEN`, `DERATEINT` and `RFSHCTL0` are never written (reset values). Looks like FREQ0 values pasted onto FREQ1 names. Also `DBG1`, `SWCTL`, `PCTRL_0` in the table are written again by the code. | Verified (read); hardware effect unknown | fixed (`imx8m-dram-cleanup`, `c129bbf9`): the EVK timing is now NXP DDR tool output, which sets the FREQ0 registers and writes FREQ1 once |
| F-105 | `src/plat/imx8m/ddr.c`, `ddr_imx8mq.c` | The DDR init never sets the CCM `DRAM_APB_CLK_ROOT` and `DRAM_SEL_CFG`. NXP's current code (U-Boot master) sets the DRAM APB clock to sys PLL1 / 4 before configuring the controller, and after every DRAM PLL change selects the PLL as DRAM clock (`DRAM_SEL_CFG` source 0) and the APB clock to / 5. Punchboot relies on the reset/ROM state. | Verified (differential trace against U-Boot, 2026-10-28) | fixed (`imx8m-dram-cleanup`, `410f767c`): `ddr_imx8mq.c` sets DRAM_APB_CLK_ROOT and DRAM_SEL_CFG as NXP's DDR tool does, and DRAM_ALT_CLK_ROOT for PHY bypass |
| F-112 | `src/drivers/usb/dwc3_udc.c` | The DWC3 device driver had never run (imx8mevk isn't in the lab) and didn't follow the RM's device programming model: every EP0 OUT TRB was Control-Setup (also the status stage), data stages were Normal TRBs, status stages started without waiting for XferNotReady(Status), a USB reset left the control transfer hanging (no timeouts: the watchdog reset the 8MP EVK during enumeration), EP0/1 used FIFO 1 and 64 bytes before connect done, connect done didn't reconfigure EP0, DCFG.DEVSPD was never set, End Transfer had no transfer resource index (never saved) nor ForceRM/CmdIOC, the FIFO sizes were overwritten with magic values, IN endpoints used the physical number as TX FIFO, field macros didn't parenthesize their argument, TRBs shared cache lines. | Verified (RM 11.2.2.5, board) | fixed (`imx8m-dram-cleanup`): rewritten from RM 11.2.2.5/11.2.2.7, named fields in `dwc3_udc_private.h`. On the 8MP EVK: enumeration, `dev show`, `part list`, 4 MiB writes with device-side verify. Not exercised yet: a USB reset during a bulk transfer, EP0 stall, full speed hosts |
| F-114 | `src/plat/imx8m/plat.c` | The OCOTP clock gate (CCGR34) was never opened: on the i.MX8M Plus EVK the unique ID read as 0, so the device UUID was all zeros and the USB serial garbage. | Verified (board) | fixed (`imx8m-dram-cleanup`): opened before `imx_ocotp_init()` (reading only) |
| F-115 | `src/board/imx8mpevk/board.c` `cm_board_init` | USB1 relies on the ROM having powered HSIOMIX and the USB1 PHY (GPC_PU_PGC_SW_PUP_REQ 0x303A00D8 bits 17 and 2) and set the PHY reference clock, which it does after a serial download boot. Booting from eMMC it doesn't. | Verified (RM), seen on the board (2026-10-29) | fixed: `usb1_power_up()`: the HSIO and USB clocks and reference clock roots (CCM slices 98/99), GPC A53 mapping and power-up of HSIOMIX and the USB1 PHY, the NoC handshake (acked only once HSIO BLK_CTRL USB_CLOCK_MODULE_EN is set), PHY_CTRL0 FSEL 24 MHz, PHY_CTRL6 bits 1:0 cleared (reset 0b11, the alternate PHY clock; without it the first DWC3 endpoint command times out). The EVK boots pb from eMMC with USB command mode |
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
| F-129 | `src/link.lds`, `src/link_flash.lds` | `.self_test_fn` is collected without `KEEP()`, so `--gc-sections` drops every entry: with `CONFIG_SELF_TEST=y` (jiffy) pb prints "All 0 tests passed". The four SHA-256 known-answer tests in `crypto.c` never run. | Seen on the board (2026-10-29), `.self_test_fn` is 0x20 bytes in `crypto.o`, `_self_test_fn_start == _self_test_fn_end` in `pb` | wontfix (self tests removed, D-019) |
| F-130 | `src/drivers/usb/imx_ci_udc.c`, `src/cm/cm_main.c` `cm_write` | Intermittently (alternating `punchboot log` and `auth`: within 6-18 iterations, once not in 120) a bulk IN reply on EP1 never completes: the dTD stays Active while the host waits for the data. `cm_write` polls `xfer_complete` without a timeout or a watchdog kick, so the watchdog resets jiffy after ~5 s and the host reports `Error: Transfer`. Seen as the glitch during the boot time measurements. Cause unknown; suspect the cached dQH: `xfer_start` writes `qh->next` into a possibly stale cache line and cleans the whole 64-byte line, writing old overlay fields back over the controller's. The prime/flush/completion loops have no timeouts. | Seen on the board (2026-10-29), located with breadcrumbs | fixed (`3746bc6c`): usbmon showed the host's 1024 byte OUT stage end after 512 bytes, the dump an EP2 overlay of the previous 512 byte command transfer into `align_buffer`: `xfer_start` wrote `qh->next` into a cached dQH line filled while the endpoint was primed and its clean wrote the old active overlay back, which the controller resumed. Now the line is invalidated first and the overlay token cleared (RM 54.4.6.6.3). 0 hangs in 1000 `log`+`auth` and 1000 `log`+board command rounds (8-11 in 500 before). F-132/F-133 and SDIS were not it |
| F-132 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_set_configuration` | An IN endpoint is enabled with bit 6 (RXR) set instead of bit 22 (TXR), so the IN data toggle is never reset on SET_CONFIGURATION. 6UL RM ENDPTCTRL1: "Whenever a configuration event is received for this Endpoint, software must write a one to this bit". | Verified (code, RM 54.6.40) | fixed (`40f82aac`), not yet run on the board |
| F-133 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_set_configuration` | ENDPTCTRLn is written whole for one direction, leaving the other as type 00 (control): EP1 OUT and EP2 IN on pb's class. 6UL RM ENDPTCTRL1 note: "the unused direction type must be changed from the default control-type to any other type… leaving an unconfigured endpoint control causes undefined behavior for the data pid tracking on the active endpoint/direction". The write also clobbers the other direction if both are used (the RM asks for read-modify-write). A candidate for F-130. | Verified (code, RM 54.6.40, 54.4.6.3.2) | fixed (`afaa9c87`): read-modify-write, an unused direction is bulk; not yet run on the board |
| F-141 | `tools/punchboot/usb.c` `pb_usb_read`, `pb_usb_write` | The actual length (`rx_sz`) of a bulk transfer is never checked, so a short read or write counts as complete and the host and pb can get out of step without an error. | Verified (code) | fixed (D-021): `UsbTransport` raises `TransferError` on a short transfer |
| F-150 | `src/board/imx8mpevk/board.c` | The board booted with the Linux boot driver, which jumps to `kernel` at EL3; arm64 Linux needs TF-A's BL31. TF-A v2.12's 8MP BL31 takes BL33 from the `bl_params` in x0 only from OCRAM/TCM and swaps base and size in that check (below 0x960000 then), else BL31 jumps to 0x40200000 without arguments. | Code (TF-A v2.12 source) | fixed: `ac10a592` (ATF boot driver), `cb1092bb` (`bl_params` in `.data`, 0x936168), `f4f01c5b` (the system counter: BL31 hung in `udelay()` without it), `875ccea8` (no SLC), F-071. Boots to the login prompt on the EVK |
| F-151 | jiffy on the bench (2026-10-29) | Linux froze a few seconds into the boot (no kernel message) and jiffy power-on-reset about 2.5 s later (`POR (1)`), with pb 3.0.0 and v2.7.1, the test bench and the old OP-TEE image, with or without kickstart, network or pbstate. Cause: jiffy's USB (command mode) was on the VIA VL813 hub. Plugged into the host directly, the test bench boots to the login prompt (dm-verity root, `pb,active-system` A, verified). Why the hub resets jiffy isn't known (port power switching after pb's USB disconnect?). Bisecting software ruled out: cpuidle, CAAM, usbotg1, watchdog, network, pbstate, kickstart. Userspace `devmem` aborts on every peripheral (AIPS-TZ defaults, privileged only), unrelated. | Verified (board) | closed: bench, not software |
| F-154 | `src/drivers/crypto/caam/imx_caam.c`, i.MX8M | pb never instantiated the CAAM's RNG4. On jiffy Linux runs secure and did it itself; on the 8MP Linux runs non-secure behind TF-A and can't (`caam: failed to acquire DECO 0`, `failed to instantiate RNG`), so there was no `rng-caam` and the entropy pool filled slowly. | Verified (EVK) | fixed 2026-10-30: `imx_caam_rng_init()` instantiates both state handles with prediction resistance (Linux tears down a handle without, which needs DECO 0) and generates the secure keys; jiffy, imx8mevk, imx8mpevk call it. Both boards: `rng-caam`, RDSTA 0x40000033, crng ready at 0.15 s (8MP) / 0.27 s (jiffy). 27 ms per boot on both; every reset kind clears the CAAM. imx8mevk not tried |
| F-159 | `tools/punchboot/transport.py` `_usb_serial`, pb 2.0.0 on the i.MX8QXP MEK | pb 2.0.0's cdns3 driver overflows the USB serial number string descriptor (`Errno 75`, `lsusb` shows it empty). The host skips devices whose serial it can't read, so `punchboot list` doesn't show the board and `-u` can't select it; without `-u` the first device is used, which works only when it is the only one. New host + old device (D-008). | Seen on the board (2026-11-08) | fixed (work-3.0.0-plat): EP0 is 64 byte packets with ERDY and ISP as Linux (`bee9508b`); the host asks a device without a readable serial for its identifier (`84cf2137`). Was open. Cause, pb 3.0.0 too: `cdns3_udc_core.c` configures EP0 with 512 byte packets (SuperSpeed) at high speed, so the 74 byte string goes out as one packet (babble). With 64 (Linux's value, the descriptor's `bMaxPacketSize0`) every transfer up to 64 bytes works but a longer one never completes, also with EP0 configured again on connect and reset and with a TRB burst length (tried on the MEK, the board then hangs in enumeration). Effects: the host takes ~22 s to enumerate the MEK after a reset (the kernel retries the string), and a CLI run that reads the serial number of every device (`punchboot list`) once ended in a watchdog reset of the MEK. Host side: list such devices by `PB_CMD_DEVICE_IDENTIFIER_READ` |
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
| F-208 | `src/plat/imx8m/plat.c` | The 8M never enabled its TZASC while | Verified (on the 8MP EVK) | fixed (8M) |
| | | `boot/atf.c` supports a BL32: the normal world could read secure DRAM (the 8M's F-096). TF-A doesn't enable | | |
| | | it: `bl31_tzc380_setup()` returns when GPR10's TZASC_EN is clear. Fixed: GPR10's TZASC_EN, ID_SWAP_BYPASS | | |
| | | and both locks before `imx8m_ddr_init()`, the DDR window in region 1, region 0 left secure. On the EVK: 16 | | |
| | | regions, 33 bit addresses; an NS read of a region made secure only is refused (FAIL_ADDRESS 0x20000000 for | | |
| | | PA 0x60000000: addresses from the DDR base), and passes again without it. GPR10 alone is enough, no | | |
| | | TZASC_ENABLE fuse (8MP RM 4.9.1.2 asks for one; none written). Linux boots through BL31 | | |
| F-233 | `plat/imx8x/plat.c` | `imx8x_wdog_init()`: without `CONFIG_ENABLE_WATCHDOG` | Verified (compiler) | fixed |
| | | the function had no `return` (gcc: -Wreturn-type): `wdog_rc` was undefined, `plat_init()` could log a | | |
| | | failed watchdog. Fixed: `SC_ERR_NONE` after the `#ifdef`. The DXL builds without the option and without the | | |
| | | warning; with it (every 8X defconfig) the code does the same as before | | |
| F-234 | `drivers/wdog/imx_wdog.c` | Also `src/plat/{imx6ul,imx8m,imxrt}/plat.c`: | Verified (on jiffy) | fixed |
| | | `base` was 0 until `imx_wdog_init()`, so `plat_reset()` (`imx_wdog_reset_now()`) and `plat_wdog_kick()` | | |
| | | wrote to address 0 without `CONFIG_ENABLE_WATCHDOG`, and `plat_reset()` from an exception before | | |
| | | `plat_early_init()` didn't reset. Fixed: the driver keeps no state, each call takes the platform's WDOG1 | | |
| | | base. jiffy built without the option resets on `dev reset`; jiffy, the 8MP EVK and the RT1060 EVK reset and | | |
| | | kick with it | | |
| F-243 | `image_helpers.c` | `load_and_hash()`: The boot path never kicks the watchdog, | Verified (code) | fixed |
| | | only command mode does: loading and verifying must fit in `CONFIG_WATCHDOG_TIMEOUT` (5 s) from arming. | | |
| | | `platform.md` 3: kick once per chunk and before the jump, never in `pb_delay`/`pb_timeout` | | |
| | | Fixed: `boot_image_load_and_hash()` kicks once per chunk, for a partition, the host's stream and in place. | | |
| | | After the chunk, when it is read and its hash is started: the kick says that the load made progress, a | | |
| | | chunk that failed gets none, and `platform.md` 3.3 has the final hash, prepare and `late_boot()` as what | | |
| | | follows the last chunk without a kick. A host's stream is read to its end after a hash error (F-061), and | | |
| | | each of its chunks is kicked for. `boot_jump()` kicks once, after `late_boot()` and `plat_lockdown()` and | | |
| | | before the delivery: the next stage gets the whole timeout, and a boot that one of the two stops has no | | |
| | | kick. Nothing else on the boot path: 3.3 names no kick between candidates or around the signature (F-320). | | |
| | | The load of `boot part` and `boot bpak` kicks per chunk with it (F-189 keeps `part verify`). Cost: per | | |
| | | chunk two register writes on the i.MX WDOG, a call to the SCU on the 8X (F-322): one per MiB on jiffy and | | |
| | | the 8MP EVK, per 4 MiB on the 8X boards and imx8mevk, per 4 KiB on the RT1060. By the measured rates a | | |
| | | chunk takes 13 ms on jiffy, 12 ms on the 8MP EVK, 23 ms on the MEK and under 1 ms on the RT1060 (the | | |
| | | README, `testbench/README.md`, `testbench/simple/README.md`), against 5 s (the RT: 30 s); a read that | | |
| | | stalls is F-321. `test_image_helpers.c`: the count per chunk for each source, the kick's place after the | | |
| | | chunk, the count up to an error. `test_boot.c`: the kick's place before both jumps. Every board builds; | | |
| | | not run on a board | | |
| F-244 | `plat/imx6ul/pb.csf.template` | On a closed 6UL nothing locked SRK | Verified (on jiffy) | fixed (6UL) |
| | | revocation before the jump: the CSF unlocks SRKREVOKE so that pb can revoke keys, and pb never set | | |
| | | OCOTP_SW_STICKY's SRK_REVOKE_LOCK (6UL RM 35.5.6). On jiffy the SRK_REVOKE shadow took writes while Linux | | |
| | | could run. Fixed: `plat_lockdown()` before the jump, `imx_ocotp_lock()`: SW_STICKY 0x1e after it and the | | |
| | | shadow write refused (HAB had locked field return and SCS already). A watchdog reset clears the locks, | | |
| | | revocation still works after a reboot. Open: the 8X (SECO) and RT, unchecked; the 8M once it has HAB | | |
| | | (`plat-imx.md` 4.3) | | |
| F-245 | `drivers/wdog/imx_wdog.c` | Each WDOG's power-down counter runs out | Verified (jiffy, 8MP, RT1060) | fixed |
| | | of every reset and drives WDOG_B low after 16 s unless WMCR.PDE is cleared (6UL RM 57.5.3, RT RM 57.6.3). | | |
| | | pb cleared it for WDOG1 only, only with `CONFIG_ENABLE_WATCHDOG` (`imx_wdog_init()`); U-Boot clears WDOG1-3 | | |
| | | on the 6UL and 8M. A board with a WDOG_B on a pad to the PMIC resets after 16 s. Fixed: | | |
| | | `imx_wdog_disable_powerdown()` for each WDOG in `plat_early_init()`. Out of the ROM PDE is set on WDOG1-3 | | |
| | | on jiffy, on WDOG2 and 3 on the 8MP EVK, on WDOG1 and 2 on the RT1060 EVK; 0 after. The RT's RTWDOG (WDOG3, | | |
| | | enabled out of reset) comes out of the ROM disabled (CS 0x2520). The 8X's watchdogs are the SCFW's | | |
| F-108 | `src/board/imx8mevk/lpddr4_timing.c`, `tests/unit/data/imx8m_ddr_init.trace` | The EVK timing's comments and 83% of its register/value pairs match NXP's 2018 `imx8mq_evk/lpddr4_timing.c` in U-Boot (GPL-2.0+, Copyright NXP), under a BSD-3-Clause header without NXP's notice, since 2019. The rest of the 2019 DDR code also followed NXP's U-Boot code; on `imx8m-dram-cleanup` the controller sequence is based on NXP's BSD-3-Clause TF-A code and the PHY driver is a clean-room implementation from `doc/ports/spec-dwc-ddrphy.md` (`imx8m-ddr.md`, "Licences and provenance"). | Verified (compared with NXP's uboot-imx 2017/2018 branches and U-Boot master) | fixed (`imx8m-dram-cleanup`): EVK timing replaced by NXP DDR Tool v4.2.0 output (BSD-3-Clause, `c129bbf9`), golden trace regenerated from it; controller sequence based on NXP's BSD-3-Clause TF-A code; clean-room PHY driver (`ee093be7`); `lpddr4.h` removed |
| F-070 | `src/boot/boot.c` | BIO and CB boots load into `header`, but `prepare`/`late_boot` get `header_ptr`, which an IN_MEM or CUSTOM boot repoints. After a failed in-memory boot, a command-mode boot would prepare (jump address, dtb) from the stale, unverified header. Latent: no board uses IN_MEM/CUSTOM. | Verified (code) | fixed with F-253: `header_ptr` is gone with the two sources, every boot verifies and prepares from `header` |
| F-219 | `src/boot/simple.c`, `boot.c`, `board.c` | The XIP / `BOOT_SOURCE_IN_MEM` path can't be | Reported | fixed |
| | | reached (nothing sets `get_in_mem_image`), and if wired up it hashes memory at `pb-load-addr` but jumps | | |
| | | to `image_base_address + offset`. Extends F-070. `boot.md` 4.1 (D-038, D-040): a part is at its | | |
| | | `pb-load-addr`, hashed and entered there, inside the candidate's mapped range; the header where | | |
| | | the format has it in the partition (D-048) | | |
| | | Fixed: `image_base_address` and `image_size` are gone, the entry is the part's `pb-load-addr` with every | | |
| | | source (`test_simple.c`). The in place source itself is F-253. Not run on a board | | |
| F-253 | `src/board.c`, `include/pb/board.h` | D-026 lost two boot sources: | Verified (code) | fixed |
| | | `setup_boot_driver()` sets `default_boot_source = BOOT_SOURCE_BIO`, and `struct board` has no field for | | |
| | | `get_in_mem_image` or `authenticate_image`. On master the boards built `struct boot_driver` themselves | | |
| | | and could boot in place (`IN_MEM`) or authenticate themselves (`CUSTOM`). Out-of-tree projects use | | |
| | | both (Jonas). Before `work-3.0.0-plat` merges: give them a way back (the boot contract decides how) | | |
| | | D-035 (Jonas): the custom source is removed, nobody uses it. Only the in place source comes | | |
| | | back; what is left of `CUSTOM` in `boot.c` goes with it. D-048, `board.md` 2.4: a flag in the | | |
| | | static partition table's entry, the device's mapped base in `PTBL_STATIC_MAPPED()`, the header | | |
| | | where the format has it; `get_in_mem_image()` doesn't come back | | |
| | | Fixed: `STATIC_PART_FLAG_IN_PLACE` in the static table's entry and `static_ptbl_init_mapped()` for the | | |
| | | device's base (the board calls it until F-265 brings `PTBL_STATIC_MAPPED`). `boot.c` asks the table for | | |
| | | the candidate's mapped range, copies the header from its last 4 KiB, checks that every part lies inside | | |
| | | the range (`boot_image_verify_parts_in_place()`) and hashes the parts where they are. The range is the | | |
| | | table's, also for a device that carries the partition's UUID. `BOOT_SOURCE_IN_MEM`, | | |
| | | `BOOT_SOURCE_CUSTOM`, `get_in_mem_image`, `authenticate_image` and `header_ptr` are gone (F-070). | | |
| | | `test_static_ptbl.c`, `test_image_helpers.c`, `test_boot.c`, and end to end on the host's flash (`test_in_place.py`). From its review: the cache's copy of the partition is dropped before the header is read, and a mapped table isn't replaced while a partition of it is booted in place. Not run on a board. Where the arch has an | | |
| | | MMU the range is mapped by the board's `mmap`, F-269 | | |
| F-264 | `src/board/imxrt1060evk/board.c` | `board_late_boot()` (the charger detection, | Verified (code) | fixed |
| | | the SBMR2 serial download check, "no pbl in the flash") and `board_get_in_mem_image()` are defined | | |
| | | and never called: `struct board` has no `.late_boot`, and on master the board registered no boot | | |
| | | driver either (dead since the board was added, 1275d8ec). `board.md` 5.3 uses the charger detection | | |
| | | as its example for `late_boot()`. Ask Jonas: wire `late_boot` for the charger check, or remove | | |
| | | the two functions? The serial download check goes with F-251, the header lookup with F-253 | | |
| | | Jonas (2026-11-12): wired, for the charger check. Fixed: `.late_boot` is set and the hook keeps the | | |
| | | charger detection only. The SBMR2 and the blank IVT checks are removed, F-251 has them; | | |
| | | `board_get_in_mem_image()` went with F-253. Not run on a board: the hook reads the detector's status and | | |
| | | relies on the detector running from reset (nothing in pb writes its control register), and on a bench | | |
| | | port that reports as a charger every boot that command mode didn't start would stop | | |
| F-289 | `src/drivers/partition/static.c` | `static_ptbl_init()` kept `bio_block_size()`'s error code as a block size: an invalid device gave `-PB_ERR_ALIGN` for the first entry and `PB_OK` for an empty table. Found by the tests for F-253 | Verified (test) | fixed: the error is returned; `test_static_ptbl.c` |
| F-290 | `src/drivers/mmc/mmc_sd.c`, `mmc_core.c` | `dev->rca << RCA_SHIFT_OFFSET` shifts a `uint16_t` as an `int`: an SD card's RCA of 0x8000 and above overflows it (undefined; UBSan: "left shift of 43690 by 16 places"). `unit.test_mmc_sd` and `unit.test_imx_usdhc` failed with `-DPB_SANITIZE=ON`, so the branch wasn't "ASan/UBSan clean" as `status.md` said. An e.MMC's RCA is pb's own, a small number | Verified (UBSan) | fixed: shifted as `uint32_t` where a card's RCA is used (`mmc_sd.c`, `mmc_send_status()`); the suite is clean with sanitizers again |
| F-255 | `src/boot/dtb.c` `set_initrd()` | `linux,initrd-start` and `-end` are written | Verified (code) | fixed (work-3.0.0-mega) |
| | | with `fdt_setprop_u32()`: a ramdisk above 4 GiB gets a truncated address. Use the root's | | |
| | | `#address-cells`. The encoding (D-038, `boot.md` 5.2): 32 bits where both fit, as today, 64 | | |
| | | bits only above 4 GiB. Fixed: `set_initrd()` computes start and end in 64 bits and writes one | | |
| | | cell each when both fit, two cells each when one doesn't (an end at 4 GiB too, which was | | |
| | | written as 0). `test_linux.c`: `test_initrd_32_bits` pins the released bytes, | | |
| | | `test_initrd_end_at_4_gib`, `test_initrd_above_4_gib`. On a 32-bit arch the load address is | | |
| | | narrowed earlier, in `linux.c` (`uintptr_t`): refusing it is F-254's | | |
| F-265 | `include/pb/board.h`, `src/board.c`, the boards | `struct board` has a member | Verified (code) | fixed |
| | | per variant (`boot_linux`, `boot_atf`, `boot_simple`; `ab`; three for a GPT, and the RT's static | | |
| | | table is code in `init()`), and which is set must agree with Kconfig. D-046, `board.md` 1: one | | |
| | | member per concept (`ptbl`, `boot_policy`, `boot_handover`, `boot_delivery`), filled with the | | |
| | | module's object (its functions and the board's configuration, made by its macro). `src/board.c` | | |
| | | calls through the object and includes no module's header. With F-256, F-258, F-259 | | |
| | | Fixed: `ptbl`, `boot_policy`, `boot_handover` and `boot_delivery` are objects by value, a module's | | |
| | | functions and a pointer to the board's configuration, made by `PTBL_GPT()`, `PTBL_STATIC()`, | | |
| | | `PTBL_STATIC_MAPPED()`, `BOOT_POLICY_AB()`, `BOOT_HANDOVER_IMAGE()` and, until F-258 and F-259, | | |
| | | `BOOT_HANDOVER_ATF()` and `BOOT_HANDOVER_SIMPLE()`. `src/board.c` includes no module's header and has no | | |
| | | `CONFIG_BOOT_*`. The modules behind the objects are unchanged: the policy's functions are today's three | | |
| | | callbacks (F-256), a delivery is refused (F-258). The RT's table is `ptbl`, set up after `init()`; the | | |
| | | device a table is on is left visible only with every kind of table (`bio.md` 2.4), so the RT's NOR | | |
| | | device itself is no longer writable by a host, its partitions are. `test_board.c`, a case through each | | |
| | | object in the modules' tests (but `BOOT_HANDOVER_ATF()`: the host doesn't build `atf.c`). From its | | |
| | | review: a wrong configuration type, NULL, or a pointer where `PTBL_GPT()` takes the array doesn't | | |
| | | compile. Every board builds; not run on a board | | |
| F-266 | `include/pb/board.h`, `src/boot/boot.c`, the boards | `late_boot()` gets | Verified (code) | fixed |
| | | `struct bpak_header *header`, not `const`, which ties the hook to one image format. D-047, | | |
| | | `board.md` 5.3: the argument goes; `boot_image_get_bpak_header()` returns the verified | | |
| | | header as `const`, NULL before the payload's hash is checked. The RT's and the host's hooks | | |
| | | ignore the argument today | | |
| | | Fixed: `late_boot` (`struct board`) and `late_boot_cb` (`struct boot_driver`) take `boot_part_uu` | | |
| | | alone. `boot_image_get_bpak_header()` is declared in `boot/image_helpers.h`, with the format's | | |
| | | other functions, and implemented in `boot.c` over its copy of the header: NULL until | | |
| | | `boot_image_verify_payload()` returned `PB_OK` for the candidate (a partition that is loaded, | | |
| | | in place, the host's stream), and NULL again when the candidate fails in prepare, when another | | |
| | | candidate's header is read, when a boot starts, and when a boot is stopped or fails after | | |
| | | prepare (`late_boot()`, the lockdown, a jump that comes back): "valid from prepare to the | | |
| | | jump". The RT's and the host's hooks lose the argument; neither used it. `test_boot.c`: the | | |
| | | fakes ask for the header at every step of a boot, and every case ends with "nothing unverified | | |
| | | was handed out"; `test_board.c` has the hook's type. What 5.3 doesn't say: F-325. What still | | |
| | | names bpak for nothing: F-326. Every board builds; not run on a board | | |
| F-293 | `src/boot/linux.c`, `atf.c`, `ab_state.c` | `boot_driver_linux_init()`, `boot_driver_atf_init()` and `boot_ab_state_init()` dereferenced a NULL configuration. No board passes one; found by the test of the handover's object (F-265) | Verified (code) | fixed: `-PB_ERR_PARAM`, as the generic handover; `test_linux.c`, `test_ab_state.c` (`atf.c` isn't built on the host) |
| F-288 | `tests/doc/check_docs.py` | The document lint checks the entries of | Verified (code) | fixed |
| | | `platform.md`, `board.md` and `boot.md` against their headers and the other way round, not | | |
| | | the drivers' contracts'. `bio.md` against `include/pb/bio.h`, `crypto.md` against | | |
| | | `include/pb/crypto.h`, `usb.md`'s operations against `struct usbd_hal_ops` and its | | |
| | | transport's functions against `include/pb/cm.h`. D-073, `process.md` 7 | | |
| | | Fixed: `check_drivers()`, each contract in its own form, both ways. `bio.md` and | | |
| | | `crypto.md`: the `Function :` entries and every function the text names are declared, and | | |
| | | what `bio.h` and `crypto.h` declare is named; an operation of `struct hash_ops` or | | |
| | | `struct dsa_ops` has the entry of its function. `usb.md`: the transport's entries are the | | |
| | | members of `struct cm_transport_ops` (functions of `cm.h` after F-267); the `Operation :` | | |
| | | entries and the items an operation leads are the members of `struct usbd_hal_ops`. An | | |
| | | entry that isn't in its header starts with its marker: one further down, or in the | | |
| | | section's intro, doesn't excuse it here (F-311 for the other three documents). Found: five | | |
| | | functions of `bio.h` that `bio.md` doesn't name, the lint's one list of exceptions (F-310). | | |
| | | `ep0_stall` has its marker (F-138). The lint has no tests of its own: tried by hand on | | |
| | | copies of the tree with a name changed on either side | | |
| F-174 | `src/boot/boot.c`, `ab_state.c` | A slot that the A/B state picks and that then fails | Verified | fixed |
| | | to load or authenticate isn't reported back. A verified slot is picked again on every | | |
| | | boot (command mode, the 10 s transport timeout, reset), for good: the other verified | | |
| | | slot is never tried. An unverified slot loses an attempt each time and rolls back. | | |
| | | Decided (D-032, `boot.md` 3.5): the other verified slot boots for this boot only, nothing | | |
| | | is written; the slot is tried again on the next boot. Code: F-256. | | |
| | | Fixed with F-256: the core's loop reports a failed candidate to the policy and asks for the next. A/B | | |
| | | hands out the other slot when it is verified, for this boot, with nothing written and mode fallback. | | |
| | | `test_ab_state.c` (`test_fallback_*`, `test_trial_that_fails_falls_back`), `test_ab_state.py` | | |
| | | (`test_fallback_when_*`, `test_no_fallback_to_a_system_that_is_not_confirmed`) | | |
| F-215 | `boot.c:114`, `ab_state.c:108`, `board.h` | `.ab = NULL` is accepted, but such a board | Reported | fixed |
| | | can't boot from storage: no `get_boot_bio_device`, and BOOTABLE is only set in `boot_ab_state_init()`. | | |
| | | Only imx8mevk today. D-046: the fixed policy's object, `BOOT_POLICY_FIXED(part)`. | | |
| | | Fixed with F-256: `BOOT_POLICY_FIXED(part)` and `BOOT_POLICY_CHAIN(parts)` exist, and each policy's init | | |
| | | marks its candidates. A board without a policy still boots nothing from storage, by `boot part` neither | | |
| | | (`-PB_ERR_NOT_SUPPORTED`, as before). imx8mevk has none: F-300 | | |
| F-256 | `src/boot/boot.c`, `ab_state.c`, `src/board.c` | No boot source policy | Verified (code) | fixed |
| | | interface: A/B is wired into `struct boot_driver` through three callbacks, it is the only policy, | | |
| | | and only it marks partitions bootable (F-215). `boot.md` 3: `boot_policy_*()`, fixed and staggered | | |
| | | policies in Kconfig (`BOOT_POLICY_FIXED`, `_CHAIN`, `_AB`; `BOOT_AB_DRIVER` still gets A/B), the | | |
| | | core's loop over the candidates with the fallback of F-174 | | |
| | | Fixed: `include/boot/policy.h` has the policy's functions (`init`, `next`, `report`, `mode`, `activate`, | | |
| | | `status`) and the core's calls `boot_policy_*()` (`src/boot/policy.c`). `boot_load()` loops over the | | |
| | | candidates: load, verify and prepare are one attempt, the policy hears the result, and `boot()` returns | | |
| | | the first candidate's error when all failed. `struct boot_driver` has the board's policy, not three | | |
| | | callbacks. New: `policy_fixed.c`, `policy_chain.c` (`BOOT_POLICY_FIXED`, `BOOT_POLICY_CHAIN`, | | |
| | | `BOOT_POLICY_AB` in Kconfig; `BOOT_AB_DRIVER` selects A/B). A/B is behind the interface with rule 3's | | |
| | | fallback (F-174); its first candidate, its state writes and `boot activate` are as they were. The host | | |
| | | board's policy is its own object, A/B that also marks the flash's partitions. `boot_policy_mode()` has | | |
| | | no caller until F-260. `test_policy.c`, `test_boot.c`, `test_ab_state.c`, `test_board.c`, | | |
| | | `test_ab_state.py`. Every board builds; not run on a board | | |
| F-258 | `src/boot/{linux,atf}.c`, `src/board/imx8dxlevk` | The handovers compute and | Verified (code) | fixed |
| | | jump each by itself; TF-A's BL33 gets Linux's arguments, hard coded; `bl_params` sit in `.data` for | | |
| | | NXP's i.MX8M BL31 (a platform fact); the DXL starts BL31 through the Linux driver, the MEK through | | |
| | | `bl_params`. `boot.md` 5.1 to 5.3: entry and four arguments, nested BL33 and BL32, `linux` becomes | | |
| | | `image`, the platform says where `bl_params` go, the DXL uses TF-A | | |
| | | Fixed: a handover's `prepare` gives `struct boot_handover_entry`, the entry, four arguments and the | | |
| | | device tree it prepared (`boot/handover.h`). The core jumps with it when the board has no delivery; | | |
| | | `BOOT_DELIVERY_TFA()` (`boot/atf.h`) makes it BL33's entry point, all four arguments, and gives BL32 | | |
| | | OP-TEE's, with the handover's device tree. An image without a part at BL33's entry is refused, as | | |
| | | before. `linux` is `image` (`image.c`, `boot/image.h`), and `atf.c` has no copy of its fields or of the | | |
| | | `/chosen` code. `bl_params`' memory is the platform's, `plat_bl_params` on the i.MX8M (F-294). The 8MP | | |
| | | EVK, the MEK and the DXL use the image handover through TF-A; the DXL's BL33 is the kernel's part | | |
| | | (F-295). `test_tfa.c` pins what BL31 reads, field by field, against what the driver wrote before, and | | |
| | | `test_tfa_image.c` runs the image handover into the delivery with the 8MP EVK's and the MEK's | | |
| | | configurations; `test_boot.c` has the direct jump. Not run on a board | | |
| F-292 | `src/boot/{simple,image}.c`, `image_helpers.c` | An entry part of size 0 | Verified (code) | fixed |
| | | passes placement and the payload hash (nothing is hashed for it), and the handover jumps to its | | |
| | | load address: bytes no hash covers. It needs a signed header, and both the loaded and the in | | |
| | | place path have it (the review of F-253). Refuse an entry part that is smaller than an entry in | | |
| | | the handover's prepare; not empty parts in general (D-008: images with one may boot today) | | |
| | | Fixed: every address pb passes control to lies in a part of which at least an entry was hashed, | | |
| | | else the prepare fails with `-PB_ERR_BAD_META`, the error of a missing part (`PB_RESULT_ERROR` on | | |
| | | the wire, no new value), and nothing is jumped to. The image handover (`image.c`): the kernel's | | |
| | | part, 4 bytes, the instruction at the entry. It asked for the kernel's load address only, so a | | |
| | | kernel without a part is refused there too. The TF-A delivery (`atf.c`): BL31's part, 4 bytes, | | |
| | | which had the same hole and for which a load address was enough, and BL32's when the board names | | |
| | | one: BL31 starts it where pb says, in the secure world, and verifies nothing. BL33 is the | | |
| | | handover's entry. The generic handover (`simple.c`): 8 bytes on every arch, what ARMv7-M's | | |
| | | `arch_jump()` reads (the initial stack pointer and the reset vector, D-028); Firmware Handoff | | |
| | | (F-259) takes the check over. The size is `bpak_part_size()`, what was loaded and hashed at the | | |
| | | load address (`compatibility.md`), the padding too: one byte of content is 512 hashed bytes and | | |
| | | passes, and only an empty part, or one stored without padding (transport) and shorter than the | | |
| | | entry, is refused. Empty parts that nothing jumps to boot as before: a ramdisk (`linux,initrd-start` | | |
| | | and `-end` are its load address), a device tree or a BL32 that the board doesn't name. An empty | | |
| | | device tree that the board names: F-330. No decision entry and no contract text yet: F-331. | | |
| | | `test_image.c` (and for ARMv7), `test_tfa.c`, `test_tfa_image.c`, `test_simple.c`; | | |
| | | `test_boot.py`: an image of the bpak CLI with an empty kernel answers `boot part` with | | |
| | | `PB_RESULT_ERROR` and `boot bpak` with OK ("loaded and verified", `command-mode.md` 5.5), and | | |
| | | neither boots. Every board builds; not run on a board | | |
| F-249 | `src/board.c` | The board's data isn't checked: `cm_board_init()` dereferences | Verified (code) | fixed |
| | | a NULL `cm_transport`; a name over 16 characters overruns `board_id` (`memcpy` with `strlen`); no | | |
| | | boot driver, or several (the last wins); `.slc = NULL`. `CONFIG_CM_AUTH_PASSWORD=y` without | | |
| | | `password_auth` in jiffy's, the RT's and the host's defconfigs; imx8mevk has `CONFIG_BOOT_LINUX` | | |
| | | without `boot_linux`. `board.md` 3: checked in `board_setup()` and `cm_board_init()`. D-046: | | |
| | | "one boot driver" becomes a policy and a handover that are set; a module that isn't compiled | | |
| | | in is a link error, not a check. D-049: `cm_transport` leaves the board, and its check with it | | |
| | | Fixed: each function checks what it uses, before it uses it, `-PB_ERR_PARAM`. `board_setup()`: `rot` | | |
| | | and `slc` before anything is set up (an `slc` without `read_status` is a board without a life cycle | | |
| | | and passes), `boot_policy` and `boot_handover` when their step comes (F-335), a GPT's layouts in | | |
| | | `gpt_ptbl_init()`, before the device is read. A failed check is a failed step: the steps after it | | |
| | | are skipped and the handover is set up when its own data passed. `cm_board_init()`: `name` (1 to 16 | | |
| | | characters, read no further than the 17th byte), `password_auth` with `CONFIG_CM_AUTH_PASSWORD`, | | |
| | | and that `cm_transport` is set, which goes with F-267; a failed check calls neither the board's | | |
| | | hook nor `cm_init()`, `cm_run()` returns and pb resets. A bad name doesn't stop a boot: | | |
| | | `board_setup()` doesn't use it. The copy to `board_id` in `cm_main.c` is unchanged: `cm_board_init()` | | |
| | | is `cm_init()`'s only caller, so the name it copies has at most 16 characters. The three defconfigs | | |
| | | lose `CONFIG_CM_AUTH_PASSWORD` and keep the token: their boards have no secret to compare with | | |
| | | (`board.md` 4.5), and `AUTHENTICATE` with a password answers `NOT_SUPPORTED` without data, as it | | |
| | | did. imx8mevk fails the policy's check and stops in command mode as before (F-300). `test_board.c`, | | |
| | | built twice (`test_board_password` with the method), `test_gpt.c`, `test_cm.c` (a name of 16 | | |
| | | characters on the wire; it gets the method by a definition). Every board builds; not run on a board | | |
| F-177 | `src/boot/ab_state.c` | Speculative rollback sets the fallback's attempts to 1 | Verified | fixed |
| | | and boots it at once without using one: it gets two boots (`pbstate switch B 3` gives | | |
| | | three). In the ping-pong, `error =` drops the other rollback bit. No board uses the | | |
| | | speculative mode; `test_speculative_rollback_from_a` asserts B, B, A. Not intended: | | |
| | | `boot.md` 3.6 (D-032), F-257. | | |
| | | Fixed with F-257: speculative mode counts no attempts and writes `enable` alone, so a slot gets one | | |
| | | boot (B, A, B) and no error bit is touched. `test_speculative_counts_no_attempts`, | | |
| | | `test_speculative_writes_only_enable`. The rollback to a confirmed slot still assigns `error`: F-340 | | |
| F-257 | `src/boot/ab_state.c` | Speculative mode differs from `boot.md` 3.6: the | Verified (code) | fixed |
| | | fallback gets one trial attempt (two boots, F-177) and a rollback bit is dropped. Per `boot.md`: | | |
| | | only with no viable candidate, alternate per boot, no attempts, ends when the OS confirms, the | | |
| | | handover says so, `depends on ENABLE_WATCHDOG`. D-040: it also starts with nothing enabled (A | | |
| | | first), and `BOOT_AB_SPECULATIVE` is its only switch: the board's `rollback_mode` field goes | | |
| | | Fixed: `CONFIG_BOOT_AB_SPECULATIVE` (`src/boot/Kconfig`, default n, `depends on BOOT_POLICY_AB && | | |
| | | ENABLE_WATCHDOG`) is the only switch; `rollback_mode` and `enum ab_rollback_mode` are gone from the | | |
| | | configuration and the boards. Without the symbol A/B is what normal mode was: run against the code | | |
| | | before over every state, sequence of results and write fault (186624 runs), the candidates, the modes | | |
| | | and the bytes on the disk are the same. With it, where A/B has no viable candidate: from the state, | | |
| | | the other slot is enabled, written before the load and handed out; with nothing enabled, A; when this | | |
| | | boot's candidates failed, the slot that isn't confirmed, for this boot and with nothing written. A | | |
| | | speculative boot's second candidate is the other slot, without a write. The write is `enable`, and the | | |
| | | attempts zeroed; no error bit changes. The mode is speculative for a candidate 3.6 handed out. Where | | |
| | | 3.6 is silent: F-342. A write that fails ends the boot as before, and a backup that can't be written | | |
| | | keeps the primary alternating with nothing booted (F-179). `test_ab_state.c` is built twice: | | |
| | | `test_ab_state_speculative` has a test for each rule (`test_speculative_*`), also for a write that | | |
| | | fails on either copy and for power lost between the copies; `check_ab_speculative_kconfig.py` has the | | |
| | | dependency. pb-host has no speculative mode, so no integration test (F-341). Every board builds, jiffy | | |
| | | also with the symbol; not run on a board | | |
| F-260 | `src/boot/dtb.c` | `boot_dtb_set_chosen()` reads the RoT, the SLC and the device | Verified (code) | fixed |
| | | id itself, and runs the board's hook after `pb,device-uuid` and before pb's other properties. | | |
| | | `boot.md` 5.5, 5.6: pb's information collected once, the hook first, `pb,version`, | | |
| | | `pb,boot-partition` and `pb,boot-mode` added (and in the README's table). From F-256: the | | |
| | | boot's mode is the core's: NORMAL for a named partition or the host's stream, the policy's | | |
| | | after its report; `boot_policy_mode()` alone is stale after a failed fallback | | |
| | | Fixed: `struct boot_info` (`boot/handover.h`) is pb's information, with the names of pb's structure: | | |
| | | `version`, `device_uuid`, `slc` (`BOOT_INFO_NO_SLC` without one), `available_keys`, `status` (0 | | |
| | | until F-248), and the candidate's `boot_mode`, `boot_partition_uuid`, `system_name` and | | |
| | | `active_key`. `boot_load()` collects the first part once, after `early_boot()` and before the first | | |
| | | candidate: a device identity, a life cycle or a RoT that can't be read stops the boot, with every | | |
| | | handover and every source, and no candidate is tried. A handover's `prepare` gets the structure | | |
| | | instead of the partition's UUID and reads nothing else. The mode is the core's: normal for a named | | |
| | | partition and for the host's stream, `boot_policy_mode()` for a candidate, asked when the policy | | |
| | | handed it out (prepare, which writes it, comes before the report). The system's name is the | | |
| | | policy's: `boot_policy_system_name()`, an optional function of a policy (`system_name`), A/B's | | |
| | | `boot_ab_part_uu_to_name()`; `resolve_part_name` is gone from both configurations and from the | | |
| | | boards. `dtb.c`: the board's hook first, then pb's properties. New: `pb,boot-mode`, | | |
| | | `pb,boot-partition` and `pb,version`, in the README's table; `pb,status` waits for F-248. The | | |
| | | released properties keep their bytes (`test_chosen_released_bytes`, pinned against the code before | | |
| | | the change), and one pb has no value for is left as the image's tree or the hook has it, as before | | |
| | | (F-348). A new one that doesn't fit the tree (a 2.x device tree with placeholders and a headroom of | | |
| | | 0; the three take 138 bytes) is left out with a warning and the image boots: the mode first, then | | |
| | | the partition, then the version, each on its own, the same ones in every mode. The generic handover | | |
| | | keeps its registers. The hook no longer finds `pb,device-uuid` in the tree it gets. The RT's and the | | |
| | | host's boot now read the information too, unmeasured (F-349). `test_image.c`, `test_boot.c`, | | |
| | | `test_simple.c`, `test_policy.c`, `test_tfa_image.c`, `test_ab_state.c`, `test_ab_state.py` | | |
| | | (`test_boot_mode`: a policy boot, a fallback, `boot part` and `boot bpak` after a fallback that | | |
| | | failed, read from pb-host's log, F-347). `boot.md` 3 names no function for the system's name: F-345. | | |
| | | Every board builds; not run on a board | | |
| F-340 | `src/boot/ab_state.c` | Minor. A rollback to a confirmed slot assigns `error` | Verified (code) | fixed |
| | | (`error = PB_STATE_ERROR_A_ROLLBACK`), which clears the other slot's rollback bit; `boot.md` | | |
| | | 3.5, rule 2 sets the rolled back slot's bit. F-177's rest: speculative mode no longer writes | | |
| | | `error` (F-257). Without speculative mode no writer leaves such a state (`pbstate -s` and | | |
| | | `boot activate` clear `error`). With it: B confirmed with its bit set, | | |
| | | `pbstate_invalidate_system(A)`, a speculative boot of A, then the rollback to B. `|=` changes | | |
| | | what normal mode writes in that state: decide (D-008) | | |
| | | Fixed: the two rollbacks in `first_candidate()` set their slot's bit (`error |=`), in both builds. | | |
| | | Where the other slot's bit isn't set, the bytes written are the same as before (D-008), so is | | |
| | | every state reachable without speculative mode. Only a state that has it set changes: the bit is | | |
| | | kept. Reached only with `CONFIG_BOOT_AB_SPECULATIVE`, by the history above. | | |
| | | `boot activate` still assigns `error = 0`, as 3.5 says; who else clears a bit: F-360. | | |
| | | `test_rollback_keeps_the_other_slots_bit` (both builds: both directions with the other bit set, all | | |
| | | 512 bytes and the board's registers; no bit set, the bytes as before) and | | |
| | | `test_speculative_then_rollback_keeps_both_bits` (the history); both fail without the fix | | |
| F-273 | `src/cm/cm_main.c` `stream_check()` | The stream's byte offset is divided by | Verified (code) | fixed |
| | | the block size and the remainder dropped: an offset that isn't a block multiple reads or writes | | |
| | | from the block's start and answers OK. The Python host sends multiples (0, the chunk size, the | | |
| | | partition's size less 4 KiB). `command-mode.md` 5.4: INVALID_ARGUMENT | | |
| | | Fixed: `stream_check()` refuses an offset that isn't a multiple of the block with | | |
| | | INVALID_ARGUMENT, for `STREAM_WRITE_BUFFER` and `STREAM_READ_BUFFER`; nothing is read or written. | | |
| | | No released host sends one: the Python host and the 2.x C host send 0, multiples of the caps' | | |
| | | chunk size (`CONFIG_CM_BUF_SIZE_KiB`, 4 KiB on pb-host, 32 KiB on the RT1060 whose NOR has 4 KiB | | |
| | | blocks, 4 MiB elsewhere) and the partition's size less 4 KiB. `test_cm.c` | | |
| | | (`test_stream_offset_inside_a_block`), `test_stream.py` | | |
| F-188 | `cm_main.c` `cmd_part_erase` | No `BIO_FLAG_WRITABLE` check. F-055 listed it and is | Reported | fixed |
| | | marked fixed, but `6a1a0e23` fixed only the range. | | |
| | | Fixed: `cmd_part_erase()` refuses a partition without `WRITABLE` with IO_ERROR, the answer | | |
| | | `STREAM_WRITE_BUFFER` gets for it (`command-mode.md` 4.5 and 5.3), after the look up and before | | |
| | | the range check. That includes the device a partition table is on, which has only `VISIBLE` since | | |
| | | F-265, so an erase of the raw device's UUID is refused too. A count of 0 still erases nothing and | | |
| | | answers as before (F-075) whatever the flags, since hosts older than 2.0 send it for every erase. | | |
| | | `test_cm.c` (`test_part_erase_needs_writable`), `test_commands.py` (the host disk's device) | | |
| F-279 | `src/cm/cm_main.c` `cmd_bpak_read()`, `cmd_part_verify()` | Low. The header's | Verified (code) | fixed |
| | | block is the partition's block count less 4 KiB of blocks, unsigned: on a partition below | | |
| | | 4 KiB it wraps, `bio_read()` refuses the range and the host gets INVALID_ARGUMENT. | | |
| | | `command-mode.md` 5.3 (D-060): NOT_FOUND for `PART_BPAK_READ`. `boot.c` has the same | | |
| | | arithmetic for a bootable partition | | |
| | | Fixed: a partition below 4 KiB is checked by its size before the header's block is computed. | | |
| | | `PART_BPAK_READ` answers NOT_FOUND (was INVALID_ARGUMENT). `PART_VERIFY` with `bpak` answers | | |
| | | INVALID_ARGUMENT as before (5.3: with `bpak` the size is at least 4 KiB inside the partition), now | | |
| | | before the hash starts. `boot.c`'s load from a partition returns `-PB_ERR_BAD_HEADER`, as the in | | |
| | | place load does (was `-PB_ERR_PARAM` from `bio_read()`): `BOOT_PART` answers ERROR, not | | |
| | | INVALID_ARGUMENT, and nothing is read. F-190's `bytes_to_verify` underflow is other arithmetic | | |
| | | (the host's size) and stays there. `test_cm.c` (`test_bpak_read_small_partition`, | | |
| | | `test_part_verify_small_partition`), `test_boot.c` (`test_bio_boot_smaller_than_a_header`), | | |
| | | `test_commands.py` (the host's 512 byte 'PB State Primary') | | |

## Dependencies / vendored code

| ID | Finding | Status |
|---|---|---|
| F-021 | Vendored bpak is ~v0.9.0, the CI CLI is v0.9.3, and there is no sync procedure. | fixed 2026-11-08: both 1.0.1, `scripts/sync-bpak` (D-024) |

## Dead code / cleanup

| ID | Finding | Status |
|---|---|---|
| F-034 | Bash tests that exist but don't run: `test_config`, `test_invalid_key_index2`, `test_setup_lock`, `test_bpak_show`, `test_part_offset_write`. They use commands and tools that no longer exist. | removed with the bash suite |
| F-146 | `src/boot/atf.c` (+ `atf-interface/`, ~1,050 lines) is never compiled: no defconfig sets `BOOT_ATF`. Its `/chosen` patching duplicates `linux.c` (with F-071). Keep for an 8MP TF-A handover, or remove. | closed: kept, the 8MP EVK boots with it (`ac10a592`), warnings fixed (`8ceeea5a`); the duplicated `/chosen` code waits for Jonas' boot review |

## Build / CI / docs

| ID | Finding | Status |
|---|---|---|
| F-111 | `scripts/pbdev` | `IMX8M_FW_DIR` (`build-firmware.sh`, `imx8m-ddr.md`) was not passed into the container, so `scripts/pbdev firmware imx8mevk` always linked the zero filled placeholder firmware. The directory must also be inside the repository, the only mount. | fixed (`imx8m-dram-cleanup`): `-e IMX8M_FW_DIR`; documented in `build-firmware.sh` |
| F-040 | `build.yml`: `actions/checkout@v1`, deprecated codecov bash uploader, Coverity steps fail without the secret (fork PRs). | fixed (D-006) |
| F-042 | `pyproject.toml` has no `[build-system]`/`[project]`; all metadata is in `setup.py`. The old author email is still in it. | fixed: PEP 621 metadata in `pyproject.toml` (setuptools backend, version from `version.txt`), `setup.py` removed, author `jonas@blxt.se` |
| F-046 | CI's `build.yml` installs neither click nor gcovr versions; it uses whatever the runner has. CI passed with old click, while PyPI users get click >= 8.2 (subgroups without a command exit with 2). `test_cli` was updated and the requirement is now `click>=8.2`. | fixed (claude-work); CI still to move to the container |
| F-045 | `tests/common.sh` hardcodes `/tmp/pb.sock`, `/tmp/disk` and `build-test/`, so tests cannot run in parallel. | fixed by the host tests (per-test working directory) |
| F-097 | The Coverity job in `build.yml` has failed on every push to master since it was added (#61): `cov-build` runs in the dev container, which has no `file` ("Cannot find '/usr/bin/file' command"). Nothing was ever uploaded. | fixed (`ci-coverity-file`): `file` is in the container; only a push to master shows whether the upload works |
| F-103 | imx8mevk is never compiled in CI (F-051: needs NXP's DDR training blobs). With placeholder blobs it compiles (`imx8m-ddr.md`, Testing), which would at least catch build breaks. | fixed (`imx8m-dram-cleanup`): CI builds it with placeholder blobs (`build-firmware.sh`, `IMX8M_FW_DIR` for the real ones) |
