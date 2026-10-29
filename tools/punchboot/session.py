"""Punchboot session class.

This is the main interface to the punchboot library.

The command sequences are frozen (D-008, doc/dev/compatibility.md): which
transfers follow a command, in which order and with which sizes. The
transcripts in tests/compat/transcripts check them against what the hosts
before this one sent.
"""

from __future__ import annotations

import contextlib
import ctypes
import hashlib
import logging
import pathlib
import uuid
from typing import IO, TYPE_CHECKING

import semver  # type: ignore[import-not-found]

from . import bpak, wire
from .errors import (
    ArgumentError,
    Error,
    GenericError,
    IOError,  # noqa: A004
    NoMemoryError,
    NotFoundError,
    NotSupportedError,
    check,
)
from .helpers import pb_id, valid_bpak_magic
from .partition import Partition, PartitionFlags
from .slc import SLC
from .transport import SocketTransport, Transport, UsbTransport
from .wire import Command, Result

if TYPE_CHECKING:
    from collections.abc import Callable, Sequence

logger = logging.getLogger("pb")

PartUUIDType = uuid.UUID | str

# Bytes asked for per log read. Devices send at most their command buffer.
LOG_READ_CHUNK_SIZE = 64 * 1024

# Authentication data and board command requests go in one transfer of this size
AUTH_DATA_SIZE = 1024
BOARD_REQUEST_SIZE = 1024
# Sizes of the C host library's buffers, which went on the wire or limited
# what it accepted
BOARD_RESPONSE_SIZE = 4096
BOARD_STATUS_SIZE = 1024
VERSION_SIZE = 64
PART_TABLE_ENTRIES = 128
PART_TABLE_ENTRIES_WRITE = 256
MAX_KEYS = 16


def _partuuid_to_uuid(uu: PartUUIDType) -> uuid.UUID:
    if isinstance(uu, str):
        return uuid.UUID(uu)

    return uu


def _u8_array(data: bytes) -> ctypes.Array[ctypes.c_uint8]:
    return (ctypes.c_uint8 * len(data)).from_buffer_copy(data)


def _is_file(file: object) -> bool:
    return hasattr(file, "read") and hasattr(file, "seek")


class Session:
    """Punchboot session class.

    This encapsulates all of functions available over the communications interface.
    """

    def __init__(
        self,
        device_uuid: uuid.UUID | str | None = None,
        socket_path: pathlib.Path | str | None = None,
        transport: Transport | None = None,
    ) -> None:
        """Initialize the punchboot session.

        Keyword arguments:
        device_uuid -- Optional UUID to address a specific device. If this
                is not given the first device that the underlying usb library
                find's will be used.
        socket_path -- Optional path to a domain socket, mainly used for
                testing the punchboot bootloader.
        transport -- Optional transport to use instead of USB or a socket.

        Exceptions:
        NotFoundError -- No device found
        """
        self._transport: Transport | None
        if transport is not None:
            self._transport = transport
        elif socket_path is not None:
            self._transport = SocketTransport(socket_path)
        else:
            uu = str(device_uuid) if device_uuid is not None else None
            self._transport = UsbTransport(uu)

    # Wire protocol

    def _io(self) -> Transport:
        if self._transport is None:
            msg = "Session is invalidated, must re-init"
            raise OSError(msg)
        return self._transport

    def _invalidate(self) -> None:
        """Close the connection after a command that resets or boots the device."""
        self.close()

    def _write(self, data: bytes) -> None:
        self._io().write(data)

    def _read(self, size: int) -> bytes:
        return self._io().read(size)

    def _send(self, cmd: Command, request: wire.Struct | bytes = b"") -> None:
        logger.debug("%s", cmd.name)
        self._write(wire.command(cmd, request))

    def _result(self) -> tuple[int, bytes]:
        """Read a pb_result, return its result code and response."""
        result = wire.pb_result.unpack(self._read(wire.pb_result.sizeof()))
        if result.magic != wire.PB_WIRE_MAGIC:
            msg = f"Bad result magic {result.magic:#x}"
            raise GenericError(msg)
        logger.debug("result %i", result.result_code)
        return int(result.result_code), bytes(result.response)

    def _command(self, cmd: Command, request: wire.Struct | bytes = b"") -> bytes:
        """Run a command without data transfers, raise on errors, return the response."""
        self._send(cmd, request)
        code, response = self._result()
        check(code)
        return response

    def _authenticate(self, method: wire.AuthMethod, key_id: int, data: bytes) -> None:
        # The data goes in one 1024 byte transfer. Devices without the check
        # answer OK to any size and wait for data that a size of 0 never sends.
        if not data or len(data) > AUTH_DATA_SIZE:
            raise ArgumentError
        auth = wire.pb_command_authenticate(
            method=method, size=len(data), key_id=key_id & 0xFFFFFFFF
        )
        self._command(Command.PB_CMD_AUTHENTICATE, auth)
        self._write(data.ljust(AUTH_DATA_SIZE, b"\0"))
        code, _ = self._result()
        check(code)

    def _read_caps(self) -> wire.pb_result_device_caps:
        response = self._command(Command.PB_CMD_DEVICE_READ_CAPS)
        return wire.pb_result_device_caps.unpack(response)

    def _transfer_caps(self) -> tuple[int, int]:
        """Return the chunk size and the number of stream buffers to use.

        Devices that predate a caps field report 0 for it (F-077).
        """
        caps = self._read_caps()
        chunk_size = caps.chunk_transfer_max_bytes or caps.stream_buffer_size
        if not chunk_size:
            msg = "The device reports no transfer size"
            raise NotSupportedError(msg)
        return chunk_size, caps.stream_no_of_buffers or 1

    def _read_identifier(self) -> wire.pb_result_device_identifier:
        response = self._command(Command.PB_CMD_DEVICE_IDENTIFIER_READ)
        return wire.pb_result_device_identifier.unpack(response)

    def _slc_read(self) -> tuple[int, Sequence[int], Sequence[int]]:
        """Return the SLC, and the active and revoked key ids."""
        response = self._command(Command.PB_CMD_SLC_READ)
        slc = wire.pb_result_slc.unpack(response).slc
        keys = wire.pb_result_slc_key_status.unpack(
            self._read(wire.pb_result_slc_key_status.sizeof())
        )
        code, _ = self._result()
        check(code)
        return slc, list(keys.active), list(keys.revoked)

    def _read_part_table(self, max_entries: int) -> list[wire.pb_result_part_table_entry]:
        response = self._command(Command.PB_CMD_PART_TBL_READ)
        entries = wire.pb_result_part_table_read.unpack(response).no_of_entries
        if entries > max_entries:
            raise NoMemoryError
        entry_size = wire.pb_result_part_table_entry.sizeof()
        data = self._read(entries * entry_size) if entries else b""
        code, _ = self._result()
        check(code)
        if not entries:
            raise GenericError
        return [
            wire.pb_result_part_table_entry.unpack(data[i * entry_size :])
            for i in range(entries)
        ]

    def _part_size(self, part: uuid.UUID, max_entries: int) -> int:
        """Return the size of a partition in bytes."""
        for entry in self._read_part_table(max_entries):
            if bytes(entry.uuid) == part.bytes:
                return int(entry.block_size * (entry.last_block - entry.first_block + 1))
        raise NotFoundError

    def _stream_init(self, part: uuid.UUID) -> None:
        init = wire.pb_command_stream_initialize(part_uuid=_u8_array(part.bytes))
        self._command(Command.PB_CMD_STREAM_INITIALIZE, init)

    def _stream_prepare_buffer(self, buffer_id: int, data: bytes) -> None:
        prepare = wire.pb_command_stream_prepare_buffer(size=len(data), id=buffer_id)
        self._send(Command.PB_CMD_STREAM_PREPARE_BUFFER, prepare)
        # The data is sent whatever the device answers
        self._result()
        self._write(data)
        code, _ = self._result()
        check(code)

    def _stream_write_buffer(self, buffer_id: int, offset: int, size: int) -> None:
        write = wire.pb_command_stream_write_buffer(size=size, offset=offset, buffer_id=buffer_id)
        self._command(Command.PB_CMD_STREAM_WRITE_BUFFER, write)

    def _stream_read_buffer(self, buffer_id: int, offset: int, size: int) -> bytes:
        read = wire.pb_command_stream_read_buffer(size=size, offset=offset, buffer_id=buffer_id)
        self._command(Command.PB_CMD_STREAM_READ_BUFFER, read)
        data = self._read(size)
        code, _ = self._result()
        check(code)
        return data

    def _stream_finalize(self) -> None:
        """Finalize a stream after a write or read, which has the verdict."""
        with contextlib.suppress(Error):
            self._send(Command.PB_CMD_STREAM_FINALIZE)
            self._result()

    def _part_write(self, f: IO[bytes], part: uuid.UUID) -> None:
        f.seek(0)
        chunk_size, buffers = self._transfer_caps()
        try:
            part_size = self._part_size(part, PART_TABLE_ENTRIES_WRITE)
            # Reject files that don't fit before writing anything. For bpak files the
            # payload is written from the start of the partition and the header to
            # the end, so they must not overlap either.
            file_size = f.seek(0, 2)
            f.seek(0)
            if file_size > part_size:
                raise ArgumentError
            self._stream_init(part)

            buffer_id = 0
            offset = 0
            data = f.read(bpak.BPAK_HEADER_SIZE)
            if len(data) == bpak.BPAK_HEADER_SIZE and bpak.valid_header(
                bpak.bpak_header.unpack(data)
            ):
                self._stream_prepare_buffer(buffer_id, data)
                self._stream_write_buffer(buffer_id, part_size - len(data), len(data))
                buffer_id = (buffer_id + 1) % buffers
            else:
                f.seek(0)

            while data := f.read(chunk_size):
                self._stream_prepare_buffer(buffer_id, data)
                self._stream_write_buffer(buffer_id, offset, len(data))
                buffer_id = (buffer_id + 1) % buffers
                offset += len(data)
        finally:
            self._stream_finalize()

    def _part_read(self, f: IO[bytes], part: uuid.UUID) -> None:
        f.seek(0)
        chunk_size, buffers = self._transfer_caps()
        bytes_left = self._part_size(part, PART_TABLE_ENTRIES)
        self._stream_init(part)

        buffer_id = 0
        offset = 0
        try:
            while True:
                size = min(bytes_left, chunk_size)
                data = self._stream_read_buffer(buffer_id, offset, size)
                buffer_id = (buffer_id + 1) % buffers
                if f.write(data) != size:
                    raise IOError
                offset += size
                bytes_left -= size
                if bytes_left <= 0:
                    break
        finally:
            self._stream_finalize()

    def _boot_bpak(self, data: bytes, pretend_part: uuid.UUID, verbose: bool) -> None:
        chunk_size, _ = self._transfer_caps()
        if len(data) < bpak.BPAK_HEADER_SIZE:
            msg = "Invalid BPAK header"
            raise GenericError(msg)
        header = bpak.bpak_header.unpack(data)
        parts = bpak.parts(header)
        if not bpak.valid_header(header) or any(
            bpak.part_offset(header, p) + bpak.part_size(p) > len(data) for p in parts
        ):
            msg = "Invalid BPAK header"
            raise GenericError(msg)

        boot = wire.pb_command_bpak_boot(
            verbose=1 if verbose else 0, uuid=_u8_array(pretend_part.bytes)
        )
        logger.debug("Loading image..")
        self._command(Command.PB_CMD_BOOT_BPAK, boot)
        self._write(data[: bpak.BPAK_HEADER_SIZE])
        code, _ = self._result()
        check(code)

        for p in parts:
            offset = bpak.part_offset(header, p)
            end = offset + bpak.part_size(p)
            logger.debug("Loading part %x, %i bytes", p.id, end - offset)
            while offset < end:
                chunk = min(chunk_size, end - offset)
                self._write(data[offset : offset + chunk])
                offset += chunk
            # The device answers each part, and the final result has its verdict
            self._result()

        code, _ = self._result()
        check(code)

    def _board_command(self, cmd_id: int, request: bytes) -> bytes:
        if len(request) > BOARD_REQUEST_SIZE:
            raise NoMemoryError
        board = wire.pb_command_board(
            command=cmd_id & 0xFFFFFFFF,
            request_size=len(request),
            response_buffer_size=BOARD_RESPONSE_SIZE,
        )
        self._send(Command.PB_CMD_BOARD_COMMAND, board)
        # The request is sent whatever the device answers
        self._result()
        if request:
            self._write(request.ljust(BOARD_REQUEST_SIZE, b"\0"))
        code, response = self._result()
        size = wire.pb_result_board.unpack(response).size
        if size > BOARD_RESPONSE_SIZE:
            raise GenericError
        data = b""
        if size:
            data = self._read(size)
            code, _ = self._result()
        check(code)
        return data

    def _board_status(self) -> str:
        self._send(Command.PB_CMD_BOARD_STATUS_READ)
        code, response = self._result()
        size = wire.pb_result_board_status.unpack(response).size
        check(code)
        if size > BOARD_STATUS_SIZE:
            raise GenericError
        # Also a transfer when the size is 0, as the device sends one
        status = self._read(size)
        code, _ = self._result()
        check(code)
        return wire.c_string(status)

    def _bootloader_version(self) -> str:
        self._send(Command.PB_CMD_BOOTLOADER_VERSION_READ)
        code, response = self._result()
        version = response.split(b"\0", 1)[0]
        if len(version) + 1 > VERSION_SIZE:
            raise NoMemoryError
        check(code)
        return version.decode(errors="replace")

    def log_read_chunk(self, position: int, size: int) -> tuple[int, bytes, int]:
        """Read part of the device's RAM log.

        Positions count the bytes written to the log since the device started.
        The device sends what it still has from 'position' on: when older bytes
        were overwritten, it starts at its oldest byte.

        Keyword arguments:
        position -- Position to read from
        size -- Maximum number of bytes to read

        Returns the position of the first byte read, the bytes (empty at the
        end of the log) and the number of bytes written to the log so far.

        Exceptions:
        NotAuthenticatedError -- Authentication required
        NotSupportedError     -- The device has no RAM log, or is too old
        """
        log = wire.pb_command_log_read(position=position, size=min(size, 0xFFFFFFFF))
        self._send(Command.PB_CMD_LOG_READ, log)
        code, response = self._result()
        # Devices older than the command
        if code == -Result.PB_RESULT_INVALID_COMMAND:
            raise NotSupportedError
        check(code)
        result = wire.pb_result_log_read.unpack(response)
        if result.size > size:
            raise GenericError
        data = b""
        if result.size:
            data = self._read(result.size)
            code, _ = self._result()
            check(code)
        return int(result.position), data, int(result.head)

    # Public API

    def close(self) -> None:
        """Close the current session.

        This will invalidate the session object.
        """
        if self._transport is not None:
            self._transport.close()
        self._transport = None

    def authenticate(self, password: str) -> None:
        """Authenticate session using a password.

        Keyword arguments:
        password -- Password string

        Exceptions:
        AuthenticationError -- Authentication failed
        """
        self._authenticate(wire.AuthMethod.PB_AUTH_PASSWORD, 0, password.encode())

    def auth_set_password(self, password: str) -> None:
        """Set password.

        Keyword arguments:
        password -- Password string
        """
        data = password.encode()
        if len(data) > wire.PB_COMMAND_REQUEST_MAX_SIZE:
            msg = "Password too long"
            raise GenericError(msg)
        self._command(Command.PB_CMD_AUTH_SET_PASSWORD, data)

    def authenticate_dsa_token(self, token: pathlib.Path | bytes, key_id: str | int) -> None:
        """Authenticate session using a DSA token.

        Keyword arguments:
        token  -- Path to a token file or a byte array
        key_id -- Key identifier as a string or a pb_id

        Exceptions:
        AuthenticationError -- Authentication failed
        """
        pb_token = token if isinstance(token, bytes) else token.read_bytes()
        pb_key_id = key_id if isinstance(key_id, int) else pb_id(key_id)
        self._authenticate(wire.AuthMethod.PB_AUTH_ASYM_TOKEN, pb_key_id, pb_token)

    def part_get_partitions(self) -> Sequence[Partition]:
        """Get a list of 'Partition' objects.

        Exceptions:
        NotAuthenticatedError -- Authentication required
        """
        return [
            Partition(
                uuid.UUID(bytes=bytes(p.uuid)),
                wire.c_string(p.description[:36]),
                int(p.first_block),
                int(p.last_block),
                int(p.block_size),
                PartitionFlags(p.flags),
            )
            for p in self._read_part_table(PART_TABLE_ENTRIES)
        ]

    def part_verify(self, file: pathlib.Path | IO[bytes] | bytes, part: PartUUIDType) -> None:
        """Verify the contents of a partition.

        Keyword arguments:
        part -- The partition UUID either as a UUID object or a string representation
        file -- The file to verify as a pathlib Path, BufferedReader or a bytes array

        Exceptions:
        PartVerifyError       -- The file contents does not match the partition
        NotFoundError         -- Partition was not found
        NotAuthenticatedError -- Authentication required

        On success this function returns nothing.
        """
        uu: uuid.UUID = _partuuid_to_uuid(part)
        data_length: int
        chunk_len: int = 1024 * 1024
        bpak_header_len: int = 4096
        bpak_header_valid: bool = False
        hash_ctx = hashlib.sha256()

        def _chunk_reader(fh: IO[bytes]) -> int:
            length: int = 0
            nonlocal bpak_header_valid
            # Check if the first 4k contains a valid BPAK header
            if chunk := fh.read(bpak_header_len):
                hash_ctx.update(chunk)
                length += len(chunk)
                bpak_header_valid = valid_bpak_magic(chunk)
            # Read the rest
            while chunk := fh.read(chunk_len):
                hash_ctx.update(chunk)
                length += len(chunk)

            return length

        if isinstance(file, pathlib.Path):
            with file.open("rb") as f:
                data_length = _chunk_reader(f)
        elif isinstance(file, bytes):
            hash_ctx.update(file)
            bpak_header_valid = valid_bpak_magic(file)
            data_length = len(file)
        elif _is_file(file):
            file.seek(0)
            data_length = _chunk_reader(file)
        else:
            msg = "File is not a supported type"
            raise TypeError(msg)

        verify = wire.pb_command_verify_part(
            uuid=_u8_array(uu.bytes),
            sha256=_u8_array(hash_ctx.digest()),
            size=data_length & 0xFFFFFFFF,
            bpak=1 if bpak_header_valid else 0,
        )
        self._command(Command.PB_CMD_PART_VERIFY, verify)

    def part_write(self, file: pathlib.Path | IO[bytes], part: PartUUIDType) -> None:
        """Write data to a partition.

        Keyword arguments:
            file  -- Path or BufferedReader to write
            part  -- UUID of target partition
        """
        uu: uuid.UUID = _partuuid_to_uuid(part)
        if isinstance(file, pathlib.Path):
            with file.open("rb") as f:
                self._part_write(f, uu)
        elif _is_file(file):
            self._part_write(file, uu)
        else:
            msg = "File is not a supported type"
            raise TypeError(msg)

    def part_read(self, file: pathlib.Path | IO[bytes], part: PartUUIDType) -> None:
        """Read data from a partition to a file.

        Keyword arguments:
            file  -- Path or BufferedReader to write to
            part  -- UUID of partition to read from
        """
        uu: uuid.UUID = _partuuid_to_uuid(part)
        if isinstance(file, pathlib.Path):
            with file.open("wb") as f:
                self._part_read(f, uu)
        elif _is_file(file):
            self._part_read(file, uu)
        else:
            msg = "File is not a supported type"
            raise TypeError(msg)

    def part_erase(
        self,
        part_uu: PartUUIDType,
        progress_cb: Callable[[int, int], None] | None = None,
    ) -> None:
        """Erase partition.

        Keyword arguments:
        part_uu -- UUID of partition to erase

        Exceptions:
        NotFoundError         -- Partition was not found
        NotSupportedError     -- If not supported
        NotAuthenticatedError -- Authentication required
        """
        uu: uuid.UUID = _partuuid_to_uuid(part_uu)
        try:
            part: Partition = next(p for p in self.part_get_partitions() if p.uuid == uu)
        except StopIteration:
            raise NotFoundError from None

        blocks_remaining: int = part.last_block - part.first_block + 1
        block_offset: int = part.first_block
        # Note: 64 is not a magical number. On large NOR flashes the largest
        # erase block is typically 256kByte and sectors size is normally 4k.
        #
        # So we chunk up the erase in 256kByte calls because erasing NOR is slow
        # which means we can't erase everyting in one go. This will trip transport
        # timeouts and/or WDT.
        #
        # This is a bit of a hack for now and the board / block device should
        # probably provide information about maximum number of blocks that can
        # be erased per erase call.
        while count := min(64, blocks_remaining):
            if progress_cb:
                progress_cb(part.last_block - part.first_block + 1, blocks_remaining)
            erase = wire.pb_command_erase_part(
                uuid=_u8_array(uu.bytes),
                start_lba=block_offset & 0xFFFFFFFF,
                block_count=count,
            )
            self._command(Command.PB_CMD_PART_ERASE, erase)
            block_offset += count
            blocks_remaining -= count

        if progress_cb:
            progress_cb(part.last_block - part.first_block + 1, 0)

    def part_table_install(self, part: PartUUIDType, variant: int = 0) -> None:
        """Install partition table.

        Punchboot supports partition table variants to, for example, support
        different memory denisty options.

        If the board implements more than one partition variant the 'variant'
        parameter can be used to select one of the built in tables.

        Keyword arguments:
        part -- UUID of target device
        variant -- Variant (Default: 0)

        Exceptions:
        NotFoundError         -- Partition was not found
        NotAuthenticatedError -- Authentication required
        """
        uu: uuid.UUID = _partuuid_to_uuid(part)
        install = wire.pb_command_install_part_table(uu=_u8_array(uu.bytes), variant=variant)
        self._command(Command.PB_CMD_PART_TBL_INSTALL, install)

    def boot_set_boot_part(self, part: PartUUIDType | None) -> None:
        """Set active boot partition.

        Keyword arguments:
        part -- Partition UUID to activate or 'None' to disable boot

        Exceptions:
        NotSupportedError     -- Some platform's or configurations do not support this
        NotFoundError         -- Partition was not found
        NotAuthenticatedError -- Authentication required
        """
        # The null UUID disables boot
        uu = uuid.UUID(int=0)
        if part is not None:
            # If it's a string we must compare to "none" to not break compability.
            if isinstance(part, str):
                if part.casefold() != "none".casefold():
                    uu = uuid.UUID(part)
            elif isinstance(part, uuid.UUID):
                uu = part
            else:
                msg = "Invalid partition specification"
                raise ValueError(msg)
        activate = wire.pb_command_activate_part(uuid=_u8_array(uu.bytes))
        self._command(Command.PB_CMD_PART_ACTIVATE, activate)

    def boot_status(self) -> tuple[uuid.UUID, str]:
        """Boot status.

        Reads the currently active boot partition.
        And may optionally return a status message.
        """
        self._send(Command.PB_CMD_BOOT_STATUS)
        code, response = self._result()
        check(code)
        status = wire.pb_result_boot_status.unpack(response)
        return (uuid.UUID(bytes=bytes(status.uuid)), wire.c_string(status.status))

    def boot_partition(self, part: PartUUIDType, verbose: bool = False) -> None:
        """Boot partition.

        Keyword arguments:
        part -- Partition UUID to boot
        verbose -- Verbose boot output

        Exceptions:
        NotSupportedError     -- Some platform's or configurations do not support this
        NotFoundError         -- Partition was not found
        NotAuthenticatedError -- Authentication required
        """
        uu: uuid.UUID = _partuuid_to_uuid(part)
        boot = wire.pb_command_boot_part(uuid=_u8_array(uu.bytes), verbose=1 if verbose else 0)
        self._command(Command.PB_CMD_BOOT_PART, boot)
        self._invalidate()

    def boot_bpak(
        self,
        file: pathlib.Path,
        pretend_part: PartUUIDType,
        verbose: bool = False,
    ) -> None:
        """Load a bpak file into ram and run it.

        Keyword arguments:
        file -- File to load and run
        pretend_part -- Pretend like we're booting from a block device
        verbose -- Verbose boot output

        Exceptions:
        NotSupportedError     -- Some platform's or configurations do not support this
        NotFoundError         -- Partition was not found
        NotAuthenticatedError -- Authentication required
        """
        uu: uuid.UUID = _partuuid_to_uuid(pretend_part)
        self._boot_bpak(file.read_bytes(), uu, verbose)

    def device_reset(self) -> None:
        """Reset the device.

        Exceptions:
        NotAuthenticatedError -- Authentication required
        """
        self._command(Command.PB_CMD_DEVICE_RESET)
        self._invalidate()

    def device_get_punchboot_version(self) -> semver.Version:
        """Read the punchboot version."""
        ver_str = self._bootloader_version().removeprefix("v")
        return semver.Version.parse(ver_str)

    def device_get_uuid(self) -> uuid.UUID:
        """Read the device UUID."""
        return uuid.UUID(bytes=bytes(self._read_identifier().device_uuid))

    def device_get_boardname(self) -> str:
        """Read the device's board name."""
        return str(self._read_identifier().board_id.decode(errors="replace"))

    def board_run_command(self, cmd: str | int, args: bytes = b"") -> bytes:
        """Execute a board specific command.

        Keyword arguments:
        cmd  -- The command to execute either in string form or a 'pb_id' integer
        args -- Arguments as a byte array

        Returns a byte array

        Exceptions:
        NotAuthenticatedError -- Authentication required
        NotSupportedError     -- On unknown commands
        """
        cmd_id: int = pb_id(cmd) if isinstance(cmd, str) else cmd
        return self._board_command(cmd_id, args or b"")

    def board_read_status(self) -> str:
        """Read board status."""
        return self._board_status().strip()

    def log_read(self) -> bytes:
        """Read the device's RAM log, oldest byte first.

        The log is read up to where it was when the read started. Command
        mode logs too, so it would otherwise never end.

        Exceptions:
        NotAuthenticatedError -- Authentication required
        NotSupportedError     -- The device has no RAM log, or is too old
        """
        log = bytearray()
        position = 0
        end: int | None = None

        while end is None or position < end:
            start, data, head = self.log_read_chunk(position, LOG_READ_CHUNK_SIZE)
            if end is None:
                end = head
            if not data:
                break
            log += data[: max(0, end - start)]
            position = start + len(data)

        return bytes(log)

    def slc_set_configuration(self) -> None:
        """Set SLC to configuration.

        Warning: This ususally means writing fuses, this operation might
        brick your device.
        """
        self._command(Command.PB_CMD_SLC_SET_CONFIGURATION)

    def slc_set_configuration_lock(self) -> None:
        """Set SLC to configuration locked.

        Warning: This ususally means writing fuses, this operation might
        brick your device.
        """
        self._command(Command.PB_CMD_SLC_SET_CONFIGURATION_LOCK)

    def slc_set_end_of_life(self) -> None:
        """Set SLC to end of life.

        Warning: This ususally means writing fuses, this operation might
        brick your device.
        """
        self._command(Command.PB_CMD_SLC_SET_EOL)

    def slc_get_active_keys(self) -> Sequence[int]:
        """Read active keys.

        Returns a list of id's of key's that can be used to authenticate boot
        images.
        """
        _, active, _ = self._slc_read()
        return tuple(k for k in active[:MAX_KEYS] if k)

    def slc_get_revoked_keys(self) -> Sequence[int]:
        """Read revoked keys.

        Returns a list of id's of key's that have been revoked.
        """
        _, _, revoked = self._slc_read()
        return tuple(k for k in revoked[:MAX_KEYS] if k)

    def slc_get_lifecycle(self) -> SLC:
        """Read Security Life Cycle (SLC)."""
        slc, _, _ = self._slc_read()
        return SLC(slc)

    def slc_revoke_key(self, key_id: int | str) -> None:
        """Revokey key.

        Keyword arguments:
        key_id  -- Key id or pb_id string
        """
        pb_key_id = pb_id(key_id) if isinstance(key_id, str) else key_id
        revoke = wire.pb_command_revoke_key(key_id=pb_key_id & 0xFFFFFFFF)
        self._command(Command.PB_CMD_SLC_REVOKE_KEY, revoke)
