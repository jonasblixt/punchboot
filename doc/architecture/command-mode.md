# Command mode

**State: draft (2026-11-12), D-057.** Level 2 (`process.md`).

What pb does for a host on the cable: when command mode runs, what a session is, which commands
a host may run and when, what each command means and what a failure leaves behind.
`compatibility.md` freezes the bytes on the wire; this freezes the behaviour. In the style of
`boot.md`, which it builds on. Vendor neutral.

Changing command mode, a command or the gate: read this, `security.md` 4.7 and
`compatibility.md`, then go through the checklist (section 8). Section 7 is fixed like the wire.

**Open (n)** marks a proposal that isn't confirmed (`process.md` 3). **Not yet in the code
(F-nnn)** marks what the code doesn't do yet. The commit that fixes the finding removes the marker.

| Open | Question                                                        | Proposal                                  |
| ---- | --------------------------------------------------------------- | ----------------------------------------- |
| 1    | A build without `CONFIG_CM_AUTH` has no gate: its command mode  | The gate is always in the build; the      |
|      | is open on a locked device (3.2, F-271)                         | option goes, the methods stay options     |
| 2    | What keeps a partition from an authenticated host:              | A hidden partition doesn't exist for the  |
|      | `part verify` tells what any partition holds, hidden ones       | host; `READABLE` only gates the bulk read |
|      | too (5.3, F-186, F-272)                                         |                                           |
| 3    | An idle timeout for an authenticated session (3.1)              | None: the session ends with the transport |
| 4    | Guessing a password over the wire (3.3)                         | A failed attempt is answered after 1 s    |
| 5    | The classes a method may grant (3.4)                            | Six, in 3.4's table                       |
| 6    | A host that stops in the middle of a command; the timeouts in   | A transfer timeout that ends the session; |
|      | the caps (4.6)                                                  | the caps carry pb's timeouts              |
| 7    | `boot bpak` when the boot is refused before the image is read   | The header is always read, and its        |
|      | (5.5, F-128)                                                    | result carries the refusal                |
| 8    | A locked device answers `INVALID_COMMAND` before authentication | Keep, as released                         |
|      | (3.2)                                                           |                                           |

What this draft asks of the documents beside it:

- `board.md` 4.4 doesn't name the reset a board's command can ask for (5.6).
- `compatibility.md`, "Fixed elsewhere", has a line for section 7.

## 1. Words and scope

| Word         | Means                                                                                     |
| ------------ | ----------------------------------------------------------------------------------------- |
| Command mode | pb's handling of a host, while pb is stopped (`system.md` 2)                              |
| Host         | Whatever is at the other end of the transport. Untrusted (`security.md` 3)                |
| Transport    | What carries the bytes: USB, or the host platform's socket in the tests (`board.md` 4.2)  |
| Session      | From the transport being ready until it is lost: one host, its authentication, its stream |
| Gate         | The one check that decides whether a command runs (`security.md` 4.7)                     |
| Exchange     | A command frame, the transfers that belong to it, and the results                         |
| Stream       | The partition a host selected for reading or writing, and the two buffers                 |

Not here: the bytes (`compatibility.md`, `include/pb-tools/wire.h`), the boot a command starts
(`boot.md` 6, `board.md` 5.4), the life cycle's states and order (`system.md` 4, D-027), the
board's own commands (`board.md` 4.4), the transport's driver (the drivers' contracts, later),
the host tools (`arch-host-tools.md`).

The rules it implements are level 1's: who may use command mode in which state (`system.md` 4,
`security.md` 4.7), what a host can reach (`security.md` 2), that a failure is never silent and
never a hang (`system.md` 5.4), and that the wire means what it meant (`system.md` 5.1).

Command mode is in the trusted computing base (`security.md` 1): it runs with pb's privilege,
and on a locked device the frame parser, the gate and the transport's stack are what an attacker
with a cable reaches. That is why it stays small: one transport, no negotiation beyond the caps,
fixed frames.

## 2. The flow

```
main()
  the boot fails, is refused or is stopped        boot.md 2, board.md 2.1, 5
  stopped: the tasks in turn                      board.md 4.6
    cm_main_task()
      its first call: set up                      2.1
      no session: wait for the transport          2.2
      a session: a command, when one has come     3, 4
  a task ended the loop: plat_reset()
```

**Not yet in the code (F-268, F-267)**: `cm_run()` is a blocking loop that `main()` calls, and
the board sets up the transport.

Command mode is one task (D-051). It never runs on the boot path, and nothing of it is set up
there (`principles.md` 4, D-023). A build without it (`CONFIG_CM` off) stops the same way and
has no host (`system.md` 2).

### 2.1 Set up

On the task's first call, in this order:

1. `plat_cm_init()`: the USB device port (`platform.md` 3.10).
2. What only command mode reads: the logs' sources that aren't pb's own (5.7).
3. The board's data that command mode uses is checked (`board.md` 3).
4. The life cycle is read, for the gate (3.2). A read that fails is not a failure of the set up:
   the life cycle is unknown and the gate is closed (`security.md` 4.4).
5. The transport is started.

A failure in 1, 3 or 5: no command mode. The task ends the loop and pb resets
(`platform.md` 3.10, `board.md` 3).

Command mode also runs after a failed `board_setup()` (`board.md` 2.1), so it relies on what
`plat_init()` set up and on nothing of the board's that wasn't checked. It doesn't run after a
platform failure: pb resets (`platform.md` 2, F-235).

### 2.2 Waiting for a host

Without a session the task polls the transport. When the transport isn't ready within
`CONFIG_CM_TRANSPORT_READY_TIMEOUT` (10 s), the task ends the loop and pb resets
(`system.md` 2): a device in the field that stopped on a fault boots again without a host.

The time runs whenever there is no session: from the set up, and again from the moment a
session ends (3.1). A host that unplugs leaves a device that resets 10 s later.

### 2.3 Leaving

| How                                                | What happens                                                 |
| -------------------------------------------------- | ------------------------------------------------------------ |
| `device reset`                                     | The result is sent, the transport is disconnected, pb resets |
| A board's command asked for a reset (5.6)          | The same, after the command's last result                    |
| A boot the host started (5.5)                      | The transport is disconnected, then the jump                 |
| No host in time (2.2)                              | pb resets                                                    |
| A boot the host started fails after the disconnect | pb resets (`board.md` 5.4)                                   |

Command mode never leaves by itself to boot from storage: a reset does that.

## 3. The session and the gate

### 3.1 The session

A session starts when the transport is ready (USB: the host configured the device) and ends
when the transport is lost or restarted: an unplug, a bus reset, a transfer that failed, a
transfer the host didn't complete (4.6). There is one session at a time and one host.

A session's state is: whether it is authenticated, and by which method (3.4); the stream (5.4).
Both are cleared when a session starts. The next host on a device that stayed powered is not
authenticated and has no stream. **Not yet in the code (F-185, F-201)**: `authenticated` and
the stream survive a replug and a bus reset.

What isn't the session's: the life cycle (the device's), the buffers' content (4.3).

**Open (3)**: no idle timeout. An authenticated session stays authenticated for as long as the
transport is up. Taking the cable over means a bus reset, which ends the session; a timeout
would only protect a host that walked away from its own cable, and would cost every long
factory flow a new authentication.

### 3.2 The gate

Every command passes the gate before any of its code runs, the board's commands and status too
(`security.md` 4.8). The gate is one function of the core:

```
the frame's magic is right and the command is one of this build's    else INVALID_COMMAND
the command is open before authentication (class identity, 3.4)       run
the device is open                                                    run
the session's method grants the command's class (3.4)                 run
                                                                      else NOT_AUTHENTICATED
```

Whether the device is open follows from the life cycle alone (`system.md` 4):

| Life cycle                                       | The device is | Why                                    |
| ------------------------------------------------ | ------------- | -------------------------------------- |
| `NOT_CONFIGURED`, `CONFIGURATION`                | Open          | The ROM runs any pb (`security.md` 3)  |
| `CONFIGURATION_LOCKED`                           | Closed        | The product in the field               |
| `EOL`                                            | Open          | The SoC's state (D-011, `system.md` 4) |
| Unknown: the read failed, or it was never set up | Closed        | `security.md` 4.4                      |
| The board has no life cycle                      | Open          | `system.md` 4                          |

- **Before authentication a closed device answers two commands**: `DEVICE_IDENTIFIER_READ` and
  `AUTHENTICATE` (`security.md` 4.7). The set never grows.
- **The gate follows the life cycle at once.** The life cycle is read again after every command
  that changes it. A session that locks the device is behind the gate from its next command, as
  every later session is.
- **A refused command runs nothing and changes nothing**, and the exchange still ends as the
  host expects (4.4).
- **An unknown life cycle** closes the gate, and a boot is refused whoever asks (`system.md` 4,
  `boot.md` 2). Without the root of trust no token verifies (`board.md` 2.1); a password can
  still open the session.

**Open (1)**: `CONFIG_CM_AUTH`. **Not yet in the code (F-271)**: without the option
`cm_run()` starts every session as authenticated, so such a build has an open command mode on
a locked device, against `security.md` 4.7 and 2 (items 2 and 4). Proposal: the gate is always
compiled in and `CONFIG_CM_AUTH` goes. The methods stay options (`CONFIG_CM_AUTH_TOKEN`,
`CONFIG_CM_AUTH_PASSWORD`). A build with no method answers `NOT_SUPPORTED` to `AUTHENTICATE`:
once locked, its command mode is closed for good, which is a product's choice (the imx8mevk's
and the 8MP EVK's defconfigs today). Every defconfig in the tree has the option on. Out of the
tree: Jonas.

**Open (8)**: the frame is checked before the gate, so a closed device answers
`INVALID_COMMAND` to a command its build doesn't have, and `NOT_AUTHENTICATED` to one it has.
That tells an unauthenticated host which commands the build knows, so roughly its version.
Proposal: keep it. Released devices do it, hosts read `INVALID_COMMAND` as "not supported"
(`compatibility.md`), and the version isn't what 4.7 protects.

### 3.3 Authentication

`AUTHENTICATE` names a method, a key and the size of the data, and the data follows in one
transfer of 1024 bytes (4.4). Methods are added, never changed (`security.md` 4.7, D-008).

| Method                               | The data                           | Verified by                   |
| ------------------------------------ | ---------------------------------- | ----------------------------- |
| Token (`CONFIG_CM_AUTH_TOKEN`)       | A signature over the device's UUID | The core, with a keystore key |
| Password (`CONFIG_CM_AUTH_PASSWORD`) | The password, `size` bytes         | The board (`board.md` 4.5)    |

- **The token**: the UUID as its 36 character string, hashed with the hash that goes with the
  key (SHA-256 for P-256, SHA-384 for P-384, SHA-512 for P-521), signed with the key the
  command names. The key is in the keystore and not revoked (`security.md` 4.3); a key whose
  status can't be read is unusable (4.4). Only EC keys; another kind answers `NOT_SUPPORTED`.
  The token is static (`security.md` 5).
- **The verdict is the verification's own.** A result left over from an earlier job of the
  crypto engine is never read as this one's. **Not yet in the code (F-181).**
- **Success** authenticates the session, with what the method grants (3.4).
- **Failure** leaves the session not authenticated, also one that was. It answers
  `AUTHENTICATION_FAILED`; for a token whose key is revoked `KEY_REVOKED`, and `ERROR` for a
  key the keystore doesn't have. That tells an unauthenticated host which key ids the build
  has and which are revoked, which the images' headers and the fuses tell too.
- **A method that isn't in the build**, or a size of 0 or above 1024: the first result is the
  error and no data follows.
- **Secrets**: the data is compared whole and in constant time, and never logged
  (`security.md` 4.10, F-004). The buffer that held it is not sent to a host (4.3).
- A device that is open accepts `AUTHENTICATE` like any command; it changes nothing a host
  notices.

**Open (4)**: a failed attempt costs nothing today, so a host can try passwords as fast as the
transport carries them. The token can't be guessed; the password's strength is the product's
(`system.md` 6). Proposal: the result of a failed attempt is sent 1 s after the data arrived,
with every method, the watchdog kicked meanwhile. No counter and no lockout: a lockout would
let anyone with a cable close the device for its service staff.

### 3.4 What a method grants

**Open (5).** Today a session is authenticated or not, and both methods grant everything. A
method added later may grant less: a token limited to reading the logs for a return analysis,
or one limited in time (`security.md` 4.7). The gate is written for that now, so that a new
method adds a row and changes no command:

- Every command has one class, in a table of the core's, next to the gate.
- The session records its method, and the method's grant is a set of classes.
- The token and the password grant every class, and always will (D-008).

| Class      | Commands                                               | Reaches                               |
| ---------- | ------------------------------------------------------ | ------------------------------------- |
| `identity` | `DEVICE_IDENTIFIER_READ`, `AUTHENTICATE`               | Open before authentication            |
| `read`     | `DEVICE_READ_CAPS`, `BOOTLOADER_VERSION_READ`,         | Changes nothing on the device         |
|            | `DEVICE_RESET`, `SLC_READ`, `PART_TBL_READ`,           |                                       |
|            | `PART_BPAK_READ`, `BOOT_STATUS`, `BOARD_STATUS_READ`,  |                                       |
|            | `LOG_READ`                                             |                                       |
| `storage`  | The `STREAM_*` commands, `PART_VERIFY`, `PART_ERASE`,  | The storage (`security.md` 2, item 4) |
|            | `PART_TBL_INSTALL`                                     |                                       |
| `boot`     | `BOOT_PART`, `BOOT_BPAK`, `PART_ACTIVATE`              | What runs next, among verified images |
| `security` | `SLC_SET_CONFIGURATION`, `SLC_SET_CONFIGURATION_LOCK`, | The root of trust's state             |
|            | `SLC_SET_EOL`, `SLC_REVOKE_KEY`                        | (`security.md` 2, item 2)             |
| `board`    | `BOARD_COMMAND`                                        | The board's commands (`board.md` 4.4) |

A retired command (5.8) has the class `read`. `PART_VERIFY` is `storage` and not `read`: it
tells what a partition holds (5.3).

## 4. The exchange

### 4.1 Frames and results

- The host sends a command: one frame of 512 bytes. The device answers with a result: one
  frame of 512 bytes. The device never sends anything unasked.
- A command may have data transfers, in either direction, whose sizes both sides know before
  the transfer starts: from the command, from a result, or fixed.
- **Every exchange ends with a result**, and the last result is the command's verdict.
- One exchange at a time. The device reads the next command when the last result is sent.

### 4.2 Host input

Host input is untrusted in every life cycle state, authenticated or not (`security.md` 4.7).

- Every size, offset, count, id and UUID from the host is checked before it is used, in the
  core. What the board's command gets is bounded by the core and checked by the board
  (`board.md` 4.4).
- A size is checked against the buffer and against the partition, with arithmetic that can't
  wrap.
- A reserved field (`rz`) is ignored. Zero there means the released behaviour
  (`compatibility.md`).
- No command takes an address in pb's memory, and none reads or writes memory the host names.

### 4.3 What pb sends

- A result's reserved bytes and every byte a command didn't produce are zero. **Not yet in the
  code (F-190)**: the identifier's result and the partition table's entries carry bytes of the
  command buffer, the identifier's before authentication.
- A data transfer carries the bytes the command produced and nothing else from the buffers.
  The buffers aren't cleared between sessions; nothing sends their old content.
- Secrets are never sent and never logged (`security.md` 4.10).

### 4.4 Sequences

`C` is the command, `R` a result, `>n` a transfer of n bytes from the host, `<n` one to the
host. The sequences are frozen (`compatibility.md`); this table says what they are.

| Command                 | Exchange                                    | After a first result that isn't OK |
| ----------------------- | ------------------------------------------- | ---------------------------------- |
| `AUTHENTICATE`          | `C R >1024 R`                               | Ends                               |
| `STREAM_PREPARE_BUFFER` | `C R >size R`                               | The host sends the data anyway     |
| `STREAM_READ_BUFFER`    | `C R <size R`                               | Ends                               |
| `PART_TBL_READ`         | `C R <n*128 R`, no data when n is 0         | Ends                               |
| `PART_BPAK_READ`        | `C R <4096 R`                               | Ends                               |
| `SLC_READ`              | `C R <128 R`                                | Ends                               |
| `LOG_READ`              | `C R`, and `<size R` when the size isn't 0  | Ends                               |
| `BOARD_STATUS_READ`     | `C R <size R`, a transfer also for size 0   | See below                          |
| `BOARD_COMMAND`         | `C R`, `>1024` when it has a request, `R`,  | The host sends the request anyway  |
|                         | and `<size R` when the size isn't 0         |                                    |
| `BOOT_BPAK`             | `C R >4096 R`, then for each part its bytes | Ends (5.5)                         |
|                         | and an `R`, then `R`                        |                                    |
| `BOOT_PART`             | `C R`, then the jump when it was OK         | Ends                               |
| `DEVICE_RESET`          | `C R`, then the reset                       |                                    |
| Every other command     | `C R`                                       |                                    |

- **The device keeps the exchange in step also when it refuses.** Where released hosts send
  data whatever the first result says (`STREAM_PREPARE_BUFFER`, `BOARD_COMMAND`), the device
  reads that data and sends the second result, with the refusal. That holds for the gate too.
  **Not yet in the code (F-187, F-274)**: the gate and the bounds checks send one result, and
  the data is then read as commands.
- **`BOARD_STATUS_READ` that fails**: the device sends the data and the second result after an
  error, and the Python host stops at the first result. Which side is right is the 2.x hosts'
  to say (F-274); until then the device's sequence stays.
- **A transfer that fails ends the session** (3.1): the two sides no longer agree on where a
  frame starts. The transport is restarted. **Not yet in the code (F-274)**: most write
  results are ignored, and the command goes on.
- **A frame that isn't a command** (a wrong magic) is answered with `INVALID_COMMAND`.

### 4.5 Results

The result's code is what the host acts on. What released devices answer is frozen with the
command's meaning (section 7).

| Code                             | When                                                                        |
| -------------------------------- | --------------------------------------------------------------------------- |
| `INVALID_COMMAND`                | The frame isn't a command, or the build doesn't have it. A host reads it as |
|                                  | "not supported by this device"                                              |
| `NOT_AUTHENTICATED`              | The gate refused (3.2)                                                      |
| `NOT_SUPPORTED`                  | The command is retired (5.8), or the build or the board lacks what it needs |
| `AUTHENTICATION_FAILED`          | 3.3                                                                         |
| `KEY_REVOKED`, `SIGNATURE_ERROR` | A key is revoked; a signature doesn't verify (3.3, `boot.md` 4.2)           |
| `NOT_FOUND`                      | No such partition or log, or no image header                                |
| `PART_NOT_BOOTABLE`              | `boot.md` 6                                                                 |
| `INVALID_ARGUMENT`               | A size, offset, count or id that the command can't take                     |
| `NO_MEMORY`                      | A buffer id or a size the buffers don't have                                |
| `STREAM_NOT_INITIALIZED`         | A stream command without a stream (5.4)                                     |
| `PART_VERIFY_FAILED`             | The hash differs, or the partition couldn't be read (5.3)                   |
| `IO_ERROR`                       | The storage failed, or the partition's flags forbid it (5.3)                |
| `TIMEOUT`, `MEM_ERROR`, `ERROR`  | A driver's timeout; pb's memory; everything else                            |

- An error that has no code of its own is `ERROR`: a key the keystore doesn't have, a payload
  whose hash differs (F-087). A new code is appended, and old hosts must take an unknown code
  as an error.
- A refusal by the life cycle's order (5.2) is `ERROR` today, and stays.
- The reason for an error is in pb's log, which the host reads (5.7).

### 4.6 Time and the watchdog

The watchdog covers command mode's commands (`system.md` 5.4).

- **Between commands** the core kicks once per round of the tasks (`platform.md` 3.3).
- **Inside a command** the core kicks where the work makes progress: once per chunk that is
  hashed, read, written or loaded, a chunk being at most a buffer. The longest time without a
  kick is then one chunk on the slowest medium, as on the boot path, and not the longest
  command. **Not yet in the code (F-189)**: nothing kicks inside a command, so a large
  `part verify` or `boot part` can outlast the watchdog.
- A board's command kicks as it makes progress (`board.md` 4.4). An erase is as long as its
  driver's erase of the blocks the host named; hosts erase in pieces.
- While a command runs, no other task runs (`board.md` 4.6).

**Open (6)**: a host that stops in the middle of an exchange. Today pb waits for the transfer
without a kick and without a timeout: with the watchdog the device resets, without one it
waits for ever, and no task runs. An unauthenticated host can do it (`AUTHENTICATE`, then no
data). Proposal:

- A transfer inside a command has a timeout, `CONFIG_CM_TRANSFER_TIMEOUT`, 10 s like the
  hosts' own. pb kicks while it waits. When the time is up the session ends (3.1) and the
  transport is restarted, so the device is back at 2.2.
- The caps carry pb's timeouts. `operation_timeout_ms` and `part_erase_timeout_ms` are in the
  frozen layout and zero in every release; a device fills them, and a host that reads zero
  uses its own (F-226).

## 5. The commands

Each command: what it means, what it needs, what a failure leaves behind. The class is 3.4's,
the exchange 4.4's. The names are the wire's, without `PB_CMD_`.

Every command runs in every life cycle state, behind the gate (3.2). What depends on the state
beyond the gate is said with the command (5.2, 5.5).

### 5.1 The device

| Command                   | Means                                                                     |
| ------------------------- | ------------------------------------------------------------------------- |
| `DEVICE_IDENTIFIER_READ`  | The device's UUID (`platform.md` 3.7) and the board's name (`board.md` 8) |
| `AUTHENTICATE`            | 3.3                                                                       |
| `DEVICE_READ_CAPS`        | The number of buffers (2), a buffer's size, the largest transfer; the     |
|                           | timeouts (Open (6)). The only negotiation (`compatibility.md`)            |
| `BOOTLOADER_VERSION_READ` | pb's version, as text                                                     |
| `DEVICE_RESET`            | Leaves command mode (2.3)                                                 |

None of them fails for a reason of its own, and none changes anything.

### 5.2 The life cycle and the keys

The order of the life cycle and the keystore are the core's, the fuses the platform's
(`security.md` 4.8, D-027). These commands write fuses: what they did can't be undone. The
host tools ask before they send one; the device doesn't, the command is the decision.

| Command                      | Means                                         | Refused                         |
| ---------------------------- | --------------------------------------------- | ------------------------------- |
| `SLC_READ`                   | The life cycle, and each keystore key's id as | The life cycle is unknown       |
|                              | active or revoked (16 at most)                |                                 |
| `SLC_SET_CONFIGURATION`      | Writes the key hash and the board's           | When locked, or in EOL          |
|                              | configuration fuses (`board.md` 8): the bits  |                                 |
|                              | that aren't set yet                           |                                 |
| `SLC_SET_CONFIGURATION_LOCK` | Locks: from `CONFIGURATION` only              | In `NOT_CONFIGURED` and in EOL  |
| `SLC_SET_EOL`                | End of life (`system.md` 4), from any state   | Without the product's option:   |
|                              | that is known                                 | `NOT_SUPPORTED` (F-263)         |
| `SLC_REVOKE_KEY`             | Revokes the keystore key the command names    | A key the keystore doesn't have |

- **Setting the state the device is in** answers OK: for `CONFIGURATION` the fuses are written
  again, so that a configuration that failed half way is completed by running it again; for
  locked and EOL nothing is written.
- **A failure** leaves the fuses that were written. The life cycle is read again after each of
  these commands, also a failed one, and the gate follows what the fuses say (3.2).
- **A key whose status can't be read** is in neither of `SLC_READ`'s lists, and the last result
  carries the error.
- **Revoking the key of the session's token** doesn't end the session: the token was verified
  when the key was good. The next session's isn't (`security.md` 4.3).
- Which keys to revoke, and not revoking the last one, is the product's (`system.md` 6).

### 5.3 Partitions

Command mode names a partition by its UUID. What a host may do with it is in the partition's
flags, which the board's partition table and the drivers set (`board.md` 2.4):

| Flag       | For command mode                                                            |
| ---------- | --------------------------------------------------------------------------- |
| `VISIBLE`  | The partition exists for the host: it is in the table, and commands find it |
| `WRITABLE` | `STREAM_WRITE_BUFFER` and `PART_ERASE` are allowed                          |
| `READABLE` | `STREAM_READ_BUFFER` is allowed                                             |
| `BOOTABLE` | A candidate for `BOOT_PART` and `PART_ACTIVATE` (`boot.md` 6, 3.4)          |

**Open (2)**: what keeps a partition's content from an authenticated host. **Not yet in the
code (F-186, F-272)**: only the table leaves hidden partitions out; every other command finds
them by UUID, and `PART_VERIFY` checks no flag. With sizes of 1, 2, 3 bytes and 256 guesses
each it reads any partition, so `READABLE` keeps nothing secret. Proposal:

- **`VISIBLE` is the boundary.** A partition without it doesn't exist for the host: every
  command that names it answers `NOT_FOUND`. A board hides what no host may reach.
- **`READABLE` only gates the bulk read**, and the contract says so: an authenticated host can
  learn what a visible partition holds. `PART_VERIFY` stays as it is for visible partitions:
  every released factory flow writes and then verifies, and no board in the tree sets
  `READABLE` (which is also why `part read` fails on them, F-083).
- The boot commands follow the same rule: a hidden partition isn't booted by name.

| Command            | Means                                                            | Needs                  |
| ------------------ | ---------------------------------------------------------------- | ---------------------- |
| `PART_TBL_READ`    | The visible partitions: UUID, description, first and last block, |                        |
|                    | block size, the flags' low byte                                  |                        |
| `PART_TBL_INSTALL` | Writes a partition table to the device the command names: the    | A table the board gave |
|                    | board's layout of that variant (`board.md` 2.4)                  |                        |
| `PART_ERASE`       | Erases `block_count` blocks from `start_lba`                     | `WRITABLE` (F-188)     |
| `PART_VERIFY`      | Compares the SHA-256 of `size` bytes of the partition with the   |                        |
|                    | host's                                                           |                        |
| `PART_BPAK_READ`   | The image header a partition holds, when it is a valid one       |                        |

- **`PART_TBL_INSTALL`**: the new table is in effect for the commands that follow, and the
  stream ends. **Not yet in the code (F-194)**: until a reset the old partitions are listed and
  written. A failure may leave the device without a valid table; command mode still runs
  (`board.md` 2.1) and the install is run again. It isn't retired, whatever the comment in
  `cm_main.c` and F-032 say: `board.md` 2.1 and 2.4 rely on it.
- **`PART_ERASE`**: `start_lba` is a block of the device the partition is on, as hosts send it
  (the partition's first block from the table, plus what they erased already). The range lies
  inside the partition, else `INVALID_ARGUMENT`. A count of 0 erases nothing and answers OK
  (F-075, accepted: `system.md` 5.1). A failure leaves some of the blocks erased.
- **`PART_VERIFY`**: the bytes are the partition's first `size` bytes. With `bpak` set they
  are a bpak file's: the header from the partition's last 4 KiB, then `size` less 4 KiB from
  the start. `size` is any number of bytes inside the partition, at least 4 KiB with `bpak`,
  and exactly those bytes are hashed. **Not yet in the code (F-182, F-190)**: a size that
  isn't a multiple of the block hashes stale bytes, and a `bpak` size below 4 KiB wraps. It
  changes nothing, the stream neither (F-186).
- **`PART_BPAK_READ`**: the header is what the partition holds. Nothing verified it: the host
  treats it as input.

### 5.4 Streams

How a host writes and reads a partition: it selects the partition, fills a buffer, and tells
the device where the buffer goes. Two buffers of `CONFIG_CM_BUF_SIZE_KiB` (4 MiB), so that a
host can send one while the other is written.

| Command                 | Means                                                         | Refused                 |
| ----------------------- | ------------------------------------------------------------- | ----------------------- |
| `STREAM_INITIALIZE`     | Selects the stream's partition; a new stream, nothing written | No such partition:      |
|                         |                                                               | then there is no stream |
| `STREAM_PREPARE_BUFFER` | Reads `size` bytes from the host into a buffer                | A buffer id or size     |
|                         |                                                               | the device doesn't have |
| `STREAM_WRITE_BUFFER`   | Writes `size` bytes of a buffer at `offset`                   | Without `WRITABLE`      |
| `STREAM_READ_BUFFER`    | Reads `size` bytes at `offset` into a buffer and sends them   | Without `READABLE`      |
| `STREAM_FINALIZE`       | Ends a write                                                  | Never                   |

- **No stream**: before a `STREAM_INITIALIZE` that succeeded in this session, a write or a read
  answers `STREAM_NOT_INITIALIZED`. **Not yet in the code (F-074, F-190)**: the stream starts
  out as the first block device.
- **`offset` and `size` are bytes.** The offset is a multiple of the block size, else
  `INVALID_ARGUMENT`. **Not yet in the code (F-273)**: it is rounded down. A write whose size
  isn't a multiple of the block writes its last block padded with zeros; a read sends `size`
  bytes. **Not yet in the code (F-182)**: the tail is lost on eMMC. The range lies inside the
  partition.
- **A write is the buffer's bytes**, whatever filled it. The host fills it first.
- **A write that fails** marks the stream failed and leaves what it wrote. Writing a partition
  isn't atomic, and nothing is verified when it is written: what a partition holds is input
  to the boot, which verifies it (`security.md` 3).
- **`STREAM_FINALIZE`**: when the stream wrote something and no write failed, the device's
  last step for a complete write runs (on eMMC it makes a boot partition bootable, F-082).
  Hosts finalize after a failure too; then nothing runs. It answers OK without a stream. The
  partition stays selected, as released.

### 5.5 Boot

The flow is `boot.md` 6's, a stop in it `board.md` 5.4's. Here: what the host sees.

| Command         | Means                                                              |
| --------------- | ------------------------------------------------------------------ |
| `BOOT_PART`     | Boots the partition the host names, one of the policy's candidates |
| `BOOT_BPAK`     | Boots the image the host sends                                     |
| `PART_ACTIVATE` | The policy's: which system boots next (`boot.md` 3.4)              |
| `BOOT_STATUS`   | The policy's: which system is active (`boot.md` 3.4)               |

- **A boot the host starts is verified like any other**, in every life cycle state
  (`security.md` 4.1). On a closed device it also needs the gate. With an unknown life cycle it
  is refused (`system.md` 4).
- **The last OK means "loaded and verified", not "booted"** (`boot.md` 6). After it the
  transport is disconnected and the session is over, whatever follows.
- **A boot that fails before the disconnect** leaves command mode as it was: the session, its
  authentication and the stream stay, and what the load wrote to memory is never run
  (`boot.md` 2).
- `verbose` only changes what pb logs.

**Open (7)**: `BOOT_BPAK` answers OK and then starts the boot. When the boot is refused before
the image's header is read (a precondition, `early_boot()`'s stop, a board without a
handover), the host is already sending the header, and pb reads it as commands (F-128).
`boot.md` 2 puts more before the load than the code has today, so this becomes the common
refusal. Proposal: after its first OK the device always reads the header's transfer, and the
header's result carries the refusal. Hosts stop at an error there. Nothing changes in the
sequence, and it doesn't rest on a host checking the first result.

### 5.6 The board

| Command             | Means                                                |
| ------------------- | ---------------------------------------------------- |
| `BOARD_COMMAND`     | Runs the board's command of that id (`board.md` 4.4) |
| `BOARD_STATUS_READ` | The board's status, as text (`board.md` 4.3)         |

- Both are behind the gate like every command, and the board's function runs only after it.
- **The request** is at most 1024 bytes and the board gets exactly `request_size` of them.
- **The response** is at most what the host said it can take (`response_buffer_size`) and at
  most a buffer. **Not yet in the code (F-095)**: the board may fill the whole buffer, and a
  host with a smaller one leaves the response on the link.
- **A reset after a command**: a board's command may ask for one
  (`cm_board_cmd_request_reboot_on_success()`). When the command then returns OK, the device
  sends the last result and leaves command mode as for `DEVICE_RESET` (2.3). A command that
  fails doesn't reset.
- The board's commands stay inside the core's rules (`security.md` 4.8, `board.md` 4.4).

### 5.7 Logs

`LOG_READ` reads a log by the id of its name: pb's own (in RAM), the list of the logs, and what
the build and the board add (the ROM's, the last boot's kernel log). The positions and the
old hosts' cases are in `compatibility.md`.

- A log is read, never cleared.
- Logs hold no secrets (`security.md` 4.10), and on a closed device they are behind the gate
  all the same: they tell what failed and where.
- A source that only command mode reads is set up in command mode (2.1, D-023).

### 5.8 Retired

`PART_RESIZE` and `AUTH_SET_PASSWORD` answer `NOT_SUPPORTED`, behind the gate, for ever. Their
slots are never reused (`compatibility.md`, F-032).

A command is retired, never removed: its handler answers `NOT_SUPPORTED`, and a decision entry
says why.

## 6. The transport

One transport per build, linked, not chosen by the board (`board.md` 4.2, D-049): USB on a
SoC, a socket on the host platform. Its functions and what a USB device controller must do are
the drivers' contract (later). What command mode needs from it:

- **Ready and lost.** It says when a host is there and when it is gone. A bus reset, an unplug
  and a failed transfer are all "lost", and each ends the session (3.1).
- **Whole transfers of a size both sides know**, in order, each way. A transfer of 0 bytes is
  one (4.4).
- **Start and poll.** A transfer is started and then polled, so that the tasks run and the
  watchdog is kicked while command mode waits for a command.
- **Disconnect** before a jump and before a reset, so that the host sees the device leave.
- **Nothing before the gate but the identity.** What the transport itself tells a host before
  authentication is the device's identity and nothing else: on USB the descriptors, with the
  device's UUID as the serial number. Its own parser (USB's control requests) checks every
  length from the host, like 4.2.
- **No transport is added for convenience**: each one's stack is in the trusted computing base
  (`security.md` 1). pb gets no network transport (`board.md` 4.2).

USB, as hosts find the device:

| What                  | Value                                               |
| --------------------- | --------------------------------------------------- |
| Vendor and product id | `1209:2019`, unless the build sets its own          |
| Interface             | One, vendor specific class                          |
| Endpoints             | Bulk IN 1 (to the host), bulk OUT 2 (from the host) |
| Serial number         | The device's UUID, as its 36 character string       |

A controller that can't send the serial number (the 8X's, today) leaves hosts to read the UUID
with `DEVICE_IDENTIFIER_READ`.

## 7. Fixed (compatibility)

Like the wire (D-008, `compatibility.md`), from the 1.0.0 release on (`system.md` 5.1):

- What each command means (section 5), and what a zero or an absent field does.
- Each command's exchange (4.4), with what released hosts do after a refusal.
- The two commands a closed device answers before authentication, and that the others answer
  `NOT_AUTHENTICATED`.
- The methods: what a token signs and with which hash, the password's bytes (3.3). A method's
  grant never shrinks.
- The results a host acts on (4.5): `INVALID_COMMAND` for a command the build doesn't have,
  `NOT_SUPPORTED` for a retired one, `NOT_AUTHENTICATED` from the gate.
- Which life cycle states are open (3.2).
- The USB interface (section 6).
- The partition flags' meaning (5.3), which boards in the field rely on.

Only security comes before it (`principles.md` 3): Open (1), Open (2) and the checks marked
"not yet in the code" refuse what a release did. Each is a rule of `security.md`, and the
decision entry that closes this draft names them.

## 8. Review checklist

For a change to command mode, a new command or a new method:

- [ ] Does it pass the gate, and is the gate still the only place that decides? A new command
      has a class (3.4).
- [ ] Is anything new answered before authentication? Then it isn't allowed (3.2).
- [ ] Every size, offset, count and id from the host is checked in the core, with arithmetic
      that can't wrap (4.2).
- [ ] The exchange ends with a result on every path, and stays in step after a refusal (4.4).
- [ ] Nothing is sent that the command didn't produce (4.3).
- [ ] The work kicks the watchdog per chunk (4.6).
- [ ] What does a failure leave behind? It is written in section 5.
- [ ] Old host and new device, new host and old device (`compatibility.md`'s checklist).
- [ ] A new command is appended and has a transcript; a new result code is appended.
- [ ] Nothing of it runs on the boot path (D-023).

## 9. Not yet in the code

| Finding             | What                                                                                 |
| ------------------- | ------------------------------------------------------------------------------------ |
| F-268, F-267        | Command mode as a task; the platform sets up the port, the transport is linked (2)   |
| F-185, F-201        | The session ends with the transport: authentication and the stream are cleared (3.1) |
| F-271               | No gate without `CONFIG_CM_AUTH` (3.2, Open (1))                                     |
| F-181               | The verdict of a token is the verification's own (3.3)                               |
| F-004               | The password is compared whole, in constant time, and not logged (3.3)               |
| F-190               | Only what a command produced is sent; build-time bounds (4.3)                        |
| F-187, F-274        | The exchange stays in step after a refusal; a failed transfer ends the session (4.4) |
| F-087               | A result code for a payload whose hash differs (4.5)                                 |
| F-189, F-226        | The watchdog per chunk; the timeouts (4.6)                                           |
| F-186, F-272, F-188 | The partitions' flags: hidden partitions, `PART_VERIFY`, `PART_ERASE` (5.3)          |
| F-194               | A new partition table is in effect at once (5.3)                                     |
| F-182, F-273, F-074 | Sizes and offsets that aren't block multiples; no stream (5.3, 5.4)                  |
| F-128, F-252        | A boot the host started that is refused or stopped (5.5)                             |
| F-095               | The board's response fits the host's buffer (5.6)                                    |
| F-263               | EOL as the product's option (5.2)                                                    |
