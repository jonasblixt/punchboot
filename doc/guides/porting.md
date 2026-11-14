# Porting: a board, a driver, a platform

How a board is written from nothing, and a driver, and a port to a new SoC. A guide: it says in
which order to do things and where each rule is, and decides nothing. The rules are the
contracts' (`doc/README.md` has the map), and a section they cite is the text to follow. Where
this guide and a contract differ, the contract is right.

Most ports are a board on a SoC that pb has a platform for: section 2, with jiffy
(`src/board/jiffy/`, an i.MX6UL with an eMMC that boots Linux) as the example. A board whose
hardware has no driver yet also needs section 3. A new SoC needs section 4 first.

A board written for 2.x is ported with `migration-3.0.md`.

## 1. Before the first line

- **What pb is and promises**: `system.md` 1 to 5. What it leaves to the product, which is
  what the board's author decides: `system.md` 6.
- **The rules a port can't change**: `security.md` 4. The core owns them (`security.md` 4.8); a
  board or a platform gives it data and hardware access.
- **What is whose.** A fact of the SoC is the platform's, a choice for the PCB or the product
  is the board's, and code for one IP block is a driver (`platform.md` 1, `board.md` 1,
  `drivers.md` 1). Sorting each thing before it is written saves moving it later.
- **Boot time**: what only command mode uses is set up when command mode first runs, not on
  the boot path (`principles.md` 4, D-023).
- **The build and the tools**: `build-and-test.md`. `scripts/pbdev firmware <board>` builds a
  board of the tree, `scripts/pbdev test` runs the host tests.

## 2. A board

The order below is the order in which a new board comes up. Each step can be tried before the
next: `board_setup()` stops at a step that fails, and pb enters command mode with what it has
(`board.md` 2.1).

### 2.1 The directory, the defconfig, the build

`board.md` 7 has the files of a board directory, in the tree or out of it, and what the
defconfig holds. jiffy's:

| File            | What                                                                       |
| --------------- | -------------------------------------------------------------------------- |
| `board.c`       | `const struct board board` and its hooks                                   |
| `partitions.h`  | The partitions' UUIDs                                                      |
| `makefile.mk`   | `src-y`: the platform packages and signs pb for the ROM                    |
| `imximage.cfg`  | The DCD: the register writes the i.MX6UL's ROM does before it loads pb,    |
|                 | the DRAM's set up among them                                               |
| `openocd.cfg`   | For `scripts/pbdebug`                                                      |

1. Copy the closest defconfig in `configs/` and the closest board's `makefile.mk`. Select the
   arch, the platform, the keystore, the modules the board will name, the platform's USB port
   for command mode and the authentication methods (`board.md` 7's table).
2. Set where pb is linked and the board's DRAM, in the defconfig: `CONFIG_PB_ENTRY`,
   `CONFIG_PB_SIZE`, `CONFIG_BOARD_DRAM_BASE` and `_SIZE`, and the buffers' place where the
   platform's default doesn't fit (`platform.md` 5.1; the load windows of the i.MX ports are
   `plat-imx.md` 5). jiffy: pb at 0x80001000 with 1 MiB, and 1 GiB of DRAM from 0x80000000.
   The link fails when pb or its buffers aren't where they may be.
3. Get the SoC's container around `pb.bin`, so that the ROM loads it: the platform's stages,
   turned on in the defconfig with the keys, and their inputs from the board (`platform.md` 6.2,
   `plat-imx.md` 6). jiffy's are the options `IMX6UL_CREATE_IMX_IMAGE` and `IMX6UL_SIGN_IMAGE`
   with the HAB keys, and its DCD, `imximage.cfg`, which the stage finds in the board's
   directory.
4. Build: `make BOARD=<path> KCONFIG_CONFIG=<defconfig> CROSS_COMPILE=<prefix>`
   (`build-and-test.md`).

The UUIDs in `partitions.h` are made once, at random, and never change for the product
(`board.md` 8).

### 2.2 The console

`console_init()`: `board.md` 2.2, and `platform.md` 7 for what of the platform works that
early. It runs before `plat_init()`. jiffy's ungates UART2's clock, sets two pads and gives
the UART's `putc` to `console_init()`.

First light is the boot banner. A board without a UART has the RAM log, which command mode
reads (`punchboot log`).

### 2.3 The keys and the life cycle

`rot` and `slc`: `board.md` 3 and 6, D-027. They are first in `board_setup()`, and command mode
needs them: without a life cycle that can be read the device counts as closed, and without the
root of trust no token opens it (`board.md` 2.1). What the states allow: `system.md` 4. Which
functions a SoC has: `plat-imx.md` 4.1.

1. The keystore (`CONFIG_KEYSTORE`) holds the public keys images are verified with, and
   `CONFIG_KEYSTORE_SLOTS` each key's revoke slot.
2. `rot` and `slc` name the platform's functions. jiffy: the HABv4 driver's (`imx_hab_*`).
   They move to the platform (F-247).
3. The configuration fuses are data, `slc.fuses`: the boot device and what else the product
   fuses. jiffy: boot from the eMMC on uSDHC1, and take the boot configuration from the fuses.
4. `CONFIG_SRK_FUSE` names the hash of the keys pb itself is signed with.

The commands that write fuses can't be undone (`command-mode.md` 5.2): run none on a board
that must stay open, and check `slc.fuses` and the SRK file against the reference manual
first. What is fixed once a product ships: `board.md` 8.

### 2.4 The crypto engine, and command mode

`init()`: `board.md` 2.3. Its first job, for now, is the crypto engine (`crypto.md` 2): the
device's identity is computed with it (`platform.md` 3.7), and command mode doesn't start
without the identity. jiffy calls `imx_caam_init()` and `imx_caam_rng_init()`. The engine moves
to the platform (F-247).

For command mode itself the board does nothing: the port is the defconfig's choice and the
platform sets it up (`board.md` 4.1, `platform.md` 3.10; the i.MX ports' options are
`plat-imx.md` 3). Now `punchboot dev show` answers, and `punchboot slc show` reads the life
cycle. The later steps are still missing: `board.md` 2.1 says what command mode can do
then.

`board.name` is what `dev show` prints as the board. It is fixed for the product
(`board.md` 3 and 8).

### 2.5 Storage

Next in `init()`: the device the boot loads from. Its clock and pads, then the driver's `init`
with the board's configuration (`drivers.md` 1 and 2.1). The driver registers the block devices
with the UUIDs the board gives (`bio.md` 2.1).

jiffy's `usdhc_emmc_setup()` ungates uSDHC1's clock, sets the pads and calls
`imx_usdhc_init()` with the bus mode, the width and the UUIDs of BOOT0, BOOT1, the user area
and the RPMB.

- What the driver leaves behind for the ROM and the next stage, an eMMC's bus mode and boot
  configuration among them: `bio.md` 5 and the driver's header
  (`include/drivers/mmc/mmc_core.h`).
- The flags a host sees on each device: `bio.md` 2.4.
- What `init()` fails on and what it only reports: `board.md` 2.3.

Try it: `punchboot part list` shows the devices.

### 2.6 The partition table

`ptbl`: `board.md` 2.4, and `bio.md` 4 for the tables. jiffy gives a GPT with one layout,
`PTBL_GPT(PART_user, gpt_tables)`. `punchboot part install` writes it, and it is in effect
at once.

The layouts' names and variants and the partitions' UUIDs are fixed for the product
(`board.md` 8). A memory mapped device whose image runs where it is: the in place source,
`board.md` 2.4 and `boot.md` 4.1.

### 2.7 The boot source policy

`boot_policy`: `board.md` 2.5 for the member, `boot.md` 3 for what a policy does and
`boot.md` 3.5 for the three there are. jiffy: A/B, with System A and B and the two state
partitions.

The A/B state is shared with the OS, which confirms an update with `pbstate`
(`boot.md` 3.7). Speculative mode is a Kconfig option, not the board's (`boot.md` 3.6).

### 2.8 The handover

`boot_handover` and `boot_delivery`: `board.md` 2.7. Which one:

| The next stage                         | Handover and delivery                 | Read          |
| -------------------------------------- | ------------------------------------- | ------------- |
| Linux or Xen, started by pb            | `BOOT_HANDOVER_IMAGE()`               | `boot.md` 5.2 |
| The same behind TF-A's BL31 (ARMv8-A,  | `BOOT_HANDOVER_IMAGE()` with          | `boot.md` 5.3 |
| where pb runs at EL3)                  | `BOOT_DELIVERY_TFA()`                 |               |
| An RTOS, an application, anything else | `BOOT_HANDOVER_FH()`                  | `boot.md` 5.4 |

jiffy: the image handover, with the parts `kernel`, `dt` and `ramdisk`, the device tree's
address in r2, and the kernel command line as `bootargs` and `bootargs_verbose`.

- The state the next stage starts in is the arch's, the same on every arch (D-028).
- What pb writes into the device tree and what the board's hook may change: `boot.md` 5.5.
- What pb tells the next stage about the device and the boot: `boot.md` 5.6.
- The parts are named by `bpak_id()` of their names; how punchboot uses bpak is `bpak.md`.

### 2.9 Where images may be loaded

`image_regions`: `board.md` 2.9, with `boot.md` 4.3 for the check and `platform.md` 5.3 for the
SoC memory a board may list. Without the list nothing is loaded.

jiffy runs in its DRAM, so its two regions go round pb: from the end of pb's 1 MiB to its
buffers, and from the end of the buffers to the end of the DRAM. They are computed from the
defconfig's values, which the link uses too; the comment above the list in `board.c` says how.

Now an image boots. `punchboot boot bpak <file>` loads one from the host into RAM and needs
neither the partition table nor the policy (`board.md` 2.1). `part write` and
`boot partition <uuid>` then try the storage, and `boot enable <uuid>` and a reset the policy.

### 2.10 The hooks, and what else a board may have

All optional. jiffy has `status()` and one named command, and none of the others.

| What                                    | Read                                    |
| --------------------------------------- | --------------------------------------- |
| `early_boot()`, `late_boot()`: stop a   | `board.md` 5.2 to 5.4                   |
| boot, start what is slow                |                                         |
| The stop after a serial download        | `board.md` 5.1: the core's, an option   |
| `late_init()`                           | `board.md` 2.6                          |
| `status()`                              | `board.md` 4.3                          |
| `password_auth()`                       | `board.md` 4.4, `security.md` 4.10      |
| A named command                         | `command-mode.md` 5.6                   |
| A task                                  | `board.md` 4.5                          |
| Hardware the platform didn't map        | `board.md` 2.8 (F-269: not in the code) |

What a board may include and touch: `board.md` 6.

### 2.11 Done

1. The review checklist: `board.md` 9.
2. What is fixed from the first product on: `board.md` 8. Decide those values on purpose.
3. The watchdog's timeout against the board's longest stretch without a kick:
   `platform.md` 3.3.
4. A board in the tree: its defconfig in `configs/`, its row in `board.md` 10, and a line in
   `scripts/ci/build-firmware.sh`.
5. A board that runs Linux gets a test bench defconfig, and both lists of
   `testbench/README.md`, "Adding a board". A board with an MCU:
   `testbench/simple/README.md`, "Adding a board".

## 3. A driver

A new implementation of a class that has a contract needs no document first (`process.md` 1,
D-063): the contract already says what it must do. The order:

1. **Is it a driver?** Code for one IP block, with the SoC's and the board's facts as
   arguments: `drivers.md` 1. Who calls its `init`, the platform or the board:
   `platform.md` 7.
2. **Set up and stop**: `drivers.md` 2. A device that acts without being asked has a `stop`
   (`drivers.md` 2.3).
3. **Waits and return codes**: `drivers.md` 3. Every wait has a bound, with its source.
4. **DMA and caches**: `drivers.md` 4. The driver includes `<pb/dma.h>` and no arch header,
   declares its own DMA memory with `PB_DMA_BUFFER()` or `PB_DMA_ARRAY()`, and follows 4.2's
   order with `dma_out()`, `dma_in_begin()` and `dma_in_end()`. A caller's buffer that a device
   writes reaches it on a cache line: the class's core refuses one that isn't (4.3).
5. **The class's contract**:

| Class                 | Contract    | Start at                                             |
| --------------------- | ----------- | ---------------------------------------------------- |
| A block device        | `bio.md`    | 2.1 (registration), 3 (the operations), 5 (what the  |
|                       |             | driver leaves behind)                                |
| A partition table     | `bio.md`    | 4                                                    |
| A crypto backend      | `crypto.md` | 2 (registration), 3 and 4 (the operations), 6 (the   |
|                       |             | engine at the jump)                                  |
| A USB device          | `usb.md`    | 4 (the controller's operations), 5 (buffers)         |
| controller            |             |                                                      |
| Anything else (a      | None        | `drivers.md`, and its header's comments. What the    |
| UART, a fuse          |             | core needs of it, it asks the platform for           |
| controller, a timer)  |             | (`platform.md` 3 and 4)                              |

6. **Tests** on the host, against a model of the device where one can be written:
   `testing.md`. A block device and a crypto backend also run the class's conformance suite
   (`tests/unit/conformance/`).
7. **The checklists**: `drivers.md` 7 and the class's own. Then the driver's row in the
   conformance tables (`drivers.md` 8 and the class's).

What a driver's interface may change, and when: `drivers.md` 6.

## 4. A platform

A platform is a SoC family, and a variant of a family that pb has is a sub-level of it
(`platform.md` 1 and 6.1). `plat-imx.md` is the worked example: it follows `platform.md`'s
sections and says how four ports meet each.

A port needs an arch. A new one gives the next stage the same state as the others (D-028,
`include/pb/arch.h`).

The order, with what shows that each step works:

| Step | What                                    | Read                    | It works when         |
| ---- | --------------------------------------- | ----------------------- | --------------------- |
| 1    | Where the ROM loads pb, the link script | `platform.md` 5.1, 6.2  | The ROM starts pb     |
|      | and the container around `pb.bin`       |                         |                       |
| 2    | `plat_early_init()`: the tick, the      | `platform.md` 2.1, 3.1  | A board's console     |
|      | watchdog, the reset, the ROM's state    | to 3.3                  | prints the banner;    |
|      |                                         |                         | `plat_reset()` resets |
| 3    | `plat_init()`: clocks, fuses, DRAM, the | `platform.md` 2.2, 4.3, | pb reaches            |
|      | TrustZone DRAM filter, the memory map   | 5.2                     | `board_setup()`       |
| 4    | The device's identity                   | `platform.md` 3.5 to    | The same UUID after   |
|      |                                         | 3.7                     | every reset           |
| 5    | `plat_cm_init()` and the USB            | `platform.md` 3.10,     | `punchboot dev show`  |
|      | controller's driver                     | `usb.md` 4              | answers               |
| 6    | The boot source and the reset reason    | `platform.md` 3.9, 3.4  | A serial download     |
|      |                                         |                         | stops in command mode |
| 7    | What `plat_init()` couldn't do          | `platform.md` 2.3, 3.8  | A boot is refused     |
|      |                                         |                         | with the filter in    |
|      |                                         |                         | bypass                |
| 8    | The keys and the life cycle             | `platform.md` 4.1, 4.2  | `punchboot slc show`  |
| 9    | The lockdown, and the other duties of   | `platform.md` 4.4, 4.5  | The next stage runs   |
|      | the stage in BL2's place                |                         |                       |
| 10   | Memory for images, and the header a     | `platform.md` 5.3, 7    | A board boots an      |
|      | board includes                          |                         | image                 |

- Steps 1 to 3 need a board for the console: write the two side by side.
- Step 4: the namespace is generated once, at random, and never changes
  (`platform.md` 3.7).
- With step 3, `plat_get_info()` (`platform.md` 3.11): the SoC's name, revision and clocks,
  read from the SoC. The boot log's line after the banner shows what step 3 set up.
- Step 7 before the first product: a filter that is left in bypass shows nowhere else
  (`platform.md` 2.3).
- Step 8 is `rot` and `slc` functions that the boards name today (F-247). The rules stay in
  the core (D-027).
- What a failure of the platform means, and why it resets where a board's failure doesn't:
  `platform.md` 2.
- The build's stages and what a board may override: `platform.md` 6.2.

Then:

1. The review checklist: `platform.md` 8.
2. The port's document, at level 4 (`process.md` 2): the SoC's facts with their sources, who
   does each duty of `platform.md` 4.5, and the port's conformance table. A new port joins
   `plat-imx.md` if it is an i.MX, or gets a document of its own in `doc/ports/` and a row in
   the map (`doc/README.md`); the lint checks both (`process.md` 7).
3. Its row in `platform.md` 9.
4. Tests: the port's `plat.c` is built for the host and tested against register fakes
   (`tests/unit/test_plat_*.c`, `testing.md`).

## 5. When the contract doesn't say

The code needs something the contract doesn't have, or says otherwise: the contract changes
first (`process.md` 1 and 5). What a port shows about the hardware is a fact, and a fact can
show that a contract is wrong; then the document changes, and the code after it.

An interface without a contract has its header and its implementations for one until then.
