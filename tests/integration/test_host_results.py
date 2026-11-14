"""The host and the result codes (compatibility.md, command-mode.md 4.5).

pb_results only grows: a device newer than the host answers codes the host
doesn't have. The host takes one it doesn't know as an error, the generic
one, as the released hosts do (the 2.x C library's pb_exception_from_rc()
raises GenericError for a code of PB_RESULT_END and above). A scripted
device answers, see scripted.py.
"""

from __future__ import annotations

import punchboot
import pytest
from punchboot import errors, wire
from punchboot.wire import Command

from .pbtest import NULL_UUID, SYSTEM_A_UUID
from .scripted import ScriptedDevice, result, run_cli

# The first value no release of pb defines, and the last an int8_t carries
UNKNOWN_CODES = [len(wire.Result), 100, 127]


@pytest.mark.parametrize("code", UNKNOWN_CODES)
def test_a_code_the_host_does_not_know_is_an_error(code: int) -> None:
    assert code not in set(wire.Result)
    dev = ScriptedDevice()
    dev.on(Command.PB_CMD_BOOT_PART, result(-code))
    s = punchboot.Session(transport=dev)

    with pytest.raises(punchboot.GenericError):
        s.boot_partition(SYSTEM_A_UUID)
    # The session goes on: the frame was a result like any other
    dev.on(Command.PB_CMD_BOOT_PART, result())
    s.boot_partition(SYSTEM_A_UUID)


@pytest.mark.parametrize("code", UNKNOWN_CODES)
def test_the_cli_reports_a_code_it_does_not_know(
    monkeypatch: pytest.MonkeyPatch, code: int
) -> None:
    dev = ScriptedDevice()
    dev.on(Command.PB_CMD_BOOT_PART, result(-code))

    run = run_cli(monkeypatch, dev, "boot", "partition", str(SYSTEM_A_UUID))

    assert run.exit_code != 0
    assert "Generic error" in run.output


def test_every_code_has_an_exception_of_its_own() -> None:
    codes = set(wire.Result) - {wire.Result.PB_RESULT_OK}

    assert set(errors.EXCEPTIONS) == codes
    assert len(set(errors.EXCEPTIONS.values())) == len(codes)
    for exc in errors.EXCEPTIONS.values():
        assert issubclass(exc, punchboot.Error)
        assert exc.__name__ in punchboot.__all__


def test_a_payload_that_differs_from_its_hash(monkeypatch: pytest.MonkeyPatch) -> None:
    # PB_RESULT_BAD_PAYLOAD, from pb 3.0.0 (F-087). A release's device
    # answers PB_RESULT_ERROR for it, so what catches GenericError catches
    # this one too
    assert wire.Result.PB_RESULT_BAD_PAYLOAD == 18
    assert issubclass(punchboot.BadPayloadError, punchboot.GenericError)

    dev = ScriptedDevice()
    dev.on(Command.PB_CMD_BOOT_PART, result(-wire.Result.PB_RESULT_BAD_PAYLOAD))
    s = punchboot.Session(transport=dev)
    with pytest.raises(punchboot.BadPayloadError):
        s.boot_partition(NULL_UUID)

    dev = ScriptedDevice()
    dev.on(Command.PB_CMD_BOOT_PART, result(-wire.Result.PB_RESULT_BAD_PAYLOAD))
    run = run_cli(monkeypatch, dev, "boot", "partition", str(SYSTEM_A_UUID))
    assert run.exit_code != 0
    assert "payload" in run.output
    assert "Generic error" not in run.output


def test_an_old_device_answers_error_for_it() -> None:
    dev = ScriptedDevice()
    dev.on(Command.PB_CMD_BOOT_PART, result(-wire.Result.PB_RESULT_ERROR))
    s = punchboot.Session(transport=dev)

    with pytest.raises(punchboot.GenericError) as raised:
        s.boot_partition(SYSTEM_A_UUID)
    assert raised.type is punchboot.GenericError
