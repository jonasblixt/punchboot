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
| F-275 | `src/cm/cm_main.c` `pb_command_parse()` | The frame is checked before the gate: | Verified (code) | fixed |
| | | a locked device answers INVALID_COMMAND before authentication to a command its build doesn't | | |
| | | have and to a frame with a wrong magic, which tells a host which commands the build knows. | | |
| | | D-058 (Jonas), `command-mode.md` 3.2: NOT_AUTHENTICATED for everything but the identifier and | | |
| | | the authentication, from 3.0.0. A change of the wire's meaning, for `security.md` 4.7 | | |
| | | Fixed: the gate (`cm_gate_allows()`) comes first. A session it refuses (locked or an unknown life | | |
| | | cycle, not authenticated) runs `DEVICE_IDENTIFIER_READ` and `AUTHENTICATE` with the right magic | | |
| | | and gets one result, NOT_AUTHENTICATED, for every other frame: a number pb doesn't define (0, 30 to | | |
| | | 255) and a wrong magic, whatever its command byte, also the two's. An open or authenticated session | | |
| | | is checked as before, the frame then the command: before and after the change, every command id | | |
| | | with the right magic, a wrong one and 0, gives the same bytes to the host. `test_cm.c`: | | |
| | | `test_gate_open_sessions` pins the result of each id for an open and an authenticated session | | |
| | | (pinned on the code before), `test_gate_refused_sessions` (also with the life cycle unreadable), | | |
| | | `test_gate_refused_then_authenticated`; `test_security.py`: the raw wire on a locked device before | | |
| | | and after a token, and an open device. No recorded transcript has INVALID_COMMAND. Before | | |
| | | authentication, `AUTHENTICATE`'s data sent after a refusal is read as two frames with a wrong | | |
| | | magic, NOT_AUTHENTICATED where it was INVALID_COMMAND; released hosts don't send it (4.4, F-274) | | |
| F-278 | `src/cm/cm_main.c` `cmd_auth()`, `auth_read_data()` | Low. An `AUTHENTICATE` with | Verified (code) | fixed |
| | | a size of 0 or above 1024 ends the authentication of a session that had it (`authenticated = | | |
| | | false` after `auth_read_data()` refused), one with a method that isn't in the build leaves it. | | |
| | | `command-mode.md` 3.3 (D-060): a refusal before the data changes nothing and isn't delayed; | | |
| | | with F-276 | | |
| | | Fixed: `auth_read_data()` says when it refused before the data, and `cmd_auth()` then leaves the | | |
| | | session as it was. The refusals before the data, each with its result as before: a method that isn't | | |
| | | in the build or isn't defined (`NOT_SUPPORTED`), the password on a board without `password_auth` | | |
| | | (`NOT_SUPPORTED`), a size of 0 or above 1024 for the token or the password (`INVALID_ARGUMENT`). Data | | |
| | | that arrived and didn't verify still ends the authentication, and so does a data transfer that failed | | |
| | | (the session ends, 3.1). `test_cm.c`: `test_auth_data_size`, `test_auth_refusal_keeps_session` | | |
| F-276 | `src/cm/cm_main.c` `cmd_auth()` | A failed authentication is answered at once: | Verified (code) | fixed |
| | | a host can try passwords as fast as the transport carries them. D-058 (Jonas), | | |
| | | `command-mode.md` 3.3: the result of a failed attempt is sent 1 s after the data arrived, with | | |
| | | every method, the watchdog kicked meanwhile; no counter, no lockout | | |
| | | Fixed: `cmd_auth()` takes the tick when the data arrived, and after a check that failed | | |
| | | `auth_failed_wait()` holds the result until 1 s from it: a slow check (ECDSA in software) shortens the | | |
| | | rest, one longer than 1 s isn't held. Every result after the data is a failure and waits, the token's | | |
| | | `KEY_REVOKED`, `ERROR` and `NOT_SUPPORTED` too; a success, a refusal before the data (F-278) and a data | | |
| | | transfer that failed are answered at once. The wait compares differences of `plat_get_us_tick()`, which | | |
| | | wraps, and kicks the watchdog after the check and every 100 ms (the shortest watchdog is 5 s). It doesn't | | |
| | | touch the transport: a host that disconnects gets nothing sooner, and its next attempt, on any | | |
| | | connection, waits its own second. Hosts wait 10 s for a transfer (2.7.1's C host, today's Python one). | | |
| | | `test_cm.c`: `test_auth_failure_is_delayed`, `test_auth_failure_delay_counts_from_the_data`, | | |
| | | `test_auth_not_delayed`; `test_security.py`: `test_failed_authentication_is_answered_after_a_second` | | |
| F-185 | `src/cm/cm_main.c`, `usbd.c` | `authenticated` survives a USB replug, a bus reset and | Reported | fixed |
| | | `restart_command_mode`: the next host on a powered, locked device is authenticated. No idle timeout. | | |
| | | Clear it on bus reset and transport restart. `command-mode.md` 3.1: the session ends with the | | |
| | | transport, and the stream is the session's too (`block_dev`, `stream_written`, `stream_failed`) | | |
| | | Fixed: `cm_session_end()` in `cm_main.c` is the one place that ends the session: `authenticated` | | |
| | | (true without `CONFIG_CM_AUTH`), no stream (`block_dev` `-PB_ERR_STATE`, not bio 0), `stream_written`, | | |
| | | `stream_failed`. `cm_run()` calls it where the transport is started and restarted (a lost transfer while | | |
| | | waiting for a command), and `cm_read()`/`cm_write()` when a transfer inside a command fails, which is | | |
| | | how the transport reports an unplug or a bus reset (`usb.md` 2.4). On USB the released CLI keeps working: | | |
| | | it opens, claims, releases and closes the device between invocations (libusb, 2.x and 3.x alike), | | |
| | | which is no reset and no `SET_CONFIGURATION`, so `auth` and the commands after it share a session. | | |
| | | On pb-host's socket each connection is a session (`usb.md` 2.6). `test_cm.c`: `test_session_*` | | |
| | | (lost while waiting, a lost read and a lost write in a command, the stream, what doesn't end it); | | |
| | | `test_security.py`: `test_authentication_ends_with_the_connection`. Left: the USB controllers don't | | |
| | | report a replug or a bus reset (F-363, F-201), a lost at a start makes pb reset (F-364) | | |
| F-330 | `src/boot/dtb.c` | An empty device tree part that the board names is read | Verified (test) | fixed |
| | | at its load address, bytes no hash covers: `boot_dtb_grow()` checks the FDT header there (the | | |
| | | vendored libfdt's `fdt_check_header()`: the magic and the version only) and compares its | | |
| | | `totalsize` with the part's size, 0. A blob there that says `totalsize` 0 passes, is grown by the | | |
| | | headroom, gets pb's `/chosen` and is handed to the next stage, and to BL32: a device tree nothing | | |
| | | authenticated (`security.md` 4.2). Anything else there is refused (`-PB_ERR`). As F-292 it needs a | | |
| | | signed header, and what an earlier, refused load left at the address. Found with F-292, in a | | |
| | | throwaway case of `test_image.c`. Refuse a device tree part that is smaller than an FDT header, and | | |
| | | a `totalsize` below it, before the part is read; Firmware Handoff's device tree (F-259) the same | | |
| | | Fixed: `boot_dtb_grow()` refuses a part smaller than an FDT header (`sizeof(struct fdt_header)`, | | |
| | | 40 bytes) with `-PB_ERR_BAD_META`, the error of a missing part, before anything at the load address | | |
| | | is read; a header whose `totalsize` is below that is refused with `-PB_ERR`, as an invalid device | | |
| | | tree. `PB_RESULT_ERROR` on the wire either way, no new value, and nothing is jumped to. The size is | | |
| | | `bpak_part_size()`, the padding too: an empty part, or one stored without padding (transport) and | | |
| | | shorter than the header. No valid device tree is refused: one is its header, a reservation map's | | |
| | | terminator and `FDT_END`, more than 40 bytes. Firmware Handoff (F-259) takes the check over. An | | |
| | | image with an empty device tree that the board names was accepted by every release: no decision | | |
| | | entry covers it yet (D-074 names F-330). `test_image.c` (and for ARMv7): | | |
| | | `test_dtb_smaller_than_its_header_is_refused`. Not run on a board | | |
| F-271 | `src/cm/cm_main.c`, `src/cm/Kconfig`, `configs/imx8m{,p}evk_defconfig` | A build | Verified (code) | fixed |
| | | with `CONFIG_CM_AUTH` and no method builds: once locked, its command mode answers NOT_SUPPORTED | | |
| | | to `AUTHENTICATE` for good. The imx8mevk's and the imx8mpevk's defconfigs select none. D-059 | | |
| | | (Jonas), `command-mode.md` 3.2: a build with authentication selects at least one method, or it | | |
| | | doesn't build; the two defconfigs get the token. `CONFIG_CM_AUTH` itself stays the product's | | |
| | | switch: off means no gate, an open command mode also on a locked device, by choice. The | | |
| | | option's help text says so | | |
| | | Fixed: `pb/cm.h` stops a build with `CONFIG_CM_AUTH` and neither `CONFIG_CM_AUTH_TOKEN` nor | | |
| | | `CONFIG_CM_AUTH_PASSWORD` with an `#error`; the header takes `config.h` itself, so the check doesn't | | |
| | | hang on what a file included before it. Kconfig can't fail a build, and a `select` or a | | |
| | | `depends on` would have turned the token on, or the gate off, without a word. The imx8mevk's and | | |
| | | the imx8mpevk's defconfigs select the token, which needs nothing of a board: the keystore, the hash | | |
| | | and the EC verification that verify its images (the CAAM on both). `CONFIG_CM_AUTH`'s help text | | |
| | | says what on and off mean. Every other defconfig with command mode has a method; the orphans | | |
| | | `bebop` and `pico8ml` (F-033) have no command mode entries and no board. `unit.cm_auth_needs_a_method` | | |
| | | (`tests/unit/CMakeLists.txt`) compiles `pb/cm.h` with the gate and no method and expects the | | |
| | | `#error`. Both boards build; not run on a board | | |
| F-277 | `src/cm/cm_main.c` `cm_read()`, `cm_write()`, `PB_CMD_DEVICE_READ_CAPS` | A | Verified (code) | fixed |
| | | transfer inside a command has no timeout: a host that stops (`AUTHENTICATE`, then no data) | | |
| | | leaves pb waiting without a kick (F-189), for ever without a watchdog. The caps' | | |
| | | `operation_timeout_ms` and `part_erase_timeout_ms` are never set. D-058 (Jonas), | | |
| | | `command-mode.md` 4.6: `CONFIG_CM_TRANSFER_TIMEOUT` (10 s), kicking while waiting, then the | | |
| | | session ends and the transport restarts; the caps carry pb's timeouts (F-226 for the host) | | |
| | | Fixed: `cm_read()` and `cm_write()` wait at most `CONFIG_CM_TRANSFER_TIMEOUT` (10 s, 1 to 65) for | | |
| | | a transfer and kick the watchdog on every poll. A transfer that fails or times out ends the session, | | |
| | | the command's later transfers fail without touching the transport, and the command loop restarts the | | |
| | | transport (`init`, then `connect`), so the device waits for a host again (`command-mode.md` 2.2). | | |
| | | That also covers F-274 (3): an ignored `cm_write()` result or a failed `cm_read()` no longer leaves | | |
| | | the command loop on the same transport. The caps carry `operation_timeout_ms` = the transfer | | |
| | | timeout; `part_erase_timeout_ms` stays zero (F-375). pb-host's socket `complete()` polls (10 ms) | | |
| | | instead of blocking, so the timeout runs there too. `test_cm.c` (`test_transfer_timeout_read`, | | |
| | | `_write`, `test_session_ends_with_a_lost_transfer`, the caps in `test_basic_commands`) | | |
| F-189 | `cm_main.c`, `usbd.c` | The watchdog is kicked only between commands: a `part verify` or | Reported | fixed |
| | | `boot part` of a few hundred MiB against a 5 s watchdog resets the board (timing not measured). | | |
| | | `cm_read`/`cm_write` spin on AGAIN without a kick or timeout. | | |
| | | Since F-243 the load of `boot part` and `boot bpak` kicks per chunk; `part verify` doesn't. | | |
| | | Fixed: `cmd_part_verify()` kicks once per chunk, after it is read and its hash started, as the boot's | | |
| | | load does since F-243, the bpak header included (`command-mode.md` 4.6). `cm_read()`/`cm_write()` | | |
| | | kick while they wait and time out since F-277. No other command of the core runs more than one | | |
| | | chunk: a stream write or read is one buffer, an erase is as long as the blocks the host names, and | | |
| | | a board's command kicks itself (`board.md` 4.4). `test_cm.c` (`test_part_verify_kicks_per_chunk`) | | |
| F-272 | `src/cm/cm_main.c` | Only `cmd_part_tbl_read()` looks at `BIO_FLAG_VISIBLE`. | Verified (code) | fixed |
| | | Stream init, erase, verify, bpak read, `boot part` and activate find a hidden partition by its | | |
| | | UUID (`bio_get_part_by_uu()`). With F-186 (`part verify` reads any partition byte by byte). | | |
| | | `command-mode.md` 5.3, D-058 (Jonas): a hidden partition is internal to pb and answers | | |
| | | NOT_FOUND to every command; `part verify` needs no `READABLE`. No board in the tree hides a | | |
| | | partition (children inherit `VISIBLE` from their device) | | |
| | | Fixed: `cm_part_hidden()` and `cm_part_by_uu()` in `cm_main.c`. A UUID that names a partition | | |
| | | without `VISIBLE` answers NOT_FOUND to stream init, erase, verify, bpak read, `boot part`, activate | | |
| | | and the table install, before anything else looks at it; the boot and the policy never hear of it. A | | |
| | | UUID that names no partition goes on as before (activate and `boot part` pass it to the policy). | | |
| | | `BOOT_BPAK`'s UUID is a label and isn't checked (5.3). `test_cm.c` (`test_hidden_partition`; | | |
| | | `test_part_table_install` makes the disk visible, as F-265's devices are) | | |
| F-186 | `cm_main.c` `cmd_part_verify`, `cmd_bpak_read` | Neither checks | Reported | fixed |
| | | `BIO_FLAG_READABLE`/`VISIBLE`: `part verify` with size 1, 2, ... and hash guesses reads any partition | | |
| | | byte by byte, hidden ones too. It also retargets the stream's `block_dev`. | | |
| | | Fixed: `cmd_part_verify()` looks the partition up into a local `dev`; the stream's `block_dev` stays | | |
| | | on its partition, also when the verify finds none. The flags as `command-mode.md` 5.3 has them | | |
| | | (D-058): a hidden partition answers NOT_FOUND to both commands (F-272); `PART_VERIFY` and | | |
| | | `PART_BPAK_READ` need no `READABLE`, which gates only the bulk read, since the host hashes its file | | |
| | | and asks pb for the same. A secret belongs on a hidden partition. `test_cm.c` | | |
| | | (`test_part_verify_keeps_the_stream`, `test_hidden_partition`). Left: the two use the stream's | | |
| | | buffers (F-365) | | |
| F-187 | `cm_main.c:881` | The auth gate sends one result for `board command`, but hosts send the | Reported | fixed |
| | | request anyway (F-095): the device parses it as commands and answers INVALID_COMMAND twice; with an | | |
| | | empty request the host hangs. Drain the request and send the second result. | | |
| | | Fixed: `cm_gate_refuse()` in `cm_main.c`. For a refused `BOARD_COMMAND` the gate sends | | |
| | | NOT_AUTHENTICATED, reads the 1024 byte request when `request_size` isn't 0 (as `cmd_board()` does), | | |
| | | and sends NOT_AUTHENTICATED again with a response size of 0; the board isn't called. `cm_drain()` | | |
| | | reads the request in pieces into the result frame, so no buffer changes. `command-mode.md` 3.2, 4.4. | | |
| | | `test_cm.c` (`test_gate_refuses_board_command`, `test_gate_refused_sessions`), `test_security.py` | | |
| | | (`test_locked_device_refuses_board_command_in_step`) | | |
| F-128 | `src/cm/cm_main.c` `cmd_boot_bpak`, `src/board/imx8mpevk` | The imx8mpevk has no boot driver, so `boot_load()` returns `-PB_ERR_PARAM` before reading the image. `cmd_boot_bpak` has already answered OK, so the host keeps sending the image, pb parses it as commands (`Invalid command: 0`) and the board resets into the serial downloader. The host sees `Error: Transfer`. | Seen on the board (2026-10-29) | fixed |
| | | The EVK got a boot driver (2026-10-29), so `boot_load()` gets as far as the image there. | | |
| | | Fixed (D-058, `command-mode.md` 5.5): `cmd_boot_bpak()` notes whether the boot read from the host. | | |
| | | When `boot_load()` fails before it read anything (`early_boot()`'s stop, pb's information that can't | | |
| | | be collected (F-260), no handover, no boot config), the device reads the header's 4096 bytes with | | |
| | | `cm_drain()` and sends the header's result with the refusal; the host stops there. After a failure | | |
| | | that read the header the boot flow has sent the results, as before. With F-274 (4). `test_cm.c` | | |
| | | (`test_boot_bpak_refused_before_the_header`, `test_boot_bpak`). Not covered on pb-host: it has no | | |
| | | refusal before the header from command mode | | |
| F-274 | `src/cm/cm_main.c`, `tools/punchboot/session.py` | Exchanges that get out of | Verified (code) | fixed |
| | | step. (1) A refused `STREAM_PREPARE_BUFFER` (the gate, the size, the id) sent one result, but | | |
| | | hosts send the data whatever the first result says. (2) `BOARD_STATUS_READ` whose `status()` | | |
| | | fails sent the error, the data and a second result. (3) Most `cm_write()` results are ignored, | | |
| | | and a failed `cm_read()` inside a command returned to the loop on the same transport. (4) | | |
| | | `boot bpak` refused before the header was read. `command-mode.md` 4.4 | | |
| | | Fixed, a commit per part: (1) the data is read and dropped and the second result has the | | |
| | | refusal; (2) the error is sent alone, as every released host expects; (3) with F-277: a transfer | | |
| | | that fails or times out ends the session, the command's later transfers fail at once and the | | |
| | | loop restarts the transport; (4) with F-128: the header is read and its result has the refusal | | |
| F-254 | `src/boot/image_helpers.c` | `boot_image_verify_parts()` checks the parts | Verified (code) | fixed |
| | | against code, data, stack, bss and `.no_init`, not `.ro_data` (the keystore's keys; `ro_data_start` | | |
| | | is imported and unused) nor `.ramlog`. `CHECK_OVERLAP` has no wrap-around check (`dtb.c` has its own | | |
| | | `overlaps()`, which does), parts aren't checked against each other nor against the board's DRAM. A | | |
| | | signed image is trusted, but unverified payload bytes land there before the payload hash is checked | | |
| | | (`principles.md` 2). An allow list instead of a deny list. D-055, `boot.md` 4.3, `board.md` | | |
| | | 2.9: `image_regions` in `struct board`; a part lies inside one region and over no other part; | | |
| | | that no region overlaps `pb`, `buffers`, the Firmware Handoff region or `bl_params`' region is | | |
| | | checked once, with the board's data. The platform header names the SoC memory a board may list. | | |
| | | Fixed: `image_regions` in `struct board` (`boot/image_regions.h`), checked first in `board_setup()` | | |
| | | by `boot_image_set_regions()`: a list that fails is no list. `boot_image_verify_parts()` compares in | | |
| | | 64 bits: every part inside one region (an empty part too) and over no other part; a range that | | |
| | | wraps or an address the arch can't hold is in no region. The DT's headroom is asked for in | | |
| | | `boot_dtb_grow()`, which no longer names pb's sections. `src/link.lds` provides `_pb_start/_end` and | | |
| | | `_buffers_start/_end`, what is linked, not the lengths of `MEMORY` (F-391). The Firmware Handoff | | |
| | | region and `bl_params`' region are in the list's check under `CONFIG_BOOT_FH` and | | |
| | | `PLAT_BL_PARAMS_BASE`, which no build defines yet (F-259, F-294). Every board has its list; the | | |
| | | headers name the 8M's BL31 OCRAM, the 8X's BL31 place and the RT's TCMs and OCRAM2. Images an | | |
| | | older release loaded outside a board's list are refused (D-055). `test_image_helpers.c` | | |
| | | (`test_verify_parts_*`, `test_regions_*`, and `test_image_helpers_handover` for the two regions | | |
| | | no build has), `test_board.c` (`test_image_regions`), `test_image.c` (the headroom), | | |
| | | `test_boot.py` (`test_image_outside_the_image_regions_is_refused`). Not run on a board. Left: | | |
| | | F-390, F-391 | | |
| F-152 | `src/drivers/mmc/mmc_sd.c:303` | `dev->rca << RCA_SHIFT_OFFSET`: the 16-bit RCA is promoted to `int` and shifted by 16, which overflows for an RCA from 0x8000 (UBSan in `test_mmc_sd` and `test_imx_usdhc` with `PB_SANITIZE=ON`, RCA 0xAAAA). The sanitizer CI run should fail on it. | Verified (UBSan) | fixed (`dede370f`): the same shift as F-290, found again by the sanitizers and fixed there |
| F-144 | `src/plat/imx8x/plat.c` `imx8x_wdog_init` | With `CONFIG_ENABLE_WATCHDOG` off the function has no `return` (UB; the caller gets garbage). | Verified (code) | fixed, as F-233: `SC_ERR_NONE` after the `#ifdef` |
| F-157 | `src/boot/image_helpers.c` `boot_image_verify_parts`, `src/boot/dtb.c` `overlaps_pb` | The check that a part doesn't load over pb lists code, data, stack, bss and `.no_init`, but not `.ro_data` (keystore included; `image_helpers.c` imports `_ro_data_region_*` and never uses them), `.ramlog` or the i.MX8M's `.umctl`. A signed image with a part on any of them passes. Two hand-kept lists of the same regions. | Verified (code, jiffy/8MP maps) | closed: superseded by F-254 (D-055). Placement is an allow list, the board's image data regions (`boot.md` 4.3); `overlaps_pb()` and both lists of pb's regions are gone. What pb links outside `pb` and `buffers` is F-390 |
| F-075 | `cm_main.c` `cmd_part_erase`, `struct pb_command_erase_part` | Wire compat (D-008): `start_lba`/`block_count` were taken from `rz` in 2309738 (v2.0.0). A pre-2.0 host sends zeros, so a current device erases 0 blocks and answers OK: a silent no-op instead of a full erase. Fix: `block_count == 0` means the whole partition. Coordinate with F-055. | Verified (code) | closed, accepted (D-035, `command-mode.md` 5.3, `system.md` 5.1): a count of 0 erases nothing and answers OK, as every release since 2.0.0; `cmd_part_erase()` says so |
| F-142 | `src/cm`, `punchboot part read` | `part read` of eMMC BOOT0 or BOOT1 on jiffy fails with `Error: I/O error` (2.8.0, 2026-10-29). Not looked into. | Seen on the board | closed: the same read as F-083, which has the cause |
| F-216 | `src/board.c:122-158` | F-178 is one of three: a failing `init()` or `late_init()` also | Reported | fixed (`e77ef7bf`, `538930ed`) |
| | | returns before `setup_boot_driver()`, so a RAM boot of a rescue image isn't possible when the eMMC | | |
| | | fails. | | |
| | | Fixed: a failed init(), late_init(), RoT or SLC sets up the handover all the same and returns the | | |
| | | step's error. A failed RoT or SLC still fails closed: the life cycle is unknown, command mode asks | | |
| | | for authentication and every boot is refused. | | |
| F-250 | `src/board.c`, `src/board/*/board.c` | `default_status()` and the boards' | Verified (code) | fixed (`5fb27acc`) |
| | | `status()` set `*length` from `snprintf()`, then append the NUL at `buf[*length]`: a truncated text | | |
| | | writes past the buffer. Latent, `CM_BUF_SIZE` is 4 MiB. `board.md` 4.3: the board writes at most | | |
| | | `*length - 1` bytes, the core clamps and appends the NUL | | |
| | | Fixed: the core clamps the length to size - 1 and appends the NUL; jiffy's and the two 8X boards' | | |
| | | status() no longer append one. A buffer of size 0 is -PB_ERR_PARAM (the contract doesn't name the | | |
| | | case). | | |
| F-183 | `Makefile:124-134` | High. `keystore.o` has no prerequisites and `srk_fuse.o` depends | Verified | fixed (`8c4866e2`) |
| | | only on the file: a changed `CONFIG_KEYSTORE`/`CONFIG_SRK_FUSE` isn't regenerated in an existing build | | |
| | | directory (shown with an out-of-tree jiffy build). pb keeps the test keys and the test SRK hash, which | | |
| | | `slc set-configuration` would fuse. `PB_VERSION` has the same gap. Depend on the input file and the | | |
| | | config. | | |
| | | Fixed: a value file per variable in the build directory (CONFIG_KEYSTORE, CONFIG_SRK_FUSE, | | |
| | | PB_VERSION), rewritten only when the value changes; what is made from the variable depends on it. | | |
| | | tests/make/check_rebuild.py. | | |
| F-229 | `Makefile:31` | The result of `genconfig.py` is ignored: a failure builds with | Reported (probe) | fixed (`44e0e198`) |
| | | the old `.config`, a mistyped defconfig name builds the Kconfig defaults, both with exit 0. | | |
| | | Fixed: genconfig.py's result is checked and a KCONFIG_CONFIG that doesn't exist stops the build | | |
| | | (menuconfig, olddefconfig and clean run without one). A missing .config was the Kconfig defaults | | |
| | | before: a change for an out-of-tree board, for Jonas. | | |
| F-143 | `src/drivers/mmc/mmc_core.c:168` | `#ifdef MMC_CORE_DEBUG_IOS` lacks the `CONFIG_` prefix, so the Kconfig option `MMC_CORE_DEBUG_IOS` never has an effect. | Verified (code) | fixed (`f9d542bb`) |
| F-211 | `src/delay.c:33-47` | Low. Timeouts compare with an absolute end tick: if the 32-bit us | Verified | fixed (`bd28f3f8`) |
| | | counter wraps between `ts_end` and the next poll the timeout takes another 71 minutes. Callers ignore | | |
| | | `pb_timeout_init_us()`'s result. Use `(now - start) >= timeout`. | | |
| | | Fixed: a timeout is its start tick and its length, expired when now - start >= length. 0 and | | |
| | | UINT32_MAX are still refused, and the timeout has then expired, for callers that ignore the result. | | |
| F-090 | `src/plat/imx8x` link, imx8dxlevk | `.text` grew past 64 KiB (0x11000 after `plat_get_boot_source`), so `.data` no longer starts on a 64 KiB boundary (ld's aarch64 max page size), and ld puts `.text` through `.bss` in one RWX LOAD segment: `warning: build-imx8dxlevk/pb has a LOAD segment with RWX permissions`. imx8mpevk (2026-10-28) too: `.text` starts at 0x920000 and ends inside the same 64 KiB page as `.data`. Only the ELF headers; `pb.bin` and the MMU setup (linker symbols) are the same. | Verified (readelf) | fixed (`ba45624b`) |
| | | Fixed: -z max-page-size=4096 for the aarch64 link, all four aarch64 boards; pb.bin is byte- | | |
| | | identical. build-firmware.sh fails an aarch64 pb with an RWE segment. The 32-bit links: F-396. | | |
| F-223 | `tools/punchboot/session.py:242` | `_stream_finalize()` never checks the result. | Reported (probe) | fixed (`547d41cb`) |
| | | FINALIZE enables the written eMMC boot partition (F-082): if that fails, `part write` exits 0 and the | | |
| | | board boots the old one. | | |
| F-224 | `tools/punchboot/__main__.py:567` | `boot enable` without its argument sends the | Reported (probe) | fixed (`aaf86dee`) |
| | | null UUID: the device disables boot, exit 0. Make it required. | | |
| | | Fixed: the CLI's boot enable needs its argument (a usage error, nothing is sent). | | |
| | | Session.boot_set_boot_part(None) is unchanged. | | |
| F-225 | `__main__.py`, `session.py`, `partition.py` | The CLI replaces every exception's | Reported (probe) | fixed (`5e860799`) |
| | | message with a fixed string (no udev rule reads "Generic error"). Unknown partition flag bits or SLC | | |
| | | values from a newer device raise `ValueError`, which the CLI doesn't catch. Tracebacks for a missing | | |
| | | file, a binary board response, a non-semver version. | | |
| | | Fixed: the CLI prints the fixed text and the exception's message; OSError and ValueError give a line | | |
| | | and exit 1; PartitionFlags has a member for every bit on the wire (FLAG_RFU4, FLAG_RFU7), so a newer | | |
| | | device's bits are kept. | | |
| F-173 | `src/boot/ab_state.c` | `init` ignores `bio_read()`'s result: a copy that can't | Verified | fixed (`0647e0a7`) |
| | | be read counts as corrupt. When both reads fail and writes work, it writes the defaults | | |
| | | (nothing enabled) over a good state and returns `PB_OK`: the next boot has no active | | |
| | | partition and stays in command mode. Shown on the ramdisk (`fail_reads_after`). Install | | |
| | | the defaults only when both copies were read and are bad, else return the read error. | | |
| | | Fixed: a copy is validated only when it was read; the defaults are installed only when both were | | |
| | | read and are bad; with no good copy and a read that failed, init returns the read error and writes | | |
| | | nothing. One good copy beside an unreadable one is used and repaired, as beside a corrupt one. | | |
| F-175 | `src/boot/ab_state.c` | `boot activate` with an unknown UUID clears `enable`, | Verified | fixed (`b5862c4b`) |
| | | `verified` and `error` in RAM, then returns `-PB_ERR_PART_NOT_BOOTABLE`. For the rest of | | |
| | | command mode `boot status` says none and booting the active slot fails, and the next | | |
| | | commit writes it (on the host board a board register write: nothing to boot after the | | |
| | | reset). `test_set_boot_partition` asserts the RAM change. Return before the change. | | |
| F-176 | `src/boot/ab_state.c` | `init` keeps two valid copies that differ (power lost | Verified | fixed (`17341a73`) |
| | | between the two writes, by pb or `pbstate`). A verified system never commits, so the | | |
| | | stale backup can stay, and it comes back when the primary goes bad (shown: B verified | | |
| | | in the primary, the old trial state in the backup, a bad primary rolls back to A). | | |
| | | Commit when they differ: the primary is always written first. | | |
| | | Fixed: two valid copies that differ get the primary's state. When that can't be written init still | | |
| | | returns PB_OK with the primary's state, as before: F-403. | | |
| F-179 | `src/boot/ab_state.c` | Minor. With both `enable` bits set `get()` boots A, | Verified | fixed (`f524c80b`, `9da04895`) |
| | | `boot_ab_state_get_boot_partition()` reports none and `pbstate` both active (no writer | | |
| | | sets both today; `validate()` could reject bits outside A and B). Normal mode with no | | |
| | | verified fallback logs "Rollback to B system", then fails with `-PB_ERR`. A rollback to | | |
| | | a verified slot fails when its commit fails (read-only eMMC); booting it would be safe, | | |
| | | the next boot picks it again. `boot activate` leaves `remaining_boot_attempts` as it | | |
| | | was. `pb_boot_state_defaults(&boot_state_backup)` does nothing: commit writes only | | |
| | | `boot_state`. `boot.md` 3.2 (D-040): a rollback or an alternation that can't be written is | | |
| | | handed out; a trial whose attempt can't be written only when no verified slot is left. | | |
| | | With speculative mode (F-257) a backup that can't be written means the primary alternates | | |
| | | on every boot and nothing boots (`test_speculative_backup_write_error` pins it) | | |
| | | Fixed: a rollback and speculative mode's alternation are handed out when the write fails; a trial | | |
| | | isn't started while the other slot is verified, which is handed out, mode fallback; with nothing | | |
| | | else to boot the trial is handed out uncounted (boot.md 3.2, D-040). The log says rollback only for | | |
| | | a rollback; the two calls that did nothing are gone. Left as they are, the contract is silent: both | | |
| | | enable bits set, and boot activate leaves the attempts (F-403). | | |
| F-007 | `src/boot/ab_state.c` | If a state partition is missing, it still does I/O with a | Verified | fixed (`84e6a619`) |
| | | negative handle. The B→A rollback branch is not symmetric with A→B. `validate()` | | |
| | | zeroes `crc` as a side effect. `init` sets `cfg` before validating, so a failed init | | |
| | | leaves it half set up. NULL-`cfg` part split out as F-012. Checked 2026-11-09: `bio` | | |
| | | rejects the negative handle (`-PB_ERR_PARAM`) and init then fails on the commit; the | | |
| | | B→A branch ends in the same state as A→B. Read errors: F-173. | | |
| | | Fixed: init does no I/O without both state partitions, and validate() puts the crc back. The B to A | | |
| | | branch was symmetric already; cfg set before validating was F-393. | | |
| F-393 | `src/boot/ab_state.c` | After a failed init the state functions write through | Verified (host) | fixed (`7f219ffb`) |
| | | partitions the init never looked up: `cfg` is set before the lookups and an early return (System A | | |
| | | or B not in the table) leaves `primary_part` and `backup_part` at 0, the first bio device. With the | | |
| | | policy attached, `boot activate A` answered OK and overwrote LBA 0 of the disk (host, table variant | | |
| | | 2). `boot_ab_state_write_board_reg()` reaches the same from board code. Until it is fixed | | |
| | | `board_setup()` detaches the policy after a failed init (F-178). Found with F-178 | | |
| | | Fixed: without its state partitions nothing is read or written: boot activate, next() and a board | | |
| | | register write return -PB_ERR_STATE. With the partitions found and the state unreadable, boot | | |
| | | activate writes a new state and the module works from it. | | |
| F-178 | `src/board.c` | When `boot_ab_state_init()` fails (a repair write, a state partition | Verified | fixed (`eb1ad645`, `4c96d5e9`) |
| | | missing from the table), `board_setup()` returns before `late_init` and the boot | | |
| | | driver: in command mode `boot activate` is `-PB_ERR_NOT_SUPPORTED` and `boot partition` | | |
| | | `-PB_ERR_PARAM`, the commands that would repair it. The GPT failure path above sets up | | |
| | | the driver for that reason. | | |
| | | 2026-11-13 (`eb1ad645`): the handover is set up after a failed policy init, so a boot from RAM | | |
| | | works. The policy is detached until F-393 is fixed: `boot activate` still answers NOT_SUPPORTED. | | |
| | | Left: delete `boot_policy_set(NULL)` in `board_setup()` with F-393 | | |
| | | Fixed: after a failed policy init the handover is set up and the policy stays attached; A/B repairs | | |
| | | or refuses (F-393), the fixed and the chain policy answer -PB_ERR_STATE (read). | | |
| F-182 | `src/drivers/mmc/mmc_core.c:163`, `cm_main.c` | High. `mmc_rw()` uses `blocks = length / | Verified | fixed (`f059c75d`) |
| | | 512` while bio's range check rounds up: a `part write` whose size isn't a block multiple loses its tail | | |
| | | and returns OK; under 512 bytes CMD24 goes out with no data and the card stays in rcv. `part verify` of | | |
| | | such a size hashes stale buffer bytes. The host disk and the ramdisk write exact byte counts, so no test | | |
| | | sees it. In cm: zero-pad the tail and round up (hosts send raw sizes, D-008). | | |
| | | Fixed: the core has a block buffer (CONFIG_BIO_MAX_BLOCK_SIZE, in .no_init); a write's last partial | | |
| | | block is written from it, padded with zeros, a read's is read into it and trimmed. The drivers | | |
| | | refuse a length that isn't whole blocks. tests/unit/conformance/ runs five tests against eight | | |
| | | devices, mmc_core on the card models and the uSDHC model among them. | | |
| F-267 | `src/board/*/board.c`, `src/board.c`, `src/cm/`, `include/pb/cm.h` | Command | Verified (code) | fixed (`cab6dd53` to `3290a752`) |
| | | mode's USB is set up by each board's `cm_init` (clocks or power domains, the PHY, the | | |
| | | controller: SoC work on every board), and the transport is `cm_transport` in `struct board`, | | |
| | | a `struct cm_transport_ops` copied into `cm_config`. D-049, `board.md` 4.1 and 4.2, `platform.md` | | |
| | | 3.10: `plat_cm_init()` at the start of `cm_run()`, the port a Kconfig choice of the platform's; | | |
| | | command mode calls the transport's functions directly, linked to the USB class driver or to the | | |
| | | host platform's socket (today in `src/board/host/board.c`). `cm_init` and `cm_transport` leave | | |
| | | `struct board` | | |
| | | Fixed: plat_cm_init() per platform, the port a Kconfig choice in the defconfig, the six | | |
| | | cm_transport_*() functions linked (the USB class driver, the host's socket); cm_init and | | |
| | | cm_transport left struct board. The set up is the boards' code moved: the compiled code is the same | | |
| | | on jiffy and the RT, and differs on the 8X and the 8M only in where a handle is read and how the | | |
| | | functions are split. One option per port in use only: F-402. | | |
| F-268 | `src/cm/cm_main.c`, `include/pb/cm.h`, `src/board.c`, the link scripts | Command | Verified (code) | fixed (`b46dcf8b`) |
| | | mode's `process` hook (`struct cm_config`, called while it waits for a command) can't be set by a | | |
| | | board since D-026: `cm_board_init()` doesn't copy it and `struct board` has no member. Out-of-tree | | |
| | | boards use it (Jonas). D-051, `board.md` 4.6: `DEFINE_TASK(name, run)` puts a task into a linker | | |
| | | section that every link script keeps; `main()` calls the tasks in turn while pb is stopped, also | | |
| | | without `CONFIG_CM`, in no order, until one returns an error; command mode becomes one of them | | |
| | | (`cm_main_task`: `cm_main.c`'s blocking loop turned into a step); `process` goes. With F-239 | | |
| | | (the link scripts) and F-267 | | |
| | | Fixed: DEFINE_TASK() and the section pb_tasks in both link scripts; main() runs the tasks in turn | | |
| | | while pb is stopped, a kick per round, and resets when one returns an error or there is none; | | |
| | | cm_main_task() is command mode as a step; process is gone. | | |
| F-221 | `tools/pbstate/src/main.c` | `-s` with anything but a/b switches to NONE and | Reported (probe) | fixed (`30c7cf54`, `a20d958f`, `06dfb5f3`, `c949d537`) |
| | | exits 0. `counter` is `uint8_t`: `-c 256` is 0, which means verified. `-w 0 zzz` writes 0. `strtol` | | |
| | | fails from 0x80000000 on 32-bit. Read-only calls end in `sync()`. | | |
| | | Fixed: -s takes a, b or none by its first letter and refuses the rest (exit 22, nothing written); -c | | |
| | | and -w take every 32-bit value; -w refuses a value that isn't a number; a read no longer ends in | | |
| | | sync(). | | |
| F-222 | `tools/pbstate/CMakeLists.txt`, `src/lib/crc.c` | `libpbstate.so` exports a | Reported (probe) | fixed (`97040888`) |
| | | global `crc32` that isn't zlib's: with zlib loaded first every state fails the CRC check. Hide or rename | | |
| | | it. | | |
| | | Fixed: the library's crc.c is compiled with hidden visibility; libpbstate.so.0 exports the 13 | | |
| | | functions of pbstate.h and no crc32. | | |
| F-010 | `tools/pbstate/CMakeLists.txt` | `BACKUP_PART` is set from `PB_STATE_PRIMARY_PART` (copy-paste). The target is listed twice in `install`. | Reported | fixed (`24766ed3`) |
| F-181 | `imx_caam.c`, `cm_main.c`, `image_helpers.c` | High. An async hash job that is | Reported (probe) | fixed (`6d1d3689`, `7d79b150`, `fd266366`) |
| | | abandoned (a read error in `boot_image_load_and_hash()`, a failed `part verify`: no `hash_final`) stays | | |
| | | in the CAAM's one-slot output ring. Every later `caam_wait_for_job()` returns the previous job's | | |
| | | completion, so `caam_ecda_verify()` reports verified from the hash job's status. In a model of the ring, | | |
| | | `auth token` then accepted any well-formed signature on a locked device. Not run on hardware. Same root | | |
| | | as F-127. Drain the ring in `caam_hash_init()` and before a verify, take the status from the ring entry, | | |
| | | finish the hash on every error path. | | |
| | | Fixed: the driver knows whether a job is in the ring; hash_init() and a verification wait for it and | | |
| | | remove its completion first. A completion of another descriptor is removed and the job stays in | | |
| | | flight (found by the review: it used to end the job on paper, and a bad signature then verified in | | |
| | | the model). imx_caam_init() empties the output ring of what an earlier stage left; it does not reset | | |
| | | the job ring. tests/unit/models/caam_model.c and test_caam_crypto.c: the first tests that reach the | | |
| | | CAAM's hash and verify. | | |
| F-127 | `src/drivers/crypto/caam/imx_caam.c` `caam_wait_for_job` | The job status is read from JRSTAR after the output ring entry is removed, not from the entry's status word, and a mismatching descriptor address is logged as "Job failed". JRSTAR only holds the status of the last job that completed, so this works only while one job is in flight (true today). | Verified (SRM, code) | fixed (`6d1d3689`, `8b1f685c`) |
| | | Fixed with F-181: the status is the output ring entry's, with a test in which JRSTAR differs from | | |
| | | it. | | |
| F-008 | `src/drivers/crypto/mbedtls/mbedtls_pb.c` | Return values of the `mbedtls_sha*` calls are ignored. | Reported | fixed (`c362451d`) |
| F-409 | imx8mpevk, the ROM, `pb.imx` | The 8MP EVK doesn't boot pb | Seen on the board (2026-11-13) | closed: not a defect |
| | | `a793102b` from eMMC: after `part write` of `pb.imx` (221696 bytes) to BOOT0, verified, and a reset, | | |
| | | the ROM is in serial download within a second, and again after every reset; nothing reaches the | | |
| | | console. The same file loaded over serial download (`uuu SDPS: boot -f`) runs, verifies and boots | | |
| | | Linux, though the kernel's console output is garbage until the login prompt. BOOT0 verifies against | | |
| | | the file with that pb too, and BOOT_PARTITION_ENABLE reads BOOT0. BOOT1 holds the pb of the morning | | |
| | | (`2d9437b9`, which booted from eMMC) and is not enabled. Not understood. What differs from the | | |
| | | morning: the crypto package, the image's size (its boot data ends at 0x9561c0, the day before at | | |
| | | 0x9551c0), and that the write was the first of a boot partition by a pb with the new block device | | |
| | | core (F-182). The MEK and jiffy were not flashed with it. | | |
| | | Not a defect: the board's boot switches had been changed, so the ROM went to serial download after | | |
| | | every reset (Jonas, 2026-11-13). | | |
| F-235 | `src/main.c` | After a failed `plat_early_init()` or | Verified (code) | fixed (`7ff33375`, `a0758479`) |
| | | `plat_init()` command mode runs without what it needs: no console or tick, no MMU, no fuses, on the | | |
| | | 8M the buffers in untrained DRAM (F-202). `platform.md` 2: reset. The RT's `plat_init()` also | | |
| | | returns `mbedtls_pb_init()`'s error | | |
| | | A failed `plat_early_init()` or `plat_init()` logs and resets; no task, board set-up or boot runs. | | |
| | | The RT's `plat_init()` logs the Mbed TLS provider's error and goes on: without it the boot fails | | |
| | | closed (F-248). | | |
| F-202 | `src/plat/imx8m/plat.c:225`, `main.c:42` | After a failed DDR init command | Reported | fixed (`7ff33375`) |
| | | mode runs with `cmd`, `result`, `buffer` and dwc3's TRBs in untrained DRAM (`.no_init`, | | |
| | | 0x40062000-). Reset on a DDR failure, or keep them in OCRAM. | | |
| | | A failed DRAM training is `plat_init()`'s error, and that resets now (F-235): command mode no longer | | |
| | | runs with its buffers in untrained DRAM. | | |
| F-237 | `include/pb/plat.h` | Also `src/device_uuid.c`: | Verified (code) | fixed (`e53fe4ef`..`1dda0e24`) |
| | | `plat_get_us_tick()` is `unsigned int` in the header, `uint32_t` in three ports. `plat_reset()` | | |
| | | isn't `noreturn`. `platform_ns_uuid` is only picked up with `extern` in `device_uuid.c`, and nothing | | |
| | | says it must never change (`platform.md` 3.6) | | |
| | | `plat.h` has the tick as `uint32_t`, `plat_reset()` noreturn and `platform_ns_uuid`; | | |
| | | `imx_wdog_reset_now()` is noreturn, the 8X's `plat_reset()` waits when the SCFW refuses. | | |
| F-262 | `plat/imx8x/plat.c` | `plat_reset()` before `plat_early_init()` | Verified (code) | fixed (`92c8fefc`) |
| | | returns: the IPC handle is still 0 and `sc_ipc_write()` returns on it. `platform.md` 3.1: valid from | | |
| | | the first instruction after `arch_init()`. The other ports before early init (the WDOG's clock): | | |
| | | unchecked | | |
| | | The 8X's `plat_reset()` opens the IPC itself before `plat_early_init()`. The 6UL, 8M and RT need no | | |
| | | code, by their manuals (`plat-imx.md` 3). | | |
| F-095 | `src/cm/cm_main.c` `cmd_board`, | The device ignores | Verified (code) | fixed (`7c1ba895`) |
| | `tools/punchboot/api_board.c` | `response_buffer_size` and lets the board fill the whole command buffer. When | | |
| | | the response is larger than the host's buffer, the host returns an error without reading the | | |
| | | response, which stays on the link and is read as the next results. 2.x hosts also don't check the | | |
| | | first result (the check is commented out), so they send the request transfer even after an error. | | |
| | | The board gets the smaller of the host's `response_buffer_size` and a buffer, and a larger size it | | |
| | | gives is cut; 0 is 0 (no released host sends 0 and takes more; for Jonas, log 2026-11-13). | | |
| F-365 | `src/cm/cm_main.c` `cmd_part_verify`, `cmd_bpak_read` | Low. Both | Verified (code) | fixed (`36a77b0e`) |
| | | read the partition into the stream's buffers (`buffer[0]`, `buffer[1]`), so a buffer a host filled | | |
| | | with `STREAM_PREPARE_BUFFER` is overwritten, and a `STREAM_WRITE_BUFFER` after them writes the | | |
| | | partition's bytes. `command-mode.md` 5.3: `PART_VERIFY` changes nothing, the stream neither. | | |
| | | Released hosts prepare before every write, so no released flow meets it | | |
| | | `PART_VERIFY` and `PART_BPAK_READ` have two buffers of their own, a quarter of a stream buffer each: | | |
| | | `.no_init` grows by 2 MiB on the boards with 4 MiB buffers, 16 KiB on the RT1060. | | |
| F-364 | `src/cm/cm_main.c` `cm_run()` | Low. A lost that the transport | Verified (code) | fixed (`03b5716a`) |
| | | reports when the read of the next command is started (`usb.md` 4.3: kept for the next `xfer_start`) | | |
| | | makes `cm_run()` leave and pb reset, where `usb.md` 2.4 restarts the transport and waits for a host | | |
| | | (`command-mode.md` 2.2). The session ends either way. Inside a command a failed start ends the | | |
| | | session and the transport is restarted (F-277) | | |
| | | A lost at the start of a command's read restarts the transport in the next round; pb no longer | | |
| | | resets. | | |
| F-074 | `src/cm/cm_main.c` stream read/write | After a failed | Verified (test) | fixed (`8b27c87b`) |
| | | STREAM_INITIALIZE, `block_dev` is an error code, and `bio_get_flags()` of it has the flag bits set, | | |
| | | so the readable/writable checks pass. `stream_check()` then fails on the block size, so it's | | |
| | | harmless. | | |
| | | The stream's read and write check the stream before the flags: `STREAM_NOT_INITIALIZED` where | | |
| | | `NO_MEMORY` was answered. | | |
| F-160 | `src/cm/cm_main.c` `cmd_bpak_read` | Returns | Seen on the RT1060 EVK (2026-11-08) | fixed (`cee66568`) |
| | | `bpak_valid_header()`'s bpak error code to `cm_run()`, which logs it as a pb error | | |
| | | (`Command error -7` for `-BPAK_NOT_SUPPORTED`). The host gets the right `NOT_FOUND`, only the log is | | |
| | | wrong. | | |
| | | `cmd_bpak_read()` returns `-PB_ERR_NOT_FOUND` for a header that isn't valid and logs bpak's code as | | |
| | | bpak's. | | |
| F-192 | `gpt.c:433` | The entry loop stops at the first entry with | Reported (probe) | fixed (`384d960d`) |
| | | `first_lba == 0`: with entry 0 deleted (`sgdisk -d 1`) the later partitions aren't registered and | | |
| | | init returns OK. Skip empty entries. | | |
| | | An empty entry is skipped and the entries after it are made. | | |
| F-193 | `gpt.c:263,446` | LBAs from the disk aren't validated: an entry at | Reported (probe) | fixed (`7c4200e1`) |
| | | LBA 1 becomes a writable partition over the GPT, a 64-bit `last_lba` is truncated, a primary with | | |
| | | MyLBA 5000 makes the repair write there. | | |
| | | 2026-11-13 (`076b7ae4`): `bio_allocate_parent()` refuses a range outside the parent. Left: what | | |
| | | `gpt.c` reads from the disk. A table on a partition would be refused now: are a child's LBAs | | |
| | | relative to its parent or to the root, for Jonas | | |
| | | An entry is a partition only inside the device, in order and off the table's own blocks; one with a | | |
| | | taken UUID is skipped too. The primary is written at LBA 1. Still for Jonas: a table on a partition | | |
| | | (a child's LBAs). | | |
| F-191 | `src/drivers/partition/gpt.c:353,412` | A valid GPT with fewer | Reported (probe) | fixed (`6028a1bd`) |
| | | than 128 entries: the backup array is read at `last-32` and the repair writes 33 blocks. Every boot | | |
| | | rewrites the PMBR and primary, fails on the backup and registers no partitions. | | |
| | | The backup's array is read where its header says, as many blocks as its entries take, and a repair | | |
| | | writes those blocks. | | |
| F-195 | `gpt.c:385,416` | Low. Repairing one copy rewrites PMBR, primary and | Reported | fixed (`a95356ad`) |
| | | backup: a power loss while the good copy is rewritten leaves no table. Write only the bad copy. | | |
| | | A repair writes only the bad copy, at the place the good copy gives; no valid place is `-PB_ERR` | | |
| | | with nothing written (for Jonas, log 2026-11-13). | | |
| F-006 | `src/drivers/partition/gpt.c:~411` | Loop over `p->first_lba` has no | Reported | fixed (`b8ef49dc`) |
| | | 128-entry limit. Primary and backup tables are never compared. | | |
| | | The loop was bounded by `no_of_parts` with F-056 (claude-work). | | |
| | | Two valid copies' entry counts and array CRCs are compared; on a difference the primary is used and | | |
| | | an error logged (for Jonas: or rewrite the backup, or refuse). | | |
| F-064 | `src/drivers/partition/gpt.c:152` | Header `last_lba` (last usable) | Verified (code) | fixed (`6bd219a8`) |
| | | is `last - 32`, which is the first backup-entry block; should be `last - 33`. pb's own allocation is | | |
| | | unaffected; other tools could overlap the backup array. | | |
| | | An install writes last usable = last - 33; a release's table is used as it is, and a release uses | | |
| | | the new one (tried with both pb-host binaries). | | |
| F-252 | `src/cm/cm_main.c` | A stop in a boot command mode started: `-PB_ERR_ABORT` | Verified (code) | fixed (`132f13fc`, `d804cbf3`) |
| | | is command mode's "leave and reset" (`device reset`), so a hook's stop in `boot part` or | | |
| | | `boot bpak` resets the board (the 8X's button, which stops these boots on purpose). After the | | |
| | | transport's disconnect, a failed `late_boot` or `plat_lockdown()` other than ABORT returns to the | | |
| | | loop, which reads from the disconnected transport (unchecked). `board.md` 5.4: before the | | |
| | | disconnect the host gets the error and command mode goes on, after it pb resets | | |
| | | A stop or a failure before the disconnect is the host's result and command mode goes on; a boot that | | |
| | | returns after the disconnect, whatever it returns, leaves command mode and pb resets. A flag set by | | |
| | | DEVICE_RESET, a board's reboot on PB_OK and the boot commands' end replaces the test for | | |
| | | `-PB_ERR_ABORT`. Not run on a board (the MEK's button) | | |
| F-399 | `src/cm/Kconfig`, `cm_main.c` | Low. `CONFIG_CM_TRANSPORT_READY_TIMEOUT` has no range, | Reported | fixed (`5d04d0ec`) |
| | | and its `* 1000000L` overflows a 32-bit long above 2147 s | | |
| | | The range is 1 to 4294 s, the product is unsigned and a static assert holds it. 0 expired at once, | | |
| | | above 4294 s the microseconds wrapped | | |
| F-415 | `src/cm/cm_main.c` `pb_command_parse()` | A board command that asked | Verified (test) | fixed (`c0f87447`) |
| | | for the reboot and then failed left the request standing: the next command that returned PB_OK | | |
| | | reset the board, a frame the gate refused included. Found with F-252. The request is dropped | | |
| | | with the command that failed (`command-mode.md` 5.6: only when that command returned PB_OK) | | |
| F-238 | `plat/{imx8m,imxrt}/plat.c` | The tick and `log_time_start()` are set up in | Verified (code) | fixed (`fc034837`, `0f5e3d27`) |
| | | `plat_init()`, after `board.console_init()`: before it there is no `pb_delay_*()` and the log time is 0. | | |
| | | The 6UL and the 8X do it in `plat_early_init()` | | |
| | | The 8M starts the tick and the log time last in `plat_early_init()`, same values. The RT runs GPT1 | | |
| | | from the 24 MHz crystal (`imx_gpt_init_24m()`, CR 0x743, PR 0x7002) from `plat_early_init()`. | | |
| | | Neither ran on a board: the RT's tick rate is to be checked against a clock, and `0f5e3d27` reverts | | |
| | | alone | | |
| F-236 | `plat_boot_reason` | `src/plat/*/plat.c`: No common meaning: the 6UL's own enum, | Verified (code) | fixed (`8917bb88`..`2ac77cc8`) |
| | | the 8X's SCFW value, the RT's raw WRSR bitmask (several bits set: "?"), the 8M not implemented, the host 0. | | |
| | | `platform.md` 3: `plat_get_boot_reason()` with a kind, the raw value and a name | | |
| | | `plat_get_boot_reason()` with a kind, the raw value and a name on every port and the host; | | |
| | | `plat_boot_reason()` and `plat_boot_reason_str()` are gone. No port writes a status register. The | | |
| | | banner is `PB <ver>, <name> (0x<raw>)`. The mappings chosen where the contract is silent are in the | | |
| | | log (2026-11-13), for Jonas. Not run on a board | | |
| F-076 | `src/boot/image_helpers.c` (payload hash and load) | bpak compat (D-008): punchboot hashes every part, while upstream `verify.c` skips parts with `BPAK_FLAG_EXCLUDE_FROM_HASH` (`bpak add --set-flag dont-hash`). Such images verify with the bpak CLI but fail to boot in punchboot. Supporting the flag only accepts more images, so it is a compatible fix. | Verified (code) | fixed (`ec32bacf`) |
| | | Decided (D-035, `security.md` 4.2): an image with a `dont-hash` part is refused, before the | | |
| | | load, with its own error. Not loaded unhashed as upstream does, and not skipped | | |
| | | An image with a `dont-hash` part is refused before anything is loaded, with `-PB_ERR_NOT_SUPPORTED` | | |
| | | and a log line that names the part, for an image that is loaded and for one in place. Such an image | | |
| | | never booted: every release hashed the part and the payload hash failed after the load (shown with | | |
| | | the golden 0.9.3 and 1.0.1 images) | | |
| F-346 | `src/rot.c` | `rot_no_of_keys()` returns its error, | Verified (code) | fixed (`5d5cc8b9`) |
| | | `-PB_ERR_NOT_SUPPORTED` as a `size_t` when the RoT isn't set up, and `rot_key_idx_to_id()` an error | | |
| | | as a `key_id_t`: a caller can't tell them from a count or an id. A loop over the count runs to | | |
| | | `SIZE_MAX` (`dtb.c` had one; unreachable, no image verifies without a RoT). `boot.c` | | |
| | | (`collect_info()`, F-260) takes a count above 255, the most a keystore holds, for the error and | | |
| | | stops the boot. An `int` with the count or the id as an out argument, or 0 keys without a RoT | | |
| | | `int rot_no_of_keys(size_t *count)` and `int rot_key_idx_to_id(unsigned int index, key_id_t *id)`; | | |
| | | `collect_info()` returns the RoT's own error, `cmd_slc_read()` without a RoT sends the same | | |
| | | transfers as before | | |
| F-404 | `src/boot/ab_state.c` | A repair of the backup alone (corrupt, unreadable, or the resync | Reported | fixed (`b272b2a9`) |
| | | of F-176) goes through the commit that writes both copies: power lost while the good primary is | | |
| | | rewritten leaves a torn primary beside a stale backup. Write only the backup. Read, not run | | |
| | | The three repairs of the backup write the backup only (the fake counts the writes per block). A | | |
| | | repair of the primary alone still goes through the commit and rewrites the good backup second | | |
| F-326 | `include/boot/boot.h`, `include/pb/board.h`, `src/board/imxrt1060evk/board.c` | bpak | Verified (code) | fixed (`fd9ce4bb`) |
| | | is still named where nothing of it is used, since F-266: `boot/boot.h` includes | | |
| | | `<bpak/bpak.h>`, so every board that includes it for `boot_get_flags()` gets the format's | | |
| | | header (D-047: a board that doesn't ask for the header doesn't include it); `pb/board.h` | | |
| | | forward declares `struct bpak_header`; the RT's board includes `<bpak/bpak.h>` and | | |
| | | `<boot/image_helpers.h>`. Taking the include out of `boot.h` needs a look at who relied on it | | |
| | | `boot/boot.h` no longer includes `<bpak/bpak.h>`, `pb/board.h` has no forward declaration, the RT | | |
| | | board no bpak include; `test_board.c` doesn't compile if either header brings it back | | |
| F-220 | security core, low | `caam_hash_input()` refills the alignment buffer while its async | Reported | fixed (`997f6a8a`, `58d39d9d`, `5d5cc8b9`, `4cd64a20`) |
| | | job may read it (wrong digest, fails closed). `rot.c:117,134` return error codes through unsigned types: | | |
| | | `slc read` loops ~2^32 times after a failed `rot_init()`. Parts are 512-aligned, a 4 KiB block device | | |
| | | fails mid-load on a second part. `mbedtls_pb.c:183` skips `mbedtls_pk_free()`. `slc_set_configuration()` | | |
| | | goes on when the state read is NOT_SUPPORTED. D-068 (Jonas): no read at a byte offset in `bio` for | | |
| | | now, so the 4 KiB case stays a limit (`bio.md` 3.1, `roadmap.md`) | | |
| | | 2026-11-13 (`997f6a8a`, `58d39d9d`): the CAAM's block buffer isn't refilled under its job, and the | | |
| | | key's context is freed when it doesn't parse (nothing leaked with Mbed TLS 3.4.0). Left: `rot.c`'s | | |
| | | error codes through unsigned types, `slc_set_configuration()` on NOT_SUPPORTED, the 4 KiB limit | | |
| | | (D-068) | | |
| | | 2026-11-13: `rot.c`'s codes are F-346's, and `slc_set_configuration()` returns any error of the | | |
| | | state's read, also NOT_SUPPORTED (a board with fuses and no `read_status` now gets it from `slc | | |
| | | configure`; none in the tree or in v2.7.1). The 4 KiB case stays a limit (D-068, `bio.md` 3.1, | | |
| | | `roadmap.md`) | | |
| F-398 | `Makefile` | Nothing depends on the flags: a changed `cflags` or `ldflags` | Verified (build) | fixed (`b4081b15`) |
| | | (`CONFIG_OPTIMIZE` on the command line, an edited `makefile.mk`) neither compiles nor links again in | | |
| | | an existing build directory, shown with F-090's flag. `srk_fuse.o` doesn't depend on `scripts/gen- | | |
| | | srk-fuse.py` (read). The value files of F-183 are the way | | |
| | | A changed compiler or linker flag, link script (from the `-T` flags) or `gen-srk-fuse.py` builds pb | | |
| | | again; six new cases in `make.rebuild` | | |
| F-407 | `src/drivers/usb/imx_ci_udc.c:38`, `CMakeLists.txt` | Low. A build without | Verified (build) | fixed (`0f3ca672`, `c90e303d`) |
| | | `CONFIG_CM` doesn't compile while `DRIVER_IMX_CI_UDC` is on, its default: the driver uses | | |
| | | `CONFIG_CM_BUF_SIZE_KiB` (the 6UL, the RT, the DXL). On the host `pb_core` is a static library, so a | | |
| | | task in a file nothing else references isn't linked into pb-host (read). From F-268 | | |
| | | `DRIVER_IMX_CI_UDC` depends on `CM`; `pb_core` is an object library, so every file of the core is in | | |
| | | pb-host (`policy_chain.c` and `policy_fixed.c` were not), checked by `make.host_link` | | |
| F-396 | `src/link.lds`, the armv7 links | Low. jiffy's and the RT1060's ELF have | Verified (readelf) | fixed (`03dd1de6`) |
| | | everything in one RWE LOAD segment, as the aarch64 boards had (F-090); arm-none-eabi's ld doesn't | | |
| | | warn. Only the ELF's headers | | |
| | | No RWE LOAD segment in jiffy's and the RT1060's ELF; `pb.bin`, the symbols and the sections are the | | |
| | | same before and after. `build-firmware.sh` checks every board. The RT1060's first segment still | | |
| | | holds the ELF's headers (F-418) | | |
| F-400 | `scripts/pbdev`, `scripts/ci/host-test.sh` | Low. Two `scripts/pbdev test` runs at | Verified (run) | fixed (`17fe70cf`) |
| | | once in one tree fail: both install the package through setuptools' `build/` directory and the wheel | | |
| | | builds collide ("Failed building wheel for punchboot"). The default and the sanitizer run must go | | |
| | | one after the other | | |
| | | Each `BUILD_DIR` builds the Python package in a directory of its own (`DIST_EXTRA_CONFIG`): the | | |
| | | default and the sanitizer run pass side by side | | |
| F-242 | `src/arch` | `src/arch/armv8a/Kconfig`, `src/arch/{armv7a,armv8a}/asm_macros.S`: | Reported | fixed (`dd7acc8d`, `11e492fa`) |
| | | Leftovers: the Kconfig help mentions sunxi FEL (no sunxi platform), a TF-A stack macro calls | | |
| | | `plat_my_core_pos`, which no platform defines (the macro seems unused) | | |
| | | The sunxi sentence and the unused `get_my_mp_stack` are gone (`get_up_stack` is unused too, F-418) | | |
| F-089 | `src/lib/libc/snprintf.c` (vendored) `unsigned_num_print` | `%x` writes the digits and padding without checking the buffer size `n`, so `snprintf` with `%x` can overflow a small buffer (`%d`/`%u`/`%s` are bounded). Unsupported specifiers (`%zu`, `%2u`, `%c`, ...) spin forever instead of failing. | Verified (code) | fixed (`c6744cc1`, `0210d1f3`) |
| | | `%x` and its padding stay inside the buffer; an unsupported letter logs an error and ends the output | | |
| | | there, terminated, where pb hung. `test_snprintf.c`. `pb.bin` changes on all six boards, in | | |
| | | `snprintf` only | | |
| F-199 | `src/drivers/mmc/imx_usdhc.c:338` | A command timeout (CTOE) returns | Reported (probe) | fixed (`e4513fd4`) |
| | | `-PB_ERR_IO`; `mmc_sd.c:281` detects SD 1.x cards by `-PB_ERR_TIMEOUT` on CMD8, so a 1.x card fails init | | |
| | | on uSDHC. `test_sdsc_v1` runs only on the HAL model. | | |
| | | A command timeout alone is `-PB_ERR_TIMEOUT` on the uSDHC, with a CMD line conflict it stays | | |
| | | `-PB_ERR_IO`; `test_sdsc_v1` runs through the uSDHC's register model. On the wire a command that | | |
| | | times out in command mode answers TIMEOUT where it answered IO_ERROR | | |
| F-116 | `src/drivers/mmc/mmc_core.c` `mmc_setup` (HS400) | HS400 without Enhanced Strobe selects HS_TIMING 3 without tuning in HS200 first (JESD84-B51 6.6.2.3 steps 5-6). With `MMC_BUS_WIDTH_8BIT_DDR_STROBE` the card is switched to BUS_WIDTH 0x86, then `imx_usdhc_set_bus_width()` rejects the width. No board uses HS400. | Verified (bench, `test_mmc_emmc` `test_hs400`) | fixed (`380df216`, `ec7a7139`) |
| | | `struct mmc_hal` has `hs400` and `hs400_es`; the uSDHC sets neither, so HS400 is refused there | | |
| | | before a switch. HS400 without Enhanced Strobe tunes in HS200 first and falls back to high speed. No | | |
| | | board uses HS400 | | |
| F-285 | `src/drivers/mmc/` | What the MMC drivers leave behind isn't written | Reported (read) | fixed (`93a666fd`, `8bf54469`) |
| | | down. PARTITION_ACCESS stays on the last partition accessed; the bus stays in the board's | | |
| | | mode (8-bit DDR52 on jiffy, HS200 on the 8M and 8X boards); BOOT_BUS_CONDITIONS is rewritten | | |
| | | at init unless `MMC_CFG_FLAG_KEEP_BOOT_BUS_CONDITIONS`; a write to a boot partition moves | | |
| | | BOOT_PARTITION_ENABLE off it first. `mmc_power_off()` has no caller. `bio.md` 5: in the | | |
| | | header, with what brings the card back for the ROM after a warm reset (F-078, F-079) | | |
| | | What the MMC drivers leave behind and change on the card for good is in `mmc_core.h` and | | |
| | | `imx_usdhc.h` | | |
| F-196 | `src/drivers/fuse/imx_ocotp.c:73` | The fuse timing is hard-coded for a 66 MHz ipg_clk | Reported | fixed (`3e81bf4f`) |
| | | (STROBE_PROG 0x299). The RT1060 uses the driver at 75-200 MHz (F-148): a 3-9 us program strobe instead | | |
| | | of 10 us on an irreversible write. Checked against both RMs, not on hardware. Compute TIMING from the | | |
| | | ipg rate. | | |
| | | `imx_ocotp_init()` takes the ipg rate and TIMING is the reset value scaled to it, rounded up; at 66 | | |
| | | MHz the register stays 0x01461299 (pinned). The 6UL passes 66 MHz. The 8M and the RT pass 0: fuses | | |
| | | are read and a write is `-PB_ERR_NOT_SUPPORTED` (the RT needs one line after its clock set-up, the | | |
| | | 8M's TIMING layout is unchecked; for Jonas). No manual on this machine: the RM sections are the | | |
| | | row's | | |
| F-197 | `imx_ocotp.c:69,91-104,121` | Against the 6UL RM: ERROR is cleared by a plain CTRL | Reported | fixed (`a36e1476`, `b4f32ce1`, `20dc8238`, `945d3a46`, `67f68a05`) |
| | | write, not CTRL_CLR (35.5.1), so after one failed access everything is `-PB_ERR_IO` until reset; no 2 us | | |
| | | postamble after a write (35.3.1.4); the index is masked with 0x7F, not checked; a read doesn't check | | |
| | | ERROR (a locked word reads 0xBADABADA with OK). | | |
| | | ERROR is cleared through CTRL_CLR, a write waits 2 us after BUSY, an index CTRL can't hold is | | |
| | | refused, a read checks ERROR (a locked word is `-PB_ERR_IO`), and the waits for BUSY end after 10 | | |
| | | ms. `test_imx_ocotp` runs against a model of the controller | | |
| F-198 | `src/drivers/memc/imx_flexspi.c` | The erase starts with the largest block without | Reported | fixed (`aaa95721`, `152974cd`, `5ff643ba`, `84ca3e2a`) |
| | | checking the address is aligned to it, rounds a short tail up to a full block, ignores write-enable and | | |
| | | busy-wait results. Latent with the EVK's layout. IDATSZ is 16 bits: a read of 64 KiB or more is | | |
| | | truncated and waited for forever. No loop has a timeout or a watchdog kick. | | |
| | | Every wait is bounded, write enable and the busy polls are checked, an erase covers exactly the | | |
| | | blocks asked for, a read goes in 32 KiB pieces. `test_imx_flexspi`: a NOR modelled behind the | | |
| | | controller's registers. Not run on the RT1060 | | |
| F-284 | `src/drivers/memc/imx_flexspi.c` | A write copies `size / 4 + 1` | Reported (read) | fixed (`aae460e1`, `8f40fb42`, `18a035d2`) |
| | | words into the FIFO in its last round: 1 to 4 bytes past the caller's buffer (whether they | | |
| | | reach the flash is unverified, the data size is the exact length). A read whose length isn't | | |
| | | a multiple of 4 is refused with `-PB_ERR_INVALID_ARGUMENT`, after the command was triggered. | | |
| | | The buffer is read and written as `uint32_t *` whatever its alignment. The busy wait's | | |
| | | result is ignored after a read and after each erase. No host test (F-230). With F-198. | | |
| | | `bio.md` 3. The write's tail is verified in the code, the rest is as reported | | |
| | | A write reads no byte past the buffer, a read takes any length, the buffer may have any alignment, | | |
| | | the codes are `drivers.md` 3's | | |
| F-004 | `src/board/imx8{qxmek,dxlevk}/board.c:~288` | `strncmp(expected, password, length)` with `length` taken from the host accepts any prefix ("i"). `LOG_DBG` prints a password that is not NUL-terminated. Example boards, but the pattern gets copied. | Verified | fixed (`1b4fbf6b`, `0b6c7ff1`) |
| | | Both 8X boards compare the whole password in constant time through `board_password_check()` | | |
| | | (`src/board/password.c`) and log nothing; a prefix or extra bytes are AUTHENTICATION_FAILED, the | | |
| | | password as released hosts send it (no NUL) still works. Not run on a board | | |
| F-212 | `src/board/imx8dxlevk` | The DXL board offers `slc lock`, but its build can't sign pb | Reported | fixed (`52e665d0`) |
| | | (`imx8xl_sign_image`: not supported): a locked DXL rejects the only image there is. Drop | | |
| | | `set_configuration_locked` until signing exists. | | |
| | | The DXL board has no `set_configuration_locked`: `slc lock` answers NOT_SUPPORTED and closes nothing | | |
| F-395 | `src/board/{imx8mevk,imxrt1060evk}/board.c` | Low. imx8mevk's `board_command()` appends | Reported | fixed (`ce19d2c6`, `8bc79131`) |
| | | a NUL at `response[*response_size]` after `snprintf()`, the pattern of F-250 in a board command | | |
| | | (latent with a 4 MiB buffer; `board.md` 4.4 has no clamp rule for a command's response). The RT's | | |
| | | `board_command()` has an unused variable `rc` (line 350), the one warning of its build | | |
| | | imx8mevk's NUL stays in the room the core gives; the RT's unused `rc` is gone, its build has no | | |
| | | warning of pb's own left | | |
| F-270 | `src/board/{imx8qxmek,imx8dxlevk}/board.c`, `include/plat/imx8x/imx8x.h` | The | Verified (code) | fixed (`ab960b07`) |
| | | two 8X boards include the arch's headers (`arch/arch_helpers.h`, `arch/armv8a/timer.h`) and the system | | |
| | | controller's API by its own path (`sci/svc/{misc,pad,pm,seco}/api.h`, found through the | | |
| | | platform's `-Iinclude/plat/imx8x/`). `imx8x.h` includes only `sci/ipc.h` and the pad service. | | |
| | | D-056, `platform.md` 7: the API stays with the platform; `imx8x.h` includes the services boards | | |
| | | use (pm, misc, seco, pad), the boards include `imx8x.h` only, and the arch comes through | | |
| | | `include/pb/arch.h`. Out-of-tree 8X boards change their includes, not their calls | | |
| | | `imx8x.h` includes the misc, pm and seco services and the two boards include only it; the MEK's and | | |
| | | the DXL's `pb.bin` are byte for byte the same | | |
| F-408 | `tools/pbstate`, `testbench/simple/src/pbstate.c` | Low, from F-180 and F-221. pbstate | Reported | fixed (`0cfd2c81`..`d2787c45`) |
| | | accepts trailing characters after a number (`-c 3x`, `-w 0 1zz`); `print_configuration()` ignores | | |
| | | the getters' errors and prints both systems as disabled on a CRC failure before it fails; `-v` with | | |
| | | anything but a or b exits 1, not EINVAL; `-c` without `-s` is ignored. The simple target's copy | | |
| | | still zeroes the attempts whichever slot it confirms, where pbstate now keeps the trial when the | | |
| | | other slot is confirmed, and so does the helper `pbstate_confirms()` in `test_ab_state.c` | | |
| | | pbstate refuses a number with characters after it, `-v` with anything but a or b and `-c` without | | |
| | | `-s` exit EINVAL, `-i` prints nothing of a state it couldn't read; the simple target's copy and the | | |
| | | test helper keep the trial when the other slot is confirmed. Valid invocations keep their output and | | |
| | | exit code | | |
| F-401 | `tools/punchboot/session.py` | Low. `part_erase` masks `start_lba` to 32 bits, so a | Reported | fixed (`5acd3b49`) |
| | | partition past block 2^32 wraps (pb 3.0.0 refuses the range, older devices unchecked). After a | | |
| | | host's timeout on USB a device before 3.0.0 can still hold the late result, which the next session | | |
| | | reads as its first answer (read, not tried). `pyproject.toml` says Python 3.10 and up; only 3.13 is | | |
| | | tested | | |
| | | `part_erase` refuses a chunk that would start past LBA 2^32 - 1 before anything is sent (the wire's | | |
| | | field is 32 bits). Left as notes: a late result after a timeout on a device before 3.0.0, and that | | |
| | | only Python 3.13 is tested | | |
| F-405 | `include/boot/policy.h`, `boot.c`, `ab_state.h`, `tools/pbstate` | Low. Comments that no | Reported | fixed (`8612b37a`, `2b092649`) |
| | | longer hold: `policy.h` and `boot.c` say next() can return the error of a state write that failed | | |
| | | (A/B no longer does), `ab_state.h` lists errors `boot_ab_state_set_boot_partition()` doesn't return. | | |
| | | pbstate returns a primary's read error without trying the backup, where pb uses a good backup; it | | |
| | | reads the backup only when the primary is corrupt | | |
| | | 2026-11-13 (`8612b37a`): the comments in `policy.h`, `boot.c` and `ab_state.h` are corrected. Left: | | |
| | | pbstate and a primary's read error | | |
| | | The comments are corrected, and pbstate uses the backup when the primary can't be read; a write | | |
| | | repairs the primary | | |
| F-201 | `src/drivers/usb/usbd.c:28` | `enumerated` is never cleared: on the 8MP EVK a replug or | Reported | fixed (`be1e1841`) |
| | | bus reset ends in a read on an unconfigured EP2, `cm_run` exits and the board resets. Clear it in | | |
| | | `usbd_init()` and on bus reset. | | |
| | | `enumerated` is cleared in `usbd_init()`, `usbd_disconnect()` and on every error of | | |
| | | `usbd_xfer_complete()`: a replug inside a session enumerates again. Not run on a board | | |
| F-184 | `src/drivers/usb/cdns3_udc_core.c:263` | The clean covers `desc_count` TRBs, not the | Verified | fixed (`0bb7128e`) |
| | | terminating one at `trb[desc_count]`: for some counts its flags word is in a cache line that isn't | | |
| | | cleaned, and a stale TRB from a larger transfer stays in memory. The effect on the controller is | | |
| | | unverified; it fits F-172 (a small write after a large one). Clean `desc_count + 1`. | | |
| | | The clean covers the terminating TRB. `test_cdns3_udc` fails at 5 TRBs without it; on the MEK a full | | |
| | | 4 MiB chunk (33 TRBs) was covered by chance, last chunks of 5, 10, 16, 21 TRBs were not. Whether it | | |
| | | is F-172's cause is for the bench | | |
| F-134 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_xfer_complete` | For an OUT transfer into a buffer that isn't 4 KiB aligned, the data is copied from `align_buffer` into the buffer and then the buffer is invalidated (DCIMVAC), which architecturally discards the copy while the lines are dirty. Every command read takes this path (`cmd` at 0x81826200 in the jiffy build), yet commands work on jiffy, and the D-cache does hold dirty lines there (the F-130 breadcrumbs in `.no_init` were lost on reset without a clean, log 2026-10-29). Not understood: perhaps the Cortex-A7 performs DCIMVAC as clean and invalidate (unchecked; that would also hide F-081 on jiffy). | Verified (code, symbol map); why it works is open | fixed (`7c03e5c8`) |
| | | The caller's buffer is invalidated before the copy from the bounce buffer; the two blocks changed | | |
| | | places, nothing else. Why it worked on jiffy before is still not known | | |
| F-131 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_xfer_start` | For a buffer that isn't 4 KiB aligned, page 0 of the dTD is `align_buffer` at offset 0 with only `align_length` bytes valid, but the controller fills page 0 up to its 4 KiB boundary before page 1, so an unaligned transfer longer than `align_length` sends or receives the wrong bytes. pb's large buffers are aligned, the small ones fit in one page, so nothing hits it today. | Verified (code) | fixed (`7c03e5c8`) |
| | | Page 0 of the descriptor is the bounce buffer at the head's offset, `align_length` is used once | | |
| | | (F-205's item), and the first descriptor's count is what its pages hold. A transfer of more than a | | |
| | | page into a buffer not on a page failed and now works; nothing that runs today changes | | |
| F-397 | `mmc_emmc.c:362`, `dwc3_udc.c:359,651` | Low. At `CONFIG_LOGLEVEL` 0 or 1 | Verified (build) | fixed (`bd2df79f`, `fc10e2a0`) |
| | | without the RAM log, variables that only a log line uses are unused (`start`, `speed`, `caplength`): | | |
| | | warnings in the imx8mpevk build. Seen with F-109 | | |
| | | 2026-11-13 (`bd2df79f`): the MMC line is done, with a test that compiles the MMC sources at log | | |
| | | level 0. Left: `dwc3_udc.c` (the USB package) | | |
| | | The MMC's and the DWC3's variables that only a log line uses | | |
| F-087 | `src/cm/cm_main.c` `error_to_wire` | `-PB_ERR_BAD_PAYLOAD` (payload hash mismatch) has no wire result, so the host reports "Generic error". `include/pb-tools/error.h` has no payload code. | Seen on jiffy (2026-10-22) | fixed (`3be3bece`) |
| | | `PB_RESULT_BAD_PAYLOAD` (18) is appended before the sentinel and `abi.txt` grew by its line; the | | |
| | | host has `BadPayloadError`, a `GenericError`. A v2.7.1 host (the same at v2.0.0) takes any code from | | |
| | | `PB_RESULT_END` up as a generic error, as it reported this case before; the 1.x host isn't in the | | |
| | | tree and wasn't read. Only a corrupt image's `boot part` and `boot bpak` answer differently | | |
| F-194 | `gpt.c:296`, `src/bio.c` | Installing a table from command mode doesn't update | Reported (probe) | fixed (`6cb29af9`) |
| | | the bio pool: until a reset the old partitions are listed and written at their old LBAs. Not documented | | |
| | | either. D-070, `bio.md` 4: in effect at once. An unchanged partition (UUID and blocks) keeps its | | |
| | | handle and flags, a changed one's handle is dead and its slot unused until the reset, a new one | | |
| | | gets a new slot; the stream ends and the policy's init runs again; no board hook runs again. For | | |
| | | the code: `bio_get_part_by_uu()` and `cmd_part_tbl_read()` stop at the first slot that isn't | | |
| | | valid; the pool needs room for both tables' partitions | | |
| | | An installed table is in effect at once: an unchanged partition keeps its handle and flags, a | | |
| | | changed or removed one's handle is dead and its slot unused until the reset, a new one gets a new | | |
| | | slot; the stream ends and the policy's init runs again; no board hook runs. A full pool answers | | |
| | | `-PB_ERR_MEM` with the table on the device. The same bytes on the medium and on the wire as before. | | |
| | | Not run on a board | | |

## Dependencies / vendored code

| ID | Finding | Status |
|---|---|---|
| F-021 | Vendored bpak is ~v0.9.0, the CI CLI is v0.9.3, and there is no sync procedure. | fixed 2026-11-08: both 1.0.1, `scripts/sync-bpak` (D-024) |

## Dead code / cleanup

| ID | Finding | Status |
|---|---|---|
| F-034 | Bash tests that exist but don't run: `test_config`, `test_invalid_key_index2`, `test_setup_lock`, `test_bpak_show`, `test_part_offset_write`. They use commands and tools that no longer exist. | removed with the bash suite |
| F-146 | `src/boot/atf.c` (+ `atf-interface/`, ~1,050 lines) is never compiled: no defconfig sets `BOOT_ATF`. Its `/chosen` patching duplicates `linux.c` (with F-071). Keep for an 8MP TF-A handover, or remove. | closed: kept, the 8MP EVK boots with it (`ac10a592`), warnings fixed (`8ceeea5a`); the duplicated `/chosen` code waits for Jonas' boot review |
| F-031 | Stale `-I src/libc/include/...` in the armv7a/armv8a makefiles. The gcovr excludes still name `bearssl` and `src/fdt`. | fixed (`45a13b44`): the three -I paths; the gcovr excludes were gone since `5e336062` |
| F-030 | libc files never compiled: `memcpy.c`, `strncmp.c`, `strlcpy.c`, `strnlen.c`, `strrchr.c`. Also `xlat_tables_v2/aarch32/nonlpae_tables.c`. | fixed (`6a2828b7`) |
| | The six files are removed; no board and no host build compiled one | |
| F-032 | Deprecated commands still handled: `PART_TBL_INSTALL`, `PART_RESIZE`, `AUTH_SET_PASSWORD`. `pb_wire_command_string`/`pb_wire_slc_string` are stubs. Corrected 2026-11-12 (`command-mode.md` 5.3, 5.8): only `PART_RESIZE` is retired. `PART_TBL_INSTALL` is in use (`board.md` 2.4), and `AUTH_SET_PASSWORD` was never implemented and is to be (`roadmap.md`). | closed (`17a3be7c`) |
| | Left after the correction: `PART_RESIZE` still has its case in `cm_main.c`, and the two string | |
| | functions in `src/wire.c` return "" | |
| | What was left is what the contract has: `PART_RESIZE` keeps its case and answers NOT_SUPPORTED | |
| | behind the gate (`command-mode.md` 5.8), the comments say so now; nothing calls | |
| | `pb_wire_command_string()` and `pb_wire_slc_string()`, which have returned "" since 1.0.0 and are | |
| | left | |

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
| F-051 | Most boards don't build: imx8qxmek uses an old SCI API (`sci_seco_api.h`, `sc_pm_*`, `sc_pad_set`); imx8dxlevk needs a `keystore.bpak` that isn't in the repo (it builds with the internal keystore); imx8mevk needs proprietary DDR blobs; imxrt1060evk had an implicit `ts_print` (gone with the timestamps, 2026-10-29: it builds, with four unused-code warnings in `board.c`, and runs on the EVK). Only the QEMU board was ever built in CI. Verified. | fixed: all six boards build (the MEK since `259d1564`), and `build.yml` builds each of them. Which boards to keep is F-033 |
| F-109 | `include/pb/log_def.h`: a disabled LOG macro expands to nothing, so `else LOG_DBG(...);` is an empty body and warns (`-Wempty-body`) at `CONFIG_LOGLEVEL=1` (seen in `src/self_test.c`, since removed). Expanding to `do { } while (0)` fixes it. Verified (build). | fixed (`f1287f69`) |
| F-147 | `Makefile` `menuconfig oldconfig:` runs `scripts/$@.py`, but `scripts/oldconfig.py` doesn't exist (there is `olddefconfig.py`, unreachable); `.PHONY` names `olddefconfig` and `keystore`, which have no rules. `README.md:208` puts `createtoken.sh` in "the tools folder" (it's `scripts/`) and passes `-sha256` (the script adds the `-`). `build.yml:75` gives `upload-artifact` a `fail_ci_if_error` left from Codecov. | fixed (`aac1cc22`): `make olddefconfig` runs its script; `make oldconfig` still has none, for Jonas |
