"""Helpers for the punchboot integration tests.

The tests run punchboot as a native process (pb-host, built by the top level
CMakeLists.txt) and talk to it through the punchboot Python package over a
unix domain socket, just like the real tools talk to a device over USB.
"""

from __future__ import annotations

import contextlib
import pathlib
import shutil
import subprocess
import tempfile
import time
import uuid
from typing import TYPE_CHECKING

import punchboot

if TYPE_CHECKING:
    from collections.abc import Iterator

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]

# Layout of src/board/host, with the default partition table installed
BLOCK_SIZE = 512
DISK_BLOCKS = 65536
DISK_UUID = uuid.UUID("1eacedf3-3790-48c7-8ed8-9188ff49672b")
SYSTEM_A_UUID = uuid.UUID("2af755d8-8de5-45d5-a862-014cfa735ce0")
SYSTEM_B_UUID = uuid.UUID("c046ccd8-0f2e-4036-984d-76c14dc73992")
READABLE_UUID = uuid.UUID("ff4ddc6c-ad7a-47e8-8773-6729392dd1b5")
NULL_UUID = uuid.UUID(int=0)
SYSTEM_A_LBA = 34
SYSTEM_B_LBA = 1058
SYSTEM_SIZE = 512 * 1024
PRIMARY_STATE_LBA = 2082
BACKUP_STATE_LBA = 2083

# Keys in pki/internal_keystore.bpak, and the auth token for the host
# board's device UUID
DEV_KEY_ID = 0xA90F9680  # pb-development
AUTH_TOKEN = REPO_ROOT / "tests" / "02e49231-756e-35ee-a982-378e5ba866a9.token"

START_TIMEOUT = 10.0
EXIT_TIMEOUT = 10.0

# Lines that ASan, LeakSanitizer and UBSan print when they find something
SANITIZER_MARKERS = ("ERROR: AddressSanitizer", "ERROR: LeakSanitizer", "runtime error:")


class Device:
    """A punchboot instance running as a host process."""

    def __init__(self, binary: pathlib.Path, workdir: pathlib.Path) -> None:
        """Create a device, backed by files in 'workdir'."""
        self.binary = binary
        self.workdir = workdir
        self.disk = workdir / "disk.img"
        # Unix socket paths are limited to 107 bytes, which a deep working
        # directory easily exceeds (e.g. on CI), so the socket gets its own
        # short directory.
        self._socket_dir = pathlib.Path(tempfile.mkdtemp(prefix="pb-"))
        self.socket = self._socket_dir / "pb.sock"
        self.boot_status_file = workdir / "boot_status"
        self.log = workdir / "pb.log"
        self._proc: subprocess.Popen[bytes] | None = None
        # Problems found while shutting down, e.g. a sanitizer report. The
        # fixture fails the test on these.
        self.problems: list[str] = []

    def start(self, *, command_mode: bool = True) -> None:
        """Power on.

        With 'command_mode' the boot is aborted and command mode is entered,
        and this returns once command mode is ready. Without it, the device
        tries to boot, use wait_exit() or wait_command_mode() after this.
        """
        if self._proc is not None:
            msg = "Device is already running"
            raise RuntimeError(msg)
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
            self.wait_command_mode()

    def wait_command_mode(self, timeout: float = START_TIMEOUT) -> None:
        """Wait for command mode, fails if the device exits (e.g. boots) instead."""
        if self._proc is None:
            msg = "Device is not running"
            raise RuntimeError(msg)
        deadline = time.monotonic() + timeout
        while not self.socket.exists():
            if self._proc.poll() is not None:
                rc = self._proc.returncode
                self._proc = None
                msg = (
                    f"pb-host exited ({rc}) instead of entering command mode, "
                    f"log tail:\n{self.log_tail()}"
                )
                raise RuntimeError(msg)
            if time.monotonic() > deadline:
                msg = "Timeout waiting for command mode"
                raise TimeoutError(msg)
            time.sleep(0.01)

    def wait_exit(self, timeout: float = EXIT_TIMEOUT) -> int:
        """Wait for the process to exit (reset or boot) and return its exit code."""
        if self._proc is None:
            msg = "Device is not running"
            raise RuntimeError(msg)
        try:
            rc = self._proc.wait(timeout=timeout)
        finally:
            if self._proc.poll() is None:
                self._proc.kill()
                self._proc.wait()
        self._proc = None
        return rc

    @property
    def running(self) -> bool:
        """True while the process is alive."""
        return self._proc is not None and self._proc.poll() is None

    def stop(self) -> None:
        """Shut down, if running.

        Prefer a reset over killing the process, so that coverage data is
        written when built with PB_COVERAGE. A non-zero exit code (e.g. from
        a sanitizer) is recorded in 'problems'.
        """
        proc = self._proc
        if proc is None:
            return
        self._proc = None
        if proc.poll() is None:
            try:
                with self.session() as s:
                    s.device_reset()
            except punchboot.Error:
                # E.g. a locked device that isn't authenticated
                proc.kill()
                proc.wait()
                return
            try:
                proc.wait(timeout=EXIT_TIMEOUT)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
                self.problems.append("pb-host did not exit on reset and was killed")
                return
        if proc.returncode != 0:
            self.problems.append(f"pb-host exited with {proc.returncode}")

    def close(self) -> None:
        """Shut down, remove the socket directory and check the log for sanitizer reports."""
        self.stop()
        shutil.rmtree(self._socket_dir, ignore_errors=True)
        if self.log.exists():
            log = self.log.read_text(errors="replace")
            self.problems.extend(
                f"sanitizer report in the pb-host log: {m}" for m in SANITIZER_MARKERS if m in log
            )

    def log_tail(self, lines: int = 20) -> str:
        """Last lines of the pb-host log."""
        if not self.log.exists():
            return ""
        return "\n".join(self.log.read_text().splitlines()[-lines:])

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
        rc = self.wait_exit()
        if rc != 0:
            msg = f"pb-host exited with {rc} on reset"
            raise RuntimeError(msg)

    def reboot(self, *, command_mode: bool = True) -> None:
        """Reset over command mode, then power on again."""
        self.reset()
        self.start(command_mode=command_mode)

    def boot(self) -> str | None:
        """Power on without forcing command mode, and expect it to boot.

        Returns the name of the partition that was booted ('A', 'B' or '?').
        """
        self.start(command_mode=False)
        rc = self.wait_exit()
        if rc != 0:
            msg = f"pb-host exited with {rc}"
            raise RuntimeError(msg)
        return self.boot_status

    @property
    def boot_status(self) -> str | None:
        """Partition name ('A', 'B' or '?') reported by the last boot, if any."""
        if not self.boot_status_file.exists():
            return None
        return self.boot_status_file.read_text()

    def read_blocks(self, lba: int, count: int = 1) -> bytes:
        """Read blocks directly from the disk image."""
        with self.disk.open("rb") as f:
            f.seek(lba * BLOCK_SIZE)
            return f.read(count * BLOCK_SIZE)

    def write_blocks(self, lba: int, data: bytes) -> None:
        """Write directly to the disk image, e.g. to corrupt it."""
        with self.disk.open("r+b") as f:
            f.seek(lba * BLOCK_SIZE)
            f.write(data)

    def pbstate(self, *args: str) -> str:
        """Run the pbstate tool on the A/B state partitions of the disk image.

        Like on a target, where pbstate runs from Linux.
        """
        primary = self.workdir / "pb_state_primary"
        backup = self.workdir / "pb_state_backup"
        primary.write_bytes(self.read_blocks(PRIMARY_STATE_LBA))
        backup.write_bytes(self.read_blocks(BACKUP_STATE_LBA))
        result = subprocess.run(  # noqa: S603
            [pbstate_binary(), "-p", str(primary), "-b", str(backup), *args],
            check=True,
            capture_output=True,
            text=True,
        )
        self.write_blocks(PRIMARY_STATE_LBA, primary.read_bytes())
        self.write_blocks(BACKUP_STATE_LBA, backup.read_bytes())
        return result.stdout


def pbstate_binary() -> str:
    """Path to the pbstate tool."""
    path = shutil.which("pbstate")
    if path is None:
        msg = "pbstate not found in PATH (scripts/ci/host-test.sh builds it)"
        raise RuntimeError(msg)
    return path


def sgdisk_verify(disk: pathlib.Path) -> str:
    """Verify a GPT disk image with sgdisk, returns the report."""
    result = subprocess.run(  # noqa: S603
        ["sgdisk", "-v", str(disk)],  # noqa: S607
        check=False,
        capture_output=True,
        text=True,
    )
    return result.stdout


def make_bpak(
    path: pathlib.Path,
    data: bytes,
    *,
    load_addr: int = 0x49000000,
    hash_kind: str = "sha256",
    signature_kind: str = "prime256v1",
    key_id: str = "pb-development",
    sign_key: str = "pki/secp256r1-key-pair.pem",
) -> pathlib.Path:
    """Create a signed bpak image with a single 'kernel' part, using the bpak CLI."""
    payload = path.with_suffix(".bin")
    payload.write_bytes(data)
    img = str(path)

    def bpak(*args: str) -> None:
        subprocess.run(["bpak", *args], check=True, cwd=REPO_ROOT, capture_output=True)  # noqa: S603, S607

    bpak("create", img, "-Y", "--hash-kind", hash_kind, "--signature-kind", signature_kind)
    bpak("add", img, "--meta", "bpak-package", "--from-string",
         "8df597ff-2cf5-42ea-b2b6-47c348721b75", "--encoder", "uuid")  # fmt: skip
    bpak("add", img, "--meta", "bpak-package-uid", "--from-string",
         str(uuid.uuid1()), "--encoder", "uuid")  # fmt: skip
    bpak("add", img, "--meta", "pb-load-addr", "--from-string", hex(load_addr),
         "--part-ref", "kernel", "--encoder", "integer")  # fmt: skip
    bpak("add", img, "--part", "kernel", "--from-file", str(payload))
    bpak("set", img, "--key-id", key_id, "--keystore-id", "pb")
    bpak("sign", img, "--key", sign_key)
    return path
