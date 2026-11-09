# Decisions

Short records of decisions and why they were made. Add new ones at the bottom,
and never edit old ones; supersede them with a new entry instead.

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
