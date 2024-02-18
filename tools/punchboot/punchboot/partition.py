"""Punchboot Partition class."""

import uuid
from dataclasses import dataclass
from enum import IntEnum


class PartitionFlags(IntEnum):
    """Partition flags.

    Flags:

    FLAG_BOOTABLE: Partition is bootable
    FLAG_OTP: Partition is OTP, One time programmable"
    FLAG_WRITABLE: Partition is writable
    FLAG_ERASE_BEFORE_WRITE: Partition must be erased before it can be written to
    """

    FLAG_BOOTABLE = (0,)
    FLAG_OTP = (1,)
    FLAG_WRITABLE = (2,)
    FLAG_ERASE_BEFORE_WRITE = (3,)
    FLAG_READABLE = (6,)


@dataclass(frozen=True)
class Partition:
    """Partition meta data.

    Parameters
    ----------
    uuid:
        Partition UUID
    description:
        Textual description of the partition
    first_block:
        Index of first block
    last_block:
        Index of last block
    block_size:
        Size of a block in bytes
    otp:
        One Time Programmable. This partition can only be written once
    writable:
        The partition is writable
    erase_before_write:
        The partition must be erased before it can be written to
    """

    uuid: uuid.UUID
    description: str
    first_block: int
    last_block: int
    block_size: int
    bootable: bool
    otp: bool
    writable: bool
    readable: bool
    erase_before_write: bool
