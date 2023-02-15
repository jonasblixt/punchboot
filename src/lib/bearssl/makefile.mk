ifdef CONFIG_BEARSSL

src-y  += src/lib/bearssl/sha2small.c
src-y  += src/lib/bearssl/sha2big.c
src-y  += src/lib/bearssl/md5.c
src-y  += src/lib/bearssl/dec32be.c
src-y  += src/lib/bearssl/dec64be.c
src-y  += src/lib/bearssl/enc64be.c
src-y  += src/lib/bearssl/dec32le.c
src-y  += src/lib/bearssl/enc32le.c
src-y  += src/lib/bearssl/enc32be.c
src-y  += src/lib/bearssl/i31_rshift.c
src-y  += src/lib/bearssl/rsa_i62_pub.c
src-y  += src/lib/bearssl/i31_decode.c
src-y  += src/lib/bearssl/i31_decmod.c
src-y  += src/lib/bearssl/i31_ninv31.c
src-y  += src/lib/bearssl/i62_modpow2.c
src-y  += src/lib/bearssl/i31_modpow.c
src-y  += src/lib/bearssl/i31_encode.c
src-y  += src/lib/bearssl/i31_bitlen.c
src-y  += src/lib/bearssl/i31_iszero.c
src-y  += src/lib/bearssl/i31_modpow2.c
src-y  += src/lib/bearssl/i31_tmont.c
src-y  += src/lib/bearssl/i31_muladd.c
src-y  += src/lib/bearssl/i32_div32.c
src-y  += src/lib/bearssl/i31_sub.c
src-y  += src/lib/bearssl/i31_add.c
src-y  += src/lib/bearssl/i31_montmul.c
src-y  += src/lib/bearssl/i31_fmont.c
src-y  += src/lib/bearssl/ccopy.c
src-y  += src/lib/bearssl/ec_secp256r1.c
src-y  += src/lib/bearssl/ec_secp384r1.c
src-y  += src/lib/bearssl/ec_secp521r1.c
src-y  += src/lib/bearssl/ecdsa_i31_vrfy_asn1.c
src-y  += src/lib/bearssl/ecdsa_i31_vrfy_raw.c
src-y  += src/lib/bearssl/ecdsa_i31_bits.c
src-y  += src/lib/bearssl/ecdsa_atr.c

src-y  += src/lib/bearssl/ec_prime_i31.c


endif
