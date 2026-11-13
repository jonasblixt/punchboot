# The transport and USB device controller contract

**State: reviewed (Jonas, 2026-11-12), D-064, D-072, D-073.** Level 3 (`process.md`).

What carries command mode's bytes: the transport's functions as command mode calls them, pb's
USB device (the class, the stack on endpoint 0) and what a USB device controller's driver
provides (`include/drivers/usb/usbd.h`, `struct usbd_hal_ops`). `command-mode.md` 6 says what
command mode needs; this says who gives it and how. The transport's stack is what an attacker
with a cable reaches before the gate (`security.md` 1, 4.7), so it stays small. It builds on
`drivers.md`. Vendor neutral: where each controller stands is section 8.

Changing the transport, the stack or a controller's driver: read this, `drivers.md` and
`command-mode.md` 6, then go through the checklist (section 7). Section 6 is fixed like the
wire.

**Not yet in the code (F-nnn)** marks what the code doesn't do yet. The commit that fixes the
finding removes the marker and updates section 8.

**No open items.** The draft's five, as decided in Jonas's review (D-072) and written into
the text:

| Item | Rule                                                                              | By    |
| ---- | --------------------------------------------------------------------------------- | ----- |
| 1    | The transport's six functions: `cm_transport_init()`, `_connect()`, `_read()`,    | Jonas |
|      | `_write()`, `_complete()`, `_disconnect()` (2)                                    |       |
| 2    | Every error from `complete` is "lost". A controller reports a bus reset and an    | Jonas |
|      | unplug as an error of the transfer (2.4)                                          |       |
| 3    | A full speed bus isn't supported, for now: high speed, or not ready (4.1)         | Jonas |
| 4    | A request the stack doesn't know is stalled: a new operation, `ep0_stall` (3.2)   | Jonas |
| 5    | A request on endpoint 0 leaves a bulk transfer alone (3.2)                        | Jonas |

## 1. Words and scope

| Layer      | Owns                                                 | Where                               |
| ---------- | ---------------------------------------------------- | ----------------------------------- |
| Transport  | What command mode calls: ready, whole transfers each | USB: `pb_dev_cls.c`. The host       |
|            | way, lost, disconnect                                | platform: `src/plat/host/socket.c`  |
| Class      | pb's USB device: the descriptors, the two bulk       | `pb_dev_cls.c`                      |
|            | endpoints                                            |                                     |
| Stack      | Endpoint 0: enumeration and the standard requests    | `usbd.c`                            |
| Controller | One IP block: endpoints, transfers, the bus's events | `struct usbd_hal_ops`, one driver   |
|            |                                                      | per block                           |

| Word     | Means                                                                              |
| -------- | ---------------------------------------------------------------------------------- |
| Transfer | `length` bytes in one direction, started by one call and polled until it is done   |
| OUT, IN  | From the host, to the host (USB's words)                                           |
| Packet   | The endpoint's largest packet: 512 bytes on the bulk endpoints at high speed       |
| Lost     | The host is gone or started over: an unplug, a bus reset, a transfer that failed   |

One transport per build, linked (`board.md` 4.2, D-049), and one controller per build: the
port is the platform's Kconfig choice, set up by `plat_cm_init()` (`platform.md` 3.10). Nothing
of this runs on the boot path (D-023).

Not here: the frames and what they mean (`compatibility.md`, `command-mode.md`), the port's
clocks, power and PHY (`platform.md` 3.10), charger detection, which a board does in its own
hooks (`board.md` 5.3), USB host mode, which pb doesn't have.

## 2. The transport

Six functions. Command mode calls them and nothing else of the layers below.

### 2.1 Function : cm_transport_init() [mandatory]

```
Argument : void
Return   : int, PB_OK or -PB_ERR_*
```

Starts the transport: on USB the class, the stack and the controller, which attaches to the
bus. Called when command mode is set up, after `plat_cm_init()` (`command-mode.md` 2.1), and
again each time the transport is restarted.

- **A restart is `cm_transport_disconnect()`, then this.** The device leaves the bus and comes
  back, so the host sees a new device and starts over. **Not yet in the code (F-287)**:
  command mode restarts with `init` alone.
- It doesn't wait for a host.
- Failure: no command mode; its task ends the loop and pb resets (`command-mode.md` 2.1).

### 2.2 Function : cm_transport_connect() [mandatory]

```
Argument : void
Return   : int, PB_OK, -PB_ERR_AGAIN or -PB_ERR_*
```

Polled until a host is there. PB_OK: the transport is ready, and a session starts
(`command-mode.md` 3.1). On USB that is when the host configured the device. `-PB_ERR_AGAIN`:
not yet; command mode's time for it is `command-mode.md` 2.2's. Any other error: the
controller failed, and command mode ends as when no host came.

**A bus reset before the device is configured is no error.** A host resets the bus when it
enumerates a device, often more than once. Until the device is configured there is no session
to lose: on a bus reset or an unplug the stack starts over, with address 0 and nothing
configured, and `connect` goes on answering `-PB_ERR_AGAIN`.

It returns at once, so that the tasks run and the watchdog is kicked meanwhile. **Not yet in
the code (F-287)**: the host platform's socket blocks in `init` until a client connects.

### 2.3 Functions : cm_transport_read(), cm_transport_write() [mandatory]

```
Argument : void *buf (const for a write), size_t length
Return   : int, PB_OK or -PB_ERR_*
```

Start a transfer of `length` bytes from the host into `buf`, or from `buf` to the host. They
return at once; `cm_transport_complete()` says when it is done.

- **One transfer at a time.** The next is started when the last is done. Starting one while
  another runs is `-PB_ERR_STATE`.
- **The buffer is the transport's until the transfer is done or the transport is restarted.**
  It owns its cache lines (`drivers.md` 4.3): command mode's command, its result and its two
  buffers are declared so, and a part of `BOOT_BPAK` goes to its load address through the
  boot's bounce buffer when that address doesn't. Any other is refused, `-PB_ERR_ALIGN`.
- **A read is done when exactly `length` bytes arrived**, and nothing is written past them.
  Fewer bytes at the end of the host's transfer (a short packet) is an error: the two sides
  no longer agree (`command-mode.md` 4.4). **Not yet in the code (F-137, F-205, F-287)**: no
  controller reports a short read, and one writes up to a packet past the buffer.
- **A read that is whole packets ends at its length**, and what the host sent beyond it stays
  for the next read. `BOOT_BPAK`'s parts rest on it: the host splits a part into transfers of
  its own size, the device reads it in pieces of `CONFIG_BOOT_LOAD_CHUNK_kB`, and on both
  sides every piece but the part's last is whole packets (`command-mode.md` 4.4; Jonas,
  D-073). A host transfer that ends inside a packet before the part does is a short packet,
  and an error. Released hosts send the caps' largest transfer, which is whole packets:
  command mode's buffer and the boot's piece are both set in units of 1024 bytes. Not
  verified where the two sizes differ (F-280).
- **A write sends exactly `length` bytes and no packet after them**, also when `length` is
  whole packets: the host knows the size and asks for no more. A write of 0 bytes sends one
  empty packet (`command-mode.md` 6: a transfer of 0 bytes is one).
- **The largest transfer** is at least command mode's buffer (`CONFIG_CM_BUF_SIZE_KiB`) and
  the boot's piece. A length the transport can't take is `-PB_ERR_PARAM`, never a shorter
  transfer. **Not yet in the code (F-287)**: one controller masks the length.
- A read of 0 bytes isn't used and is `-PB_ERR_PARAM`.

### 2.4 Function : cm_transport_complete() [mandatory]

```
Argument : void
Return   : int, PB_OK, -PB_ERR_AGAIN or -PB_ERR_*
```

Polls the transfer that was started. It never waits for the host's data: command mode calls
it once per round of the tasks while it waits for a command (`board.md` 4.6), and in a loop
with its own timeout and the watchdog's kick inside a command (`command-mode.md` 4.6). The
transport has no timeout of its own for a transfer. A request on endpoint 0 that it answers
on the way (3.2) is bounded.

| Return          | Means                                                                       |
| --------------- | --------------------------------------------------------------------------- |
| `PB_OK`         | The transfer is done: all of `length`, and for a read the bytes are in      |
|                 | `buf`. Returned once; with no transfer started it is `-PB_ERR_STATE`        |
| `-PB_ERR_AGAIN` | Not done yet                                                                |
| Any other       | **Lost**: the session ends and command mode restarts the transport          |
|                 | (`command-mode.md` 3.1)                                                     |

- **Lost is every way the host goes away or starts over, once the device is configured**: an
  unplug, a bus reset, a transfer that failed, a host that deconfigured the device. The
  transport reports each as an error of the transfer that was running. One is always running
  while a session waits for a command. What happens while none runs, inside a command, is
  kept and is the error of the next transfer, so nothing is missed. **Not yet in the code
  (F-287, F-363)**: two controllers report
  nothing, the third only a bus reset during a bulk transfer; and command mode keeps polling
  after `-PB_ERR_TIMEOUT`.
- It is also where the stack answers endpoint 0 (3.2).

### 2.5 Function : cm_transport_disconnect() [mandatory]

```
Argument : void
Return   : int, PB_OK or -PB_ERR_*
```

The device leaves: on USB the controller's `stop` (4.2). Before a jump, before a reset and
before a restart (`command-mode.md` 2.3), so that the host sees the device go and nothing of
the transport runs in the next stage (`drivers.md` 2.3). A transfer that was running is given
up; its buffer is the caller's again.

A disconnect that fails before a jump is a failed boot, and pb resets (`drivers.md` 2.3).
**Not yet in the code (F-287)**: its result is ignored.

### 2.6 The host platform's socket

The tests' transport (`testing.md`): a Unix socket, the same six functions, the same rules. A
socket is a byte stream, so it has no packets: a read is done at `length` bytes, a client that
closes is lost, and `connect` is the accept. It is where command mode's own rules are tested
(the exchanges, lost, the timeouts); what only USB has (short packets, endpoint 0, bus
resets) is the bench's.

## 3. pb's USB device

### 3.1 What a host sees

One device, fixed like the wire (section 6): one configuration, one vendor specific
interface, a bulk IN and a bulk OUT endpoint, the device's UUID as its serial number.

- **Nothing but the identity before the gate** (`security.md` 4.7, `command-mode.md` 6): the
  descriptors say that this is pb and which device it is, the same as the two commands a
  closed device answers. They carry no version, no life cycle and nothing of the board's but
  what `DEVICE_IDENTIFIER_READ` gives.
- **The serial number is read from the device**, by every controller. pb 2.0.0 on one
  controller couldn't send it (F-159, fixed); hosts read the UUID with
  `DEVICE_IDENTIFIER_READ` from a device whose serial number they can't read.
- **Windows binds the device without a driver package**: the stack answers Microsoft's OS
  string descriptor and its compatible id request with `WINUSB`.

### 3.2 Endpoint 0

The stack answers these requests and no others:

| Request                             | Answer                                                    |
| ----------------------------------- | --------------------------------------------------------- |
| `GET_DESCRIPTOR`                    | Device, qualifier, configuration (with the interface and  |
|                                     | the endpoints), the strings: the language, the product    |
|                                     | and manufacturer, the serial number, Microsoft's (0xEE)   |
| `SET_ADDRESS`, `SET_CONFIGURATION`  | Done; the device is ready after the second                |
| `GET_STATUS` (device)               | Two zero bytes                                            |
| Vendor request 1, index 4           | Microsoft's compatible id descriptor                      |

- **Every length and index from the host is checked** (`command-mode.md` 4.2): a reply is at
  most `wLength` and at most the descriptor; a string index the class doesn't have is
  stalled; `SET_CONFIGURATION` takes the one configuration there is, or 0,
  which deconfigures and is "lost". **Not yet in the code (F-287)**: the configuration's value
  isn't looked at.
- **Any other request is stalled.** The host then goes on at once. **Not yet in the
  code (F-138)**: nothing is answered, the host waits out its timeout, and the transfer that
  was running fails. pb's device answers what enumeration needs and no more: the other
  standard requests (`GET_CONFIGURATION`, `CLEAR_FEATURE` and the rest) are stalled too.
- **Three that the code answers today are stalled** (Jonas, D-073): the debug descriptor,
  which gets the interface descriptor; `SET_IDLE`, a request of another class; and a string
  index the class doesn't have, which gets an empty string. **Not yet in the code (F-287).**
- **A request doesn't disturb a bulk transfer.** Hosts send requests at any time
  (`lsusb -v`, another program that lists devices). The stack answers on endpoint 0 and the
  bulk transfer goes on where it was. **Not yet in the code (F-203)**: on the ChipIdea, whose
  driver has one transfer for all endpoints, a transfer of a packet at most is cancelled and
  started again, and a longer one fails.
- **A control transfer is bounded**: a host that stops inside one doesn't hold pb. Its data
  and status stages are waited for with a timeout, and a new SETUP ends the one before
  (F-138).
- The stack keeps nothing between sessions but the address and the configuration, and both
  are cleared when the bus is reset or the controller is stopped.

## 4. The controller

A controller's driver gives `struct usbd_hal_ops` to the stack when the platform sets it up
(`usbd_init_hal_ops()`, from `plat_cm_init()`). Every operation is mandatory. The stack checks
that at registration and calls none it didn't check. **Not yet in the code (F-287)**: three
are called unchecked.

Every operation returns at once but `init`, `stop` and the stages of a control transfer
(4.4), which wait with a bound (`drivers.md` 3). The driver polls: no interrupt is enabled.

The driver's own `<driver>_init()` (`drivers.md` 2.1), which `plat_cm_init()` calls, takes
the SoC's facts and registers the operations. It doesn't touch the bus: the controller is
reset and attached by the operation `init`, when the transport starts.

### 4.1 Operation : init [mandatory]

Resets the controller, sets it up as a device and attaches it to the bus. The port's clocks,
power and PHY are done (`platform.md` 3.10).

- **It works on a controller that runs**: after the ROM's serial download, after a `stop`,
  after a session that was lost.
- **High speed, or not ready**, for now (Jonas, D-072). The descriptors are for high speed (512 byte
  packets). On a bus that only does full speed the device doesn't become ready, and command
  mode ends as when no host came. **Not yet in the code (F-287)**: only one controller asks
  for a speed, and none acts on what the bus gave.
- The address is 0 and nothing is configured.

### 4.2 Operation : stop [mandatory]

`drivers.md` 2.3: the pull-up is off, the controller is halted, nothing is queued, no address
in pb's memory is left in it. Bounded, and it can be called twice. **Not yet in the code
(F-204)**: one controller's is empty.

### 4.3 Operations : xfer_start, xfer_complete, xfer_cancel [mandatory]

```
xfer_start(usb_ep_t ep, void *buf, size_t length)   PB_OK or -PB_ERR_*
xfer_complete(usb_ep_t ep)                          PB_OK, -PB_ERR_AGAIN or -PB_ERR_*
xfer_cancel(usb_ep_t ep)
```

The transport's rules (2.3, 2.4), per endpoint:

- **One transfer per endpoint, and the endpoints are independent**: a transfer on endpoint 0
  leaves a bulk transfer as it is (3.2).
- **`buf` owns its lines** (`drivers.md` 4.3). A controller that needs more of a buffer (a
  page boundary) bounces inside the driver, and copies after it invalidated
  (`drivers.md` 4.2).
- **OUT: done at exactly `length` bytes.** Nothing is written past `length`, also when the
  controller counts in packets: the driver bounces the last packet. A short packet before
  `length` ends the transfer with `-PB_ERR_IO`.
- **IN: exactly `length` bytes**, no empty packet after whole packets; 0 bytes is one empty
  packet.
- **`xfer_complete` reads the controller's own result**: an endpoint that halted, a buffer
  error or bytes left over are `-PB_ERR_IO` (F-137). It returns PB_OK once per transfer.
- **A bus reset or an unplug ends every transfer with `-PB_ERR_IO`** (2.4), and the endpoints
  are unconfigured after it. With no transfer running it is kept, and is the error of the
  next `xfer_start` or `xfer_complete`. Before the device is configured it is no error (2.2).
- **`xfer_cancel` gives a transfer up**, bounded: the endpoint is idle, the buffer is the
  caller's again, and nothing of it completes later.
- A length above what the driver can queue is `-PB_ERR_PARAM`; the limit is at least 2.3's.

### 4.4 Operations for control requests [mandatory]

```
poll_setup_pkt(struct usb_setup_packet *pkt)        PB_OK, -PB_ERR_AGAIN or -PB_ERR_*
set_address(uint16_t addr)
set_configuration(const struct usb_endpoint_descriptor *eps, size_t no_of_eps)
ep0_xfer_zlp(usb_ep_t ep)
ep0_stall(void)
```

- **`poll_setup_pkt`**: PB_OK with the packet when a SETUP arrived, `-PB_ERR_AGAIN` when
  none did. A SETUP is never lost and never read twice: one that arrives while the last is
  handled replaces it (F-138). An error is a bus reset or an unplug of a device that was
  configured (2.4); before that the driver starts over by itself (2.2).
- **`set_address`** takes effect after the request's status stage, as USB requires.
- **`set_configuration`** sets up the class's endpoints as the descriptors say and makes them
  ready; called again it starts them over.
- **`ep0_xfer_zlp`** is the status stage, bounded.
- **`ep0_stall`.** Stalls endpoint 0 until the next SETUP. **Not yet in the code
  (F-138)**: there is no such operation.

## 5. Buffers and sizes

| What                          | Size                              | Declared by                     |
| ----------------------------- | --------------------------------- | ------------------------------- |
| A command, a result           | 512 bytes, one packet             | Command mode, owns its lines    |
| Command mode's two buffers    | `CONFIG_CM_BUF_SIZE_KiB`          | Command mode, owns its lines    |
| A piece of a `BOOT_BPAK` part | `CONFIG_BOOT_LOAD_CHUNK_kB`,      | The image's load address, or    |
|                               | whole packets but the last        | the boot's bounce buffer        |
| The controller's own          | Descriptors, rings, a setup       | The driver, with the macro of   |
|                               | packet, bounce buffers            | `drivers.md` 4.4                |
| What the stack sends on       | Descriptors, a status             | The stack. Out only: it needs   |
| endpoint 0                    |                                   | no alignment, and isn't changed |
|                               |                                   | until it is sent                |

**Not yet in the code (F-282)**: the controllers' objects are aligned each in its own way.

The caps tell a host the buffer's size and the largest transfer (`command-mode.md` 5.1); both
are `CONFIG_CM_BUF_SIZE_KiB` today.

## 6. Fixed (compatibility)

Like the wire (D-008, `command-mode.md` 7), what hosts and operating systems find the device
by:

| What                  | Value                                                             |
| --------------------- | ----------------------------------------------------------------- |
| Vendor and product id | `1209:2019`, unless the build sets its own                        |
| Configuration         | One, with one interface: vendor specific class, subclass and      |
|                       | protocol (0xFF)                                                   |
| Endpoints             | Bulk IN 1 (to the host), bulk OUT 2 (from the host)               |
| Serial number         | The device's UUID, as its 36 character string (string 2)          |
| Product string        | `Punchboot` (string 1)                                            |
| Microsoft's OS string | Index 0xEE, `MSFT100`, vendor code 1; the compatible id `WINUSB`  |
| Transfers             | The sizes are the exchange's (`command-mode.md` 4.4); no empty    |
|                       | packet follows a transfer of whole packets, either way            |

What 3.0.0 answers differently from a release (D-072, D-073). None is in the table above:

- A request the stack doesn't know is stalled, where it got no answer and the host waited
  out its timeout. The debug descriptor, `SET_IDLE` and a string index the class doesn't
  have are stalled, where they were answered (3.2).
- When the transport is restarted the device leaves the bus first (2.1).

`struct usbd_hal_ops` and the transport's functions are the porting API: they change with a
major release at most (D-018).

## 7. Review checklist

For a new controller driver, and for a change to one, to the stack or to the transport:

- [ ] `drivers.md` 7.
- [ ] `init` works on a running controller; `stop` leaves the bus, halts and can be called
      twice.
- [ ] Every operation but `init` and `stop` returns at once; every wait is bounded.
- [ ] An OUT transfer ends at exactly `length`, writes nothing past it, and a short packet
      is an error. Tried with a length that isn't whole packets, and with a host transfer
      larger and smaller than the read.
- [ ] An IN transfer of whole packets sends no empty packet; one of 0 bytes sends one.
- [ ] `xfer_complete` reads the controller's result, and a bus reset and an unplug end the
      transfer with an error.
- [ ] A request on endpoint 0 during a bulk transfer changes nothing of it; an unknown one is
      stalled.
- [ ] Every length and index of a control request is checked. Nothing new is told before the
      gate.
- [ ] The driver's DMA objects are declared with the macro, and the setup packet shares its
      line with nothing.
- [ ] Nothing of section 6 changed: `lsusb -v` before and after, and a Windows host binds.
- [ ] On the bench: every command of the integration tests, a replug and a bus reset inside
      a session, `boot bpak` with pieces that aren't the host's, a jump with the cable in.

## 8. Conformance

Where each controller's driver is (2026-11-12). Read in the code, not run; "unverified" is
what the code doesn't settle without the hardware.

| Requirement                      | ChipIdea            | DWC3                | Cadence USB3      | Finding |
| -------------------------------- | ------------------- | ------------------- | ----------------- | ------- |
| `init` on a running controller   | yes                 | yes                 | unverified        | F-204   |
| Every wait bounded               | no                  | yes                 | no                | F-136,  |
|                                  |                     |                     |                   | F-205   |
| High speed, or not ready         | no                  | asks, doesn't act   | no                | F-287   |
| A bus reset and an unplug are    | no                  | a reset during a    | no                | F-287,  |
| reported                         |                     | bulk transfer only  |                   | F-363   |
| OUT: nothing past `length`       | yes; an overflow    | no: rounded up to a | unverified        | F-205,  |
|                                  | isn't seen          | packet              |                   | F-137   |
| OUT: a short packet is an error  | no                  | no                  | no                | F-137,  |
|                                  |                     |                     |                   | F-287   |
| A read of whole packets ends at  | yes (jiffy)         | yes (8MP EVK)       | unverified        | F-280   |
| its length                       |                     |                     |                   |         |
| IN: no empty packet after whole  | yes                 | yes                 | yes               |         |
| packets                          |                     |                     |                   |         |
| A transfer of 0 bytes            | yes                 | IN yes              | no (unverified)   | F-287   |
| A length it can't queue is       | yes                 | no: masked to 24    | partly            | F-287,  |
| refused                          |                     | bits                |                   | F-205   |
| `xfer_complete` reads the        | no                  | no                  | partly            | F-137,  |
| controller's result              |                     |                     |                   | F-287   |
| Endpoint 0 independent of a bulk | no: one transfer    | yes                 | yes               | F-203   |
| transfer                         | for the driver      |                     |                   |         |
| An unknown request is stalled    | no                  | no                  | no                | F-138   |
| The driver's DMA objects own     | yes                 | partly: aligned,    | no                | F-205,  |
| their lines                      |                     | not padded          |                   | F-282   |
| The order of clean, invalidate   | no                  | unverified          | no                | F-134,  |
| and copy                         |                     |                     |                   | F-131,  |
|                                  |                     |                     |                   | F-184   |
| `stop`                           | yes, unbounded      | yes                 | no                | F-204   |
| The serial number is read        | yes                 | yes                 | yes (the MEK,     |         |
|                                  |                     |                     | 2026-11-09)       |         |
| Runs on the bench                | jiffy, the RT1060   | the 8MP EVK         | the MEK; resets   | F-172   |
|                                  |                     |                     | on some writes    |         |

The stack and the transport:

| Requirement                                   | Today                                    | Finding      |
| --------------------------------------------- | ---------------------------------------- | ------------ |
| Lost ends the session                         | yes, when the controller reports it      | F-363        |
| A restart disconnects first                   | no                                       | F-287        |
| Unknown requests stalled, control transfers   | no                                       | F-138        |
| bounded                                       |                                          |              |
| A request leaves a bulk transfer alone        | yes; on the ChipIdea one of a packet is  | F-203        |
|                                               | started again, a longer one fails        |              |
| The socket's `connect` polls                  | no: `init` blocks in the accept          | F-287        |
| Tested on the host                            | Command mode on the socket and on a fake | F-230        |
|                                               | transport; the USB transport's functions |              |
|                                               | on a fake controller, no controller      |              |

**The conformance suite** (`process.md` 7). **Not yet in the code (F-287)**: the transport's
rules (section 2) as one set of tests, run on the host against the socket and against the
stack on a fake controller that plays a host: transfers of every size around a packet, a
short packet, a bus reset and an unplug at each step, a request during a bulk transfer, an
unknown request. The controllers themselves have no model; they are the bench's, with the
checklist's last item.

## 9. Not yet in the code

| Finding             | What                                                                         |
| ------------------- | ---------------------------------------------------------------------------- |
| F-287, F-363        | Lost is reported and ends the session; a restart disconnects first (2.4)     |
| F-137, F-205        | An OUT transfer ends at `length`, writes nothing past it, and its result is  |
|                     | the controller's (2.3, 4.3)                                                  |
| F-280               | `BOOT_BPAK`'s parts where the device's pieces aren't the host's (2.3)        |
| F-138, F-203        | Endpoint 0: stall, bounds, no effect on a bulk transfer (3.2)                |
| F-204               | Every controller has a `stop` (4.2)                                          |
| F-136, F-205        | Every wait is bounded (4)                                                    |
| F-131, F-134, F-184 | The cache's order around a transfer (`drivers.md` 4.2)                       |
| F-282               | The controllers' DMA objects own their lines (5)                             |
| F-139, F-140        | The ChipIdea's set up against its reference manual                           |
| F-172               | Writes that reset the MEK                                                    |
