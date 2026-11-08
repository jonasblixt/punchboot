"""Helpers of the MCU test bench: the boards, the console and pb's boot state."""

from __future__ import annotations

import contextlib
import dataclasses
import io
import os
import re
import struct
import termios
import threading
import time
import uuid
import zlib
from typing import TYPE_CHECKING

from punchboot import Session
from punchboot.helpers import list_usb_devices

if TYPE_CHECKING:
    import pathlib

SYSTEM_A = uuid.UUID("2af755d8-8de5-45d5-a862-014cfa735ce0")
SYSTEM_B = uuid.UUID("c046ccd8-0f2e-4036-984d-76c14dc73992")
STATE_PRIMARY = uuid.UUID("f5f8c9ae-efb5-4071-9ba9-d313b082281e")
STATE_BACKUP = uuid.UUID("656ab3fc-5856-4a5e-a2ae-5a018313b3ee")

# include/boot/pb_state_blob.h
PB_STATE_MAGIC = 0x026D4A65
A_ENABLED = A_VERIFIED = ERROR_A_ROLLBACK = 1 << 0
B_ENABLED = B_VERIFIED = ERROR_B_ROLLBACK = 1 << 1


@dataclasses.dataclass(frozen=True)
class Board:
    """A board of the MCU test bench."""

    name: str
    # Glob for the console, a /dev/serial/by-id link
    console: str
    # pb's watchdog timeout (CONFIG_WATCHDOG_TIMEOUT, as the hardware rounds it)
    watchdog_s: float


BOARDS = {
    "imxrt1060evk": Board(
        name="imxrt1060evk",
        console="/dev/serial/by-id/usb-SEGGER_J-Link_*-if00",
        watchdog_s=30.5,
    ),
}


def pb_crc32(data: bytes) -> int:
    """CRC-32 as pb's src/lib/crc.c computes it: no initial or final inversion."""
    return zlib.crc32(data, 0xFFFFFFFF) ^ 0xFFFFFFFF


def state_blob(enable: int, verified: int, attempts: int = 0, error: int = 0) -> bytes:
    """Return a struct pb_boot_state (512 bytes) with a valid CRC."""
    head = struct.pack("<5I", PB_STATE_MAGIC, enable, verified, attempts, error)
    body = head + bytes(512 - len(head) - 4)
    # Over the whole struct with the crc field zero
    return body + struct.pack("<I", pb_crc32(body + bytes(4)))


class Console:
    """The board's console, read by a thread into a buffer."""

    def __init__(self, path: str) -> None:
        """Open 'path' at 115200 8N1, raw."""
        self._fd = os.open(path, os.O_RDONLY | os.O_NOCTTY)
        attrs = termios.tcgetattr(self._fd)
        attrs[0] = 0  # iflag
        attrs[1] = 0  # oflag
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL  # cflag
        attrs[3] = 0  # lflag
        attrs[4] = attrs[5] = termios.B115200
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 1
        termios.tcsetattr(self._fd, termios.TCSANOW, attrs)
        termios.tcflush(self._fd, termios.TCIFLUSH)
        self._buf = b""
        self._lock = threading.Lock()
        self._stop = False
        self._thread = threading.Thread(target=self._reader, daemon=True)
        self._thread.start()

    def _reader(self) -> None:
        while not self._stop:
            with contextlib.suppress(OSError):
                data = os.read(self._fd, 4096)
                if data:
                    with self._lock:
                        self._buf += data

    def close(self) -> None:
        """Stop the reader and close the device."""
        self._stop = True
        self._thread.join()
        os.close(self._fd)

    def mark(self) -> int:
        """Position to wait from, everything before it is ignored."""
        with self._lock:
            return len(self._buf)

    def text(self, since: int = 0) -> str:
        """Return what arrived since 'since', without the CRs."""
        with self._lock:
            data = self._buf[since:]
        return data.decode(errors="replace").replace("\r", "")

    def wait(self, pattern: str, since: int, timeout: float) -> re.Match[str]:
        """Wait for 'pattern' (a regex, multiline) after 'since'."""
        deadline = time.monotonic() + timeout
        regex = re.compile(pattern, re.MULTILINE)
        while True:
            if m := regex.search(self.text(since)):
                return m
            if time.monotonic() > deadline:
                msg = f"No '{pattern}' within {timeout} s, the console said:\n{self.text(since)}"
                raise TimeoutError(msg)
            time.sleep(0.05)


@dataclasses.dataclass
class Run:
    """One run of the test application, from its console lines."""

    mode: str
    system: str
    checks: dict[str, str]
    state: str
    result: str


def parse_runs(text: str) -> list[Run]:
    """Every run of the application in 'text', in order."""
    runs = []
    for block in text.split("MCUTEST START ")[1:]:
        lines = block.splitlines()
        checks = {
            m.group(1): m.group(2)
            for line in lines
            if (m := re.match(r"MCUTEST CHECK (\S+) (PASS|FAIL)", line))
        }
        system = re.search(r"^MCUTEST SYSTEM (\S)", block, re.MULTILINE)
        state = re.search(r"^MCUTEST STATE (.*)$", block, re.MULTILINE)
        result = re.search(r"^MCUTEST RESULT (\S+)", block, re.MULTILINE)
        runs.append(
            Run(
                mode=lines[0].split()[0],
                system=system.group(1) if system else "?",
                checks=checks,
                state=state.group(1) if state else "",
                result=result.group(1) if result else "",
            ),
        )
    return runs


class Device:
    """pb on the board, reached over USB."""

    def __init__(self, board: Board, device_uuid: str | None) -> None:
        """Find the device: 'device_uuid', or the one that says it is 'board'."""
        self.board = board
        self.uuid = device_uuid or self._find()

    def _find(self) -> str:
        for uu in list_usb_devices():
            with contextlib.suppress(Exception):
                s = Session(device_uuid=uu)
                try:
                    if s.device_get_boardname() == self.board.name:
                        return str(uu)
                finally:
                    s.close()
        msg = f"No {self.board.name} in command mode on USB"
        raise RuntimeError(msg)

    def present(self) -> bool:
        """Whether pb answers on USB, in command mode."""
        return any(str(uu) == self.uuid for uu in list_usb_devices())

    def wait_command_mode(self, timeout: float) -> None:
        """Wait until pb is in command mode."""
        deadline = time.monotonic() + timeout
        time.sleep(0.5)
        while not self.present():
            if time.monotonic() > deadline:
                msg = f"{self.board.name} not in command mode within {timeout} s"
                raise TimeoutError(msg)
            time.sleep(0.2)

    def session(self) -> Session:
        """Open a new session, which boot and reset commands end."""
        return Session(device_uuid=self.uuid)

    def write_image(self, part: uuid.UUID, image: pathlib.Path) -> None:
        """Erase, write and verify a partition."""
        s = self.session()
        try:
            s.part_erase(part)
            s.part_write(image, part)
            s.part_verify(image, part)
        finally:
            s.close()

    def write_state(self, blob: bytes) -> None:
        """Both copies of the boot state, what pbstate on the device does."""
        s = self.session()
        try:
            for part in (STATE_PRIMARY, STATE_BACKUP):
                s.part_erase(part)
                s.part_write(io.BytesIO(blob), part)
        finally:
            s.close()
