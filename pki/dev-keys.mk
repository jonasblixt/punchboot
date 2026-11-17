#
# Punch BOOT
#
# Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
#
# SPDX-License-Identifier: BSD-3-Clause
#
# pb's development keys (board.md 7), for a board whose makefile.mk names no
# key. Their private keys are in this directory: never a product's keys.
# CMakeLists.txt gives the host build the same three.

VERIFY_KEY0_PATH = pki/secp256r1-pub-key.der
VERIFY_KEY0_ID   = pb-development
VERIFY_KEY1_PATH = pki/secp384r1-pub-key.der
VERIFY_KEY1_ID   = pb-development2
VERIFY_KEY2_PATH = pki/secp521r1-pub-key.der
VERIFY_KEY2_ID   = pb-development3
