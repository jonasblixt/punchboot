#!/bin/bash

set -e
BPAK=$(which bpak)
IMG=test.bpak

$BPAK --version

$BPAK create $IMG -Y

$BPAK add $IMG --meta bpak-package \
               --from-string "3d5a6d96-675d-4207-bb54-7687fa60e54d" \
               --encoder uuid

dd if=/dev/urandom of=test.data bs=1024 count=16

$BPAK add $IMG --part test --from-file test.data
