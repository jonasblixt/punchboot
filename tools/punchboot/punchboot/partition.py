"""Punchboot Partition class."""

from __future__ import annotations

import uuid  # noqa: TCH003
from dataclasses import dataclass
from enum import Flag


class PartitionFlags(Flag):
    """Partition flags.

    Flags:

    FLAG_BOOTABLE: Partition is bootable
    FLAG_OTP: Partition is OTP, One time programmable"
    FLAG_WRITABLE: Partition is writable
    FLAG_ERASE_BEFORE_WRITE: Partition must be erased before it can be written to
    """

    FLAG_BOOTABLE = 1 << 0
    FLAG_OTP = 1 << 1
    FLAG_WRITABLE = 1 << 2
    FLAG_ERASE_BEFORE_WRITE = 1 << 3
    FLAG_UNUSED1 = 1 << 5
    FLAG_READABLE = 1 << 6


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
    partition_flags:
        Flags for the partition
    """

    uuid: uuid.UUID
    description: str
    first_block: int
    last_block: int
    block_size: int
    partition_flags: PartitionFlags

    @property
    def bootable(self) -> bool:
        """Get if the partition is bootable."""
        return PartitionFlags.FLAG_BOOTABLE in self.partition_flags

    @property
    def otp(self) -> bool:
        """Get if partition is one time programmable."""
        return PartitionFlags.FLAG_OTP in self.partition_flags

    @property
    def writable(self) -> bool:
        """Get if partition is writable."""
        return PartitionFlags.FLAG_WRITABLE in self.partition_flags

    @property
    def erase_before_write(self) -> bool:
        """Get if partition requires erase before writing."""
        return PartitionFlags.FLAG_ERASE_BEFORE_WRITE in self.partition_flags

    @property
    def readable(self) -> bool:
        """Get if partition is readable."""
        return PartitionFlags.FLAG_READABLE in self.partition_flags
