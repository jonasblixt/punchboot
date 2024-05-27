"""Punchboot session class.

This is the main interface to the punchboot library.
"""

import hashlib
import io
import pathlib
import uuid
from typing import IO, Any, Callable, Iterable, Optional, Tuple, Union

import _punchboot  # type: ignore
import semver

from .helpers import pb_id, valid_bpak_magic
from .partition import Partition, PartitionFlags
from .slc import SLC


def _has_fileno(file: IO[bytes]) -> bool:
    if not hasattr(file, "fileno"):
        return False

    try:
        file.fileno()
    except io.UnsupportedOperation:
        return False

    return True


class Session(object):
    """Punchboot session class.

    This encapsulates all of functions available over the communications interface.
    """

    _s: Any

    def __init__(
        self,
        device_uuid: Optional[Union[uuid.UUID, str, None]] = None,
        socket_path: Optional[Union[pathlib.Path, str, None]] = None,
    ):
        """Initialize the punchboot session.

        Keyword arguments:
        device_uuid -- Optional UUID to address a specific device. If this
                is not given the first device that the underlying usb library
                find's will be used.
        socket_path -- Optional path to a domain socket, mainly used for
                testing the punchboot bootloader.
        """
        _device_uuid = str(device_uuid) if isinstance(device_uuid, uuid.UUID) else device_uuid
        _socket_path = (
            str(socket_path.resolve()) if isinstance(socket_path, pathlib.Path) else socket_path
        )
        self._s = _punchboot.Session(_device_uuid, _socket_path)

    def close(self):
        """Close the current session.

        This will invalidate the session object.
        """
        self._s.close()

    def authenticate(self, password: str):
        """Authenticate session using a password.

        Keyword arguments:
        password -- Password string

        Exceptions:
        AuthenticationError -- Authentication failed
        """
        self._s.authenticate(password)

    def auth_set_password(self, password: str):
        """Set password.

        Keyword arguments:
        password -- Password string
        """
        self._s.auth_set_password(password)

    def authenticate_dsa_token(self, token: Union[pathlib.Path, bytes], key_id: Union[str, int]):
        """Authenticate session using a DSA token.

        Keyword arguments:
        token  -- Path to a token file or a byte array
        key_id -- Key identifier as a string or a pb_id

        Exceptions:
        AuthenticationError -- Authentication failed
        """
        _token = token if isinstance(token, bytes) else token.read_bytes()
        _key_id = key_id if isinstance(key_id, int) else pb_id(key_id)
        self._s.authenticate_dsa_token(_token, _key_id)

    def part_get_partitions(self) -> Iterable[Partition]:
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
                bool(p[5] & (1 << PartitionFlags.FLAG_BOOTABLE)),
                bool(p[5] & (1 << PartitionFlags.FLAG_OTP)),
                bool(p[5] & (1 << PartitionFlags.FLAG_WRITABLE)),
                bool(p[5] & (1 << PartitionFlags.FLAG_READABLE)),
                bool(p[5] & (1 << PartitionFlags.FLAG_ERASE_BEFORE_WRITE)),
            )
            for p in self._s.part_get_partitions()
        ]

    def part_verify(
        self, file: Union[pathlib.Path, IO[bytes], bytes], part: Union[uuid.UUID, str]
    ):
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
        _uu: uuid.UUID = uuid.UUID(part) if isinstance(part, str) else part
        _data_length: int
        _chunk_len: int = 1024 * 1024
        _bpak_header_len: int = 4096
        _bpak_header_valid: bool = False
        _hash_ctx = hashlib.sha256()

        def _chunk_reader(fh: IO[bytes]) -> int:
            _length: int = 0
            nonlocal _bpak_header_valid
            # Check if the first 4k contains a valid BPAK header
            if _chunk := fh.read(_bpak_header_len):
                _hash_ctx.update(_chunk)
                _length += len(_chunk)
                _bpak_header_valid = valid_bpak_magic(_chunk)
            # Read the rest
            while _chunk := fh.read(_chunk_len):
                _hash_ctx.update(_chunk)
                _length += len(_chunk)

            return _length

        if isinstance(file, pathlib.Path):
            with file.open("rb") as f:
                _data_length = _chunk_reader(f)
        elif isinstance(file, bytes):
            _hash_ctx.update(file)
            _bpak_header_valid = valid_bpak_magic(file)
            _data_length = len(file)
        elif _has_fileno(file):
            _data_length = _chunk_reader(file)
        else:
            raise ValueError("Unacceptable input")

        self._s.part_verify(_uu.bytes, _hash_ctx.digest(), _data_length, _bpak_header_valid)

    def part_write(self, file: Union[pathlib.Path, IO[bytes]], part: Union[uuid.UUID, str]):
        """Write data to a partition.

        Keyword arguments:
            file  -- Path or BufferedReader to write
            part  -- UUID of target partition
        """
        _uu: uuid.UUID = uuid.UUID(part) if isinstance(part, str) else part
        if isinstance(file, pathlib.Path):
            with file.open("rb") as f:
                self._s.part_write(f, _uu.bytes)
        elif _has_fileno(file):
            self._s.part_write(file, _uu.bytes)
        else:
            raise ValueError("Unacceptable input")

    def part_read(self, file: Union[pathlib.Path, IO[bytes]], part: Union[uuid.UUID, str]):
        """Read data from a partition to a file.

        Keyword arguments:
            file  -- Path or BufferedReader to write to
            part  -- UUID of partition to read from
        """
        _uu: uuid.UUID = uuid.UUID(part) if isinstance(part, str) else part
        if isinstance(file, pathlib.Path):
            with file.open("wb") as f:
                self._s.part_read(f, _uu.bytes)
        elif _has_fileno(file):
            self._s.part_read(file, _uu.bytes)
        else:
            raise ValueError("Unacceptable input")

    def part_erase(
            self, part_uu: Union[uuid.UUID, str],
            progress_cb: Optional[Callable[[int, int], None]] = None
    ):
        """Erase partition.

        Keyword arguments:
        part_uu -- UUID of partition to erase

        Exceptions:
        NotFoundError         -- Partition was not found
        NotSupportedError     -- If not supported
        NotAuthenticatedError -- Authentication required
        """
        _uu: uuid.UUID = uuid.UUID(part_uu) if isinstance(part_uu, str) else part_uu
        try:
            part: Partition = [p for p in self.part_get_partitions() if p.uuid == _uu][0]
        except IndexError:
            raise _punchboot.NotFoundError()

        blocks_remaining: int = part.last_block - part.first_block + 1
        block_offset: int = part.first_block
        # TODO: 64 is not a magical number. On large NOR flashes the largest
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
            self._s.part_erase(_uu.bytes, block_offset, count)
            block_offset += count
            blocks_remaining -= count

        if progress_cb:
            progress_cb(part.last_block - part.first_block + 1, 0)

        # self._s.part_erase(_uu.bytes)
    def part_table_install(self, part: Union[uuid.UUID, str], variant: Optional[int] = 0):
        """Install partition table.

        Punchboot supports partition table variants to, for example, support
        different memory denisty options.

        If the board implements more than one partition variant the 'variant'
        parameter can be used to select one of the built in tables.

        Keyword arguments:
        part -- UUID of target device
        variant -- Optional variant (Default: 0)

        Exceptions:
        NotFoundError         -- Partition was not found
        NotAuthenticatedError -- Authentication required
        """
        _uu: uuid.UUID = uuid.UUID(part) if isinstance(part, str) else part
        self._s.part_table_install(_uu.bytes, variant)

    def boot_set_boot_part(self, part: Union[uuid.UUID, str, None]):
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
                    self._s.boot_set_boot_part(None)
                else:
                    self._s.boot_set_boot_part(uuid.UUID(part).bytes)
            elif isinstance(part, uuid.UUID):
                self._s.boot_set_boot_part(part.bytes)
            else:
                raise ValueError("Unacceptable input")
        else:
            self._s.boot_set_boot_part(None)

    def boot_status(self) -> Tuple[uuid.UUID, str]:
        """Boot status.

        Reads the currently active boot partition.
        And may optionally return a status message.
        """
        uu_bytes, status_msg = self._s.boot_status()
        return (uuid.UUID(bytes=uu_bytes), status_msg)

    def boot_partition(self, part: Union[uuid.UUID, str], verbose: Optional[bool] = False):
        """Boot partition.

        Keyword arguments:
        part -- Partition UUID to boot
        verbose -- Verbose boot output

        Exceptions:
        NotSupportedError     -- Some platform's or configurations do not support this
        NotFoundError         -- Partition was not found
        NotAuthenticatedError -- Authentication required
        """
        _uu: uuid.UUID = uuid.UUID(part) if isinstance(part, str) else part
        self._s.boot_partition(_uu.bytes, verbose)

    def boot_bpak(
        self,
        file: pathlib.Path,
        pretend_part: Union[uuid.UUID, str],
        verbose: Optional[bool] = False,
    ):
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
        _uu: uuid.UUID = uuid.UUID(pretend_part) if isinstance(pretend_part, str) else pretend_part
        self._s.boot_bpak(file.read_bytes(), _uu.bytes, verbose)

    def device_reset(self):
        """Reset the device.

        Exceptions:
        NotAuthenticatedError -- Authentication required
        """
        self._s.device_reset()

    def device_get_punchboot_version(self) -> semver.Version:
        """Read the punchboot version."""
        _ver_str: str = self._s.device_get_punchboot_version()
        if _ver_str.startswith("v"):
            _ver_str = _ver_str[1:]
        return semver.Version.parse(_ver_str)

    def device_get_uuid(self) -> uuid.UUID:
        """Read the device UUID."""
        return uuid.UUID(bytes=self._s.device_get_uuid())

    def device_get_boardname(self) -> str:
        """Read the device's board name."""
        return str(self._s.device_get_boardname())

    def board_run_command(self, cmd: Union[str, int], args: Optional[bytes] = b"") -> bytes:
        """Execute a board specific command.

        Keyword arguments:
        cmd  -- The command to execute either in string form or a 'pb_id' integer
        args -- Optional arguments as a byte array

        Returns a byte array

        Exceptions:
        NotAuthenticatedError -- Authentication required
        NotSupportedError     -- On unknown commands
        """
        _cmd_id: int = pb_id(cmd) if isinstance(cmd, str) else cmd
        return bytes(self._s.board_run_command(_cmd_id, args))

    def board_read_status(self) -> str:
        """Read board status."""
        result: str = self._s.board_read_status().strip()
        return result

    def slc_set_configuration(self):
        """Set SLC to configuration.

        Warning: This ususally means writing fuses, this operation might
        brick your device.
        """
        self._s.slc_set_configuration()

    def slc_set_configuration_lock(self):
        """Set SLC to configuration locked.

        Warning: This ususally means writing fuses, this operation might
        brick your device.
        """
        self._s.slc_set_configuration_lock()

    def slc_set_end_of_life(self):
        """Set SLC to end of life.

        Warning: This ususally means writing fuses, this operation might
        brick your device.
        """
        self._s.slc_set_end_of_life()

    def slc_get_active_keys(self) -> Iterable[int]:
        """Read active keys.

        Returns a list of id's of key's that can be used to authenticate boot
        images.
        """
        return tuple(self._s.slc_get_active_keys())

    def slc_get_revoked_keys(self) -> Iterable[int]:
        """Read revoked keys.

        Returns a list of id's of key's that have been revoked.
        """
        return tuple(self._s.slc_get_revoked_keys())

    def slc_get_lifecycle(self) -> SLC:
        """Read Security Life Cycle (SLC)."""
        return SLC(self._s.slc_get_lifecycle())

    def slc_revoke_key(self, key_id: Union[int, str]):
        """Revokey key.

        Keyword arguments:
        key_id  -- Key id or pb_id string
        """
        _key_id = pb_id(key_id) if isinstance(key_id, str) else key_id
        self._s.slc_revoke_key(_key_id)
