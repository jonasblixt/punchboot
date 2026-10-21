"""Fixtures for the punchboot integration tests, see pbtest.py."""

from __future__ import annotations

import os
import pathlib
from typing import TYPE_CHECKING

import pytest

from .pbtest import DISK_UUID, REPO_ROOT, Device

if TYPE_CHECKING:
    from collections.abc import Generator, Iterator

DEFAULT_BINARY = REPO_ROOT / "build-host" / "pb-host"


@pytest.fixture(scope="session")
def pb_host_binary() -> pathlib.Path:
    """Path to the pb-host executable."""
    binary = pathlib.Path(os.environ.get("PB_HOST_BIN", DEFAULT_BINARY))
    if not binary.exists():
        pytest.exit(f"pb-host not found at {binary}, build it or set PB_HOST_BIN")
    return binary


@pytest.fixture
def blank_device(
    pb_host_binary: pathlib.Path, tmp_path: pathlib.Path, request: pytest.FixtureRequest
) -> Iterator[Device]:
    """Start a device in command mode, with a blank disk."""
    dev = Device(pb_host_binary, tmp_path)
    dev.start(command_mode=True)
    yield dev
    dev.stop()

    report = getattr(request.node, "rep_call", None)
    if report is not None and report.failed and dev.log.exists():
        print(f"\n--- pb-host log ({dev.log}) ---\n{dev.log.read_text()}")  # noqa: T201


@pytest.fixture
def device(blank_device: Device) -> Device:
    """Start a device in command mode, with the default partition table installed."""
    with blank_device.session() as s:
        s.part_table_install(DISK_UUID)
    blank_device.reboot()
    return blank_device


@pytest.hookimpl(wrapper=True, tryfirst=True)
def pytest_runtest_makereport(
    item: pytest.Item,
) -> Generator[None, pytest.TestReport, pytest.TestReport]:
    """Make the test result available to fixtures, for printing logs on failure."""
    report = yield
    setattr(item, f"rep_{report.when}", report)
    return report
