"""pb's boot of an MCU image: the handover, A/B, rollback and the watchdog.

Every run of the test application prints its checks of what pb handed over
on the console (src/main.c); the tests here set up the boot state and the
images and read what happened.
"""

from __future__ import annotations

import re
import statistics
from contextlib import closing
from typing import TYPE_CHECKING

import pytest
from mcu import (
    A_VERIFIED,
    B_ENABLED,
    ERROR_B_ROLLBACK,
    SYSTEM_A,
    SYSTEM_B,
    Board,
    Console,
    Device,
    parse_runs,
    state_blob,
)

if TYPE_CHECKING:
    import pathlib

SYSTEMS = {"A": SYSTEM_A, "B": SYSTEM_B}
# The application's report and its reset into command mode
RUN_TIMEOUT_S = 15


def assert_clean_handover(run_text: str, system: str) -> None:
    runs = parse_runs(run_text)
    assert runs, run_text
    run = runs[-1]
    failed = [name for name, result in run.checks.items() if result != "PASS"]
    assert not failed, run_text
    assert run.system == system, run_text
    assert run.result == "PASS", run_text


@pytest.mark.parametrize("system", ["A", "B"])
def test_boot_from_command_mode(
    device: Device,
    console: Console,
    images: dict[str, pathlib.Path],
    system: str,
) -> None:
    """Punchboot boot partition: verified, loaded, started, back on request."""
    device.write_image(SYSTEMS[system], images["good"])
    mark = console.mark()
    with closing(device.session()) as s:
        s.boot_partition(SYSTEMS[system])

    console.wait(r"^MCUTEST RESET command-mode", mark, RUN_TIMEOUT_S)
    assert_clean_handover(console.text(mark), system)
    console.wait(r"^Command mode requested", mark, RUN_TIMEOUT_S)
    device.wait_command_mode(timeout=RUN_TIMEOUT_S)


@pytest.mark.parametrize("system", ["A", "B"])
def test_enabled_system_boots(
    device: Device,
    console: Console,
    images: dict[str, pathlib.Path],
    system: str,
) -> None:
    """The enabled system boots after a reset, without the host."""
    device.write_image(SYSTEMS[system], images["good"])
    with closing(device.session()) as s:
        s.boot_set_boot_part(SYSTEMS[system])
        mark = console.mark()
        s.device_reset()

    console.wait(r"^MCUTEST RESET command-mode", mark, RUN_TIMEOUT_S)
    assert_clean_handover(console.text(mark), system)
    device.wait_command_mode(timeout=RUN_TIMEOUT_S)
    with closing(device.session()) as s:
        assert s.boot_status()[0] == SYSTEMS[system]


def test_rollback_after_boot_attempts(
    device: Device,
    console: Console,
    images: dict[str, pathlib.Path],
) -> None:
    """An update that never marks itself verified rolls back to the old system.

    A runs and is verified, B is the update: enabled, not verified, two boot
    attempts, what pbstate -s B on the device sets up. B resets without
    marking itself verified, pb boots it twice, then A again.
    """
    device.write_image(SYSTEM_A, images["good"])
    device.write_image(SYSTEM_B, images["noverify"])
    device.write_state(state_blob(enable=B_ENABLED, verified=A_VERIFIED, attempts=2))
    mark = console.mark()
    with closing(device.session()) as s:
        s.device_reset()

    console.wait(r"^MCUTEST RESET command-mode", mark, 4 * RUN_TIMEOUT_S)
    runs = parse_runs(console.text(mark))
    assert [r.system for r in runs] == ["B", "B", "A"], console.text(mark)
    assert [r.mode for r in runs] == ["noverify", "noverify", "good"]
    assert runs[-1].state == (
        f"enable 0x00000001 verified 0x00000001 attempts 0 error {ERROR_B_ROLLBACK:#010x}"
    )
    device.wait_command_mode(timeout=RUN_TIMEOUT_S)
    with closing(device.session()) as s:
        assert s.boot_status()[0] == SYSTEM_A


@pytest.mark.slow
def test_watchdog_rolls_back_a_hung_update(
    board: Board,
    device: Device,
    console: Console,
    images: dict[str, pathlib.Path],
) -> None:
    """An update that hangs: the watchdog pb started resets it, then rollback."""
    device.write_image(SYSTEM_A, images["good"])
    device.write_image(SYSTEM_B, images["hang"])
    device.write_state(state_blob(enable=B_ENABLED, verified=A_VERIFIED, attempts=1))
    mark = console.mark()
    with closing(device.session()) as s:
        s.device_reset()

    hang = console.wait(r"^MCUTEST HANG", mark, RUN_TIMEOUT_S)
    after_hang = mark + len(console.text(mark)[: hang.end()].encode())
    console.wait(r"^MCUTEST RESET command-mode", after_hang, board.watchdog_s + 2 * RUN_TIMEOUT_S)
    runs = parse_runs(console.text(mark))
    assert [r.system for r in runs] == ["B", "A"], console.text(mark)
    assert runs[-1].state.endswith(f"error {ERROR_B_ROLLBACK:#010x}")
    device.wait_command_mode(timeout=RUN_TIMEOUT_S)


def test_boot_time(
    device: Device,
    console: Console,
    images: dict[str, pathlib.Path],
) -> None:
    """Warm reset to the image's entry, timed on the board (app-boottime.bpak).

    The image stores a clock that runs through the reset just before it
    resets, and reads it again at its entry, APP_BOOT_RUNS times. pytest -s
    prints the times.
    """
    device.write_image(SYSTEM_A, images["boottime"])
    with closing(device.session()) as s:
        s.boot_set_boot_part(SYSTEM_A)
        mark = console.mark()
        s.device_reset()

    console.wait(r"^MCUTEST RESET command-mode", mark, 6 * RUN_TIMEOUT_S)
    text = console.text(mark)
    us = [int(t) for t in re.findall(r"^MCUTEST BOOTTIME (\d+) us", text, re.MULTILINE)]
    assert us, text
    assert "FAIL" not in text, text
    ms = [t / 1000 for t in us]
    print(  # noqa: T201
        f"\nwarm reset to entry, {len(ms)} runs: min {min(ms):.2f} "
        f"median {statistics.median(ms):.2f} max {max(ms):.2f} ms",
    )
    # A boot that took longer than a second is not a measurement
    assert max(ms) < 1000
    device.wait_command_mode(timeout=RUN_TIMEOUT_S)
    with closing(device.session()) as s:
        s.boot_set_boot_part(None)
