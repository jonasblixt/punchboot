# The platform contract

**State: reviewed (Jonas, 2026-11-12), D-030, D-053 to D-056.** Level 3 (`process.md`). Reviewed
once before (2026-11-10), then passed against `boot.md` and `board.md`, and reviewed again.

What a platform (`src/plat/<p>/`, `include/plat/<p>/`) provides, when, and what the rest of pb may
rely on. Decided in D-030, written in the style of TF-A's porting guide. It is independent of any
SoC vendor: how the i.MX ports meet it, with the SoC facts and their sources, is in `plat-imx.md`.

Changing or adding a platform: read this, then go through the review checklist (section 8).
`compatibility.md` applies too: the device identity (3.7) is as fixed as the wire.

Entries are tagged:

- `[mandatory]`: every platform implements it.
- `[optional]`: a platform may leave it out; the entry says what it returns then.
- `[mandatory when X]`: mandatory if the SoC has X.

Each entry says what it does, from when it may be called ("valid from") and what a failure means.

**Not yet in the code (F-nnn)** marks what the code doesn't do yet. The commit that fixes the
finding removes the marker here and updates the port's conformance table (`plat-imx.md`).

**No open items.** What the review decided and added, as written into the text:

| Item                             | Rule                                                            | By           |
| -------------------------------- | --------------------------------------------------------------- | ------------ |
| Hardware the platform didn't map | The board's `mmap`, data that `pb_mmu_init()` maps (5.2)        | Jonas, D-054 |
| Memory for images                | The board's image data regions; the platform declares none, its | Jonas, D-055 |
|                                  | header names the SoC memory a board may list and `bl_params`'   |              |
|                                  | region (5.3)                                                    |              |
| What a board may include         | The four folders of section 7. The arch through                 | Jonas, D-056 |
|                                  | `include/pb/arch.h`; a system controller's API through the      |              |
|                                  | platform header, not by its own path                            |              |
| The filter's text (4.3)          | No fact of one port's in the rule                               | Jonas        |

The pass (D-053) closed five of the seven items of 2026-11-11, because the documents above and
beside this one leave one answer:

| Item                                 | Rule                                           | By                |
| ------------------------------------ | ---------------------------------------------- | ----------------- |
| A bypassed DRAM filter (2.3)         | Refuses every boot, in every life cycle state  | `security.md` 4.5 |
| A failed `plat_lockdown()` (4.4)     | pb stops; a boot command mode started resets   | `boot.md` 2,      |
|                                      |                                                | `board.md` 5.4    |
| Who clears the reset reason (3.4)    | Not pb: the OS reads the same register         | `system.md` 5.1   |
| `plat_get_boot_source()`'s tag (3.9) | Mandatory when the ROM has a serial downloader | `board.md` 5.1    |
| Who stops DMA before the jump (4.5)  | Whoever started it                             | `board.md` 5.3    |

## 1. Scope

| Layer    | Owns                                                        | Where                                |
| -------- | ----------------------------------------------------------- | ------------------------------------ |
| Arch     | The CPU core: entry, vectors, caches, enabling the MMU, the | `src/arch/<a>/`                      |
|          | handover state (D-028)                                      |                                      |
| Platform | What the SoC decides                                        | `src/plat/<p>/`, `include/plat/<p>/` |
| Board    | What the PCB and the product decide (D-026, `board.md`)     | `src/board/<b>/`, out of tree too    |

- **A platform is a SoC family.** A SoC variant is a sub-level of it: each variant has its own
  Kconfig symbol, which selects the family's common symbol (`PLAT_<FAMILY>_COMMON`); the variant's
  code is in `plat_<soc>.c`, and the platform header says what differs per variant.
- **SoC fact or board choice.** A fact of the SoC (fuse locations, the ROM's memory use, the DRAM
  window, where the image header sits) belongs to the platform. A choice made for the PCB or the
  product (DRAM size, `PB_ENTRY`, the boot device, the configuration fuses, the keys) belongs to
  the board.
- **Shared IP, platform data.** A driver for an IP block several SoCs have is shared, and the
  platform passes the SoC's facts as data (`tzc380`, D-027).
- **No board overrides of platform services.** A board can't replace `plat_rot`, `plat_slc` or the
  memory map. Revisit when a real case needs it (an external secure element).

## 2. Lifecycle

`main()`:

```
ramlog_init()
arch_init()
plat_early_init()       fails: reset
board.console_init()
plat_init()             fails: reset
board_setup()           fails: pb stops
boot()                  fails: pb stops       (also when a precondition fails, 2.3)
  plat_lockdown()       before the jump (4.4)
the tasks, in turn      when the boot failed or stopped (5.1); command mode is one; then reset
```

A platform failure resets instead of stopping (`system.md` 5.4): stopped, pb runs its tasks, and
command mode is one of them (`board.md` 4). Without what `plat_init()` sets up, command mode has
no console, no MMU or no DRAM for its buffers (F-202, F-235). A deterministic
failure is then a reset loop; the recovery is the ROM's serial download.

### 2.1 Function : plat_early_init() [mandatory]

```
Argument : void
Return   : int, PB_OK or -PB_ERR_*
```

Called once by `main()`, after `arch_init()`, before `board.console_init()`. Keep it short, it is
on every boot (D-023). When it returns PB_OK:

- `plat_get_us_tick()` counts, and `log_time_start()` has been called.
- The watchdog is armed, when `CONFIG_ENABLE_WATCHDOG` is set.
- Every timer the SoC arms out of reset that would reset the system or assert a reset pin is
  disabled, whatever `CONFIG_ENABLE_WATCHDOG` is (a watchdog's power-down counter).
- Every reset source brings the SoC back to the ROM in a known state (warm resets made cold where
  the SoC can).
- The ROM's state that later code overwrites is saved: the ROM's log, what its serial downloader
  left set up.
- The reset reason is read, and not cleared: the OS reads the same registers.
- What the SoC needs before a UART can be set up is done (a system controller's channel). The
  UART's clock and pads are the board's (`board.console_init()`), which may use what the platform
  header marks as valid from here (7).

It touches no DRAM the ROM didn't set up, and doesn't enable the MMU.

Failure: return an error only for what can't be recovered from; `main()` resets. A platform should
make it unable to fail.

### 2.2 Function : plat_init() [mandatory]

```
Argument : void
Return   : int, PB_OK or -PB_ERR_*
```

Called once by `main()`, after `board.console_init()`. When it returns PB_OK:

- The core clocks are set.
- Fuses can be read (`plat_get_unique_id()`, `plat_slc.fuse_read`).
- DRAM works, where the ROM didn't set it up.
- The TrustZone DRAM filter is set up (4.3), before anything else uses DRAM.
- The MMU is on, where the arch has one, with the memory map of 5.2, after DRAM works.
- The SoC's crypto engine and hardware RNG are set up when their Kconfig option is on. A board
  that needs the software implementation turns the option off in its defconfig. How they are
  handed to the next stage is 4.5. **Not yet in the code (F-247)**: the boards set them up.
- `plat_rot` and `plat_slc` can be used.
- The state of everything 2.3 checks is known (`plat_get_status()`).

Failure has two kinds:

- **What command mode needs is missing**: DRAM, the MMU, the clocks `plat_cm_init()` relies on,
  fuse access. Return the error; `main()` resets.
- **Anything else** returns PB_OK and is recorded in `plat_get_status()` (3.8). Logging it is not
  enough: what the boot depends on is checked before the boot (2.3).

What only command mode needs is set up by command mode's task, not here (D-023): 3.10.

### 2.3 No silent boot: the boot's preconditions

pb never boots silently when something the boot's security depends on failed (`security.md` 4.5).
F-096 is the example: the DRAM filter was bypassed for years, everything worked, and nothing
showed it.

`boot()` checks the platform's status (3.8) in every boot, also one command mode started, before
`early_boot()` and before anything is loaded (`boot.md` 2). The core decides, not the platform.
The conditions are sorted by what rests on them (`security.md` 4.5):

| Condition                       | Rests on it                  | When it failed                                  |
| ------------------------------- | ---------------------------- | ----------------------------------------------- |
| The DRAM filter is bypassed     | The secure world's isolation | No boot                                         |
| The crypto engine failed        | What runs next: the image's  | No boot. A fallback that verifies the same      |
|                                 | verification                 | would boot; no build has one today              |
| The watchdog didn't arm         | Availability                 | Boots, and reports `watchdog-not-armed` in pb's |
| (with `CONFIG_ENABLE_WATCHDOG`) |                              | status (`boot.md` 5.6)                          |
| The ROM's logs are missing      | Nothing of the boot's        | Logged only                                     |

- **The same rule in every life cycle state**, without a development override: a development
  board is where a bypassed filter goes unnoticed (F-096).
- **A bypassed filter refuses every boot**, also for a product with nothing in the secure world:
  pb can't tell that an image starts a secure OS, and a declaration is what a board forgets. The
  cost is a porting note for 3.0.0: a product whose setup before pb leaves the filter in bypass
  stops booting with its new pb (`security.md` 4.5).
- **"No boot"** means `boot()` fails: pb stops, and command mode, which needs authentication on a
  locked device, has the reason in its log.
- The lockdown (4.4) is not in the table: whether it can be done is known only at the jump, not
  before the load. Its failure stops the boot there.

## 3. Services

### 3.1 Function : plat_reset() [mandatory]

```
Argument : void
Return   : does not return
```

Resets the SoC. Valid from the first instruction after `arch_init()`, also before
`plat_early_init()` (the arch's exception handlers call it) and whatever `CONFIG_ENABLE_WATCHDOG`
is: a driver's base address is a constant, only arming the watchdog depends on the option.
Declared `noreturn`.

### 3.2 Function : plat_get_us_tick() [mandatory]

```
Argument : void
Return   : uint32_t, microseconds
```

A free-running microsecond counter from an arbitrary start. It wraps modulo 2^32 (71.6 minutes):
callers only use differences, `now - start`, never absolute values or an end tick (F-211). Valid
from `plat_early_init()`. The platform header states its accuracy (F-084).

Microseconds, not milliseconds: drivers wait for 1 to 100 us, and the boot time log measures each
chunk's read and hash below a millisecond.

### 3.3 Function : plat_wdog_kick() [mandatory]

```
Argument : void
Return   : void
```

Restarts the watchdog. Cheap and safe at any time, also before the watchdog is armed and without
`CONFIG_ENABLE_WATCHDOG`.

Who kicks it: the core, where work makes progress. Once per chunk in `load_and_hash`, once before
the jump (the final hash, prepare and `late_boot()` follow the last chunk without a kick
otherwise; the signature is checked on the header, before the load), and once per round of the
tasks while pb is stopped (`board.md` 4.5). Inside a command, once per chunk and while a transfer
is waited for (`command-mode.md` 4.6). A board's command that runs long kicks as it
makes progress (`command-mode.md` 5.6). Never in
`pb_delay_*()` or `pb_timeout_*()`: a driver polling forever would keep the board alive, which is
what the watchdog is there to catch.

So `CONFIG_WATCHDOG_TIMEOUT` must be longer than each of these:

- The longest time without a kick from arming to the first chunk. The platform states its share in
  its header, measured (DRAM training is the usual long one).
- One chunk on the slowest medium the board loads from. The board states it.
- From the kick before the jump to the next stage's first kick: BL31, a secure OS, the kernel up
  to its watchdog driver. Usually the longest; the product measures it.
- Command mode's longest stretch without a kick: one chunk, as above, a board's command between
  its own kicks, or an erase of the blocks one command names (`command-mode.md` 4.6).

The watchdog stays armed at the jump: the next stage takes it over.

### 3.4 Function : plat_get_boot_reason() [mandatory]

```
Argument : struct pb_boot_reason *
Return   : int, PB_OK or -PB_ERR_*
```

```c
enum pb_boot_reason_kind {
    PB_BOOT_REASON_UNKNOWN = 0,
    PB_BOOT_REASON_POR,   /* power on, brown-out */
    PB_BOOT_REASON_WDOG,  /* any watchdog, also a system controller's or the ROM's */
    PB_BOOT_REASON_SW,    /* software reset, e.g. the OS's reboot */
    PB_BOOT_REASON_DEBUG, /* JTAG/SWD */
    PB_BOOT_REASON_FAULT, /* thermal, lockup, ECC, tamper, ... */
};

struct pb_boot_reason {
    enum pb_boot_reason_kind kind; /* portable: for the core and the boards */
    uint32_t raw;                  /* the SoC's value, documented in the platform header */
    const char *name;              /* for the banner and the log */
};
```

The reason is read in `plat_early_init()`, not cleared. When several causes are set, the priority
in the platform header decides `kind`. `raw` is for board code that needs the SoC's detail. It is
not on the wire.

pb doesn't clear it. The OS reads the same register, and what an OS in the field reads doesn't
change in an update (`system.md` 5.1). A reset status register is usually sticky, so a cause
stays set over the resets that follow, until a power-on reset (F-210: the i.MX6UL's):

- `kind` means "the highest priority cause since the status was last cleared", not "the cause
  of this reset".
- The platform header says which causes survive a reset.
- Clearing is the OS's.

### 3.5 Function : plat_get_unique_id() [mandatory]

```
Argument : uint8_t *output, size_t *length (in: buffer size, out: bytes written)
Return   : int, PB_OK, -PB_ERR_BUF_TOO_SMALL or -PB_ERR_*
```

The SoC's unique id, at most 16 bytes, the same for the life of the device. Valid from
`plat_init()`: it reads fuses. Its bytes, their order and their number are part of the device
identity (3.7).

Failure: a read that fails returns its error and writes nothing. Never a partial id or one from
memory that wasn't read: the device's identity and its tokens rest on it, and `device_uuid()`
passes the error on.

### 3.6 Data : platform_ns_uuid [mandatory]

```c
const char *platform_ns_uuid; /* 16 bytes */
```

The UUID namespace of the platform. A constant: valid always.

### 3.7 The device identity

The device UUID is `uuid_gen_uuid3(platform_ns_uuid, unique id)` (`device_uuid.c`). The host sees
it, and command mode tokens are signatures over its string. Changing the namespace or the unique
id's bytes changes the identity of every device in the field and invalidates their tokens. Both are
fixed like the wire (D-008). A new platform generates its namespace once, at random.

### 3.8 Function : plat_get_status() [mandatory]

```
Argument : struct plat_status *
Return   : void
```

```c
enum plat_tz_dram_filter {
    PLAT_TZ_DRAM_NONE,        /* no TrustZone, e.g. a Cortex-M core */
    PLAT_TZ_DRAM_LATER_STAGE, /* owned by a later stage, e.g. BL31 through a system controller */
    PLAT_TZ_DRAM_ACTIVE,      /* set up by pb, see 4.3 */
    PLAT_TZ_DRAM_BYPASSED,    /* the SoC has one, and it doesn't check DRAM accesses */
};

struct plat_status {
    enum plat_tz_dram_filter tz_dram_filter;
    bool wdog_armed;          /* false also without CONFIG_ENABLE_WATCHDOG */
    bool crypto_engine;       /* the engine of 2.2 works; false when turned off */
    bool rom_log;             /* the ROM's logs were found */
};
```

What `plat_init()` recorded (2.2), for the boot's preconditions (2.3) and for command mode, which
shows it in its log. Valid from `plat_init()`. A platform without a field's feature reports it as
absent (`PLAT_TZ_DRAM_NONE`, `false`). New fields come with a row in 2.3.

### 3.9 Function : plat_get_boot_source() [mandatory when the ROM has a serial downloader]

```
Argument : struct pb_boot_source *
Return   : int, PB_OK or -PB_ERR_NOT_SUPPORTED
```

Where the ROM (or a debugger) loaded pb from: a bio partition's UUID, USB, UART, the debugger, or
unknown (with PB_OK: the platform can't tell this time). On a SoC whose ROM has no serial
downloader a platform may leave it out: -PB_ERR_NOT_SUPPORTED. Only
the platform knows how its ROM reports this; drivers only report facts about their device
(`mmc_get_boot_part_config()`).

Valid from: USB, UART and the debugger from `plat_early_init()`, which saves what the ROM left
(2.1). A partition once the board's storage is set up (`board_setup()`): naming it may need the
device.

Why mandatory: the core's serial download stop asks this function, and a board doesn't check
for a serial download itself (`board.md` 5.1). With -PB_ERR_NOT_SUPPORTED the stop never
triggers, and nothing shows it. What is mandatory is reporting USB and UART; naming the
partition and the debugger stays optional (unknown). **Not yet in the code (F-251)**: the RT
reports only the serial download its boot mode pins select.

### 3.10 Function : plat_cm_init() [mandatory when CONFIG_CM]

```
Argument : void
Return   : int, PB_OK or -PB_ERR_*
```

Called once by command mode's task, when it first runs, never on the boot path (D-023,
`board.md` 4.5). It sets up the USB device port command mode uses (D-049, `board.md` 4.1): its
clocks, its power, its PHY and its controller, so that the transport can start.

- Which port: a Kconfig choice of the platform's, one option per port of the SoC that can be a
  device. The board's defconfig makes it.
- Valid from: what `plat_init()` provides. It also runs after a failed `board_setup()`, so it
  relies on nothing of the board's.
- Failure: no command mode. Its task ends the loop and pb resets.
- The transport itself is linked, not set up here: the USB class driver on a SoC, and on the
  host platform its socket, for the tests (`board.md` 4.2).

## 4. Security

### 4.1 Data : plat_rot [mandatory]

**Not yet in the code (F-247)**: the boards set `board.rot` to the SoC's functions.

```c
extern const struct rot_config plat_rot; /* revoke_key, read_key_status */
```

The SoC's key revocation (D-027): the revoke bit of key slot n, shared with the ROM. A SoC whose
revocation pb doesn't support yet still implements `read_key_status`, without it no key can be
used; `revoke_key` returns -PB_ERR_NOT_SUPPORTED.

### 4.2 Data : plat_slc [mandatory]

**Not yet in the code (F-247)**: `struct slc_config` is split into the SoC's operations and the
board's data.

```c
extern const struct slc_ops plat_slc; /* read_status, set_configuration(_locked), set_eol,
                                         fuse_read, fuse_write */
```

The board keeps only its configuration fuses (`board.slc_fuses`, `board.slc_fuses_length`; today
`fuses` in the board's `struct slc_config`, which `board.md` calls `slc.fuses`), by the platform's
fuse index macro (`<P>_FUSE()`). `board_setup()` passes both to `slc_init()`. The
rules (the order of the states, D-011, F-166) are the core's, in `slc.c`; the platform only reads
and writes fuses.

`set_eol`: every port implements EOL, as the SoC's own end of life state, or its document says
why the SoC can't (`system.md` 4). A product chooses it with a Kconfig option, off by default.
**Not yet in the code (F-263)**: only the host platform has it.

### 4.3 The TrustZone DRAM filter [mandatory when the SoC has one]

**Not yet in the code (F-261)**: the ports open the SoC's whole DRAM window, not the board's
DRAM (item 3).

This is pb's part of the secure world's isolation (`security.md` 4.9), and a boot without it is
refused (2.3).

TrustZone is architectural (Secure and Non-secure state, the NS bit on the bus), the filter that
checks DRAM accesses isn't: it is SoC IP (an Arm TZC-380 or TZC-400, a vendor's own controller, or
a system controller's partitions). In Arm's model the stage in BL2's place sets it up (TF-A porting
guide, `bl2_platform_setup()`: "configuration of the TrustZone controller to allow non-secure
masters access to most of DRAM"). pb is that stage.

A platform whose SoC has a filter:

1. Takes it out of bypass before DRAM is used, or checks that the stage before pb did.
2. Locks the enable where the SoC can, so that the normal world can't switch it off.
3. Opens the board's DRAM, not the whole address space, to both worlds in a region of its own and
   leaves the default region secure only: where DRAM appears more than once in the address space
   (aliases), the copies stay protected. No lockdown: the secure OS replaces the region with its
   own and locks.
4. Passes the SoC's facts to the shared driver as data: the base, how it counts addresses (from 0
   or from the DRAM's base), the aliases, quirks.
5. Reports the state (`plat_get_status()`).

A platform without one says why in its header: no TrustZone, or a later stage owns it.

Cite the document and section of a fact, never "the TRM": Arm's TRMs describe IP blocks (TZC-380:
DDI0431), a SoC vendor's reference manual the SoC.

### 4.4 Function : plat_lockdown() [mandatory]

```
Argument : void
Return   : int, PB_OK or -PB_ERR_*
```

Called by the core right before the jump, after the last moment command mode could use what it
locks (`security.md` 4.6). Locks, until the next reset, what only pb may change: key revocation
(pb revokes keys in command mode, so the ROM may have been told to leave it unlocked), the fuses
that reopen a closed device (field return, debug unlock), and the controller settings the SoC
can lock for the boot's lifetime. A platform with nothing to lock returns PB_OK and says so in
its header.

It may be called more than once before a reset (a boot that stopped after it, then a boot from
command mode): locking what is already locked is not an error.

Failure: the boot doesn't jump. It is not one of 2.3's preconditions, it is known only here.

What follows a failure is the flow's (`boot.md` 2, `board.md` 5.4): pb stops, as after any
failure outside the loop over the candidates, and a boot command mode started resets, because
its transport is disconnected by then. Some of the locks may be set, and they hold until the
next reset: command mode can't revoke a key, or do what else they cover, until then, and passes
on the driver's error for what a lock refuses. A lockdown that always fails therefore ends in
command mode, not in a reset loop.

### 4.5 Other duties of the stage in BL2's place

What a SoC needs from its first mutable stage before the next one runs. The platform does what the
next stage can't; what a later stage owns (BL31, the secure OS) is left to it, in the state it
expects. `plat-imx.md` lists, per port, who does each and where pb stands.

- **Bus and peripheral access control** (peripheral and bus master security attributes): put into
  the state the reference manual requires before use; the secure world's owner restricts and locks
  it. Without a secure OS the default stands.
- **The hardware RNG**: instantiated, when the next stage runs in the normal world and can't.
- **The crypto engine**: handed over as the next stage expects (its job rings, their owner).
- **The architected timer**: counting, with its frequency in `CNTFRQ` (Linux's arm64 boot
  protocol requires it; only EL3 can write it).
- **Timers armed out of reset and reset routing**: 2.1.
- **DMA stopped before the jump** ("Quiesce all DMA capable devices", Linux's booting.rst).
  Whoever started it stops it (`board.md` 5.3). The core: command mode's transport, which stops
  the USB controller that `plat_cm_init()` started, and the device it loaded from. The board:
  what its hooks started, in `late_boot()`. The platform: what `plat_init()` started. How a
  driver is stopped is `drivers.md` 2.3.

## 5. Memory

### 5.1 The link interface

Two different memories: the one pb runs in (`pb` region, usually on-chip RAM, or a part of DRAM
the ROM set up) and the board's DRAM (where images are loaded). Their names say which.

| Value                                 | From                  | What                   | Used by                  |
| ------------------------------------- | --------------------- | ---------------------- | ------------------------ |
| `CONFIG_PB_ENTRY`                     | Board defconfig       | Where pb is loaded and | Linker, image stages     |
|                                       |                       | linked                 |                          |
| `CONFIG_BOARD_DRAM_BASE`, `_SIZE`     | Board defconfig, with | The board's DRAM       | MMU layer 1, linker      |
|                                       | DRAM                  |                        | checks                   |
| `CONFIG_PB_BUFFERS_BASE`, `_SIZE`     | Board, default from   | The `buffers` region   | Linker                   |
|                                       | the platform          |                        |                          |
| `CONFIG_BOOT_FH_REGION_BASE`, `_SIZE` | Board defconfig, with | The transfer list's    | Placement, linker checks |
|                                       | Firmware Handoff      | region (`boot.md` 5.4) |                          |
| The ROM's load windows                | Platform              | Where the ROM may load | Linker checks, `pb` size |
|                                       |                       | pb                     |                          |
| `PLAT_*_ADDR_SPACE_SIZE`              | Platform              | The translation tables | xlat_tables              |
| `MAX_XLAT_TABLES`, `MAX_MMAP_REGIONS` | Board, default from   | Their number, with the | xlat_tables              |
|                                       | the platform          | board's regions (5.2)  |                          |
| `PB_SECTION_ALIGNMENT`                | Arch                  | Section alignment      | Linker                   |

Each value has one source and reaches both the linker (`--defsym`) and C (`-D`).

- The `pb` region runs from `PB_ENTRY` to the end of the ROM's load window it is in (or to the
  start of `buffers`): its length is derived, not set.
- The platform's `link.lds` defines `MEMORY` from these values, with no addresses of its own except
  the SoC's regions (a DRAM controller's save area, a reset stub). It `ASSERT`s that `pb` is in a
  load window, that `buffers` is in memory that works after `plat_init()` (the board's DRAM or the
  SoC's on-chip RAM), and that they don't overlap. The Firmware Handoff region is checked the
  same way, and against both.
- `src/link.lds` places pb's sections: code, data, rodata, stack, bss and the RAM log in `pb`,
  `.no_init` and the translation tables in `buffers`. It provides `_pb_start/_end` and
  `_buffers_start/_end`.
- The link script keeps the task list (`KEEP`): the tasks that `DEFINE_TASK()` defines (D-051,
  `board.md` 4.5), which the core calls while pb is stopped, sit in a section of their own in
  rodata, which no code names, so the linker would drop it. It provides the list's start and end. A
  platform may define tasks too, by the same rules.
- It keeps the list of named commands the same way (`pb_commands`, `command-mode.md` 5.6,
  D-076). A platform declares its own commands there, for what only its SoC has (the 8M's DRAM
  training data): a command of the platform's stays inside the core's rules
  (`security.md` 4.8), and its name, id and bytes are fixed like a board's (`board.md` 8) for
  every board of the platform.

The RAM log is in `pb`, which works from the first instruction. `buffers` may be in DRAM the
platform trains, so nothing before `plat_init()` uses `.no_init`.

### 5.2 The memory map

**Not yet in the code (F-158)**: the ports copy the same MMU setup.

`pb_mmu_init(plat_mmap)` (`src/mmu.c`) maps four layers. xlat_tables_v2 accepts fully nested
flat regions, the innermost wins; partial overlaps are refused.

1. **The board's DRAM**: `[BOARD_DRAM_BASE, +SIZE)`, RW, execute never.
2. **The platform's `plat_mmap[]`**: devices, the ROM (its APIs), on-chip RAM, SoC regions. A
   region layer 1 already covers stays named when the SoC uses it for something particular (the
   next stage's load address over a reset stub). The SoC's peripheral buses are mapped whole,
   not only the peripherals the platform has a driver for: a board uses others.
3. **The board's `mmap`**: what the board reaches and the platform didn't map (below).
4. **pb itself**: `.text` RX, `.rodata` RO execute never, the rest of `pb` RW execute never,
   `buffers` RW execute never. `pb` and `buffers` are two regions: one span over the gap between
   them maps the address space in between as memory (F-158).

The platform calls it in `plat_init()` once DRAM works. It builds the tables and ends in the
arch's enable (`enable_mmu_svc_mon()` on ARMv7-A, `enable_mmu_el3()` on ARMv8-A): the platform
doesn't call that itself. ARMv7-M has an MPU and no `plat_mmap`. Layer 1 also maps DRAM pb never
touches, such as a secure carve-out: pb runs in the Secure state, and the filter (4.3), not pb's
MMU, protects it from the normal world.

**The board's regions** `[optional]`. **Not yet in the code (F-269).** A board will want hardware
the platform didn't map (Jonas, D-054): a device on an external bus (a chip select, an FPGA), a
memory mapped flash, a SoC peripheral outside the buses of layer 2. The board declares it as
data, `mmap` in `struct board` (`board.md` 2.8), and `pb_mmu_init()` maps it with the rest. The
platform does nothing for it, and the board needs no patch to the platform:

```c
static const struct pb_mmap_region board_mmap[] = {
    PB_MAP_DEVICE(0x08000000, 0x00010000),    /* The FPGA on chip select 0 */
    PB_MAP_MEMORY_RO(0x60000000, 0x01000000), /* The NOR flash's window */
    PB_MAP_END,
};

const struct board board = {
    .mmap = board_mmap,
};
```

- **Why data, not a call.** The tables are built once, before the MMU is on, and before any of
  the board's code that needs them. A call that maps at run time would put xlat_tables' dynamic
  tables into every build, on the boot path (D-023), and its failure would come late. As data,
  a region is checked with the rest of the map and is there from `board.init()` on.
- **Three kinds**, from `include/pb/mmu.h`, so a board includes nothing of xlat_tables:
  `PB_MAP_DEVICE` (device memory, RW), `PB_MAP_MEMORY_RO` and `PB_MAP_MEMORY_RW` (normal
  memory). None is executable: only pb's `.text` and the platform's ROM are.
- **Flat**, as everything in pb. The base and the size are multiples of the page size, which the
  macro checks in the build.
- **Outside what is mapped already**: `pb`, `buffers`, the board's DRAM and every region of
  `plat_mmap[]`. xlat_tables would take a nested region and let it win, so a board's region
  could change what pb or the platform mapped; `pb_mmu_init()` checks and refuses instead. A
  board that needs other attributes inside a region of the platform's has found an error in
  `plat_mmap[]`.
- **Room.** `MAX_MMAP_REGIONS` and `MAX_XLAT_TABLES` (5.1) count the board's regions too. They
  are Kconfig values with the platform's default, which a board with many regions raises in
  its defconfig. The tables are in `buffers`, whose size the linker checks.
- **Failure.** A region that overlaps, or doesn't fit, fails `pb_mmu_init()` with a log line
  that names it, and with it `plat_init()` (2.2): `main()` resets. The console is up by then
  (`board.console_init()`), so it shows on the first boot of a new board. No region is ever
  left out silently.
- **Valid from** `board.init()`. `board.console_init()` runs before the MMU is on and reaches
  everything.
- **Access only.** A region's clock, power and pads are the board's to set up (section 7).
- **ARMv7-M**: pb leaves the MPU off and everything is reachable. `mmap` is accepted and not
  read.
- NULL: nothing but layers 1, 2 and 4.

### 5.3 Memory for images and handovers

**Not yet in the code (F-294)**: `bl_params`' memory is the platform's to give
(`plat_bl_params`, `PLAT_BL_PARAMS_SIZE` from its makefile), but on the i.MX8M it is an object
in `pb`'s own data, as it was, not a SoC region of its own.

An image's parts are loaded only into the image data regions, a list the board gives
(`board.md` 2.9, `boot.md` 4.3, D-055). The platform declares no loadable memory of its own:
what a product loads outside DRAM is the product's to say, and a list of the platform's would
allow those loads on every board of the SoC.

The platform's part:

- **SoC memory a board may list.** The header names the memory outside DRAM that a part can be
  loaded into, base and size, with what it is for: the on-chip RAM a BL31 is linked for, a TCM
  for another core's image. It is in `plat_mmap[]`, read and write.
- **Where `bl_params` live** `[mandatory when the SoC's BL31 restricts it]`: TF-A's BL31 may take
  its parameters only from some memory (`boot.md` 5.3). The header then defines
  `PLAT_BL_PARAMS_BASE` and `PLAT_BL_PARAMS_SIZE`, a SoC region that is in `plat_mmap[]` and
  that the link script keeps `pb` and `buffers` off (5.1). The TF-A delivery writes them
  there. Without it: pb's own data.

The check of a part names nothing of pb's: inside one region, over no other part. What keeps a
part off pb is checked once, on the list, when the board's data is checked (`board.md` 3). No
image data region overlaps:

- `pb` or `buffers`. Everything pb links is in one of the two (5.1), so a section added later
  is covered without a change to the check.
- The Firmware Handoff region, with `CONFIG_BOOT_FH`.
- `bl_params`' region, where the platform has one.

Not the platform's to declare:

- **The Firmware Handoff region** is the board's (5.1's table).
- **A partition that is booted in place** is the board's (`board.md` 2.4): the board gives the
  address its device is mapped at. Where the arch has an MMU that range is mapped readable: in
  `plat_mmap[]` when it is a SoC region, in the board's `mmap` (5.2) when it isn't.

## 6. The build interface

### 6.1 Kconfig

- `choice PLAT` in `src/plat/Kconfig`: one symbol per platform or variant, depending on its arch
  (`ARCH_ARMV7`, `ARCH_ARMV8`, `ARCH_ARMV7M`) and the vendor's `SOC_FAMILY_*`; a family's common
  symbol selects its drivers.
- `src/plat/<p>/Kconfig`: the platform's options, among them the image stages' (6.2).

### 6.2 Image stages

**Not yet in the code (F-240)**: the boards hold the recipes.

`src/plat/<p>/makefile.mk` provides the SoC's stages, in `$(BUILD_DIR)`:

| Stage               | Target           | In, out                               | Kconfig            |
| ------------------- | ---------------- | ------------------------------------- | ------------------ |
| Package             | `<p>_image`      | `pb.bin`, the SoC's container         | `<P>_CREATE_IMAGE` |
| Signing description | `<p>_sign_desc`  | the container, the description        | `<P>_SIGN_IMAGE`   |
| Sign                | `<p>_sign_image` | the description, the signed container | `<P>_SIGN_IMAGE`   |

The platform's documentation names the files. A platform may have more outputs (a variant for the
ROM's serial downloader). The SoC's knowledge is in the stages: the container format, where the
image header sits relative to `PB_ENTRY`, the serial downloader's load address, a flash boot
header's layout. The board's data is input: the ROM's register setup (DRAM, done by the ROM), a
system controller's firmware, the boot device, the keys and certificates (Kconfig strings), a flash
configuration block.

Rules for the platform's makefile, so that a board can override:

- The board's makefile is included before the platform's: every input is set with `?=`, the
  signing tool too.
- A stage is added to `image-y` (today `plat-y`) only through its Kconfig option.
- `FINAL_OUTPUT` is set when the makefile is parsed, from the same options, not with `$(eval)`
  in a recipe.

A board, also one out of tree (`make BOARD=<path>`), overrides at three levels, without changing a
file in the tree:

1. **The tool**: its own signing tool, same description in, same output (keys in an HSM, a
   signing server).
2. **Signing**: `<P>_SIGN_IMAGE=n`, and its own target after `<p>_image` in `image-y`.
3. **Everything**: both options off, its own tooling from `pb.bin`. The platform documents what that
   tooling needs: `pb.bin` is the raw image linked at `PB_ENTRY`, its entry point, and what the ROM
   requires around it (container, image header, the ROM's register setup, boot data).

The stage names, their outputs and their input variables are an interface for out-of-tree boards:
they change only in a major release (D-018). CI builds a board that overrides at level 3, so the
interface is tested.

The Kconfig options are part of it. A renamed option drops out of an out-of-tree defconfig without
an error, and the stage is then off: F-240's commit keeps today's names where it can, and the
release notes list the ones it renames.

## 7. What a board may use

The platform's side. What the core promises a board and asks of it is `board.md`.

A board includes `include/pb/`, `include/drivers/`, `include/boot/` and `include/plat/<p>/`,
nothing from `src/plat/` (the same list as `board.md` 6).

A board that asks for the verified header also includes the image format's header,
`<bpak/bpak.h>` (`board.md` 5.3, D-047).

Nothing else (Jonas, D-056).

- **Not the arch's headers** (`arch/arch_helpers.h`, `arch/armv8a/timer.h`). What a board needs
  from the arch comes through `include/pb/arch.h`.
- **A system controller's API through the platform header.** A board that sets up its own
  clocks and power on such a SoC calls the system controller, so its API is part of what the
  platform gives a board. It lives with the platform (`include/plat/<p>/`, `src/plat/<p>/`):
  it is the SoC's, and the platform is built on it. The platform header includes the services
  a board needs, and the board includes the platform header, never the API's headers by their
  own path. An include path into the API's folder is for the platform's own sources. With
  command mode's USB in the platform (3.10) the boards' own calls get fewer.

The platform header documents:

- Base addresses, and the pin macros (`<P>_PIN()`, D-026).
- The fuse index macro, for `board.slc_fuses`.
- One accessor, `<p>_plat()`, with documented fields: the SoC revision, the speed grade, the
  clock rates boards need, a system controller's handle. Its fields are set by `plat_init()`,
  except those the header marks as valid from `plat_early_init()`: `board.console_init()` runs
  between the two (`board.md` 2.2) and may need a system controller's handle for its clock and
  pads.
- Helpers for boards (pad setup through a system controller), and which of a system
  controller's services the header gives a board.
- The SoC memory a board may list as an image data region, and `bl_params`' region (5.3).
- The tick's accuracy, the watchdog's longest stretch without a kick, the boot reason's `raw`
  values and priority, what `plat_lockdown()` locks, and for level 3 builds what the ROM requires
  around `pb.bin`.

Clocks are code (Jonas, 2026-11-09). Who sets one up: whoever selects the peripheral. The platform
does the timer, the watchdog, the fuse controller, the TrustZone filter, DRAM, the crypto engine
and the RNG, and command mode's USB (3.10); the board does the UART, SD/MMC and Ethernet.

## 8. Review checklist

For a new platform, and for a change to one:

- [ ] `plat_early_init()` starts the tick and calls `log_time_start()`, arms the watchdog, disables
      the timers armed out of reset, routes resets back to the ROM, saves the ROM's state, reads
      the reset reason without clearing it; it can't fail.
- [ ] `plat_init()` returns an error only when command mode's prerequisites are missing, and
      records the rest in `plat_get_status()`.
- [ ] `plat_early_init()` touches no DRAM the ROM didn't set up and doesn't enable the MMU.
- [ ] `plat_reset()` works before `plat_early_init()` and without `CONFIG_ENABLE_WATCHDOG`.
- [ ] `plat_wdog_kick()` is safe before the watchdog is armed. The header gives the longest
      stretch without a kick, measured.
- [ ] `plat_get_us_tick()` is in microseconds and wraps at 2^32; the header gives its accuracy.
- [ ] `plat_get_boot_reason()` maps every cause to a `kind`, with the priority in the header, and
      the header says which causes survive a reset. Nothing clears the status.
- [ ] `plat_get_boot_source()` reports a serial download, USB and UART, where the ROM has one.
- [ ] `plat_cm_init()` sets up the USB port the defconfig chose, relies on nothing of the
      board's, and nothing of it runs on the boot path.
- [ ] The device identity is unchanged: `platform_ns_uuid`, the unique id's bytes, order, length.
      A new platform has a new random namespace. `plat_get_unique_id()` returns an error when a
      read fails, never a partial id.
- [ ] The TrustZone filter (4.3): out of bypass before DRAM is used, the enable locked where
      possible, the board's DRAM open in a region of its own, the default region secure, the state
      reported; or the header says why there is none.
- [ ] `plat_lockdown()` locks key revocation and what reopens a closed device, or the header says
      why there is nothing. It can be called twice.
- [ ] The duties of 4.5 are done, or left to a named later stage. What `plat_init()` started
      and must not run in the next stage is stopped.
- [ ] `plat_rot` and `plat_slc` only read and write fuses; the rules stay in `rot.c`, `slc.c`.
      EOL is the SoC's own state, or the port's document says why there is none.
- [ ] The link script has no addresses but the SoC's regions; its `ASSERT`s check the windows.
- [ ] `plat_mmap[]` holds only SoC regions, with the peripheral buses whole; `pb_mmu_init()` is
      called after DRAM works. Nothing in the platform is written for one board's hardware:
      that is the board's `mmap` (5.2).
- [ ] The header names the SoC memory a board may list as an image data region, and
      `bl_params`' region where BL31 restricts it. The platform itself allows no load (5.3).
- [ ] The image stages follow 6.2: `?=` inputs, Kconfig gated, all three override levels work.
- [ ] The header documents what section 7 lists.
- [ ] Facts from a reference manual are cited by document and section. NDA documents are not
      quoted, only named.
- [ ] The port's conformance table is updated, and the markers of what was implemented are removed.

## 9. Ports

| Port                                                     | Documented in                                        |
| -------------------------------------------------------- | ---------------------------------------------------- |
| imx6ul, imx8m (MQ, MP), imx8x (QXP, DXL), imxrt (RT10xx) | `plat-imx.md`, with each port's conformance          |
| host (test only)                                         | `testing.md`. It implements what the tests need: the |
|                                                          | tick, the unique id, the console, reset as `exit()`  |
