"""Recorded wire transcripts (D-008, compatibility guard 4), see transcript.py.

Each scenario is a list of Session calls on one connection. Its transcript in
tests/compat/transcripts was recorded once, from the host of that time against
pb-host, and is never changed. The tests serve each transcript to the current
host and check that it sends the same bytes and returns the same values, so
it keeps working with devices that answer like the recorded one.

To record the transcript of a new scenario (an existing one is never recorded
again):

    PB_RECORD_TRANSCRIPTS=1 pytest tests/integration -k transcripts

A scenario's steps and setup are frozen once its transcript is committed. To
cover more, add a scenario.
"""

from __future__ import annotations

import dataclasses
import os
import pathlib
import tempfile
from importlib.metadata import version
from typing import TYPE_CHECKING

import punchboot
import pytest
from punchboot.transport import SocketTransport, Transport

from . import transcript
from .pbtest import (
    AUTH_TOKEN,
    DEV_KEY_ID,
    DISK_UUID,
    READABLE_UUID,
    REPO_ROOT,
    SYSTEM_A_UUID,
    SYSTEM_B_UUID,
    Device,
)

if TYPE_CHECKING:
    from collections.abc import Callable, Iterator

TRANSCRIPTS = REPO_ROOT / "tests" / "compat" / "transcripts"
RECORD = os.environ.get("PB_RECORD_TRANSCRIPTS") == "1"

GOLDEN_IMAGE = REPO_ROOT / "tests" / "compat" / "bpak" / "v0.9.3" / "two-parts.bpak"
UNKNOWN_UUID = "8ff2ad4c-b4a6-4f17-9d6a-0d1c8bd05b10"
DATA = bytes(range(256)) * 32  # 8 KiB, two of pb-host's chunks
CHUNK = 4 * 1024  # pb-host's chunk_transfer_max_bytes (CONFIG_CM_BUF_SIZE_KiB)


def _files(tmp: pathlib.Path) -> None:
    """Create the files that the steps use."""
    (tmp / "data").write_bytes(DATA)
    (tmp / "other").write_bytes(DATA[::-1])
    (tmp / "too-large").write_bytes(bytes(512 * 1024 + 1))  # System A is 512 KiB


def _read(s: punchboot.Session, tmp: pathlib.Path, part: str | object) -> bytes:
    out = tmp / "read"
    s.part_read(out, part)  # type: ignore[arg-type]
    return out.read_bytes()



@dataclasses.dataclass
class Scenario:
    """Session calls on one connection, recorded from 'device' after 'setup'."""

    name: str
    steps: list[tuple[str, Callable[[punchboot.Session, pathlib.Path], object]]]
    setup: Callable[[Device, pathlib.Path], None] | None = None


def _install_table(device: Device, _tmp: pathlib.Path) -> None:
    with device.session() as s:
        s.part_table_install(DISK_UUID)
    device.reboot()


def _install_table_and_image(device: Device, tmp: pathlib.Path) -> None:
    _install_table(device, tmp)
    with device.session() as s:
        s.part_write(GOLDEN_IMAGE, SYSTEM_A_UUID)


SCENARIOS = [
    Scenario(
        "blank",
        steps=[
            ("device_get_uuid", lambda s, _: s.device_get_uuid()),
            ("device_get_boardname", lambda s, _: s.device_get_boardname()),
            ("device_get_punchboot_version", lambda s, _: s.device_get_punchboot_version()),
            ("part_get_partitions", lambda s, _: s.part_get_partitions()),
            ("boot_set_boot_part_none", lambda s, _: s.boot_set_boot_part(None)),
            ("part_table_install", lambda s, _: s.part_table_install(DISK_UUID)),
            ("device_reset", lambda s, _: s.device_reset()),
        ],
    ),
    Scenario(
        "info",
        setup=_install_table,
        steps=[
            ("part_get_partitions", lambda s, _: s.part_get_partitions()),
            ("boot_status", lambda s, _: s.boot_status()),
            ("board_read_status", lambda s, _: s.board_read_status()),
            (
                "board_run_command",
                lambda s, _: s.board_run_command("test-command", b"0x11223344"),
            ),
            ("board_run_command_no_args", lambda s, _: s.board_run_command("test-command")),
            ("board_run_command_unknown", lambda s, _: s.board_run_command("test-unknown")),
            (
                "board_run_command_failing",
                lambda s, _: s.board_run_command("test-command2", b"0x00112233"),
            ),
            ("slc_get_lifecycle", lambda s, _: s.slc_get_lifecycle()),
            ("slc_get_active_keys", lambda s, _: s.slc_get_active_keys()),
            ("slc_get_revoked_keys", lambda s, _: s.slc_get_revoked_keys()),
            ("auth_set_password", lambda s, _: s.auth_set_password("secret")),
            ("authenticate_password", lambda s, _: s.authenticate("secret")),
            ("authenticate_empty_password", lambda s, _: s.authenticate("")),
            ("log_read", lambda s, _: s.log_read()),
            ("device_reset", lambda s, _: s.device_reset()),
        ],
    ),
    Scenario(
        "partitions",
        setup=_install_table,
        steps=[
            ("part_write", lambda s, t: s.part_write(t / "data", SYSTEM_A_UUID)),
            ("part_verify", lambda s, t: s.part_verify(t / "data", SYSTEM_A_UUID)),
            ("part_verify_bytes", lambda s, _: s.part_verify(DATA, str(SYSTEM_A_UUID))),
            ("part_verify_other", lambda s, t: s.part_verify(t / "other", SYSTEM_A_UUID)),
            ("part_write_bpak", lambda s, _: s.part_write(GOLDEN_IMAGE, SYSTEM_B_UUID)),
            ("part_verify_bpak", lambda s, _: s.part_verify(GOLDEN_IMAGE, SYSTEM_B_UUID)),
            ("part_write_readable", lambda s, t: s.part_write(t / "data", READABLE_UUID)),
            ("part_read", lambda s, t: _read(s, t, READABLE_UUID)),
            ("part_read_not_readable", lambda s, t: _read(s, t, SYSTEM_A_UUID)),
            ("part_erase", lambda s, _: s.part_erase(SYSTEM_A_UUID)),
            ("part_write_unknown", lambda s, t: s.part_write(t / "data", UNKNOWN_UUID)),
            ("part_write_too_large", lambda s, t: s.part_write(t / "too-large", SYSTEM_A_UUID)),
            ("part_erase_unknown", lambda s, _: s.part_erase(UNKNOWN_UUID)),
            ("part_verify_unknown", lambda s, t: s.part_verify(t / "data", UNKNOWN_UUID)),
            ("device_reset", lambda s, _: s.device_reset()),
        ],
    ),
    Scenario(
        "boot_bpak",
        setup=_install_table,
        steps=[
            ("boot_set_boot_part", lambda s, _: s.boot_set_boot_part(SYSTEM_A_UUID)),
            ("boot_status", lambda s, _: s.boot_status()),
            ("boot_set_boot_part_none", lambda s, _: s.boot_set_boot_part("none")),
            ("boot_bpak_not_bpak", lambda s, t: s.boot_bpak(t / "data", SYSTEM_A_UUID)),
            (
                "boot_bpak",
                lambda s, _: s.boot_bpak(GOLDEN_IMAGE, SYSTEM_A_UUID, verbose=True),
            ),
        ],
    ),
    Scenario(
        "boot_partition",
        setup=_install_table_and_image,
        steps=[
            ("boot_partition", lambda s, _: s.boot_partition(SYSTEM_A_UUID, verbose=True)),
        ],
    ),
    Scenario(
        "security",
        setup=_install_table,
        steps=[
            ("slc_set_configuration", lambda s, _: s.slc_set_configuration()),
            ("slc_set_configuration_lock", lambda s, _: s.slc_set_configuration_lock()),
            ("slc_get_lifecycle_locked", lambda s, _: s.slc_get_lifecycle()),
            ("part_get_partitions_locked", lambda s, _: s.part_get_partitions()),
            (
                "authenticate_token_too_large",
                lambda s, _: s.authenticate_dsa_token(bytes(1025), DEV_KEY_ID),
            ),
            (
                "authenticate_token_wrong_key",
                lambda s, _: s.authenticate_dsa_token(AUTH_TOKEN, "pb-development2"),
            ),
            (
                "authenticate_token",
                lambda s, _: s.authenticate_dsa_token(AUTH_TOKEN, DEV_KEY_ID),
            ),
            ("slc_get_lifecycle", lambda s, _: s.slc_get_lifecycle()),
            ("slc_revoke_key", lambda s, _: s.slc_revoke_key("pb-development3")),
            ("slc_get_active_keys", lambda s, _: s.slc_get_active_keys()),
            ("slc_get_revoked_keys", lambda s, _: s.slc_get_revoked_keys()),
            ("slc_set_end_of_life", lambda s, _: s.slc_set_end_of_life()),
            ("slc_get_lifecycle_eol", lambda s, _: s.slc_get_lifecycle()),
            ("device_reset", lambda s, _: s.device_reset()),
        ],
    ),
]


def _run(
    scenario: Scenario, s: punchboot.Session, tmp: pathlib.Path, step: Callable[[str], None]
) -> list[str]:
    """Run the steps of a scenario, return the description of each result."""
    results = []
    for name, call in scenario.steps:
        step(name)
        try:
            results.append(transcript.describe(call(s, tmp)))
        except punchboot.Error as e:
            results.append(transcript.describe_exception(e))
    return results


@pytest.fixture
def socket_dir() -> Iterator[pathlib.Path]:
    # Not in tmp_path: unix socket paths are limited to 107 bytes
    with tempfile.TemporaryDirectory(prefix="pb-") as tmp:
        yield pathlib.Path(tmp)


def _record(
    scenario: Scenario,
    pb_host_binary: pathlib.Path,
    tmp_path: pathlib.Path,
    socket_dir: pathlib.Path,
) -> None:
    device = Device(pb_host_binary, tmp_path / "device")
    device.workdir.mkdir()
    try:
        device.start()
        if scenario.setup:
            scenario.setup(device, tmp_path)
        proxy = socket_dir / "proxy"
        recorder = transcript.Recorder(proxy, device.socket)
        s = punchboot.Session(socket_path=proxy)
        results = _run(scenario, s, tmp_path, recorder.step)
        s.close()
        steps = recorder.steps()
    finally:
        device.close()
    if device.problems:
        pytest.fail("\n".join(device.problems))

    for step, result in zip(steps, results, strict=True):
        step.result = result
    header = [
        f"Wire transcript '{scenario.name}' (D-008, compatibility guard 4).",
        "Recorded once by test_transcripts.py and never changed.",
        f"Host: punchboot {version('punchboot')}. Device: pb-host.",
        "'>' host to device, '<' device to host, 'zeros n' is n zero bytes.",
    ]
    (TRANSCRIPTS / f"{scenario.name}.txt").write_text(transcript.dump(steps, header))


@pytest.mark.parametrize("scenario", SCENARIOS, ids=[s.name for s in SCENARIOS])
def test_transcripts(
    scenario: Scenario,
    request: pytest.FixtureRequest,
    tmp_path: pathlib.Path,
    socket_dir: pathlib.Path,
) -> None:
    path = TRANSCRIPTS / f"{scenario.name}.txt"
    _files(tmp_path)
    if not path.exists():
        if not RECORD:
            pytest.fail(f"{path} is missing, record it with PB_RECORD_TRANSCRIPTS=1")
        _record(scenario, request.getfixturevalue("pb_host_binary"), tmp_path, socket_dir)

    steps = transcript.load(path)
    assert [st.name for st in steps] == [name for name, _ in scenario.steps]

    fake = transcript.FakeDevice(socket_dir / "fake", steps)
    io = RecordingTransport(SocketTransport(socket_dir / "fake"))
    s = punchboot.Session(transport=io)
    results = _run(scenario, s, tmp_path, lambda _: None)
    s.close()
    assert fake.problem() == ""
    assert results == [st.result for st in steps]

    # The transcripts don't have the transfer sizes, which matter over USB
    # where a transfer ends in a short packet. The hosts so far sent each
    # segment in one transfer, except RAM boot, which sends each part in
    # chunks of chunk_transfer_max_bytes.
    writes = iter(io.writes)
    for st in steps:
        for segment in st.segments:
            if segment.direction == transcript.HOST:
                size = len(segment.data)
                expected = [CHUNK] * (size // CHUNK) + ([size % CHUNK] if size % CHUNK else [])
                assert [next(writes, None) for _ in expected] == expected, st.name
    assert next(writes, None) is None


class RecordingTransport(Transport):
    """Passes transfers on, and records the size of each write."""

    def __init__(self, inner: Transport) -> None:
        """Pass the transfers on to 'inner'."""
        self.inner = inner
        self.writes: list[int] = []

    def write(self, data: bytes) -> None:
        """Record and send a transfer."""
        self.writes.append(len(data))
        self.inner.write(data)

    def read(self, size: int) -> bytes:
        """Receive a transfer."""
        return self.inner.read(size)

    def close(self) -> None:
        """Close the inner transport."""
        self.inner.close()
