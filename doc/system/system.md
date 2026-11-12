# The system

**State: reviewed (Jonas, 2026-11-11), D-034 to D-036, D-051, D-061.** Level 1 (`process.md`).

What pb is, its interfaces to the outside, the device's life cycle, and what pb promises. The
documents below say how; this one says what they must add up to. Against whom the promises hold
is in `security.md`.

## 1. What pb is

A bootloader for embedded devices in a verified boot chain. It does two things:

- **It boots**: it picks a system, verifies its image and starts it, as fast as the hardware
  allows.
- **It provisions and updates**: command mode, a host on a cable writing images, partition
  tables, fuses and keys, in the factory, in development and in service.

There is no run time configuration: a build is one board's configuration. pb has no shell, no
scripts, no file systems and no network.

## 2. The chain and the modes

```
ROM -> pb -> secure firmware (-> a secure OS) -> an operating system or a hypervisor
          -> an operating system or a hypervisor
          -> an application (a microcontroller)
```

| Mode         | Entered                                              | Ends with                           |
| ------------ | ---------------------------------------------------- | ----------------------------------- |
| Boot         | Every reset                                          | The jump, or command mode           |
| Command mode | A boot that failed or was refused; the board's stop; | A boot the host started, or a reset |
|              | a serial download (`board.md` 5.1)                   |                                     |
| Reset        | A platform failure; the watchdog; command mode's end | Boot                                |

Command mode without a host ends by itself: when its transport isn't ready in time
(`CONFIG_CM_TRANSPORT_READY_TIMEOUT`, 10 s), pb resets and boots again.

Command mode is what pb does when it is stopped, in a build that has it (the default). Stopped,
pb runs the build's tasks in turn (`board.md` 4.6, D-051), and command mode is one of them. A
build without command mode stops for the same reasons and runs its other tasks; with none it
resets.

The flows: `boot.md`, `command-mode.md`.

## 3. The interfaces to the outside

| Interface           | The other side             | Fixed                           | Where                          |
| ------------------- | -------------------------- | ------------------------------- | ------------------------------ |
| The ROM             | Loads and authenticates pb | The SoC's, not pb's             | `platform.md`, `plat-imx.md`   |
| The image           | The build that signs       | Like the wire: the format       | `compatibility.md`,            |
|                     | images                     | (bpak), the meta data pb reads  | `boot.md` 4                    |
| The wire            | A host in command mode     | Frames, values, sequences,      | `compatibility.md`; behaviour: |
|                     |                            | meaning                         | `command-mode.md`              |
| The handover        | The next stage             | Like the wire: the entry state, | `boot.md` 5, 7                 |
|                     |                            | the arguments, pb's information |                                |
| The OS at run time  | The update client,         | Like the wire: the A/B state's  | `boot.md` 3.7, 7; `board.md` 8 |
|                     | `pbstate`                  | format and meaning, the         |                                |
|                     |                            | partition UUIDs                 |                                |
| The host tools      | Scripts, factory lines     | By version: a breaking change   | Below                          |
|                     |                            | is a major release              |                                |
| The device identity | Hosts, the token, the OS   | Like the wire: the UUID's       | `platform.md` 3.7              |
|                     |                            | derivation                      |                                |
| The porting API     | Boards and platforms, out  | By version: `struct board`, the | `board.md`, `platform.md`      |
|                     | of the tree too            | hooks, the build interface      |                                |

- **The ROM.** pb expects the ROM's state as the port's document records it, and says so when
  the ROM left something else (the logs, the boot source, `security.md` 4.5).
- **The host tools** (the Python API, the CLI) follow semantic versioning: new functions in a
  minor release, a breaking change in a major one, and with care. They are not frozen. Only the
  wire and the image format are (D-008), and there only security comes first (`principles.md`).
  `pbstate` on the target has the release's version too; its library's SONAME is the ABI's
  version and changes only when the ABI breaks (`libpbstate.so.0`).
- **pb's status is part of pb's information in the handover**: what pb couldn't do and booted
  anyway (`security.md` 4.5), so that the next stage can report it or act on it. Which entries
  is `boot.md` 5.6's, decided with the conditions themselves (F-248); new ones are appended.
- **The image format may get company.** Jonas (2026-11-11): bpak solves more than pb needs, and
  pb may get a simpler format of its own later. The boot flow's coupling to bpak stays loose
  enough for that: what it needs from a format is `boot.md` 4.4. bpak images stay accepted
  (D-008), so a new format comes next to bpak. Whether
  one build accepts several formats is to discuss (`roadmap.md`).
- **A build may leave bpak out** (Jonas, 2026-11-12, D-061), once there is another format to
  build in its place. That is the product's choice for its own devices, made in its defconfig,
  like a build without command mode's authentication (section 6). bpak is in by default, and a
  build that has it accepts every bpak image a release accepted (5.1).

## 4. The life cycle

The device's security life cycle (SLC), kept in the SoC's fuses. It only moves forward. The
order is the core's (D-027), the fuses the platform's.

| State                   | Means                            | The ROM               | Command mode                |
| ----------------------- | -------------------------------- | --------------------- | --------------------------- |
| `NOT_CONFIGURED`        | Nothing fused                    | Runs any pb           | Open                        |
| `CONFIGURATION`         | The key hash and the board's     | Runs any pb           | Open                        |
|                         | configuration fuses are written; |                       |                             |
|                         | can be repeated until locked     |                       |                             |
| `CONFIGURATION_LOCKED`  | The product in the field         | Runs only a signed pb | Authentication              |
| `EOL`                   | End of life, optional            | The SoC's             | Open (D-011)                |
| Unknown (a read failed) |                                  |                       | Authentication, and no boot |

- pb verifies images the same way in every state (`security.md` 4.1). The states differ in what
  the ROM enforces and in who may use command mode.
- A board without a life cycle (no SLC support) has an open command mode.
- **A life cycle that can't be read doesn't boot**, with every handover (`security.md` 4.4;
  today only where the handover writes a DT). When an attack made the read fail, that is the
  right answer. When it was a fault, the device isn't lost: in the field, without a host, command
  mode gives up and pb resets (section 2), so a read that works the next time boots; in
  development the ROM's serial download recovers a pb that broke the read.
- **EOL is the SoC's end of life state** (Jonas, 2026-11-11), extended from NXP's field return.
  The device is open again and the SoC's security state has changed, so what was sealed to the
  device (key blobs) can't be opened any more and the secrets stay protected.
  - A product chooses it: a Kconfig option, off by default. Without it `slc set-eol` answers
    NOT_SUPPORTED and the device can't enter EOL.
  - Every port supports it. Where the SoC can't, the port's document says why.
  - A port implements EOL only as the SoC's own state and says in its document what the ROM and
    the keys do then. Command mode is open in EOL because of that state, never because of a
    flag pb keeps.
  - **Not yet in the code (F-263)**: only the host platform implements EOL, and there is no
    option.

## 5. What pb promises

**5.1 Compatibility** (`principles.md` 3, D-008). Old hosts work with new devices and new hosts
with old devices. Every image a release accepted is accepted. What a product in the field relies
on doesn't change in an update (`compatibility.md`, "Fixed elsewhere"). The floor is the 1.0.0
release: hosts and firmware from v1.0.0 on. Only security comes before it. F-075 (v2.0.0 changed
erase for 1.x hosts) is accepted, no product used it.

**5.2 Only verified images run** (`security.md`).

**5.3 Speed** (`principles.md` 4, D-023). The boot path does what the boot needs and nothing
else. The times per board are measured and recorded (the README, `testbench/README.md`).

**5.4 A failure is never silent and never a hang.**

- A boot that fails or is refused ends in command mode, with the reason in the log (without
  command mode in the build: stopped, section 2).
- A platform failure resets (`platform.md` 2); the recovery is the ROM's serial download.
- The watchdog covers the boot, command mode's commands and the jump to the next stage's first
  kick (`platform.md` 3.3).
- What pb couldn't do and booted anyway, it reports (section 3).

**5.5 One identity.** A device has the same UUID for its whole life, in every release.

**5.6 Nothing is dropped without asking** (`principles.md` 7). A feature out-of-tree users may
build on changes its form in a major release at most.

## 6. What pb leaves to the product

- The signing keys, their custody and when to revoke one.
- Whether command mode asks for authentication at all (`CONFIG_CM_AUTH`, on by default), and
  with which methods (`command-mode.md` 3.2).
- Which image formats its build accepts, once there is more than one (section 3). bpak is in
  by default.
- The update client in the OS and its use of the A/B state (`pbstate`).
- What the next stage verifies (the root file system).
- The password's secret and where the board keeps it (`board.md` 4.5).
- The partition layout, the boot source policy, the handover: the board's choices (`board.md`).
