# The drivers' contracts: every driver

**State: draft (2026-11-12), D-064.** Level 3 (`process.md`).

What a driver (`src/drivers/<class>/`, `include/drivers/<class>/`) is, and what every driver does
whatever its class: when it is set up and stopped, how long it may wait, what it returns, and the
one rule for DMA and caches. The three classes with several implementations have a contract each,
which builds on this one:

| Class                      | Contract    | Implementations                                        |
| -------------------------- | ----------- | ------------------------------------------------------ |
| Block devices              | `bio.md`    | MMC and SD on the uSDHC, the FlexSPI's NOR, virtio     |
|                            |             | block, the host's disk file, the tests' RAM disk       |
| Crypto backends            | `crypto.md` | The CAAM, Mbed TLS                                     |
| USB device controllers and | `usb.md`    | ChipIdea, DWC3, Cadence USB3; the host platform's      |
| command mode's transport   |             | socket                                                 |

In the style of `platform.md`. Vendor neutral: no fact of one IP block is in a rule; where each
driver stands is in the conformance tables (section 8 here, and each class's own).

Writing or changing a driver: read this, the class's contract when it has one, then go through
the checklist (section 7).

**Not yet in the code (F-nnn)** marks what the code doesn't do yet. The commit that fixes the
finding removes the marker and updates the conformance table.

**Open (n)**: a proposal that isn't confirmed. The five, each marked in the text:

| Open | Question                                            | Proposal                                    |
| ---- | --------------------------------------------------- | ------------------------------------------- |
| 1    | One `drivers.md`, or a file per class (asked        | This document for what is common, and one   |
|      | 2026-11-11, not answered)                           | per class: each has its own entries,        |
|      |                                                     | checklist and conformance table             |
| 2    | Where the DMA rule lives in the code (4.4)          | `include/pb/dma.h`: the line size, a macro  |
|      |                                                     | for a buffer that owns its lines, three     |
|      |                                                     | functions. Drivers stop calling the arch's  |
|      |                                                     | cache functions                             |
| 3    | A caller's buffer that doesn't own its lines (4.3)  | The class's core bounces it through a       |
|      |                                                     | buffer of its own. A driver never sees one, |
|      |                                                     | and no image is refused for it (D-008)      |
| 4    | The line size (4.1)                                 | A constant, 64 bytes, checked against the   |
|      |                                                     | core's own in `arch_init()`                 |
| 5    | Which drivers have a `stop` (2.3)                   | Those whose device acts without being       |
|      |                                                     | asked. A driver whose calls are synchronous |
|      |                                                     | has nothing to stop                         |

## 1. Scope

- **A driver is the code for one IP block**, shared by every SoC that has the block. The SoC's
  and the board's facts reach it as data: a base address, a clock rate, a bus width, a quirk
  (`platform.md` 1). It includes `include/pb/` and its own class's headers, and nothing of a
  platform's or a board's. That holds today: no file under `src/drivers/` includes one.
- **Who sets one up**: whoever selects the peripheral (`platform.md` 7). The platform: what
  every board of the SoC needs (the timer, the watchdog, the fuses, the TrustZone filter, DRAM,
  the crypto engine, command mode's USB). The board: what the PCB has (storage, the UART).
- **Clocks, pads and power are not the driver's.** Whoever sets it up has done them before
  `init`.
- **What gets a contract** (`process.md` 2): a class with several implementations, or one that
  security rests on. The others have their header for a contract, and this document: the DRAM
  controller and PHY (`imx8m-ddr.md` records that port's work), the fuse controller, GPIO, the
  pin controller, the timer, the watchdog, the UARTs, the TrustZone filter's driver, the ROM's
  logs. What the core needs from them it asks the platform for (`platform.md` 3, 4).
- **A second implementation is the moment a class gets its contract.** Until then a new driver
  doesn't wait for a document (`process.md` 1, D-063).

Not here: which driver a board or a platform selects (`board.md` 2, `platform.md` 2.2, 3.10),
the partition tables' formats (`bio.md` 4), the wire (`compatibility.md`).

## 2. Lifecycle

pb takes no interrupts: a driver polls its device, and enables none of the device's interrupt
requests. Nothing runs concurrently with a driver's call.

### 2.1 Function : `<driver>_init()` [mandatory]

```
Argument : the SoC's and the board's facts: a base address, a configuration
Return   : int, PB_OK or -PB_ERR_*
```

Called by whoever selects the peripheral, once its clocks, pads and power are set up.

- **Never on the boot path for what only command mode uses** (D-023, `principles.md` 4):
  command mode's USB is set up when its task first runs (`platform.md` 3.10). What the boot uses
  is set up in `plat_init()` or the board's `init()`, and does there only what the boot needs: a
  driver defers what can wait (a faster bus mode that the load doesn't need isn't negotiated
  before the load).
- It resets the device to a known state first: the ROM, a serial download or a boot that
  stopped before may have left it running.
- It registers what the class's core needs (a block device, the backend's operations, the
  controller's operations). Nothing is registered when it fails.
- **Failure** leaves the device quiet: no DMA, nothing started. What follows is the caller's:
  the platform's failure (`platform.md` 2.2) or the board's (`board.md` 2.1).
- A driver whose class restarts it (the transport, `usb.md` 2) can be set up again after its
  `stop`.

### 2.2 Calls

Every call of a driver either completes before it returns, or is polled:

- **Synchronous**: when the call returns, the device has done the work or the call failed. The
  device is idle: nothing of this call is in flight (a block read, a signature check).
- **Polled**: a call starts the work and returns; a second one says whether it is done, with
  `-PB_ERR_AGAIN` while it isn't. The class's contract names each such pair and says how long
  the caller's buffer belongs to the device (a USB transfer, a hash that is fed while the next
  chunk is read).

No driver calls back into the core, and none calls `plat_wdog_kick()` (3).

### 2.3 Function : `<driver>_stop()` [mandatory when the device acts without being asked]

**Open (5).**

```
Argument : void
Return   : int, PB_OK or -PB_ERR_*
```

Before the jump nothing of pb's runs on in a device (`platform.md` 4.5, "Quiesce all DMA
capable devices"). Whoever started a driver stops it (`board.md` 5.3): the core its transport,
the platform what `plat_init()` started, the board what its hooks started.

A driver has a `stop` when its device would otherwise go on by itself: a USB device controller
that is attached to the bus, a controller with a transfer queued, a device that raises an
interrupt request the next stage didn't ask for. When `stop` returns:

- The device starts no DMA and completes none: it is halted or held in reset.
- It holds no address in pb's memory that it would use on a later event. The next stage
  reuses that memory (F-204: a SETUP packet after the handover is written into the OS's memory).
- It is detached from what it was attached to (the USB pull-up is off), so the other side sees
  it leave.
- Calling it twice, or before `init`, is not an error.

A driver whose calls are all synchronous has no `stop`: between two calls nothing is in flight.
What state it leaves its device in for the next stage (a card's bus mode, the partition that is
selected) is in the class's contract (`bio.md` 5).

A failed `stop` is a failed boot: pb doesn't jump with a device it couldn't stop. In a boot
command mode started, the transport is gone by then and pb resets (`board.md` 5.4).

## 3. Time and errors

**A failure is never a hang** (`system.md` 5.4).

- **Every wait is bounded.** A loop that waits for the device has a timeout and returns
  `-PB_ERR_TIMEOUT`. The bound is the driver's constant, with its source next to it: the IP
  block's manual, the medium's standard, or a measurement. **Not yet in the code (F-102,
  F-136, F-198, F-200, F-205, F-281)**: section 8 says where each driver stands.
- **No driver kicks the watchdog**, and neither do `pb_delay_*()` and `pb_timeout_*()`
  (`platform.md` 3.3): a driver that polls for ever must not keep the board alive. The core
  kicks between a driver's calls, once per chunk.
- **So one call is short.** A synchronous call returns within a second on every medium the
  driver supports, for the largest request the class's contract allows; a call that can't
  (erasing a large range) says in its header how long it takes per unit, and its caller splits
  the work (`command-mode.md` 4.6).
- **The timeout's arithmetic** is `pb_timeout_*()`'s. **Not yet in the code (F-211)**: a wait
  that spans the tick's wrap takes 71 minutes more.

What a driver returns:

| Code                    | Means                                                                    |
| ----------------------- | ------------------------------------------------------------------------ |
| `PB_OK`                 | Done, all of it. There is no partial success                             |
| `-PB_ERR_AGAIN`         | A polled call isn't done yet. From nothing else                          |
| `-PB_ERR_TIMEOUT`       | The device didn't answer within the driver's bound                       |
| `-PB_ERR_IO`            | The device answered with an error, or what it sent failed a check        |
| `-PB_ERR_PARAM`         | The caller's arguments: a range, a length or an id the device can't take |
| `-PB_ERR_ALIGN`         | A buffer that isn't aligned as the call requires (4.3)                   |
| `-PB_ERR_NOT_SUPPORTED` | The device or the driver doesn't have the operation                      |
| `-PB_ERR_STATE`         | Called out of order: before `init`, or while another operation runs      |

- The code is part of the interface: callers branch on it (F-199: an SD card's version is told
  by a timeout, and the uSDHC returns `-PB_ERR_IO` for one).
- A driver logs the cause once, with `LOG_ERR`, where it is detected. It never logs a secret
  (`security.md` 4.10).
- After an error the driver is usable: the next call starts from a known state, and nothing of
  the failed one completes later (F-181: a hash job that was given up answers the next
  operation's wait).

## 4. DMA and caches

One rule for every driver whose device reads or writes memory itself. F-081, F-134, F-184,
F-205 and F-220 are each this rule broken in one place.

pb maps memory one to one and there is no IOMMU: the address the CPU uses is the address the
device gets. After `plat_init()` the D-cache is on (`platform.md` 2.2): what the CPU wrote may
be in the cache only, and what a device wrote isn't in the cache.

### 4.1 Words

| Word | Means                                                                                |
| ---- | ------------------------------------------------------------------------------------ |
| Out  | The device reads memory the CPU wrote: data for a card, a descriptor, bytes to hash  |
| In   | The device writes memory the CPU reads afterwards: a block, a USB transfer, a status |
| Line | A cache line. `PB_DMA_ALIGN` bytes: no smaller than the line of any core pb runs on  |

**Open (4)**: `PB_DMA_ALIGN` is a constant, 64, and `arch_init()` checks that the core's line
isn't larger. An alignment must be known when a buffer is declared, so it can't be read from the
core at run time; the functions of 4.4 use the core's own line size for their stride.

### 4.2 The rule

1. **Out: clean, then start, then leave it alone.** The range is cleaned before the device is
   started, and the CPU doesn't write it again until the device is done with it. That goes
   for everything the device follows: a descriptor list with its terminating entry (F-184), a
   buffer the driver refills for the next job (F-220).
2. **In: nothing dirty at the start, invalidate at the end.** Before the device is started no
   line of the range is dirty; after it is done, and before the CPU reads the range, its
   lines are invalidated. Between the two the CPU doesn't touch them.
3. **What a device writes owns its lines.** The range starts on a line, and nothing else is in
   its last line. A line shared with other data has no right order: invalidating it discards
   the neighbour's bytes that were only in the cache (F-081, F-205), and cleaning it writes
   stale bytes over what the device wrote.
4. **Invalidate before the CPU's own writes, never after.** A driver that copies from a bounce
   buffer into the caller's buffer invalidates the bounce buffer, then copies. The copy is in
   the cache, and an invalidate after it discards it (F-134).
5. **A driver's own DMA memory owns its lines too**: descriptors, rings, event buffers, a setup
   packet, bounce buffers. Each is declared with the macro of 4.4, not next to the driver's
   other state.
6. **An address the device can't take is refused**, not truncated: a controller with 32-bit
   address fields and a buffer above 4 GiB (F-200) is `-PB_ERR_PARAM`.
7. **A bus master the SoC treats as non-secure** needs its memory open in the TrustZone filter.
   That is the platform's (`platform.md` 4.3), and a driver assumes nothing about it.
8. **Nothing is in flight at the jump**: 2.3.

**Not yet in the code (F-282)**: each driver calls the arch's cache functions itself, with its
own alignment (64, 4096, or none) and its own order.

### 4.3 The caller's buffer

**Open (3).**

A driver's device also reads and writes buffers that aren't the driver's: a block read into an
image's load address, a transfer into command mode's buffer, the bytes to hash.

- **Out** needs nothing of the buffer: cleaning whole lines harms no neighbour. The caller
  leaves the buffer unchanged for as long as the class's contract says (`crypto.md` 3.3).
- **In** needs a buffer that owns its lines (4.2, item 3). A buffer on the stack never does:
  the driver's own frames are its neighbours.

Who makes it so:

- **pb's own buffers** are declared with the macro of 4.4: command mode's command and its two
  buffers, the image header, the partition tables, the A/B state's blocks.
- **A buffer the core is handed** (an image part's load address, an out-of-tree board's read)
  is checked by the class's core, not by each driver. One that starts on a line and whose
  length is whole lines goes to the driver as it is: whole blocks are whole lines. Any other
  is bounced by the core through a buffer of its own that owns its lines, a block or a chunk
  at a time, and copied.
- **A driver therefore sees only buffers that own their lines**, and returns `-PB_ERR_ALIGN`
  for one that doesn't start on a line. It bounces only for what its own device needs beyond
  that (a controller that needs page alignment).
- **No image is refused for its load addresses** (D-008): an image whose part starts inside a
  line loads through the bounce buffer, slower.
- **Small results** (a digest, a status byte) are written by the device into the driver's own
  memory and copied to the caller.

### 4.4 In the code

**Open (2). Not yet in the code (F-282).**

`include/pb/dma.h`, built on the arch's cache functions. A driver includes it and no arch
header:

| Name                         | Does                                                             |
| ---------------------------- | ---------------------------------------------------------------- |
| `PB_DMA_ALIGN`               | The line size buffers are declared with (4.1)                    |
| `PB_DMA_BUFFER(type, name)`, | Declares an object, or an array, that starts on a line and is    |
| `PB_DMA_ARRAY(...)`          | padded to whole lines, in `.no_init`                             |
| `dma_out(addr, length)`      | Before the device reads: clean                                   |
| `dma_in_begin(addr, length)` | Before the device writes: nothing dirty in the range             |
| `dma_in_end(addr, length)`   | After the device wrote, before the CPU reads: invalidate         |
| `dma_owns_lines(addr, len)`  | Whether a range starts on a line and is whole lines (4.3)        |

- `dma_in_begin()` and `dma_in_end()` take only ranges that own their lines, and say so with
  an assertion in a debug build.
- `.no_init` is in `buffers`, which works after `plat_init()` (`platform.md` 5.1): a driver that
  runs before it (the console's UART) does no DMA.
- **The arch gives** clean, invalidate, and clean and invalidate, by range, with the start
  rounded down to a line and the end up, each ending with the barrier the architecture
  requires; and a D-cache with nothing valid in it when it is enabled. **Not yet in the code
  (F-282, F-207)**: the three are declared in each arch's own header, which the drivers include
  by its path; clean and invalidate exists on ARMv8-A only; the D-cache is enabled without
  being invalidated first.
- On the host the three do nothing, so no host test sees a missing clean or invalidate. This
  rule is checked by review and on the bench.

## 5. What a driver trusts

- **What comes from the other side is input** (`security.md` 3): the bytes on the storage, the
  host's requests, and what a device reports about itself. A driver checks every length,
  count and offset it takes from them before it uses one: a card's capacity (F-200), a
  partition table's entries (F-193), a control request's `wLength`.
- **The rules are the core's** (`security.md` 4.8). A driver reads, writes, hashes and
  verifies what it is told to. It decides nothing about the life cycle, the keys, the gate or
  which image boots, and it has no way around the core's checks.
- **A driver's verdict is its own operation's.** It never reports a result it didn't compute
  for this call (`crypto.md` 4).

## 6. Fixed

Nothing of a driver's own interface is fixed like the wire: the classes' operations change
with a major release at most, as the porting API does (D-018). What a class fixes for a
product in the field is in its contract (`bio.md` 6, `usb.md` 6).

## 7. Review checklist

For a new driver, and for a change to one:

- [ ] It includes nothing of a platform's or a board's; the SoC's facts are arguments.
- [ ] `init` is off the boot path unless the boot needs the device, and does only what the
      boot needs.
- [ ] Every wait has a bound with its source, and returns `-PB_ERR_TIMEOUT`.
- [ ] No call kicks the watchdog, and a synchronous call returns within a second.
- [ ] The return codes are section 3's; after an error the next call works.
- [ ] Out: cleaned before the device starts, descriptors with their last entry; not written
      until the device is done.
- [ ] In: begun and ended with `dma_in_begin()` and `dma_in_end()`; the end comes before any
      copy.
- [ ] Every DMA object of the driver's is declared with the macro. Nothing is written by a
      device into the stack or next to other data.
- [ ] Lengths and counts from the medium, the host or the device are checked before use.
- [ ] A device that acts without being asked has a `stop`, which can be called twice.
- [ ] The class's checklist and conformance table (`bio.md`, `crypto.md`, `usb.md`), and
      section 8 here.

## 8. Conformance

Where each driver is against sections 2 to 4 (2026-11-12). Read in the code, not run. What a
class asks beyond them is in its own table (`bio.md` 8, `crypto.md` 9, `usb.md` 8).

| Driver                  | Every wait bounded    | DMA memory owns lines   | `stop`              | Finding       |
| ----------------------- | --------------------- | ----------------------- | ------------------- | ------------- |
| MMC, SD and the uSDHC   | partly: the card's    | partly: aligned, in     | n/a: synchronous    | F-200, F-281  |
|                         | state is polled       | `.no_init`, next to the |                     |               |
|                         | without one           | driver's state          |                     |               |
| FlexSPI                 | no                    | n/a: no DMA             | n/a: synchronous    | F-198         |
| CAAM                    | yes: 1 s              | no                      | none, and its rings | F-081, F-286  |
|                         |                       |                         | name pb's memory    |               |
| Mbed TLS                | n/a                   | n/a                     | n/a                 |               |
| ChipIdea                | no                    | yes                     | yes, unbounded      | F-136         |
| DWC3                    | yes                   | partly: aligned, not    | yes                 | F-282         |
|                         |                       | padded                  |                     |               |
| Cadence USB3            | no                    | no                      | empty               | F-205, F-204  |
| The DRAM controller and | no                    | n/a                     | n/a                 | F-102         |
| PHY                     |                       |                         |                     |               |
| virtio (in no build)    | no                    | partly                  | none                | F-052, F-281  |

Every driver with DMA calls the arch's cache functions itself, through the arch's own header
(F-282). The other single drivers (the fuse controller, GPIO, the pin controller, the timer,
the watchdog, the UARTs, the TrustZone filter's, the ROM's logs) do no DMA; their waits are
unchecked here, and F-197 and F-206 are what is known of them.
