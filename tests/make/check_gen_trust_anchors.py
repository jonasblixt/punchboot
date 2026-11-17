#!/usr/bin/env python3
"""Check scripts/gen-keystore.py, which writes pb's keys (board.md 7, D-083).

The ids are bpak's of the names, or the hex ones; the keys are in slot order,
each with its slot; DER and PEM give the same key; and it refuses what can't be
one of pb's keys:

  check_gen_keystore.py <source dir>
"""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys
import tempfile

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec


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
    slots = re.findall(r"\.slot = (\d+),", result.stdout)
    kinds = re.findall(r"\.kind = (\w+),", result.stdout)
    check(
        "the development keys: their ids and curves, in slot order",
        ok=result.returncode == 0
        and ids == ["0xa90f9680", "0x25c6dd36", "0x52c1eda0"]
        and slots == ["0", "1", "2"]
        and kinds == ["DSA_EC_SECP256r1", "DSA_EC_SECP384r1", "DSA_EC_SECP521r1"]
        and "pb_no_of_keys = 3;" in result.stdout,
        detail=f": {ids} {slots} {kinds} {result.stderr}",
    )

    pem = run(5, "k", pki / "secp384r1-pub-key.pem")
    der = run(5, "k", p384)
    check("PEM and DER", ok=pem.returncode == 0 and pem.stdout == der.stdout, detail=pem.stderr)

    # A compressed point comes out uncompressed: the same key as its own file
    p256_der = p256.read_bytes()
    point = serialization.load_der_public_key(p256_der).public_bytes(
        serialization.Encoding.X962, serialization.PublicFormat.CompressedPoint
    )
    algorithm = p256_der[2:23]  # The AlgorithmIdentifier, 0x13 bytes and its header
    body = algorithm + bytes([0x03, len(point) + 1, 0x00]) + point
    other = ec.generate_private_key(ec.SECP256K1()).public_key()
    with tempfile.TemporaryDirectory() as tmp:
        compressed = pathlib.Path(tmp) / "compressed.der"
        compressed.write_bytes(bytes([0x30, len(body)]) + body)
        k1 = pathlib.Path(tmp) / "secp256k1.der"
        k1.write_bytes(
            other.public_bytes(
                serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo
            )
        )
        result = run(0, "k", compressed)
        check(
            "a compressed point, uncompressed",
            ok=result.returncode == 0 and result.stdout == run(0, "k", p256).stdout,
            detail=result.stderr,
        )
        result = run(0, "k", k1)
        check(
            "refused: secp256k1",
            ok=result.returncode != 0 and "P-256, P-384 or P-521" in result.stderr,
            detail=result.stderr,
        )

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
