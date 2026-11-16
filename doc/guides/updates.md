# Updates: A/B with pbstate

**State: draft** (F-232), not reviewed. How a product updates its system with pb's A/B boot
source policy and `pbstate`, the OS's side of it. The rules are `boot.md` 3.5 to 3.7 and 5.6;
this guide cites them and adds nothing they don't say.

## 1. What is involved

- **Two slots**, the partitions System A and System B, each holding a signed boot image.
- **The state**, 512 bytes in two partitions, a primary and a backup (`boot.md` 3.7): which
  slot is enabled, whether each is verified (confirmed by the OS), the boot attempts a trial has
  left, an error bit per slot, and four board registers.
- **pb** reads the state at every boot, picks the slot, spends an attempt of a trial before it
  loads, and rolls back when a trial runs out (`boot.md` 3.5).
- **The OS** writes the new image into the slot that isn't running, starts a trial, and confirms
  the new system once it has checked itself, with `pbstate`.
- **The watchdog**: a system that hangs during its trial is reset by pb's watchdog, and the reset
  spends an attempt. Without `CONFIG_ENABLE_WATCHDOG` a hang stays hung.

## 2. The board

The board names the four partitions in a `struct boot_ab_state_config` and makes its policy
with `BOOT_POLICY_AB()` (`board.md` 2.5, jiffy's `board.c`):

```c
static const struct boot_ab_state_config boot_state_cfg = {
    .primary_state_part_uu = PART_primary_state,
    .backup_state_part_uu = PART_backup_state,
    .sys_a_uu = PART_sys_a,
    .sys_b_uu = PART_sys_b,
};

    .boot_policy = BOOT_POLICY_AB(&boot_state_cfg),
```

The state partitions are in the board's partition table like the slots. The OS finds them by
their UUIDs; it must not write them but through `pbstate`.

## 3. The OS's side: pbstate

`pbstate` (`tools/pbstate`) reads and writes the state; `libpbstate` is the same as a library.
It is given the two state partitions' devices:

```sh
pbstate -p /dev/mmcblk0p5 -b /dev/mmcblk0p6 -i      # print the state
```

The test bench wraps that in `pbstate-pb`, which finds the devices by the partitions' UUIDs
with `blkid` (`testbench/board/common/rootfs-overlay/usr/sbin/pbstate-pb`).

| Command          | What it writes                                                             |
| ---------------- | -------------------------------------------------------------------------- |
| `-s b -c 3`      | Enables B on trial: not verified, 3 boot attempts, the error bits cleared  |
| `-s b`           | Enables B and marks it verified at once: no trial, no rollback to A        |
| `-s none`        | Disables both: pb boots neither (command mode, or speculative mode, 3.6)   |
| `-v b`           | Marks B verified: the trial is over, B is the system from now on           |
| `-w 0 0x1234`    | Writes board register 0, which the board's pb may read                     |

Which slot booted, and how, is in the device tree pb hands over (`boot.md` 5.6):
`/chosen/pb,active-system` is "A", "B", or "?" for an image booted from RAM; `pb,boot-mode` is
"normal", "fallback" or "speculative".

## 4. An update, step by step

A runs and is verified. The new system goes into B:

1. **Write the image** into System B's partition, all of it. A torn write is no danger: pb
   verifies the image before it boots it (`boot.md` 4.2), and a B that doesn't verify is
   skipped for A in the same boot (3.5, rule 3).
2. **Start the trial**: `pbstate -s b -c 3`. B is enabled, not verified, with 3 attempts.
3. **Reboot.** pb spends one attempt, writes the state, and boots B.
4. **B checks itself** in whatever way the product decides: its services start, it reaches
   its server.
5. **B confirms**: `pbstate -v b`. From here B is the system, and A is its fallback while A
   stays verified.

What happens when B is bad:

- **B doesn't load or verify**: pb boots A in the same boot, writes nothing, and
  `pb,boot-mode` is "fallback". B's attempt is spent; it is tried again at the next boots until
  its attempts run out (3.5, rule 3).
- **B boots and hangs, or resets before it confirms**: each boot spends an attempt. When they
  are gone pb rolls back: A, verified, is enabled again, and B's error bit is set (rule 2).
- **B boots and never confirms**: the same, at the next resets.

Don't confirm too early: a confirmed B that turns out bad has no way back to A but another
update. Don't mark a system verified that didn't boot: `-s b` without `-c` does that.

## 5. The first boot, in the factory

`punchboot boot enable <UUID>` (command mode's `boot activate`) enables the slot and marks it
verified, and clears the other slot's verified bit and the error bits (`boot.md` 3.5). A device
activated in the factory therefore has no fallback until its OS has confirmed the other slot
once, after its first update.

Nothing enabled, as on a blank state, boots nothing: pb enters command mode, unless the build
has speculative mode.

## 6. Speculative mode

With `BOOT_AB_SPECULATIVE` a device with no viable slot doesn't wait in command mode: pb tries
the slots in turn, one per boot, as long as they verify, until the OS confirms one (`boot.md`
3.6). It relaxes "confirmed by the OS", never the signature. `pb,boot-mode` is "speculative".
It needs the watchdog. A device in the factory with it boots an image that was written and not
yet activated.

## 7. Not yet

`pbstate` handles A/B only, and pb and `pbstate` each have their own code for the state
(`boot.md` 3.7, F-180).
