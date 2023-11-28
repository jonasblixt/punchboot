#include <mbedtls/check_config.h>
#include <stdlib.h>

#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS

#define MBEDTLS_PKCS1_V21
#define MBEDTLS_PLATFORM_STD_SNPRINTF snprintf

#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C

#define MBEDTLS_BASE64_C
#define MBEDTLS_BIGNUM_C

#define MBEDTLS_ERROR_C
#define MBEDTLS_MD_C

#define MBEDTLS_MEMORY_BUFFER_ALLOC_C
#define MBEDTLS_OID_C

#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PK_WRITE_C

#define MBEDTLS_PLATFORM_C

#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED

#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_MD5_C

#define MBEDTLS_VERSION_C

/* MPI / BIGNUM options */
#define MBEDTLS_MPI_WINDOW_SIZE       2
#define MBEDTLS_MPI_MAX_SIZE          256

/* Memory buffer allocator options */
#define MBEDTLS_MEMORY_ALIGN_MULTIPLE 8

/* Optimizations */
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_ECP_NIST_OPTIM

/*
 * Prevent the use of 128-bit division which
 * creates dependency on external libraries.
 */
#define MBEDTLS_NO_UDBL_DIVISION

/*
 * Warn if errors from certain functions are ignored.
 *
 * The warnings are always enabled (where supported) for critical functions
 * where ignoring the return value is almost always a bug. This macro extends
 * the warnings to more functions.
 */
#define MBEDTLS_CHECK_RETURN_WARNING
