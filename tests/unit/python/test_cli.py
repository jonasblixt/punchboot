"""The punchboot CLI's arguments and error messages, without a device.

The session is a fake (FakeSession), put where the CLI opens one: what the CLI
passes to it and what it makes of the exceptions is what is tested.
tests/integration/test_cli.py runs the CLI against pb-host.
"""

from __future__ import annotations

import pathlib
import uuid
from importlib.metadata import version
from typing import Any, ClassVar

import punchboot
import punchboot.__main__ as cli_main
import pytest
from click.testing import CliRunner
from punchboot import errors

SOCKET = ("-t", "socket", "-s", "/nonexistent/pb.sock")


class FakeSession:
    """Records the calls; raises 'error' from every call when it is set."""

    calls: ClassVar[list[tuple[str, tuple[Any, ...]]]] = []
    opened: ClassVar[list[dict[str, Any]]] = []
    error: ClassVar[Exception | None] = None
    result: ClassVar[Any] = b""

    def __init__(self, **kwargs: Any) -> None:  # noqa: ANN401
        """Record how the CLI opened the session."""
        FakeSession.opened.append(kwargs)

    def __getattr__(self, name: str) -> Any:  # noqa: ANN401
        """Return a method that records its call."""

        def call(*args: Any) -> Any:  # noqa: ANN401
            FakeSession.calls.append((name, args))
            if FakeSession.error is not None:
                raise FakeSession.error
            return FakeSession.result

        return call


@pytest.fixture(autouse=True)
def fake_session(monkeypatch: pytest.MonkeyPatch) -> None:
    FakeSession.calls = []
    FakeSession.opened = []
    FakeSession.error = None
    FakeSession.result = b""
    monkeypatch.setattr(cli_main, "Session", FakeSession)
    monkeypatch.delenv("PB_DEVICE_UUID", raising=False)


def run(*args: str, stdin: bytes | None = None) -> Any:  # noqa: ANN401
    return CliRunner().invoke(cli_main.cli, list(args), input=stdin)


def test_version() -> None:
    result = run("--version")

    assert result.exit_code == 0
    assert result.output == f"Punchboot tool {version('punchboot')}\n"
    assert FakeSession.opened == []


def test_unknown_transport() -> None:
    result = run("-t", "serial", "dev", "show")

    assert result.exit_code == 2
    assert FakeSession.opened == []


def test_bad_device_uuid() -> None:
    result = run("-u", "not-a-uuid", "dev", "show")

    assert result.exit_code == 2
    assert FakeSession.opened == []


def test_socket_path() -> None:
    result = run(*SOCKET, "dev", "reset")

    assert result.exit_code == 0, result.output
    assert FakeSession.opened == [{"socket_path": pathlib.Path("/nonexistent/pb.sock")}]
    assert FakeSession.calls == [("device_reset", ())]


def test_socket_with_a_device_uuid_is_refused() -> None:
    result = run(*SOCKET, "-u", str(uuid.UUID(int=1)), "dev", "reset")

    assert result.exit_code == 2
    assert "not possible to address different devices over sockets" in result.output
    assert FakeSession.calls == []


def test_device_not_found(monkeypatch: pytest.MonkeyPatch) -> None:
    class Missing(FakeSession):
        def __init__(self, **_kwargs: Any) -> None:  # noqa: ANN401
            raise punchboot.NotFoundError

    monkeypatch.setattr(cli_main, "Session", Missing)
    result = run(*SOCKET, "dev", "reset")

    assert result.exit_code == 1
    assert "Could not connect to device" in result.output


@pytest.mark.parametrize("exc", list(errors.EXCEPTIONS.values()), ids=lambda e: e.__name__)
def test_each_error_has_its_message(exc: type[punchboot.Error]) -> None:
    FakeSession.error = exc()
    result = run(*SOCKET, "dev", "reset")

    assert result.exit_code == 1
    message = result.output.removeprefix("Error: ").strip()
    assert message
    # Not the fallback, except for the one it is the message of
    if exc is not punchboot.GenericError:
        assert message != "Generic error", exc.__name__


def test_error_message_has_the_exceptions_text() -> None:
    FakeSession.error = punchboot.TransferError("the USB stack said no")
    result = run(*SOCKET, "dev", "reset")

    assert result.exit_code == 1
    assert result.output == "Error: Transfer: the USB stack said no\n"


def test_os_and_value_errors_are_messages() -> None:
    FakeSession.error = ValueError("not a semantic version")
    result = run(*SOCKET, "dev", "reset")
    assert result.output == "Error: not a semantic version\n"

    FakeSession.error = OSError()
    result = run(*SOCKET, "dev", "reset")
    assert result.output == "Error: OSError\n"


@pytest.mark.parametrize(
    ("argument", "sent"), [("0x1234", 0x1234), ("42", 42), ("pb-development", "pb-development")]
)
def test_revoke_key_id(argument: str, sent: int | str) -> None:
    result = run(*SOCKET, "slc", "revoke-key", argument, "--force")

    assert result.exit_code == 0, result.output
    assert FakeSession.calls == [("slc_revoke_key", (sent,))]


def test_slc_lock_asks_first() -> None:
    # No: nothing is sent, and that isn't an error
    result = run(*SOCKET, "slc", "lock", stdin=b"n\n")

    assert result.exit_code == 0, result.output
    assert FakeSession.calls == []

    result = run(*SOCKET, "slc", "lock", stdin=b"y\n")
    assert result.exit_code == 0, result.output
    assert FakeSession.calls == [("slc_set_configuration_lock", ())]


@pytest.mark.parametrize(
    ("argument", "sent"), [("0xc72b6e9e", 0xC72B6E9E), ("test-command", "test-command")]
)
def test_board_command_id(argument: str, sent: int | str) -> None:
    FakeSession.result = b"answer\0"
    result = run(*SOCKET, "board", "command", argument, "args")

    assert result.exit_code == 0, result.output
    assert result.output == "answer\n"
    assert FakeSession.calls == [("board_run_command", (sent, b"args"))]


def test_board_command_args_or_file(tmp_path: pathlib.Path) -> None:
    args = tmp_path / "args"
    args.write_bytes(b"\x00\x01")

    result = run(*SOCKET, "board", "command", "x", "--args-from-file", str(args))
    assert result.exit_code == 0, result.output
    assert FakeSession.calls == [("board_run_command", ("x", b"\x00\x01"))]

    FakeSession.calls = []
    result = run(*SOCKET, "board", "command", "x", "a", "--args-from-file", str(args))
    assert result.exit_code == 2
    assert FakeSession.calls == []


def test_named_command_request(tmp_path: pathlib.Path) -> None:
    result = run(*SOCKET, "command", "test", "hello")
    assert result.exit_code == 0, result.output
    assert FakeSession.calls == [("command_run", ("test", b"hello"))]

    # From stdin when it isn't a terminal
    FakeSession.calls = []
    result = run(*SOCKET, "command", "test", stdin=b"piped")
    assert FakeSession.calls == [("command_run", ("test", b"piped"))]

    # At most 1024 bytes (command-mode.md 5.6)
    request = tmp_path / "request"
    request.write_bytes(bytes(1024))
    FakeSession.calls = []
    result = run(*SOCKET, "command", "test", "--args-from-file", str(request))
    assert result.exit_code == 0, result.output

    request.write_bytes(bytes(1025))
    FakeSession.calls = []
    result = run(*SOCKET, "command", "test", "--args-from-file", str(request))
    assert result.exit_code == 2
    assert "at most 1024" in result.output
    assert FakeSession.calls == []
