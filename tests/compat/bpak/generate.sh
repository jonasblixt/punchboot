#!/usr/bin/env bash
#
# Creates the golden bpak images (D-008, doc/dev/compatibility.md).
#
# Images made by a released bpak CLI must keep booting in every later
# punchboot, so they are made once, committed, and never regenerated.
# test_compat.py boots each of them in pb-host.
#
# Run it once per bpak CLI version, in the dev container:
#
#   scripts/pbdev run tests/compat/bpak/generate.sh
#   BPAK=/path/to/another/bpak scripts/pbdev run tests/compat/bpak/generate.sh
#
# The images go to tests/compat/bpak/v<version>/ and their checksums are
# appended to tests/compat/bpak/SHA256SUMS. The commands are for the 0.9 CLI;
# an older CLI may need other options.

set -euo pipefail

cd "$(dirname "$0")/../../.."

BPAK="${BPAK:-bpak}"
version=$("$BPAK" --version | awk '{print $2}')
out="tests/compat/bpak/v${version}"

if [ -e "$out" ]; then
    echo "$out exists. Golden images are never regenerated." >&2
    exit 1
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/v${version}"

# image <name> <hash kind> <signature kind>
image() {
    img="$tmp/v${version}/$1"
    "$BPAK" create "$img" -Y --hash-kind "$2" --signature-kind "$3"
    "$BPAK" add "$img" --meta bpak-package --from-string \
        8df597ff-2cf5-42ea-b2b6-47c348721b75 --encoder uuid
    "$BPAK" add "$img" --meta bpak-package-uid --from-string \
        "$(python3 -c 'import uuid; print(uuid.uuid4())')" --encoder uuid
}

# part <name> <size in bytes> <load address> [flag]
part() {
    head -c "$2" /dev/urandom > "$tmp/$1.bin"
    "$BPAK" add "$img" --meta pb-load-addr --from-string "$3" --part-ref "$1" \
        --encoder integer
    "$BPAK" add "$img" --part "$1" --from-file "$tmp/$1.bin" ${4:+--set-flag "$4"}
}

# sign <key id> <private key> <public key>
sign() {
    "$BPAK" set "$img" --key-id "$1" --keystore-id pb
    "$BPAK" sign "$img" --key "$2"
    "$BPAK" verify "$img" --key "$3" > /dev/null
}

# One image per hash and signature kind, with the keys in
# pki/internal_keystore.bpak
image sha256-secp256r1.bpak sha256 prime256v1
part kernel 65536 0x49000000
sign pb-development pki/secp256r1-key-pair.pem pki/secp256r1-pub-key.pem

image sha384-secp384r1.bpak sha384 secp384r1
part kernel 65536 0x49000000
sign pb-development2 pki/secp384r1-key-pair.pem pki/secp384r1-pub-key.pem

image sha512-secp521r1.bpak sha512 secp521r1
part kernel 65536 0x49000000
sign pb-development3 pki/secp521r1-key-pair.pem pki/secp521r1-pub-key.pem

# Two parts, the first one not a multiple of the 512 byte alignment
image two-parts.bpak sha256 prime256v1
part kernel 33333 0x49000000
part dt 8192 0x48000000
sign pb-development pki/secp256r1-key-pair.pem pki/secp256r1-pub-key.pem

# A part that is excluded from the payload hash (F-076)
image dont-hash.bpak sha256 prime256v1
part kernel 32768 0x49000000
part extra 4096 0x48000000 dont-hash
sign pb-development pki/secp256r1-key-pair.pem pki/secp256r1-pub-key.pem

mkdir -p "$out"
cp "$tmp/v${version}"/*.bpak "$out/"
(cd tests/compat/bpak && sha256sum "v${version}"/*.bpak >> SHA256SUMS)
echo "Created $out. Commit it together with SHA256SUMS."
