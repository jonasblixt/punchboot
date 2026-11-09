"""Transports: USB (pyusb and libusb) and unix domain sockets (pb-host, for tests).

A transport moves bytes, the protocol is in session.py. Each write() and
read() is one transfer: over USB the transfer boundaries are part of the
protocol where a transfer ends in a short packet, so callers must not merge
or split them.
"""

from __future__ import annotations

import builtins
import contextlib
import logging
import socket
import uuid
from typing import TYPE_CHECKING, Any

from . import wire
from .errors import (
    Error,
    GenericError,
    NotFoundError,
    NotSupportedError,
    TimeoutError,  # noqa: A004
    TransferError,
)

if TYPE_CHECKING:
    import pathlib
    from collections.abc import Iterator

logger = logging.getLogger("pb")

PB_USB_VID = 0x1209
PB_USB_PID = 0x2019
USB_EP_IN = 0x81
USB_EP_OUT = 0x02
USB_INTERFACE = 0
USB_TIMEOUT_MS = 10000

# pb-host answers within milliseconds. The timeout is only there so that a
# hung pb-host can't hang the tests.
SOCKET_TIMEOUT_S = 60.0


class Transport:
    """A connection to one device."""

    def write(self, data: bytes) -> None:
        """Send one transfer."""
        raise NotImplementedError

    def read(self, size: int) -> bytes:
        """Receive one transfer of 'size' bytes."""
        raise NotImplementedError

    def close(self) -> None:
        """Close the connection."""


class SocketTransport(Transport):
    """pb-host's command mode on a unix domain socket."""

    def __init__(self, path: pathlib.Path | str, timeout: float | None = SOCKET_TIMEOUT_S) -> None:
        """Connect to the socket at 'path'."""
        if not hasattr(socket, "AF_UNIX"):
            raise NotSupportedError
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._sock.settimeout(timeout)
        try:
            self._sock.connect(str(path))
        except OSError as e:
            self._sock.close()
            raise NotFoundError from e

    def write(self, data: bytes) -> None:
        """Send all of 'data'."""
        try:
            self._sock.sendall(data)
        except builtins.TimeoutError as e:
            raise TimeoutError from e
        except OSError as e:
            raise GenericError from e

    def read(self, size: int) -> bytes:
        """Receive exactly 'size' bytes."""
        data = bytearray()
        while len(data) < size:
            try:
                chunk = self._sock.recv(size - len(data))
            except builtins.TimeoutError as e:
                raise TimeoutError from e
            except OSError as e:
                raise GenericError from e
            if not chunk:
                msg = "Connection closed by the device"
                raise GenericError(msg)
            data += chunk
        return bytes(data)

    def close(self) -> None:
        """Close the socket."""
        self._sock.close()


def _usb_backend() -> Any:  # noqa: ANN401
    """Return the libusb backend: the one from libusb-package, or the system's."""
    try:
        import libusb_package  # noqa: PLC0415
    except ImportError:
        return None
    return libusb_package.get_libusb1_backend()


def _usb_devices() -> Iterator[Any]:
    """Yield the punchboot USB devices."""
    try:
        import usb.core  # noqa: PLC0415
    except ImportError as e:
        msg = "USB needs pyusb (pip install pyusb libusb-package)"
        raise NotSupportedError(msg) from e
    try:
        devices = usb.core.find(
            find_all=True, idVendor=PB_USB_VID, idProduct=PB_USB_PID, backend=_usb_backend()
        )
    except usb.core.NoBackendError as e:
        msg = "No libusb found (pip install libusb-package, or install libusb)"
        raise NotSupportedError(msg) from e
    yield from devices


def _usb_serial(dev: Any) -> str | None:  # noqa: ANN401
    """Return a device's serial number, its UUID, or None if it can't be read."""
    import usb.core  # noqa: PLC0415
    import usb.util  # noqa: PLC0415

    try:
        serial: str = usb.util.get_string(dev, dev.iSerialNumber)
    except (usb.core.USBError, ValueError, NotImplementedError):
        return None
    finally:
        usb.util.dispose_resources(dev)
    return serial


def identify(transport: Transport) -> str | None:
    """Return the UUID the device reports (PB_CMD_DEVICE_IDENTIFIER_READ), or None."""
    try:
        transport.write(wire.command(wire.Command.PB_CMD_DEVICE_IDENTIFIER_READ))
        result = wire.pb_result.unpack(transport.read(wire.pb_result.sizeof()))
    except Error:
        logger.debug("No identifier", exc_info=True)
        return None
    if result.magic != wire.PB_WIRE_MAGIC or result.result_code != 0:
        return None
    ident = wire.pb_result_device_identifier.unpack(bytes(result.response))
    return str(uuid.UUID(bytes=bytes(ident.device_uuid)))


def _usb_uuid(dev: Any) -> str | None:  # noqa: ANN401
    """Return a device's UUID: its serial number, or what it reports over the wire.

    pb's cdns3 driver (the i.MX8QXP MEK) can't send the serial number string
    (F-159), the identifier command works on every device.
    """
    serial = _usb_serial(dev)
    if serial is not None:
        return serial
    try:
        transport = UsbTransport(dev=dev)
    except Error:
        logger.debug("Can't open the device", exc_info=True)
        return None
    try:
        return identify(transport)
    finally:
        transport.close()


def list_usb_devices() -> list[str]:
    """Return the UUIDs of the attached punchboot devices."""
    try:
        return [s for dev in _usb_devices() if (s := _usb_uuid(dev)) is not None]
    except NotSupportedError:
        logger.debug("USB is not available", exc_info=True)
        return []


class UsbTransport(Transport):
    """A punchboot device on USB."""

    def __init__(self, device_uuid: str | None = None, dev: Any = None) -> None:  # noqa: ANN401
        """Open the device 'dev', the one with UUID 'device_uuid', or the first one found."""
        import usb.core  # noqa: PLC0415
        import usb.util  # noqa: PLC0415

        self._usb = usb
        if dev is None:
            dev = next(
                (
                    d
                    for d in _usb_devices()
                    if device_uuid is None or _usb_uuid(d) == device_uuid
                ),
                None,
            )
        if dev is None:
            raise NotFoundError
        try:
            with contextlib.suppress(NotImplementedError, usb.core.USBError):
                if dev.is_kernel_driver_active(USB_INTERFACE):
                    dev.detach_kernel_driver(USB_INTERFACE)
            usb.util.claim_interface(dev, USB_INTERFACE)
        except usb.core.USBError as e:
            usb.util.dispose_resources(dev)
            raise GenericError(str(e)) from e
        self._dev = dev

    def write(self, data: bytes) -> None:
        """Send one bulk transfer."""
        try:
            written = self._dev.write(USB_EP_OUT, data, USB_TIMEOUT_MS)
        except self._usb.core.USBError as e:
            raise TransferError(str(e)) from e
        if written != len(data):
            msg = f"Sent {written} of {len(data)} bytes"
            raise TransferError(msg)

    def read(self, size: int) -> bytes:
        """Receive one bulk transfer of 'size' bytes."""
        try:
            data = bytes(self._dev.read(USB_EP_IN, size, USB_TIMEOUT_MS))
        except self._usb.core.USBError as e:
            raise TransferError(str(e)) from e
        if len(data) != size:
            msg = f"Received {len(data)} of {size} bytes"
            raise TransferError(msg)
        return data

    def close(self) -> None:
        """Release the device."""
        with contextlib.suppress(self._usb.core.USBError):
            self._usb.util.release_interface(self._dev, USB_INTERFACE)
        self._usb.util.dispose_resources(self._dev)
