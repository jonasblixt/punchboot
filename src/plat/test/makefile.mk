#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


PB_ARCH_NAME = armv7a

CFLAGS += -mtune=cortex-a15

PLAT_C_SRCS  += plat/test/uart.c
PLAT_C_SRCS  += plat/test/usb_test.c
PLAT_C_SRCS  += plat/test/crypto.c
PLAT_C_SRCS  += plat/test/reset.c
PLAT_C_SRCS  += plat/test/pl061.c
PLAT_C_SRCS  += plat/test/semihosting.c
PLAT_C_SRCS  += plat/test/wdog.c
PLAT_C_SRCS  += plat/test/plat.c
PLAT_C_SRCS  += plat/test/gcov.c
PLAT_C_SRCS  += plat/test/virtio.c
PLAT_C_SRCS  += plat/test/virtio_serial.c
PLAT_C_SRCS  += plat/test/virtio_block.c

PLAT_ASM_SRCS += plat/test/semihosting_call.S

CFLAGS += -fprofile-arcs -ftest-coverage

# Lib tomcrypt
CFLAGS += -I 3pp/libtomcrypt/src/headers
CFLAGS += -DARGTYPE=3 -DLTC_NO_TEST -DLTC_NO_FILE -DLTC_SOURCE
CFLAGS += -DUSE_LTM -DLTM_DESC -I3pp/libtommath/ -L3pp/libtommath -L3pp/libtomcrypt
LDFLAGS += -L3pp/libtommath 

LIBS += -ltommath 

PLAT_C_SRCS += 3pp/libtomcrypt/src/hashes/sha2/sha256.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/math/ltm_desc.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/ecc/ltc_ecc_map.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/math/tfm_desc.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/math/multi.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/math/rand_prime.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/misc/crypt/crypt_prng_descriptor.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/misc/crypt/crypt_prng_is_valid.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/misc/crypt/crypt_ltc_mp_descriptor.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/rsa/rsa_make_key.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/rsa/rsa_free.c

PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/rsa/rsa_set.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/rsa/rsa_exptmod.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/ecc/ltc_ecc_mul2add.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/ecc/ltc_ecc_points.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/misc/zeromem.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/ecc/ltc_ecc_projective_add_point.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/ecc/ltc_ecc_projective_dbl_point.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/ecc/ltc_ecc_mulmod.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/math/fp/ltc_ecc_fp_mulmod.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/ecc/ltc_ecc_mulmod_timing.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/rsa/rsa_sign_hash.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/pkcs1/pkcs_1_v1_5_decode.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/sequence/der_decode_sequence_ex.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/boolean/der_decode_boolean.c

PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/boolean/der_encode_boolean.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/integer/der_decode_integer.c

PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/integer/der_encode_integer.c

PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/short_integer/der_encode_short_integer.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/short_integer/der_decode_short_integer.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/short_integer/der_length_short_integer.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/integer/der_length_integer.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/bit/der_decode_bit_string.c

PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/bit/der_encode_bit_string.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/octet/der_decode_octet_string.c

PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/octet/der_encode_octet_string.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/octet/der_length_octet_string.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/bit/der_length_bit_string.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/object_identifier/der_decode_object_identifier.c

PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/object_identifier/der_encode_object_identifier.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/object_identifier/der_length_object_identifier.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/ia5/der_decode_ia5_string.c

PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/ia5/der_encode_ia5_string.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/generalizedtime/der_decode_generalizedtime.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/ia5/der_length_ia5_string.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/generalizedtime/der_length_generalizedtime.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/printable_string/der_decode_printable_string.c

PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/printable_string/der_encode_printable_string.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/printable_string/der_length_printable_string.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/utf8/der_decode_utf8_string.c

PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/utf8/der_encode_utf8_string.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/utf8/der_length_utf8_string.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/utctime/der_decode_utctime.c

PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/utctime/der_encode_utctime.c

PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/utctime/der_length_utctime.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/bit/der_decode_raw_bit_string.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/sequence/der_length_sequence.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/boolean/der_length_boolean.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/teletex_string/der_length_teletex_string.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/prngs/sprng.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/prngs/rng_get_bytes.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/misc/crypt/crypt_register_prng.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/pkcs1/pkcs_1_pss_decode.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/pkcs1/pkcs_1_mgf1.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/misc/crypt/crypt_hash_is_valid.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/misc/crypt/crypt_hash_descriptor.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/rsa/rsa_verify_hash.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/teletex_string/der_decode_teletex_string.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/choice/der_decode_choice.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/pkcs1/pkcs_1_v1_5_encode.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/sequence/der_encode_sequence_ex.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/set/der_encode_set.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/set/der_encode_setof.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/bit/der_encode_raw_bit_string.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/asn1/der/generalizedtime/der_encode_generalizedtime.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/misc/mem_neq.c
PLAT_C_SRCS += 3pp/libtomcrypt/src/pk/pkcs1/pkcs_1_pss_encode.c


plat_dep:
	@make -C 3pp/libtommath CC=$(CC) AR=$(AR)

plat_clean:
	@-rm -rf plat/test/*.o plat/test/*.gcda plat/test/*.gcno
	@make -C 3pp/libtommath clean
