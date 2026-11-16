"""Released hosts against today's device (D-008, roadmap guard 3).

Old hosts must work with new devices (compatibility.md). These tests run a
released punchboot command line tool against pb-host over its socket: what
it shows, writes and boots. The tools are not part of the tree: name them,
separated by colons, in PB_OLD_HOSTS, each the 'punchboot' of a Python
environment with that release installed from PyPI, e.g.

    python3 -m venv /tmp/pb-2.7.1
    /tmp/pb-2.7.1/bin/pip install punchboot==2.7.1
    PB_OLD_HOSTS=/tmp/pb-2.7.1/bin/punchboot pytest tests/integration/test_old_hosts.py

Without the variable the tests are skipped.
"""

from __future__ import annotations

import os
import pathlib
import subprocess

import pytest

from .pbtest import REPO_ROOT, SYSTEM_A_UUID, SYSTEM_B_UUID, Device

OLD_HOSTS = [pathlib.Path(p) for p in os.environ.get("PB_OLD_HOSTS", "").split(":") if p]

pytestmark = pytest.mark.skipif(not OLD_HOSTS, reason="no released host named in PB_OLD_HOSTS")

GOLDEN = REPO_ROOT / "tests" / "compat" / "bpak" / "v1.0.1" / "sha256-secp256r1.bpak"


def host_id(host: pathlib.Path) -> str:
    """Name a host by its environment's directory."""
    return host.parent.parent.name


@pytest.fixture(params=OLD_HOSTS, ids=[host_id(h) for h in OLD_HOSTS])
def old_host(request: pytest.FixtureRequest) -> pathlib.Path:
    """Return a released host's punchboot tool."""
    host: pathlib.Path = request.param
    return host


def run(host: pathlib.Path, device: Device, *args: str) -> subprocess.CompletedProcess[str]:
    """Run the released tool against 'device' over its socket."""
    return subprocess.run(  # noqa: S603
        [str(host), "-t", "socket", "-s", str(device.socket), *args],
        check=False,
        capture_output=True,
        text=True,
        stdin=subprocess.DEVNULL,
        timeout=60,
    )


def test_dev_show(old_host: pathlib.Path, device: Device) -> None:
    result = run(old_host, device, "dev", "show")

    assert result.returncode == 0, result.stderr
    assert "host test" in result.stdout


def test_part_list(old_host: pathlib.Path, device: Device) -> None:
    result = run(old_host, device, "part", "list")

    assert result.returncode == 0, result.stderr
    assert str(SYSTEM_A_UUID) in result.stdout


@pytest.mark.parametrize("size", [1, 511, 4096, 100003])
def test_part_write_and_verify(
    old_host: pathlib.Path, device: Device, tmp_path: pathlib.Path, size: int
) -> None:
    data = tmp_path / "data.bin"
    other = tmp_path / "other.bin"
    data.write_bytes(bytes((i * 7 + 3) & 0xFF for i in range(size)))
    other.write_bytes(bytes((i * 11 + 5) & 0xFF for i in range(size)))

    result = run(old_host, device, "part", "write", str(data), str(SYSTEM_B_UUID))
    assert result.returncode == 0, result.stderr

    result = run(old_host, device, "part", "verify", str(data), str(SYSTEM_B_UUID))
    assert result.returncode == 0, result.stderr

    result = run(old_host, device, "part", "verify", str(other), str(SYSTEM_B_UUID))
    assert result.returncode != 0


def test_board_and_boot_status(old_host: pathlib.Path, device: Device) -> None:
    for args in (["board", "status"], ["boot", "status"], ["slc", "show"]):
        result = run(old_host, device, *args)
        assert result.returncode == 0, (args, result.stderr)


def test_boot_enable_and_disable(old_host: pathlib.Path, device: Device) -> None:
    result = run(old_host, device, "boot", "enable", str(SYSTEM_A_UUID))
    assert result.returncode == 0, result.stderr
    with device.session() as s:
        assert s.boot_status()[0] == SYSTEM_A_UUID

    result = run(old_host, device, "boot", "disable")
    assert result.returncode == 0, result.stderr
    with device.session() as s:
        assert s.boot_status()[0] != SYSTEM_A_UUID


def test_boot_bpak_from_ram(old_host: pathlib.Path, device: Device) -> None:
    result = run(old_host, device, "boot", "bpak", str(GOLDEN))

    assert result.returncode == 0, result.stderr
    assert device.wait_exit() == 0


def test_dev_reset(old_host: pathlib.Path, device: Device) -> None:
    result = run(old_host, device, "dev", "reset")

    assert result.returncode == 0, result.stderr
    assert device.wait_exit() == 0
