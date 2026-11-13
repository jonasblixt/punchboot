"""Kconfig's bounds for the times command mode computes with (command-mode.md 2.2, F-399).

pb's timeouts count microseconds in 32 bits (pb/delay.h), so a time in seconds
that Kconfig lets through must fit there. Kconfig is where a build's values are
decided, so these tests read Kconfig as the build does; no device runs.

Also the option of the core's serial download stop (board.md 5.1, F-251).
"""

from __future__ import annotations

import importlib
from typing import TYPE_CHECKING, Any

import pytest

from .pbtest import REPO_ROOT

if TYPE_CHECKING:
    from collections.abc import Callable

READY_TIMEOUT = "CM_TRANSPORT_READY_TIMEOUT"
SERIAL_DOWNLOAD_STOP = "CM_ON_SERIAL_DOWNLOAD"
# pb_timeout_init_us() refuses 0 and UINT32_MAX: a refused timeout has expired
TIMEOUT_US_MAX = 2**32 - 2
DEFCONFIGS = sorted(p.name for p in (REPO_ROOT / "configs").glob("*_defconfig"))


@pytest.fixture
def load_config(monkeypatch: pytest.MonkeyPatch) -> Callable[[str], Any]:
    """Return a function that reads Kconfig with a defconfig of configs/."""
    # Kconfig's 'source' lines are relative to the source tree
    monkeypatch.chdir(REPO_ROOT)
    monkeypatch.syspath_prepend(str(REPO_ROOT / "scripts"))
    kconfiglib = importlib.import_module("kconfiglib")

    def load(defconfig: str) -> Any:  # noqa: ANN401
        kconf = kconfiglib.Kconfig("src/Kconfig", warn=False)
        kconf.load_config(str(REPO_ROOT / "configs" / defconfig))
        return kconf

    return load


@pytest.mark.parametrize("seconds", [0, 4295, 100000, 2**31 - 1])
def test_ready_timeout_outside_its_range_is_not_used(
    load_config: Callable[[str], Any], seconds: int
) -> None:
    # 0 is a timeout that has expired when it is started: command mode would
    # leave at its first poll. Above 4294 s the microseconds wrap in 32 bits,
    # and above 2147 s they overflowed a 32-bit long.
    sym = load_config("host_defconfig").syms[READY_TIMEOUT]
    sym.set_value(str(seconds))

    assert 1 <= int(sym.str_value) * 1000000 <= TIMEOUT_US_MAX


@pytest.mark.parametrize("seconds", [1, 10, 2147, 2148, 4294])
def test_ready_timeout_inside_its_range_is_kept(
    load_config: Callable[[str], Any], seconds: int
) -> None:
    sym = load_config("host_defconfig").syms[READY_TIMEOUT]
    sym.set_value(str(seconds))

    assert int(sym.str_value) == seconds


@pytest.mark.parametrize("defconfig", DEFCONFIGS)
def test_ready_timeout_of_every_defconfig_fits(
    load_config: Callable[[str], Any], defconfig: str
) -> None:
    kconf = load_config(defconfig)

    if kconf.syms["CM"].str_value != "y":
        pytest.skip("No command mode in this build")

    assert 1 <= int(kconf.syms[READY_TIMEOUT].str_value) * 1000000 <= TIMEOUT_US_MAX


@pytest.mark.parametrize("defconfig", DEFCONFIGS)
def test_eol_is_off_unless_the_product_chose_it(
    load_config: Callable[[str], Any], defconfig: str
) -> None:
    # EOL is the product's option, off by default (system.md 4, F-263): no
    # board's build can enter EOL. The host's has it, for the tests
    eol = load_config(defconfig).syms["SLC_EOL"]

    assert eol.str_value == ("y" if defconfig == "host_defconfig" else "n")


def test_eol_is_off_by_default(load_config: Callable[[str], Any]) -> None:
    eol = load_config("host_defconfig").syms["SLC_EOL"]
    eol.unset_value()

    assert eol.str_value == "n"


@pytest.mark.parametrize("defconfig", DEFCONFIGS)
def test_serial_download_stop_is_on_where_there_is_command_mode(
    load_config: Callable[[str], Any], defconfig: str
) -> None:
    # board.md 5.1 (F-251): the core's serial download stop is on by default,
    # in every build that has a command mode to stop in, and no board in the
    # tree turns it off
    kconf = load_config(defconfig)

    assert kconf.syms[SERIAL_DOWNLOAD_STOP].str_value == kconf.syms["CM"].str_value


def test_serial_download_stop_is_an_option(load_config: Callable[[str], Any]) -> None:
    # A product that boots from storage after a serial download turns it off
    kconf = load_config("host_defconfig")
    sym = kconf.syms[SERIAL_DOWNLOAD_STOP]

    assert sym.str_value == "y"
    sym.set_value("n")
    assert sym.str_value == "n"

    # Without command mode there is nothing to turn on
    sym.set_value("y")
    kconf.syms["CM"].set_value("n")
    assert sym.str_value == "n"
