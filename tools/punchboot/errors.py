"""Punchboot exceptions, one per pb_results value (include/pb-tools/error.h)."""

from __future__ import annotations

from .wire import Result


class Error(Exception):
    """Base class of the punchboot exceptions."""


class GenericError(Error):
    """PB_RESULT_ERROR, and result codes the host doesn't know."""


class AuthenticationError(Error):
    """PB_RESULT_AUTHENTICATION_FAILED."""


class NotAuthenticatedError(Error):
    """PB_RESULT_NOT_AUTHENTICATED."""


class NotSupportedError(Error):
    """PB_RESULT_NOT_SUPPORTED."""


class ArgumentError(Error):
    """PB_RESULT_INVALID_ARGUMENT."""


class CommandError(Error):
    """PB_RESULT_INVALID_COMMAND."""


class PartVerifyError(Error):
    """PB_RESULT_PART_VERIFY_FAILED."""


class PartNotBootableError(Error):
    """PB_RESULT_PART_NOT_BOOTABLE."""


class NoMemoryError(Error):
    """PB_RESULT_NO_MEMORY."""


class TransferError(Error):
    """PB_RESULT_TRANSFER_ERROR."""


class NotFoundError(Error):
    """PB_RESULT_NOT_FOUND."""


class StreamNotInitializedError(Error):
    """PB_RESULT_STREAM_NOT_INITIALIZED."""


class TimeoutError(Error):  # noqa: A001
    """PB_RESULT_TIMEOUT."""


class KeyRevokedError(Error):
    """PB_RESULT_KEY_REVOKED."""


class SignatureError(Error):
    """PB_RESULT_SIGNATURE_ERROR."""


class MemError(Error):
    """PB_RESULT_MEM_ERROR."""


class IOError(Error):  # noqa: A001
    """PB_RESULT_IO_ERROR."""


EXCEPTIONS: dict[Result, type[Error]] = {
    Result.PB_RESULT_ERROR: GenericError,
    Result.PB_RESULT_AUTHENTICATION_FAILED: AuthenticationError,
    Result.PB_RESULT_NOT_AUTHENTICATED: NotAuthenticatedError,
    Result.PB_RESULT_NOT_SUPPORTED: NotSupportedError,
    Result.PB_RESULT_INVALID_ARGUMENT: ArgumentError,
    Result.PB_RESULT_INVALID_COMMAND: CommandError,
    Result.PB_RESULT_PART_VERIFY_FAILED: PartVerifyError,
    Result.PB_RESULT_PART_NOT_BOOTABLE: PartNotBootableError,
    Result.PB_RESULT_NO_MEMORY: NoMemoryError,
    Result.PB_RESULT_TRANSFER_ERROR: TransferError,
    Result.PB_RESULT_NOT_FOUND: NotFoundError,
    Result.PB_RESULT_STREAM_NOT_INITIALIZED: StreamNotInitializedError,
    Result.PB_RESULT_TIMEOUT: TimeoutError,
    Result.PB_RESULT_KEY_REVOKED: KeyRevokedError,
    Result.PB_RESULT_SIGNATURE_ERROR: SignatureError,
    Result.PB_RESULT_MEM_ERROR: MemError,
    Result.PB_RESULT_IO_ERROR: IOError,
}


def check(result_code: int) -> None:
    """Raise the exception for a result code (0 or -PB_RESULT_*), unless it is OK."""
    if result_code == 0:
        return
    try:
        exc = EXCEPTIONS[Result(-result_code)]
    except (ValueError, KeyError):
        exc = GenericError
    raise exc
