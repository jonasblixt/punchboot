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
