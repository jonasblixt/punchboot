# The boot flow contract

**Draft for Jonas's review (2026-11-11).**

**Open for the review.** Decided with Jonas over four rounds (`log/2026-11-11.md`); these are my
choices in the draft that he hasn't confirmed:

1. The transfer list is always in a board region, also with a DT, so pb copies the DT into it
   (5.4; from "the list is dynamic information from pb, not the image's").
2. Firmware Handoff replaces `simple` instead of standing next to it: released pb
   (`armv7m_baremetal`) passed no arguments, r0/r1 exist only on the 3.0.0 branch (5.4).
3. Speculative mode needs an enabled slot: a device never activated stays in command mode (3.6).
4. Preparing the handover is part of a candidate's attempt (a bad DT tries the next candidate); a
   failure in `late_boot()` or `plat_lockdown()` tries none (2).
5. `boot part` is limited to the policy's candidates (6).
6. pb's structure's layout, up to 16 keys (5.4), and the Kconfig names of the region and the
   policies.

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
| | including pb's information (5.6). Image (Linux, Xen), TF-A, Firmware Handoff |

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
  the serial download stop                       board.md 5.1
  board.early_boot()                             board.md 5.2
  candidate = boot_policy_next()                 3, persists its state before the load
  while there is a candidate:
      load, verify, prepare the handover         4, 5
      boot_policy_report(candidate, result)
      on success: leave the loop
      candidate = boot_policy_next()
  no candidate left: command mode                (A/B's speculative mode is a candidate, 3.6)
  board.late_boot()                              board.md 5.3
  plat_lockdown()                                platform.md 4.4
  the jump                                       5.1
```

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
- **Board hooks**: the board's own stop conditions and setup (`board.md` 5). Not: anything between
  the header's check and the payload's hash.

- **Load, verify and prepare are one attempt.** A failure in any of them is the candidate's: the
  policy hears it, and the next candidate is tried. What a failed attempt left in memory is
  overwritten or ignored; nothing of it runs.
- **After the loop, a failure stops the boot.** `late_boot()` and `plat_lockdown()` decide about
  this boot, not about the image: no other candidate is tried.
- **No hook runs between the header's check and the payload's hash.** The board's hooks are before
  the loop and after it.
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

### 3.3 Function : boot_policy_report() [mandatory]

```
Argument : const uuid_t part_uu, int result
Return   : void
```

The attempt's result (2). The policies of this document persist nothing on a failure (3.5);
`report` is where a policy with per-candidate state would.

### 3.4 Functions : boot_policy_mode(), boot_policy_activate(), boot_policy_status() [mandatory]

- `boot_policy_mode()`: how this boot was chosen, for the handover (5.6): normal, fallback (not
  the policy's first candidate), speculative.
- `boot_policy_activate(uuid)`: command mode's `boot activate`. `-PB_ERR_NOT_SUPPORTED` where the
  policy has nothing to activate. An unknown UUID changes nothing (F-175).
- `boot_policy_status(uuid)`: command mode's `boot status`: what the next boot tries first.

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

### 3.6 A/B's speculative mode [optional, BOOT_AB_SPECULATIVE]

**Not yet in the code (F-257)**: today's speculative mode gives the fallback one trial attempt
(two boots, F-177) and drops a rollback bit.

For a system that must stay reachable for an update when no slot is viable. With it:

1. It starts only when a slot is enabled and A/B has no viable candidate (3.5, 4).
2. It never boots an image that fails verification: it relaxes "confirmed by the OS", never the
   signature, the hash or the placement (`principles.md` 2).
3. It alternates per boot. Before the load it enables the other slot and writes it, so a hang and
   the watchdog lead to the other one. In the same boot, when its slot fails, the other is tried.
4. It counts no attempts. It ends when the OS confirms a slot through `pbstate`.
5. The handover's mode is speculative (5.6).
6. It depends on `CONFIG_ENABLE_WATCHDOG`: without one, a hang stays hung.
7. No format change: "no viable candidate" follows from the state, the alternation is `enable`.

### 3.7 The state

The A/B state is `struct pb_boot_state` (`include/boot/pb_state_blob.h`), 512 bytes, CRC32, in two
partitions: the primary is written first, the backup second. Its format is fixed (section 7): an
extension takes a field from `rz[]`, where zero means today's behaviour. `pbstate` is the OS's side
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
| Custom | Boards with their own authentication | The board loads and authenticates; pb trusts it |

**Not yet in the code (F-253, F-219)**: the board can't select the in place or the custom source
since D-026, and the in place source hashes the parts at their load addresses while the entry is in
the mapped flash. The in place source becomes a property of a partition (mapped at an address), so
that any policy can hand one out. The custom source is the escape hatch: it skips 4.2 and 4.3, the
board owns them, and the board's documentation says how.

### 4.2 The sequence

1. Read the header; it is valid bpak (`bpak_valid_header()`).
2. The key it names is in the keystore and not revoked (`rot_read_key_status()`).
3. The header's signature verifies with that key (`boot_image_auth_header()`).
4. Every part lands in allowed memory (4.3).
5. Load and hash each part, chunk by chunk, kicking the watchdog per chunk. **Not yet in the code
   (F-243)**: no kick on the boot path.
6. The payload's hash is the header's (`boot_image_verify_payload()`).
7. Later: the image's version isn't below the floor (anti-rollback, roadmap). A refused image is a
   failed candidate; the policies don't change.

Unverified bytes reach memory in 5, before 6: that is why 4 comes first. Nothing runs, and nothing
is patched, before 6. Every part is hashed. **Not yet in the code (F-076)**: bpak's `dont-hash`
flag is ignored, unlike upstream.

### 4.3 Placement

**Not yet in the code (F-254)**: the parts are only kept off some of pb's sections.

An allow list. Every part, and the room a handover grows it by (the DT's headroom), lies inside the
board's DRAM (`BOARD_DRAM_*`, `platform.md` 5.1) or a region the platform declares loadable (on-chip
RAM, TCM), and overlaps none of: pb's sections (all of them, the keystore and the RAM log too),
another part, the Firmware Handoff region (5.4). Address ranges that wrap are refused. A load
address the arch can't hold (above 4 GiB on a 32-bit arch) is refused.

## 5. Handovers

### 5.1 The model

**Not yet in the code (F-258)**: each boot driver computes and jumps by itself; TF-A's BL33
arguments are Linux's, hard coded.

A handover turns the verified header and pb's information (5.6) into an entry, four arguments and
what it wrote in memory (a DT's `/chosen`, a transfer list). It is delivered:

- **Directly**: `arch_jump(entry, a0..a3)`, at the exception level pb runs at, in the arch's
  handover state (D-028).
- **Through TF-A** (5.3): the same entry and arguments become BL33's (or BL32's) entry point in
  `bl_params`, and BL31 passes them on.

One handover per board, chosen by its configuration (`board.md` 2.7), not by the image: the same
parts mean different handovers on different boards today (the DXL's `atf` goes through the image
handover, the MEK's through TF-A), so letting the image choose would change what old images do
(D-008). A signed meta that chooses can come later, opt in.

### 5.2 Handover : image (Linux, Xen) [optional, CONFIG_BOOT_LINUX]

The Linux boot protocol, which Xen follows too. **Not yet in the code (F-258)**: named `linux`.

| Arch | Registers |
|---|---|
| ARMv7-A | r0 = 0, r1 = 0xffffffff (no machine type, the DT describes it), r2 = the DT |
| ARMv8-A | x0 = the DT, x1 to x3 = 0 |

Parts: the kernel (its load address is the entry), the DT, the ramdisk (both optional, named by the
board). Without `set_dtb_boot_arg` no DT goes in the registers. The DT gets pb's `/chosen` (5.5),
the ramdisk `linux,initrd-start` and `-end`. **Not yet in the code (F-255)**: written in 32 bits.

At pb's exception level: on a platform where pb runs at EL3, Linux and Xen go through TF-A (Xen
needs Non-Secure EL2, its `booting.txt`). Xen's dom0 kernel and ramdisk are `/chosen` module nodes
(`multiboot,module`): they are in the image's DT, written when the image is built from the same
addresses as the parts. pb writes nothing for Xen.

### 5.3 Handover : TF-A [optional, CONFIG_BOOT_ATF]

The entry is BL31 (the part the board names), x0 = `bl_params` (v2) with:

- **BL33**: its entry and SPSR (EL2 when the CPU has it, else EL1), its arguments from a nested
  handover: image (Linux, Xen) or Firmware Handoff. **Not yet in the code (F-258)**: Linux's only.
- **BL32** (optional): OP-TEE's convention, as TF-A's opteed reads it: arg0 = 32 or 64 (the
  `optee-arch` meta), arg1 = 0, arg2 = 0, arg3 = the DT. Others can be added.

Where `bl_params` may live is the platform's to say (NXP's i.MX8M BL31 takes them only from low
OCRAM). **Not yet in the code (F-258)**: `atf.c` puts them in `.data` for it. The DXL moves to this
handover (it uses the image handover to start BL31). TF-A's own transfer list (tag 0x102, entry
point info) is a later path, for a BL31 that takes it.

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

- **The image's DT**, when the board names a DT part: pb fills `/chosen` (5.5), then copies the DT
  into the list's FDT entry (tag 1). The register points to the copy. An image without the part
  fails to boot.
- **pb's structure**, when the board names none: one entry with pb's tag.

The list is in a region of the board's (Kconfig base and size): pb's information changes on every
boot, it isn't part of the image. The region is checked like a part (4.3), and the image's memory
map keeps it free: it is part of the board's interface to image authors (`board.md` 8). The list
has no pointers into itself (the next stage may move it); checksums are on.

pb's structure (proposal, open): little-endian, no pointers, a size field first, fields only
appended (D-008):

```c
struct pb_fh_handover {
    uint32_t size;                 /* of this structure */
    uint32_t boot_mode;            /* 5.6 */
    uint8_t device_uuid[16];
    uint8_t boot_partition_uuid[16];
    char system_name[8];           /* "A", "B", or empty; NUL terminated */
    int32_t slc;                   /* -1 without an SLC */
    uint32_t active_key;
    uint32_t no_of_available_keys;
    uint32_t available_keys[16];
};
```

Its tag is allocated in the specification (requested upstream by Jonas, before 3.0.0); the
non-standard range is discouraged there.

### 5.5 What pb writes into a DT

**Not yet in the code (F-260)**: the board's hook runs in the middle of pb's properties, and
`dtb.c` reads the RoT, the SLC and the device id itself.

- `dtb.c` stays: it finds and grows the DT (`boot_dtb_grow()`, the headroom checked as in 4.3) and
  writes `/chosen`, from the information the core collected (5.6).
- Order: the board's hook first (`board.md` 2.7), with the whole tree; then pb's properties, so
  pb's values always win.
- pb writes only `/chosen`: its own properties, and for the image handover `linux,initrd-*`
  (`bootargs` is the board's, through its hook). Nothing else, nothing for Xen or OP-TEE.
- `CONFIG_BOOT_DTB_HEADROOM` (4 KiB) is the room for both.

### 5.6 pb's information

Collected once by the core, written by each handover in its format. **Not yet in the code
(F-260)**: the boot partition and the mode.

| Information | `/chosen` | pb's structure | From |
|---|---|---|---|
| The device's identity | `pb,device-uuid` | `device_uuid` | `device_uuid()` |
| The booted system | `pb,active-system` ("A", "B"; A/B only) | `system_name` | The policy |
| The boot partition | `pb,boot-partition` (UUID string), new | `boot_partition_uuid` | The policy, or command mode |
| How it was chosen | `pb,boot-mode`: "normal", "fallback", | `boot_mode`: 0, 1, 2 | `boot_policy_mode()` |
| | "speculative", new | | |
| The life cycle | `pb,slc` (absent without an SLC) | `slc` | `slc_read_status()` |
| The image's key | `pb,slc-active-key` | `active_key` | The header |
| The keys not revoked | `pb,slc-available-keys` | `available_keys` | The RoT |

New entries are appended (section 7).

## 6. Command mode's boot commands

- `boot part <uuid>`: one candidate, the partition the host names, which must be one of the
  policy's (bootable). No policy, nothing counted, mode normal.
- `boot bpak`: the host stream (4.1), the partition the host names in the handover.
- Both answer once the image is loaded and verified: the host's OK means "loaded and verified",
  not "booted" (`board.md` 5.4, F-252).
- `boot activate`, `boot status`: the policy's (3.4).

## 7. Fixed (compatibility)

Like the wire (D-008, `compatibility.md`):

- The A/B state's format and meaning (3.7), which `pbstate` and the OS write.
- The `/chosen` properties and their meaning (5.6, the README's table): only added to.
- Each handover's register convention and entry (5.2 to 5.4), and pb's structure (append only) and
  its tag once allocated.
- What an image that a released pb booted gets: its parts, entry and arguments under the same board
  configuration.

## 8. Review checklist

- [ ] The step's responsibilities stay its own (section 2's table); no hook between the header and
      the payload's hash.
- [ ] A policy writes its state before the load, and only what its decision changed.
- [ ] Every candidate goes through 4.2 and 4.3 (the custom source documents what it does instead).
- [ ] Placement is the allow list of 4.3, also for what a handover grows.
- [ ] A handover is an entry, four arguments and what it wrote; it reads pb's information, not the
      RoT, SLC or policy directly.
- [ ] A DT gets the board's hook first, then pb's properties, only in `/chosen`.
- [ ] Section 7 unchanged; new information appended.
- [ ] The markers of what was implemented are removed.

## 9. Not yet in the code

| Finding | What |
|---|---|
| F-256 | The policy interface (3.1 to 3.4), fixed and staggered, the loop with the fallback (F-174), each |
| | policy marking its candidates (F-215) |
| F-257 | Speculative mode (3.6, F-177) |
| F-258 | The handover model, `image`, TF-A's nested BL33 and BL32, `bl_params` from the platform, the DXL |
| F-259 | Firmware Handoff (5.4): the list, the region, pb's structure and its tag |
| F-260 | pb's information collected once, `pb,boot-partition`, `pb,boot-mode`, the hook's order |
| F-253, F-219 | The in place and custom sources (4.1) |
| F-254, F-255 | Placement (4.3), the initrd in 64 bits |
| F-243, F-248 | The watchdog per chunk, the boot's preconditions (`platform.md` 2.3) |
| F-173, F-175, F-176, F-179, F-180 | The A/B review's fixes, `pbstate` |
| F-252, F-076 | A stop in a boot command mode started; `dont-hash` |
