# punchboot MCU test bench

Hardware tests of punchboot on microcontrollers, where the Linux test bench
(`../README.md`) doesn't apply (D-025). A small bare metal test application,
built for each board in the dev container, and a pytest runner on the host
that drives pb over USB and reads the console. No RTOS and no vendor SDK:
what is tested is what pb hands over, not someone's startup code.

| Board | Status |
| ----- | ------ |
| i.MX RT1060 EVK | all tests pass (2026-11-08) |

## What it tests

pb's side of the boot, on the board:

- pb verifies the signed boot image of System A or B, copies it to RAM and
  starts it (`src/boot/armv7m_baremetal.c`).
- The handover: r0 is the booted system's name, r1 the boot partition's
  UUID, VTOR the image's vector table, MSP its initial stack pointer,
  CONTROL and FAULTMASK 0, no interrupt enabled, SysTick off, nothing
  pending, the watchdog running, the console usable. The caches are
  reported (the RT1060's ROM turns both on).
- A/B: the enabled system boots, the image marks itself verified in pb's
  boot state like pbstate does on Linux.
- Rollback: an update that never marks itself verified is booted for its
  boot attempts, then pb goes back to the verified system.
- The watchdog: an update that hangs is reset by the watchdog pb started,
  then rolled back.

## The application (`src/`, `board/<board>/`)

Every run prints one line per check on the console, `MCUTEST CHECK <name>
PASS|FAIL ...`, then `MCUTEST SYSTEM`, `MCUTEST STATE` (the boot state as
pb left it) and `MCUTEST RESULT`. It is built in three modes, one boot image
each:

| Image | Does |
| ----- | ---- |
| `app-good.bpak` | marks the system verified, then resets into pb's command mode |
| `app-noverify.bpak` | doesn't mark it, resets: pb boots it again, a boot attempt |
| `app-hang.bpak` | stops petting the watchdog |

The reset into command mode is a request in a register that survives a warm
reset, which the board's pb checks in its early boot callback (the RT1060:
`PB_CM_REQUEST` in SRC_GPR5, `src/board/imxrt1060evk/board.c`). That is how
the runner gets the board back without a hand on it.

Per board: `board.mk` (CPU, interrupts, load address), `link.ld` and
`board.c` (console, watchdog, writing the boot state, reset).

## Build

In the dev container, from the top of the punchboot tree:

```sh
scripts/pbdev run make -C testbench/mcu                   # BOARD=imxrt1060evk
scripts/pbdev firmware imxrt1060evk                       # pb itself
scripts/pbdev run make BOARD=src/board/imxrt1060evk \
    KCONFIG_CONFIG=configs/imxrt1060evk_defconfig CROSS_COMPILE=arm-none-eabi- all
```

The images go to `build/<board>/app-<mode>.bpak`, signed with the test key
that pb's test keystore trusts (`scripts/mkimage`).

## Run

On the host, with the board in pb's command mode on USB and its console
connected, from a venv with this tree's `punchboot` package:

```sh
python3 -m venv .venv && .venv/bin/pip install -e ../.. pytest
.venv/bin/pytest tests                      # all, about a minute on the RT1060
.venv/bin/pytest tests -m "not slow"        # without the watchdog test
.venv/bin/pytest tests --board imxrt1060evk --device <uuid> --console /dev/ttyACM0
```

Every test starts and ends with pb in command mode. The tests write System A
and B and pb's boot state.

## i.MX RT1060 EVK

- pb in the QSPI NOR (`pb_flash.bin` to `BL`), its layout: `BL` 256 KiB,
  the two boot state copies (4 KiB each), System A at 512 KiB and System B,
  1 MiB each.
- The application runs from OCRAM2 at 0x20210000, between the ROM's data
  (0x20200000-0x20207fff, which its API still uses) and pb (0x20249000).
- It writes the boot state with the ROM's FlexSPI NOR API (RM 9.13),
  configured with pb's own FCB.
- Console: the J-Link's serial port (LPUART1). The watchdog is WDOG1, 30 s
  (`CONFIG_WATCHDOG_TIMEOUT`, 30.5 s as the hardware counts it).

## Adding a board

1. pb: a boot driver for the board (on ARMv7-M the armv7m one, the image
   in RAM), System A/B and the boot state, and the command mode request in
   its early boot callback.
2. `board/<board>/`: `board.mk`, `link.ld` (the RAM pb doesn't use) and
   `board.c`.
3. `tests/mcu.py`: the board in `BOARDS`.
4. Run the tests, add the board to the table above.
