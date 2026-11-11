# The boot flow contract

**Draft for Jonas's review (2026-11-11).**

**Open for the review.** Decided with Jonas over four rounds (`log/2026-11-11.md`); these are my
choices in the draft that he hasn't confirmed:


Jonas: Note to self, read up on firmware handoff

1. The transfer list is always in a board region, also with a DT, so pb copies the DT into it
   (5.4; from "the list is dynamic information from pb, not the image's").
2. Firmware Handoff replaces `simple` instead of standing next to it: released pb
   (`armv7m_baremetal`) passed no arguments, r0/r1 exist only on the 3.0.0 branch (5.4).
3. Speculative mode needs an enabled slot: a device never activated stays in command mode (3.6).
4. Preparing the handover is part of a candidate's attempt (a bad DT tries the next candidate); a
   failure in `late_boot()` or `plat_lockdown()` tries none (2).
5. `boot part` is limited to the policy's candidates (6). For A/B this is what the code does today
   (`boot.c` refuses a partition without `BIO_FLAG_BOOTABLE`); new only for the other policies.
6. pb's structure's layout, up to 16 keys (5.4), and the Kconfig names of the region and the
   policies.

From the review pass (2026-11-11, a second model read the draft against the code and the
specifications; `log/2026-11-11.md`). Each is written into the text as a proposal and marked
**Open (n)** there:

7. Decided (Jonas, D-035, `security.md` 4.2): an image with a `dont-hash` part is refused
   (4.2, F-076).
8. `boot status` stays "the enabled slot" for A/B, as on the wire today (3.4); the draft's "what
   the next boot tries first" differs for a trial without attempts left.
9. Speculative mode also starts when this boot's failures leave no candidate (a verified slot that
   doesn't load, the other one not confirmed): for this boot only, nothing written (3.6).
10. The in place source (4.1, 4.3): a part is where the map puts it, its `pb-load-addr` must say
    the same (released pb hashed at `pb-load-addr`), the header is in the partition's last 4 KiB.
    Question: is that where the out-of-tree in place images have their header?
11. Decided (Jonas, D-035, `security.md` 4.1): the custom source is removed.
12. A state write that fails in `boot_policy_next()` (3.2): a trial whose attempt can't be written
    isn't handed out, a rollback or an alternation is.
13. The information of a boot command mode started (5.6, 6): `pb,active-system` stays "?" for a
    partition that isn't a slot (today's value), `pb,boot-partition` is absent when the host names
    none, the UUID `boot bpak` names is taken as given.
14. `rollback_mode` (3.6): Kconfig compiles speculative mode in, the board's field still selects
    it, so out-of-tree boards keep their data.
15. pb's tag (5.4): allocation is a pull request with the entry's layout, so 6 comes first. pb's
    structure isn't in a release before the tag is allocated; the DT variant doesn't wait.
16. `linux,initrd-*` (5.2, F-255): 32 bits as today when both addresses fit, 64 bits only above
    4 GiB, instead of following `#address-cells`, so that an image that boots today gets the same
    bytes.
17. The flow (2): the boot's preconditions are checked before `early_boot()`, also in a boot
    command mode started; pb's information is collected before the loop and its failure stops the
    boot.

What pb does from `boot()` to the jump: which partition it boots (the boot source policy), how it
loads and verifies the image, and what it hands the next stage (the handover). For each step:
what it is responsible for, what it may rely on, and what a failure means. Decided in D-032, in
the style of `platform.md` and `board.md`, which it builds on. Vendor neutral.

Changing the boot flow, a policy or a handover: read this and `principles.md`, then go through the
checklist (section 8). Section 7 is fixed like the wire (`compatibility.md`).

**Not yet in the code (F-nnn)** marks what the code doesn't do yet. The commit that fixes the
finding removes the marker.

## 1. Words and scope

| Word | Means |
|---|---|
| Boot flow | Everything from `boot()` to the jump, and command mode's boot commands |
| Boot source policy | Which partitions this boot tries, in which order, and the state that decides it. One per |
| | build, chosen in Kconfig: fixed, staggered chain or A/B |
| Candidate | A partition the policy hands out to try |
| Source | Where an image's bytes come from: a partition, the host's stream, memory mapped flash |
| Verification | The core's checks: the header's signature, where the parts land, the payload's hash |
| Handover | What the next stage gets: an entry, four arguments and what pb wrote in memory for it, |
| | including pb's information (5.6). Two: image (Linux, Xen) and Firmware Handoff |
| Delivery | How the handover reaches the next stage: directly (`arch_jump()`) or through TF-A, |
| | whose BL31 pb starts and which passes the handover on to BL33 |

Not here: the platform's and the board's parts (`platform.md`, `board.md`), the bpak format
(`bpak.md`), the wire (`compatibility.md`), the security life cycle and the RoT (D-027).

Standards it leans on (`principles.md` 5): the Linux boot protocol
(`Documentation/arch/arm/booting.rst`, `arm64/booting.rst`), which Xen follows too (xenbits,
`misc/arm/booting.txt`), TF-A's `bl_params` (v2), OP-TEE's entry convention (TF-A's opteed), Arm's
Firmware Handoff (transfer list version 2, register convention 1), the devicetree specification's
`/chosen`.

## 2. The boot flow

```
boot()
  the serial download stop                       board.md 5.1, not in a boot command mode started
  the boot's preconditions                       platform.md 2.3
  board.early_boot()                             board.md 5.2
  collect pb's information                       5.6: what doesn't depend on the candidate
  candidate = boot_policy_next()                 3, persists its state before the load
  while there is a candidate:
      load, verify, prepare the handover         4, 5 (the board's DT hook runs in prepare, 5.5)
      boot_policy_report(candidate, result)
      on success: leave the loop
      candidate = boot_policy_next()
  no candidate left: command mode                (A/B's speculative mode is a candidate, 3.6)
  board.late_boot()                              board.md 5.3
  plat_lockdown()                                platform.md 4.4
  the jump                                       5.1
```

A boot command mode started (section 6) is the same flow with one candidate that doesn't come from
the policy: the partition the host names, or the host's stream.

**Open (17)**: where the two new steps sit. The preconditions are checked before `early_boot()`, so
that a boot that will be refused starts no hardware, and in every boot, also one command mode
started (**not yet in the code, F-248**). pb's information is collected before the loop: the
device's identity, the life cycle and the keys don't depend on the candidate, so a failure to read
them is no candidate's fault and stops the boot (**not yet in the code, F-260**: today it fails in
prepare).

Each step's responsibilities:

- **Boot source policy**: the candidates and their order, its state, learning each result,
  `boot activate` and `boot status`, the mode it boots in (5.6). Not: reading or verifying images,
  whether an image is allowed, the handover.
- **Source**: reading the bytes, or mapping them. Not: their meaning.
- **Verification**: the image is signed by a key that isn't revoked, its parts land in allowed
  memory, the loaded bytes are the signed ones; later, a version floor. Not: which partition, the
  next stage's format.
- **Handover**: the next stage's entry and arguments from the verified header, pb's information in
  the next stage's format, the board's DT hook. Not: choosing or verifying the image.
- **Board hooks**: the board's own stop conditions and setup (`board.md` 5), and its changes to
  the DT (5.5). Not: anything between the header's check and the payload's hash.

- **Load, verify and prepare are one attempt.** A failure in any of them is the candidate's: the
  policy hears it, and the next candidate is tried. What a failed attempt left in memory is
  overwritten or ignored; nothing of it runs.
- **Before and after the loop, a failure stops the boot.** The preconditions, `early_boot()`,
  pb's information, `late_boot()` and `plat_lockdown()` decide about this boot, not about the
  image: no other candidate is tried, and pb enters command mode (in a boot command mode started:
  `board.md` 5.4). `boot()` returns the first candidate's error when every candidate failed.
- **No hook runs between the header's check and the payload's hash.** `early_boot()` is before the
  loop and `late_boot()` after it. Inside the loop the board runs only in prepare, on a verified
  image: its DT hook (5.5).
- **Boot time** (`principles.md` 4): the failure path may cost a candidate's load; the common path
  is one candidate.

## 3. Boot source policies

One policy per build, chosen in Kconfig (`choice BOOT_POLICY`). The board gives its data
(`board.md` 2.5). **Not yet in the code (F-256)**: A/B is the only policy, wired into the core
through three callbacks; without it no partition is bootable (F-215).

### 3.1 Function : boot_policy_init() [mandatory]

```
Argument : the board's data for the policy
Return   : int, PB_OK or -PB_ERR_*
```

Called once by `board_setup()`, after the partition table. Reads and repairs the policy's state,
and marks its candidates bootable (`BIO_FLAG_BOOTABLE`): only they can be booted, also by
`boot part`. Failure: `board.md` 2.1.

### 3.2 Function : boot_policy_next() [mandatory]

```
Argument : uuid_t part_uu (out)
Return   : int, PB_OK with a candidate, -PB_ERR_NOT_FOUND when none is left
```

Hands out the next candidate of this boot. A policy with state writes what the decision changes
before it returns (a trial's attempt, a rollback, speculative mode's slot): a hang during the load
then still counts.

**Open (12)**: when that write fails (**not yet in the code, F-179**: today any failed write ends
the boot). Proposed: the write decides only where skipping it would be unsafe.

- A trial's attempt that can't be written: the trial isn't handed out, it would get attempts
  without end. The next candidate follows (3.5, rule 3).
- A rollback that can't be written: the verified slot is handed out. The next boot decides the
  same again.
- Speculative mode's alternation that can't be written: the slot is handed out. A hang then comes
  back to the same slot, which is still better than no system.

### 3.3 Function : boot_policy_report() [mandatory]

```
Argument : const uuid_t part_uu, int result
Return   : void
```

The attempt's result (2). The policies of this document persist nothing on a failure (3.5);
`report` is where a policy with per-candidate state would.

### 3.4 Functions : boot_policy_mode(), boot_policy_activate(), boot_policy_status() [mandatory]

- `boot_policy_mode()`: how this boot was chosen, for the handover (5.6): normal, fallback (not
  the first candidate the policy handed out in this boot), speculative.
- `boot_policy_activate(uuid)`: command mode's `boot activate`. `-PB_ERR_NOT_SUPPORTED` where the
  policy has nothing to activate. An unknown UUID changes nothing (F-175).
- `boot_policy_status(uuid)`: command mode's `boot status`. **Open (8)**: for A/B the enabled
  slot, or the null UUID when none is, as the wire has it today (`PB_CMD_BOOT_STATUS`, D-008): a
  trial without attempts left is still reported, though the next boot rolls it back. Fixed: its
  partition. Staggered chain: the first of the list.

### 3.5 The policies

**Fixed** (`BOOT_POLICY_FIXED`). One partition, from the board. Candidates: it. No state. `boot
activate`: not supported.

**Staggered chain** (`BOOT_POLICY_CHAIN`). The board's list, in the board's order. Candidates: the
list. No state. It protects against an image that doesn't load or verify (corrupt storage, a torn
write, a revoked key), not against one that verifies and then fails: that one boots again on every
reset. `boot activate`: not supported.

**A/B** (`BOOT_POLICY_AB`). Two slots and the state of 3.7. Its candidates, in order:

1. The enabled slot, when it is verified (confirmed by the OS), or when it is on trial with
   attempts left: one attempt is spent and written first.
2. A trial without attempts left rolls back: the other slot, when it is verified, is enabled, and
   the rolled back slot's error bit is set (written). Not verified: no viable candidate.
3. When the first candidate failed in this boot: the other slot, when it is verified. Nothing is
   written, mode fallback (F-174, decided by Jonas for this boot only). A trial that failed spent its
   attempt; it is tried again on the next boots until its attempts run out.
4. No viable candidate: none, and pb enters command mode; with speculative mode, 3.6.

Nothing enabled (a device never activated) has no candidate. `boot activate X` enables X and marks
it verified, as today; the null UUID disables both. The OS starts a trial and confirms it through
`pbstate`.

As today, `boot activate X` also clears the other slot's verified bit and the error bits
(`boot_ab_state_set_boot_partition()`; wire semantics, D-008). A device activated in the factory
therefore has no fallback candidate (rule 3) until the OS has confirmed the other slot once.

### 3.6 A/B's speculative mode [optional, BOOT_AB_SPECULATIVE]

**Not yet in the code (F-257)**: today's speculative mode gives the fallback one trial attempt
(two boots, F-177) and drops a rollback bit.

For a system that must stay reachable for an update when no slot is viable. With it:

1. It starts only when a slot is enabled and A/B has no viable candidate (3.5, 4). That is one of
   two cases:
   - **From the state**: the enabled slot is a trial without attempts left and the other slot
     isn't verified. Items 3 and 4 apply.
   - **From this boot** (**Open (9)**): the candidates of 3.5 failed to load or verify, and a slot
     that isn't confirmed is left to try. It is tried for this boot only and nothing is written,
     like the fallback of 3.5, rule 3: the enabled slot keeps its state and is tried first again
     on the next boot.
2. It never boots an image that fails verification: it relaxes "confirmed by the OS", never the
   signature, the hash or the placement (`principles.md` 1).
3. It alternates per boot. Before the load it enables the other slot and writes it, so a hang and
   the watchdog lead to the other one. In the same boot, when its slot fails, the other is tried,
   without another write.
4. It counts no attempts. It ends when the OS confirms a slot through `pbstate`.
5. The handover's mode is speculative (5.6).
6. It depends on `CONFIG_ENABLE_WATCHDOG`: without one, a hang stays hung.
7. No format change: the first case follows from the state and its alternation is `enable`; the
   second writes nothing.

**Open (14)**: who selects it. Today it is the board's data (`rollback_mode` in
`struct boot_ab_state_config`), and Jonas decided on Kconfig (round 3). Proposed: both.
`BOOT_AB_SPECULATIVE` compiles the mode in and carries the watchdog dependency, the board's field
still selects it, and the data check (`board.md` 3) refuses `AB_ROLLBACK_MODE_SPECULATIVE` without
the option. Out-of-tree boards keep their data and add one line to their defconfig.

### 3.7 The state

The A/B state is `struct pb_boot_state` (`include/boot/pb_state_blob.h`), 512 bytes, CRC32, in two
partitions: the primary is written first, the backup second. Its format is fixed (section 7): an
extension takes a field from `rz[]`, where zero means today's behaviour. The four board registers
(`board_regs[]`) are the board's, and part of the format. `pbstate` is the OS's side
and isn't Linux specific. **Not yet in the code (roadmap, F-173, F-176, F-179, F-180)**: one
freestanding core shared by pb and `pbstate` (Linux, Zephyr, bare metal; the simple target's copy
goes), the other policies in `pbstate`, the fixes of the A/B review.

## 4. Load and verify

### 4.1 Sources

| Source | Used by | What |
|---|---|---|
| Partition | The policies, `boot part` | A bio partition, the bpak header in its last 4 KiB |
| Host stream | `boot bpak` | Command mode reads the header, then the parts, from the host |
| In place | Boards that run from memory mapped flash | A partition mapped at an address: the image is |
| | | verified where it is and runs there |

**Not yet in the code (F-253, F-219)**: the board can't select the in place source since D-026,
and it hashes the parts at their load addresses while the entry is in the mapped flash. The in
place source becomes a property of a partition (mapped at an address), so that any policy can hand
one out.

There is no custom source, where the board loads and authenticates the image itself: released pb
had one, nobody uses it, and it was the one way around 4.2 (Jonas, D-035, `security.md` 4.1).

**Open (10)**: the in place source's rules, which the draft left out.

- The header is in the partition's last 4 KiB, read through the map, as for a partition that is
  loaded. Released pb got the header's address from the board (`get_in_mem_image()`), so an
  out-of-tree board may keep it elsewhere: Jonas?
- A part is at the partition's mapped address plus its offset in the payload. Nothing is copied:
  step 5 of 4.2 hashes it there, and the entry is there.
- `pb-load-addr` stays mandatory and must be that address. Released pb hashed at `pb-load-addr`
  and jumped to the mapped address, so an image where the two differ ran bytes that weren't the
  hashed ones: refusing it is the security exception of `compatibility.md` and needs its decision
  entry.
- Placement (4.3): the partition's mapped range, without the header, is the allowed memory for
  its parts. The handover's own memory (the Firmware Handoff region) is in RAM as always.

### 4.2 The sequence

1. Read the header; it is valid bpak (`bpak_valid_header()`).
2. The key it names (`key_id`) is in the keystore and not revoked (`rot_read_key_status()`). The
   header's `keystore_id` is logged and not compared: pb has one keystore, and a check added now
   could refuse images that boot today (D-008).
3. The header's signature verifies with that key (`boot_image_auth_header()`).
4. Every part has a `pb-load-addr`, lies inside its source (the partition without the header) and
   lands in allowed memory (4.3).
5. Load and hash each part, chunk by chunk, kicking the watchdog per chunk. **Not yet in the code
   (F-243)**: no kick on the boot path.
6. The payload's hash is the header's (`boot_image_verify_payload()`).
7. Later: the image's version isn't below the floor (anti-rollback, roadmap). A refused image is a
   failed candidate; the policies don't change.

Unverified bytes reach memory in 5, before 6: that is why 4 comes first. Nothing runs, and nothing
is patched, before 6.

An image with a part outside the hash (bpak's `dont-hash` flag, `BPAK_FLAG_EXCLUDE_FROM_HASH`)
is refused in step 4, before anything is loaded (D-035, `security.md` 4.2). The flag is for content
verified at run time, such as a file system under a hash tree whose root hash is signed; pb doesn't
boot such content, and loading it would leave bytes nothing authenticated in the next stage's
memory. **Not yet in the code (F-076)**: today such an image fails on its payload hash, so none
that booted is refused.

### 4.3 Placement

**Not yet in the code (F-254)**: the parts are only kept off some of pb's sections.

An allow list. Every part, and the room a handover grows it by (the DT's headroom), lies inside the
board's DRAM (`BOARD_DRAM_*`, `platform.md` 5.1) or a region the platform declares loadable (on-chip
RAM, TCM), and overlaps none of: pb's sections (all of them, the keystore and the RAM log too),
another part, and what a handover writes outside the parts: the Firmware Handoff region (5.4) and
TF-A's `bl_params` where the platform puts them (5.3). Address ranges that wrap are refused. A load
address the arch can't hold (above 4 GiB on a 32-bit arch) is refused. The in place source: 4.1.

## 5. Handovers

### 5.1 The model

**Not yet in the code (F-258)**: each boot driver computes and jumps by itself; TF-A's BL33
arguments are Linux's, hard coded.

A handover turns the verified header and pb's information (5.6) into an entry, four arguments and
what it wrote in memory (a DT's `/chosen`, a transfer list). There are two, image (5.2) and
Firmware Handoff (5.4), and two deliveries:

- **Directly**: `arch_jump(entry, a0..a3)`, at the exception level pb runs at, in the arch's
  handover state (D-028).
- **Through TF-A** (5.3): the same entry and arguments become BL33's entry point in `bl_params`,
  and BL31 passes them on.

One handover and one delivery per board, chosen by its configuration (`board.md` 2.7), not by the
image: the same parts mean different things on different boards today (the DXL's `atf` is started
directly, as an image, the MEK's through `bl_params`), so letting the image choose would change
what old images do (D-008). A signed meta that chooses can come later, opt in.

### 5.2 Handover : image (Linux, Xen) [optional, CONFIG_BOOT_LINUX]

The Linux boot protocol, which Xen follows too. **Not yet in the code (F-258)**: named `linux`.

| Arch | Registers |
|---|---|
| ARMv7-A | r0 = 0, r1 = 0xffffffff (no machine type, the DT describes it), r2 = the DT |
| ARMv8-A | x0 = the DT, x1 to x3 = 0 |

Parts: the kernel (its load address is the entry), the DT, the ramdisk (both optional, named by the
board). Without `set_dtb_boot_arg` all four registers are 0. The DT gets pb's `/chosen` (5.5) and,
with a ramdisk, `linux,initrd-start` and `-end`.

**Not yet in the code (F-255)**: a ramdisk above 4 GiB gets a truncated address. **Open (16)**: the
encoding. F-255 proposes the root's `#address-cells`, as U-Boot does; that turns today's 4 bytes
into 8 on every 64-bit board. Linux reads both, another next stage may not. Proposed: 32 bits when
both addresses fit, as today, and 64 bits only when one doesn't, which never worked.

At pb's exception level: on a platform where pb runs at EL3, Linux and Xen go through TF-A (Xen
needs Non-Secure EL2, its `booting.txt`). Xen's dom0 kernel and ramdisk are `/chosen` module nodes
(`multiboot,module`): they are in the image's DT, written when the image is built from the same
addresses as the parts. pb writes nothing for Xen.

### 5.3 Delivery : TF-A [optional, CONFIG_BOOT_ATF]

ARMv8-A only. pb jumps to BL31 (the part the board names) with x0 = `bl_params` (v2) and x1 to
x3 = 0. `bl_params` hold:

- **BL33**: its entry and SPSR (AArch64; EL2 when the CPU has it, else EL1), and as its arguments
  the board's handover: image (Linux, Xen) or Firmware Handoff. **Not yet in the code (F-258)**:
  Linux's only.
- **BL32** (optional): OP-TEE's convention, as TF-A's opteed reads it. arg0 = OP-TEE's execution
  state as TF-A encodes it (`MODE_RW_64` or `MODE_RW_32`), from the `optee-arch` meta (64 or 32;
  64 without the meta). arg1 = 0 (no pageable part), arg2 = 0 (no memory limit), arg3 = the DT
  BL33 gets, or 0 without one. Others can be added.

Where `bl_params` may live is the platform's to say (NXP's i.MX8M BL31 takes them only from low
OCRAM). **Not yet in the code (F-258)**: `atf.c` puts them in `.data` for it. The DXL moves to this
delivery (it starts BL31 directly today, as an image). TF-A's own transfer list (tag 0x102, the
AArch64 entry point info) is a later path, for a BL31 that takes it.

### 5.4 Handover : Firmware Handoff, generic [optional]

**Not yet in the code (F-259)**: the generic handover is `simple` (r0 = the system's name, r1 = the
boot partition's UUID, only on the 3.0.0 branch). Released pb (`armv7m_baremetal`) passed nothing,
so images that ignore their registers keep working.

For any next stage: an RTOS, an application, a hypervisor, a stage that parses a DT. The entry is
the part the board names (on ARMv7-M its vector table, D-028; in place, its mapped address).

| Arch | Registers |
|---|---|
| AArch64 | x0 = the DT in the list or 0, x1 = 0x4a0fb10b with bits 39:32 = 1, x2 = 0, x3 = the list |
| AArch32 | r0 = 0, r1 = 0x0fb10b with bits 31:24 = 1, r2 = the DT in the list or 0, r3 = the list |
| ARMv7-M | AArch32's. The specification covers AArch64 and AArch32 only: pb's extension |

The transfer list holds one of two, never both:

- **The image's DT**, when the board names a DT part: pb opens the verified DT into the list's
  FDT entry (tag 1) with the headroom (`fdt_open_into()` copies and grows in one step), then the
  board's hook and pb's `/chosen` work on that copy (5.5). The register points to it. The headroom
  is the region's, so this handover needs none at the part's load address (4.3). An image without
  the part fails to boot, and so does a DT that doesn't fit the region with its headroom.
- **pb's structure**, when the board names none: one entry with pb's tag.

The list is in a region of the board's (Kconfig base and size): pb's information changes on every
boot, it isn't part of the image. The region is checked like a part (4.3) and is 8-byte aligned.
The image's memory map keeps it free until the next stage has read or moved the list: it is part
of the board's interface to image authors (`board.md` 8). The list is version 2 with checksums on,
and has no pointers into itself (the next stage may move it).

pb's structure (proposal, open): little-endian, no pointers, fields only appended (D-008). The
entry's `data_size` is its size, so a receiver reads the fields `data_size` covers and it carries
no size or version of its own (changed in the review pass: the draft had a `size` field first).

```c
struct pb_fh_handover {
    uint32_t boot_mode;            /* 5.6 */
    uint8_t device_uuid[16];       /* the bytes in the order of the UUID's string */
    uint8_t boot_partition_uuid[16]; /* all zero: none (6) */
    char system_name[8];           /* "A", "B", "?" or empty; NUL terminated */
    int32_t slc;                   /* the wire's pb_slc values; -1 without an SLC */
    uint32_t active_key;
    uint32_t no_of_available_keys;
    uint32_t available_keys[16];
};
```

A build whose keystore has more than 16 keys doesn't compile with this handover (a static assert).

Its tag is allocated in the specification (requested upstream by Jonas, before 3.0.0); the
non-standard range is discouraged there. **Open (15)**: the order and the timing. A tag is
allocated by a pull request that adds the entry's layout to the specification, so the layout above
(open 6) is settled first. Proposed: pb's structure isn't in a release before its tag is
allocated, since the tag can't change afterwards (section 7); the DT variant doesn't wait for it.

### 5.5 What pb writes into a DT

**Not yet in the code (F-260)**: the board's hook runs in the middle of pb's properties, and
`dtb.c` reads the RoT, the SLC and the device id itself.

- `dtb.c` stays: it finds and grows the DT (`boot_dtb_grow()`, the headroom checked as in 4.3; for
  Firmware Handoff inside the list, 5.4) and writes `/chosen`, from the information the core
  collected (5.6).
- Order: the board's hook first (`board.md` 2.7), with the whole tree; then pb's properties, so
  pb's values always win.
- pb writes only `/chosen`: its own properties, and for the image handover `linux,initrd-*`
  (`bootargs` is the board's, through its hook). Nothing else, nothing for Xen or OP-TEE.
- `CONFIG_BOOT_DTB_HEADROOM` (4 KiB) is the room for both.

### 5.6 pb's information

Collected once by the core, written by each handover in its format. **Not yet in the code
(F-260)**: the boot partition and the mode. The identity, the life cycle and the keys not revoked
are collected before the loop (2); the system, the partition, the mode and the image's key belong
to the candidate.

| Information | `/chosen`, and its encoding | pb's structure | From |
|---|---|---|---|
| The device's identity | `pb,device-uuid`, string | `device_uuid` | `device_uuid()` |
| The booted system | `pb,active-system`, string: "A", "B" or | `system_name` | The policy |
| | "?"; A/B only | | |
| The boot partition | `pb,boot-partition`, string (a UUID), new | `boot_partition_uuid` | The policy, or |
| | | | command mode |
| How it was chosen | `pb,boot-mode`, string: "normal", | `boot_mode`: 0, 1, 2 | `boot_policy_mode()` |
| | "fallback", "speculative", new | | |
| The life cycle | `pb,slc`, u32, the wire's `pb_slc` values; | `slc` | `slc_read_status()` |
| | absent without an SLC | | |
| The image's key | `pb,slc-active-key`, u32 (a key id) | `active_key` | The header |
| The keys not revoked | `pb,slc-available-keys`, u32 list (key | `available_keys` | The RoT |
| | ids) | | |

New entries are appended (section 7). The encodings are part of what section 7 fixes.

**Open (13)**: a boot command mode started.

- `pb,active-system` is "?" when the partition isn't a slot: `boot bpak` without a UUID, or with
  another one. That is today's value (`boot_ab_part_uu_to_name()`), so it stays (D-008).
- `pb,boot-partition` is absent, and `boot_partition_uuid` all zero, when `boot bpak` names no
  partition.
- The UUID `boot bpak` names is taken as given, as today: the host is authenticated on a locked
  device, and the image is verified like any other.
- The mode is normal.

## 6. Command mode's boot commands

- `boot part <uuid>`: one candidate, the partition the host names, which must be one of the
  policy's (bootable). No policy, nothing counted, mode normal.
- `boot bpak`: the host stream (4.1), the partition the host names in the handover (5.6).
- Both go through the preconditions, `early_boot()`, 4.2 and 4.3 like any boot (2); only the
  serial download stop is skipped.
- The host's last OK means "loaded and verified", not "booted" (`board.md` 5.4, F-252). `boot part`
  answers after prepare. `boot bpak` answers after the payload's hash, before prepare (the wire's
  sequence, D-008): a failure in prepare reaches the log, not the host.
- `boot activate`, `boot status`: the policy's (3.4).

## 7. Fixed (compatibility)

Like the wire (D-008, `compatibility.md`):

- The A/B state's format and meaning (3.7), which `pbstate` and the OS write.
- The `/chosen` properties, their encoding and their meaning (5.6, the README's table): only added
  to.
- `boot activate` and `boot status` as the wire has them (3.4, 3.5).
- Each handover's register convention and entry (5.2 to 5.4), and pb's structure (append only) and
  its tag once allocated.
- What an image that a released pb booted gets: its parts, entry and arguments under the same board
  configuration.

## 8. Review checklist

- [ ] The step's responsibilities stay its own (section 2's list); no hook between the header and
      the payload's hash.
- [ ] A policy writes its state before the load, and only what its decision changed; a failed
      write never gives a trial more attempts.
- [ ] Every candidate goes through 4.2 and 4.3, and every byte pb loads is hashed.
- [ ] Placement is the allow list of 4.3, also for what a handover grows or writes.
- [ ] A handover is an entry, four arguments and what it wrote; it reads pb's information, not the
      RoT, SLC or policy directly. The delivery changes none of them.
- [ ] A DT gets the board's hook first, then pb's properties, only in `/chosen`.
- [ ] Section 7 unchanged; new information appended.
- [ ] The markers of what was implemented are removed.

## 9. Not yet in the code

| Finding | What |
|---|---|
| F-256 | The policy interface (3.1 to 3.4), fixed and staggered, the loop with the fallback (F-174), each |
| | policy marking its candidates (F-215) |
| F-257 | Speculative mode (3.6, F-177) |
| F-258 | The handover and delivery model, `image`, TF-A's BL33 and BL32, `bl_params` from the |
| | platform, the DXL |
| F-259 | Firmware Handoff (5.4): the list, the region, pb's structure and its tag |
| F-260 | pb's information collected once, before the loop; `pb,boot-partition`, `pb,boot-mode`, the |
| | hook's order |
| F-253, F-219 | The in place source (4.1), its rules (open 10); the custom source's removal |
| F-254, F-255 | Placement (4.3), the initrd above 4 GiB (open 16) |
| F-243, F-248 | The watchdog per chunk, the boot's preconditions (`platform.md` 2.3) in the flow (2) |
| F-173, F-175, F-176, F-179, F-180 | The A/B review's fixes, a failed state write (3.2), `pbstate` |
| F-252, F-076 | A stop in a boot command mode started; `dont-hash` refused (4.2) |
