"""Finding USB devices whose serial number can't be read (F-159).

pb 2.0.0's cdns3 driver (the i.MX8QXP MEK) couldn't send its serial number
string. Every controller's driver sends it since (usb.md 3.1), but such a
device is still in the field, so the host asks one whose serial number it
can't read for its UUID with PB_CMD_DEVICE_IDENTIFIER_READ.
These tests use fake USB devices and a scripted transport, no hardware.
"""

from __future__ import annotations

import ctypes
import uuid
from typing import Any

import pytest
from punchboot import transport, wire
from punchboot.errors import TransferError
from punchboot.wire import Command

MEK_UUID = uuid.UUID("db55e2ff-9114-3b77-bf4e-d98eb048b469")
OTHER_UUID = uuid.UUID("0da92402-437e-3979-bedc-d596539340b5")


class IdentifyingDevice(transport.Transport):
    """Answers PB_CMD_DEVICE_IDENTIFIER_READ with 'uu', or 'result_code'."""

    def __init__(self, uu: uuid.UUID, result_code: int = 0, fail: bool = False) -> None:
        """Make a device with UUID 'uu' that fails the read when 'fail' is set."""
        self.uu = uu
        self.result_code = result_code
        self.fail = fail
        self.commands: list[Command] = []
        self.closed = False

    def write(self, data: bytes) -> None:
        """Take a command."""
        self.commands.append(Command(wire.pb_command.unpack(data).command))

    def read(self, size: int) -> bytes:
        """Answer the identifier command."""
        if self.fail:
            raise TransferError
        ident = wire.pb_result_device_identifier(
            device_uuid=(ctypes.c_uint8 * 16)(*self.uu.bytes), board_id=b"imx8qxpmek"
        )
        result = wire.pb_result(magic=wire.PB_WIRE_MAGIC, result_code=self.result_code)
        data = ident.pack()
        ctypes.memmove(result.response, data, len(data))
        assert size == wire.pb_result.sizeof()
        return result.pack()

    def close(self) -> None:
        """Release the device."""
        self.closed = True


def test_identify() -> None:
    """The UUID comes from the identifier command."""
    dev = IdentifyingDevice(MEK_UUID)
    assert transport.identify(dev) == str(MEK_UUID)
    assert dev.commands == [Command.PB_CMD_DEVICE_IDENTIFIER_READ]


def test_identify_fails() -> None:
    """A failed transfer or an error result is no UUID."""
    assert transport.identify(IdentifyingDevice(MEK_UUID, fail=True)) is None
    assert transport.identify(IdentifyingDevice(MEK_UUID, result_code=-1)) is None


@pytest.fixture
def usb(monkeypatch: pytest.MonkeyPatch) -> dict[str, Any]:
    """Two fake USB devices: one with a serial number, the MEK without."""
    fake: dict[str, Any] = {
        "devices": ["other", "mek"],
        "serials": {"other": str(OTHER_UUID), "mek": None},
        "opened": [],
    }

    def open_device(device_uuid: str | None = None, dev: Any = None) -> IdentifyingDevice:  # noqa: ANN401
        assert device_uuid is None
        fake["opened"].append(dev)
        return IdentifyingDevice(MEK_UUID)

    monkeypatch.setattr(transport, "_usb_devices", lambda: iter(fake["devices"]))
    monkeypatch.setattr(transport, "_usb_serial", lambda dev: fake["serials"][dev])
    monkeypatch.setattr(transport, "UsbTransport", open_device)
    return fake


def test_list_without_serial(usb: dict[str, Any]) -> None:
    """A device without a readable serial number is listed by its identifier."""
    assert transport.list_usb_devices() == [str(OTHER_UUID), str(MEK_UUID)]
    # Only the device without a serial number is opened
    assert usb["opened"] == ["mek"]


@pytest.mark.usefixtures("usb")
def test_list_device_that_cant_be_opened(monkeypatch: pytest.MonkeyPatch) -> None:
    """A device that can't be opened (e.g. in use) is left out."""

    def busy(*_args: object, **_kwargs: object) -> None:
        raise transport.GenericError

    monkeypatch.setattr(transport, "UsbTransport", busy)
    assert transport.list_usb_devices() == [str(OTHER_UUID)]
