# The boot flow contract

**State: reviewed (Jonas, 2026-11-11), D-032, D-038 to D-041, D-043, D-044, D-046, D-048,
D-050, D-055.** Level 2
(`process.md`).

No open items. The draft's seventeen, as decided and written into the text:

| Item | Rule | By |
|---|---|---|
| 1 | The transfer list is always in a board region, so a DT is copied into it (5.4) | Jonas, D-039 |
| 2 | Firmware Handoff replaces `simple` (5.4) | Jonas, D-039 |
| 3 | Speculative mode also starts with no slot enabled, A first; for now (3.6) | Jonas, D-040 |
| 4 | Preparing the handover is part of a candidate's attempt; a failed lockdown tries no | Level 1, D-038 |
| | other candidate (2) | |
| 5 | `boot part` takes only the policy's candidates, with every policy (6) | D-039 |
| 6 | pb's structure's fields, with room for 16 keys (5.4); the Kconfig names (3.5, 3.6, 5.4) | Jonas, D-040 |
| | pb's status: a string list in the DT, bit flags in pb's structure (5.6) | Jonas, D-041 |
| 7 | An image with a part outside the hash is refused (4.2) | Jonas, D-035 |
| 8 | `boot status` stays "the enabled slot" for A/B (3.4) | Level 1, D-038 |
| 9 | Speculative mode also starts when this boot's failures leave no candidate (3.6) | Jonas, D-039 |
| 10 | In place: the header where the format has it in the partition; a part is at its load | Jonas, D-040, |
| | address, inside the candidate's mapped range (4.1) | D-048 |
| 11 | The custom source is removed (4.1) | Jonas, D-035 |
| 12 | A trial whose attempt can't be written is started only when no verified slot is left | Jonas, D-040 |
| | (3.2) | |
| 13 | A boot command mode started keeps today's values in the handover (5.6) | Level 1, D-038 |
| 14 | `BOOT_AB_SPECULATIVE` is the only switch for speculative mode (3.6) | Jonas, D-040 |
| 15 | pb's structure isn't released before its tag is allocated (5.4) | Level 1, D-038 |
| 16 | `linux,initrd-*` stay 32 bits where both addresses fit (5.2) | Level 1, D-038 |
| 17 | The preconditions are checked before `early_boot()` (2) | D-039 |

From Jonas's final read: pb's status stays and pb's version joins pb's information (5.6, D-043);
the flow is written for any image format, and names bpak only in 4.4 (D-044). After the review
(D-046): the policy, the handover and the delivery are objects the board names in its data, no
longer a Kconfig choice (3, 5.1). In place, the board gives no address for the header (4.1,
D-048; D-039 had "or where the board says"). pb's status has a second entry, the board's
(5.6, D-050). Placement's allow list is the board's image data regions, and what keeps a part
off pb is checked once, on that list (4.3, Jonas, D-055; it was the board's DRAM and the
platform's loadable regions, less pb's own).

What pb does from `boot()` to the jump: which partition it boots (the boot source policy), how it
loads and verifies the image, and what it hands the next stage (the handover). For each step:
what it is responsible for, what it may rely on, and what a failure means. Decided in D-032 and
D-038 to D-040, in the style of `platform.md` and `board.md`, which it builds on. Vendor neutral.

Changing the boot flow, a policy or a handover: read this and `principles.md`, then go through the
checklist (section 8). Section 7 is fixed like the wire (`compatibility.md`).

**Not yet in the code (F-nnn)** marks what the code doesn't do yet. The commit that fixes the
finding removes the marker.

## 1. Words and scope

| Word               | Means                                                                                    |
| ------------------ | ---------------------------------------------------------------------------------------- |
| Boot flow          | Everything from `boot()` to the jump, and command mode's boot commands                   |
| Boot source policy | Which partitions this boot tries, in which order, and the state that decides it. One per |
|                    | board, chosen by the board's data: fixed, staggered chain or A/B                         |
| Candidate          | A partition the policy hands out to try                                                  |
| Source             | Where an image's bytes come from: a partition, the host's stream, memory mapped flash    |
| Verification       | The core's checks: the header's signature, where the parts land, the payload's hash      |
| Handover           | What the next stage gets: an entry, four arguments and what pb wrote in memory for it,   |
|                    | including pb's information (5.6). Two: image (Linux, Xen) and Firmware Handoff           |
| Delivery           | How the handover reaches the next stage: directly (`arch_jump()`) or through TF-A,       |
|                    | whose BL31 pb starts and which passes the handover on to BL33                            |

Not here: the platform's and the board's parts (`platform.md`, `board.md`), an image format's
own layout (bpak's: `bpak.md`), the wire (`compatibility.md`), the security life cycle and the RoT
(D-027).

The rules this flow implements are level 1's: what may run and in which state (`security.md` 4),
what pb promises the next stage and the OS (`system.md` 3 to 5). The flow is written for any image
format that gives it what 4.4 lists: "the header", "a part" and "its load address" are the
format's, whichever it is. The one format today is named only in 4.4, and where a command carries
its name (`boot bpak`), so that another format can come next to it (`system.md` 3, D-044).

Standards it leans on (`principles.md` 5): the Linux boot protocol
(`Documentation/arch/arm/booting.rst`, `arm64/booting.rst`), which Xen follows too (xenbits,
`misc/arm/booting.txt`), TF-A's `bl_params` (v2), OP-TEE's entry convention (TF-A's opteed), Arm's
Firmware Handoff (transfer list version 2, register convention 1), the devicetree specification's
`/chosen`.

## 2. The boot flow

```
boot()
  the serial download stop                       board.md 5.1, not in a boot command mode started
  the boot's preconditions                       security.md 4.5, platform.md 2.3
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

The two new steps:

- **The preconditions are checked in every boot**, also one command mode started, and the same
  in every life cycle state (`security.md` 4.5). What integrity rests on refuses the boot; what
  only availability rests on boots and is reported in the handover (5.6). **Not yet in the code
  (F-248).** They sit before `early_boot()`, so that a boot that will be refused starts no
  hardware.
- **pb's information is read before the loop.** The device's identity, the life cycle and the
  keys don't depend on the candidate, so a failure to read them is no candidate's fault and stops
  the boot: a life cycle that can't be read doesn't boot, with any handover (`security.md` 4.4,
  `system.md` 4). **Not yet in the code (F-260)**: today it fails in prepare, and only with a DT.

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
  policy hears it, and the next candidate is tried, so that a device with a verified system boots
  it (`security.md` 2, item 5). What a failed attempt left in memory is overwritten or ignored;
  nothing of it runs.
- **Before and after the loop, a failure stops the boot.** The preconditions, `early_boot()`,
  pb's information, `late_boot()` and `plat_lockdown()` decide about this boot, not about the
  image: no other candidate is tried, and pb enters command mode (in a boot command mode started:
  `board.md` 5.4). Without the lockdown pb never jumps (`security.md` 4.6). `boot()` returns the
  first candidate's error when every candidate failed.
- **No hook runs between the header's check and the payload's hash.** `early_boot()` is before the
  loop and `late_boot()` after it. Inside the loop the board runs only in prepare, on a verified
  image: its DT hook (5.5).
- **Boot time** (`principles.md` 4): the failure path may cost a candidate's load; the common path
  is one candidate.

## 3. Boot source policies

One policy per board, chosen by the board's data (D-046): `board.boot_policy` is the object of
the policy the board chose, the policy's functions (3.1 to 3.4) with the board's configuration
for it (3.5, `board.md` 2.5). The core calls the board's policy. Kconfig compiles a policy in
(`BOOT_POLICY_FIXED`, `BOOT_POLICY_CHAIN`, `BOOT_POLICY_AB`), more than one where a build wants
them (the host's tests), and a board that names one that isn't compiled in doesn't link.
**Not yet in the code (F-256, F-265)**: A/B is the only policy, wired into the core through
three callbacks; without it no partition is bootable (F-215).

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

When that write fails (**not yet in the code, F-179**: today any failed write ends the boot), the
write decides only where skipping it would be unsafe:

- A rollback that can't be written: the verified slot is handed out. The next boot decides the
  same again.
- Speculative mode's alternation that can't be written: the slot is handed out. A hang then comes
  back to the same slot, which is still better than no system.
- A trial's attempt that can't be written: while the other slot is verified, that slot is handed
  out instead (3.5, rule 3, mode fallback). When there is nothing else to boot, the trial is
  handed out uncounted. With a fault that lasts, pb makes this choice again on every boot: an
  image that verifies and hangs must not be started forever while a confirmed system sits in the
  other slot. The cost: with such a fault and a good new image, the device stays on its old
  system.

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
- `boot_policy_status(uuid)`: command mode's `boot status`. For A/B the enabled slot, or the
  null UUID when none is, as the wire has it today (`PB_CMD_BOOT_STATUS`): the meaning of a value
  on the wire is frozen (`compatibility.md`). A trial without attempts left is therefore still
  reported, though the next boot rolls it back. Fixed: its partition. Staggered chain: the first
  of the list.

### 3.5 The policies

Each policy says what the board gives it; the board's object is made by the policy's macro.

**Fixed** (`BOOT_POLICY_FIXED(part)`). The board gives one partition's UUID. Candidates: it. No
state. `boot activate`: not supported.

**Staggered chain** (`BOOT_POLICY_CHAIN(parts)`). The board gives a list of partitions' UUIDs, at
least one. Candidates: the list, in the board's order. No state. It protects against an image that
doesn't load or verify (corrupt storage, a torn write, a revoked key), not against one that
verifies and then fails: that one boots again on every reset. `boot activate`: not supported.

**A/B** (`BOOT_POLICY_AB(&config)`). The board gives the two slots' partitions and the primary
and backup state partitions (`struct boot_ab_state_config`). Two slots and the state of 3.7. Its
candidates, in order:

1. The enabled slot, when it is verified (confirmed by the OS), or when it is on trial with
   attempts left: one attempt is spent and written first (a write that fails: 3.2).
2. A trial without attempts left rolls back: the other slot, when it is verified, is enabled, and
   the rolled back slot's error bit is set (written). Not verified: no viable candidate.
3. When the first candidate failed in this boot: the other slot, when it is verified. Nothing is
   written, mode fallback (F-174, decided by Jonas for this boot only). A trial that failed spent its
   attempt; it is tried again on the next boots until its attempts run out.
4. No viable candidate: none, and pb enters command mode; with speculative mode, 3.6.

Nothing enabled (a device never activated, a state that was lost) has no candidate, and pb enters
command mode; with speculative mode, 3.6. `boot activate X` enables X and marks it verified, as
today; the null UUID disables both. The OS starts a trial and confirms it through `pbstate`.

As today, `boot activate X` also clears the other slot's verified bit and the error bits
(`boot_ab_state_set_boot_partition()`; wire semantics, D-008). A device activated in the factory
therefore has no fallback candidate (rule 3) until the OS has confirmed the other slot once.

### 3.6 A/B's speculative mode [optional, BOOT_AB_SPECULATIVE]

**Not yet in the code (F-257)**: today's speculative mode gives the fallback one trial attempt
(two boots, F-177) and drops a rollback bit.

For a system that must stay reachable for an update when no slot is viable. With it:

1. It starts when A/B has no viable candidate (3.5, 4). That is one of three cases:
   - **From the state**: the enabled slot is a trial without attempts left and the other slot
     isn't verified. Items 3 and 4 apply.
   - **From this boot**: the candidates of 3.5 failed to load or verify, and a slot that isn't
     confirmed is left to try. It is tried for this boot only and nothing is written, like the
     fallback of 3.5, rule 3: the enabled slot keeps its state and is tried first again on the
     next boot.
   - **Nothing enabled**: a state that is blank, lost or disabled by the host. A is tried first;
     items 3 and 4 apply. In the field almost anything beats looping into command mode (Jonas,
     D-040, for now). Released speculative mode stays in command mode here. The cost: with this
     mode "disable both slots" doesn't park a device in command mode, and a device in the factory
     boots an image that was written and not yet activated.
2. It never boots an image that fails verification: it relaxes "confirmed by the OS", never the
   signature, the hash or the placement (`security.md` 4.1, 4.2).
3. It alternates per boot. Before the load it enables the other slot and writes it, so a hang and
   the watchdog lead to the other one. In the same boot, when its slot fails, the other is tried,
   without another write.
4. It counts no attempts. It ends when the OS confirms a slot through `pbstate`.
5. The handover's mode is speculative (5.6).
6. It depends on `CONFIG_ENABLE_WATCHDOG`: without one, a hang stays hung.
7. No format change: the first and the third case follow from the state and their alternation is
   `enable`; the second writes nothing.

`BOOT_AB_SPECULATIVE` is the only switch, and it carries the watchdog dependency. The board's
`rollback_mode` field (`struct boot_ab_state_config`) goes: two switches that must agree fail
silently when a board sets one and forgets the other, and every board file is rewritten for 3.0.0
anyway (D-026).

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

| Source      | Used by                                  | What                                                      |
| ----------- | ---------------------------------------- | --------------------------------------------------------- |
| Partition   | The policies, `boot part`                | A bio partition, the header where the format has it (4.4) |
| Host stream | `boot bpak`                              | Command mode reads the header, then the parts, from the   |
|             |                                          | host                                                      |
| In place    | Boards that run from memory mapped flash | A partition mapped at an address: the image is            |
|             |                                          | verified where it is and runs there                       |

**Not yet in the code (F-253, F-219)**: the board can't select the in place source since D-026,
and the generic handover jumps to another address than the one that was hashed. The in place
source becomes a property of a partition, so that any policy can hand one out: a flag in the
board's partition table, on a device the board says is mapped (`board.md` 2.4).

The in place source's rules:

- Nothing is copied. A part is at its load address (4.4), as in released pb: it is hashed there
  and the entry is there, never at another address (`security.md` 4.2). For code that runs in
  place that is the address the part was linked for, so the image has it anyway.
- Placement (4.3): the candidate's mapped range, without the header, is the allowed memory for
  its parts. An image for slot A that was written to slot B is therefore refused as a candidate,
  not started. The handover's own memory (the Firmware Handoff region) is in RAM as always.
- The header is where the format has it in a partition (4.4), read through the map, as for a
  partition that is loaded. The board gives no address for it (D-048): released pb's
  `get_in_mem_image()` doesn't come back.
- Every image released pb accepted has its parts at their load address, so none is refused.

### 4.2 The sequence

The same steps for every image format; what a step reads from the image is the format's (4.4).

1. Read the header from where the format has it in the source. It is a valid header of a format
   this build accepts (the format's own check).
2. The key it names is in the keystore and not revoked (`rot_read_key_status()`).
3. The header's signature verifies with that key (`boot_image_auth_header()`).
4. Every part has a load address, lies inside its source (the partition without the header) and
   lands in allowed memory (4.3).
5. Load and hash each part, chunk by chunk, kicking the watchdog per chunk. **Not yet in the code
   (F-243)**: no kick on the boot path.
6. The payload's hash is the header's (`boot_image_verify_payload()`).
7. Later: the image's version isn't below the floor (anti-rollback, roadmap). A refused image is a
   failed candidate; the policies don't change.

Unverified bytes reach memory in 5, before 6: that is why 4 comes first. Nothing runs, and nothing
is patched, before 6.

An image with a part outside the payload's hash is refused in step 4, before anything is loaded
(D-035, `security.md` 4.2). A format may allow such a part (4.4) for content verified at run time,
such as a file system under a hash tree whose root hash is signed; pb doesn't boot such content,
and loading it would leave bytes nothing authenticated in the next stage's memory. **Not yet in
the code (F-076)**: today such an image fails on its payload hash, so none that booted is refused.

### 4.3 Placement

**Not yet in the code (F-254)**: the parts are only kept off some of pb's sections.

An allow list: the board's image data regions (`board.md` 2.9, D-055), the only memory an image's
parts are loaded into. Every part, and the room a handover grows it by (the DT's headroom), lies
inside one of them and overlaps no other part. Address ranges that wrap are refused. A load
address the arch can't hold (above 4 GiB on a 32-bit arch) is refused. The in place source: 4.1.

The rule names nothing of pb's. That no region holds pb's sections (all of them, the keystore
and the RAM log too) or what a handover writes outside the parts (the Firmware Handoff region,
5.4, and TF-A's `bl_params` where the platform puts them, 5.3) is checked once, on the board's
list, before any image is read (`board.md` 3, `platform.md` 5.3). A list that fails is the
board's failure (`board.md` 2.1), not a refused image. A board without a list loads nothing:
only a partition that is booted in place boots.

### 4.4 The image format

The flow doesn't depend on one image format (D-044). It needs what the table lists from a format,
and nothing else of it. bpak is the one format today (`bpak.md`); another comes next to it and
gives the same, and every bpak image a released pb accepted stays accepted (D-008, `system.md` 3).

| The flow needs | bpak |
|---|---|
| A header of a bounded size, at a place the source alone gives, | `struct bpak_header`, 4 KiB: a partition's last |
| and a check of it that relies on nothing in the image | 4 KiB, first in the host's stream; |
| | `bpak_valid_header()` |
| The id of the key that signed it | `key_id`. `keystore_id` is logged and not |
| | compared: pb has one keystore, and a check added |
| | now could refuse images that boot today (D-008) |
| A signature over the header, and the hash it is made with | In the header, zeroed while the header is hashed; |
| | `hash_kind`: SHA-256, SHA-384 or SHA-512 |
| The parts: for each an id, where it is in the source, its size | The part headers; the load address is the part's |
| and its load address | `pb-load-addr` meta |
| One hash over the bytes of the parts, in the signed header | `payload_hash`. A part with the `dont-hash` flag |
| | (`BPAK_FLAG_EXCLUDE_FROM_HASH`) is outside it: |
| | refused (4.2) |
| Signed attributes of the image, for a handover | Metas: `optee-arch` (5.3) |

- A format's code reads and checks its own layout, and answers the table's questions. The steps
  of 4.2, placement (4.3) and the handovers are the core's, the same for every format: a format
  adds no step and skips none.
- The signature's algorithm is the key's (the keystore), not the format's.
- Today the core reads bpak's header directly (`image_helpers.c`, `boot.c`, the handovers). The
  format's interface in the code comes with the second format, and with it what the format
  changes outside this table: the part ids a board names (`board.md` 2.7), the host's command,
  whether one build accepts several formats (`roadmap.md`).

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

One handover and one delivery per board, chosen by the board's data (D-046, `board.md` 2.7):
`board.boot_handover` is the handover's object with the board's configuration for it
(`BOOT_HANDOVER_IMAGE(&config)`, `BOOT_HANDOVER_FH(&config)`), `board.boot_delivery` the delivery's
(`BOOT_DELIVERY_TFA(&config)`; absent: directly). Each section below says what the board gives. Not
by the image: the same parts mean different things on different boards today (the DXL's `atf` is
started directly, as an image, the MEK's through `bl_params`), so letting the image choose would
change what old images do (D-008). A signed attribute of the image (4.4) that chooses can come
later, opt in.

### 5.2 Handover : image (Linux, Xen) [optional, CONFIG_BOOT_LINUX]

The Linux boot protocol, which Xen follows too. **Not yet in the code (F-258)**: named `linux`.

| Arch | Registers |
|---|---|
| ARMv7-A | r0 = 0, r1 = 0xffffffff (no machine type, the DT describes it), r2 = the DT |
| ARMv8-A | x0 = the DT, x1 to x3 = 0 |

The board gives: the kernel's part (its load address is the entry), the DT's and the ramdisk's
(both optional), its DT hook (optional, 5.5) and `set_dtb_boot_arg`. Without `set_dtb_boot_arg`
all four registers are 0. The DT gets pb's `/chosen` (5.5) and,
with a ramdisk, `linux,initrd-start` and `-end`.

**Not yet in the code (F-255)**: a ramdisk above 4 GiB gets a truncated address. The encoding: 32
bits when both addresses fit, as today, and 64 bits only when one doesn't, which never worked. The
root's `#address-cells`, as U-Boot does, would turn today's 4 bytes into 8 on every 64-bit board;
Linux reads both, another next stage may not, and an image that boots today gets the same bytes
(`system.md` 5.1).

At pb's exception level: on a platform where pb runs at EL3, Linux and Xen go through TF-A (Xen
needs Non-Secure EL2, its `booting.txt`). Xen's dom0 kernel and ramdisk are `/chosen` module nodes
(`multiboot,module`): they are in the image's DT, written when the image is built from the same
addresses as the parts. pb writes nothing for Xen.

### 5.3 Delivery : TF-A [optional, CONFIG_BOOT_ATF]

ARMv8-A only. The board gives: BL31's part and BL32's (optional). pb jumps to BL31 with x0 =
`bl_params` (v2) and x1 to x3 = 0. `bl_params` hold:

- **BL33**: its entry and SPSR (AArch64; EL2 when the CPU has it, else EL1), and as its arguments
  the board's handover: image (Linux, Xen) or Firmware Handoff. **Not yet in the code (F-258)**:
  Linux's only.
- **BL32** (optional): OP-TEE's convention, as TF-A's opteed reads it. arg0 = OP-TEE's execution
  state as TF-A encodes it (`MODE_RW_64` or `MODE_RW_32`), from the image's attribute for it
  (4.4: 64 or 32; 64 without it). arg1 = 0 (no pageable part), arg2 = 0 (no memory limit),
  arg3 = the DT BL33 gets, or 0 without one. Others can be added.

Where `bl_params` may live is the platform's to say (NXP's i.MX8M BL31 takes them only from low
OCRAM). **Not yet in the code (F-258)**: `atf.c` puts them in `.data` for it. The DXL moves to this
delivery (it starts BL31 directly today, as an image). TF-A's own transfer list (tag 0x102, the
AArch64 entry point info) is a later path, for a BL31 that takes it.

### 5.4 Handover : Firmware Handoff, generic [optional, CONFIG_BOOT_FH]

**Not yet in the code (F-259)**: the generic handover is `simple` (r0 = the system's name, r1 = the
boot partition's UUID, only on the 3.0.0 branch). Released pb (`armv7m_baremetal`) passed nothing,
so images that ignore their registers keep working. It replaces `simple`, it doesn't stand next
to it.

For any next stage: an RTOS, an application, a hypervisor, a stage that parses a DT. The board
gives: the entry's part (on ARMv7-M its vector table, D-028; in place, its mapped address), the
DT's part (optional) and its DT hook (optional, 5.5). The region is Kconfig's, below.

| Arch    | Registers                                                                                |
| ------- | ---------------------------------------------------------------------------------------- |
| AArch64 | x0 = the DT in the list or 0, x1 = 0x4a0fb10b with bits 39:32 = 1, x2 = 0, x3 = the list |
| AArch32 | r0 = 0, r1 = 0x0fb10b with bits 31:24 = 1, r2 = the DT in the list or 0, r3 = the list   |
| ARMv7-M | AArch32's. The specification covers AArch64 and AArch32 only: pb's extension             |

The transfer list holds one of two, never both:

- **The image's DT**, when the board names a DT part: pb opens the verified DT into the list's
  FDT entry (tag 1) with the headroom (`fdt_open_into()` copies and grows in one step), then the
  board's hook and pb's `/chosen` work on that copy (5.5). The register points to it. The headroom
  is the region's, so this handover needs none at the part's load address (4.3). An image without
  the part fails to boot, and so does a DT that doesn't fit the region with its headroom.
- **pb's structure**, when the board names none: one entry with pb's tag.

The list is in a region of the board's (`BOOT_FH_REGION_BASE` and `BOOT_FH_REGION_SIZE`), also
with a DT: pb's information changes on every boot, it isn't part of the image. The region is
checked like a part (4.3) and is 8-byte aligned. The image's memory map keeps it free until the
next stage has read or moved the list: it is part of the board's interface to image authors
(`board.md` 8). The list is version 2 with checksums on, and has no pointers into itself (the next
stage may move it). The specification describes the checksum byte in two ways
(`firmware-handoff.md`): the code follows the reference library and is tested against a receiver.

pb's structure: little-endian, no pointers, fields only appended (D-008). The entry's `data_size`
is its size, so a receiver reads the fields `data_size` covers, and the structure carries no size
or version of its own.

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
    uint32_t status;               /* 5.6: what pb booted without, bit flags; 0: nothing */
    char version[32];              /* 5.6: pb's version, NUL terminated */
};
```

A build whose keystore has more than 16 keys, or whose version string doesn't fit, doesn't compile
with this handover (static asserts).

Its tag is allocated in the specification (requested upstream by Jonas, before 3.0.0); the
non-standard range is discouraged there. A tag is allocated by a pull request that adds the
entry's layout to the specification, so the layout above is settled first. pb's structure isn't
in a release before its tag is allocated: what is released is
fixed (section 7, `system.md` 3), and the tag can't change afterwards. The DT variant doesn't wait
for it.

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
(F-260)**: pb's version, the boot partition and the mode. The version is the build's; the
identity, the life cycle and the keys not revoked are collected before the loop (2); the system,
the partition, the mode and the image's key belong to the candidate.

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
| What pb booted without | `pb,status`, string list, new, always | `status`, bit flags | The preconditions |
| | written | | (2), the board |
| pb's version | `pb,version`, string, new | `version` | The build |
| | | | (`version.txt`) |

New entries are appended (section 7). The encodings are part of what section 7 fixes.

pb's version is the string the wire's version command answers (`PB_CMD_BOOTLOADER_VERSION_READ`):
`version.txt` when pb was built, "3.0.0". It tells the OS which pb booted it without command mode,
for an inventory or a report (D-043). A next stage that needs one of the entries above looks for
that entry, not for a version: an entry that is absent was added after the pb that booted it.

pb's status (`system.md` 3, `security.md` 4.5) is what pb couldn't do and booted anyway, so that
the next stage can report it or act on it. **Not yet in the code (F-248).** A list of strings in
the DT, its own idiom and readable without a table; bit flags in pb's structure, whose fields
have a fixed size (Jonas, D-041):

| Condition | String in `pb,status` | Bit in `status` |
|---|---|---|
| The watchdog didn't arm (`platform.md` 2.3) | `watchdog-not-armed` | 0 |
| The board reported a failure the boot goes on without (`board.md` 2.3) | `board-degraded` | 1 |

- A condition gets its string and its bit together, when its precondition is added (F-248,
  `platform.md` 3.8). Neither is reused or renamed afterwards (section 7).
- Nothing to report: `pb,status` is written and empty, `status` is 0. The property is always
  there, so that an OS can tell "nothing" from a pb that doesn't report.
- A receiver ignores strings and bits it doesn't know.
- When 32 bits run out, a second field (`status2`) is appended to pb's structure, which a
  receiver finds through `data_size` like any appended field. The string list has no limit.

A boot command mode started keeps today's values (`compatibility.md`):

- `pb,active-system` is "?" when the partition isn't a slot: `boot bpak` without a UUID, or with
  another one (`boot_ab_part_uu_to_name()`).
- The UUID `boot bpak` names is taken as given: the host is authenticated on a locked device
  (`security.md` 4.7), and the image is verified like any other (4.1 there).
- The mode is normal.
- New: `pb,boot-partition` is absent, and `boot_partition_uuid` all zero, when `boot bpak` names
  no partition.

## 6. Command mode's boot commands

- `boot part <uuid>`: one candidate, the partition the host names, which must be one of the
  policy's (bootable). No policy, nothing counted, mode normal. For A/B that is what the wire
  does today (`boot.c` refuses a partition without `BIO_FLAG_BOOTABLE`), so it stays
  (`compatibility.md`), and the fixed and the chain policy get the same rule: what else a host
  wants to boot it sends with `boot bpak`.
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
  to. The same for the status' strings and bits.
- `boot activate` and `boot status` as the wire has them (3.4, 3.5).
- Each handover's register convention and entry (5.2 to 5.4), and pb's structure (append only) and
  its tag once allocated.
- What an image that a released pb booted gets: its parts, entry and arguments under the same board
  configuration.

## 8. Review checklist

- [ ] The step's responsibilities stay its own (section 2's list); no hook between the header and
      the payload's hash.
- [ ] A policy writes its state before the load, and only what its decision changed; a trial
      whose attempt can't be written is started only when no verified slot is left.
- [ ] Every candidate goes through 4.2 and 4.3, and every byte pb loads is hashed.
- [ ] The flow names no image format: what it reads from an image is in 4.4's table.
- [ ] Placement is the allow list of 4.3, also for what a handover grows or writes.
- [ ] A handover is an entry, four arguments and what it wrote; it reads pb's information, not the
      RoT, SLC or policy directly. The delivery changes none of them.
- [ ] A DT gets the board's hook first, then pb's properties, only in `/chosen`.
- [ ] Section 7 unchanged; new information appended.
- [ ] The markers of what was implemented are removed.

## 9. Not yet in the code

| Finding | What |
|---|---|
| F-256 | The policy interface (3.1 to 3.4), fixed and staggered, their Kconfig names, the loop with |
| | the fallback (F-174), each policy marking its candidates (F-215) |
| F-265 | The policy's, the handover's and the delivery's objects in the board's data (3, 5.1) |
| F-257 | Speculative mode (3.6, F-177): nothing enabled, Kconfig as its only switch |
| F-258 | The handover and delivery model, `image`, TF-A's BL33 and BL32, `bl_params` from the |
| | platform, the DXL |
| F-259 | Firmware Handoff (5.4): the list, the region, pb's structure and its tag |
| F-260 | pb's information collected once, before the loop; `pb,version`, `pb,boot-partition`, |
| | `pb,boot-mode`, the hook's order |
| F-253, F-219 | The in place source (4.1), its rules; the custom source's removal |
| F-254, F-255 | Placement (4.3), the initrd above 4 GiB (5.2) |
| F-243, F-248 | The watchdog per chunk, the boot's preconditions in the flow (2), pb's status in the |
| | handover (5.6) |
| F-173, F-175, F-176, F-179, F-180 | The A/B review's fixes, a failed state write (3.2), `pbstate` |
| F-252, F-076 | A stop in a boot command mode started; a part outside the hash refused (4.2) |
