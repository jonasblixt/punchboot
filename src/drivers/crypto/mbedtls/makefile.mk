

LIBMBEDTLS_SRCS += $(addprefix download/mbedtls-3.4.0/library/, \
                    asn1parse.c           \
                    asn1write.c           \
                    constant_time.c       \
                    memory_buffer_alloc.c \
                    oid.c                 \
                    platform.c            \
                    platform_util.c       \
                    bignum.c              \
                    gcm.c                 \
                    md.c                  \
                    pk.c                  \
                    pk_wrap.c             \
                    pkparse.c             \
                    pkwrite.c             \
                    sha256.c              \
                    sha512.c              \
					md5.c				  \
                    ecdsa.c               \
                    ecp_curves.c          \
                    ecp.c                 \
					bignum_core.c		  \
					hash_info.c		\
                    )

src-$(CONFIG_DRIVERS_CRYPTO_MBEDTLS) += $(LIBMBEDTLS_SRCS)
src-$(CONFIG_DRIVERS_CRYPTO_MBEDTLS) += src/drivers/crypto/mbedtls/mbedtls_pb.c
cflags-$(CONFIG_DRIVERS_CRYPTO_MBEDTLS) += -DMBEDTLS_CONFIG_FILE=\"../src/drivers/crypto/mbedtls/mbedtls_config.h\"
cflags-$(CONFIG_DRIVERS_CRYPTO_MBEDTLS) += -Idownload/mbedtls-3.4.0/include
