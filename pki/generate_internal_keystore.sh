#!/bin/bash
set -e
BPAK=bpak
IMG=internal_keystore.bpak
PKG_UUID=5df103ef-e774-450b-95c5-1fef51ceec28

$BPAK create $IMG -Y --hash-kind sha256 --signature-kind prime256v1 $V

$BPAK add $IMG --meta bpak-package --from-string $PKG_UUID --encoder uuid $V

$BPAK add $IMG --part pb-development \
               --from-file secp256r1-pub-key.der $V

$BPAK add $IMG --part pb-development2 \
               --from-file secp384r1-pub-key.der $V

$BPAK add $IMG --part pb-development3 \
               --from-file secp521r1-pub-key.der $V

$BPAK add $IMG --part pb-development4 \
               --from-file dev_rsa_public.der $V
