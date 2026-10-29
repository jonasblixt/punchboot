"""Punchboot wire protocol, a mirror of include/pb-tools/wire.h and error.h.

The layout is frozen (D-008, doc/dev/compatibility.md): test_compat.py checks
every value, struct size and field offset here against tests/compat/abi.txt.
"""

# The struct classes are named and laid out like the C structs they mirror
# ruff: noqa: D101, N801

from __future__ import annotations

import ctypes
from enum import IntEnum
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from typing_extensions import Self

PB_WIRE_MAGIC = 0x50424C30  # 'PBL0'
PB_COMMAND_REQUEST_MAX_SIZE = 504
PB_RESULT_RESPONSE_MAX_SIZE = 504


class AuthMethod(IntEnum):
    """enum pb_auth_method."""

    PB_AUTH_INVALID = 0
    PB_AUTH_ASYM_TOKEN = 1
    PB_AUTH_PASSWORD = 2


class Command(IntEnum):
    """enum pb_commands. Only append, never renumber (D-008)."""

    PB_CMD_INVALID = 0
    PB_CMD_DEVICE_RESET = 1
    PB_CMD_DEVICE_IDENTIFIER_READ = 2
    PB_CMD_DEVICE_READ_CAPS = 3
    PB_CMD_SLC_SET_CONFIGURATION = 4
    PB_CMD_SLC_SET_CONFIGURATION_LOCK = 5
    PB_CMD_SLC_SET_EOL = 6
    PB_CMD_SLC_REVOKE_KEY = 7
    PB_CMD_SLC_READ = 8
    PB_CMD_BOOTLOADER_VERSION_READ = 9
    PB_CMD_PART_TBL_READ = 10
    PB_CMD_PART_TBL_INSTALL = 11
    PB_CMD_PART_VERIFY = 12
    PB_CMD_PART_ACTIVATE = 13
    PB_CMD_PART_BPAK_READ = 14
    PB_CMD_PART_ERASE = 15
    PB_CMD_AUTHENTICATE = 16
    PB_CMD_AUTH_SET_PASSWORD = 17
    PB_CMD_STREAM_INITIALIZE = 18
    PB_CMD_STREAM_PREPARE_BUFFER = 19
    PB_CMD_STREAM_WRITE_BUFFER = 20
    PB_CMD_STREAM_FINALIZE = 21
    PB_CMD_BOOT_PART = 22
    PB_CMD_BOOT_BPAK = 23
    PB_CMD_BOARD_COMMAND = 24
    PB_CMD_BOARD_STATUS_READ = 25
    PB_CMD_STREAM_READ_BUFFER = 26
    PB_CMD_PART_RESIZE = 27
    PB_CMD_BOOT_STATUS = 28
    PB_CMD_LOG_READ = 29


class Result(IntEnum):
    """enum pb_results. The device sends them negated in pb_result.result_code."""

    PB_RESULT_OK = 0
    PB_RESULT_ERROR = 1
    PB_RESULT_AUTHENTICATION_FAILED = 2
    PB_RESULT_NOT_AUTHENTICATED = 3
    PB_RESULT_NOT_SUPPORTED = 4
    PB_RESULT_INVALID_ARGUMENT = 5
    PB_RESULT_INVALID_COMMAND = 6
    PB_RESULT_PART_VERIFY_FAILED = 7
    PB_RESULT_PART_NOT_BOOTABLE = 8
    PB_RESULT_NO_MEMORY = 9
    PB_RESULT_TRANSFER_ERROR = 10
    PB_RESULT_NOT_FOUND = 11
    PB_RESULT_STREAM_NOT_INITIALIZED = 12
    PB_RESULT_TIMEOUT = 13
    PB_RESULT_KEY_REVOKED = 14
    PB_RESULT_SIGNATURE_ERROR = 15
    PB_RESULT_MEM_ERROR = 16
    PB_RESULT_IO_ERROR = 17


class Slc(IntEnum):
    """enum pb_slc."""

    PB_SLC_INVALID = 0
    PB_SLC_NOT_CONFIGURED = 1
    PB_SLC_CONFIGURATION = 2
    PB_SLC_CONFIGURATION_LOCKED = 3
    PB_SLC_EOL = 4


PB_WIRE_PART_FLAG_BOOTABLE = 1 << 0
PB_WIRE_PART_FLAG_OTP = 1 << 1
PB_WIRE_PART_FLAG_WRITABLE = 1 << 2
PB_WIRE_PART_FLAG_ERASE_BEFORE_WRITE = 1 << 3

u8 = ctypes.c_uint8
i8 = ctypes.c_int8
u16 = ctypes.c_uint16
u32 = ctypes.c_uint32
u64 = ctypes.c_uint64
char = ctypes.c_char


class Struct(ctypes.LittleEndianStructure):
    """A packed little-endian wire struct (PACK() in wire.h)."""

    _pack_ = 1
    _layout_ = "ms"  # Python 3.14 wants it with _pack_; older versions ignore it

    def pack(self) -> bytes:
        """Return the struct as bytes."""
        return bytes(self)

    @classmethod
    def unpack(cls, data: bytes) -> Self:
        """Return the struct at the start of 'data'."""
        return cls.from_buffer_copy(data[: ctypes.sizeof(cls)])

    @classmethod
    def sizeof(cls) -> int:
        """Return the size of the struct in bytes."""
        return ctypes.sizeof(cls)


class pb_command(Struct):
    _fields_ = (
        ("magic", u32),
        ("command", u8),
        ("rz", u8 * 3),
        ("request", u8 * PB_COMMAND_REQUEST_MAX_SIZE),
    )


class pb_result(Struct):
    _fields_ = (
        ("magic", u32),
        ("result_code", i8),
        ("rz", u8 * 3),
        ("response", u8 * PB_RESULT_RESPONSE_MAX_SIZE),
    )


class pb_result_device_caps(Struct):
    _fields_ = (
        ("stream_no_of_buffers", u8),
        ("stream_buffer_size", u32),
        ("operation_timeout_ms", u16),
        ("part_erase_timeout_ms", u16),
        ("bpak_stream_support", u8),
        ("chunk_transfer_max_bytes", u32),
        ("rz", u8 * 18),
    )


class pb_result_part_table_read(Struct):
    _fields_ = (
        ("no_of_entries", u8),
        ("rz", u8 * 31),
    )


class pb_result_part_table_entry(Struct):
    _fields_ = (
        ("uuid", u8 * 16),
        ("description", char * 37),
        ("first_block", u64),
        ("last_block", u64),
        ("block_size", u16),
        ("flags", u8),
        ("rz", u8 * 56),
    )


class pb_command_stream_initialize(Struct):
    _fields_ = (
        ("part_uuid", u8 * 16),
        ("rz", u8 * 16),
    )


class pb_command_stream_prepare_buffer(Struct):
    _fields_ = (
        ("size", u32),
        ("id", u8),
        ("rz", u8 * 27),
    )


class pb_command_stream_write_buffer(Struct):
    _fields_ = (
        ("size", u32),
        ("offset", u64),
        ("buffer_id", u8),
        ("rz", u8 * 19),
    )


class pb_command_stream_read_buffer(Struct):
    _fields_ = (
        ("size", u32),
        ("offset", u64),
        ("buffer_id", u8),
        ("rz", u8 * 19),
    )


class pb_command_authenticate(Struct):
    _fields_ = (
        ("method", u8),
        ("size", u16),
        ("key_id", u32),
        ("rz", u8 * 25),
    )


class pb_command_verify_part(Struct):
    _fields_ = (
        ("uuid", u8 * 16),
        ("sha256", u8 * 32),
        ("size", u32),
        ("bpak", u8),
        ("rz", u8 * 2),
    )


class pb_command_activate_part(Struct):
    _fields_ = (
        ("uuid", u8 * 16),
        ("rz", u8 * 16),
    )


class pb_command_read_bpak(Struct):
    _fields_ = (
        ("uuid", u8 * 16),
        ("rz", u8 * 16),
    )


class pb_command_erase_part(Struct):
    _fields_ = (
        ("uuid", u8 * 16),
        ("start_lba", u32),
        ("block_count", u32),
        ("rz", u8 * 8),
    )


class pb_command_install_part_table(Struct):
    _fields_ = (
        ("uu", u8 * 16),
        ("variant", u8),
    )


class pb_result_device_identifier(Struct):
    _fields_ = (
        ("device_uuid", u8 * 16),
        ("board_id", char * 16),
        ("rz", u8 * 23),
    )


class pb_result_slc(Struct):
    _fields_ = (
        ("slc", u8),
        ("rz", u8 * 31),
    )


class pb_result_slc_key_status(Struct):
    _fields_ = (
        ("active", u32 * 16),
        ("revoked", u32 * 16),
    )


class pb_command_revoke_key(Struct):
    _fields_ = (
        ("key_id", u32),
        ("rz", u8 * 28),
    )


class pb_command_boot_part(Struct):
    _fields_ = (
        ("uuid", u8 * 16),
        ("verbose", u8),
        ("rz", u8 * 15),
    )


class pb_command_bpak_boot(Struct):
    _fields_ = (
        ("verbose", u8),
        ("uuid", u8 * 16),
        ("rz", u8 * 15),
    )


class pb_command_board(Struct):
    _fields_ = (
        ("command", u32),
        ("request_size", u32),
        ("response_buffer_size", u32),
        ("rz", u8 * 20),
    )


class pb_result_board(Struct):
    _fields_ = (
        ("size", u32),
        ("rz", u8 * 28),
    )


class pb_result_board_status(Struct):
    _fields_ = (
        ("size", u32),
        ("rz", u8 * 28),
    )


class pb_result_boot_status(Struct):
    _fields_ = (
        ("uuid", u8 * 16),
        ("status", char * 16),
    )


class pb_command_log_read(Struct):
    _fields_ = (
        ("position", u32),
        ("size", u32),
        ("rz", u8 * 24),
    )


class pb_result_log_read(Struct):
    _fields_ = (
        ("position", u32),
        ("size", u32),
        ("head", u32),
        ("rz", u8 * 20),
    )


STRUCTS: tuple[type[Struct], ...] = (
    pb_command,
    pb_result,
    pb_result_device_caps,
    pb_result_part_table_read,
    pb_result_part_table_entry,
    pb_command_stream_initialize,
    pb_command_stream_prepare_buffer,
    pb_command_stream_write_buffer,
    pb_command_stream_read_buffer,
    pb_command_authenticate,
    pb_command_verify_part,
    pb_command_activate_part,
    pb_command_read_bpak,
    pb_command_erase_part,
    pb_command_install_part_table,
    pb_result_device_identifier,
    pb_result_slc,
    pb_result_slc_key_status,
    pb_command_revoke_key,
    pb_command_boot_part,
    pb_command_bpak_boot,
    pb_command_board,
    pb_result_board,
    pb_result_board_status,
    pb_result_boot_status,
    pb_command_log_read,
    pb_result_log_read,
)


def command(cmd: Command, request: Struct | bytes = b"") -> bytes:
    """Return a 512 byte pb_command with 'request' in its request field."""
    data = request.pack() if isinstance(request, Struct) else request
    if len(data) > PB_COMMAND_REQUEST_MAX_SIZE:
        msg = "Request too large"
        raise ValueError(msg)
    frame = pb_command(magic=PB_WIRE_MAGIC, command=cmd)
    ctypes.memmove(frame.request, data, len(data))
    return frame.pack()


def c_string(data: bytes) -> str:
    """Return the NUL terminated string at the start of 'data'."""
    return data.split(b"\0", 1)[0].decode(errors="replace")
