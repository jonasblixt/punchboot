# The block device contract

**State: reviewed (Jonas, 2026-11-12), D-064, D-067 to D-070, D-073.** Level 3 (`process.md`).

What a block device is in pb (`include/pb/bio.h`, `src/bio.c`): what the boot, command mode, a
boot source policy and a board may rely on when they read, write and erase one, and what a
driver that registers one provides. It builds on `drivers.md`, which every driver follows. Vendor
neutral: where each driver stands is section 8.

Changing `bio`, a storage driver or a partition table: read this and `drivers.md`, then go
through the checklist (section 7). Section 6 is fixed for a product in the field.

**Not yet in the code (F-nnn)** marks what the code doesn't do yet. The commit that fixes the
finding removes the marker and updates section 8.

**No open items.** What Jonas's review decided (D-067 to D-070), as written into the text:

| Item | Rule                                                                         | By    |
| ---- | ---------------------------------------------------------------------------- | ----- |
| 1    | The core owns a length that isn't whole blocks; a driver sees whole blocks   | Jonas |
|      | only (3.1, 3.2)                                                              |       |
| 2    | No read at a byte offset for now. A part that doesn't start on a block of    | Jonas |
|      | its device is refused, as released (3.1)                                     |       |
| 3    | An installed table is in effect at once, for `bio` and the core. A handle    | Jonas |
|      | never comes to mean another partition: an unchanged partition keeps its      |       |
|      | handle, a changed one's is dead until the reset. No hook of the board's runs |       |
|      | again (4)                                                                    |       |
| 4    | `bio_write()` is to erase what it writes where the flag is set. A future     | Jonas |
|      | task: the erase blocks' accounting is designed first (3.3, `roadmap.md`).    |       |
|      | Until then the writer erases, as released                                    |       |
| 5    | A device is left as it is at the jump, nothing in flight (5)                 | Jonas |
| 6    | `bio_erase()` counts blocks, not bytes (3.3)                                 | Jonas |
|      | `bio_write()` is optional, whatever the flags say (3.2)                      | Jonas |

## 1. Words and scope

| Word      | Means                                                                                   |
| --------- | --------------------------------------------------------------------------------------- |
| Device    | A range of blocks of one size with a UUID, read, written and erased through `bio`       |
| Partition | A device that is a range of another one (its parent): made by a partition table         |
| Handle    | `bio_dev_t`: 0 and up is a device, a negative value is an error. 0 is a valid device    |
| Block     | The device's unit, `bio_block_size()` bytes. An LBA counts blocks from the device's own |
|           | first block                                                                             |
| User      | Whoever calls `bio_*()`: the boot, command mode, a boot source policy, a board          |
| Driver    | Whoever registers a device and gives its callbacks                                      |

Not here: what a host may do with a partition (`command-mode.md` 5.3), which partition boots
(`boot.md` 3), the board's choice of table (`board.md` 2.4), the in place source, which is
memory that is mapped and not read through `bio` (`boot.md` 4.1).

What is on a device is input (`security.md` 3): whoever writes the storage controls every
byte, the partition table too. `bio` carries bytes; nothing it returns is trusted before the
boot verified it.

## 2. Devices

### 2.1 Registration

A driver registers its devices in its `init` (`drivers.md` 2.1), which the board's `init()`
calls (`board.md` 2.3): `bio_allocate()` with the block range, the block size, the UUID and a
description, then the callbacks and the flags.

- **The UUID is the board's** and fixed for the product (`board.md` 8). A driver takes it from
  its configuration and invents none.
- **The pool** holds `CONFIG_BIO_MAX_DEVS` devices, partitions included. A full pool is
  `-PB_ERR_MEM`: the board's failure (`board.md` 2.1).
- **A device is registered once.** A driver that is set up again doesn't add its devices a
  second time. **Not yet in the code (F-283)**: nothing stops a second `mmc_init()` from it.
- **The range is what the medium has**, read from it and checked before use: a size of 0, or
  one the handle can't express, is refused (`drivers.md` 5, F-200).
- LBAs are 32 bits (`lba_t`): 2 TiB of 512 byte blocks. `bio_size()` is 64 bits.

### 2.2 Partitions

A partition table makes partitions with `bio_allocate_parent()` (section 4). A partition:

- **Lies inside its parent.** Its range is in the parent's blocks, as the table says, and the
  core checks that it is inside.
- **Has the parent's block size**, callbacks, driver flags and private value, and starts with
  the parent's flags. It doesn't get the parent's `write_done` or its table's install callback.
- **Is addressed from its own first block.** A user never adds the partition's offset; the
  driver's callback gets the parent's LBA.

### 2.3 The block size

One per device, from the driver: 512 bytes on MMC and SD, the erase sector on a NOR (4 KiB on
the RT1060 EVK's).

- **A user asks `bio_block_size()`** and never assumes 512. **Not yet in the code (F-283)**:
  the GPT has 512 written out in four places, and the A/B state writes 512 bytes as one block.
- **The largest block is `CONFIG_BIO_MAX_BLOCK_SIZE`** (4 KiB), the size of the core's block
  buffer (3.1). A device with a larger one is refused at registration.
- A format that puts something at a fixed byte position (bpak's header in a partition's last
  4 KiB, `boot.md` 4.4) needs a block size that divides that position. What doesn't start on
  a block isn't read (3.1).

### 2.4 Flags

Sixteen flags per device. Their meaning for a host is `command-mode.md` 5.3, and their low byte
is on the wire (section 6).

| Flag                 | Set by                                 | Means                                    |
| -------------------- | -------------------------------------- | ---------------------------------------- |
| `VISIBLE`            | The driver, for what a host may see;   | The device exists for a host             |
|                      | the board hides what it may not        |                                          |
| `WRITABLE`           | The driver; the core takes it away     | A host may write and erase it            |
|                      | from the device a table is on          |                                          |
| `READABLE`           | The board                              | A host may read it in bulk               |
| `BOOTABLE`           | The boot source policy, never a board  | A candidate for a boot (`boot.md` 3)     |
|                      | (`board.md` 6) or a driver             |                                          |
| `ERASE_BEFORE_WRITE` | The driver                             | 3.3                                      |

- **The flags gate the host, not pb.** Inside pb every device can be read and written whatever
  its flags say; `BOOTABLE` is the one flag the core itself obeys.
- **A driver sets no more than the medium allows**: a device that must never be written from
  command mode is registered without `WRITABLE`, and one a host must not see without
  `VISIBLE` (an eMMC's RPMB).
- The other bits are reserved and zero.

### 2.5 The driver's own

Eight driver flags (`bio_set_hal_flags()`) and one private value (`bio_set_private()`) per
device, for the driver to tell its devices apart. Partitions inherit both. Nobody else reads
them.

### 2.6 Finding a device

- **By its UUID**: `bio_get_part_by_uu()`, or `bio_get_part_by_uu_str()` for the UUID as
  text. `-PB_ERR_NOT_FOUND` when no device has it. A UUID names at most one device. **Not yet
  in the code (F-283)**: nothing refuses a second device with a UUID that is taken, and the
  first one is found.
- **All of them**: a handle is a number from 0 to `CONFIG_BIO_MAX_DEVS` less one, and
  `bio_valid()` says whether it is a device. A user that lists the devices tries every
  number and skips the ones that aren't: a handle that is dead (section 4) can lie between
  two that aren't. **Not yet in the code (F-194)**: the look up by UUID and command mode's
  table read stop at the first number that isn't a device.
- **What a device is**: `bio_get_uu()`, `bio_get_description()`, `bio_block_size()`,
  `bio_size()` in bytes, `bio_get_no_of_blocks()`, and `bio_get_first_block()` and
  `bio_get_last_block()`, a partition's place on its parent, which the partition table's
  read reports (section 6). For a handle that isn't a device each answers `-PB_ERR_PARAM`, or
  NULL. **Not yet in the code (F-283)**: the three block getters return `int`, so a device of
  2^31 blocks or more reads as an error.
- **The flags**: `bio_get_flags()`, `bio_set_flags()`, `bio_clear_set_flags()` (2.4).
- **A handle is good until the reset**, and a user may keep one. The one exception is a
  partition whose table is installed again (section 4).
- `bio_reset()` releases every device. It is for the tests.

## 3. Reading, writing, erasing

Every call is synchronous (`drivers.md` 2.2): when it returns, the work is done or it failed,
and nothing is in flight. The return codes are `drivers.md` 3's. A range outside the device is
`-PB_ERR_PARAM`, checked by the core with arithmetic that can't wrap, before the driver is
called.

### 3.1 Function : bio_read() [mandatory]

```
Argument : bio_dev_t dev, lba_t lba, size_t length, void *buf
Return   : int, PB_OK or -PB_ERR_*
```

Reads `length` bytes from block `lba` of the device into `buf`.

- **Exactly `length` bytes are written to `buf`**, never more.
- **`length` is bytes.** A length that isn't whole blocks is the core's: the last
  block is read into the core's block buffer and the bytes asked for are copied. **Not yet in
  the code (F-284)**: the FlexSPI's driver refuses a length that isn't a multiple of 4.
- **A length of 0** is `PB_OK` and reads nothing; the driver isn't called.
- **The buffer** starts on a cache line and owns its lines (`drivers.md` 4.3).
  One that doesn't is refused, `-PB_ERR_ALIGN`, before the driver is called. An image part
  whose load address doesn't is the boot's to bounce, not `bio`'s.
- **No limit on the length** but the device's size: the driver splits a request its controller
  can't take in one piece. How long a call takes is its caller's to bound, by how much it
  asks for (`drivers.md` 3).
- **A failed read** leaves `buf` with unknown content. The caller doesn't use it (F-173).
- `-PB_ERR_NOT_SUPPORTED`: the device has no read.

**No read at a byte offset, for now** (Jonas, D-068). A read starts on a block. An image's
parts start where its format puts them, and bpak aligns them to 512 bytes: on a device with
larger blocks (the NOR's 4 KiB) a part that doesn't start on a block isn't read, and the boot
fails with `-PB_ERR_ALIGN` when it reaches that part, as in every release (F-220). An image's
first part always starts on a block: it is at the partition's first byte. A read at a byte
offset in the core would lift the limit; it is noted in `roadmap.md`.

The driver's callback:

```
Argument : bio_dev_t dev, lba_t lba, size_t length, void *buf
Return   : int, PB_OK or -PB_ERR_*
```

- `lba` is the parent's, `length` whole blocks and never 0, `buf` owns its lines.
- It finds its device with `bio_get_private()` or its driver flags.
- It does its own cache maintenance (`drivers.md` 4.2) and waits with a bound.

### 3.2 Function : bio_write() [optional]

```
Argument : bio_dev_t dev, lba_t lba, size_t length, const void *buf
Return   : int, PB_OK or -PB_ERR_*
```

Writes `length` bytes of `buf` from block `lba` on.

- **Exactly `length` bytes are read from `buf`**, never more. **Not yet in the code (F-284)**:
  the FlexSPI's driver reads up to four bytes past it.
- **A length that isn't whole blocks.** The core writes the last block from its
  block buffer: the bytes given, then zeros (`command-mode.md` 5.4). Nothing is read back to
  keep what the block held.
- **When it returns PB_OK the data is on the medium**: the device accepted it and is ready
  again. There is no cache to flush.
- **A write that fails** leaves any of the blocks written, in any order. Writing isn't atomic;
  what needs it writes two copies (`boot.md` 3.7).
- **It doesn't erase**, today: 3.3.
- A length of 0, the buffer and the limit as for a read.
- `-PB_ERR_NOT_SUPPORTED`: the device has no write.

### 3.3 Function : bio_erase() [optional]

```
Argument : bio_dev_t dev, lba_t first_lba, size_t count
Return   : int, PB_OK or -PB_ERR_*
```

Erases `count` blocks from `first_lba`. A device whose medium has no erase, or whose driver
has none, answers `-PB_ERR_NOT_SUPPORTED` (MMC and SD today: `part erase` isn't supported on
them).

- **The count is blocks, not bytes** (Jonas, D-069). A read and a write take bytes because the
  core can make a partial block whole: it pads a write and trims a read. An erase can't be
  padded: half a block can't be erased, so a byte count that isn't whole blocks could only be
  refused. With blocks the request that has no meaning can't be made. The wire counts in
  blocks too (`PART_ERASE`, `compatibility.md`).
- **Exactly those blocks**, whatever the medium's own erase units are. A driver that erases in
  larger units uses one only where the range covers it whole. **Not yet in the code (F-198)**:
  the FlexSPI's driver starts with its largest unit without checking the address, and rounds a
  short tail up.
- **What an erased block reads as** isn't promised. A user that needs zeros writes them.
- **A count of 0** is `PB_OK`, and the driver isn't called.
- **Time**: the driver's header says how long a block's erase takes at most
  (`drivers.md` 3). Erasing is the slow operation of a medium that has it; its caller erases
  in pieces (`command-mode.md` 4.6).
- A failure leaves some of the blocks erased.

**`ERASE_BEFORE_WRITE`.** The driver sets it on a device whose medium only clears bits when
it is written (NOR). It means: a write stores the bytes it is given only on blocks that have
been erased since they were last written.

- **Today the writer erases**, as in every release: `bio_write()` doesn't. A host sees the
  flag and erases (`part erase`, then the write), the A/B state erases its block before each
  write. A user in pb that writes any device (a policy's state, a board) checks the flag.
- **`bio_write()` is to do it** (Jonas, D-067): on a device with the flag a write erases what
  it writes, and no user has to know the medium. A future task, not designed here: the erase
  blocks' accounting comes first (`roadmap.md`). What it must keep: a host that erases before
  it writes, as every released host does where the flag is set, still gets its bytes written.

### 3.4 Function : bio_write_done() [optional]

```
Argument : bio_dev_t dev
Return   : int, PB_OK or -PB_ERR_*
```

Tells the driver that a complete write of the device ended without an error. Command mode
calls it when a host finalizes a stream that wrote something and no write failed
(`command-mode.md` 5.4). A device without the callback answers PB_OK.

It is for a medium where what was written must be made the one that is used: an eMMC's boot
partition, which becomes the one the ROM boots from. The switch is made only here, after the
whole write, so that a write that is cut off leaves the old one in use.

- A partition doesn't inherit it: it is the device's.
- A driver may prepare for it in the write (the eMMC's driver moves the ROM's choice off the
  boot partition that is being written), and says so in its header.

### 3.5 Function : bio_install_partition_table() [optional]

```
Argument : const uuid_t part_uu, int variant
Return   : int, PB_OK or -PB_ERR_*
```

Writes the board's layout of that variant to the device (`board.md` 2.4,
`command-mode.md` 5.3), and the core then puts the new table into effect (section 4). The
callback is the partition table's, set on the device the table is on. Without one:
`-PB_ERR_NOT_SUPPORTED`.

## 4. Partition tables

A table reads a device and makes its partitions. Two: the GPT (`src/drivers/partition/gpt.c`,
the format is the UEFI specification's) and the static table, which is the board's data for a
device with no table on it (`board.md` 2.4).

- **A table on the medium is input** (`security.md` 3). Every entry is checked before a
  partition is made of it: inside the device, outside the table's own blocks, first not after
  last, values that fit `lba_t`. An entry that fails is skipped and logged, an empty entry is
  skipped, and the others are made. **Not yet in the code (F-193, F-192, F-006)**: an entry
  over the table becomes a writable partition, and an empty first entry ends the list.
- **A table can't make pb jump to anything.** It names where images are; the boot verifies
  them (`security.md` 3). It can hide a system or point two UUIDs at the same blocks; pb then
  ends in command mode.
- **Repair writes only what is bad** (F-195), and only a copy whose place the good copy gives.
  A device whose table can't be repaired has no partitions; command mode installs one
  (`board.md` 2.1).
- **A table that is installed is in effect at once** (Jonas, D-070). `command-mode.md` 5.3 asks
  it: the commands that follow the install see the new partitions. A partition's handle is
  kept where `bio` can't see it: command mode's stream, the boot source policy, a board's own
  variables (the host board's fuse box keeps one from `late_init()`). So the rule starts
  with the handle:
  1. **A handle never comes to mean another partition.** A partition that the new table
     leaves unchanged, with the same UUID and the same blocks, keeps its handle and its
     flags. One that changed or is gone is released: its handle is dead, `-PB_ERR_PARAM` for
     whoever still holds it, and nothing else gets that handle before the reset. A new
     partition gets a new one.
  2. **The core sets up again what is the core's**: command mode's stream ends, and the boot
     source policy's init runs again, for its state's partitions and the candidates.
  3. **No hook of the board's runs again.** What a board looked up in `late_init()` stays
     right for the partitions that didn't change, and fails loudly for the ones that did,
     until the reset. A board that wants a new partition at once looks it up by its UUID
     when it uses it.

  The pool has room for the old partitions and the new ones. When it doesn't, the table is
  on the device, the install answers the error, and a reset uses the table.

  **Not yet in the code (F-194)**: the old partitions stay until a reset, listed and written
  at their old blocks, and a device that had no table has no partitions until then.

  Considered and not taken: running again what `board_setup()` does after `init()`,
  `late_init()` too. A stale value in a board is then not only a handle (a size, what it
  read from a partition, what it registered), and a `late_init()` that can't run twice would
  show only at an install. And: after a reset only, as released, with the old partitions
  released at the install; that ends the flows that reinstall a table and go on writing.
- **The static table** is checked like the board's other data (`board.md` 3): entries inside
  the device, not overlapping, sizes in whole blocks.

## 5. What a driver leaves behind

- **Nothing is in flight** when a call returned (section 3), so a storage driver has no `stop`
  (`drivers.md` 2.3). The core has nothing to stop on the device it loaded from
  (`platform.md` 4.5).
- **The device stays as the last call left it** at the jump: its bus mode, the partition an
  eMMC has selected, the controller enabled. The next stage sets the device up again and
  relies on nothing of it.
- **The ROM can boot from it after every reset** (`platform.md` 2.1). A driver that puts the
  medium in a mode the ROM can't start from says in its header what brings it back: the
  device's reset pin, which is the board's to wire and to configure, or a reset the platform
  makes cold. Unverified on jiffy (F-078, F-079).
- **What a driver changes on the medium for good** (an eMMC's boot bus conditions, the boot
  partition that is enabled) is named in its header, with the configuration that keeps the
  medium's own.

## 6. Fixed (compatibility)

For a product in the field (D-008, `system.md` 5.1):

- The flags' low byte, bit for bit, and what each means (`command-mode.md` 7,
  `compatibility.md`: `abi.txt` holds the values).
- What the partition table's read reports for a partition: the first and the last block on
  its parent, the block size.
- A host that erases a device with `ERASE_BEFORE_WRITE` and then writes it gets its bytes
  written, also when `bio_write()` erases by itself one day (3.3). That a write doesn't erase
  is not fixed (Jonas, D-067).
- An eMMC's boot partition becomes the ROM's choice when a stream that wrote it is finalized
  (3.4), not before.
- The devices' and the partitions' UUIDs are the board's (`board.md` 8).

`bio`'s functions and a driver's callbacks are the porting API: they change with a major
release at most (D-018).

## 7. Review checklist

For a new storage driver or partition table, and for a change to one or to `bio`:

- [ ] `drivers.md` 7.
- [ ] Each device is registered once, with the board's UUID, a size read from the medium and
      checked, and no more flags than the medium allows.
- [ ] The callbacks take whole blocks, read and write exactly `length` bytes of the buffer,
      and split what the controller can't take at once.
- [ ] When a write returns PB_OK the data is on the medium.
- [ ] An erase erases exactly the blocks named; the header gives its time per block.
- [ ] The header says what the driver leaves behind (section 5) and what it changes for good.
- [ ] A user takes the block size from `bio_block_size()`, checks every result, and uses
      nothing of a failed read. One that lists the devices skips the handles that aren't one.
- [ ] A table checks every entry before it makes a partition of it.
- [ ] After an install an unchanged partition has its handle and its flags, a changed one's
      handle is dead and not given out again, and nothing of a board's ran.
- [ ] Nothing of section 6 changed.
- [ ] The conformance suite runs against the new driver or its model (section 8).

## 8. Conformance

Where each driver is (2026-11-12). Read in the code, not run, unless a test is named.

| Requirement                  | MMC, SD (uSDHC)    | FlexSPI NOR       | Host disk, RAM disk | Finding |
| ---------------------------- | ------------------ | ----------------- | ------------------- | ------- |
| A second `init` adds no      | no                 | no                | no                  | F-283   |
| devices                      |                    |                   |                     |         |
| The medium's size checked    | no                 | n/a (the board's) | n/a                 | F-200   |
| Exactly `length` bytes of    | yes                | no: a write reads | yes                 | F-284   |
| the buffer                   |                    | 1 to 4 bytes more |                     |         |
| A length of 0                | yes: the core's    | yes               | yes                 |         |
|                              | (`test_mmc_emmc`)  |                   |                     |         |
| Any length in one call       | yes (chunks)       | no: below 64 KiB  | yes                 | F-198   |
| Addresses above 4 GiB        | no: truncated      | n/a (no DMA)      | n/a                 | F-200   |
| refused                      |                    |                   |                     |         |
| Every wait bounded           | partly             | no                | n/a                 | F-200,  |
|                              |                    |                   |                     | F-198,  |
|                              |                    |                   |                     | F-281   |
| The codes of `drivers.md` 3  | no: a command      | no                | yes                 | F-199,  |
|                              | timeout is IO      |                   |                     | F-284   |
| Erase: exactly the blocks    | n/a (no erase)     | no                | n/a (no erase)      | F-198   |
| `write_done`                 | yes (`test_mmc_`   | n/a               | n/a                 |         |
|                              | `core`)            |                   |                     |         |
| Flags no wider than the      | yes                | yes               | yes                 |         |
| medium                       |                    |                   |                     |         |
| What it leaves behind, in    | no                 | no                | n/a                 | F-285   |
| its header                   |                    |                   |                     |         |

The virtio block driver has no user and is in no build (F-052); it isn't in the table.

The core:

| Requirement                                         | Today                        | Finding |
| --------------------------------------------------- | ---------------------------- | ------- |
| A length that isn't whole blocks is the core's      | yes (the suite, below)       |         |
| A length of 0 doesn't reach the driver              | yes (`test_bio`)             |         |
| A buffer that doesn't start on a line is refused    | no                           | F-282   |
| A block larger than the block buffer is refused     | yes (`test_bio`)             |         |
| A partition lies inside its parent                  | yes (`test_bio`)             |         |

The partition tables:

| Requirement                         | GPT                  | Static | Finding             |
| ----------------------------------- | -------------------- | ------ | ------------------- |
| Every entry checked                 | no                   | partly | F-193, F-192, F-006 |
| The device's block size             | no: 512 in four      | yes    | F-283               |
|                                     | places               |        |                     |
| Repair writes only the bad copy     | no                   | n/a    | F-195, F-191, F-064 |
| An installed table in effect        | no                   | n/a    | F-194               |
| Tested on the host                  | yes (`test_gpt`,     | no     | F-230               |
|                                     | `test_gpt.py`)       |        |                     |

**The conformance suite** (`process.md` 7): one set of tests
for section 3, run on the host against every device that can run there: the RAM disk, the host's
disk, and the MMC and SD drivers on their card models through the uSDHC's model. The fakes pass
the same suite, so that none is more forgiving than a driver (F-182 hid behind two that wrote
exact byte counts). The FlexSPI's driver has no model: the bench, through the simple target
(D-025).

`tests/unit/conformance/bio_conformance.c` has its start: a length that isn't whole blocks, a
length of 0, a range outside the device, a length of more than a driver's chunk. It runs
against the RAM disk (512 byte and 4 KiB blocks, and a partition), the host's disk and its
flash (`test_bio_conformance`), and `mmc_core` on the e.MMC and the SD model (`test_mmc_emmc`,
`test_mmc_sd`) and through the uSDHC's driver and model (`test_imx_usdhc`). The RAM disk and
the host's devices refuse what a driver never gets: a length that isn't whole blocks, or 0.
**Not yet in the code (F-283)**: the rest of section 3: an erase, `write_done`, a device
without a callback, a driver's errors and their codes, a write that fails half way.

## 9. Not yet in the code

| Finding             | What                                                                           |
| ------------------- | ------------------------------------------------------------------------------ |
| F-283               | The block size asked, not assumed; devices registered once; the rest of the    |
|                     | conformance suite (2, 8)                                                       |
| F-284               | The FlexSPI's driver: the buffer's bounds, lengths, results that are ignored   |
| F-198               | The FlexSPI's erase and its reads of 64 KiB and more (3.3)                     |
| F-285               | The MMC drivers: what they leave behind and change for good, in the header (5) |
| F-199, F-200, F-281 | Timeouts and their codes; sizes from the card; addresses above 4 GiB           |
| F-193, F-192, F-006 | A table's entries are checked (4)                                              |
| F-195, F-191, F-064 | The GPT's repair and its backup's place (4)                                    |
| F-194               | An installed table is in effect at once (4)                                    |
| F-078, F-079        | The ROM boots from the eMMC after a warm reset (5)                             |
