"""Punchboot session class.

This is the main interface to the punchboot library.
"""

from __future__ import annotations

import hashlib
import io
import pathlib
import uuid
from typing import IO, TYPE_CHECKING, Callable, Optional, Tuple, Union

import _punchboot  # type: ignore[import-not-found]
import semver  # type: ignore[import-not-found]

from .helpers import pb_id, valid_bpak_magic
from .partition import Partition, PartitionFlags
from .slc import SLC

if TYPE_CHECKING:
    from collections.abc import Sequence


def _has_fileno(file: IO[bytes]) -> bool:
    if not hasattr(file, "fileno"):
        return False

    try:
        file.fileno()
    except io.UnsupportedOperation:
        return False

    return True


PartUUIDType = Union[uuid.UUID, str]


def _partuuid_to_uuid(uu: PartUUIDType) -> uuid.UUID:
    if isinstance(uu, str):
        return uuid.UUID(uu)

    return uu


class Session:
    """Punchboot session class.

    This encapsulates all of functions available over the communications interface.
    """

    pb_s: _punchboot.Session

    def __init__(
        self,
        device_uuid: Optional[Union[uuid.UUID, str]] = None,
        socket_path: Optional[Union[pathlib.Path, str]] = None,
    ) -> None:
        """Initialize the punchboot session.

        Keyword arguments:
        device_uuid -- Optional UUID to address a specific device. If this
                is not given the first device that the underlying usb library
                find's will be used.
        socket_path -- Optional path to a domain socket, mainly used for
                testing the punchboot bootloader.
        """
        pb_device_uuid = str(device_uuid) if isinstance(device_uuid, uuid.UUID) else device_uuid
        pb_socket_path = (
            socket_path.resolve().as_posix()
            if isinstance(socket_path, pathlib.Path)
            else socket_path
        )
        self.pb_s = _punchboot.Session(pb_device_uuid, pb_socket_path)

    def close(self) -> None:
        """Close the current session.

        This will invalidate the session object.
        """
        self.pb_s.close()

    def authenticate(self, password: str) -> None:
        """Authenticate session using a password.

        Keyword arguments:
        password -- Password string

        Exceptions:
        AuthenticationError -- Authentication failed
        """
        self.pb_s.authenticate(password)

    def auth_set_password(self, password: str) -> None:
        """Set password.

        Keyword arguments:
        password -- Password string
        """
        self.pb_s.auth_set_password(password)

    def authenticate_dsa_token(
        self, token: Union[pathlib.Path, bytes], key_id: Union[str, int]
    ) -> None:
        """Authenticate session using a DSA token.

        Keyword arguments:
        token  -- Path to a token file or a byte array
        key_id -- Key identifier as a string or a pb_id

        Exceptions:
        AuthenticationError -- Authentication failed
        """
        pb_token = token if isinstance(token, bytes) else token.read_bytes()
        pb_key_id = key_id if isinstance(key_id, int) else pb_id(key_id)
        self.pb_s.authenticate_dsa_token(pb_token, pb_key_id)

    def part_get_partitions(self) -> Sequence[Partition]:
        """Get a list of 'Partition' objects.

        Exceptions:
        NotAuthenticatedError -- Authentication required
        """
        return [
            Partition(
                uuid.UUID(bytes=p[0]),
                p[1],
                p[2],
                p[3],
                p[4],
                PartitionFlags(p[5]),
            )
            for p in self.pb_s.part_get_partitions()
        ]

    def part_verify(self, file: Union[pathlib.Path, IO[bytes], bytes], part: PartUUIDType) -> None:
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
        elif _has_fileno(file):
            data_length = _chunk_reader(file)
        else:
            msg = "File is not a supported type"
            raise TypeError(msg)

        self.pb_s.part_verify(uu.bytes, hash_ctx.digest(), data_length, bpak_header_valid)

    def part_write(self, file: Union[pathlib.Path, IO[bytes]], part: PartUUIDType) -> None:
        """Write data to a partition.

        Keyword arguments:
            file  -- Path or BufferedReader to write
            part  -- UUID of target partition
        """
        uu: uuid.UUID = _partuuid_to_uuid(part)
        if isinstance(file, pathlib.Path):
            with file.open("rb") as f:
                self.pb_s.part_write(f, uu.bytes)
        elif _has_fileno(file):
            self.pb_s.part_write(file, uu.bytes)
        else:
            msg = "File is not a supported type"
            raise TypeError(msg)

    def part_read(self, file: Union[pathlib.Path, IO[bytes]], part: PartUUIDType) -> None:
        """Read data from a partition to a file.

        Keyword arguments:
            file  -- Path or BufferedReader to write to
            part  -- UUID of partition to read from
        """
        uu: uuid.UUID = _partuuid_to_uuid(part)
        if isinstance(file, pathlib.Path):
            with file.open("wb") as f:
                self.pb_s.part_read(f, uu.bytes)
        elif _has_fileno(file):
            self.pb_s.part_read(file, uu.bytes)
        else:
            msg = "File is not a supported type"
            raise TypeError(msg)

    def part_erase(
        self,
        part_uu: PartUUIDType,
        progress_cb: Optional[Callable[[int, int], None]] = None,
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
            raise _punchboot.NotFoundError from None

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
            self.pb_s.part_erase(uu.bytes, block_offset, count)
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
        self.pb_s.part_table_install(uu.bytes, variant)

    def boot_set_boot_part(self, part: Optional[PartUUIDType]) -> None:
        """Set active boot partition.

        Keyword arguments:
        part -- Partition UUID to activate or 'None' to disable boot

        Exceptions:
        NotSupportedError     -- Some platform's or configurations do not support this
        NotFoundError         -- Partition was not found
        NotAuthenticatedError -- Authentication required
        """
        if part is not None:
            # If it's a string we must compare to "none" to not break compability.
            if isinstance(part, str):
                if part.casefold() == "none".casefold():
                    self.pb_s.boot_set_boot_part(None)
                else:
                    self.pb_s.boot_set_boot_part(uuid.UUID(part).bytes)
            elif isinstance(part, uuid.UUID):
                self.pb_s.boot_set_boot_part(part.bytes)
            else:
                msg = "Invalid partition specification"
                raise ValueError(msg)
        else:
            self.pb_s.boot_set_boot_part(None)

    def boot_status(self) -> Tuple[uuid.UUID, str]:
        """Boot status.

        Reads the currently active boot partition.
        And may optionally return a status message.
        """
        uu_bytes, status_msg = self.pb_s.boot_status()
        return (uuid.UUID(bytes=uu_bytes), status_msg)

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
        self.pb_s.boot_partition(uu.bytes, verbose)

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
        self.pb_s.boot_bpak(file.read_bytes(), uu.bytes, verbose)

    def device_reset(self) -> None:
        """Reset the device.

        Exceptions:
        NotAuthenticatedError -- Authentication required
        """
        self.pb_s.device_reset()

    def device_get_punchboot_version(self) -> semver.Version:
        """Read the punchboot version."""
        ver_str: str = self.pb_s.device_get_punchboot_version()
        if ver_str.startswith("v"):
            ver_str = ver_str[1:]
        return semver.Version.parse(ver_str)

    def device_get_uuid(self) -> uuid.UUID:
        """Read the device UUID."""
        return uuid.UUID(bytes=self.pb_s.device_get_uuid())

    def device_get_boardname(self) -> str:
        """Read the device's board name."""
        return str(self.pb_s.device_get_boardname())

    def board_run_command(self, cmd: Union[str, int], args: bytes = b"") -> bytes:
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
        return bytes(self.pb_s.board_run_command(cmd_id, args))

    def board_read_status(self) -> str:
        """Read board status."""
        return str(self.pb_s.board_read_status().strip())

    def slc_set_configuration(self) -> None:
        """Set SLC to configuration.

        Warning: This ususally means writing fuses, this operation might
        brick your device.
        """
        self.pb_s.slc_set_configuration()

    def slc_set_configuration_lock(self) -> None:
        """Set SLC to configuration locked.

        Warning: This ususally means writing fuses, this operation might
        brick your device.
        """
        self.pb_s.slc_set_configuration_lock()

    def slc_set_end_of_life(self) -> None:
        """Set SLC to end of life.

        Warning: This ususally means writing fuses, this operation might
        brick your device.
        """
        self.pb_s.slc_set_end_of_life()

    def slc_get_active_keys(self) -> Sequence[int]:
        """Read active keys.

        Returns a list of id's of key's that can be used to authenticate boot
        images.
        """
        return tuple(self.pb_s.slc_get_active_keys())

    def slc_get_revoked_keys(self) -> Sequence[int]:
        """Read revoked keys.

        Returns a list of id's of key's that have been revoked.
        """
        return tuple(self.pb_s.slc_get_revoked_keys())

    def slc_get_lifecycle(self) -> SLC:
        """Read Security Life Cycle (SLC)."""
        return SLC(self.pb_s.slc_get_lifecycle())

    def slc_revoke_key(self, key_id: Union[int, str]) -> None:
        """Revokey key.

        Keyword arguments:
        key_id  -- Key id or pb_id string
        """
        pb_key_id = pb_id(key_id) if isinstance(key_id, str) else key_id
        self.pb_s.slc_revoke_key(pb_key_id)
