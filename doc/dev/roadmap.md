# Roadmap

What comes next, and what is to be discussed. Where the work is, and the order of the code
inside 3.0.0, is `status.md`. The first roadmap (2026-10-22: a dev loop, fixes, dependencies,
host tools, cleanup and docs, as phases 0 to 5) is in git. Its phase 0 is done; what was left
of the others is under "Compatibility guards" and "Left from the first roadmap".

## Before 3.0.0 is tagged

Collected 2026-11-12 from the documents and the decisions. The list is mine and not reviewed
(Jonas). Not decided: which open findings gate the tag.

- **The contracts' code**, a commit per finding (`process.md` 5): each contract's "Not yet in
  the code" table. What changes the interface an out-of-tree board builds against comes only
  in a major release (D-031), so it is in 3.0.0 or waits for 4.0.0. By my reading that is
  F-259 (Firmware Handoff in `simple`'s place), F-267 (`cm_init` and `cm_transport` leave
  `struct board`), F-268 (tasks, `process` goes), F-247 (the RoT's and the SLC's functions
  move to the platform), F-236 (`plat_get_boot_reason()`), F-239 and F-240 (the addresses
  and the build's stages), F-270 (the 8X boards' includes) and F-282 (`pb/dma.h`: a buffer
  that doesn't own its cache lines is refused).
- **The Firmware Handoff tag** (`boot.md` 5.4, D-038): pb's structure isn't in a release
  before its tag is allocated in the specification. Jonas requests it upstream, with a pull
  request there, so it is asked for early: it waits for other people.
- **The bench run**: since 2026-11-12 the four boards run the branch and boot their test
  bench images (`log/2026-11-12.md`, its last entry, with what that didn't show). The rest
  of what each change has to show is there under "Not run on a board". Never run on any
  board: fuse writes, the 8X closed state, imx8mevk and the DXL.
- **A test record for the release** (`process.md` 7): what ran on which board and what never
  did, next to `guides/release-checklist.txt`.
- **The migration guide** (`guides/`): what an out-of-tree board changes from 2.x. Promised
  or implied so far: `struct board` and its objects (D-026, D-046), `late_boot` without the
  header (D-047), in place as a flag of the static table (D-048), no `cm_init` and no
  transport (D-049), tasks for `process` (D-051), `mmap` and `image_regions` (D-054, D-055),
  the 8X boards' includes (D-056), buffers for DMA (`drivers.md` 4, D-066), the boot
  driver's new name and then Firmware Handoff (D-028, D-039), the MMC HAL, the uSDHC's set up
  and the DDR drivers (D-018), the serial download stop and `CONFIG_CM_AUTH_PASSWORD`
  (D-031).
- **The porting guide** (`guides/`): how a board and a port are written, citing the
  contracts.
- **The release notes** say first that the wire and bpak are compatible and that the major
  bump is for the porting API (D-018). Then what a host sees change: `NOT_AUTHENTICATED`
  before authentication on a locked device, hidden partitions, a stream offset inside a
  block, the delay after a failed authentication (D-058); the stalled requests and the
  restart that leaves the bus (D-073, `usb.md` 6). And the images that are refused now
  (D-024, D-055, D-074).
- **A changelog**: the release checklist's step 2 names one and there is none (F-232). The
  checklist itself is stale (F-044).

## Compatibility guards (D-008, `compatibility.md`)

`compatibility.md` cites this list as "roadmap Phase 1". In order of value per effort:

1. Done: frozen layout file `tests/compat/abi.txt`, only-grows check in CI and pre-commit,
   `_Static_assert`s on the struct sizes (`compatibility.md`, Guards).
2. Done for bpak 0.9.3 and 1.0.1: golden images in `tests/compat/bpak/`, booted from RAM and
   from a partition by `test_compat.py`, plus pinned ids and the Python mirrors. Still open:
   images from older bpak CLIs (0.5-0.8 need Mbed TLS 2.x to build).
3. Old hosts against the current device: a CI job installs released `punchboot` wheels from
   PyPI (1.0.0 and up, as available: the floor, D-035) and runs a read-mostly subset of the
   integration tests against today's pb-host.
4. Recorded transcripts: raw command/response bytes for each command, captured once. Done
   (D-021, `test_transcripts.py`): served by a fake device to the current host library (new
   host → old device). Open: replayed against pb-host (old host → new device), and, from the
   first release after this branch, each release's pb-host kept to test the current wheel
   against it.
5. CODEOWNERS + a PR template checkbox for the files listed in `compatibility.md`.

## From the level 1 review (2026-11-11, D-035)

To do (Jonas):

- **Anti-rollback**: a version floor, fuse-backed. A version below the floor fails verification, so
  the boot source policies don't change; it may change what a fallback may boot (a CVE).
- **Examples for `password_auth()`**: a few good ones, such as a secret in the RPMB. A board's own
  implementation stays allowed (`board.md` 4.4, F-004).
- **The compatibility floor is v1.0.0**: the guards test hosts and firmware from the 1.0.0 release
  on (guard 3 above).

To discuss (Jonas):

- **EOL** (`system.md` Open (1)): the SoC's end of life state (NXP's field return), behind Kconfig;
  a product may not allow it at all.
- **Encrypted images** (`security.md` 2): confidentiality isn't protected today.
- **Authentication methods** (`security.md` 4.7): authentication is pluggable. A token limited in
  time, on platforms with secure time. Capabilities: a token for a return analysis that only
  allows reading certain logs.
  Jonas (2026-11-12, D-058): not designed in `command-mode.md` yet, he wants more time on it.
  His idea: a manifest of the commands a session may run, sent as the token together with its
  signature, so that the access control can be as fine as needed. The contract keeps only what
  that needs: the gate is the one place that decides (`command-mode.md` 3.4).
- **Boot failure records** (`system.md` 5.4): counters for failed boots that the system reads
  later, and error logs written to flash in some cases, for the analysis of returned devices.
- **Another image format** (`system.md` 3): bpak solves more than pb needs; a simpler format of
  pb's own, loosely coupled, and whether one build accepts several formats. With it: the keystore
  without the round trip through the bpak CLI at build time (key ids unchanged, the bpak input
  still accepted). The parked `pbi-image-format` branch belongs here. What the boot flow needs
  from a format is `boot.md` 4.4 (D-044); with the format come its interface in the code, the part
  ids a board names and the host's command. What command mode keeps for it is `command-mode.md`
  5.9 (D-060): the bpak commands keep meaning bpak, the format gets its own, the capabilities
  tell which formats a build accepts. A build may leave bpak out (Jonas, D-061). With keys whose
  signature is above 1024 bytes (ML-DSA) comes a new token method (`command-mode.md` 3.3).
- **Erase on the wire**: Jonas may want to change it again (F-075 is accepted, no product used it).
- pb's own update with a rollback (`security.md` 5; my addition).

## From the command mode contract (2026-11-12, D-057)

To do (Jonas):

- **Implement `AUTH_SET_PASSWORD`** (`command-mode.md` 5.8). The slot is on the wire since 2020
  and has answered `NOT_SUPPORTED` in every release: it was never implemented, and never
  retired. Written into `command-mode.md` and `board.md` 4.4 first, with a decision entry. To
  settle there:
  - The request is fixed by released hosts (D-008): `punchboot auth password --set` sends the
    password in the command's request, zero padded, without a length. A transcript has it.
  - Where the secret is kept is the board's: a hook next to `password_auth()`. A board without
    it answers `NOT_SUPPORTED`, as every device does today.
  - Who may set it, and when: it is behind the gate since 2.0.0.
    Whether a password that is set can be changed, or only in an open life cycle state; until
    2.0.0 the command was named `AUTH_SET_OTP_PASSWORD`.
  - The password is never logged and never sent back (`security.md` 4.10).
  - With the examples for `password_auth()` (above): the same storage, such as the RPMB.

## From the drivers' contracts (2026-11-12, D-067)

To do (Jonas):

- **`bio_write()` erases where `ERASE_BEFORE_WRITE` is set** (`bio.md` 3.3). Today the writer
  erases: the host (`part erase`, then the write) and the A/B state. Written into `bio.md`
  first, with a decision entry. To hammer out there, the erase blocks' accounting:
  - The unit. A write erases the blocks it writes, and `bio`'s block on such a device is the
    medium's smallest erase unit (4 KiB on the RT1060 EVK's NOR), so whole blocks can be
    erased exactly. A medium whose smallest unit is larger than `bio`'s largest block has no
    answer yet.
  - Larger units where the range covers one whole (64 and 32 KiB on that NOR), as
    `bio_erase()` does: the board's table gives 300 ms for 4 KiB and 1 s for 64 KiB, so a
    4 MiB buffer is minutes in small units and about a minute in large ones.
  - Time: such a write is far over a second (`drivers.md` 3) and over a host's 10 s. The
    core splits it and kicks the watchdog, and the caps' timeouts tell the host
    (`command-mode.md` 4.6).
  - Blocks that are erased twice, by a host that erases first and then by the write: time
    and wear. Skip a block that is blank (it costs a read), or let the host say that it
    erased.
  - How a host learns that the device erases by itself, so that a new host may stop erasing:
    a capability. A new host against an old device must keep erasing (D-008).
  - A write that fails after its erase leaves blocks erased; the A/B state's two copies
    already assume that. Its own erase goes.
  - A partial last block is padded with zeros (`bio.md` 3.2), so the whole block is rewritten
    and erasing it is right.

- **A full speed bus for command mode's USB** (`usb.md` 4.1): not supported, for now (Jonas,
  D-072). The descriptors are high speed's; a device on a full speed bus doesn't become ready.
- **A read at a byte offset in `bio`** (`bio.md` 3.1, F-220): not for now (Jonas, D-068). It
  would let a device with blocks larger than 512 bytes hold an image whose later parts don't
  start on its blocks. Today such an image is refused; a one part image always loads.

## From the decisions, on no list before (2026-11-12)

To do:

- **Conformance suites** (`process.md` 7; `bio.md` 8, `crypto.md` 9, `usb.md` 8): one per
  contract that can run on the host, against every implementation and the fakes; on the
  bench through the simple target (D-025). They are what brings a contract to "in force"
  (`process.md` 4).
- **The simple target bench on Cortex-A** (D-029): the arch and board split, then the 8MP
  EVK, jiffy and the MEK; boot time to the simple target.
- **The i.MX8M's HAB** (D-027), once its SRK_REVOKE word is verified.
- **New ports**: `nanopi-neo-core2` is a branch to rebase onto master, with its WIP in a
  stash (2026-10-28).
- **Recovery partitions** for the boot source policies (D-032: later).
- **A hardware-in-the-loop tier** (D-019): the crypto engines checked on silicon from the
  host. A QEMU smoke tier for arch code (D-004), which the virtio drivers are kept for
  (F-052).

To discuss (Jonas):

- **D-020 is marked temporary**: code without a user in the tree stays "while new boards
  come in", to revisit when they are in. Since then `principles.md` 7 says the same without
  a date. Either an entry drops the revisit, or it is held; then also what still goes as
  dead (F-030, F-031).
- **Left undecided by the final read of the drivers' contracts** (D-073): a
  `STREAM_PREPARE_BUFFER` of 0 bytes, USB suspend, the pool running out after several
  installs of a table, how a product selects `HASH_MD5_BROKEN`, how each conformance row is
  verified.

## Left from the first roadmap

- Security and correctness: F-004, F-006, F-007 and F-008 are the open ones of F-001 to
  F-008.
- Dependencies: Mbed TLS 3.4 → 3.6 LTS (F-020), or 4.x (`status.md`); trim
  `mbedtls_config.h`. The version of each vendored component in a manifest (F-022).
- Host tools: mypy in CI, ruff-format (F-043). pytest unit tests for the Python layer
  (F-041). `pbtest.py`'s `PB_CMD_*` can use `punchboot.wire` (F-035).
- `pbstate` (tool and library) for every boot source policy, not only A/B (Jonas, 2026-11-11):
  one freestanding core with I/O callbacks shared with pb's firmware (as `src/wire.c` is with the
  host), used on Linux, Zephyr and bare metal, the simple target's copy replaced; extensions from
  `rz[]` where zero is today's behaviour (D-008); Arm's FWU metadata as a model for the concepts,
  not the format. Not Linux specific, it never was. The name stays: scripts and products call the
  tool and link `libpbstate`, and "pb's state" is still what it is (decided 2026-11-11).
- Firmware cleanup: dead libc and xlat files, stale include paths (F-030, F-031). Which
  boards and defconfigs to keep (F-033). `PART_RESIZE` is the one retired command (F-032,
  `command-mode.md` 5.3); its number is never reused.
- User docs: the README (F-044, F-232), then the CLI, the Python API and the wire protocol,
  starting from it. For a port, the documentation is the contracts and the two guides above.

## Later / to discuss

- Build system: keep Make + Kconfiglib, or move to CMake/Meson? The build's stages are an
  interface for out-of-tree boards (`platform.md` 6, D-030), so a move is a major release's.
- Wire protocol version field and capability negotiation. Only next to `PBL0` and the caps
  command, never replacing them (D-008). The capabilities are to tell which image formats a
  build accepts (`command-mode.md` 5.9).
- PyPI trusted publishing (F-228).
- bpak format extensions. Only backward compatible ones (D-008).
- Low prio: move the whole repo to `jonas@blxt.se`. The files are moved (2026-11-14: the
  copyright headers, the licenses, the Coverity upload's email). Left: rewriting the history of
  the ~1000 old commits would change every hash and tag (the PyPI releases point at tags), so
  decide that separately; the Coverity project is still `jonpe960/punchboot`.
