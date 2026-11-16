"""pb-host as a process: what its connections and the tests' own end do to it (F-067).

A host that goes away mid exchange must not take pb-host with it: on a
board the transport fails and command mode waits for the next host. And a
pb-host whose tests went away must not stay behind.
"""

from __future__ import annotations

import contextlib
import os
import pathlib
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
from typing import TYPE_CHECKING

from .pbtest import EXIT_TIMEOUT, PB_COMMAND_SIZE, PB_WIRE_MAGIC, START_TIMEOUT

if TYPE_CHECKING:
    from .pbtest import Device

PB_CMD_DEVICE_IDENTIFIER_READ = 2  # include/pb-tools/wire.h


def test_a_host_that_reads_nothing_does_not_end_pb_host(device: Device) -> None:
    # The host shuts its side down for reading before it sends a command:
    # pb-host's answer can't be delivered, and its send fails with EPIPE.
    # Without MSG_NOSIGNAL that was a SIGPIPE, which ended the process
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as host:
        host.settimeout(10)
        host.connect(str(device.socket))
        host.shutdown(socket.SHUT_RD)
        command = struct.pack("<IB3x", PB_WIRE_MAGIC, PB_CMD_DEVICE_IDENTIFIER_READ)
        host.sendall(command.ljust(PB_COMMAND_SIZE, b"\0"))
        host.shutdown(socket.SHUT_WR)

    # Command mode takes the next host
    with device.session() as s:
        assert str(s.device_get_uuid())
    assert device.running


# A parent that starts pb-host in command mode, prints its pid and waits
PARENT = """
import subprocess, sys, time
binary, workdir, sock = sys.argv[1:]
args = [binary, "--disk", workdir + "/disk.img", "--fuses", workdir + "/fuses.img",
        "--socket", sock, "--command-mode"]
log = open(workdir + "/pb.log", "wb")
print(subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT).pid, flush=True)
time.sleep(60)
"""


def _alive(pid: int) -> bool:
    """Whether 'pid' runs: neither gone nor a zombie that waits to be reaped."""
    try:
        stat = pathlib.Path(f"/proc/{pid}/stat").read_text()
    except (FileNotFoundError, ProcessLookupError):  # Gone, also while it is read
        return False
    return stat.rpartition(")")[2].split()[0] != "Z"


def test_pb_host_ends_with_its_parent(pb_host_binary: pathlib.Path, tmp_path: pathlib.Path) -> None:
    # The tests killed (a timeout, an interrupt) left pb-host waiting for a
    # host, for ever. It asks for SIGKILL when its parent ends
    sock_dir = pathlib.Path(tempfile.mkdtemp(prefix="pb-"))
    sock = sock_dir / "pb.sock"
    parent = subprocess.Popen(  # noqa: S603
        [sys.executable, "-c", PARENT, str(pb_host_binary), str(tmp_path), str(sock)],
        stdout=subprocess.PIPE,
        text=True,
    )
    pid = None
    try:
        assert parent.stdout is not None
        pid = int(parent.stdout.readline())
        deadline = time.monotonic() + START_TIMEOUT
        while not sock.exists():
            assert time.monotonic() < deadline, "pb-host didn't enter command mode"
            time.sleep(0.01)

        parent.kill()
        parent.wait()

        deadline = time.monotonic() + EXIT_TIMEOUT
        while _alive(pid):
            assert time.monotonic() < deadline, "pb-host outlived its parent"
            time.sleep(0.01)
    finally:
        if parent.poll() is None:
            parent.kill()
            parent.wait()
        if pid is not None and _alive(pid):
            # It may be reaped in between
            with contextlib.suppress(ProcessLookupError):
                os.kill(pid, signal.SIGKILL)
        shutil.rmtree(sock_dir, ignore_errors=True)
