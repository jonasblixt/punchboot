#!/usr/bin/env python3
"""Check scripts/gen-keystore.py, which writes pb's keystore (board.md 7, D-083).

The ids are bpak's of the names, or the hex ones; the keys are in slot order,
each with its slot; DER and PEM give the same key; and it refuses what can't be
in pb's keystore:

  check_gen_keystore.py <source dir>
"""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys


def say(text: str) -> None:
    """Print a line of the report."""
    sys.stdout.write(text + "\n")


def main() -> int:
    """Run the cases and return the number of problems."""
    root = pathlib.Path(sys.argv[1]).resolve()
    script = root / "scripts/gen-keystore.py"
    pki = root / "pki"
    problems = 0

    def run(*args: object) -> subprocess.CompletedProcess[str]:
        return subprocess.run(  # noqa: S603
            [sys.executable, "-I", script, *map(str, args)],
            capture_output=True,
            text=True,
            check=False,
        )

    def check(what: str, *, ok: bool, detail: str = "") -> None:
        nonlocal problems
        say(f"ok: {what}" if ok else f"FAILED: {what}{detail}")
        problems += not ok

    # The development keys, out of order: slot order, their ids as before
    p256, p384, p521 = (pki / f"secp{c}-pub-key.der" for c in ("256r1", "384r1", "521r1"))
    result = run(2, "pb-development3", p521, 0, "pb-development", p256, 1, "0x25c6dd36", p384)
    ids = re.findall(r"\.id = (0x[0-9a-f]+),", result.stdout)
    slots = re.findall(r"^    (\d+),$", result.stdout, re.MULTILINE)
    check(
        "the development keys: their ids, in slot order",
        ok=result.returncode == 0
        and ids == ["0xa90f9680", "0x25c6dd36", "0x52c1eda0", "0x8c837982"]
        and slots == ["0", "1", "2"],
        detail=f": {ids} {slots} {result.stderr}",
    )

    pem = run(5, "k", pki / "secp384r1-pub-key.pem")
    der = run(5, "k", p384)
    check("PEM and DER", ok=pem.returncode == 0 and pem.stdout == der.stdout, detail=pem.stderr)

    for what, args, error in (
        ("no key", (), "slot"),
        ("an argument short", (0, "k"), "slot"),
        ("slot 16", (16, "k", p256), "0 to 15"),
        ("a slot that isn't a number", ("x", "k", p256), "0 to 15"),
        ("an id with a space", (0, "a b", p256), "not an id"),
        ("an empty id", (0, "", p256), "not an id"),
        ("a file that isn't there", (0, "k", pki / "none.der"), "No such file"),
        ("a private key, DER", (0, "k", pki / "secp256r1-priv-key.der"), "SubjectPublicKeyInfo"),
        ("a private key, PEM", (0, "k", pki / "secp256r1-key-pair.pem"), "PUBLIC KEY"),
        ("an RSA key", (0, "k", pki / "test_rsa_public.der"), "ECDSA"),
        ("two keys in a slot", (0, "a", p256, 0, "b", p384), "VERIFY_KEY0's too"),
        ("two keys with an id", (0, "pb-development", p256, 3, "0xa90f9680", p384), "too"),
    ):
        result = run(*args)
        check(
            f"refused: {what}",
            ok=result.returncode != 0 and error in result.stderr and not result.stdout,
            detail=f": exit {result.returncode}: {result.stderr}",
        )
    return problems


if __name__ == "__main__":
    sys.exit(main())
