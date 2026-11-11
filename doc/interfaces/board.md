# The board contract

What a board (`src/board/<b>/`, or a directory out of the tree) provides, when the core calls it,
and what it may rely on. Decided in D-031, in the style of the platform contract (`platform.md`,
D-030), which it builds on. It is independent of any SoC vendor: the i.MX boards' pins and clocks
are in `plat-imx.md`.

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

- **A board is what the PCB and the product decide** (D-026): the console, the storage and USB
  bring-up, the pins, the clocks of the peripherals it selects (`platform.md` 7), the partition
  layout, the A/B state, the boot driver's configuration, the configuration fuses, its own command
  mode extensions.
- **Not the SoC's facts.** They are the platform's (`platform.md` 1). A board can't replace a
  platform service, and it asks the platform what the ROM did (D-009): it doesn't read the ROM's
  boot mode itself.
- **Not the rules.** The core owns them (D-027): key revocation and the SLC order (`rot.c`,
  `slc.c`), the image verification, command mode's auth gate, the boot's preconditions
  (`platform.md` 2.3). A board's hook can stop a boot, never make one happen that the core refused.
- **A board is data.** `const struct board board` (`include/pb/board.h`), which the core sets up
  (`src/board.c`). What boards repeat moves into the core: as little board code as possible.
- **In or out of the tree.** `make BOARD=<path>` builds a board from any directory (section 7).
  `struct board`, the meaning of its hooks and the build interface change only in a major release
  (D-018). **Not yet in the code (F-240)**: CI builds an out-of-tree board, the level 3 override of
  `platform.md` 6.2, which tests this interface too.

## 2. Lifecycle

```
main()
  plat_early_init()
  board.console_init()                     2.2
  plat_init()
  board_setup()                            src/board.c
    the board's data is checked            3
    rot_init(board.rot), slc_init(board.slc)
    board.init()                           2.3
    the partition table                    2.4
    boot_ab_state_init(board.ab)           2.5
    board.late_init()                      2.6
    the boot driver                        2.7, also after a failure (2.1)
  boot()
    the serial download stop               5.1
    board.early_boot()                     5.2
    load, verify, prepare
    board.late_boot()                      5.3
    plat_lockdown(), the jump
  cm_run()                                 when board_setup() or boot() failed
    board.cm_init(), board.cm_transport    4.1, 4.2
```

### 2.1 Failure: a board failure stops the boot, never command mode

When a step of `board_setup()` fails, pb doesn't boot from storage. It enters command mode, and
command mode still has its boot commands.

`board_setup()` runs the board's steps in order (the list in section 2). When one fails it logs
which, skips the steps after it, sets up the boot driver all the same, and returns the step's
error. `main()` then enters command mode, where the host can install the partition table, repair
the A/B state or boot an image from RAM.

**Not yet in the code (F-216, F-178)**: `board_setup()` returns at the first step that fails. The
boot driver is the last step, so a failure before it leaves command mode without one: it can
neither repair the A/B state nor boot a rescue image from RAM. The one exception today is a failed
partition table, whose path does set up the boot driver.

- A board failure doesn't reset (a platform failure does, `platform.md` 2): command mode has what
  `plat_init()` set up.
- A failed RoT or SLC fails closed. Command mode asks for authentication when the life cycle is
  unknown (`slc_requires_auth()`), and without the RoT no image verifies (`rot_get_dsa_key()`
  refuses).
- What failed is in pb's log, which command mode reads (`punchboot log`).

### 2.2 Function : console_init() [optional]

```
Argument : void
Return   : void
```

Called once by `main()`, after `plat_early_init()`, before `plat_init()`. It is on every boot:
keep it short (D-023). It sets up the debug UART: its clock and pads, the UART driver, then
`console_init()` with the UART's ops.

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
crypto engine and its RNG. What only command mode needs is set up in `cm_init()` (D-023).

Failure: return an error only when the boot can't go on safely: the device the boot loads from, the
crypto engine the boot verifies with. Anything else is logged, shown by `status()` (4.3), and
returns PB_OK (jiffy: the CAAM's RNG for Linux). An error: 2.1.

NULL: no hardware of the board's own.

### 2.4 Data : gpt_dev_uu, gpt_tables [optional]

`gpt_dev_uu` is a bio device `init()` set up, with a GPT. `gpt_tables` are the layouts command mode
can install (`part install`), each with a name and a variant. The core reads the table after
`init()` (`gpt_ptbl_init()`).

Failure: no valid table: 2.1. Command mode installs one.

NULL: no GPT. The board sets up its partitions in `init()` (the RT: `static_ptbl_init()` on its
NOR).

### 2.5 Data : ab [optional]

Shaped by the boot contract (`boot.md` 3, draft): this is A/B's data; the fixed and staggered
policies get their own, and the data check (3) asks for the selected policy's.

`struct boot_ab_state_config`: the primary and backup state partitions, System A and B, the rollback
mode. The state's format is shared with `pbstate` in Linux.

Failure: a state that can't be read or repaired: 2.1.

NULL: no A/B state. **Not yet in the code (F-215)**: such a board can't boot from storage (no
`get_boot_bio_device`); a `boot_part_uu` field will name its boot partition.

### 2.6 Function : late_init() [optional]

```
Argument : void
Return   : int, PB_OK or -PB_ERR_*
```

Called once by `board_setup()`, after the partition table and the A/B state: for what needs the
partitions. Failure as in 2.3. NULL: nothing.

### 2.7 Data : boot_linux, boot_atf, boot_simple [mandatory, exactly one]

Shaped by the boot contract (`boot.md` 5, draft): the handover, `image` or Firmware Handoff, and
its delivery, directly or through TF-A; the DT hook runs first, with the whole tree, and pb's
properties after it (`boot.md` 5.5).

The boot driver's configuration (`boot/linux.h`, `boot/atf.h`, `boot/simple.h`); its Kconfig option
is on. The next stage's state is the arch's (D-028), what pb passes it is the boot driver's.
`bootargs` and `bootargs_verbose` are the kernel command line `board_patch_bootargs()` sets, when
the board passes it as `dtb_patch_cb`; `bootargs_verbose` is used with `BOOT_FLAG_VERBOSE`.

Set up last, and also after a failed step (2.1).

## 3. The board's data, checked

**Not yet in the code (F-249)**: nothing is checked, and a NULL `cm_transport` is dereferenced.

`board_setup()` and `cm_board_init()` check the board's data before they use it, a few lines tested
on the host:

| Field | Rule |
|---|---|
| `name` | Mandatory, 1 to 16 characters: the wire's `board_id` |
| `rot`, `slc` | Mandatory. Until F-247 moves them to the platform (`plat_rot`, `plat_slc`) |
| `boot_linux`, `boot_atf`, `boot_simple` | Exactly one, with its Kconfig option on |
| `gpt_tables` | At least one when `gpt_dev_uu` is set |
| `cm_transport` | Mandatory with `CONFIG_CM` |
| `password_auth` | Mandatory with `CONFIG_CM_AUTH_PASSWORD` |

A failed check in `board_setup()` is a failed step (2.1), with `-PB_ERR_PARAM`. One in
`cm_board_init()` means no command mode: `cm_run()` returns and `main()` resets.

## 4. Command mode

### 4.1 Function : cm_init() [optional]

```
Argument : void
Return   : int, PB_OK or -PB_ERR_*
```

Called by `cm_board_init()` at the start of `cm_run()`. It sets up command mode's hardware: the USB
controller, its PHY, their clocks.

Valid from: what `plat_init()` provides. It also runs after a failed `board_setup()` (2.1), so it
doesn't rely on what `init()` or `late_init()` set up.

Failure: no command mode, `main()` resets. NULL: the transport needs no hardware of the board's.

### 4.2 Data : cm_transport [mandatory with CONFIG_CM]

`struct cm_transport_ops`, usually `pb_dev_cls_transport` (USB). The core disconnects it before a
jump. What a transport and its USB device controller must do is the drivers' contract (later).

### 4.3 Function : status() [optional]

```
Argument : uint8_t *buf, size_t *length
Return   : int, PB_OK or -PB_ERR_*
```

Answers `board status`, which needs authentication on a locked device. In: `*length` is the size
of `buf`. Out: text in `buf`, `*length` bytes. It shows what `init()` logged and didn't fail on
(2.3).

**Not yet in the code (F-250)**: each board, and the default, appends the NUL itself and sets
`*length` from `snprintf()`, which writes past `buf` when the text was truncated. The rule: the
board writes at most `*length - 1` bytes of text; the core clamps the length and appends the NUL.

NULL: "Board status OK!".

### 4.4 Function : command() [optional]

```
Argument : uint32_t cmd, uint8_t *bfr, size_t bfr_length, uint8_t *rsp, size_t *rsp_size
Return   : int, PB_OK or -PB_ERR_*, the result the host gets
```

Answers `board command`: the board's own commands, by id. Behind the same auth gate as every
command (open in EOL, D-011). `bfr` is the host's request, `bfr_length` at most 1024 bytes. `rsp`
holds `*rsp_size` bytes (the command buffer).

- The request comes from the host: the board checks it against `bfr_length` (F-063).
- `*rsp_size` is set also when it fails.
- Command mode kicks the watchdog only between commands (F-189): a command that runs long calls
  `plat_wdog_kick()` as it makes progress.
- The ids and their answers are the board's wire (section 8).

NULL: the core's default, `-PB_ERR_NOT_SUPPORTED` for every id.

### 4.5 Function : password_auth() [mandatory with CONFIG_CM_AUTH_PASSWORD]

```
Argument : const char *password, size_t length
Return   : int, PB_OK or -PB_ERR_AUTHENTICATION_FAILED
```

The password comes from the host, `length` bytes, not NUL terminated. The board compares the whole
password (the lengths match too) in constant time, never logs it, and keeps the secret device
unique and out of the image (an RPMB, a fuse). **Not yet in the code (F-004)**: the 8X examples
accept any prefix and log the password.

Token authentication (a signature over the device's UUID) is the core's.

## 5. The boot

Where the hooks sit in the boot flow: `boot.md` 2 (draft).

The boot path is where boot time goes: the hooks start what is slow and don't wait for it.

### 5.1 The serial download stop (the core's)

**Not yet in the code (F-251)**: the RT (`late_boot`, SBMR2) and the 8X (`early_boot`, the SCU's
boot type) read the ROM's boot mode themselves; only the 6UL's `plat_get_boot_source()` reports a
serial download.

With `CONFIG_CM_ON_SERIAL_DOWNLOAD` (default y): when `plat_get_boot_source()` reports `USB` or
`UART`, the ROM's serial downloader loaded pb, and command mode didn't start the boot, `boot()`
stops before `early_boot()` and pb enters command mode. It is a register read, so it costs no boot
time. A board doesn't check for a serial download itself.

### 5.2 Function : early_boot() [optional]

```
Argument : void
Return   : int, PB_OK, -PB_ERR_ABORT or -PB_ERR_*
```

Called by `boot()` before the image is loaded, on every boot, also one command mode started
(`BOOT_FLAG_CMD` in `boot_get_flags()`). Valid from: everything `board_setup()` set up.

- It starts what is slow: a hardware check that needs time, whose result `late_boot()` reads, so
  that the load runs while it settles. It doesn't wait for hardware (D-023).
- It sets up what the next stage needs from the board.
- It may stop the boot: a check whose answer is there now (a button, a request register).

Return:

- PB_OK: go on.
- `-PB_ERR_ABORT`: stop, enter command mode. Not a failure.
- Any other error: a failure. The boot stops the same way, logged as an error.

`BOOT_FLAG_CMD`: a boot command mode started is one the host asked for. A check that stops boots
for command mode's sake skips itself then; one that must stop even such a boot (the 8X's button)
doesn't. The board decides, the core passes the flag.

NULL: nothing.

### 5.3 Function : late_boot() [optional]

```
Argument : struct bpak_header *header, uuid_t boot_part_uu
Return   : int, PB_OK, -PB_ERR_ABORT or -PB_ERR_*
```

Called by `boot()` after the image is loaded, verified and prepared, before `plat_lockdown()` and
the jump: the board's last call, and its last chance to stop. `header` is the verified header,
`boot_part_uu` the boot partition's UUID.

- It reads the result of what `early_boot()` started or what has run since reset. The example is
  the RT's charger detection: the hardware needs more than 100 ms, so pb loads the image while it
  runs and checks for a charger here.
- After its checks, it stops what the board started that must not run in the next stage (DMA, a
  controller), or the board's documentation says what it leaves running.
- It changes nothing that was verified. The board changes the next stage's input only through
  `dtb_patch_cb` (2.7).

Return and `BOOT_FLAG_CMD` as in 5.2.

NULL: nothing.

### 5.4 A stop in a boot that command mode started

**Not yet in the code (F-252)**: command mode reads `-PB_ERR_ABORT` as its own "leave and reset"
(`device reset`), so a hook that stops a boot command mode started resets the board, also before
the disconnect, where the host could get the error. Any other failure after the disconnect returns
to command mode's loop, which reads the next command from the disconnected transport (unchecked).

`boot part` answers the host once the image is loaded and verified, `boot bpak` (from RAM) after
each step of the load. Both then disconnect the transport and call `late_boot()`,
`plat_lockdown()` and the jump. The host's last OK means "loaded and verified", not "booted".

- A stop or failure before the disconnect (`early_boot()`, the load): the host gets the error,
  command mode goes on.
- A stop or failure after it (`late_boot()`, `plat_lockdown()`): pb resets.

## 6. What a board may use and touch

- It includes `include/pb/`, `include/drivers/`, `include/boot/` and `include/plat/<p>/`, nothing
  from `src/plat/`. What the platform header documents for boards is `platform.md` 7.
- It sets up the clocks and pads of the peripherals it selects: the UART, SD/MMC, USB, Ethernet
  (`platform.md` 7). The platform does the timer, the watchdog, the fuse controller, the TrustZone
  filter, DRAM, the crypto engine and the RNG.
- It reads the ROM's state, the boot source and the boot reason through the platform's services
  (`plat_get_boot_source()`, `plat_get_boot_reason()`). It may show raw registers in `status()`.
- It never writes fuses itself: the configuration fuses are data (`slc.fuses`), written by the
  core's `slc_set_configuration()` (D-027).

## 7. The build interface

A board directory holds:

| File | What |
|---|---|
| `board.c` | `const struct board board`, the hooks |
| `makefile.mk` | `src-y` for its sources, image stage overrides (`platform.md` 6.2), included before the |
| | platform's makefile |
| `Kconfig` | Optional, the board's options (`osource "$(BOARD)/Kconfig"` in `src/Kconfig`) |
| `include/` | Optional, on the include path |

The defconfig selects the arch, the platform, the boot driver and the keys: `configs/<b>_defconfig`
in the tree, any file out of it, passed as `KCONFIG_CONFIG` (`build-and-test.md`). The build
directory is `build-<last part of BOARD>`.

**Not yet in the code (F-239)**: the board's addresses (`PB_ENTRY`, `BOARD_RAM_BASE`, `_END`) are
make variables and `-D` flags in `makefile.mk`; `platform.md` 5.1 makes them Kconfig values
(`CONFIG_PB_ENTRY`, `CONFIG_BOARD_DRAM_*`). **Not yet in the code (F-240)**: the boards hold the
SoC's image recipes; `platform.md` 6.2 moves them to the platform.

## 8. Fixed once a product ships

A board's product in the field relies on these like on the wire (D-008). A board changes them only
for a new product, never in an update:

- `name`: the wire's `board_id`, readable without authentication. Hosts and factory scripts select
  devices by it.
- The partition UUIDs, the GPT tables' names and variants, the A/B state partitions. The OS, its
  update tool and `pbstate` find them by UUID.
- The configuration fuses (`slc.fuses`) and the keystore's slots (`CONFIG_KEYSTORE_SLOTS`, D-027):
  fuses can't be unwritten.
- The ids of the board's commands, and their answers (4.4). The core's default answer is fixed
  too (F-218: it changed the 8X boards' answer from OK to NOT_SUPPORTED).
- The handover, its delivery and, with Firmware Handoff, the transfer list's region (`boot.md`
  5.4, draft): the images' memory maps keep it free.

## 9. Review checklist

For a new board, and for a change to one:

- [ ] Only what the PCB and the product decide: no SoC facts, no rules, no platform registers
      outside `status()`.
- [ ] The data passes section 3.
- [ ] `console_init()` uses nothing `plat_init()` sets up.
- [ ] `init()` and `late_init()` fail only when the boot can't go on safely; the rest is logged and
      in `status()`.
- [ ] `cm_init()` sets up command mode's hardware and relies on nothing from `init()`.
- [ ] The boot hooks don't wait for hardware: a slow check starts in `early_boot()` (or runs from
      reset) and is read in `late_boot()`.
- [ ] Each stop check decides for `BOOT_FLAG_CMD`; no check for a serial download (5.1).
- [ ] `late_boot()` changes nothing that was verified, and stops what must not run in the next
      stage.
- [ ] `command()` checks the request's size and kicks the watchdog in long commands; `status()`
      writes at most `*length - 1` bytes.
- [ ] `password_auth()`: the whole password, constant time, never logged, a device unique secret.
- [ ] Section 8 unchanged for a product in the field.
- [ ] A board that runs Linux has its test bench defconfig and both lists of
      `testbench/README.md`, "Adding a board", are done.
- [ ] The conformance table is updated, and the markers of what was implemented are removed.

## 10. Boards

Where each board is (2026-11-11). The host board is the tests' (`testing.md`) and follows the
contract too, so that the order of section 2 is tested end to end (F-217).

| Requirement | jiffy | 8MP EVK | MEK | DXL EVK | RT1060 EVK | imx8mevk | host | Finding |
|---|---|---|---|---|---|---|---|---|
| Data (3) | no | yes | yes | yes | no | no | no | F-249 |
| No serial download check (5.1) | yes | yes | no | no | no | yes | yes | F-251 |
| `status()` (4.3) | no | default | no | no | default | default | default | F-250 |
| `password_auth()` (4.5) | n/a | n/a | no | no | n/a | n/a | n/a | F-004 |
| Storage boot with `ab` | yes | yes | yes | yes | yes | no | yes | F-215 |
| The order of section 2 | yes | yes | yes | yes | yes | yes | no | F-217 |
| Test bench defconfig | yes | yes | yes | no | n/a | no | n/a | F-033 |

Data: jiffy, the RT and the host set `CONFIG_CM_AUTH_PASSWORD` without a callback; imx8mevk has
`CONFIG_BOOT_LINUX` without `boot_linux`.
