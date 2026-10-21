"""Fixtures for the punchboot integration tests.

The tests run punchboot as a native process (pb-host, built by the top level
CMakeLists.txt) and talk to it through the punchboot Python package over a
unix domain socket, just like the real tools talk to a device over USB.

Each test gets its own working directory, so the disk image persists across
reboots within a test, and tests can run in parallel.
"""

from __future__ import annotations

import contextlib
import os
import pathlib
import subprocess
import time
from typing import TYPE_CHECKING

import punchboot
import pytest

if TYPE_CHECKING:
    from collections.abc import Generator, Iterator

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_BINARY = REPO_ROOT / "build-host" / "pb-host"

# UUIDs from src/board/host
DISK_UUID = "1eacedf3-3790-48c7-8ed8-9188ff49672b"
SYSTEM_A_UUID = "2af755d8-8de5-45d5-a862-014cfa735ce0"
SYSTEM_B_UUID = "c046ccd8-0f2e-4036-984d-76c14dc73992"

START_TIMEOUT = 10.0
EXIT_TIMEOUT = 10.0


class Device:
    """A punchboot instance running as a host process."""

    def __init__(self, binary: pathlib.Path, workdir: pathlib.Path) -> None:
        """Create a device, backed by files in 'workdir'."""
        self.binary = binary
        self.workdir = workdir
        self.disk = workdir / "disk.img"
        self.socket = workdir / "pb.sock"
        self.boot_status_file = workdir / "boot_status"
        self.log = workdir / "pb.log"
        self._proc: subprocess.Popen[bytes] | None = None

    def start(self, *, command_mode: bool = True) -> None:
        """Power on. With 'command_mode', boot is aborted and command mode is entered."""
        assert self._proc is None, "Device is already running"
        self.socket.unlink(missing_ok=True)
        self.boot_status_file.unlink(missing_ok=True)

        args = [
            str(self.binary),
            "--disk",
            str(self.disk),
            "--socket",
            str(self.socket),
            "--boot-status",
            str(self.boot_status_file),
        ]
        if command_mode:
            args.append("--command-mode")

        with self.log.open("ab") as log:
            log.write(f"=== start: {' '.join(args)}\n".encode())
            log.flush()
            self._proc = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)  # noqa: S603

        if command_mode:
            deadline = time.monotonic() + START_TIMEOUT
            while not self.socket.exists():
                if self._proc.poll() is not None:
                    msg = f"pb-host exited with {self._proc.returncode} during start"
                    raise RuntimeError(msg)
                if time.monotonic() > deadline:
                    msg = "Timeout waiting for the command mode socket"
                    raise TimeoutError(msg)
                time.sleep(0.01)

    def wait_exit(self, timeout: float = EXIT_TIMEOUT) -> int:
        """Wait for the process to exit (reset or boot) and return its exit code."""
        assert self._proc is not None, "Device is not running"
        try:
            rc = self._proc.wait(timeout=timeout)
        finally:
            if self._proc.poll() is None:
                self._proc.kill()
                self._proc.wait()
        self._proc = None
        return rc

    def stop(self) -> None:
        """Shut down, if running.

        Prefer a reset over killing the process, so that coverage data is
        written when built with PB_COVERAGE.
        """
        if self._proc is None:
            return
        if self._proc.poll() is None:
            with contextlib.suppress(Exception):
                self.reset()
        if self._proc is not None:
            self._proc.kill()
            self._proc.wait()
            self._proc = None

    @contextlib.contextmanager
    def session(self) -> Iterator[punchboot.Session]:
        """Open a command mode session. Only one session can be open at a time."""
        s = punchboot.Session(socket_path=self.socket)
        try:
            yield s
        finally:
            s.close()

    def reset(self) -> None:
        """Reset the device over command mode and wait for it to go down."""
        with self.session() as s:
            s.device_reset()
        assert self.wait_exit() == 0

    def reboot(self, *, command_mode: bool = True) -> None:
        """Reset and power on again."""
        self.reset()
        self.start(command_mode=command_mode)

    @property
    def boot_status(self) -> str | None:
        """Partition name ('A', 'B' or '?') reported by the last boot, if any."""
        if not self.boot_status_file.exists():
            return None
        return self.boot_status_file.read_text()


@pytest.fixture(scope="session")
def pb_host_binary() -> pathlib.Path:
    """Path to the pb-host executable."""
    binary = pathlib.Path(os.environ.get("PB_HOST_BIN", DEFAULT_BINARY))
    if not binary.exists():
        pytest.exit(f"pb-host not found at {binary}, build it or set PB_HOST_BIN")
    return binary


@pytest.fixture
def device(
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


@pytest.hookimpl(wrapper=True, tryfirst=True)
def pytest_runtest_makereport(
    item: pytest.Item,
) -> Generator[None, pytest.TestReport, pytest.TestReport]:
    """Make the test result available to fixtures, for printing logs on failure."""
    report = yield
    setattr(item, f"rep_{report.when}", report)
    return report
