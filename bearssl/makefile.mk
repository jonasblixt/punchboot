ifdef CONFIG_BEARSSL

PLAT_C_SRCS  += bearssl/sha2small.c
PLAT_C_SRCS  += bearssl/sha2big.c
PLAT_C_SRCS  += bearssl/md5.c
PLAT_C_SRCS  += bearssl/dec32be.c
PLAT_C_SRCS  += bearssl/dec64be.c
PLAT_C_SRCS  += bearssl/enc64be.c
PLAT_C_SRCS  += bearssl/dec32le.c
PLAT_C_SRCS  += bearssl/enc32le.c
PLAT_C_SRCS  += bearssl/enc32be.c
PLAT_C_SRCS  += bearssl/i31_rshift.c
PLAT_C_SRCS  += bearssl/rsa_i62_pub.c
PLAT_C_SRCS  += bearssl/i31_decode.c
PLAT_C_SRCS  += bearssl/i31_decmod.c
PLAT_C_SRCS  += bearssl/i31_ninv31.c
PLAT_C_SRCS  += bearssl/i62_modpow2.c
PLAT_C_SRCS  += bearssl/i31_modpow.c
PLAT_C_SRCS  += bearssl/i31_encode.c
PLAT_C_SRCS  += bearssl/i31_bitlen.c
PLAT_C_SRCS  += bearssl/i31_iszero.c
PLAT_C_SRCS  += bearssl/i31_modpow2.c
PLAT_C_SRCS  += bearssl/i31_tmont.c
PLAT_C_SRCS  += bearssl/i31_muladd.c
PLAT_C_SRCS  += bearssl/i32_div32.c
PLAT_C_SRCS  += bearssl/i31_sub.c
PLAT_C_SRCS  += bearssl/i31_add.c
PLAT_C_SRCS  += bearssl/i31_montmul.c
PLAT_C_SRCS  += bearssl/i31_fmont.c
PLAT_C_SRCS  += bearssl/ccopy.c
PLAT_C_SRCS  += bearssl/ec_secp256r1.c
PLAT_C_SRCS  += bearssl/ec_secp384r1.c
PLAT_C_SRCS  += bearssl/ec_secp521r1.c
PLAT_C_SRCS  += bearssl/ecdsa_i31_vrfy_asn1.c
PLAT_C_SRCS  += bearssl/ecdsa_i31_vrfy_raw.c
PLAT_C_SRCS  += bearssl/ecdsa_i31_bits.c
PLAT_C_SRCS  += bearssl/ecdsa_atr.c
PLAT_C_SRCS  += bearssl/ec_all_m15.c

PLAT_C_SRCS  += bearssl/ec_prime_i15.c
PLAT_C_SRCS  += bearssl/ec_p256_m15.c
PLAT_C_SRCS  += bearssl/ec_c25519_m15.c
PLAT_C_SRCS  += bearssl/i15_add.c
PLAT_C_SRCS  += bearssl/i15_sub.c
PLAT_C_SRCS  += bearssl/i15_montmul.c
PLAT_C_SRCS  += bearssl/i15_modpow.c
PLAT_C_SRCS  += bearssl/i15_encode.c
PLAT_C_SRCS  += bearssl/i15_decmod.c
PLAT_C_SRCS  += bearssl/i15_iszero.c
PLAT_C_SRCS  += bearssl/i15_tmont.c
PLAT_C_SRCS  += bearssl/i15_muladd.c
PLAT_C_SRCS  += bearssl/x509_decoder.c

endif
