#!/bin/bash

BPAK=bpak
IMG=test_keystore.bpak
PKG_UUID=5df103ef-e774-450b-95c5-1fef51ceec28
PRI_KEY=pki/secp256r1-key-pair.pem
PUB_KEY=pki/secp256r1-pub-key.der

set -e

$BPAK create $IMG -Y

$BPAK add $IMG --meta bpak-package --from-string $PKG_UUID --encoder uuid


$BPAK add $IMG --part pb-development \
               --from-file pki/secp256r1-pub-key.der \
               --encoder key

$BPAK add $IMG --part pb-development2 \
               --from-file pki/secp384r1-pub-key.der \
               --encoder key

$BPAK add $IMG --part pb-development3 \
               --from-file pki/secp521r1-pub-key.der \
               --encoder key

$BPAK add $IMG --part pb-development4 \
               --from-file pki/dev_rsa_public.der \
               --encoder key


$BPAK add $IMG --meta bpak-key-id --from-string bpak-test-key --encoder id
$BPAK add $IMG --meta bpak-key-store --from-string bpak-internal --encoder id

$BPAK generate keystore $IMG --name internal


$BPAK show $IMG --hash | openssl pkeyutl -sign -inkey $PRI_KEY \
                    -keyform PEM > /tmp/sig.data

$BPAK sign $IMG --signature /tmp/sig.data --key-id bpak-test-key \
                --key-store bpak-internal

$BPAK show $IMG
$BPAK verify $IMG --key $PUB_KEY
