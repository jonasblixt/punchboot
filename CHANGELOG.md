# Changelog

The release notes, newest first. The releases before 3.0.0 have no entry: their history is the
tags' (`git log v2.7.0..v2.7.1`).

Each line names its source. D-nnn is a decision (`doc/dev/decisions.md`), F-nnn a finding
(`doc/dev/findings.md`, or `findings-closed.md` once it is fixed), and a date is that day's log
(`doc/dev/log/`).

## 3.0.0

**The wire protocol and the bpak image format are compatible with every release from 1.0.0
on** (D-008, D-018, D-035). A 3.0 host works with a 2.x or a 1.x device, and a released host
with a 3.0 device. An image that a release accepted is accepted, with the exceptions under
"Images that are refused now", each of which has its decision.

**The major version is for the porting API** (D-018): `struct board`, the platform's and the
drivers' interfaces, and the build's options changed, so a board, a driver or a platform out of
the tree has to be ported. `doc/guides/migration-3.0.md` says how. Hosts and devices of 2.x and
3.x can be mixed, and a pin such as `punchboot~=2.7` or `<3` only keeps the fixes out.

New since 2.7.1, among more: the i.MX8M Plus and its EVK, SD cards, a log in RAM that a host
reads (D-018), named commands (D-076), two more boot source policies (D-046), and the host
tools in pure Python (D-021).

### What a host sees change

Before authentication, on a locked device:

- Every frame but the device's identifier and the authentication answers `NOT_AUTHENTICATED`:
  also a command number the device doesn't have, and a frame with a wrong magic. Every release
  answered `INVALID_COMMAND` there. A host that probes for a command authenticates first
  (D-058, F-275).
- A failed authentication is answered one second after its data arrived (D-058, F-276).
- The MEK's and the DXL EVK's example password is compared whole. A prefix of it opened them
  before (F-004, 2026-11-13).

Partitions:

- A partition the board hides answers `NOT_FOUND` to every command. No board in the tree hides
  one (D-058, F-272).
- A stream offset inside a block is `INVALID_ARGUMENT`. It was rounded down (D-058, F-273).
- An erase needs the partition to be writable, as a write does, and answers `IO_ERROR`
  without. A count of 0 still erases nothing and answers OK (F-188, 2026-11-12).
- The device a partition table is on can't be written as a whole, with any kind of table: its
  partitions are written. On the RT1060 EVK that ends writes to its NOR outside the partitions
  (F-265, 2026-11-12).
- A partition table that is installed is in effect for the commands that follow, and the
  stream ends with the install (D-070, F-194, 2026-11-13).
- A command timeout of an MMC or SD card answers `TIMEOUT`. It answered `IO_ERROR` (F-199,
  2026-11-13).

Boot:

- A new result code, `PB_RESULT_BAD_PAYLOAD` (18): an image's payload doesn't have the header's
  hash. Devices answered `ERROR`. A released host reports its generic error for the new code,
  which is what it reported before; the 3.0 host raises `BadPayloadError`, a `GenericError`
  (F-087, 2026-11-13).
- **A boot is refused when the SoC's TrustZone DRAM filter is bypassed**, whoever starts it and
  in every life cycle state, and when the platform's crypto engine failed. The host gets an
  error, and the reason is in pb's log: "No boot: the TrustZone DRAM filter is bypassed"
  (F-248, 2026-11-14).
- **A pb that the ROM's serial downloader loaded stops in command mode** and doesn't boot from
  storage by itself (`CONFIG_CM_ON_SERIAL_DOWNLOAD`, on by default). A boot the host starts
  (`punchboot boot ...`) isn't stopped. jiffy booted from storage after `uuu` before (D-031,
  F-251, 2026-11-14).
- A `boot bpak` that is refused before the image is read still reads the header's transfer,
  and the header's result has the refusal (D-058, F-128).
- When a board's hook stops a boot that the host started before the device has left the bus,
  the host gets an error and the device stays in command mode. It reset (F-252, 2026-11-13).

Named commands (D-076, 2026-11-13):

- One new command, `COMMAND_LIST`, and `punchboot command list` and `punchboot command <name>`
  in the host tool: a device says which commands it has, with a help line each. A device older
  than the command answers `INVALID_COMMAND`, and a command is still run by its name.
- A board's command keeps its id, and `punchboot board command` works as released.
- An id the device doesn't have answers `NOT_SUPPORTED`. The RT1060 EVK, imx8mevk, the MEK and
  the DXL EVK answered OK to any id (F-218).
- jiffy's board command `dump-rom-log` is gone: `punchboot log rom` reads the ROM's log
  (`4bb4c2f6`).
- The MEK's and the DXL EVK's `board status` answers "Board status OK!". What it printed is
  the platform's command `info` (2026-11-14).

Logs: `punchboot log` reads pb's own log, `punchboot log <name>` another of the device's, and
`punchboot log --list` names them. A device without the command answers `INVALID_COMMAND`
(`doc/system/compatibility.md`, "Example: selecting a log").

The session:

- A transfer inside a command that the host doesn't complete within 10 s ends the session,
  and the device waits for a host again. The capabilities carry the time in
  `operation_timeout_ms`, which was zero in every release (D-058, F-277).
- A host that sets the device's address or its configuration again has started over: the
  session and its authentication end. `punchboot` doesn't do that between two invocations
  (F-363, 2026-11-13).

The host tools (`pip install punchboot`):

- Pure Python: one wheel for every platform, no C extension and no C host library. `Session`
  and the exceptions keep their API (D-021).
- `part write` fails when the device's last step of the write fails. `boot enable` needs its
  partition. The CLI prints what failed, with the error's message (F-223 to F-225,
  2026-11-13).
- For a program that uses the package: `part_verify` above 4 GiB raises `ArgumentError`; after
  a transfer error later calls raise `OSError`; `part_get_partitions` doesn't raise
  `ValueError` for flag bits it doesn't know; `Transport.set_timeout` is new (2026-11-13).

`pbstate`, where it did the wrong thing without saying so (2026-11-13):

- `-v X` for a slot that isn't the enabled one keeps the enabled slot's trial and its attempts.
- `-s` with a value that doesn't start with a, b or n exits 22. It disabled both slots and
  exited 0.
- `-s a -c N` gives N attempts from 256 on, and `-w I V` with a V that isn't a number exits 22.
- A read doesn't `sync()`, and `libpbstate.so.0` doesn't export `crc32`.

### What the next stage sees change

- Four new properties in `/chosen`: `pb,version`, `pb,boot-partition`, `pb,boot-mode` and
  `pb,status`. The released ones keep their bytes. A device tree without room for a new one
  boots, and the property is left out (F-260, F-248; the README has the sizes).
- With both A/B slots verified, an enabled slot that fails to load or to verify no longer ends
  in command mode: the other slot boots, for that boot, and nothing is written. The state
  still names the slot that failed as enabled, and `pb,boot-mode` is `fallback` (F-174,
  2026-11-12).
- The watchdog is kicked after each chunk that is loaded, and before the jump (F-243).
- An image on ARMv7-M starts with the D-cache off and PRIMASK set (D-028), and gets Arm's
  Firmware Handoff registers and a transfer list where it got nothing (D-039).
- `linux,initrd-start` and `-end` are the bytes every release wrote. Only a ramdisk that
  reaches 4 GiB gets 64 bits for both, which never worked before (F-255).

### Images that are refused now

| Image                                | Why, and whether one booted before                | By    |
| ------------------------------------ | ------------------------------------------------- | ----- |
| A header that bpak 0.9's validation  | Always invalid: a used part or meta slot after an | D-024 |
| let through and bpak 1.0's doesn't   | empty one, meta data at an offset that isn't a    |       |
|                                      | multiple of 8, `pad_bytes` of 512 or more, sizes  |       |
|                                      | that overflow, a signature over 512 bytes. Every  |       |
|                                      | golden image of bpak 0.9.3 and 1.0.1 boots        |       |
| A part with the `dont-hash` flag     | None booted: such an image failed on its          | D-035 |
|                                      | payload's hash in every release. It is refused    |       |
|                                      | before anything is loaded now, with               |       |
|                                      | `NOT_SUPPORTED`                                   |       |
| A part outside the board's image     | One could boot: a release loaded a part anywhere  | D-055 |
| data regions, or over another part   | that wasn't pb's own memory. Security comes       |       |
|                                      | before compatibility here. The regions are the    |       |
|                                      | board's, and a product lists what its images use  |       |
| The kernel, BL31, a BL32 the board   | Every release started such an image: it jumped to | D-074 |
| names or the generic entry in a part | the part's load address, into bytes no hash       |       |
| that is missing or empty             | covers. The image needs a valid signature, so it  |       |
|                                      | is a signer's mistake. No golden image has one    |       |
| A device tree part the board names   | Every release took what was at the part's address | D-074 |
| that is smaller than a device tree's | for the device tree. No valid tree is that small  |       |
| header (40 bytes)                    |                                                   |       |
| A device tree part in a partition    | pb would grow and patch the tree where it is, in  | F-291 |
| that is booted in place              | the flash. A kernel alone boots in place. In      |       |
|                                      | place is 3.0.0's form of the 2.x source that      |       |
|                                      | verified an image in memory (D-048): a board that |       |
|                                      | used that with a device tree has no equivalent    |       |
|                                      | yet                                               |       |
| On the DXL EVK: an image without the | It booted: 2.x started the part `atf` and named   | F-295 |
| part `kernel`                        | no BL33. The board in the tree now goes through   |       |
|                                      | TF-A with `kernel` as BL33. Not run on a board    |       |

The custom source, where a board authenticated the image itself, is gone: it was the one
way around the core's verification (D-035).

### Before pb is updated on a device in the field

The board is ported first (`doc/guides/migration-3.0.md`). Its last section lists what a port
can change by mistake that a device in the field relies on. Then check:

- **The TrustZone DRAM filter**, on a SoC that has one. A product whose set up before pb leaves
  it in bypass doesn't boot with 3.0.0: pb refuses, and stops in command mode. On the i.MX6UL
  the board's DCD takes it out of bypass, as jiffy's does; a pb that a debugger loaded has no
  DCD and refuses too (`doc/interfaces/platform.md` 2.3, 2026-11-14).
- **The images in the field against the table above**: every part inside the board's image
  data regions, the entry's part there and not empty, no `dont-hash` part. The boards in the
  tree leave out the DRAM next to pb and its buffers, which 2.x loaded into (F-392).
- **The revoke slot of each key.** The board's key map is gone, and `CONFIG_KEYSTORE_SLOTS`
  gives the slots: a key must keep the fuse bit it had (D-027).
- **Whatever loads pb with the ROM's serial downloader** and expects it to boot from storage:
  the factory's flow, a recovery script. It sends a boot command now, or the build turns
  `CONFIG_CM_ON_SERIAL_DOWNLOAD` off (D-031).
- **Hosts and scripts** against "What a host sees change": one that probes a locked device and
  waits for `INVALID_COMMAND`, one that sends a prefix of the password, one that relies on OK
  for a board command the board doesn't have, one that writes the device a partition table is
  on.
- **The OS's updater.** After a fallback the running system is not the one the A/B state
  names as enabled. An updater that writes "the slot that isn't enabled" would write over the
  system it runs from: it reads `pb,active-system` (2026-11-12, "The policy is an interface").
- **What confirms an update.** `pbstate -v` for the booted slot no longer ends another slot's
  trial (2026-11-13).
- **A device tree built without free space**, for a headroom of 0: it boots, without the new
  properties (F-260).
- **An image for ARMv7-M** that relied on the D-cache being on or on interrupts being unmasked
  at its entry (D-028).
- **A GPT that another tool changed.** A sane table gives the same partitions and nothing is
  written. An entry that covers the table's own blocks is no longer a partition, and a device
  whose backup table is stale logs one error line per boot (2026-11-13, "The GPT").
