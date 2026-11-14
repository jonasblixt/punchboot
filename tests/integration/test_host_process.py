"""pb-host as a process: what its connections and the tests' own end do to it (F-067).

A host that goes away mid exchange must not take pb-host with it: on a
board the transport fails and command mode waits for the next host. And a
pb-host whose tests went away must not stay behind.
"""

from __future__ import annotations

import socket
import struct
from typing import TYPE_CHECKING

from .pbtest import PB_COMMAND_SIZE, PB_WIRE_MAGIC

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
