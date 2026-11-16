"""The result codes' exceptions in punchboot.errors, without a device."""

from __future__ import annotations

import punchboot
import pytest
from punchboot import errors, wire


def test_ok_raises_nothing() -> None:
    errors.check(0)


@pytest.mark.parametrize("result", [r for r in wire.Result if r != wire.Result.PB_RESULT_OK])
def test_each_result_has_its_exception(result: wire.Result) -> None:
    exc = errors.EXCEPTIONS[result]

    with pytest.raises(exc) as info:
        errors.check(-result)
    # Its own class, not one it derives from
    assert type(info.value) is exc
    assert isinstance(info.value, punchboot.Error)


def test_the_exceptions_are_distinct_and_exported() -> None:
    classes = list(errors.EXCEPTIONS.values())

    assert len(classes) == len(set(classes))
    for exc in classes:
        assert getattr(punchboot, exc.__name__) is exc
        assert exc.__name__ in punchboot.__all__


@pytest.mark.parametrize("code", [-19, -127, -128, 1, 5, 127])
def test_unknown_codes_are_generic(code: int) -> None:
    # A newer device's code, or one that isn't negative: GenericError
    with pytest.raises(punchboot.GenericError) as info:
        errors.check(code)
    assert type(info.value) is punchboot.GenericError


def test_bad_payload_is_a_generic_error() -> None:
    # An older host takes the code it doesn't know as GenericError: what
    # catches that catches the new one too
    with pytest.raises(punchboot.GenericError):
        errors.check(-wire.Result.PB_RESULT_BAD_PAYLOAD)


def test_builtin_names_are_not_shadowed_by_import() -> None:
    # punchboot.TimeoutError and IOError are pb's, the builtins stay builtins
    assert punchboot.TimeoutError is not TimeoutError
    assert punchboot.IOError is not OSError
    assert not issubclass(punchboot.TimeoutError, OSError)
