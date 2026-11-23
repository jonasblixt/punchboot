# Changelog

The release notes, newest first. The releases before 3.0.0 have no entry: their history is the
tags' (`git log v2.7.0..v2.7.1`).

Where a decision is behind a line, the line names it: D-nnn, in `doc/decisions.md`.

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
reads (D-018), named commands (D-076), two more boot source policies (D-046), the host
tools in pure Python (D-021), and on the i.MX8M two stages in one image: an SPL that brings
up DRAM and pb proper in DRAM (D-088).

### What a host sees change

Before authentication, on a locked device:

- Every frame but the device's identifier and the authentication answers `NOT_AUTHENTICATED`:
  also a command number the device doesn't have, and a frame with a wrong magic. Every release
  answered `INVALID_COMMAND` there. A host that probes for a command authenticates first
  (D-058).
- A failed authentication is answered one second after its data arrived (D-058).
- The MEK's and the DXL EVK's example password is compared whole. A prefix of it opened them
  before.

Partitions:

- A partition the board hides answers `NOT_FOUND` to every command. No board in the tree hides
  one (D-058).
- A stream offset inside a block is `INVALID_ARGUMENT`. It was rounded down (D-058).
- An erase needs the partition to be writable, as a write does, and answers `IO_ERROR`
  without. A count of 0 still erases nothing and answers OK.
- The device a partition table is on can't be written as a whole, with any kind of table: its
  partitions are written. On the RT1060 EVK that ends writes to its NOR outside the
  partitions.
- A partition table that is installed is in effect after the reset, as before. One that
  changes the partitions makes a write, an erase or an activation of one answer `ERROR`
  until the reset: they were written at their old blocks (D-093).
- A bulk read (`part read`) of a partition the board doesn't let a host read answers
  `NOT_SUPPORTED`. It answered `IO_ERROR`. The boards in the tree let a host read pb's boot
  partitions and its A/B state (the MEK and the DXL EVK only the A/B state); before, no
  partition at all (D-092).
- A command timeout of an MMC or SD card answers `TIMEOUT`.

On an i.MX8MQ B1 (rev 2.1) or later:

- **The device UUID is new, and each device's own.** Every release read the unique id directly
  from the fuse array, which the B1 refuses with a fixed value: every B1 had the same UUID, and
  the life cycle and the keys read wrong. pb reads the shadow registers there now. A token made
  for the old UUID doesn't authenticate a B1 any more. Older 8MQs, such as the pico8ml's, keep
  their UUID. It answered `IO_ERROR`.

Boot:

- A new result code, `PB_RESULT_BAD_PAYLOAD` (18): an image's payload doesn't have the header's
  hash. Devices answered `ERROR`. A released host reports its generic error for the new code,
  which is what it reported before; the 3.0 host raises `BadPayloadError`, a `GenericError`.
- **A boot is refused when the SoC's TrustZone DRAM filter is bypassed**, whoever starts it and
  in every life cycle state, and when the platform's crypto engine failed. The host gets an
  error, and the reason is in pb's log: "No boot: the TrustZone DRAM filter is bypassed".
- **A pb that the ROM's serial downloader loaded stops in command mode** and doesn't boot from
  storage by itself (`CONFIG_CM_ON_SERIAL_DOWNLOAD`, on by default). A boot the host starts
  (`punchboot boot ...`) isn't stopped. jiffy booted from storage after `uuu` before (D-031).
- A `boot bpak` that is refused before the image is read still reads the header's transfer,
  and the header's result has the refusal (D-058).
- When a board's hook stops a boot that the host started before the device has left the bus,
  the host gets an error and the device stays in command mode. It reset.

The life cycle on the i.MX8M:

- The i.MX8M boards (imx8mevk, imx8mpevk, pico8ml) have a life cycle: HABv4's SRK hash and its
  closed bit. `slc show` reads it, `slc configure` fuses the board's SRK hash, `slc lock` closes
  the device. A locked 8M device answers nothing but its identity before authentication, as
  every other port; command mode on the 8M was open before.
- The i.MX8MQ's keys are read and revoked through SRK_REVOKE; on 8MQ boards no key could be
  used before. The i.MX8M Plus's keys read as active and can't be revoked.
- `slc lock` and a revocation are refused on an 8M whose boot HAB reports a failure for: closed,
  it wouldn't boot.

The keys (D-085):

- `SLC_READ`'s result says which active keys the device can't revoke: those in a slot the
  SoC has no revoke bit for (HABv4's slots from 3, the i.MX8X's from 4). `punchboot slc show`
  marks them "(cannot be revoked)", and the Python API has `slc_get_unrevocable_keys()`. A
  device before 3.0.0 reports none, which says nothing about its keys.
- `SLC_REVOKE_KEY` on such a key answers `NOT_SUPPORTED` on every platform. The i.MX8X
  answered `INVALID_ARGUMENT`.
- The build warns for a board key in such a slot. On the i.MX8X such a key is refused: the
  build stops, and the device treats it as revoked (D-092).
- `SLC_SET_CONFIGURATION` reads every fuse word first and writes nothing when one has a bit
  its value lacks: it answers `ERROR`. A word of the SRK hash with another bit answered
  `MEM_ERROR`; a configuration word with another bit was completed (D-094).

Named commands (D-076):

- One new command, `COMMAND_LIST`, and `punchboot cmd list` and `punchboot cmd <name>` in
  the host tool: a device says which commands it has, with a help line each. A device older
  than the command answers `INVALID_COMMAND`, and a command is still run by its name.
- A board's command keeps its id, and `punchboot board command` works as released.
- `punchboot board` is deprecated: `board command` is `cmd <name>`, what `board status`
  printed is in `cmd info`. Both still work as released, but the help and tab completion no
  longer show them.
- An id the device doesn't have answers `NOT_SUPPORTED`. The RT1060 EVK, imx8mevk, the MEK and
  the DXL EVK answered OK to any id.
- jiffy's board command `dump-rom-log` is gone: `punchboot log rom` reads the ROM's log
  (`4bb4c2f6`).
- Every device has the named command `info`: the SoC, its revision, its clocks and its
  firmware, the lines pb logs at boot after the banner (D-077).
- The MEK's, the DXL EVK's and jiffy's `board status` answers "Board status OK!". What they
  printed is in `info` (D-077).

Logs: `punchboot log` reads pb's own log, `punchboot log <name>` another of the device's, and
`punchboot log --list` names them. A device without the command answers `INVALID_COMMAND`
(`doc/system/compatibility.md`, "Example: selecting a log").

The session:

- A transfer inside a command that the host doesn't complete within 10 s ends the session,
  and the device waits for a host again. The capabilities carry the time in
  `operation_timeout_ms`, which was zero in every release (D-058).
- A host that sets the device's address or its configuration again has started over: the
  session and its authentication end. `punchboot` doesn't do that between two
  invocations.

The host tools (`pip install punchboot`):

- Pure Python: one wheel for every platform, no C extension and no C host library. `Session`
  and the exceptions keep their API (D-021).
- `part write` fails when the device's last step of the write fails. `boot enable` needs its
  partition. The CLI prints what failed, with the error's message.
- For a program that uses the package: `part_verify` above 4 GiB raises `ArgumentError`; after
  a transfer error later calls raise `OSError`; `part_get_partitions` doesn't raise
  `ValueError` for flag bits it doesn't know; `Transport.set_timeout` is new.

`pbstate`, where it did the wrong thing without saying so:

- `-v X` for a slot that isn't the enabled one keeps the enabled slot's trial and its attempts.
- `-s` with a value that doesn't start with a, b or n exits 22. It disabled both slots and
  exited 0.
- `-s a -c N` gives N attempts from 256 on, and `-w I V` with a V that isn't a number exits 22.
- A read doesn't `sync()`, and `libpbstate.so.0` doesn't export `crc32`.

### What the next stage sees change

- Four new properties in `/chosen`: `pb,version`, `pb,boot-partition`, `pb,boot-mode` and
  `pb,status`. The released ones keep their bytes. A device tree without room for a new one
  boots, and the property is left out (the README has the sizes).
- With both A/B slots verified, an enabled slot that fails to load or to verify no longer ends
  in command mode: the other slot boots, for that boot, and nothing is written. The state
  still names the slot that failed as enabled, and `pb,boot-mode` is `fallback`.
- The watchdog is kicked after each chunk that is loaded, and before the jump.
- On the i.MX6UL, 8M and RT the watchdog's time-out is `CONFIG_WATCHDOG_TIMEOUT` exactly,
  half a second shorter than before: a board with 5 s gave the next stage 5.5 s to take over
  the watchdog, and gives it 5.0 s now.
- An image on ARMv7-M starts with the D-cache off and PRIMASK set (D-028), and gets Arm's
  Firmware Handoff registers and a transfer list where it got nothing (D-039).
- `linux,initrd-start` and `-end` are the bytes every release wrote. Only a ramdisk that
  reaches 4 GiB gets 64 bits for both, which never worked before.

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
| A device tree part in a partition    | pb would grow and patch the tree where it is, in  | D-092 |
| that is booted in place              | the flash. A kernel alone boots in place. In      |       |
|                                      | place is 3.0.0's form of the 2.x source that      |       |
|                                      | verified an image in memory (D-048): a board that |       |
|                                      | used that with a device tree has no equivalent    |       |
|                                      | yet                                               |       |
| On the CAAM (the 6UL, the 8M, the    | It booted there: the CAAM's parser read the two   | D-092 |
| 8X): an image or a token whose       | INTEGERs and nothing else, while Mbed TLS refused |       |
| signature has bytes after its DER,   | it. Security comes before compatibility. bpak and |       |
| or a SEQUENCE length that isn't what | every signer write exact DER                      |       |
| follows                              |                                                   |       |
| On the i.MX8X: an image signed by a  | It booted: no revoke bit covers that slot, so the | D-092 |
| key in a slot from 4 up              | key could never be revoked. Security comes before |       |
|                                      | compatibility. The boards in the tree use slots 0 |       |
|                                      | to 3                                              |       |
| On the DXL EVK: an image without the | It booted: 2.x started the part `atf` and named   | D-032 |
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
  DCD and refuses too (`doc/interfaces/platform.md` 2.3).
- **The images in the field against the table above**: every part inside the board's image
  data regions, the entry's part there and not empty, no `dont-hash` part. The boards in the
  tree start their regions where pb's link and its buffers end, so the DRAM 2.x loaded into is
  in them.
- **The revoke slot of each key.** The board's key map is gone, and key n of the board's
  `VERIFY_KEY<n>` is in slot n: a key must keep the fuse bit it had (D-027, D-083).
- **The SRK hash's words** in the board's `slc_fuses`, against the words 2.x fused. They are
  the board's data again, as in 2.x, and nothing checks them against the keys pb is signed
  with (D-094).
- **Whatever loads pb with the ROM's serial downloader** and expects it to boot from storage:
  the factory's flow, a recovery script. It sends a boot command now, or the build turns
  `CONFIG_CM_ON_SERIAL_DOWNLOAD` off (D-031).
- **Hosts and scripts** against "What a host sees change": one that probes a locked device and
  waits for `INVALID_COMMAND`, one that sends a prefix of the password, one that relies on OK
  for a board command the board doesn't have, one that writes the device a partition table is
  on.
- **The OS's updater.** After a fallback the running system is not the one the A/B state names
  as enabled. An updater that writes "the slot that isn't enabled" would write over the system
  it runs from: it reads `pb,active-system`.
- **What confirms an update.** `pbstate -v` for the booted slot no longer ends another slot's
  trial.
- **A device tree built without free space**, for a headroom of 0: it boots, without the new
  properties.
- **An image for ARMv7-M** that relied on the D-cache being on or on interrupts being unmasked
  at its entry (D-028).
- **The i.MX8M's image** (D-088): `pb_signed.imx` holds the SPL and pb proper, and the
  board's memory moved to DRAM (`PB_ENTRY`, `PB_SIZE`). pb 2.x writes it to a boot partition
  as any image. On the i.MX8MQ the new image boots from storage; over the ROM's serial
  downloader its SPL is a fastboot device that takes pb proper from the host (D-089): a
  recovery script with `SDP: boot` sends the image once more, `FB: download` and `FB: boot`
  (`doc/guides/build-and-test.md`).
- **A GPT that another tool changed.** A sane table gives the same partitions and nothing is
  written. An entry that covers the table's own blocks is no longer a partition, and a device
  whose backup table is stale logs one error line per boot.

### Known

- **Mbed TLS 3.4.0**, the software backend's, gets no fixes upstream any more: 3.4 isn't a
  long term support branch, and 3.5.0 followed it in 2023. 3.0.0 ships with it, the boards
  that use it are the RT1060 EVK's builds; it is to be replaced, with what replaces it
  bringing the hashes the RT1060 doesn't register (`doc/guides/vendored.md`).
- **The i.MX8M's security fuses** aren't checked against the SoC's own Security Reference
  Manual: the 8M Plus's wasn't available for 3.0.0. The SRK hash and the closed bit are
  where NXP's U-Boot has them for the whole family (`mx8m_secure_boot.txt`), the 8MQ's
  SRK_REVOKE where its reference manual's OCOTP chapter has it, and all three agree with a
  sibling's manual, the 8M Nano's. pb has written no fuse on an 8M board, and the in-tree 8M
  Plus board never does (`CONFIG_SLC_NO_FUSE_WRITES`).
- **The i.MX8M Plus's keys can't be revoked** in 3.0.0: where its SRK_REVOKE is isn't public,
  so `slc revoke-key` answers `NOT_SUPPORTED` for every key there and a key that leaks stays
  valid (`doc/guides/slc.md` 7).
