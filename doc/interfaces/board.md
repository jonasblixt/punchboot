# The board contract

**State: reviewed (Jonas, 2026-11-12), D-031, D-045 to D-052.** Level 3 (`process.md`). Two
entries were added after it, in the review of `platform.md`, and reviewed there: `mmap` (2.8,
D-054) and `image_regions` (2.9, D-055).

What a board (`src/board/<b>/`, or a directory out of the tree) provides, when the core calls it,
and what it may rely on. Decided in D-031, in the style of the platform contract (`platform.md`,
D-030), which it builds on. It is independent of any SoC vendor: the i.MX boards' pins and clocks
are in `plat-imx.md`.

Read with it: `boot.md` (reviewed), which says where the board's hooks and data sit in the boot
flow, and level 1 (`security.md`, `system.md`), whose rules the entries cite. A pass against them
(D-045) rewrote 2.5, 2.7 and the flow of section 2. D-046 changed how a board gives a module its
configuration: one member per concept, an object the module provides (section 1).

**No open items.** What the pass (D-045) left open and what the review added, as decided and
written into the text:

| Item                                   | Rule                                                        | By           |
| -------------------------------------- | ----------------------------------------------------------- | ------------ |
| The policy's and the handover's fields | One member per concept, a variant is an object; a module's  | Jonas, D-046 |
|                                        | configuration is documented with the module (1, 2.5, 2.7)   |              |
| The in place source                    | A flag in the static table's entry, the device's mapped     | Jonas, D-048 |
|                                        | base, the header where the format has it (2.4)              |              |
| bpak in `late_boot()`                  | No header argument; `boot_image_get_bpak_header()` (5.3)    | Jonas, D-047 |
| A board's condition in pb's status     | `board_report_degraded()`, `board-degraded` in the handover | Jonas, D-050 |
|                                        | (2.3)                                                       |              |
| `cm_init()` and `cm_transport`         | Both leave the board: command mode's USB is the platform's, | Jonas, D-049 |
|                                        | the transport is chosen at link time (4.1, 4.2)             |              |
| What runs while pb is stopped          | Tasks, `DEFINE_TASK()`, in a loop of the core's; command    | Jonas, D-051 |
|                                        | mode is one of them, and its `process` hook goes (4.5)      |              |

Changing or adding a board: read this, then go through the review checklist (section 9). A board
that runs Linux also gets its test bench defconfig (`testbench/README.md`, "Adding a board").
Section 8 is as fixed as the wire for a product in the field (`compatibility.md`).

Entries are tagged:

- `[mandatory]`: every board sets it.
- `[optional]`: a board may leave it NULL; the entry says what happens then.
- `[mandatory with X]`: mandatory when the Kconfig option X is on.

Each entry says from when it is called ("valid from") and what a failure means.

**Not yet in the code (F-nnn)** marks what the code doesn't do yet. The commit that fixes the
finding removes the marker here and updates the board's row in the conformance table (section 10).

## 1. Scope

- **A board is what the PCB and the product decide** (D-026, `system.md` 6): the console, the
  storage bring-up, the pins, the clocks of the peripherals it selects (`platform.md` 7),
  the partition layout, the boot source policy and its data, the handover and its delivery, the
  configuration fuses, its own command mode extensions.
- **Not the SoC's facts.** They are the platform's (`platform.md` 1). A board can't replace a
  platform service, and it asks the platform what the ROM did (D-009): it doesn't read the ROM's
  boot mode itself.
- **Not the rules.** The core owns them (`security.md` 4.8, D-027): key revocation and the SLC
  order (`rot.c`, `slc.c`), the image verification, command mode's auth gate, the boot's
  preconditions (`security.md` 4.5). A board's hook can stop a boot, never make one happen that
  the core refused (`security.md` 4.1).
- **A board is data.** `const struct board board` (`include/pb/board.h`), which the core sets up
  (`src/board.c`). What boards repeat moves into the core: as little board code as possible.
- **One member per concept, and a concept with variants is an object** (D-046). The partition
  table, the boot source policy, the handover and its delivery each have one member. The board
  fills it with an object that the module it chose provides: the module's functions and the
  board's configuration for it, made by the module's macro (`BOOT_POLICY_AB(&config)`). The
  board's data makes the choice. Kconfig compiles a module in, and a board that names one that isn't
  compiled in doesn't link. A module's configuration is documented with the module (`boot.md` 3.5
  and 5), not here. A new variant is a new object: `struct board` doesn't change for it.
  An object is a member by value: the module's functions and a pointer to the board's
  configuration (`include/pb/ptbl.h`, `boot/policy.h`, `boot/handover.h`). `src/board.c`
  calls through it and includes no module's header.
- **In or out of the tree.** `make BOARD=<path>` builds a board from any directory (section 7).
  `struct board`, the meaning of its hooks and the build interface change only in a major release
  (D-018, `system.md` 3). A new object for a member is no such change.
  **Not yet in the code (F-240)**: CI builds an out-of-tree board, the level 3 override of
  `platform.md` 6.2, which tests this interface too.

## 2. Lifecycle

```
main()
  plat_early_init()
  board.console_init()                     2.2
  plat_init()                              the memory map, with board.mmap: 2.8
  board_setup()                            src/board.c
    the board's data is checked            3
    rot_init(board.rot), slc_init(board.slc)
    board.init()                           2.3
    the partition table                    2.4
    boot_policy_init()                     2.5, boot.md 3.1
    board.late_init()                      2.6
    the handover                           2.7, also after a failure (2.1)
  boot()                                   boot.md 2
    the serial download stop               5.1
    the boot's preconditions               security.md 4.5
    board.early_boot()                     5.2
    per candidate: load, verify, prepare   the board's DT hook runs in prepare (2.7)
    board.late_boot()                      5.3
    plat_lockdown(), the jump
  the tasks, in turn, until one ends it    4.5: when board_setup() or boot() failed or was stopped
    command mode's, with CONFIG_CM         4.1, 4.2: plat_cm_init(), the transport
    the board's, a platform's, a driver's
  plat_reset()
```

### 2.1 Failure: a board failure stops the boot, never command mode

When a step of `board_setup()` fails, pb doesn't boot from storage. It enters command mode, and
command mode still has its boot commands (`system.md` 5.4: a boot that fails ends in command
mode, with the reason in the log).

`board_setup()` runs the board's steps in order (the list in section 2). When one fails it logs
which, skips the steps after it, sets up the handover all the same, and returns the step's error.
`main()` then enters command mode, where the host can install the partition table, repair the
policy's state (`boot activate`) or boot an image from RAM (`boot bpak`).

- A board failure doesn't reset (a platform failure does, `platform.md` 2): command mode has what
  `plat_init()` set up.
- A failed RoT or SLC fails closed (`security.md` 4.4). Command mode asks for authentication when
  the life cycle is unknown (`slc_requires_auth()`), a boot that command mode starts is refused
  like any other (`system.md` 4, `boot.md` 2), and without the RoT no image verifies
  (`rot_get_dsa_key()` refuses).
- `boot bpak` needs neither the partition table nor the policy. `boot part` takes only the
  policy's candidates (`boot.md` 6), which a failed `boot_policy_init()` may not have marked.
- What failed is in pb's log, which command mode reads (`punchboot log`).
- Without a host, command mode ends by itself and pb resets (`system.md` 2): a step that failed
  on a fault that passes is run again.
- A build without command mode (`CONFIG_CM` off) stops the same way: its tasks run (4.5), and
  with none pb resets.

### 2.2 Function : console_init() [optional]

```
Argument : void
Return   : void
```

Called once by `main()`, after `plat_early_init()`, before `plat_init()`. It is on every boot:
keep it short (`principles.md` 4, D-023). It sets up the debug UART: its clock and pads, the UART
driver, then `console_init()` with the UART's ops.

Valid from: what `plat_early_init()` provides (`platform.md` 2.1): the tick, the watchdog, the RAM
log, and what the platform header says works from early init (a system controller's handle,
`imx8x_ipc()`). Not yet: DRAM the ROM didn't set up, the MMU, `.no_init`, the fields of
`<p>_plat()` that `plat_init()` sets.

Failure: it can't fail. A UART that doesn't work leaves the RAM log (`CONFIG_RAMLOG`), which keeps
everything printed before and after.

NULL: no console, the RAM log only.

### 2.3 Function : init() [optional]

```
Argument : void
Return   : int, PB_OK or -PB_ERR_*
```

Called once by `board_setup()`, after the RoT and the SLC. Valid from: what `plat_init()` provides
(`platform.md` 2.2): DRAM, the MMU, fuses, `<p>_plat()`. It sets up the hardware the boot needs:
the storage the boot loads from (its bio devices and their partition UUIDs), and until F-247 the
crypto engine and its RNG. What only command mode needs isn't the board's (4.1, D-023).

Failure, sorted the way `security.md` 4.5 sorts a failed precondition:

- What the boot can't go on without, or what its security rests on, returns an error (2.1): the
  device the boot loads from; until F-247 the crypto engine the boot verifies with, unless a
  fallback verifies the same.
- Anything else is logged, shown by `status()` (4.3), and returns PB_OK. jiffy: the CAAM's RNG,
  which Linux needs and pb doesn't.

The second kind is what pb couldn't do and booted anyway, which pb reports to the next stage
(`system.md` 5.4). The log and `status()` need a host, so the board also calls
`board_report_degraded()`, and the handover carries `board-degraded` in pb's status
(`boot.md` 5.6, D-050). It is one entry for every board: what it stands for is in the log and
in `status()`. Valid in `init()`, `late_init()` and `early_boot()`; pb's information is
collected after `early_boot()` (`boot.md` 2), so a call from `late_boot()` comes too late.

NULL: no hardware of the board's own.

### 2.4 Data : ptbl [optional]

The partition table on a bio device that `init()` set up. The core reads it after `init()`.

| Object                       | What the board gives                                                            |
| ---------------------------- | ------------------------------------------------------------------------------- |
| `PTBL_GPT(device, tables)`   | The device's UUID, and the layouts command mode can install                     |
|                              | (`part install`), each with a name and a variant (`struct gpt_table_list`)      |
| `PTBL_STATIC(device, table)` | The device's UUID and the partitions themselves                                 |
|                              | (`struct static_part_table`): for a device without a table on it (the RT's NOR) |

Failure: no valid table: 2.1. Command mode installs a GPT.

Absent: no table of the core's. The board's `init()` created its partitions, or it has none.

One table is the core's. A board with a second device that has a table sets that one up in
its `init()` with the driver's function (the host: its GPT is `ptbl`, its flash's static table
is `static_ptbl_init_mapped()` in `init()`).

The device a table is on is left visible only, for its capacity: a host writes its
partitions, not the device (`bio.md` 2.4). The core does that for its table of any kind; a
board that sets up another table does it for that one's device (the host, for its flash).

**The in place source** (`boot.md` 4.1): a partition that is memory mapped, whose image is
verified where it is and runs there.

`boot.md` makes in place a property of a partition, so that any policy can hand one out. How a
board gives it (Jonas, D-048):

- The static table's object gets the address the device's first byte is mapped at
  (`PTBL_STATIC_MAPPED(device, base, table)`), and an entry gets a flag
  (`STATIC_PART_FLAG_IN_PLACE`): this partition is booted in place. Its mapped address follows
  from the base and the entry's first block, so no second number has to agree with the first,
  and no address stands for "not mapped" (flash can be mapped at 0).
- A candidate with the flag is verified in place, with its mapped range as the allowed memory
  for its parts (`boot.md` 4.1); any other candidate is loaded.
- The header is where the image's format has it in a partition (`boot.md` 4.4: for bpak the
  last 4 KiB), read through the map. The board gives no address for it, and released pb's
  `get_in_mem_image()` doesn't come back. A format with its header first says so itself, for
  every source.
- A GPT's partitions are never booted in place: the table comes from the storage, an address
  can't. The mapped range is always the board's table's, also for a device that names itself
  with the UUID of a partition that is booted in place. A partition that is booted in place
  stays so: its table isn't replaced by another mapped table.
- The entry is the part's load address with every source, never a second address computed
  from the partition's base: `boot_simple` has no `image_base_address` and no `image_size`.

### 2.5 Data : boot_policy [mandatory]

The board's boot source policy (`boot.md` 3): the object of the policy the board chose, with
the board's configuration for it. `boot_policy_init()` runs after the partition table
(`boot.md` 3.1).

| Object                     | What the board gives (`boot.md` 3.5)                                    |
| -------------------------- | ----------------------------------------------------------------------- |
| `BOOT_POLICY_FIXED(part)`  | The one partition's UUID                                                |
| `BOOT_POLICY_CHAIN(parts)` | The partitions' UUIDs, in the order they are tried: an array of         |
|                            | pointers to UUIDs, whose size the macro takes                           |
| `BOOT_POLICY_AB(&config)`  | `struct boot_ab_state_config`: the primary and backup state partitions, |
|                            | System A and B                                                          |

- A/B's configuration has no `rollback_mode`: `BOOT_AB_SPECULATIVE` is the only switch for
  speculative mode (`boot.md` 3.6, D-040).
- The A/B state's format is shared with `pbstate` in the OS (`boot.md` 3.7). Its four board
  registers are the board's (section 6).
- The policy marks its candidates bootable (`boot.md` 3.1). A board doesn't set
  `BIO_FLAG_BOOTABLE` itself (the host does, in a policy object of its own, F-301).

Failure: a partition the configuration names isn't in the table, or the policy's state can't be
read or repaired: 2.1.

### 2.6 Function : late_init() [optional]

```
Argument : void
Return   : int, PB_OK or -PB_ERR_*
```

Called once by `board_setup()`, after the partition table and the policy's state: for what needs
the partitions. Failure as in 2.3. NULL: nothing.

### 2.7 Data : boot_handover [mandatory], boot_delivery [optional]

The handover and its delivery (`boot.md` 5.1): one of each per board, chosen by the board's data
and never by the image.

| Member          | Object                         | What the board gives                                    |
| --------------- | ------------------------------ | ------------------------------------------------------- |
| `boot_handover` | `BOOT_HANDOVER_IMAGE(&config)` | The image handover (`boot.md` 5.2): the kernel's part,  |
|                 |                                | the DT's and the ramdisk's, the DT hook,                |
|                 |                                | `set_dtb_boot_arg`                                      |
|                 | `BOOT_HANDOVER_FH(&config)`    | Firmware Handoff (`boot.md` 5.4): the entry's part, the |
|                 |                                | DT's, the DT hook                                       |
| `boot_delivery` | `BOOT_DELIVERY_TFA(&config)`   | Through TF-A (`boot.md` 5.3): BL31's part and BL32's.   |
|                 |                                | BL33 is the handover's entry. Absent: directly          |

- `BOOT_HANDOVER_FH()` replaces `boot_simple` (`boot.md` 5.4, D-039). Its region is an address
  the build needs, so it is Kconfig's (`BOOT_FH_REGION_BASE` and `_SIZE`), like the board's DRAM.
- **The DT hook** (`dtb_patch_cb` in a handover's configuration) is the only way a board changes
  the next stage's input. It runs in prepare, on a verified image, with the whole tree, before pb
  writes its own properties: pb's values win (`boot.md` 5.5). Prepare is part of a candidate's
  attempt (`boot.md` 2), so the hook may run once per candidate in a boot: it changes only the
  tree it is given. It can read the verified header (5.3).
- `bootargs` and `bootargs_verbose` are the kernel command line that `board_patch_bootargs()`
  sets, when the board passes it as the DT hook; `bootargs_verbose` is used with
  `BOOT_FLAG_VERBOSE`.
- `resolve_part_name` goes: the booted system's name is the policy's (`boot.md` 5.6).
- The next stage's state is the arch's (D-028), what pb passes it is the handover's.
- The parts are named by the image format's ids (`boot.md` 4.4): for bpak `bpak_id()` of the
  part's name. They are in the handover's configuration, which is the handover's to change when
  a second format comes, not `struct board`'s.

Set up last, and also after a failed step (2.1).

### 2.8 Data : mmap [optional]

**Not yet in the code (F-269).** Added after the review (Jonas, D-054), and reviewed with
`platform.md`.

The memory the board reaches and the platform didn't map: a device on an external bus (a chip
select, an FPGA), a memory mapped flash, a SoC peripheral outside the buses the platform maps.
It is a list of regions, read once by `pb_mmu_init()` in `plat_init()`, so before any other
member of the board's but `console_init()`. The rules and an example are `platform.md` 5.2.

| Region                         | Mapped as                                                       |
| ------------------------------ | --------------------------------------------------------------- |
| `PB_MAP_DEVICE(base, size)`    | Device memory, read and write                                   |
| `PB_MAP_MEMORY_RO(base, size)` | Normal memory, read only: a flash that is booted in place (2.4) |
| `PB_MAP_MEMORY_RW(base, size)` | Normal memory, read and write: RAM on an external bus           |
| `PB_MAP_END`                   | Ends the list                                                   |

- Never executable, flat, whole pages, and outside what the core and the platform map: pb
  itself, the board's DRAM, the platform's regions.
- A region gives access, nothing else: its clock, power and pads are set up in `init()`.
- The platform's default for the number of regions and tables has room for a few. A board with
  more raises `MAX_MMAP_REGIONS` and `MAX_XLAT_TABLES` in its defconfig.

Valid from: `init()`. `console_init()` runs before the MMU is on and reaches everything.

Failure: a region that breaks a rule or doesn't fit fails `plat_init()`, and `main()` resets
(`platform.md` 2.2). It is the board's data that is wrong, so it shows on the first boot, on
the console.

NULL: the board reaches what the platform maps.

### 2.9 Data : image_regions [optional]

Added after the review (Jonas, D-055).

The image data regions: the only memory an image's parts are loaded into (`boot.md` 4.3). The
board names them, because where a product's images land is the product's: its DRAM, and the SoC
memory a part of its is linked for.

```c
static const struct boot_image_region board_image_regions[] = {
    BOOT_IMAGE_REGION(0x40000000, 0x80000000),                /* DRAM */
    BOOT_IMAGE_REGION(IMX8M_BL31_BASE, IMX8M_BL31_SIZE),      /* BL31's on-chip RAM */
    BOOT_IMAGE_REGION_END,
};
```

- A part lies inside one region, with the room a handover grows it by, and over no other part.
  Nothing else is checked for a part.
- A region holds nothing of pb's: not `pb`, not `buffers`, not the Firmware Handoff region
  (2.7), not `bl_params`' region where the platform has one (`platform.md` 5.3). Where pb runs
  in DRAM, the region starts above it.
- A region is memory pb can write: the board's DRAM, SoC memory the platform header names for
  it (`platform.md` 5.3), or a `PB_MAP_MEMORY_RW` region of the board's (2.8). The core doesn't
  check that: a load into what isn't mapped faults, and pb resets.
- There is no default. The board's whole DRAM as one would bring pb's own regions back into
  the check of every part.
- A partition that is booted in place needs no region: its mapped range is its allowed memory
  (2.4, `boot.md` 4.1).
- Fixed once a product ships (section 8).

Failure: the list is checked with the board's data (section 3), before any image is read. A
list that fails is a failed step (2.1): no boot, and pb stops.

NULL: nothing is loaded. Only a partition that is booted in place boots.

## 3. The board's data, checked

**Not yet in the code (F-335)**: `boot_policy` and `boot_handover` are checked when their step
comes, after the partition table, and not at the start with `rot` and `slc`: a step before them
that fails hides the check.

`board_setup()` and `cm_board_init()` check the board's data before they use it, a few lines tested
on the host:

| Field                          | Rule                                                                               |
| ------------------------------ | ---------------------------------------------------------------------------------- |
| `name`                         | Mandatory, 1 to 16 characters: the wire's `board_id`                               |
| `rot`, `slc`                   | Mandatory. Until F-247 moves them to the platform (`plat_rot`, `plat_slc`). A      |
|                                | board without a life cycle has an `slc` without `read_status`: its command mode is |
|                                | open (`system.md` 4)                                                               |
| `boot_policy`, `boot_handover` | Mandatory                                                                          |
| `ptbl`                         | A GPT has at least one layout                                                      |
| `password_auth`                | Mandatory with `CONFIG_CM_AUTH_PASSWORD`                                           |
| `image_regions`                | No region is empty or wraps, and each is inside what the arch can address. None    |
|                                | overlaps `pb`, `buffers`, the Firmware Handoff region or `bl_params`' region (2.9) |

`mmap` isn't in the table: `pb_mmu_init()` checks it earlier, in `plat_init()` (2.8).

What an object's own configuration must hold (a chain with at least one partition, a handover's
entry part) is checked by its module when it is set up. That an object's module is compiled in
needs no check: the board doesn't link without it.

A failed check in `board_setup()` is a failed step (2.1), with `-PB_ERR_PARAM`; the handover is
set up when its own data passed. One in `cm_board_init()` means no command mode: its task ends
the loop (4.5) and pb resets.

## 4. Stopped: command mode and tasks

When `board_setup()` or `boot()` fails, or a hook stops the boot, pb is stopped: the core calls
the build's tasks in turn (4.5). Command mode is one of them, in a build that has it
(`CONFIG_CM`, the default); 4.1 to 4.4 are command mode's; its commands are named commands
(`command-mode.md` 5.6).

### 4.1 Command mode's hardware (the platform's)

A board sets up nothing for command mode (Jonas, D-049). Command mode's transport is USB, the port
most SoCs' ROMs use for recovery and provisioning, and what a USB device port needs is the SoC's:
its clocks, its power, its PHY, its controller. The platform does it in `plat_cm_init()`
(`platform.md` 3.10), when command mode's task first runs, off the boot path (D-023). Which of the
SoC's ports is a Kconfig choice of the platform's, in the board's defconfig.

No hook is left for the PCB: no board needs one (Jonas). One can be added when a board does (a
mux, a VBUS switch).

### 4.2 The transport (the core's)

The transport is not the board's choice (D-049). Command mode calls the transport's functions
directly, and the build links one implementation: the USB class driver, or the host platform's
socket for the tests. The core disconnects it before a jump. pb gets no network transport: its
stack would be in the trusted computing base (`security.md` 1, `system.md` 1). What a transport
and its USB device controller must do is `usb.md`.

### 4.3 Function : status() [optional]

```
Argument : uint8_t *buf, size_t *length
Return   : int, PB_OK or -PB_ERR_*
```

Answers `board status`, which needs authentication on a locked device (`security.md` 4.7). In:
`*length` is the size of `buf`. Out: text in `buf`, `*length` bytes. It shows what `init()` logged
and didn't fail on (2.3), and never a secret (4.4).

The board writes at most `*length - 1` bytes of text; the core clamps the length and appends the
NUL.

NULL: "Board status OK!".

### 4.4 Function : password_auth() [mandatory with CONFIG_CM_AUTH_PASSWORD]

```
Argument : const char *password, size_t length
Return   : int, PB_OK or -PB_ERR_AUTHENTICATION_FAILED
```

The password comes from the host, `length` bytes, not NUL terminated. The board compares the whole
password (the lengths match too) in constant time, never logs it (`security.md` 4.10), and keeps
the secret device unique and out of the image (an RPMB, a fuse). It is the one secret pb holds
(`security.md` 2), and what it is and where it is kept is the product's (`system.md` 6).

Token authentication (a signature over the device's UUID) is the core's.

### 4.5 Tasks [optional]

What a board runs while pb is stopped is a task (Jonas, D-051):

```
DEFINE_TASK(name, run)

run : int (*)(void), PB_OK or -PB_ERR_*
```

`DEFINE_TASK()` puts a task, its name and its function, into a list the linker builds. Anything
that is linked can define one: a board, a platform, a driver. When pb is stopped, the core
calls every task in the list, one after the other, again and again.

The loop is the core's, in `main()`, and not command mode's: pb can be built without command
mode, and such a build runs its tasks the same way. Command mode's handling of the host is one
task (`cm_main_task`), in a build that has it. What a board may rely on:

- **No switching, no pre-emption.** A task runs until it returns, and nothing else runs
  meanwhile.
- **No order.** Which task runs first is not defined, and a task doesn't depend on another
  having run.
- **Never on the boot path** (D-023): tasks run only when pb is stopped.
- **No set-up function.** A task that needs one does it on its first call.
- **Return**: PB_OK to be called again. Any error ends the loop, and pb resets (`system.md` 2):
  command mode's task ends it that way (`device reset`, no host in time).
- **No task in the build**: pb resets at once. A build whose tasks never end the loop stays
  stopped, with the watchdog kicked: that is the board's choice.

What a task does:

- It returns quickly and never waits for hardware: the other tasks, the host among them, wait
  for it.
- It takes no input from outside the device. Only command mode's task reads the transport,
  behind its gate (`security.md` 4.7).
- It stays inside the core's rules, like a named command (`command-mode.md` 5.6, `security.md` 4.8): it starts
  no image, writes no fuse, and changes neither the keys nor the life cycle. It runs on a locked
  device before any authentication.
- It doesn't kick the watchdog: the core does, once per round.
- It has no deadline it can count on. A command runs to its end inside command mode's task, and
  a long one (a large write, a partition's hash) holds every other task off for as long as it
  takes. Fine for now (Jonas): nothing a board does there is time critical.

Without a task: nothing of the board's runs while pb is stopped.

## 5. The boot

Where the hooks sit in the boot flow: `boot.md` 2. Both run once in a boot, outside the loop over
the candidates: `early_boot()` before the first candidate, `late_boot()` after the one that
verified. No hook runs between the header's check and the payload's hash; inside the loop the
board runs only in its DT hook (2.7), on a verified image (`security.md` 4.2).

The boot path is where boot time goes: the hooks start what is slow and don't wait for it
(`principles.md` 4).

### 5.1 The serial download stop (the core's)

**Not yet in the code (F-251)**: the 8M's `plat_get_boot_source()` reports no serial download,
and the RT's only the one its boot mode pins select: the stop doesn't trigger for the others.

With `CONFIG_CM_ON_SERIAL_DOWNLOAD` (default y): when `plat_get_boot_source()` reports `USB` or
`UART`, the ROM's serial downloader loaded pb, and command mode didn't start the boot, `boot()`
stops before anything else and pb enters command mode. It is a register read, so it costs no boot
time. A board doesn't check for a serial download itself.

### 5.2 Function : early_boot() [optional]

```
Argument : void
Return   : int, PB_OK, -PB_ERR_ABORT or -PB_ERR_*
```

Called by `boot()` after the boot's preconditions and before the first candidate, once in every
boot, also one command mode started (`BOOT_FLAG_CMD` in `boot_get_flags()`). A boot the
preconditions refuse doesn't reach it, so it starts no hardware then (`boot.md` 2). Valid from:
everything `board_setup()` set up.

- It starts what is slow: a hardware check that needs time, whose result `late_boot()` reads, so
  that the load runs while it settles. It doesn't wait for hardware (D-023).
- It sets up what the next stage needs from the board.
- It may stop the boot: a check whose answer is there now (a button, a request register).

Return:

- PB_OK: go on.
- `-PB_ERR_ABORT`: stop, enter command mode. Not a failure.
- Any other error: a failure. The boot stops the same way, logged as an error.

A stop or a failure is about this boot, not about an image: no candidate is tried (`boot.md` 2).

`BOOT_FLAG_CMD`: a boot command mode started is one the host asked for. A check that stops boots
for command mode's sake skips itself then; one that must stop even such a boot (the 8X's button)
doesn't. The board decides, the core passes the flag.

NULL: nothing.

### 5.3 Function : late_boot() [optional]

```
Argument : uuid_t boot_part_uu
Return   : int, PB_OK, -PB_ERR_ABORT or -PB_ERR_*
```

Called by `boot()` after the loop, when a candidate is loaded, verified and prepared, before
`plat_lockdown()` and the jump: the board's last call, and its last chance to stop.
`boot_part_uu` is the candidate's partition; in a `boot bpak` it is the UUID the host named, all
zero when it named none (`boot.md` 5.6).

The hook gets no header (Jonas, D-047): the flow is written for any image format (D-044), and a
hook's arguments change only in a major release. A board that needs the verified header asks
the format for it:

```
const struct bpak_header *boot_image_get_bpak_header(void)
```

- It returns the header of the image this boot verified, and NULL before the payload's hash is
  checked or when the image isn't a bpak image. Nothing unverified is handed out
  (`security.md` 4.2).
- Valid from prepare to the jump: in the DT hook (2.7) and in `late_boot()`.
- Another format brings its own function (`boot_image_get_<format>_header()`), compiled
  in with the format. Neither `late_boot()` nor `struct board` changes for it.

- It reads the result of what `early_boot()` started or what has run since reset. The example is
  the RT's charger detection: the hardware needs more than 100 ms, so pb loads the image while it
  runs and checks for a charger here.
- After its checks, it stops what the board started that must not run in the next stage (DMA, a
  controller), or the board's documentation says what it leaves running (`platform.md` 4.5:
  whoever started it stops it).
- It changes nothing that was verified. The board changes the next stage's input only through
  its DT hook (2.7).
- A stop here comes after the policy wrote its state (`boot.md` 3.2): on an A/B trial it costs an
  attempt, and a trial that is stopped on every boot rolls back without having run. A check whose
  answer is there before the load belongs in `early_boot()`.

Return and `BOOT_FLAG_CMD` as in 5.2. No other candidate is tried after a stop or a failure.

NULL: nothing.

### 5.4 A stop in a boot that command mode started

`boot part` answers the host after prepare. `boot bpak` (from RAM) answers after each step of the
load, the last time after the payload's hash and before prepare (`boot.md` 6). Both then
disconnect the transport and call `late_boot()`, `plat_lockdown()` and the jump. The host's last
OK means "loaded and verified", not "booted".

- A stop or failure before the disconnect (`early_boot()`, the load, prepare): command mode goes
  on. The host gets the error, except for `boot bpak`'s prepare, which runs after its last
  answer: that failure reaches the log.
- A stop or failure after it (`late_boot()`, `plat_lockdown()`): pb resets.

## 6. What a board may use and touch

- It includes `include/pb/`, `include/drivers/`, `include/boot/` and `include/plat/<p>/`, nothing
  from `src/plat/`. Not the arch's headers, and a system controller's API only through the
  platform header, never by its own path (D-056). What the
  platform header documents for boards is `platform.md` 7. A board
  that asks for the verified header (5.3) also includes the format's header, `<bpak/bpak.h>`.
- It sets up the clocks and pads of the peripherals it selects: the UART, SD/MMC, Ethernet
  (`platform.md` 7). The platform does the timer, the watchdog, the fuse controller, the TrustZone
  filter, DRAM, command mode's USB (4.1), and after F-247 the crypto engine and the RNG.
- It reads the ROM's state, the boot source and the boot reason through the platform's services
  (`plat_get_boot_source()`, `plat_get_boot_reason()`). It may show raw registers in `status()`.
- It may define tasks, which run while pb is stopped (4.5), and named commands, which a host
  runs by name (`command-mode.md` 5.6).
- A partition's handle that it keeps (`bio_get_part_by_uu()`) is good until a partition table
  is installed. After that the handle of a partition that changed is dead until the reset,
  and no hook runs again for it (`bio.md` 4). A board that needs a partition right after an
  install looks it up when it uses it.
- It may map hardware the platform didn't (`mmap`, 2.8). It never calls xlat_tables itself and
  never patches the platform's `plat_mmap[]` for its own hardware.
- The boot flags (`boot_get_flags()`, `boot_clear_set_flags()`): bits 0 to 15 are the core's
  (`BOOT_FLAG_CMD`, `BOOT_FLAG_VERBOSE`), bits 16 to 31 the board's.
- The A/B state's four board registers (`boot_ab_state_read_board_reg()`, `_write_board_reg()`)
  are the board's, and what they mean is between the board and its OS (`boot.md` 3.7). The other
  policies have no state, so no registers.
- It never writes fuses itself: the configuration fuses are data (`slc.fuses`), written by the
  core's `slc_set_configuration()` (D-027). The root of trust's state changes only through the
  core (`security.md` 2).
- It never verifies or starts an image itself (`security.md` 4.1), and never marks a partition
  bootable (2.5).

## 7. The build interface

A board directory holds:

| File          | What                                                                                    |
| ------------- | --------------------------------------------------------------------------------------- |
| `board.c`     | `const struct board board`, the hooks                                                   |
| `makefile.mk` | `src-y` for its sources, image stage overrides (`platform.md` 6.2), included before the |
|               | platform's makefile                                                                     |
| `Kconfig`     | Optional, the board's options (`osource "$(BOARD)/Kconfig"` in `src/Kconfig`)           |
| `include/`    | Optional, on the include path                                                           |

The defconfig selects the arch, the platform and the keys: `configs/<b>_defconfig` in the tree,
any file out of it, passed as `KCONFIG_CONFIG` (`build-and-test.md`). The build directory is
`build-<last part of BOARD>`.

The defconfig compiles in what the board's objects name, and holds the product's choices that
aren't the board's data (`system.md` 6):

| What                        | Options                                                                        |
| --------------------------- | ------------------------------------------------------------------------------ |
| The modules the board names | `BOOT_POLICY_FIXED`, `_CHAIN` or `_AB`; `BOOT_LINUX` or `BOOT_FH`; `BOOT_ATF`; |
|                             | the partition table's driver                                                   |
| Speculative mode            | `BOOT_AB_SPECULATIVE` (`boot.md` 3.6)                                          |
| Addresses the build needs   | `BOOT_FH_REGION_BASE` and `_SIZE` (`boot.md` 5.4); the board's DRAM and entry  |
|                             | (`platform.md` 5.1)                                                            |
| Command mode                | The USB port (4.1); `CM_ON_SERIAL_DOWNLOAD` (5.1); the authentication methods, |
|                             | `CM_AUTH_TOKEN` and `CM_AUTH_PASSWORD`                                         |
| The keys and the life cycle | `KEYSTORE_SLOTS` (D-027); EOL, off by default (`system.md` 4)                  |

`BOOT_ATF` is the delivery alone: a board that goes through TF-A also has its handover's option
(`BOOT_LINUX`), as it names both objects. Before 3.0.0 `BOOT_ATF` was a whole boot driver and
selected the device tree code: a board that is ported sets the handover's option with it, or it
doesn't link.

**Not yet in the code (F-239)**: the board's addresses (`PB_ENTRY`, `BOARD_RAM_BASE`, `_END`) are
make variables and `-D` flags in `makefile.mk`; `platform.md` 5.1 makes them Kconfig values
(`CONFIG_PB_ENTRY`, `CONFIG_BOARD_DRAM_*`). **Not yet in the code (F-240)**: the boards hold the
SoC's image recipes; `platform.md` 6.2 moves them to the platform.

## 8. Fixed once a product ships

A board's product in the field relies on these like on the wire (D-008, `system.md` 5.1). A board
changes them only for a new product, never in an update:

- `name`: the wire's `board_id`, readable without authentication. Hosts and factory scripts select
  devices by it.
- The partition UUIDs, the GPT tables' names and variants. The OS, its update tool and `pbstate`
  find them by UUID.
- The boot source policy and its data: the A/B state partitions, System A and B, a chain's
  order, and what the state's board registers mean (2.5, section 6). The state's format is
  `boot.md` 7's.
- The configuration fuses (`slc.fuses`) and the keystore's slots (`CONFIG_KEYSTORE_SLOTS`, D-027):
  fuses can't be unwritten.
- The names of the board's commands, so their ids (`pb_id()` of each), and their answers
  (`command-mode.md` 5.6). A command a released board answered through `command()`, removed in
  3.0.0, keeps its id as a named command. The
  core's default answer is fixed too (F-218: it changed the 8X boards' answer from OK to
  NOT_SUPPORTED).
- The handover, its delivery and the parts the board names for them (2.7): an image that a
  released pb booted gets the same entry and arguments (`boot.md` 7). With Firmware Handoff also
  the transfer list's region (`boot.md` 5.4): the images' memory maps keep it free.
- The image data regions (2.9): an image that a released pb loaded still fits. A region is
  made larger or added, never made smaller or removed.
- The password, where a product uses one: what the secret is derived from (4.4). An
  authentication method is added, never changed (`security.md` 4.7).

## 9. Review checklist

For a new board, and for a change to one:

- [ ] Only what the PCB and the product decide: no SoC facts, no rules, no platform registers
      outside `status()`.
- [ ] The data passes section 3, and every object's module is in the defconfig.
- [ ] The image data regions are what the product's images need and no more, each is memory pb
      can write, and none holds anything of pb's (2.9).
- [ ] `console_init()` uses nothing `plat_init()` sets up.
- [ ] What the board reaches outside the platform's map is in `mmap`, with the least kind that
      works (read only where nothing is written), and the defconfig has room for it (2.8).
- [ ] `init()` and `late_init()` fail when the boot can't go on or its security rests on what
      failed; the rest is logged, in `status()` and reported with `board_report_degraded()` (2.3).
- [ ] Nothing of command mode's USB is set up by the board; its port is in the defconfig.
- [ ] The boot hooks don't wait for hardware: a slow check starts in `early_boot()` (or runs from
      reset) and is read in `late_boot()`. A check whose answer is there before the load is in
      `early_boot()`.
- [ ] Each stop check decides for `BOOT_FLAG_CMD`; no check for a serial download (5.1).
- [ ] `late_boot()` changes nothing that was verified, and stops what must not run in the next
      stage.
- [ ] The DT hook changes only the tree it is given, and may run once per candidate.
- [ ] No image verified or started by the board, no `BIO_FLAG_BOOTABLE` set, no fuse written.
- [ ] A named command checks what it reads in the request, stays inside the core's rules, and
      kicks the watchdog in long commands (`command-mode.md` 5.6); `status()` writes at most
      `*length - 1` bytes and no secret.
- [ ] `password_auth()`: the whole password, constant time, never logged, a device unique secret.
- [ ] A task returns quickly, takes no input from the host, stays inside the core's rules and
      relies on no order (4.5).
- [ ] Section 8 unchanged for a product in the field.
- [ ] A board that runs Linux has its test bench defconfig and both lists of
      `testbench/README.md`, "Adding a board", are done.
- [ ] The conformance table is updated, and the markers of what was implemented are removed.

## 10. Boards

Where each board is (2026-11-12). The host board is the tests' (`testing.md`) and follows the
contract too, so that the order of section 2 is tested end to end (F-217).

| Requirement                    | jiffy | 8MP EVK | MEK | DXL EVK | RT1060 EVK | imx8mevk | host    | Finding |
| ------------------------------ | ----- | ------- | --- | ------- | ---------- | -------- | ------- | ------- |
| Data (3)                       | yes   | yes     | yes | yes     | yes        | no       | yes     | F-300   |
| A policy (2.5)                 | yes   | yes     | yes | yes     | yes        | no       | yes     | F-300   |
| Only the headers of section 6  | yes   | yes     | yes | yes     | yes        | yes      | yes     |         |
| No `BIO_FLAG_BOOTABLE` (2.5)   | yes   | yes     | yes | yes     | yes        | yes      | no      | F-301   |
| No serial download check (5.1) | yes   | yes     | yes | yes     | yes        | yes      | yes     | F-251   |
| `status()` (4.3)               | yes   | default | yes | yes     | default    | default  | default |         |
| `password_auth()` (4.4)        | n/a   | n/a     | yes | yes     | n/a        | n/a      | n/a     |         |
| The order of section 2         | yes   | yes     | yes | yes     | yes        | yes      | no      | F-217   |
| Test bench defconfig           | yes   | yes     | yes | no      | n/a        | no       | n/a     | F-033   |

Data: imx8mevk has neither a policy nor a handover (F-300), so the policy's check fails its
`board_setup()` with `-PB_ERR_PARAM`. The RoT, the SLC, `init()` and the table are set up before
it, and the board stops in command mode with what it had there before the check. jiffy, the RT
and the host have no `password_auth()`, and their builds have no password method.

A policy: every board with one uses A/B; imx8mevk has none (F-300). The host's is an object
its board file builds by hand, not one made by a module's macro: A/B, whose init also marks
the flash's two bootable partitions (`testing.md`, F-301). They are candidates the policy
never hands out, so only `boot part` boots them. The marks are in the policy's init, which
`board_setup()` doesn't reach when the table fails: on a blank disk the flash's partitions
aren't bootable until a table is installed and pb restarts.
