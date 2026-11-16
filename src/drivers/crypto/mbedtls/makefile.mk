ifeq (${CONFIG_DRIVERS_CRYPTO_MBEDTLS}, y)

ifeq (${MBEDTLS_DIR},)
  $(error Error: MBEDTLS_DIR not set)
endif


src-$(CONFIG_DRIVERS_CRYPTO_MBEDTLS) += $(addprefix ${MBEDTLS_DIR}/library/, \
                    memory_buffer_alloc.c \
                    platform.c            \
                    platform_util.c       \
                    )

src-$(CONFIG_MBEDTLS_STRERROR) += $(addprefix ${MBEDTLS_DIR}/library/, \
                    error.c \
                    )

src-$(CONFIG_MBEDTLS_ECDSA) += $(addprefix ${MBEDTLS_DIR}/library/, \
                    asn1parse.c           \
                    asn1write.c           \
                    constant_time.c       \
                    oid.c                 \
                    bignum.c              \
                    gcm.c                 \
                    md.c                  \
                    pk.c                  \
                    pk_wrap.c             \
                    pkparse.c             \
                    pkwrite.c             \
                    ecdsa.c               \
                    ecp_curves.c          \
                    ecp.c                 \
                    bignum_core.c         \
                    hash_info.c           \
                    )

src-$(CONFIG_MBEDTLS_MD_SHA256) += ${MBEDTLS_DIR}/library/sha256.c
src-$(CONFIG_MBEDTLS_MD_SHA384) += ${MBEDTLS_DIR}/library/sha512.c
src-$(CONFIG_MBEDTLS_MD_MD5) += ${MBEDTLS_DIR}/library/md5.c
src-$(CONFIG_DRIVERS_CRYPTO_MBEDTLS) += src/drivers/crypto/mbedtls/mbedtls_pb.c

# Mbed TLS 3.4.0's PSA headers declare psa_set_key_domain_parameters() twice
# (crypto_struct.h and crypto_extra.h): its own warning, not pb's
$(BUILD_DIR)/$(MBEDTLS_DIR)/library/%.o: cflags-y += -Wno-redundant-decls

# The curve and big number code at -O2 whatever CONFIG_OPTIMIZE is: on the
# RT1060 (-Os) a P-256 signature takes 22.6 ms instead of 26.6, for 4.4 KiB
# more of pb (about 0.06 ms of the ROM's load). bignum_core.c alone gives
# 0.4 ms of it (F-458)
$(addprefix $(BUILD_DIR)/$(MBEDTLS_DIR)/library/,bignum.o bignum_core.o ecp.o ecp_curves.o): \
    cflags-y += -O2

cflags-$(CONFIG_DRIVERS_CRYPTO_MBEDTLS) += -DMBEDTLS_CONFIG_FILE=\"$(shell readlink -f src/drivers/crypto/mbedtls/mbedtls_config.h)\"
cflags-$(CONFIG_DRIVERS_CRYPTO_MBEDTLS) += -I${MBEDTLS_DIR}/include

endif
