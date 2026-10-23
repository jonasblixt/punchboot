"""The host's checks of the authentication data (F-094).

Released devices answer OK to an authentication request of any size, so the
host has to check the size itself. These tests talk to a fake device that
does just that, instead of pb-host, which refuses bad sizes (F-005).
"""

from __future__ import annotations

import contextlib
import multiprocessing
import pathlib
import socket
import struct
import tempfile
from typing import TYPE_CHECKING

import punchboot
import pytest

from .pbtest import PB_COMMAND_SIZE, PB_WIRE_MAGIC

if TYPE_CHECKING:
    from collections.abc import Iterator
    from multiprocessing.connection import Connection

PB_CMD_AUTHENTICATE = 16
AUTH_DATA_SIZE = 1024
OK = struct.pack("<Ib", PB_WIRE_MAGIC, 0).ljust(PB_COMMAND_SIZE, b"\0")


def _recv(conn: socket.socket, size: int) -> bytes:
    data = b""
    while len(data) < size:
        chunk = conn.recv(size - len(data))
        if not chunk:
            return b""
        data += chunk
    return data


class OldDevice:
    """Answers every command with OK, and reads authentication data of any size.

    It runs in its own process: the punchboot extension holds the GIL while it
    waits for the device (F-009), so a thread here would never run.
    """

    def __init__(self, path: pathlib.Path) -> None:
        """Listen on 'path' and serve one connection."""
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(str(path))
        server.listen(1)
        self._commands, child_commands = multiprocessing.Pipe(duplex=False)
        self._process = multiprocessing.get_context("fork").Process(
            target=self._serve, args=(server, child_commands), daemon=True
        )
        self._process.start()
        server.close()
        child_commands.close()

    @staticmethod
    def _serve(server: socket.socket, commands: Connection) -> None:
        conn, _ = server.accept()
        with conn:
            while cmd := _recv(conn, PB_COMMAND_SIZE):
                command = cmd[4]
                commands.send(command)
                conn.sendall(OK)
                (size,) = struct.unpack_from("<H", cmd, 9)  # pb_command_authenticate.size
                if command == PB_CMD_AUTHENTICATE and size:
                    if not _recv(conn, AUTH_DATA_SIZE):
                        return
                    conn.sendall(OK)

    def commands(self) -> list[int]:
        """Stop serving and return the commands the host sent."""
        self._process.join(timeout=10)
        self._process.kill()
        received = []
        with contextlib.suppress(EOFError):  # poll() is also true at the end
            while self._commands.poll():
                received.append(self._commands.recv())
        return received


@pytest.fixture
def old_device() -> Iterator[tuple[OldDevice, pathlib.Path]]:
    # Not in tmp_path: Unix socket paths are limited to 107 bytes, which the
    # pytest temporary directory exceeds on CI.
    with tempfile.TemporaryDirectory(prefix="pb-") as tmp:
        path = pathlib.Path(tmp) / "old-device"
        dev = OldDevice(path)
        yield dev, path
        dev.commands()


@pytest.mark.parametrize("size", [0, AUTH_DATA_SIZE + 1, 0x10000])
def test_token_size_is_checked_by_the_host(
    old_device: tuple[OldDevice, pathlib.Path], size: int
) -> None:
    dev, path = old_device
    s = punchboot.Session(socket_path=path)
    with pytest.raises(punchboot.ArgumentError):
        s.authenticate_dsa_token(bytes(size), 0xA90F9680)
    s.close()
    assert PB_CMD_AUTHENTICATE not in dev.commands()


def test_empty_password_is_checked_by_the_host(old_device: tuple[OldDevice, pathlib.Path]) -> None:
    dev, path = old_device
    s = punchboot.Session(socket_path=path)
    with pytest.raises(punchboot.ArgumentError):
        s.authenticate("")
    s.close()
    assert PB_CMD_AUTHENTICATE not in dev.commands()
