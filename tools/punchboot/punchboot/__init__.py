"""Punchboot."""

from .session import Session
from .helpers import library_version, pb_id, wait_for_device, list_usb_devices
from .partition import Partition, PartitionFlags
from .slc import SLC

from _punchboot import (  # type: ignore # noqa: F401
    GenericError,
    AuthenticationError,
    NotAuthenticatedError,
    NotSupportedError,
    ArgumentError,
    CommandError,
    PartVerifyError,
    PartNotBootableError,
    NoMemoryError,
    TransferError,
    NotFoundError,
    StreamNotInitializedError,
    TimeoutError,
    KeyRevokedError,
    SignatureError,
    MemError,
    IOError,
)

_pb_exceptions = [
    "GenericError",
    "AuthenticationError",
    "NotAuthenticatedError",
    "NotSupportedError",
    "ArgumentError",
    "CommandError",
    "PartVerifyError",
    "PartNotBootableError",
    "NoMemoryError",
    "TransferError",
    "NotFoundError",
    "StreamNotInitializedError",
    "TimeoutError",
    "KeyRevokedError",
    "SignatureError",
    "MemError",
    "IOError",
]

__all__ = [
    "Session",
    "Partition",
    "PartitionFlags",
    "SLC",
    "library_version",
    "pb_id",
    "wait_for_device",
    "list_usb_devices",
]
__all__ += _pb_exceptions
