# Decisions

Short records of decisions and why they were made. Add new ones at the bottom,
and never edit old ones; supersede them with a new entry instead.

## Index

Which entries still hold. This table is the one part of the file that is edited (D-075): an
entry that replaces, narrows or closes an older one changes the older one's row with it.

- **In force**: holds as written. What follows a semicolon is what to know with it.
- **Narrowed by**: holds, except where the named entries say otherwise.
- **Replaced by**: no longer holds.
- **Closed by**: a draft, or a step of a review. The named entries decided what it left open,
  and not always as it proposed.
- **Waits**: made without Jonas, not confirmed.

| ID    | Decision                                         | State                                              |
| ----- | ------------------------------------------------ | -------------------------------------------------- |
| D-001 | bpak format unchanged "for now"                  | Replaced by D-008                                  |
| D-002 | AI working context in `doc/dev/`                 | Narrowed by D-037, D-042                           |
| D-003 | Release flow                                     | In force                                           |
| D-004 | Host tests replace the QEMU suite                | In force                                           |
| D-005 | Tooling for the host tests                       | In force                                           |
| D-006 | CI runs the host tests; QEMU suite removed       | In force                                           |
| D-007 | Unit tests fake neighbours; no bug as a pass     | In force                                           |
| D-008 | Never break the wire or the bpak format          | Narrowed by D-035 (security first), D-061          |
| D-009 | The boot source: a kind and a bio partition      | In force                                           |
| D-011 | Command mode is open in EOL                      | Narrowed by D-036 (EOL is the SoC's state)         |
| D-012 | No Codecov                                       | In force                                           |
| D-013 | A golden register trace for hardware init        | In force                                           |
| D-014 | DDR IP in `src/drivers/ddr`                      | In force                                           |
| D-017 | MMC/SD tested against card models                | In force                                           |
| D-018 | The next release is 3.0.0, for the porting API   | In force                                           |
| D-019 | No boot-time self tests                          | In force                                           |
| D-020 | Slimming limited to dead files (temporary)       | In force; its revisit is open (`roadmap.md`)       |
| D-021 | The host tools are pure Python                   | In force                                           |
| D-022 | One Buildroot test bench                         | In force                                           |
| D-023 | Keep the boot path short                         | In force                                           |
| D-024 | bpak 1.0.1, and the headers it rejects           | In force                                           |
| D-025 | An MCU test bench                                | Narrowed by D-029 (its name), D-039 (the handover) |
| D-026 | A board is data, `struct board`                  | Narrowed by D-046, D-047, D-049                    |
| D-027 | The core owns the security rules                 | In force                                           |
| D-028 | One handover state; the `simple` boot driver     | Narrowed by D-039 (Firmware Handoff for `simple`)  |
| D-029 | The simple target test bench, also Cortex-A      | In force                                           |
| D-030 | The platform contract                            | Closed by D-053 to D-056                           |
| D-031 | The board contract                               | Closed by D-045 to D-052                           |
| D-032 | The boot flow contract (draft)                   | Closed by D-038 to D-041; changed by D-035, D-046, |
|       |                                                  | D-048, D-055                                       |
| D-033 | Documents first, top down (draft)                | Closed by D-036; narrowed by D-063                 |
| D-034 | Level 1: system and security (draft)             | Closed by D-035, D-036                             |
| D-035 | The level 1 review: security first               | In force                                           |
| D-036 | Level 1 and the process are reviewed             | In force                                           |
| D-037 | The documents' folders are the levels            | In force; D-062 removed two documents              |
| D-038 | The boot flow contract after level 1             | In force                                           |
| D-039 | The boot flow, Jonas's first answers             | Narrowed by D-048 (the in place header's address)  |
| D-040 | The boot flow, Jonas's second answers            | Narrowed by D-046 (the policy is no Kconfig        |
|       |                                                  | choice)                                            |
| D-041 | pb's status; the boot flow contract is reviewed  | In force                                           |
| D-042 | The agent instructions are `AGENTS.md`           | In force                                           |
| D-043 | pb's status stays, pb's version joins            | In force                                           |
| D-044 | The boot flow is written for any image format    | In force                                           |
| D-045 | The board contract after the levels above        | In force; its open items closed by D-046 to D-051  |
| D-046 | One member per concept in `struct board`         | Narrowed by D-049 (`cm_transport` leaves)          |
| D-047 | `late_boot()` gets no image header               | In force                                           |
| D-048 | In place: a flag in the static table             | In force                                           |
| D-049 | Command mode's USB is the platform's             | In force                                           |
| D-050 | `board-degraded` in pb's status                  | In force                                           |
| D-051 | Tasks                                            | In force                                           |
| D-052 | The board contract is reviewed                   | In force                                           |
| D-053 | The platform contract after the ones beside it   | In force; its open items closed by D-054 to D-056  |
| D-054 | A board maps hardware as data (`mmap`)           | In force; its form taken as approved (D-056)       |
| D-055 | Image data regions                               | In force                                           |
| D-056 | The platform contract is reviewed                | In force                                           |
| D-057 | The command mode contract (draft)                | Closed by D-058, D-059                             |
| D-058 | Command mode, Jonas's review                     | In force; its item (1) closed by D-059             |
| D-059 | `CONFIG_CM_AUTH` stays; the contract is reviewed | In force                                           |
| D-060 | Command mode and another image format            | In force; its open question answered by D-061      |
| D-061 | A build may leave bpak out                       | In force                                           |
| D-062 | No overviews of the code at level 2              | In force                                           |
| D-063 | Code that decides nothing needs no document      | In force                                           |
| D-064 | The drivers' contracts (drafts)                  | Closed by D-065 to D-073                           |
| D-065 | `drivers.md`, the first answers                  | In force                                           |
| D-066 | `drivers.md` is reviewed                         | In force; Open (5), which drivers have a `stop`,   |
|       |                                                  | is my reading                                      |
| D-067 | `bio.md`, Jonas's review                         | Narrowed by D-068 (the uncommented items)          |
| D-068 | `bio.md`, the uncommented items                  | In force                                           |
| D-069 | `bio.md`: an erase counts blocks                 | In force; the installed table decided by D-070     |
| D-070 | `bio.md` is reviewed                             | In force; which form of "A" is my reading          |
| D-071 | `crypto.md` is reviewed                          | In force                                           |
| D-072 | `usb.md` is reviewed                             | In force                                           |
| D-073 | The drivers' contracts, the final read           | In force                                           |
| D-074 | An entry in an empty part is refused             | Waits for Jonas's review                           |
| D-075 | The decisions have an index                      | In force                                           |

## D-001: Keep the bpak file format unchanged for now (2026-10-22)

The modernization works around the bpak format. Refactoring how punchboot
*uses* bpak is fine, as long as the on-disk and on-wire `struct bpak_header`
stays compatible.

## D-002: AI working context lives in `doc/dev/` (2026-10-22)

`CLAUDE.md` stays small and points to files that are loaded on demand.
`doc/source/` remains the user-facing Sphinx documentation (removed, D-020). `doc/dev/` is
developer and AI context: status, roadmap, findings, decisions, architecture
notes and a dated log.

## D-003: Release flow (2026-10-22)

Releases are made from master. Bump `version.txt` in a PR, then create a GitHub
release with tag `vX.Y.Z` and generated notes. The tag triggers
`punchboot-tools.yml`, which uploads to PyPI.

## D-004: Host-native test setup replaces the QEMU suite (2026-10-22)

Tests run on the host: CMocka unit tests, plus pytest integration tests against punchboot built
as a native process (`plat/host`, `board/host`). They are built with CMake/CTest, while the
firmware for targets stays on Make + Kconfig. The 43 bash/QEMU tests are ported one by one, and
the QEMU suite is removed when everything is covered. A small QEMU smoke tier for
arch-specific code may come back later.
Why: the QEMU suite was slow (~15 min), serial, and broke with toolchain upgrades (the gcov hack).
It also couldn't use sanitizers or a debugger.

## D-005: Tooling for the host tests (2026-10-22)

CMake + CTest, CMocka for C unit tests, and pytest driving the real `punchboot` Python package for
integration tests (this also tests the host library and the Python bindings).

## D-006: CI runs the host tests in the dev container; QEMU suite removed (2026-10-22)

`build.yml` runs everything through `scripts/pbdev`, the same as locally: the host tests (default
and sanitizers), coverage (gcovr, D-012), a compile check of the firmware for the boards that
build (jiffy, imx8dxlevk), and Coverity on master. The bash/QEMU suite, `src/board/test`,
`src/plat/qemu` and `configs/test_defconfig` were removed once all tests were ported. The virtio
drivers are kept for now; they would be needed for a QEMU smoke tier.

## D-007: Unit tests fake the neighbouring modules; known bugs are not encoded as passing (2026-10-22)

A unit test links one module (sometimes two) and fakes what it calls: the image helpers for
`boot.c`, the boot flow, root of trust and SLC for `cm_main.c`, the crypto providers through the
crypto API, and a scripted transport for command mode. The ramdisk fake injects I/O errors and the
platform fake has a controllable clock. A path that hits a known open bug is not tested; a comment
names the finding. In pytest, a known bug gets a `xfail(strict=True)` test that names the finding,
so the fix shows up as an XPASS.
Why: the integration tests can't reach most error paths, and a test that passes on buggy behaviour
would have to be changed by the fix.

## D-008: Never break backward compatibility of the wire protocol or the bpak format (2026-10-22)

Supersedes D-001. This is permanent, not "for now". Released hosts must keep working with new
devices, new hosts with released devices, and every image that a released bpak CLI produced and a
released punchboot accepted must still be accepted. Extensions are allowed when they are additive
and opt-in (append-only enums, fields taken from reserved bytes where zero keeps the old
behaviour, new commands that old devices reject cleanly, capabilities). Rejecting images that were
always invalid is the only exception, and needs its own decision entry. `compatibility.md` lists
what is frozen, how to extend, and the guards.
Why: devices in the field are not updated in step with the tools, factory lines pin tool versions,
and signed images outlive both.

## D-009: The boot source is a kind plus a bio partition; only the platform decides it (2026-10-22)

`plat_get_boot_source()` reports where pb was loaded from: `STORAGE` with the bio partition UUID of
the boot slot, or `USB`, `UART`, `DEBUGGER`, `UNKNOWN`. Every boot slot is a bio partition,
whatever the medium (eMMC boot partitions, NOR flash regions on the imxrt, the A/B offsets in the
MCX N internal flash), so any number of slots (A/B, primary/backups/factory recovery) works without
changing the API; the partition name says what the slot is. Drivers only report facts about their
device (`mmc_get_boot_part_config()`: what BOOT_PARTITION_ENABLE was at init). The platform turns
them into a boot source, because only it knows its ROM (fallbacks, serial download). Platforms that
don't know return `-PB_ERR_NOT_SUPPORTED`, and `UNKNOWN` when they can't tell this boot. New kinds
are appended. Not on the wire yet; exposing it to the host is a wire addition (D-008).
Why: an eMMC-only "booted from BOOT0/BOOT1" call was wrong for USB boots, for other ROMs and for
other media.

## D-011: Command mode is open without authentication in SLC EOL (2026-10-23)

Authentication is only enforced in `CONFIGURATION_LOCKED`. In `EOL` every command is open, as in
`NOT_CONFIGURED` and `CONFIGURATION`. Closes F-002 as intended.
Why: Jonas confirmed this is the intended EOL behaviour (2026-10-23). Note that it also opens
`cmd_board` (F-063) and everything else in EOL, so bounds checks on host input matter there too.

## D-012: No Codecov; coverage is a gcovr report in CI (2026-10-23)

The coverage job prints the gcovr summary and uploads the HTML report as a workflow artifact.
Nothing is uploaded to Codecov, and coverage is not a gate.
Why: the number only covers what the host build compiles, so most driver and platform code is not
in it. Codecov's default targets failed PR #65 because `mmc_core.c` and `rom_log.c` entered the
report for the first time (-7.5% project), the upload was non-fatal and the statuses not required,
and it needed `id-token: write`. What matters is that each fix has a test that fails without it.

## D-013: Hardware init without hardware is refactored behind a golden register trace (2026-10-28)

Code that can only run on a board we don't have (first: the i.MX8M LPDDR4 init) gets a host unit
test that runs it against a device model behind a fake `pb/mmio.h` and compares every register
write to a checked-in trace. Cosmetic commits keep the trace identical. A commit that means to
change what reaches the hardware regenerates the trace and says why in its message.
Why: there is no imx8mevk in the lab, and a DRAM init that is wrong in one bit fails silently or
not at all until it is on a board. The trace proves that a cleanup changed nothing; it does not
prove the sequence is right.

## D-014: DDR IP code lives in `src/drivers/ddr`, selected by the platforms (2026-10-28)

The Synopsys uMCTL2 controller and the firmware-trained DWC DDR PHY are drivers
(`DRIVERS_DDR_UMCTL2`, `DRIVERS_DDR_DWC_PHY`, hidden Kconfig symbols). Platforms select what their
SoC has; the i.MX8M SoCs do it through `PLAT_IMX8M_COMMON`, which `PLAT_IMX8MQ` (and later
`PLAT_IMX8MP`) select. The drivers take the base address (and the PHY its CSR stride and firmware)
from the caller and know nothing about the SoC. The init sequence, resets, clocks and the TF-A
handover stay in the platform, with one `ddr_<soc>.c` per SoC behind `ddr_soc.h`.
Why: the i.MX8M Plus has the same controller and PHY as the 8MQ, and the controller is also in the
ZynqMP and Allwinner H6/H616. The Allwinner H5 has neither (Synopsys names at Allwinner offsets,
a PUB-style PHY), so its DRAM code stays in its platform.

## D-017: MMC/SD drivers are tested against standard-based card models (2026-10-28)

`mmc_core` and the host controller drivers are tested on the host against models of an e.MMC
(JESD84-B51) and an SD card behind a bus interface, and the uSDHC against a register model from
its RM (`tests/unit/models/`, `testing.md`). The models behave like the device and log every
host action the standard or RM does not allow, with the section; tests fail on any. A bug the
bench finds gets a finding, and its test is skipped (`SKIP_OPEN`) until the fix enables it.
Why: the drivers are shared by five boards on four SoCs and only two boards are in the lab. The
first run found bugs on every board that hardware testing hadn't shown (R1b without busy wait,
commands above the clock limit in DDR52, ignored status errors). The models are a reading of the
documents, not the documents: each rule cites its section so it can be checked, and electrical
timing (sampling points, DLL taps) still needs a board.

## D-018: The next release is 3.0.0, for the porting API, not the wire (2026-10-29)

The release after v2.7.1 (2.8.0 was never tagged) is 3.0.0, built up on branch `work-3.0.0`.
The major bump is for the board and driver porting API, which breaks out-of-tree boards: the
request-based MMC HAL with one `mmc_device` per card, the `imx_usdhc` setup, `PLAT_IMX8M_COMMON`
and the DDR drivers in `src/drivers/ddr` (D-014), the DWC3 and ChipIdea UDC rework, Kconfig and
defconfig changes. The wire protocol and bpak stay compatible (D-008): 3.0 hosts work with 2.x
devices and the other way round, and every image 2.x accepted is still accepted. The release
notes say both first.
Why: Jonas wants a major release for the size of the change (new SoC and EVK, MMC/SD, DWC3,
CAAM, RAM log, many fixes). A major version normally reads as "incompatible", so without the
note users could think 2.x and 3.x tools and devices must not be mixed, and `punchboot~=2.7` or
`<3` pins on PyPI won't pick up the fixes.

## D-019: No boot-time self tests; on-target checks go through command mode (2026-10-29)

`CONFIG_SELF_TEST`, `DECLARE_SELF_TEST`, `src/self_test.c` and the four SHA-256 known-answer
tests in `crypto.c` are removed in 3.0.0, with `hash_print()`, which only they used.
Why: the tests had run nothing since `--gc-sections` (F-129) and nobody noticed, every defconfig
had them off, and the host unit tests cover the same vectors in CI. Boot is fail-closed anyway: a
wrong hash fails the signature check and pb doesn't boot, so the tests added a message, not
safety, at boot time cost on every boot. They also didn't test ECDSA, the dangerous failure.
Checking the crypto engines on silicon (CAAM, DCP) is done from the host against a board, the way
CAAM was checked on jiffy and the 8MP EVK, and belongs in a future hardware-in-the-loop tier.

## D-020: Slimming is limited to dead files and subsystems while new boards come in (2026-10-29, temporary)

Until the boards that are on their way are in, code is not removed just because no board in
the tree uses it: drivers, library and porting API, helpers, macros, register/pin/clock headers
and board hooks stay, because new boards and examples will need them. Jonas brought back
imx_gpio, imx_usbdcd, `hash_copy_update`, `bio_get_part_by_uu_str`, `usbd_xfer_cancel`, the
`mmc_*` read/write/info/extcsd calls, `boot_image_copy_and_hash`, `utils_def.h` and the
`imx_cdns3_udc.h` include in imx8dxlevk the same day. Ask Jonas before removing anything of
that kind, and hold the porting API simplifications in F-145 until the new boards show what
they need.
What still goes: whole subsystems nobody enables, docs nobody builds, files nothing references,
config nothing reads, dead scripts. The first round (3.0.0) removed littlefs, the Sphinx and
Read the Docs tree (`doc/source`, `.readthedocs.yml`; user docs start from the README, F-044),
the unreferenced presentation and image in `doc/`, unused TF-A asm helpers and asm macros,
headers nothing includes, always-on and unread Kconfig options, Makefile leftovers and dead
scripts. Boards that don't build (F-033, F-051), the ATF boot driver (F-146) and virtio
(F-052) wait for Jonas.
Why: Jonas wants the code slimmed down; dead code is read, grepped, reviewed and kept
compiling, and a stub doc tree looks like documentation (`--gc-sections` already kept most of
it out of the binaries). But more boards are coming, and Jonas expects to add things to the
boards or add examples that use what looks unused today.
Revisit: when the incoming boards are in, decide whether to go back to removing code without
an in-tree user.

## D-021: The host tools are pure Python (2026-10-29)

The `punchboot` package no longer has a C extension: the wire protocol, the transports and the
command sequences are Python (`tools/punchboot/`), USB goes through pyusb with libusb
from the libusb-package wheels (falling back to the system's libusb), and the C host library
(`tools/punchboot/*.c`, `api.h`) is gone. Nothing outside the extension used it. `_punchboot`
is gone too; `Session` and the exceptions keep their API.
Safety net (D-008): compatibility guard 4 first. The transcripts in `tests/compat/transcripts`
were recorded from the 2.x C library against pb-host, and the Python host must send the same
bytes, in the same transfer sizes, and return the same values. The structs in `wire.py` and
`bpak.py` are checked against `abi.txt`.
Why: one wheel for every platform and Python version instead of a cibuildwheel matrix with
vcpkg and patched Homebrew libusb bottles; Linux aarch64 for free; no memory-safety bugs in host
code (F-094) and no GIL held during USB I/O (F-009); ~550 lines of boilerplate (F-145) gone. A
32 MiB write to the 8MP EVK is as fast as with the C library (~28.5 MiB/s, within noise).
Cost: libusb now comes from someone else's wheels (libusb-package); a platform without them
needs a system libusb. There is no C host library for embedding anymore.

## D-022: One Buildroot test bench for the boards that run Linux (2026-10-29)

`testbench/` is a Buildroot `BR2_EXTERNAL` tree, built out of tree (`make BOARD=<board>`), with
a defconfig per board: imx8mpevk and jiffy to start. It builds punchboot and pbstate from this
tree (local packages), bpak and kickstart from a branch of their git repositories (`work-1.0.0`
and `master` of `../bpak` and `../kickstart`, the head commit is the version), Linux, and for
aarch64 TF-A BL31. The
initramfs is only kickstart; the root file system is a squashfs root image (`root.bpak`) that
kickstart verifies and mounts with dm-verity. No OP-TEE, no application packages.
The toolchain is musl: kickstart is linked statically and refuses glibc (its licensing).
The host bpak tool is the bpak python package of that branch (scikit-build-core, a host package).
Why: one place to get a Linux on any board, testing the tree as it is instead of a release
(jiffy's own test bench pins v2.7.1 and carries OP-TEE and MQTT).
Cost: bpak and kickstart are found next to this tree and have no hash to check until they are
released; a branch head is not reproducible.

## D-023: Keep the boot path short; defer to command mode what only command mode uses (2026-10-30)

Code that only command mode needs is set up in command mode (`cm_run()`), not in `main()`,
`plat_init()` or `plat_board_init()`: log sources that are only read over USB, board command
state, anything a host asks for. The boot path does what booting needs, and what can't wait:
state that may be gone later, or that the boot reads itself (the ROM log is copied at init, and
the i.MX6UL boot checks it for a serial download). First case: the Linux ramoops log (`linux_ramoops_init()`) moved from
`main()` to `cm_run()`.
Why: every boot pays for what is on the boot path, command mode is entered on a small share of
them, and less code on that path is less that can stop a boot.

## D-024: bpak 1.0.1, and the headers its validation rejects (2026-11-08)

punchboot follows bpak 1.0.1: the dev container has the 1.0.1 CLI (the `bpak` package from
PyPI, the C CLI is gone upstream), the test bench builds bpak's `master`, the golden images
have a `v1.0.1` set, and the vendored library (`src/lib/bpak.c`, `include/bpak/`) is 1.0.1,
copied by `scripts/sync-bpak` without local edits (`bpak.md`). The format and the keystore
codegen output are unchanged (only whitespace), and every golden image of 0.9.3 and 1.0.1 boots.
This is the exception D-008 allows: the firmware now rejects headers that 0.9's
`bpak_valid_header()` let through. They were always invalid:
- A used part or meta slot after an empty one (`-BPAK_NOT_SUPPORTED`). pb stops at the first
  empty part slot when it loads and hashes, the 0.9 CLI skipped empty slots and hashed what
  follows, so a signed image like that failed pb's payload hash anyway. Meta data after an empty
  slot was found by `bpak_get_meta()` but never bounds checked. No released CLI writes a gap,
  apart from 0.9's `bpak delete` with all 32 meta slots in use, which cleared `parts[0]` (F-068):
  that image had lost its first part.
- Meta data at an offset that isn't a multiple of 8 (`-BPAK_BAD_ALIGNMENT`). The CLI always
  aligns, and pb reads `pb-load-addr` as a `uint64_t` through that offset.
- `pad_bytes` of 512 or more (`-BPAK_BAD_ALIGNMENT`). The CLI pads to the next 512 bytes.
- Part sizes that overflow `size_t` (4 GiB on the 32-bit boards) or `off_t`, alone or added up
  (`-BPAK_SIZE_ERROR`). 0.9 truncated them in `bpak_part_size()`, so pb hashed less than the CLI.
- `signature_sz` over 512 (`-BPAK_SIZE_ERROR`). `bpak_copyz_signature()` already refused it
  before verifying; now `read-bpak` and the boot reject it in the validation too.
Lookups of id 0 (an empty slot) are `-BPAK_NOT_FOUND`; no board uses id 0.
Why: the validation runs on headers that aren't verified yet, and the lookups trust it. A copy
that follows upstream is also easier to keep in sync (F-021).
Cost: a header that only a hand-edited or broken tool could make is now refused by
`read-bpak` instead of shown. Each case has a unit test (`test_bpak.c`).

## D-025: An MCU test bench: a bare metal test application and a pytest runner (2026-11-08)

`testbench/mcu/` (`testbench/simple/` since 2026-11-09, D-029) tests pb on microcontrollers,
where the Buildroot test bench (D-022) doesn't apply. A small bare metal application, built per
board in the dev container (`arm-none-eabi`, no
RTOS, no vendor SDK), checks what pb hands over and reports it on the console; a pytest runner on
the host writes the images and the boot state over USB and reads the console. It covers the boot
of a signed image, the handover, A/B, rollback after the boot attempts and the watchdog. First
board: the i.MX RT1060 EVK.
What pb guarantees an ARMv7-M image (the contract the application checks): VTOR is its vector
table, MSP and PSP its initial stack pointer, r0 the booted system's name ("A", "B"), r1 the boot
partition's UUID, the D-cache and the MPU off, the I-cache invalidated, PRIMASK set and no
interrupt enabled (D-028). A board's pb enters command mode when an image asks for it in a register that
survives a warm reset (the RT1060: SRC_GPR5), so the runner gets the board back without a hand
on it.
Why: the tests check pb's behaviour, so the application must not bring its own startup and
assumptions; a few hundred lines per board are easy to port to the next MCU and run in the
existing container. Zephyr can come later as a "real application" on top, once this passes.
Cost: one more application to keep, and per board a linker script, a console, a watchdog and a
way to write the boot state (the RT1060 uses the ROM's NOR API).
The RT1060 EVK's NOR layout changes for it: `BL`, the two boot state copies, System A and B. The
product partitions it had (`SBL1`, `Config`, `Application`, `SBL2`) are gone; `SBL1` held an
older application on the EVK, backed up in `build-imxrt1060evk/nor-backup-2026-11-08/`.

## D-026: A board is data, `struct board`, and the core sets it up (2026-11-09)

Branch `work-3.0.0-plat`, on `work-3.0.0`. A board defines `const struct board board`
(`pb/board.h`): its console, RoT and SLC callbacks, hooks for its own hardware (`init`,
`late_init`, `cm_init`, `early_boot`, `late_boot`), its partition table, A/B state (optional),
boot driver config, kernel command lines and command mode transport. `main()` calls
`arch_init()`, `plat_early_init()`, `board.console_init()`, `plat_init()`, then `board_setup()`
(`src/board.c`): RoT and SLC, `init()`, GPT, A/B, `late_init()`, the boot driver. Command mode
calls `cm_board_init()`. The RoT and SLC come first so command mode knows the life cycle even
when the rest fails (F-168).
What the boards repeated is shared: command mode over USB (`pb_dev_cls_transport`),
`board_patch_bootargs()`, a default status and board command. The storage layout (the
partition UUIDs, the GPT, the A/B state) stays each board's own, also where boards have the same
one: a shared default layout (`pb/board_layout.h`) was tried and moved back (Jonas, 2026-11-09). The platforms no longer call the board or hand it a platform struct; the two boards
that need platform data ask for it (`imx6ul_plat()`, `imx8x_ipc()`, `imx8x_plat()`).
Why: every board had the same sequence with small differences, a different `board_init()`
signature per platform. The old order (RoT/SLC last) let a failed init open command mode.
The boards are 1,542 lines shorter: less to review and to port, and Jonas wants as little
board code as possible ("ideally no board code to maintain").
Pins are tables in the board's hooks: `struct imx_pin` (IOMUXC, `IMX6UL_PIN()`, `IMX8M_PIN()`,
`IMXRT_PIN()` by the pin's name, `imx_iomuxc_setup()`) and `struct imx8x_pad` (SCFW,
`imx8x_pads_setup()`).
Cost: out-of-tree boards must be rewritten (allowed by D-018). Clocks are still code in the
boards.

## D-027: The core owns the security rules, the platforms only read and write fuses (2026-11-09)

- RoT keys are the keystore's, the boards' key maps are gone (they only repeated the
  keystore). A key's revoke slot n is SRK n's revoke bit, shared with the ROM. The slots are
  `CONFIG_KEYSTORE_SLOTS`, the key ids in slot order, next to `CONFIG_KEYSTORE`: a key dropped
  from the keystore keeps its slot in the list, so the next key doesn't move onto its burnt bit.
  `rot_init()` refuses a list it can't read or without a slot for a keystore key. Empty is the
  keystore's order, as before. A platform says how many SRKs its ROM can revoke and refuses the
  rest (F-165); a key revoked past that by an older pb still reads as revoked.
- The SLC order is checked in `src/slc.c` once (F-166): CONFIGURATION can be written until the
  device is locked, locking needs CONFIGURATION, EOL is allowed from any state (D-011), and
  setting the current state of the last two writes nothing. The wire values don't change
  (D-008).
- HABv4 (i.MX6UL, RT1060, later the i.MX8M) has one driver, `drivers/fuse/imx_hab_fuses.c`,
  with the fuse locations from the platform (F-167). AHAB/SECO (i.MX8X) stays in its platform.
- The configuration fuses (boot device, ...) are board data, `slc_config.fuses`, with the
  platform's `fuse_read`/`fuse_write` by fuse index. `slc_set_configuration()` calls the
  platform's `set_configuration` (the SRK hash), then sets the bits of each word that aren't
  set yet; a word that has them all is not written again, also with other bits in it.
- The SRK hash to fuse comes from the build (`CONFIG_SRK_FUSE`, srktool's `*_fuse.bin`, turned
  into `srk_fuse.c`), not from words copied by hand into the board: a wrong hash closes the
  device to every image.
Why: the review of the RoT/SLC code found the same rule implemented differently on every
platform, with bugs in each (F-001, F-161..F-167). The rules are the product's, the fuse
locations the SoC's.
Open: the 8M's SRK_REVOKE word is unverified (8MP RM: reserved), so the 8M has no HAB support yet.

## D-028: One handover state on every arch; the simple boot driver is arch neutral (2026-11-09)

Every arch gives the next stage the same state (`include/pb/arch.h`): the MMU (on ARMv7-M the
MPU) and the D-cache off with pb's data in memory, the I-cache invalidated, interrupts masked
(IRQ and FIQ on ARMv7-A, DAIF on ARMv8-A, PRIMASK on ARMv7-M), at the exception level pb runs at
(EL3 on the i.MX8M and i.MX8X), p0..p3 in the first four argument registers.
`arch_disable_mmu()` turns the MMU and the D-cache off and masks interrupts, `arch_jump()`
masks them again, invalidates the I-cache and jumps; every arch has `arch_invalidate_icache()`.
A new arch must give the same.
The boot driver for images without an OS handover protocol is `simple` (`boot/simple.h`,
`CONFIG_BOOT_SIMPLE`, `board.boot_simple`), was `armv7m_baremetal`: only what the entry address
is depends on the arch (ARMv7-M: the vector table; ARMv7-A, ARMv8-A: the first instruction). It
starts applications and RTOSes alike: the name is pb's simple boot protocol (the entry, the
system's name, the partition's UUID), not what runs. "baremetal" said something about the image.
Why: the handover differed per arch (ARMv7-M left the D-cache on and interrupts unmasked, ARMv7-A
never invalidated the I-cache), so an image could only rely on what the arch it was tested on
happened to do. Linux and TF-A ask for the same state.
Cost: ARMv7-M images start with the D-cache off and PRIMASK set and turn them on themselves.
Out-of-tree boards rename `boot_armv7m` and `CONFIG_BOOT_ARMV7M_BAREMETAL` (D-018).

## D-029: The simple target test bench, also for Cortex-A (2026-11-09)

`testbench/mcu` is `testbench/simple`: it tests pb's simple boot protocol (`boot/simple.h`,
D-028), and a simple target makes sense on 32 and 64 bit Cortex-A boards too, started by pb at
EL3 without TF-A or Linux. Its boot time, reset to the simple target's first instruction, is a
metric of its own: ROM, pb's init, load, verify and handover, without a kernel (`boot-time-image`
stops before the jump, the Linux bench's reset to init is mostly the kernel).
Why: Jonas, the bench is about the protocol, not the MCU.
Planned, not done: the application split into `src/arch/{armv7m,armv7a,armv8a}/` (start-up and
each arch's handover checks: EL, DAIF, SCTLR.M and .C on Cortex-A) and `board/<board>/`
(console, reset, timer); then ARMv8-A on the 8MP EVK, ARMv7-A on jiffy, the MEK. On the Cortex-A
boards only the handover and boot time at first: A/B, rollback and the watchdog need the target to
write pb's boot state, which is on eMMC there (an MMC driver in the target); the Linux bench
covers them there. Back to command mode needs no per board register: a one-shot `boot
partition` leaves no active system. Also rename the `MCUTEST` console prefix and `tests/mcu.py`.

## D-030: The platform contract (2026-11-10)

`doc/dev/platform.md` is what a platform provides, when, and what the rest of pb may rely on, in
the style of TF-A's porting guide: each entry tagged mandatory or optional, with "valid from" and
what a failure means, and a review checklist. It is vendor neutral (Jonas): how the i.MX ports meet
it, the SoC facts with their sources and each port's conformance table are in `plat-imx.md`. It
describes the target; what the code doesn't do yet is marked "Not yet in the code (F-nnn)", and the
commit that fixes the finding removes the marker and updates the conformance table.

- A platform is a SoC family, a variant is a documented sub-level (`PLAT_IMX8MQ/MP`). A SoC fact
  belongs to the platform, a PCB or product choice to the board. Shared IP gets the SoC's facts as
  data (D-027).
- `plat_early_init()` starts the tick and the log time, arms the watchdog, saves the ROM's state
  and reads the reset reason; it can't fail. `plat_init()` returns an error only when command
  mode's prerequisites are missing (DRAM, MMU, its clocks, fuses), and records the rest in
  `plat_get_status()`. `main()` resets after a platform failure instead of entering command mode
  (F-235, F-202).
- No silent boot (Jonas): when something the boot's security depends on failed (a bypassed DRAM
  filter with a BL32, a lockdown that can't be done), `boot()` fails into command mode; logging is
  not enough. The policy per condition is a proposal, open (F-248).
- The tick stays in microseconds, `uint32_t`, wrapping: callers use differences only (F-211).
- The watchdog is kicked where work makes progress: per chunk in `load_and_hash`, before the jump,
  in command mode's loop. Never in `pb_delay`/`pb_timeout`: a driver polling forever would keep the
  board alive (Jonas). The platform states its longest stretch without a kick.
- `plat_get_boot_reason()`: a portable kind, the SoC's raw value for board code, a name for the
  log.
- The platform provides `plat_rot` and `plat_slc`, the board only its configuration fuses. No
  board overrides of platform services until a real case needs one.
- The TrustZone DRAM filter is pb's job where the SoC has one: TF-A's porting guide gives it to
  the stage in BL2's place, and TF-A's BL31 on the i.MX8M only programs it when it was enabled
  before. pb enables it before DRAM is used, locks the enable where it can, opens the board's DRAM
  to both worlds in a region of its own, leaves the default region secure (OP-TEE panics on the
  8M otherwise: aliases), and reports the state. The i.MX8X's is BL31's (SCFW partitions), the RT
  has none.
- The other duties of the stage in BL2's place (Jonas: "are there more?", checked against U-Boot,
  TF-A and OP-TEE): `plat_lockdown()` before the jump locks key revocation and what reopens a closed
  device (F-244: jiffy's CSF unlocks SRK revocation for pb and nothing locks it again); timers
  armed out of reset are disabled in early init (F-245); bus access control, the RNG, the crypto
  engine's handover and the architected timer are done or left to a named later stage (F-246).
- The MMU maps three layers: board RAM, the platform's map, pb's sections (`ram` and `buffers`
  apart, F-158). The i.MX8X's region below pb stays named in its map.
- The board's addresses become Kconfig values; link scripts contain no addresses and check the
  SoC's windows. pb's own memory (the `pb` region, its length derived from the ROM's load window)
  and the board's DRAM (`BOARD_DRAM_*`) are named apart: `BOARD_RAM` and `PB_RAM_LENGTH` were
  confused (Jonas).
- The SoC's packaging and signing move to the platform as stages with `?=` inputs; a board, also
  out of tree, overrides the tool, the signing step or everything from `pb.bin` (level 3 is what
  Jonas's custom systems use). CI builds a level 3 override. The stages are an interface for
  out-of-tree boards and change only in a major release (D-018).
- The device identity (`platform_ns_uuid`, the unique id) is fixed like the wire.

Why: the platform interface was `plat.h`'s nine functions, and the rest was found by copying a
platform. Each platform split early and late init differently, ran command mode after failures it
can't recover from, and its boards repeated SoC knowledge (security wiring, link addresses,
signing recipes). The defects found on the way are F-233..F-248.
Open: the policy of the boot's preconditions (F-248), the 8MP's TZASC fuse (F-208), the 6UL's
TZASC addresses and TZASC1_BOOT_LOCK (F-241), the Arm specs (TBBR, PSBG, FWU, the TZC-380 TRM)
aren't on the share. The whole bootloader's contract
(the security model, the handover, the A/B state) comes after this one, with TF-A's threat model,
TBBR, PSA and FWU as inspiration, not as compliance targets.

## D-031: The board contract (2026-11-11)

`doc/dev/board.md` is what a board provides, when the core calls it, and what it may rely on, in the
style of the platform contract (D-030): entries tagged mandatory or optional, with "valid from" and
what a failure means, a review checklist and a conformance table per board. Vendor neutral; what
the code doesn't do yet is marked "Not yet in the code (F-nnn)", as in `platform.md`.

- A board failure stops the boot, never command mode (Jonas): `board_setup()` ends at a failed
  step, skips the steps after it except the boot driver, which it always sets up, and returns the
  error; command mode can then install the table, repair the A/B state and boot from RAM (F-216,
  F-178). A platform failure resets (D-030), a board failure doesn't: command mode has what
  `plat_init()` set up. `init()` fails only when the boot can't go on safely, the rest is logged
  and in `status()`.
- The board's data is checked at run time in `board_setup()` and `cm_board_init()` (Jonas), a few
  lines tested on the host: the name, one boot driver, the transport, the password callback
  (F-249). For the platform, documentation and review were chosen instead; data checks are cheap.
- `struct board`, the hooks' meaning and the build interface change only in a major release
  (Jonas, D-018). CI's out-of-tree board (F-240) tests this interface too.
- `early_boot` and `late_boot` stay (Jonas): `early_boot` starts what is slow, the load overlaps it,
  `late_boot` reads the result and is the last chance to stop. The RT's charger detection needs
  more than 100 ms, so pb loads the image while it runs and checks in `late_boot`: minimal boot
  time is critical. A single stop hook before the load, proposed first, would have cost that time
  on every boot.
- `BOOT_FLAG_CMD` stays the board's to check: the boards differ on purpose (the 8X's button stops
  even a boot command mode started). `-PB_ERR_ABORT` from a hook means "enter command mode", not a
  failure.
- The serial download stop is the core's (Jonas, default on): with `CONFIG_CM_ON_SERIAL_DOWNLOAD`,
  `boot()` stops before `early_boot()` when `plat_get_boot_source()` reports USB or UART and command
  mode didn't start the boot. The RT and the 8X read the ROM's boot mode themselves today, which
  D-009 gives to the platform (F-251).
- A stop in a boot command mode started (F-252): before the transport's disconnect the host gets
  the error and command mode goes on; after it (`late_boot`, `plat_lockdown()`) pb resets. The
  host's OK for `boot part` and `boot bpak` means "loaded and verified".
- What a product in the field relies on is fixed like the wire: the board's name (`board_id`), the
  partition UUIDs and GPT variants, the A/B state partitions, the configuration fuses, the board
  commands' ids and answers.

Why: D-026 made the board data with hooks, but nothing said what each hook may rely on or what its
failure means, and the boards differed: a failed init left command mode without a boot driver,
every stop check bypassed command mode's boots by hand, two boards read the ROM's boot mode, the
board data could crash command mode (a NULL transport). Out-of-tree boards (Jonas's level 3
systems) need a written interface, and 3.0.0 is the porting API's release (D-018).
Cost: jiffy and the 8MP EVK stop in command mode after a serial download (`uuu`) instead of booting
from storage. `CONFIG_CM_AUTH_PASSWORD` comes out of the defconfigs without a callback (jiffy, the
RT, the host).

## D-032: The boot flow contract (2026-11-11, draft for review)

`doc/dev/boot.md`: the boot flow from `boot()` to the jump, worked out with Jonas over four rounds
(`log/2026-11-11.md`). It shapes `board.md` 2.5, 2.7 and 5.

- Words: the boot flow; the boot source policy (A/B is one); the handover (what the next stage
  gets, pb's information included, no separate "boot info").
- One boot source policy per build, chosen in Kconfig: fixed (one partition), a staggered chain
  (the board's order, stateless: it protects against an image that doesn't load or verify, not one
  that verifies and fails), A/B. A system boots without A/B. The core tries the policy's candidates
  in turn; load, verify and prepare are one attempt.
- A/B: a slot that fails to load or verify falls back to the other verified slot for this boot
  only, persisting nothing (F-174); a trial that fails spent its attempt and is tried again. When
  nothing is viable: command mode, or speculative mode (Kconfig, depends on the watchdog):
  alternate the slots per boot, best effort, so that the system can take an update; never an image
  that fails verification; ends when the OS confirms a slot; the handover says so. Recovery
  partitions later.
- `pbstate` is extended, not replaced, and isn't Linux specific; one shared core later (roadmap).
  The name stays.
- Verification is the core's, without hooks. Placement becomes an allow list (F-254). Anti-rollback
  will be a verification step, so a refused older image is a failed candidate and the policies
  don't change.
- Nothing is dropped (Jonas: out-of-tree projects): the in place and custom sources, speculative
  mode, `set_dtb_boot_arg`. The in place source becomes a property of a partition (F-253, F-219).
- A handover is an entry, four arguments and what pb wrote; it is delivered directly or through
  TF-A, whose BL33 (image or Firmware Handoff) and BL32 (OP-TEE's convention, others possible) are
  nested handovers. The board picks the handover, not the image: the same parts mean different
  handovers on different boards (DXL, MEK), so choosing by the image would change what old images
  do. `linux` becomes `image` (Linux, Xen). The DXL moves to TF-A.
- The generic handover is Arm's Firmware Handoff (Jonas: lean on standards): the transfer list holds
  the image's DT (copied into the list) or pb's structure, never both; the board decides by naming
  a DT part; pb makes no DT. The list is in a board region (pb's information changes every boot).
  pb's structure gets a tag allocated in the specification, requested upstream before 3.0.0.
  Cortex-M uses the AArch32 convention (pb's extension). It replaces `simple`, whose arguments
  exist only on the 3.0.0 branch; released pb passed none.
- pb's information is collected once and written per format; `dtb.c` stays (Jonas, against
  spreading DT code into the RoT and SLC modules). The board's DT hook keeps the whole tree and runs
  first, pb's properties after it. New: `pb,boot-partition`, `pb,boot-mode`.
- Xen: its DT carries dom0's module nodes from the image build; pb writes nothing for Xen. From EL3
  Xen goes through TF-A (Non-Secure EL2).

Why: the boot interface mixed where the image comes from, what to do with it and the board's
hooks in one struct; A/B was wired into the core and the drivers; two boot sources were lost or
broken; the drivers each handled the DT; the generic handover couldn't grow.
Open: the review of the draft, pb's structure's layout (`boot.md` 5.4), the region's Kconfig names.

## D-033: Documents first, top down (2026-11-11, draft for review)

`doc/dev/process.md`: the code follows its documents, and each document follows the one above it.

- Levels: purpose (`principles.md`), system (what pb promises to the outside and against whom:
  `system.md`, `security.md`, `compatibility.md`), flows (`boot.md`, command mode), interfaces
  (`platform.md`, `board.md`, the drivers), ports (`plat-imx.md`), code.
- A document decides only what its level owns. A question it can't answer from above goes up as
  an open item of the level that owns it. The code and the hardware give facts (findings, the
  ports' documents); they can show a document wrong, and then the document changes first.
- A contract has a state: draft, reviewed, in force. It is reviewed when the ones above it are.
- A change starts at the highest level it touches: the documents, the review, a finding per gap,
  a commit per finding.

Why: Jonas, after the three contracts: this must be top down. The platform contract was written
first and now waits for the board's and the boot's ("follows" in six sections), and their open
items ask what pb defends against and promises, which no document says. The contracts worked as a
habit; nothing said which document owns a question or when the code may follow.
Cost: two more documents (`system.md`, `security.md`) before the drafts' reviews are done, if
Jonas takes that order.
Open: the review of the draft (four items at its top).

## D-034: Level 1: the system and the security model (2026-11-11, draft for review)

`doc/dev/system.md` and `doc/dev/security.md`, the level above the flows and the contracts
(D-033). Jonas: two documents, written before the `boot.md` review goes on.

- `security.md`: the chain, what is protected and in which order (what runs next, the root of
  trust's state, the secure world, the storage through command mode, then availability), the
  attackers, ten rules the contracts cite (4.1 to 4.10), the known limits.
- `system.md`: what pb is, its modes, its interfaces to the outside and where each is fixed, the
  life cycle's states, what pb promises, what it leaves to the product.
- Both collect what was decided (principles 2 and 3, D-008, D-011, D-027, D-030 to D-032); what
  is new is an open item. Proposed in them: the promises hold on a locked device only; the
  preconditions are sorted by what rests on them (integrity refuses the boot, availability boots
  and reports); every byte pb loads is authenticated; the static token is a known limit.

Why: the drafts below asked questions that belong above them (`platform.md` Open (1), 2.3's
policy, `boot.md` Open (7) and (11)), and nothing said what pb defends against or in which state.
Open: the review of both (6 and 5 items).

## D-035: The level 1 review: security first (2026-11-11)

Jonas reviewed `principles.md`, `security.md` and `system.md` with comments in the text.

- **Security is first, even when it breaks compatibility.** `principles.md` is reordered: fail
  closed is 1, defence in depth 2, compatibility 3 (they were 2, 3 and 1; entries before this one
  cite the old numbers). A security fix that refuses what a release accepted needs a decision
  entry naming it (`compatibility.md`). This narrows D-008's "never".
- **The custom boot source is removed.** Nobody uses it, and it was the one way around the
  core's verification. This changes D-032 ("nothing is dropped") for this source only; the in
  place source, speculative mode and `set_dtb_boot_arg` stay (F-253).
- **An image with a `dont-hash` part is refused**, before the load. The flag is for content
  verified at run time (a file system under a hash tree), which pb doesn't boot. Such an image
  fails on its hash today, so no image that booted is refused (F-076). Accepting more later
  stays possible.
- **A life cycle that can't be read doesn't boot**, and command mode requires authentication. An
  attack gets nothing; a fault heals on the reset that ends a command mode without a host, and
  development boards have the ROM's serial download.
- **The host tools follow semantic versioning**: a breaking change is a major release, made with
  care, not forbidden. Only the wire and the image format are frozen.
- **The compatibility floor is the 1.0.0 release.** F-075 (erase for 1.x hosts) is accepted.
- **The static token stays**; authentication is pluggable, new methods are added next to it.
- **pb's status goes into the handover** as a principle; the entries are decided with the
  conditions (F-248).
- **Wording**: level 1 speaks of the secure world's isolation, not of a DRAM filter, and of
  secure firmware, operating systems and hypervisors, not of products.
- Taken as accepted, left without a comment: the model holds on a locked device only; the
  preconditions are sorted by what rests on them (integrity refuses the boot, availability boots
  and reports).

Open: EOL (`system.md` Open (1)); my answers to three questions (`security.md` Open (1) to (3):
pb's place in the chain, the isolation always enforced, methods that grant less). The roadmap has
what Jonas wants discussed later.

## D-036: Level 1 and the process are reviewed (2026-11-11)

Jonas's second round of comments, in `security.md`, `system.md`, `process.md` and `principles.md`.

- `security.md`, `system.md` and `process.md` are reviewed: no open items left.
- Confirmed: where the secure world's isolation falls to pb, pb always sets it up and refuses to
  boot without it, also with nothing in the secure world. Authentication is by method; methods
  are added, and one may grant less than everything.
- EOL is the SoC's end of life state. A product chooses it with a Kconfig option, off by
  default. Every port supports it; where the SoC can't, the port's document says why (F-263).
- Principle 8 is "simple and terse code": low complexity and low verbosity. "Little code" didn't
  say what Jonas meant.
- No cap on a review's open items (I had proposed 7, a number without a reason behind it).
- Taken as accepted, left without a comment: pb's place in the chain and its cost
  (`security.md` 1), and "in force" as its own state (`process.md` 4).

## D-037: The documents' folders are the levels (2026-11-11)

`doc/` is laid out by D-033's levels: `principles.md` and `process.md` at the top, then
`system/`, `architecture/`, `interfaces/`, `ports/`, and `guides/` for the how-to. `doc/dev/`
keeps the working records only: status, roadmap, decisions, findings, the log. `doc/README.md` is
the map, with each document's state. This narrows D-002: the contracts are the project's
documentation, for out-of-tree ports too, not AI working context.

- File names don't change and are unique across the folders: the documents cite each other by
  name and section, so a move changes only paths. Entries and logs before this one say
  `doc/dev/<name>.md` for documents that now sit in a level's folder.
- Level 2 is "architecture" (it was "flows"): it holds the flows and the architecture notes.
- The findings that are fixed, rejected or closed are in `findings-closed.md`; `findings.md`
  lists what is open. Ids are kept.

Why: where a document sits says what it may decide and what it must cite, and the folder listing
shows the order top down. 89 of 245 findings were closed and read every time.

## D-038: The boot flow contract after level 1 (2026-11-11)

A pass over `boot.md`'s 15 open items against `security.md`, `system.md` and `compatibility.md`
(D-033: a document decides only what its level owns). Derived by me from level 1, for Jonas's
review of `boot.md`; ten items are left for him there.

Closed, because level 1 leaves one answer:

- Preparing the handover is part of a candidate's attempt (availability: a device with a
  verified system boots it); a failed lockdown tries no other candidate (no jump without it).
- `boot status` stays "the enabled slot" for A/B, and `boot part` stays limited to bootable
  partitions for A/B: the meaning of a value on the wire is frozen.
- A boot command mode started keeps today's values in the handover.
- `linux,initrd-*` stay 32 bits where both addresses fit: an image that boots today gets the
  same bytes. 64 bits only above 4 GiB, which never worked (F-255).
- pb's structure isn't released before its tag is allocated: what is released is fixed.
- The preconditions are checked in every boot, also one command mode started; a life cycle
  that can't be read stops the boot with any handover.
- In place: a part is hashed where it runs, never at another address (F-219).

New in `boot.md` from level 1: pb's status in the handover (`system.md` 3), a `/chosen`
property and a field in pb's structure, so the structure's layout must have it before the tag is
requested; the flow says "the header" where it doesn't depend on bpak (`system.md` 3).

Left for Jonas: 1, 2, 3, 9, 12, 14 as they were; 5 for the fixed and chain policies only; 6 with
the status field; 10 as two questions (where the header is, whether a differing `pb-load-addr`
is refused); 17 as one (the preconditions before `early_boot()`).

## D-039: The boot flow contract, Jonas's first answers (2026-11-11)

Jonas answered `boot.md`'s ten open items in the text. Decided:

- The transfer list is always in a board region, also with a DT, which is copied into it.
- Firmware Handoff replaces `simple`. Released pb passed no arguments, so nothing released is
  dropped.
- Speculative mode also starts when this boot's failures leave no candidate, for this boot
  only, nothing written.
- The in place source's header: the partition's last 4 KiB, or where the board says, as in
  released pb. Both.
- The transfer list holds the image's DT or pb's structure, one or the other (D-032 stands;
  asked again after the brief on Firmware Handoff, `doc/guides/firmware-handoff.md`).

Written in as rules without a decision of his, since he saw no question in them: `boot part`
takes only the policy's candidates with every policy; the preconditions are checked before
`early_boot()`.

Open, with his view and my proposal in `boot.md`: speculative mode with nothing enabled (3), what
pb hands over (6: the structure, the Kconfig names, the status' encoding), `pb-load-addr` in
place (10), a trial whose attempt can't be written (12), Kconfig as the only switch for
speculative mode (14).

## D-040: The boot flow contract, Jonas's second answers (2026-11-11)

- **Speculative mode also starts with no slot enabled** (a state that is blank, lost or
  disabled by the host): A first, then alternating. In the field almost anything beats looping
  into command mode. "For now" (Jonas). The cost: with this mode "disable both slots" doesn't
  park a device in command mode, and a device in the factory boots an image that was written and
  not yet activated. Released speculative mode stays in command mode there.
- **`BOOT_AB_SPECULATIVE` is the only switch** for speculative mode; the board's `rollback_mode`
  field goes. Two switches that must agree fail silently, and D-026 rewrites the board files.
- **In place, a part is at its `pb-load-addr`**, as in released pb: hashed and entered there,
  and placement holds it inside the candidate's mapped range. Slot A's image in slot B is
  refused as a candidate. Jonas had leaned to ignoring the meta; in released pb it is where the
  part is, not a second address.
- **A trial whose attempt can't be written** is started only when no verified slot is left;
  otherwise the verified slot boots. A rollback or an alternation that can't be written is
  handed out. Jonas had leaned to always starting the trial; with a lasting fault that starts an
  image that verifies and hangs forever.
- **pb's structure** (`struct pb_fh_handover`) as in `boot.md` 5.4, with room for 16 keys.
- **The Kconfig names**: `BOOT_POLICY_FIXED`, `BOOT_POLICY_CHAIN`, `BOOT_POLICY_AB` (a defconfig
  with `BOOT_AB_DRIVER` still gets A/B), `BOOT_AB_SPECULATIVE`, `BOOT_FH_REGION_BASE`,
  `BOOT_FH_REGION_SIZE`. `BOOT_FH` for the handover itself is mine, from the region's names.
- `boot part` only for the policy's candidates and the preconditions before `early_boot()`
  (D-039, written in without his decision) stood through his second read.

Open: how pb's status is encoded. Jonas asked for strings instead of bit flags. Proposed: a
string list in the DT (`pb,status`), bit flags in pb's structure (`status`), one table for both.

## D-041: pb's status, and the boot flow contract is reviewed (2026-11-11)

pb's status (what pb couldn't do and booted anyway, `system.md` 3) is a list of strings in the
DT (`pb,status`) and bit flags in pb's structure (`status`). One table in `boot.md` 5.6 gives a
condition its string and its bit together; neither is reused. The property is always written,
empty when there is nothing. When 32 bits run out, a second field is appended to the structure.

Why: Jonas asked for strings. In the DT they are the idiom and need no table to read. In pb's
structure a list of strings has no fixed size and could only be the last field for good, while
the structure grows by appending; he accepted the bits there given that more can be added.

With it `boot.md` has no open items: reviewed (`process.md` 4). The code follows it, a commit
per finding (section 9).

## D-042: The agent instructions are `AGENTS.md`, for any harness (2026-11-11)

`CLAUDE.md` is renamed `AGENTS.md`, the name most coding agents read; the repo names no
harness. It amends D-002: the file stays small and points to documents that are loaded on
demand. One file at the root, plain Markdown: no nested instruction files, no imports, no tool
or command names of one harness. The dated log and the `claude-work` branch name in the closed
findings stay as they are, they are history.

Why: Jonas wants the repo to work with different harnesses. Claude Code reads `AGENTS.md` where
a project has no `CLAUDE.md` (2.1.322), so no stub is kept.

## D-043: pb's status stays, and pb's version joins pb's information (2026-11-11)

In his final read of `boot.md` Jonas doubted the value of pb's status (D-041), which has one
entry, and considered removing it. It stays. pb's version is added: `pb,version` in the DT, a
string, and `version[32]` appended to pb's structure; the string the wire's version command
answers (`version.txt`).

Why the status stays: `security.md` 4.5 sorts a failed precondition into refuse, boot and report,
or log only, and the status is the only output of the middle one. Without it those conditions
are a line on a UART nobody reads in the field, which is F-096's pattern. The watchdog entry
alone would not carry it (the next stage arms its own watchdog). Entries I expect, each decided
with its condition: an A/B state that couldn't be written (`boot.md` 3.2, F-179), a verification
with the software fallback after the crypto engine failed, a ROM that started pb from another
place than expected. Removing it would have been safe for compatibility, nothing is released.

Why the version: an OS can tell which pb booted it without command mode, for an inventory or a
report. It is not for telling which entries exist: a next stage looks for the entry. Appended
last in the structure, so the layout that goes to the specification only grows.

## D-044: The boot flow is written for any image format (2026-11-11)

Jonas, on `boot.md` 4.2: bpak is one format, another is planned (`system.md` 3), so the flow is
written without naming bpak. `boot.md` 4.4 is the one place that does: a table of what the flow
needs from a format (a header with its own validity check, the key's id, a signature over the
header and its hash, the parts with their place, size and load address, one payload hash in the
signed header, signed attributes for a handover) and what bpak has for each. 4.1, 4.2 and
section 5 say "the header", "a part", "its load address", "an attribute of the image". The
command's name `boot bpak` stays, it is the wire's.

The table is my reading of today's code (`image_helpers.c`, `boot.c`, `atf.c`), not a design of
the second format. Nothing in the code changes now: the core reads bpak's header directly, and
the format's interface in the code comes with the second format, together with what that format
changes outside the table (the part ids a board names, the host's command, several formats in
one build; `roadmap.md`).

## D-045: The board contract after the boot flow contract and level 1 (2026-11-11)

A pass over `board.md` against `boot.md` (reviewed, D-041), `security.md`, `system.md` and
`principles.md`, as D-038 was for `boot.md`. Derived by me, for Jonas's review of `board.md`;
five items are left for him there. The sections keep their numbers: `boot.md`, `platform.md` and
level 1 cite them.

Written in, because the documents above leave one answer:

- The lifecycle and section 5 follow `boot.md` 2: the preconditions before `early_boot()`, the
  loop over the candidates, the hooks once per boot outside it. A stop or failure in a hook
  tries no other candidate.
- 2.5 is the boot source policy's data, not A/B's alone, and `ab` has no `rollback_mode`
  (D-040). The policy marks its candidates: a board sets no `BIO_FLAG_BOOTABLE`.
- 2.7 is one handover and one delivery (`boot.md` 5.1), Firmware Handoff in `simple`'s place
  (D-039). The DT hook runs in prepare, so once per candidate, before pb's properties.
  `resolve_part_name` goes: the system's name is the policy's.
- `init()`'s failures are sorted as `security.md` 4.5 sorts a precondition. A failed RoT or SLC
  also refuses a boot command mode started (`system.md` 4).
- `late_boot()` runs after the policy wrote its state: a stop there costs an A/B trial an
  attempt. Written as a consequence and a rule of thumb (what is known before the load is
  checked in `early_boot()`), not as a change.
- 5.4 follows `boot.md` 6: `boot bpak` answers before prepare, so a failed prepare reaches the
  log, and command mode goes on.
- A board's command stays inside the core's rules (`security.md` 4.8); the boot flags' upper 16
  bits and the A/B state's board registers are the board's (they were in the code, not in the
  contract).
- Section 8 gains the policy, the parts the board names for the handover, and the password's
  secret. `compatibility.md`'s summary of it follows after the review.

Open for Jonas (`board.md`, top): the policy's fields; how a board gives the in place source (a
table, or released pb's hook), which `work-3.0.0-plat`'s merge waits for; the handover's fields;
bpak in the board's interface (`late_boot()`'s header, the part ids); a board's condition in pb's
status, which would be an entry of `boot.md` 5.6.

Found on the way: F-264, the RT's `late_boot` was never called.

## D-046: One member per concept in `struct board`, a variant is an object (2026-11-11)

Jonas, before his review of `board.md`: "board is data" (D-026) didn't turn out well, and the
boot source policy's fields (`ab`, `boot_part_uu`, `boot_chain`, D-045's Open (1)) felt awkward.
What stays of D-026: the core owns the order and the failure rules, the board is
`const struct board`. What changes is how a board gives a module its configuration.

Where a concept has variants, `struct board` has one member for it, and the board fills it with
an object the chosen module provides: the module's functions and the board's configuration for
it, made by the module's macro. `cm_transport` already worked this way.

| Concept | Member | Objects |
|---|---|---|
| The partition table | `ptbl` | `PTBL_GPT(device, tables)`, `PTBL_STATIC(device, table)` |
| The boot source policy | `boot_policy` | `BOOT_POLICY_FIXED(part)`, `BOOT_POLICY_CHAIN(parts)`, |
| | | `BOOT_POLICY_AB(&config)` |
| The handover | `boot_handover` | `BOOT_HANDOVER_IMAGE(&config)`, `BOOT_HANDOVER_FH(&config)` |
| Its delivery | `boot_delivery` | `BOOT_DELIVERY_TFA(&config)`; absent: directly |
| Command mode's transport | `cm_transport` | `pb_dev_cls_transport`, as today |

- The board's data makes the choice. Kconfig compiles a module in; a board that names one that
  isn't compiled in doesn't link. The policy is no Kconfig `choice` any more (D-040's option
  names stay, as options), and one build may hold several policies.
- A module's configuration is documented with the module (`boot.md` 3.5, 5.2 to 5.4), not in
  `board.md`.
- What stays in Kconfig: `BOOT_AB_SPECULATIVE` (D-040), and the addresses the build needs
  (`BOOT_FH_REGION_*`, the board's DRAM and entry).

Why: with a member per variant (`boot_linux`, `boot_atf`, `boot_simple`; `ab` and two more for
the new policies) every choice was made twice, in Kconfig and by which pointer is set, and their
agreement was a run time check (F-249). Every new variant changed `struct board` and
`src/board.c`, which D-031 allows only in a major release; a second image format and more
policies are planned. With an object, a mismatch is a link error, a new variant leaves the
porting API alone, and one `pb-host` can run every policy in the integration tests.

Considered:

- Modules find their data by name (`extern const struct ... boot_policy_ab_config`), Kconfig
  selects, as the platform contract does with `plat_*`. Also a link time check, and
  `struct board` would hold only hooks; but a board is no longer read in one place, and each
  policy needs its own host build to be tested.
- The board registers its choices with calls from a hook (`boot_policy_ab_init(&config)`). The
  choices then aren't `const`, and a missing one shows at run time.
- Grouping the flat struct. Cosmetic.

Cost: a `const void *` behind the macros; the board files are rewritten once more (F-265, with
F-256, F-258 and F-259, before `work-3.0.0-plat` merges or right after it); one sentence of the
reviewed `boot.md` changed (3: the policy was "chosen in Kconfig").

Not settled by this entry: `cm_transport` and `cm_init()`. Jonas's idea, right after it: both
leave the board's interface, the platform sets up command mode's USB and Kconfig selects the
interface (`board.md` Open (4), to discuss).

On `board.md`'s open items (D-045): the policy's and the handover's fields are no longer its
questions. The in place source is proposed as a field of the static partition table's entry;
where the header is stays open. Left: in place, `late_boot()`'s bpak header, a board's condition
in pb's status, and the transport's idea above.

## D-047: `late_boot()` gets no image header; the format hands out the verified one (2026-11-11)

Jonas, in his review of `board.md` (its Open (2)): changing the hook is fine for 3.0.0. The
`struct bpak_header *` argument goes, and a board that needs the verified header calls a
function of the image's format: `boot_verification_get_bpak_header()`, and one per format that
comes (`boot_verification_get_<format>_header()`).

- It returns `const struct bpak_header *`: the header of the image this boot verified, NULL
  before the payload's hash is checked or when the image isn't a bpak image. Valid from prepare
  to the jump, so the DT hook can use it too. (The return value and from when it is valid are
  mine.)
- `late_boot()` keeps `boot_part_uu`, which names no format.

Why: the boot flow is written for any image format (D-044), and a hook's arguments change only
in a major release (D-031). With the header as an argument a second format would have needed a
second hook or 4.0.0. As a function of the format it is compiled in with the format, and a
board that doesn't ask for it doesn't include `<bpak/bpak.h>`.

The name (Jonas, the same day): `boot_image_get_bpak_header()` and
`boot_image_get_<format>_header()`, like the module's other functions (`boot_image_auth_header()`,
`boot_image_verify_payload()`). `boot_verification_get_*` above was his example.

Replaces what I had proposed (keep the argument, as `const`, for bpak only). Code: F-266.

## D-048: In place: a flag in the static table, the device's mapped base, no header address (2026-11-11)

Jonas, in his review of `board.md` (its Open (1)):

- A board says a partition is booted in place with a flag in its entry of the static partition
  table. The table's object carries the address the device's first byte is mapped at
  (`PTBL_STATIC_MAPPED(device, base, table)`); the partition's address follows from it and the
  entry's first block.
- The header is where the image's format has it in a partition (for bpak the last 4 KiB), read
  through the map. The board gives no address for it. This drops "or where the board says"
  from `boot.md` 4.1, which D-039 had kept ("both"), and released pb's `get_in_mem_image()`
  doesn't come back.

Why: his boards have the header at the partition's end, so the second way had no user. Where a
header sits is the format's (`boot.md` 4.4): a format with its header first says so itself, for
every source. An address per entry with 0 for "not mapped" was my first proposal; his question
(what would 0 mean for a header at the beginning) showed the sentinel's problem, flash can be
mapped at 0, and an address per entry must agree with the entry's first block.

Compatibility: no image changes. A board that kept its header elsewhere than the format has it
would have to move it; none is known (Jonas). Allowed for the porting API in 3.0.0 (D-018).
Code: F-253, with F-219 and F-265.

## D-049: Command mode's USB is the platform's, and the transport is linked (2026-11-11)

Jonas, in his review of `board.md` (4.1, 4.2):

- pb's transport is USB and stays USB. Most SoCs' ROMs use USB for recovery and provisioning,
  so it fits; other transports, an IP stack above all, would add a large attack surface to a
  stage that runs with the highest privilege (`security.md` 1; `system.md` 1: no network).
- A board sets up nothing for command mode. The platform does it: `plat_cm_init()`
  (`platform.md` 3.10), called at the start of `cm_run()`. Which of the SoC's ports is a Kconfig
  choice of the platform's. `cm_init()` leaves `struct board`, without a hook for the PCB in its
  place: none of his boards needs one.
- `cm_transport` leaves `struct board`. Command mode calls the transport's functions directly,
  and the build links one implementation: the USB class driver, or the host platform's socket.

Why: every board's `cm_init` is SoC work (clocks or power domains, the PHY, the controller), no
pin and nothing of the PCB; the only board choice in it is the port. Every board sets the same
transport. The seam stays, at link time, because it has a second implementation: the tests and
`pb-host` talk to command mode through a socket. That also keeps a way open for a SoC without a
USB device port, without the board's interface in it.

This takes `cm_transport` out of D-046's table; D-046's rule holds for the other four. Here
Kconfig chooses, not the board's data: the choice is among the SoC's ports, the platform's.
Code: F-267.

## D-050: A board reports what it booted without: `board-degraded` in pb's status (2026-11-11)

Jonas, in his review of `board.md` (its last open item): accepted as proposed. What a board's
`init()` logs and doesn't fail on (jiffy: the CAAM's RNG for Linux) is "what pb couldn't do and
booted anyway" (`system.md` 5.4). The board calls `board_report_degraded()`, and the handover
carries one entry for every board: `board-degraded` in `pb,status`, bit 1 in `status`
(`boot.md` 5.6). What it stands for is in the log and in `status()`.

The function's name, and that it is valid in `init()`, `late_init()` and `early_boot()` (pb's
information is collected after `early_boot()`), are mine. Code: with F-248.

## D-051: Tasks: a linker list the core calls in turn while pb is stopped (2026-11-11)

Jonas, in his review of `board.md`: `DEFINE_TASK(name, run)` defines a task, a name and a
function, in a linker section. When pb is stopped the core calls every task in the list in
turn, again and again. No switching, no pre-emption. Command mode's own handling of the host is
one of the tasks (`cm_main_task`): the core owns the loop, not `cm_main.c`. Command mode's
optional `process` hook goes.

The loop is outside command mode, in `main()` (Jonas, corrected the same day; I had first
written it as command mode's): pb can be built without command mode, and there are uses for
tasks without it. Such a build stops the same way and runs its tasks; with none it resets, as
today. `system.md` 2 says so.

His answers to what I asked:

- **No order.** It isn't important, so none is defined and the struct has no priority.
- **The rules for a task**: it returns quickly, takes no input from the host (only command
  mode's task reads the transport), stays inside the core's rules, and the core kicks the
  watchdog. Accepted.
- **Latency**: a long command holds every other task off. Fine for now; nothing his boards do
  there is time critical.
- **The link scripts keep the section**; no set-up function.
- **The large change**, not only a replacement for `process`: command mode becomes a task.

Mine: `run` returns int, PB_OK to be called again, an error ends the loop and pb resets, as
when command mode ends today. `DEFINE_TASK()`'s arguments.

Why: `process` was one hook for one user, and since D-026 no board could set it (F-268). A list
lets a board, a platform and a driver each have a task without a member in `struct board` or
`cm_config`, and it stays off the boot path (D-023). The host's tests need no link script for
it: the linker provides a plain section's start and end.

Cost: `cm_main.c`'s blocking loop becomes a step function; a section to keep in six link
scripts (with F-239). Where it is written: `system.md` 2 (stopped, and command mode as one
task), `board.md` 4.6 (what a board may rely on) and `platform.md` 5.1 (the section). Code:
F-268.

## D-052: The board contract is reviewed (2026-11-12)

Jonas finished his review of `board.md`. No open items are left: what the pass (D-045) left
open and what the review added is decided in D-046 to D-051, in the table at its top. Its state
is reviewed (`process.md` 4); the code follows it, a commit per finding.

With it, `compatibility.md`'s "Fixed elsewhere" names what `board.md` 8 fixes for a product in
the field today: the boot source policy and its partitions, the keystore's slots, the handover
and the parts named for it, and the password's secret joined the name, the partition UUIDs, the
GPT variants, the configuration fuses and the board commands. The line is a summary of
`board.md` 8; no guard changed.

## D-053: The platform contract after the boot flow and board contracts (2026-11-12)

A pass over `platform.md`'s seven open items (2026-11-11) against `boot.md` and `board.md`,
both reviewed now, and level 1, as D-038 was for `boot.md` and D-045 for `board.md`. Derived by
me, for Jonas's review of `platform.md`; two items are left for him there.

Closed, because the documents above and beside it leave one answer:

- **A bypassed DRAM filter refuses every boot**, in every life cycle state (`security.md` 4.5,
  D-036). 2.3's table is sorted by 4.5: the filter and a failed crypto engine refuse the boot
  (no build has a fallback that verifies the same), a watchdog that didn't arm boots and is
  reported (`boot.md` 5.6), missing ROM logs are logged. The check sits before `early_boot()`,
  in every boot (`boot.md` 2).
- **A failed `plat_lockdown()`**: pb stops; a boot command mode started resets (`boot.md` 2,
  `board.md` 5.4). Command mode then passes on the driver's error for what a set lock refuses.
- **pb doesn't clear the reset reason**: the OS reads the same register, and what an OS in the
  field reads doesn't change in an update (`system.md` 5.1). `kind` is the highest priority
  cause since the status was last cleared (F-210 stays a fact of the SoC, in the header).
- **`plat_get_boot_source()` is mandatory when the ROM has a serial downloader**: the core's
  stop rests on it and a board doesn't check itself (`board.md` 5.1); without it the stop
  silently never triggers.
- **Whoever started DMA stops it** (`board.md` 5.3): the core its transport, and with it the
  USB controller `plat_cm_init()` started, and the device it loaded from; the board what its
  hooks started; the platform what `plat_init()` started.

Added from this week's decisions: `plat_slc`'s `set_eol` is every port's (`system.md` 4,
F-263); the Firmware Handoff region is in the link interface's table (D-040's names); a
partition booted in place is the board's to declare, its range in `plat_mmap[]` (D-048); pb
"stops" where the text said "enters command mode" (D-051).

Left for Jonas: the form of the loadable regions and of `bl_params`' place (5.3; `boot.md` says
that the platform declares them, not how); what a board may include beyond section 7's list (the
two 8X boards include the arch's headers and the system controller's API).

## D-054: A board maps hardware the platform didn't, as data (2026-11-12)

Jonas, reviewing `platform.md` 5.2: board code will want a peripheral the platform hasn't
mapped, and that must be supported in an elegant way. The requirement is his; the form below is
mine and waits for his review.

- **`mmap` in `struct board`** (`board.md` 2.8, `platform.md` 5.2): a list of regions,
  `PB_MAP_DEVICE`, `PB_MAP_MEMORY_RO`, `PB_MAP_MEMORY_RW`, ended by `PB_MAP_END`, from
  `include/pb/mmu.h`. `pb_mmu_init()` maps it with the board's DRAM, `plat_mmap[]` and pb
  itself. The platform does nothing for it.
- **Data, not a call**: the tables are built once in `plat_init()`, before the MMU is on. A
  call would need xlat_tables' dynamic tables in every build, on the boot path (D-023), and
  would fail late. It also fits D-046: one member per concept.
- **Rules**: flat, whole pages (checked in the build), never executable, outside pb, the
  board's DRAM and the platform's regions. xlat_tables lets a nested region win, so
  `pb_mmu_init()` refuses the overlap itself.
- **Room**: `MAX_MMAP_REGIONS` and `MAX_XLAT_TABLES` become Kconfig values with the platform's
  default, which a board raises.
- **Failure**: `pb_mmu_init()` fails, so `plat_init()` does, and `main()` resets. The console
  is up by then.
- **The platform's side**: `plat_mmap[]` maps the SoC's peripheral buses whole, so that the
  usual case (a SoC peripheral the platform has no driver for) needs nothing from the board.
- An in place partition's range is in `plat_mmap[]` when it is a SoC region and in the board's
  `mmap` when it isn't (`platform.md` 5.3, D-048).

Not chosen: a run-time `map()` call (above); a linker list as for the tasks (D-051), which
would spread a board's map over its files where one list can be reviewed.

Code: F-269, with F-158 (`pb_mmu_init()`).

## D-055: Image data regions: placement's allow list is the board's (2026-11-12)

Jonas, reviewing `platform.md` 5.3: turn the placement check around. It named pb's own regions
one by one; better that a board says where image data may go. I agreed and proposed the form;
he approved it.

- **The board's list** (`image_regions` in `struct board`, `board.md` 2.9): the only memory an
  image's parts are loaded into. A part lies inside one region, with the room a handover grows
  it by, and over no other part (`boot.md` 4.3). The rule names nothing of pb's.
- **Checked once**, with the board's data: no region overlaps `pb`, `buffers`, the Firmware
  Handoff region or `bl_params`' region. Everything pb links is in `pb` or `buffers`, so a
  section added later is covered. A list that fails is the board's failure, found at bring-up,
  not a refused image.
- **The platform declares no loadable memory.** Its header names the SoC memory a board may
  list (BL31's on-chip RAM, a TCM). A list of the platform's would allow those loads on every
  board of the SoC.
- **No default.** A board without a list loads nothing and boots only in place. The whole DRAM
  as a default would bring back the subtraction of pb's regions.
- **`bl_params`**: in pb's own data, unless BL31 restricts where they may be; then the platform
  header defines `PLAT_BL_PARAMS_BASE` and `_SIZE` (the names are mine).
- **Fixed for a product in the field** (`board.md` 8): a region is made larger or added, never
  made smaller or removed.

This changes `boot.md` 4.3, which was reviewed: its allow list was the board's DRAM and the
platform's loadable regions, less pb's sections, the Firmware Handoff region and `bl_params`.
It closes `platform.md`'s open item on the form of those declarations.

What it costs: where pb runs in DRAM (jiffy, the MEK, the DXL) the board's region starts above
pb, a number that must agree with the link and is checked only at run time. An image that an
older release loaded outside the regions is refused, as with F-254's allow list before
(security before compatibility, `principles.md`).

Code: F-254, with F-258 for `bl_params`.

## D-056: The platform contract is reviewed; what a board may include (2026-11-12)

Jonas finished his review of `platform.md` and closed its last open item.

- **A board includes** `include/pb/`, `include/drivers/`, `include/boot/` and
  `include/plat/<p>/`, and the image format's header when it asks for the verified header
  (D-047). Nothing else.
- **Not the arch's headers**: what a board needs from the arch comes through
  `include/pb/arch.h`.
- **A system controller's API through the platform header** (Jonas). The API lives with the
  platform (the 8X's `include/plat/imx8x/sci/`, `src/plat/imx8x/sci/`): it is the SoC's, and
  the platform is built on it. A board that sets up its clocks and power calls it, and gets it
  by including the platform header, which includes the services boards need. A board never
  names the API's headers by their own path.

I first wrote this stricter than Jonas meant: no API in a board at all, with helpers in the
platform header that wrap each call. He corrected it the same day. Wrappers that mirror the
API one to one would hide nothing, and clocks are code that whoever selects the peripheral
writes (Jonas, 2026-11-09), which on such a SoC is a call to the system controller.

The review's other outcomes are D-054 (`mmap`), D-055 (image data regions) and the wording of
4.3 (no fact of one port's in the rule). The form of D-054 was a proposal of mine; Jonas
reviewed 5.2 with it in and closed the review, which I take as approval.

Code: F-270 (the MEK's and the DXL's includes, `imx8x.h`).

## D-057: The command mode contract, as a draft (2026-11-12)

Jonas: the documents that guide the code come before the code. After the three contracts
(`boot.md`, `board.md`, `platform.md`) the next two are `command-mode.md` (level 2) and the
drivers' contracts (level 3), in that order (`process.md` 1: top down). The code (F-253 first)
waits for them.

`doc/architecture/command-mode.md` is a draft with eight open items. What it takes from the
documents above it, and so doesn't decide:

- Command mode is a task (D-051), its USB is the platform's and its transport is linked
  (D-049). Who may use it in which state is `system.md` 4 and `security.md` 4.7.
- The gate is the core's and the only place that decides (`security.md` 4.8). The device is
  open or closed by its life cycle alone; a closed device answers the identifier and the
  authentication, nothing else.
- A session ends with the transport, and its authentication and stream with it (F-185).
- The exchanges are the released ones (D-008), written down for the first time (4.4). Where
  released hosts send data after a refusal, the device stays in step.
- The watchdog is kicked per chunk inside a command (`system.md` 5.4, `platform.md` 3.3).

What it proposes, for Jonas (its Open (1) to (8)): no build without the gate (`CONFIG_CM_AUTH`
goes); a hidden partition doesn't exist for the host, and `READABLE` only gates the bulk read;
no idle timeout; a failed authentication is answered after 1 s; six classes of commands that a
future method may grant; a transfer timeout that ends the session, and the caps' timeout
fields filled; `boot bpak` always reads the header and answers a refusal in its result;
`INVALID_COMMAND` before authentication stays.

Three of them refuse what a release did (a build without the gate, a hidden partition by its
UUID, a stream offset inside a block). Each is `security.md`'s; the entry that closes the
draft names them (`principles.md` 3).

`compatibility.md`, "Fixed elsewhere", gets a line for its section 7: command mode's
behaviour, and the USB interface hosts find the device by, which no document had fixed.

New findings: F-271 to F-274. Not changed: any code.

## D-058: The command mode contract, Jonas's review (2026-11-12)

Jonas reviewed `command-mode.md` and answered its eight open items. One stays open.

- **(2) A hidden partition is internal to pb.** A partition without `VISIBLE` isn't reachable
  over the wire: `NOT_FOUND` for every command. `PART_VERIFY` needs no `READABLE`, and that is
  right: the host hashes its file and asks pb for the same over the partition. `READABLE` gates
  the bulk read only. F-272.
- **(3) No idle timeout.** No benefit, and a change of behaviour that hosts would trip over.
- **(4) A failed authentication is answered after 1 s.** F-276.
- **(5) What a method may grant is not designed yet.** The six classes are out of the
  contract. Jonas isn't comfortable committing to them, and is thinking of a manifest of the
  allowed commands, signed and sent as the token. `roadmap.md` has it. The contract keeps the
  one thing that needs: the gate is the only place that decides.
- **(6) A transfer the host doesn't complete ends the session** after
  `CONFIG_CM_TRANSFER_TIMEOUT` (10 s), and the caps carry pb's timeouts. F-277.
- **(7) `BOOT_BPAK` always reads the header** after its first OK, and the header's result
  carries a refusal. F-128.
- **(8) A closed device answers `NOT_AUTHENTICATED` to everything but the identifier and the
  authentication**, from 3.0.0: also to a command its build doesn't have, and (my addition,
  for one rule) to a frame with a wrong magic. Every release answers `INVALID_COMMAND` there.
  F-275.
- **(1) Open.** Jonas: pb can't keep a product from weak authentication, a board's password
  check is the board's; but a build should have to select at least one method. Whether the
  switch `CONFIG_CM_AUTH` itself goes is not answered (my proposal: it goes).

What 3.0.0 answers differently from a release, for `security.md` 4.7 and 2 (`principles.md`
3): (8) changes a result code on a locked device before authentication; (2) refuses a hidden
partition by its UUID, which no board in the tree has; a stream offset inside a block is
refused (F-273); (4) delays a failed authentication. `compatibility.md` says what (8) means for
a host that probes for a command.

`board.md` 4.4 names the reset a board's command can ask for
(`cm_board_cmd_request_reboot_on_success()`): the draft listed it as missing and the review
left it so.

The rest of the draft had no comments. Taken as read, not as confirmed: the document stays a
draft until (1) is closed.

## D-059: `CONFIG_CM_AUTH` stays a switch; the command mode contract is reviewed (2026-11-12)

Jonas closed the last open item of `command-mode.md`: "make it a switch".

- `CONFIG_CM_AUTH` stays, on by default. Off, the build has no gate and command mode is open
  in every life cycle state, on a locked device too: a product's explicit choice.
- On, the build selects at least one method (the token, the password), or it doesn't build
  (D-058, his proposal). F-271.
- I had proposed that the switch goes and the gate is always compiled in. Why not: pb can't
  keep a product from weak authentication anyway (a board's password check is the board's), so
  the choice is the product's either way, and a switch says it plainly.

Level 1 follows: `security.md` 4.7 names the build without authentication as the product's
choice, and `system.md` 6 lists it among what pb leaves to the product. Both are reviewed
documents, changed for this.

`command-mode.md` is reviewed, with no open items. Its code: F-271 to F-277 and the findings
its section 9 lists.

## D-060: Command mode and another image format; a second pass over the contract (2026-11-12)

Jonas asked for another review pass over `command-mode.md` and whether it is loose enough for
other image formats than bpak. The findings are mine; he read them and said "write them in".
Nothing in the code changes.

**Another image format** (`command-mode.md` 5.9, new). Command mode knows bpak in three places,
and those are the wire's: `BOOT_BPAK` (a header of 4096 bytes, then the parts it lists),
`PART_BPAK_READ` (4096 bytes from the partition's end) and `PART_VERIFY`'s `bpak` flag (the
layout in a partition). Everything else carries bytes or names partitions, and `BOOT_PART` boots
whichever format the build accepts (`boot.md` 4.2).

- The three keep meaning bpak and aren't widened: the contract had worded two of them as any
  format's ("the image header a partition holds", "the image the host sends"), under a section
  7 that fixes each command's meaning. For another format's image they answer as for no bpak
  image.
- A format's own commands (a boot from the host's stream, its header) are new commands,
  appended. The capabilities tell a host which formats a build accepts; saying nothing means
  bpak alone.
- Not decided: whether a build may leave bpak out (level 1's, `roadmap.md`), and the second
  format itself (D-044, the parked `pbi-image-format` branch).

Why not widen `BOOT_BPAK`: its header transfer is 4096 bytes for ever (D-008), and the pbi
draft's header is variable, up to 8 KiB.

**`INVALID_COMMAND` against `NOT_SUPPORTED`** (4.5). `INVALID_COMMAND` is a frame that isn't a
command or a number the release doesn't define: "the device is older than the command", where
a host falls back. A command the release defines and the build left out answers
`NOT_SUPPORTED`. That is what the code does (`pb_wire_valid_command()` only checks the range;
no RAM log, no method and EOL without the option answer `NOT_SUPPORTED`); the contract had
"the build doesn't have it" for the first and "the build lacks what it needs" for the second.
`compatibility.md`, "New command", has the sentence for hosts.

**The rest of the pass:**

- The UUID `BOOT_BPAK` carries isn't a lookup, so the hidden partitions' rule (5.3, D-058)
  doesn't apply to it: it is the handover's label, taken as given (`boot.md` 5.6). 5.3 had
  "every command that names it ... the boot commands too".
- `BOOT_BPAK`'s parts are a stream of the part's size, which the host and the device split
  each their own way (4.1, 4.4, section 6). After a part's result that isn't OK the host sends
  the rest and the device reads it; the last result is the verdict. Both are what the code
  and the hosts do. Whether every USB controller driver takes it where the two sizes differ
  isn't verified: F-280.
- A refusal before the authentication data (an unknown method, a bad size) changes nothing
  and isn't delayed (3.3). F-278.
- The token's transfer stays 1024 bytes; a key whose signature is larger (ML-DSA) gets a new
  method (3.3, `roadmap.md`).
- The caps' `bpak_stream_support` stays zero (5.1). `PART_BPAK_READ` answers `NOT_FOUND` for
  a partition below 4 KiB (F-279). `NOT_FOUND` for "no image header" is `PART_BPAK_READ`'s;
  a boot answers `ERROR` for a header that isn't valid (4.5).

## D-061: A build may leave bpak out (2026-11-12)

Jonas, on the question `command-mode.md` 5.9 left for level 1 (D-060), may a build leave bpak
out: "I'd say so."

- A product may build pb without bpak, once there is another format to build in its place.
  It is the product's choice for its own devices, in its defconfig, like a build without
  command mode's authentication (D-059).
- D-008 is unchanged for a build that has bpak: every bpak image a release accepted is
  accepted. bpak is in by default, and a release doesn't drop it.

Written in, top down: `system.md` 3 and 6, `compatibility.md` ("bpak format: what is frozen"),
`boot.md` 4.4, `command-mode.md` 5.9, `roadmap.md`.

Derived by me, not asked of Jonas:

- "Once there is another format": today bpak is the only one, so the switch comes with the
  second format and no code changes now. No finding.
- What the bpak commands answer in such a build (`command-mode.md` 5.9, from its 4.5 and
  5.5): `PART_BPAK_READ` and `PART_VERIFY` with the `bpak` flag answer `NOT_SUPPORTED`;
  `BOOT_BPAK` reads the header's transfer and answers `NOT_SUPPORTED` in its result, so
  that hosts which send the header at once stay in step (D-058, item 7).

It answers item 3's question in the parked pbi draft's discussion state (branch
`pbi-image-format`, `doc/dev/pbi.md`). The rest of that item (the deprecation plan, the
defaults for new boards, the time scale) and whether one build accepts several formats stay
open.

## D-062: No overviews of the code at level 2 (2026-11-12)

Jonas: "I think we could remove the 'arch-host-tools.md' and 'arch-firmware.md'. I don't
really see what they add other than an overview that needs to stay in sync with the code."

- `arch-firmware.md` and `arch-host-tools.md` are removed. Neither was a contract: one was
  "behind the code", the other "not a contract yet" (`README.md`), and what they said was
  either a contract's by now or a description of the code.
- Level 2 is `boot.md` and `command-mode.md`. A description of the code that decides nothing
  isn't a document of a level: it is the header's comments, the Kconfig help, or a guide.

Where their content is:

- The boot flow: `boot.md`. The security model: `security.md`. The wire: `compatibility.md`
  and `command-mode.md`. The conventions: `AGENTS.md`.
- The logs (the RAM log, the log sources, `LOG_MAX_LEVEL`, ramoops): `pb/ramlog.h`,
  `pb/log_source.h`, `pb/log_def.h`, the Kconfig help of `RAMLOG*` and `BOOT_LINUX_RAMOOPS`,
  `src/boot/linux_ramoops.c`, `testbench/README.md`.
- The ROM's event log and the HAB log, the addresses per SoC and their sources:
  `plat-imx.md` 2, moved there.
- The host tools' promise: `system.md` 3, which had it. The Python package's modules, the
  transports and `pbstate`: the code. The 2.x host and the measurement against it: D-021,
  `log/2026-10-29.md`.
- The table of platforms and boards and when each was last touched: `status.md` and git.

Derived by me, not asked of Jonas:

- `system.md` 3 names `pbstate` beside the host tools: its version is the release's, and
  its library's SONAME changes only with its ABI (`libpbstate.so.0`). It was only in the
  removed document and in `tools/pbstate/CMakeLists.txt`.
- F-232 keeps what the firmware overview lacked (storage, USB, exceptions) as missing; the
  drivers' contracts cover the first two.

## D-063: Code that decides nothing needs no document first (2026-11-12)

Jonas: "process implies that there can't be any change to the code base without first a
document change. I'd like to challenge that. There may be, for example a new USB driver, that
does not drive any documentation."

`process.md` 1 said it with its scope ("a change to what pb does at a boundary"), and three
places dropped the scope: its first lines ("the code follows its documents"), "when the
document doesn't say, the document comes first", and section 5, which had no way out for a
change that touches no level.

- A document comes first for a decision: what the other side of a boundary may rely on.
- Code that decides nothing goes straight to the code: a new implementation of a contract (a
  driver, a board, a port), a refactoring, a test, the build. A bug fix as before.
- A new implementation still updates the records, in its own commit: the conformance table,
  the port's document. Those are facts from below, not decisions.
- When the code needs what the contract doesn't say, or says otherwise, the contract changes
  first.
- An interface whose contract isn't written yet (the drivers', today) has its header and its
  implementations for a contract until then. A new implementation doesn't wait for the
  document; what it shows is a finding for it. Jonas agreed to this one as proposed.

Written in: `process.md` (its first lines, section 1, section 5 step 1), `README.md`'s first
lines.

D-057 ("the documents that guide the code come before the code") stays what it was: the order
of the 3.0.0 work, where the contracts are being written for code that changes with them. It
is not a rule that every change waits for a document.

Mine, not asked of Jonas: the title of `process.md` ("documents first, top down") and D-033's
name stay.

## D-064: The drivers' contracts, as drafts (2026-11-12)

Jonas: the remaining contract work, after `command-mode.md` (D-057). Four drafts in
`doc/interfaces/`, level 3:

- `drivers.md`: what every driver does, and the one rule for DMA and caches.
- `bio.md`: block devices, the storage drivers and the partition tables.
- `crypto.md`: hashing and signature verification, the CAAM and Mbed TLS.
- `usb.md`: command mode's transport, pb's USB device and the device controllers.

What they take from the documents above, and so don't decide:

- Whoever started a driver stops it before the jump (`platform.md` 4.5, `board.md` 5.3).
- No driver kicks the watchdog; the core kicks per chunk (`platform.md` 3.3). A failure is
  never a hang (`system.md` 5.4), so every wait is bounded.
- Command mode's USB is the platform's and its transport is linked (D-049). What command mode
  needs of the transport is `command-mode.md` 6; a transfer that fails ends the session.
- Partial blocks: a write's last block is padded with zeros (`command-mode.md` 5.4).
- A failed verification engine refuses the boot (`security.md` 4.5); a check that can't be
  made is a no (4.4); the storage and the host are input (3).
- The device's identity never changes (`system.md` 5.5): its MD5 stays in every backend.

What they propose, for Jonas (twenty open items, five in each):

- `drivers.md`: one common document and one per class; `include/pb/dma.h` as the only place
  the cache is handled for DMA; a caller's buffer that doesn't own its cache lines is bounced
  by the class's core, so that no image is refused for its load addresses; the line size is a
  constant, 64; only a driver whose device acts without being asked has a `stop`.
- `bio.md`: the core owns partial blocks, drivers see whole blocks; a read at a byte offset
  for the boot (F-220); an installed table is in effect at once; `bio_write()` never erases,
  as released; a storage driver leaves its device as it is at the jump.
- `crypto.md`: no abort, `hash_init()` and `dsa_verify()` start from an idle engine (F-181);
  PB_OK if and only if `*verified`, and a bad signature is `-PB_ERR_SIGNATURE`; no
  verification while a hash is open; one backend per algorithm, no fallback at run time;
  `HASH_MD5_BROKEN` stays.
- `usb.md`: the transport's six functions by name; every error from `complete` is "lost", and
  a controller reports a bus reset and an unplug as a transfer's error; high speed or not
  ready; an unknown request is stalled (a new operation); a request on endpoint 0 leaves a
  bulk transfer alone.

None of them refuses what a release accepted. What changes for a host: an unknown control
request is stalled where it timed out, and a restart of the transport leaves the bus first.

Mine, not asked of Jonas: the four file names; that the conformance suites are named in each
draft and not designed; `command-mode.md` 6 no longer says that the 8X's controller can't
send its serial number (F-159 is fixed).

## D-065: `drivers.md`, Jonas's review: the first answers (2026-11-12)

Jonas's comments in the draft, and what was written in:

- **Open (1), "One file per class"**: a contract per class, each in its own file (`bio.md`,
  `crypto.md`, `usb.md`). Read by me as: `drivers.md` stays for what is common to every
  driver, the DMA rule among it, which the three cite. To be corrected if he meant no common
  document.
- **Open (4), "ok"**: the line size is a constant, 64 bytes, checked in `arch_init()`.
- **No IOMMU is not a rule**: "There is no IOMMU in any of the currently supported platforms.
  This might change though. We should not make it sound like a rule." Agreed. Section 4 now
  says it as a fact of today's platforms, apart from the cache rule, which holds with or
  without one. A driver turns a pointer into the device's address in one place, so that a
  translation has somewhere to go; the entry for an IOMMU is written with the first platform
  that has one.

Asked, not decided:

- **Open (3), "guide me"**: the draft had every class's core bounce a caller's buffer that
  doesn't own its cache lines. My suggestion after his question, written in as the proposal:
  sort the buffers by whose they are. pb's own and a board's are declared right, and the
  class's core refuses one that isn't (`-PB_ERR_ALIGN`): loud, where a shared line is silent.
  The only buffer pb doesn't choose is an image part's load address, and only that is
  bounced, in the boot's load loop, which serves a partition and the host's stream alike. No
  image is refused (D-008), the common path stays without a copy, and the bounce is one place
  that a host test covers. `bio.md` 3.1 and `usb.md` 2.3 follow.
- **Open (2), "explain again"**: why `include/pb/dma.h` and not the arch's functions at each
  place. The reasons are now in 4.4, with the lighter alternative (the arch's three declared
  in `include/pb/arch.h`, the line size and the macro, the order left to the checklist).

## D-066: `drivers.md` is reviewed (2026-11-12)

Jonas, after D-065's answers: "Okay to your suggestions."

- **Open (2)**: the DMA rule lives in `include/pb/dma.h` (`PB_DMA_ALIGN`, the macro for a
  buffer that owns its lines, `dma_out()`, `dma_in_begin()`, `dma_in_end()`). Drivers stop
  calling the arch's cache functions. The lighter alternative (the arch's three declared in
  `include/pb/arch.h`, the order left to the checklist) is not taken.
- **Open (3)**: the class's core refuses a buffer that doesn't start on a cache line
  (`-PB_ERR_ALIGN`). An image part's load address, the one buffer pb doesn't choose, is
  bounced in the boot's load loop, so no image is refused (D-008). Out-of-tree boards get a
  line in the migration guide for 3.0.0.
- **The common document stays**: `drivers.md` for what every driver does, a file per class
  beside it, as D-065 read his "one file per class".

Mine, to be corrected if wrong: Open (5), which drivers have a `stop`, had no comment in his
review and wasn't one of the two suggestions I asked about. I took the uncommented text as
accepted, as with `board.md` (D-052), and closed it with the others: a driver has a `stop`
when its device acts without being asked, and one whose calls are synchronous has none.

`drivers.md` is reviewed, with no open items. Its code: F-282 (the header, the checks, the
bounce in the load loop), F-281 and the findings its conformance table names.

## D-067: `bio.md`, Jonas's review (2026-11-12)

Jonas's comments in the draft, and what was written in:

- **3.2, `bio_write()`**: "we should just say that this is optional. Don't mention WRITABLE
  here." It is `[optional]`; a device without a write answers `-PB_ERR_NOT_SUPPORTED`. The
  flags gate the host and say nothing of which callbacks a device has.
- **Open (4), `ERASE_BEFORE_WRITE`**: "This should be implemented, let's save it as a future
  task. I think we would at least need to hammer out the erase block accounting." And on
  section 6, where the draft fixed that a write never erases: "I think we can challenge this.
  To me it should be possible to fix bio_write so it respects that flag." So the draft's
  proposal (as released, for good) is not taken. `bio_write()` is to erase what it writes on
  a device with the flag. It isn't designed in this contract: `roadmap.md` has the task with
  what the accounting must settle (the unit, larger units, the time against the watchdog and
  the host's timeout, blocks erased twice, how a host learns it, a failure after the erase).
  Until then the writer erases, as released. What section 6 keeps fixed is the other side: a
  host that erases and then writes still gets its bytes written. `compatibility.md` follows.
- **Open (5), section 5**: "OK". A storage driver leaves its device as it is at the jump.

Asked, not decided:

- **3.3, "Is blocks the right argument here or should we use bytes?"** My answer, written in
  as Open (6): blocks. A read and a write take bytes because the core can make a partial
  block whole (it pads and trims); an erase can't be padded, so a byte count that isn't whole
  blocks could only be refused. The wire's `PART_ERASE` counts blocks too.

Mine, to be corrected if wrong: Open (1), (2) and (3) had no comment (partial blocks are the
core's, `bio_read_at()`, an installed table in effect at once with
`bio_release_children()`). As with `drivers.md` (D-066) I took the uncommented text as
accepted; the table at the document's top marks them "Jonas, no comment".

`bio.md` stays a draft until Open (6) is answered.

## D-068: `bio.md`, Jonas's answers on the uncommented items (2026-11-12)

I had closed the draft's Open (1) to (3) as "no comment" (D-067) and asked. Jonas: "1. Yes,
2: Hm, no for now. 3: Explain more."

- **Open (1), yes**: the core owns a length that isn't whole blocks; a driver sees whole
  blocks only.
- **Open (2), no for now**: `bio_read_at()` is taken out. A read starts on a block. An image
  whose later parts don't start on a block of its device (bpak aligns to 512 bytes, the NOR
  has 4 KiB blocks) fails with `-PB_ERR_ALIGN`, as released; a one part image always loads.
  `roadmap.md` notes the read at a byte offset; F-220 keeps the limit.
- **Open (3), reopened**: the draft made it look like one function. What "in effect at once"
  takes is written into section 4: the stream ends, the table's partitions are released, and
  what `board_setup()` does after `init()` runs again (the table, the boot source policy,
  `late_init()`, the boot's set up), because each of them holds partition handles. The other
  way is in the text too: as released, the new table is used from the next reset.
  `command-mode.md` 5.3 (reviewed) says "in effect for the commands that follow"; whichever
  Jonas picks, the two documents say the same.

So "no comment" was the wrong reading for one of the three. Uncommented open items are asked
about from here on, not closed.

Still open: (3), and (6) from D-067 (an erase counts blocks).

## D-069: `bio.md`: an erase counts blocks; the installed table, a second proposal (2026-11-12)

- **Open (6)**: Jonas: "ok, let's stick to blocks". `bio_erase()` takes a count of blocks.
- **Open (3)**, still open. Jonas: "Of course 'A' is tempting. But let's imagine a board who
  holds stale references, how would this work in practice?" Worked through, the first
  proposal (release the partitions, run `board_setup()`'s second half again) fails on that
  question in two ways. A handle is an index into the pool, so after a rebuild a board's old
  handle names whatever partition got the slot: I/O on the wrong partition, silently (the
  host board's fuse box keeps a handle from `late_init()` and rewrites block 0 through it).
  And a board holds more than handles (sizes, content it read, what it registered), which
  only a `late_init()` that is safe to run twice would refresh; one that isn't shows at an
  install, in the factory.

  The second proposal, written into section 4: the rule is on the handle. An unchanged
  partition keeps its handle and its flags; a changed or removed one's handle is dead until
  the reset and isn't given to another partition; a new partition gets a new handle. The core
  sets up again what is its own (the stream, the policy's init). No hook of the board's runs
  again: a stale reference in a board is either still right or fails with `-PB_ERR_PARAM`.
  The first proposal and "after a reset only" are in the text as considered.

## D-070: `bio.md` is reviewed; an installed table is in effect at once (2026-11-12)

Jonas, after D-069's second proposal: "Okay, let's go with A."

A was the name of "in effect at once" against B, "after a reset". Read by me as: at once, in
the form his question about stale references led to, which is the proposal that stood in the
text when he answered. Not the first form (run `board_setup()`'s second half again), which
that question took apart. To be corrected if he meant the first.

- An installed table is in effect for the commands that follow (`command-mode.md` 5.3).
- A handle never comes to mean another partition: a partition with the same UUID and blocks
  keeps its handle and flags; a changed or removed one's handle is dead until the reset and
  isn't given out again; a new partition gets a new handle.
- The core sets up again what is its own: the stream ends, the boot source policy's init
  runs again. No hook of the board's runs again.
- A pool without room for both tables' partitions: the table is on the device, the install
  answers the error, a reset uses it.

Written in: `bio.md` 4 and its checklist; `command-mode.md` 5.3 cites it; `board.md` 6 says
what a kept handle is worth after an install. F-194 carries the code.

`bio.md` is reviewed, with no open items (D-067 to D-070).

## D-071: `crypto.md` is reviewed (2026-11-12)

Jonas's comments in the draft, one on each of the five open items:

- **Open (1), (2), (3): "OK".** No abort: `hash_init()` starts from an idle engine. PB_OK if
  and only if `*verified`, and a signature that doesn't verify is `-PB_ERR_SIGNATURE`.
  `dsa_verify()` while a hash is open is refused.
- **Open (4)**: "It might make sense to have more than one backend. Let's say we have
  hardware hash acceleration but not signature verification. Do you agree?", "It's fine to
  not have fallback", and on the bullet that a backend has every algorithm: "Unsure about
  this. What I want is to have for example a hardware driver to supply accelerated hashing,
  while a software driver could give us the verification. If that's what you mean, then it's
  fine." Agreed, and it is what "one backend per algorithm" was meant to say; the text read
  as if one backend had to have everything. Written now as: a build may have several
  backends and each algorithm comes from one of them; together they have what the build's
  keys and images use, no single one needs it all; no fallback at run time, and an algorithm
  that two backends register is refused at the second registration (F-286 (12): today the
  first wins, silently).
- **Open (5)**: "It stays." `HASH_MD5_BROKEN` stays. Which products have it wasn't said.

Mine, found while writing it in: the draft said both that the next `dsa_verify()` cleans up a
hash that was given up (3.1) and that `dsa_verify()` is refused while a hash is open (4).
The second stands, as Jonas confirmed it: the cleanup is the next `hash_init()`, and until
then a verification is refused, which is a no. Every verification hashes first, so no flow
is stuck. A backend whose engine also hashes still starts its verification from an idle
engine (section 4).

`crypto.md` is reviewed, with no open items.

## D-072: `usb.md` is reviewed (2026-11-12)

Jonas's comments in the draft, one on each of the five open items:

- **Open (1), "ok"**: the transport's functions are `cm_transport_init()`, `_connect()`,
  `_read()`, `_write()`, `_complete()` and `_disconnect()`.
- **Open (2), "ok"**: every error from `complete` is "lost"; a controller reports a bus reset
  and an unplug as an error of the transfer. No new call.
- **Open (3), "That's fine for now"**: a full speed bus isn't supported: high speed, or not
  ready. For now; `roadmap.md` has a line.
- **Open (4), "ok"**: a request the stack doesn't know is stalled, with a new operation,
  `ep0_stall`.
- **Open (5), "ok"**: a request on endpoint 0 leaves a bulk transfer alone.

`usb.md` is reviewed, with no open items. With it the four drivers' contracts are reviewed
(D-064 to D-072), and every document of levels 0 to 3 is reviewed or in force.

## D-073: The final read of the drivers' contracts: seven answers (2026-11-12)

After the four were reviewed I read them together and listed what needed a decision. Jonas:
"1 to 7: yes, go with your recommendations."

1. **No fixed second for a driver's call.** `drivers.md` 3 said a synchronous call returns
   within a second, a number of mine, while `bio.md` 3.1 puts no limit on a read's length.
   A call's time follows from its request: the driver's header gives the longest time per
   unit, and the core asks in pieces that fit the watchdog (`platform.md` 3.3).
2. **The UARTs get no contract**, though they are two drivers behind one interface: it is
   one output function, only the log rests on it, and `board.md` 2.2 says what the console
   is. `drivers.md` 1 no longer says that a second implementation always brings a contract.
3. **`bio.md` 2.6, finding a device**: by its UUID, all of them (every handle is tried and
   the ones that aren't a device are skipped, since a dead handle can lie between two live
   ones, D-070), what a device is, and how long a handle is good. Two gaps of the code are
   named with it (F-283): a second device with a taken UUID isn't refused, and the block
   getters return `int`.
4. **Three requests the code answers are stalled**: the debug descriptor (answered with the
   interface descriptor), `SET_IDLE`, and a string index the class doesn't have (answered
   with an empty string). A host sees the difference; `usb.md` 6 lists it with the stall of
   unknown requests and the restart that leaves the bus.
5. **A host's transfers inside a part of `BOOT_BPAK` are whole packets but the part's
   last.** Released hosts send the caps' largest transfer, which is; `usb.md` 2.3 and
   `command-mode.md` 4.4 say it now. Without it a read in the device's own pieces meets a
   short packet.
6. **`md_length` is exactly the digest size of `md_alg`**, checked in the core. The token's
   call passes 64 whatever the hash; it is fixed in the same commit, after the tokens that
   verify today were tried on both backends.
7. **The lint is to check the drivers' contracts' entries** against their headers, as it
   does for `platform.md` and `board.md`: F-288.

Not decided, noted in my review and left: a `STREAM_PREPARE_BUFFER` of 0 bytes, USB suspend,
the pool running out after several installs, how a product selects `HASH_MD5_BROKEN`, and how
each conformance row is to be verified.

## D-074: An entry in an empty or missing part is refused; security before compatibility (2026-11-12)

Mine, made while Jonas had asked for the code without questions; it waits for his review.

F-292: an image whose entry part is empty passed placement and the payload's hash, since
nothing is hashed for it, and pb jumped to its load address, into bytes no hash covers. The
same for a kernel or a BL31 that has a load address and no part at all. Every release did so,
and the bpak CLI builds such an image. The image needs a valid signature, so it is a signer's
mistake or a signer's key that is misused, not an open door; what runs is still whatever
the storage held at that address.

- **Refused**, in the handover's and the delivery's prepare: the image handover's kernel, the
  TF-A delivery's BL31 and a BL32 the board names, the generic handover's entry part, when the
  part is missing or shorter than what the jump reads there (4 bytes; 8 for the generic
  handover, ARMv7-M's two words).
- **Still accepted**: an empty part nothing jumps to (a ramdisk, a DT or BL32 the board doesn't
  name).
- **Why it may break D-008**: `security.md` 4.1 and 4.2, only bytes the signed hash covers may
  run, and security comes before compatibility (D-035). No golden image has such a part.
- **Not decided**: the sentence for `boot.md` 5.1 ("Every entry lies in a part of the image,
  in bytes the payload's hash covers ...") is Jonas's to add; F-331 stays open for it.
- **Also refused** (F-330, added with its fix): a device tree part the board names that is
  smaller than an FDT header (40 bytes), before anything at its load address is read, and a
  header whose `totalsize` is below that. Every release accepted such an image: what was at
  the address got pb's `/chosen` and was handed on as the device tree. The same reasons, and
  no valid device tree is that small.

## D-075: The decisions have an index, the one part that is edited (2026-11-12)

Jonas, after a review of the working records against the reviewed contracts: yes to an index.

The entries are never edited (this file's first lines), so an entry that a later one replaced,
narrowed or closed reads as if it held as written: by this entry that is 18 of 71. The
newer entry names the older one, never the other way round. The table at the top gives each
entry its state, and it is the one part of this file that is edited: an entry that replaces,
narrows or closes an older one changes the older one's row with it.

The states' words and each row's state are mine, read from the entries. A row that is wrong
is corrected in the table; it needs no entry.

From the same review (`log/2026-11-12.md`): `roadmap.md` starts with what 3.0.0 waits for
besides the code, and `status.md` is short again.
