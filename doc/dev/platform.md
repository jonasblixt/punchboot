# The platform contract

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

## 1. Scope

| Layer | Owns | Where |
|---|---|---|
| Arch | The CPU core: entry, vectors, caches, enabling the MMU, the handover state (D-028) | `src/arch/<a>/` |
| Platform | What the SoC decides | `src/plat/<p>/`, `include/plat/<p>/` |
| Board | What the PCB and the product decide (D-026) | `src/board/<b>/`, out of tree too |

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
board_setup()           fails: command mode
boot()                  fails: command mode   (also when a precondition fails, 2.3)
  plat_lockdown()       before the jump (4.4)
cm_run()                then reset
```

A platform failure resets instead of entering command mode: without what `plat_init()` sets up,
command mode has no console, no MMU or no DRAM for its buffers (F-202, F-235). A deterministic
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
  disabled, whatever `CONFIG_ENABLE_WATCHDOG` is (a watchdog's power-down counter). **Not yet in
  the code (F-245).**
- Every reset source brings the SoC back to the ROM in a known state (warm resets made cold where
  the SoC can).
- The ROM's state that later code overwrites is saved: the ROM's log, what its serial downloader
  left set up.
- The reset reason is read, and not cleared: the OS reads the same registers.
- What the SoC needs before a UART can be set up is done. The UART's clock and pads are the
  board's (`board.console_init()`).

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
- The MMU is on, with the memory map of 5.2, after DRAM works.
- The SoC's crypto engine and hardware RNG are set up when their Kconfig option is on (4.5). A
  board that needs the software implementation turns the option off in its defconfig. **Not yet
  in the code (F-247)**: the boards set them up.
- `plat_rot` and `plat_slc` can be used.
- The state of everything 2.3 checks is known (`plat_get_status()`).

Failure has two kinds:

- **What command mode needs is missing**: DRAM, the MMU, the clocks `board.cm_init` relies on,
  fuse access. Return the error; `main()` resets.
- **Anything else** returns PB_OK and is recorded in `plat_get_status()` (3.8). Logging it is not
  enough: what the boot depends on is checked before the boot (2.3).

What only command mode needs is set up in `cm_run()`, not here (D-023).

### 2.3 No silent boot: the boot's preconditions

**Not yet in the code (F-248). The rule is decided, the policy below is a proposal (open).**

pb never boots silently when something the boot's security depends on failed. F-096 is the
example: the DRAM filter was bypassed for years, everything worked, and nothing showed it.

`boot()` checks the platform's status (3.8) before it loads anything, against what this boot
needs, and the core decides, not the platform:

| Condition | Matters when | Proposed: when it failed |
|---|---|---|
| The DRAM filter is bypassed | The image starts a secure OS (BL32) | No boot |
| The lockdown (4.4) can't be done | Always | No boot |
| The watchdog didn't arm | `CONFIG_ENABLE_WATCHDOG` | Open: A/B's rollback needs it, but |
| | | refusing means no system at all |
| The crypto engine failed | The board uses it | Open: no boot, or the software fallback |
| The ROM's logs are missing | Never | Logged only |

"No boot" means `boot()` fails: pb enters command mode, which needs authentication on a locked
device, and reports the reason in its log. Open with it:

- The same rule in every life cycle state, or a development override? Proposed: the same rule; a
  development board is where a bypassed filter goes unnoticed (F-096).
- Tell the OS: pass the status in the device tree (`/chosen`), so the OS can report or refuse too.
  Part of the OS-facing contract, later.

## 3. Services

### 3.1 Function : plat_reset() [mandatory]

```
Argument : void
Return   : does not return
```

Resets the SoC. Valid from the first instruction after `arch_init()`, also before
`plat_early_init()` (the arch's exception handlers call it) and whatever `CONFIG_ENABLE_WATCHDOG`
is: a driver's base address is a constant, only arming the watchdog depends on the option.
Declared `noreturn`. **Not yet in the code (F-234, F-237).**

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
the jump (the signature check after the last chunk isn't kicked otherwise), and in command mode's
loop (F-243). Never in `pb_delay_*()` or `pb_timeout_*()`: a driver polling forever would keep the
board alive, which is what the watchdog is there to catch.

So `CONFIG_WATCHDOG_TIMEOUT` must be longer than both of these:

- The longest time without a kick from arming to the first chunk. The platform states its share in
  its header, measured (DRAM training is the usual long one).
- One chunk on the slowest medium the board loads from. The board states it.

The watchdog stays armed at the jump: the next stage takes it over.

### 3.4 Function : plat_get_boot_reason() [mandatory]

**Not yet in the code (F-236)**: replaces `plat_boot_reason()` and `plat_boot_reason_str()`.

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

### 3.5 Function : plat_get_unique_id() [mandatory]

```
Argument : uint8_t *output, size_t *length (in: buffer size, out: bytes written)
Return   : int, PB_OK or -PB_ERR_BUF_TOO_SMALL
```

The SoC's unique id, at most 16 bytes, the same for the life of the device. Valid from
`plat_init()`: it reads fuses. Its bytes, their order and their number are part of the device
identity (3.7).

### 3.6 Data : platform_ns_uuid [mandatory]

**Not yet in the code (F-237)**: not declared in a header.

```c
const char *platform_ns_uuid; /* 16 bytes */
```

The UUID namespace of the platform.

### 3.7 The device identity

The device UUID is `uuid_gen_uuid3(platform_ns_uuid, unique id)` (`device_uuid.c`). The host sees
it, and command mode tokens are signatures over its string. Changing the namespace or the unique
id's bytes changes the identity of every device in the field and invalidates their tokens. Both are
fixed like the wire (D-008). A new platform generates its namespace once, at random.

### 3.8 Function : plat_get_status() [mandatory]

**Not yet in the code (F-248)**: today the platforms only log.

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
    bool crypto_engine;       /* the engine of 4.5 works; false when turned off */
    bool rom_log;             /* the ROM's logs were found */
};
```

What `plat_init()` recorded (2.2), for the boot's preconditions (2.3) and for command mode, which
shows it in its log. Valid from `plat_init()`. A platform without a field's feature reports it as
absent (`PLAT_TZ_DRAM_NONE`, `false`). New fields come with a row in 2.3.

### 3.9 Function : plat_get_boot_source() [optional]

```
Argument : struct pb_boot_source *
Return   : int, PB_OK or -PB_ERR_NOT_SUPPORTED
```

Where the ROM (or a debugger) loaded pb from: a bio partition's UUID, USB, UART, the debugger, or
unknown. Without it: -PB_ERR_NOT_SUPPORTED. Only the platform knows how its ROM reports this;
drivers only report facts about their device (`mmc_get_boot_part_config()`).

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

The board keeps only its configuration fuses (`board.slc_fuses`, `board.slc_fuses_length`), by
the platform's fuse index macro (`<P>_FUSE()`). `board_setup()` passes both to `slc_init()`. The
rules (the order of the states, D-011, F-166) are the core's, in `slc.c`; the platform only reads
and writes fuses.

### 4.3 The TrustZone DRAM filter [mandatory when the SoC has one]

**Not yet in the code (F-241, F-208).**

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
   (aliases), the copies stay protected. Some DMA masters are non-secure, so without that region
   pb's own USB stops. No lockdown: the secure OS replaces the region with its own and locks.
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
locks. Locks, until the next reset, what only pb may change: key revocation (pb revokes keys in
command mode, so the ROM may have been told to leave it unlocked), the fuses that reopen a closed
device (field return, debug unlock), and the controller settings the SoC can lock for the boot's
lifetime. A platform with nothing to lock returns PB_OK and says so in its header.

Failure: the boot doesn't jump (2.3).

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
- **DMA stopped before the jump** ("Quiesce all DMA capable devices", Linux's booting.rst): the
  drivers', through the boot driver. Part of the handover contract, later.

## 5. Memory

### 5.1 The link interface

**Not yet in the code (F-239)**: the board's addresses are make variables and `-D` flags today,
and link scripts repeat them as constants.

Two different memories: the one pb runs in (`pb` region, usually on-chip RAM, or a part of DRAM
the ROM set up) and the board's DRAM (where images are loaded). Their names say which.

| Value | From | What | Used by |
|---|---|---|---|
| `CONFIG_PB_ENTRY` | Board defconfig | Where pb is loaded and linked | Linker, image stages |
| `CONFIG_BOARD_DRAM_BASE`, `_SIZE` | Board defconfig, with DRAM | The board's DRAM | MMU layer 1, linker checks |
| `CONFIG_PB_BUFFERS_BASE`, `_SIZE` | Board, default from the platform | The `buffers` region | Linker |
| The ROM's load windows | Platform | Where the ROM may load pb | Linker checks, `pb` size |
| `PLAT_*_ADDR_SPACE_SIZE`, `MAX_XLAT_TABLES`, | Platform | The translation tables | xlat_tables |
| `MAX_MMAP_REGIONS` | | | |
| `PB_SECTION_ALIGNMENT` | Arch | Section alignment | Linker |

Each value has one source and reaches both the linker (`--defsym`) and C (`-D`).

- The `pb` region runs from `PB_ENTRY` to the end of the ROM's load window it is in (or to the
  start of `buffers`): its length is derived, not set.
- The platform's `link.lds` defines `MEMORY` from these values, with no addresses of its own except
  the SoC's regions (a DRAM controller's save area, a reset stub). It `ASSERT`s that `pb` is in a
  load window, that `buffers` is in memory that works after `plat_init()` (the board's DRAM or the
  SoC's on-chip RAM), and that they don't overlap.
- `src/link.lds` places pb's sections: code, data, rodata, stack, bss and the RAM log in `pb`,
  `.no_init` and the translation tables in `buffers`. It provides `_pb_start/_end` and
  `_buffers_start/_end`.

The RAM log is in `pb`, which works from the first instruction. `buffers` may be in DRAM the
platform trains, so nothing before `plat_init()` uses `.no_init`.

### 5.2 The memory map

**Not yet in the code (F-158)**: the ports copy the same MMU setup.

`pb_mmu_init(plat_mmap)` (`src/mmu.c`) maps three layers. xlat_tables_v2 accepts fully nested
flat regions, the innermost wins; partial overlaps are refused.

1. **The board's DRAM**: `[BOARD_DRAM_BASE, +SIZE)`, RW, execute never.
2. **The platform's `plat_mmap[]`**: devices, the ROM (its APIs), on-chip RAM, SoC regions. A
   region layer 1 already covers stays named when the SoC uses it for something particular (the
   next stage's load address over a reset stub).
3. **pb itself**: `.text` RX, `.rodata` RO execute never, the rest of `pb` RW execute never,
   `buffers` RW execute never. `pb` and `buffers` are two regions: one span over the gap between
   them maps the address space in between as memory (F-158).

The platform calls it in `plat_init()` once DRAM works; the arch enables the MMU
(`enable_mmu_svc_mon()` on ARMv7-A, `enable_mmu_el3()` on ARMv8-A). ARMv7-M has an MPU and no
`plat_mmap`. Layer 1 also maps DRAM pb never touches, such as a secure carve-out: pb runs in the
Secure state, and the filter (4.3), not pb's MMU, protects it from the normal world.

## 6. The build interface

### 6.1 Kconfig

- `choice PLAT` in `src/plat/Kconfig`: one symbol per platform or variant, depending on its arch
  (`ARCH_ARMV7`, `ARCH_ARMV8`, `ARCH_ARMV7M`) and the vendor's `SOC_FAMILY_*`; a family's common
  symbol selects its drivers.
- `src/plat/<p>/Kconfig`: the platform's options, one per image stage (6.2).

### 6.2 Image stages

**Not yet in the code (F-240)**: the boards hold the recipes.

`src/plat/<p>/makefile.mk` provides the SoC's stages, in `$(BUILD_DIR)`:

| Stage | Target | In, out | Kconfig |
|---|---|---|---|
| Package | `<p>_image` | `pb.bin`, the SoC's container | `<P>_CREATE_IMAGE` |
| Signing description | `<p>_sign_desc` | the container, the description | `<P>_SIGN_IMAGE` |
| Sign | `<p>_sign_image` | the description, the signed container | `<P>_SIGN_IMAGE` |

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

## 7. What a board may use

A board includes `include/pb/`, `include/drivers/` and `include/plat/<p>/`, nothing from
`src/plat/`. The platform header documents:

- Base addresses, and the pin macros (`<P>_PIN()`, D-026).
- The fuse index macro, for `board.slc_fuses`.
- One accessor, `<p>_plat()`, with documented fields: the SoC revision, the speed grade, the
  clock rates boards need, a system controller's handle. Its fields are set by `plat_init()`.
- Helpers for boards (pad setup through a system controller).
- The tick's accuracy, the watchdog's longest stretch without a kick, the boot reason's `raw`
  values and priority, what `plat_lockdown()` locks, and for level 3 builds what the ROM requires
  around `pb.bin`.

Clocks are code (Jonas, 2026-11-09). Who sets one up: whoever selects the peripheral. The platform
does the timer, the watchdog, the fuse controller, the TrustZone filter, DRAM, the crypto engine
and the RNG; the board does the UART, SD/MMC, USB and Ethernet.

## 8. Review checklist

For a new platform, and for a change to one:

- [ ] `plat_early_init()` starts the tick and calls `log_time_start()`, arms the watchdog, disables
      the timers armed out of reset, routes resets back to the ROM, saves the ROM's state, reads
      the reset reason without clearing it; it can't fail.
- [ ] `plat_init()` returns an error only when command mode's prerequisites are missing, and
      records the rest in `plat_get_status()`.
- [ ] `plat_reset()` works before `plat_early_init()` and without `CONFIG_ENABLE_WATCHDOG`.
- [ ] `plat_wdog_kick()` is safe before the watchdog is armed. The header gives the longest
      stretch without a kick, measured.
- [ ] `plat_get_us_tick()` is in microseconds and wraps at 2^32; the header gives its accuracy.
- [ ] `plat_get_boot_reason()` maps every cause to a `kind`, with the priority in the header.
- [ ] The device identity is unchanged: `platform_ns_uuid`, the unique id's bytes, order, length.
      A new platform has a new random namespace.
- [ ] The TrustZone filter (4.3): out of bypass before DRAM is used, the enable locked where
      possible, the board's DRAM open in a region of its own, the default region secure, the state
      reported; or the header says why there is none.
- [ ] `plat_lockdown()` locks key revocation and what reopens a closed device, or the header says
      why there is nothing.
- [ ] The duties of 4.5 are done, or left to a named later stage.
- [ ] `plat_rot` and `plat_slc` only read and write fuses; the rules stay in `rot.c`, `slc.c`.
- [ ] The link script has no addresses but the SoC's regions; its `ASSERT`s check the windows.
- [ ] `plat_mmap[]` holds only SoC regions; `pb_mmu_init()` is called after DRAM works.
- [ ] The image stages follow 6.2: `?=` inputs, Kconfig gated, all three override levels work.
- [ ] The header documents what section 7 lists.
- [ ] Facts from a reference manual are cited by document and section. NDA documents are not
      quoted, only named.
- [ ] The port's conformance table is updated, and the markers of what was implemented are removed.

## 9. Ports

| Port | Documented in |
|---|---|
| imx6ul, imx8m (MQ, MP), imx8x (QXP, DXL), imxrt (RT10xx) | `plat-imx.md`, with each port's conformance |
| host (test only) | `testing.md`. It implements what the tests need: the tick, the unique id, the |
| | console, reset as `exit()` |

The core's part of this contract, not yet in the code:

- `main()` resets after a platform failure: no, it runs command mode (F-235).
- The boot path kicks the watchdog: no, only command mode does (F-243).
- `boot()` checks the preconditions (2.3): no (F-248).
- `plat.h`: the tick's type, `noreturn`, `platform_ns_uuid` (F-237).
