"""Recorded wire transcripts (D-008, compatibility guard 4).

A transcript holds the raw bytes that went between a host and pb-host for a
scripted list of Session calls (a scenario, see test_transcripts.py), and what
each call returned. They are recorded once and never changed:

- Recorder sits between a Session and pb-host as a unix socket proxy and
  records each byte, per step of the scenario.
- FakeDevice serves a transcript to a Session: it checks that the host sends
  the recorded bytes and answers with the recorded responses, so the current
  host must send and understand exactly what the recorded one did.

Both run in their own process, so that they also work while the host blocks
the interpreter in a call.

The file format is text. A step starts with 'step <name>'. A segment is the
bytes sent in one direction until the other side answers: '> <size>' from the
host, '< <size>' from the device, followed by lines of hex, where 'zeros <n>'
stands for n zero bytes. 'result <value>' ends the step. Lines starting with
'#' are comments. The segments are a byte stream: the size of each transfer
inside a segment is not recorded.
"""

from __future__ import annotations

import contextlib
import dataclasses
import hashlib
import multiprocessing
import select
import socket
import uuid
from enum import Enum
from typing import TYPE_CHECKING, Any

import punchboot
import semver

if TYPE_CHECKING:
    import pathlib
    from multiprocessing.connection import Connection

HOST = ">"
DEVICE = "<"
HEX_LINE_BYTES = 32
TIMEOUT = 10.0


@dataclasses.dataclass
class Segment:
    """Bytes sent in one direction, until the other side sends."""

    direction: str
    data: bytes


@dataclasses.dataclass
class Step:
    """One Session call: the bytes on the wire and what it returned."""

    name: str
    segments: list[Segment] = dataclasses.field(default_factory=list)
    result: str = ""


def describe(value: object) -> str:
    """Return a description of a Session call's return value that doesn't depend on the host.

    The same for every Python version, so reprs of enums and dataclasses are
    not used.
    """
    if isinstance(value, bytes):
        return f"bytes {len(value)} sha256:{hashlib.sha256(value).hexdigest()}"
    if isinstance(value, punchboot.Partition):
        return (
            f"Partition({value.uuid}, {value.description!r}, {value.first_block}, "
            f"{value.last_block}, {value.block_size}, {value.partition_flags.value:#x})"
        )
    if isinstance(value, Enum):
        return value.name
    if isinstance(value, (uuid.UUID, semver.Version)):
        return str(value)
    if isinstance(value, (list, tuple)):
        return "[" + ", ".join(describe(v) for v in value) + "]"
    return repr(value)


def describe_exception(exc: BaseException) -> str:
    """Return the description of a Session call that raised."""
    return f"raise {type(exc).__name__}"


def _encode(data: bytes) -> list[str]:
    """Return the hex lines for a segment's data."""
    lines = []
    pos = 0
    while pos < len(data):
        end = pos
        while end < len(data) and data[end] == 0:
            end += 1
        if end - pos >= HEX_LINE_BYTES:
            lines.append(f"  zeros {end - pos}")
            pos = end
            continue
        line_end = min(pos + HEX_LINE_BYTES, len(data))
        lines.append(f"  {data[pos:line_end].hex()}")
        pos = line_end
    return lines


def dump(steps: list[Step], header: list[str]) -> str:
    """Return the text of a transcript."""
    lines = [f"# {h}" if h else "#" for h in header]
    for step in steps:
        lines.append("")
        lines.append(f"step {step.name}")
        for segment in step.segments:
            lines.append(f"{segment.direction} {len(segment.data)}")
            lines.extend(_encode(segment.data))
        lines.append(f"result {step.result}")
    return "\n".join(lines) + "\n"


def _decode(lines: list[str]) -> bytes:
    """Return the data of a segment's hex lines."""
    data = bytearray()
    for line in lines:
        word, _, rest = line.partition(" ")
        data += bytes(int(rest)) if word == "zeros" else bytes.fromhex(line)
    return bytes(data)


def load(path: pathlib.Path) -> list[Step]:
    """Read a transcript."""
    steps: list[Step] = []
    # The segment being read: its size and hex lines
    size = 0
    hex_lines: list[str] | None = None

    for number, raw in enumerate([*path.read_text().splitlines(), "end"], 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        word, _, rest = line.partition(" ")
        if hex_lines is not None and word in ("step", HOST, DEVICE, "result", "end"):
            data = _decode(hex_lines)
            if len(data) != size:
                msg = f"{path}:{number}: segment has {len(data)} bytes, not {size}"
                raise ValueError(msg)
            steps[-1].segments[-1].data = data
            hex_lines = None
        if word == "step":
            steps.append(Step(rest))
        elif word in (HOST, DEVICE):
            steps[-1].segments.append(Segment(word, b""))
            size = int(rest)
            hex_lines = []
        elif word == "result":
            steps[-1].result = rest
        elif hex_lines is not None:
            hex_lines.append(line)
        elif word != "end":
            msg = f"{path}:{number}: unexpected line"
            raise ValueError(msg)
    return steps


def _recv_exactly(conn: socket.socket, size: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        chunk = conn.recv(min(size - len(data), 1 << 16))
        if not chunk:
            break
        data += chunk
    return bytes(data)


class Recorder:
    """Unix socket proxy between a host and pb-host that records the bytes, per step.

    Call step() before each Session call, and steps() at the end.
    """

    def __init__(self, listen: pathlib.Path, device: pathlib.Path) -> None:
        """Listen on 'listen' and forward one connection to the device at 'device'."""
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(str(listen))
        server.listen(1)
        self._control, child = multiprocessing.Pipe()
        self._process = multiprocessing.get_context("fork").Process(
            target=self._serve, args=(server, str(device), child), daemon=True
        )
        self._process.start()
        server.close()
        child.close()

    @staticmethod
    def _serve(server: socket.socket, device_path: str, control: Connection) -> None:
        steps: list[Step] = []
        # The host and device connections, once the host connected
        conns: dict[socket.socket, tuple[socket.socket, str]] = {}
        while True:
            fds: list[Any] = [control, server, *conns]
            ready, _, _ = select.select(fds, [], [])
            if control in ready:
                msg = control.recv()
                if msg[0] != "step":
                    control.send(steps)
                    return
                steps.append(Step(msg[1]))
                control.send("ok")
            elif server in ready:
                host, _ = server.accept()
                device = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                device.connect(device_path)
                conns = {host: (device, HOST), device: (host, DEVICE)}
            else:
                conn = next(c for c in conns if c in ready)
                other, direction = conns[conn]
                if not Recorder._forward(conn, other, direction, steps):
                    # One side closed: close both, like the host or a reset would
                    for c in conns:
                        c.close()
                    conns = {}

    @staticmethod
    def _forward(
        conn: socket.socket, other: socket.socket, direction: str, steps: list[Step]
    ) -> bool:
        """Record and forward what 'conn' sent. Return False if it closed."""
        try:
            data = conn.recv(1 << 16)
        except ConnectionResetError:
            data = b""
        if not data:
            return False
        if not steps:
            steps.append(Step("connect"))
        segments = steps[-1].segments
        if segments and segments[-1].direction == direction:
            segments[-1].data += data
        else:
            segments.append(Segment(direction, data))
        with contextlib.suppress(OSError):
            other.sendall(data)
        return True

    def step(self, name: str) -> None:
        """Start a new step. Everything sent from now on belongs to it."""
        self._control.send(("step", name))
        self._control.recv()

    def steps(self) -> list[Step]:
        """Stop recording and return the steps."""
        self._control.send(("done",))
        steps: list[Step] = self._control.recv()
        self._process.join(timeout=TIMEOUT)
        self._process.kill()
        return steps


class FakeDevice:
    """Serves one connection from a transcript, and checks what the host sends."""

    def __init__(self, listen: pathlib.Path, steps: list[Step]) -> None:
        """Listen on 'listen' and serve the segments of 'steps' to one connection."""
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(str(listen))
        server.listen(1)
        self._control, child = multiprocessing.Pipe()
        self._process = multiprocessing.get_context("fork").Process(
            target=self._serve, args=(server, steps, child), daemon=True
        )
        self._process.start()
        server.close()
        child.close()

    @staticmethod
    def _serve(server: socket.socket, steps: list[Step], control: Connection) -> None:
        server.settimeout(TIMEOUT)
        try:
            conn, _ = server.accept()
        except TimeoutError:
            control.send("the host never connected")
            return
        conn.settimeout(TIMEOUT)
        with conn:
            for step in steps:
                for n, segment in enumerate(step.segments):
                    where = f"step {step.name}, segment {n}"
                    if segment.direction == DEVICE:
                        try:
                            conn.sendall(segment.data)
                        except OSError as e:
                            control.send(f"{where}: the host closed the connection ({e})")
                            return
                        continue
                    try:
                        data = _recv_exactly(conn, len(segment.data))
                    except TimeoutError:
                        control.send(f"{where}: timeout waiting for the host")
                        return
                    if data != segment.data:
                        control.send(f"{where}: {_difference(segment.data, data)}")
                        return
            # The host must not send anything more
            try:
                extra = conn.recv(1)
            except (TimeoutError, OSError):
                extra = b""
            if extra:
                control.send("the host sent more than the transcript")
                return
        control.send("")

    def problem(self) -> str:
        """Wait until the host is done, return what went wrong or ''."""
        if not self._control.poll(TIMEOUT * 2):
            self._process.kill()
            return "the fake device did not finish"
        problem: str = self._control.recv()
        self._process.join(timeout=TIMEOUT)
        self._process.kill()
        return problem


def _difference(expected: bytes, got: bytes) -> str:
    if len(got) < len(expected):
        return f"the host sent {len(got)} bytes, expected {len(expected)}"
    offset = next(i for i, (a, b) in enumerate(zip(expected, got, strict=True)) if a != b)
    return (
        f"the host sent other bytes at offset {offset}: "
        f"expected {expected[offset : offset + 16].hex()}, got {got[offset : offset + 16].hex()}"
    )
