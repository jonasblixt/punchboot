"""Fixtures of the MCU test bench: the board, its console and its images."""

from __future__ import annotations

import glob
import os
import pathlib
from typing import TYPE_CHECKING

import pytest
from mcu import BOARDS, Board, Console, Device

if TYPE_CHECKING:
    from collections.abc import Iterator

BUILD = pathlib.Path(__file__).resolve().parent.parent / "build"


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption("--board", default=os.environ.get("MCU_BOARD", "imxrt1060evk"))
    parser.addoption(
        "--device",
        default=os.environ.get("MCU_DEVICE"),
        help="pb's device UUID, default: the one that says it is --board",
    )
    parser.addoption(
        "--console",
        default=os.environ.get("MCU_CONSOLE"),
        help="the console's tty, default: the board's /dev/serial/by-id link",
    )


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line("markers", "slow: waits for the watchdog")


@pytest.fixture(scope="session")
def board(request: pytest.FixtureRequest) -> Board:
    return BOARDS[request.config.getoption("--board")]


@pytest.fixture(scope="session")
def console(request: pytest.FixtureRequest, board: Board) -> Iterator[Console]:
    path = request.config.getoption("--console")
    if path is None:
        found = sorted(glob.glob(board.console))  # noqa: PTH207
        if not found:
            pytest.fail(f"No console {board.console}, set --console")
        path = found[0]
    c = Console(path)
    yield c
    c.close()


@pytest.fixture(scope="session")
def device(request: pytest.FixtureRequest, board: Board) -> Device:
    return Device(board, request.config.getoption("--device"))


@pytest.fixture(scope="session")
def images(board: Board) -> dict[str, pathlib.Path]:
    """Return the application's boot images, which make BOARD=<board> builds."""
    modes = ("good", "noverify", "hang", "boottime")
    found = {m: BUILD / board.name / f"app-{m}.bpak" for m in modes}
    missing = [str(p) for p in found.values() if not p.exists()]
    if missing:
        pytest.fail(f"Missing {', '.join(missing)}: run make BOARD={board.name}")
    return found


@pytest.fixture(autouse=True)
def in_command_mode(device: Device) -> Device:
    """Every test starts and ends with pb in command mode."""
    device.wait_command_mode(timeout=60)
    return device
