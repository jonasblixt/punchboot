/*===========================================================================*/
/**
    @file    srktool.c

    @brief   Processes input command line arguments, extracts public key
             information from input SRK certificates and generates SRK table
             for HAB4 and AHAB.

@verbatim
=============================================================================

              Freescale Semiconductor
    (c) Freescale Semiconductor, Inc. 2011, 2012, 2013 All rights reserved.
    Copyright 2018 NXP

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================
@endverbatim */

/*===========================================================================
                                INCLUDE FILES
=============================================================================*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h> /* strncasecmp */
#include <getopt.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#if defined _WIN32 || defined __CYGWIN__
#include <openssl/applink.c>
#endif
#include "err.h"
#include "srk_helper.h"

/*===========================================================================
                               LOCAL CONSTANTS
=============================================================================*/

const char *g_tool_name = "SRKTOOL";   /**< Global holds tool name */

/* Maximums supported by srktool */
#define MAX_STRING_BYTES          256  /**< Max. length of strings supported */

/* General definitions */
#define BYTES_IN_WORD             4    /**< Number of bytes in a word */

/* HAB4 version for use in data structures */
#define HAB4_DATA_STRUCT_VER      0x40

/** Command line arguments definitions */
#define CMDLINE_ARG_CERT_SEPARATOR (",")           /**< Argument separator */
#define CMDLINE_ARG_HASH_INDICATOR ('%')           /**< Generate cert hash */

/** Valid short command line option letters. */
const char* const short_options = "lvh:at:e:f:d:s:c:";

/** Valid long command line options. */
const struct option long_options[] =
{
    {"license", no_argument, 0, 'l'},
    {"version", no_argument,  0, 'v'},
    {"hab_ver", required_argument, 0, 'h'},
    {"ahab_ver", no_argument, 0, 'a'},
    {"table", required_argument,  0, 't'},
    {"efuses",  required_argument, 0, 'e'},
    {"fuse_format", required_argument, 0, 'f'},
    {"digest",  required_argument, 0, 'd'},
    {"sign_digest",  required_argument, 0, 's'},
    {"certs", required_argument, 0, 'c'},
    {NULL, 0, NULL, 0}
};

/** Default AHAB message digest algorithm  */
const char* ahab_md_alg_str = HASH_ALG_SHA512;

/*===========================================================================
                  LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/
/** Fuse format
 *
 * Defines two possible formats for output fuses binary file
 */
typedef enum fuse_format
{
    EIGHT_FUSES_PER_WORD = 0,
    THIRTY_TWO_FUSES_PER_WORD,
    INVALID_FUSE_FORMAT
} fuse_format_t;

/** HAB version */
typedef enum hab_version
{
    HAB_4 = 4
} hab_version_t;

/* Certificate files */
typedef struct cert_file
{
    bool hash;        /**< HAB4 only: compute hash of certificate */
    char *filename;   /**< Character string holding the cert filename */
} cert_file_t;

/*===========================================================================
                          LOCAL FUNCTION PROTOTYPES
=============================================================================*/

/* ------------------- HAB4/AHAB SRK Table Functions  ------------------- */

/** Generates SRK table data
 *
 * Generates both the SRK table data and SRK table fuse data which are
 * written to files.
 *
 * @param[in] target Define which component is targeted, HAB4 or AHAB
 *
 * @param[in] table_filename Charater string holding the SRK table filename
 *
 * @param[in] fuses_filename Charater array holding the SRK efuse filename
 *
 * @param[in] md_alg_str Charater array holding the message digest algorithm
 *
 * @param[in] certs Array of structs holding the cert filename and hash flag
 *
 * @param[in] num_certs Number of entries in @a certs
 *
 * @param[in] fuse_file_format Flag indicate which format to use when writing
 *            SRK table to efuse file
 *
 * @param[in] sd_alg_str Define which signature hash algorithm will be used
 *                       in conjunction with the given key
 *
 * @pre  @a table_filename, @a fuses_filename, @a md_alg_str and @a certs
 *       are not NULL
 *
 * @post Output files for SRK table and SRK efuse values are generated
 *
 * @returns None
 */
static void
generate_xhab_srk_table(tgt_t target,
                        const char *table_filename,
                        const char *fuses_filename,
                        const char *md_alg_str,
                        const cert_file_t *certs,
                        uint32_t num_certs,
                        fuse_format_t fuse_file_format,
                        const char *sd_alg_str);

/** Checks the command line arguments when --hab_ver is 4 or --ahab_ver is set
 *
 * Checks that:
 *           - the table filename is defined
 *           - the efuse filename is defined
 *           - there is at least one input certificate provided
 *           - the message digest algorithm is defined
 *
 * @param[in] table_filename Output filename for the SRK table
 *
 * @param[in] fuse_filename Output filename for the SRK table message digest
 *
 * @param[in] certs Array of structs holding the cert filename and hash flag
 *
 * @param[in] md_alg_str String containing the message digest algorithm type
 *
 * @pre  @a table_filename, @a fuses_filename, @md_alg_str are not NULL
 *
 * @post if any one of the checks fails the program usage is displayed on
 *        STDOUT and the program exits.
 *
 * @returns None
 */
static void
check_xhab_arguments(const char *table_filename,
                     const char *fuses_filename,
                     const char *md_alg_str,
                     uint32_t num_certs);

/** Write SRK table hash value to an output file with the given format
 *
 * @param[in] fuses_filename Output filename for the SRK table message digest
 *
 * @param[in] fuse_file_format Flag inidicating how fuses are to be written
 *
 * @param[in] hash_bytes Size of hash digest in bytes
 *
 * @param[in] hash_data Location of hash digest
 *
 * @remark If @a fuse_file_format is set the digest value is written as one
 *         byte per 32 bit address.  Otherwise the hash digest value is
 *         written as four bytes per 32 bit address.
 *
 * @pre  None
 *
 * @post Memory pointed to by @a hash_data is released
 *
 * @post if any one of the checks fails the program usage is displayed on
 *       STDOUT and the program exits.
 *
 * @returns None
 */
static void
write_srk_table_fuses(const char *fuses_filename,
                      fuse_format_t fuse_file_format,
                      size_t hash_bytes,
                      uint8_t *hash_data);

/** Generates SRK data
 *
 * This function uses openssl API to extract exponent and modulus from
 * each SRK X509 certificate. It call other functions to update srk table array
 *
 * @param[in] target Define which component is targeted, HAB4 or AHAB
 *
 * @param[in] certs Array of structs holding the cert filename and hash flag
 *
 * @param[in] num_certs number of entries in the @a certs
 *
 * @param[in] hash_alg Charater string containing the hash algorithm
 *            e.g. "sha1" or "sha256"
 *
 * @param[in] hash_only TRUE if caller wants hash of key to be inserted in the
 *            table, FALSE if caller wants full key to be inserted in the table
 *
 * @param[out] total_bytes  size of g_table_data global table
 *             data array in bytes
 *
 * @param[in] sd_alg_str Define which signature hash algorithm will be used
 *                       in conjunction with the given key
 *
 * @pre  none
 *
 * @post  if successful, g_table_data will have the SRK table and
 *        g_index_to_table_data modified.
 *
 * @remark All certificates provided in @a certs must have the same
 *         configuration for the basicConstaints CA extension.
 *
 * @returns #CST_SUCCESS if successful, #CST_FAILURE otherwise
 */
static cst_status_t
generate_srk_data(tgt_t target,
                  const cert_file_t *certs,
                  uint32_t num_certs,
                  const char *hash_alg,
                  bool hash_only,
                  uint32_t *total_bytes,
                  const char *sd_alg_str);

/** Update table data global array
 *
 * This function updates g_table_data global srk table entry using the data
 * provided in the srk entry. This function is called for each SRK certificate.
 *
 * @param[in] srk Pointer to SRK table entry structure
 *
 * @param[in] exp_bytes Size of byte array in third parameter
 *
 * @param[in] hash #TRUE if caller wants hash of key to be inserted in table
 *                 #FALSE if caller wants full key to be inserted in table
 *
 * @param[in] hash_alg Charater string containing the hash algorithm
 *            e.g. "sha1" or "sha256"
 *
 * @param[in] exclude_hash_header set to #TRUE if caller does not want
 *            hashed key header bytes to be included in table data.
 *
 * @pre @a srk and @a hash_alg must not be NULL
 *
 * @pre The data pointed to by @a srk->entry follows the key data stucture
 *      format described in the HAB4 SIS.
 *
 * @post  if successful, g_table_data will be updated with new key.
 *
 * @returns the next index in table or it can be interpreted as new size of
 *          table.
 */
static uint32_t
update_table_data(const srk_entry_t *srk,
                  bool hash,
                  const char *hash_alg,
                  bool exclude_hash_header);

/* ------------------------ Generic functions ------------------------ */

/** Checks for a valid hab_ver command line argument and converts
 *  @version_str to integer format
 *
 * @param[in] version_str Character string containing the HAB version
 *
 * @pre  @a version_str is not NULL
 *
 * @remark If @a version_str contains unknown version number an error is
 *         displayed to STDOUT and the program exits.
 *
 * @retval #HAB_4 if @a version_str is "4".
 */
static hab_version_t
check_hab_version(const char *alg_str);

/** Checks for a valid digest command line argument and converts @a alg_str
 *  to a correspinding integer value.
 *
 * @param[in] target Define which component is targeted, HAB4 or AHAB
 *
 * @param[in] alg_str Character string containing the digest algorithm
 *
 * @pre  @a alg_str is not NULL
 *
 * @remark If @a alg_str contains unknown algorithm string an error is
 *         displayed to STDOUT and the program exits.
 *
 * @retval #HAB_ALG_SHA1 if @a alg_str is "sha1" and target is HAB,
 *
 * @retval #HAB_ALG_SHA256 if @a alg_str is "sha256" and target is HAB,
 *
 * @retval #HAB_ALG_SHA512 if @a alg_str is "sha512" and target is AHAB.
 */
static uint32_t
check_digest_alg(tgt_t target, const char *alg_str);

/** Checks for a valid fuse file format command line argument and converts
 *  @a format_str to a corresponding integer value.
 *
 * @param[in] format_str Character string containing the fuse file format
 *            selection
 *
 * @pre  @a format_str is not NULL
 *
 * @remark If @format_str contains unknown format an error is
 *         displayed to STDOUT and the program exits.
 *
 * @retval #EIGHT_FUSES_PER_WORD, if @a format_str is "0",
 *
 * @retval #THIRTY_TWO_FUSES_PER_WORD, if @a format_str is "1".
 */
static fuse_format_t
check_fuse_format(const char *format_str);

/** Parses the certificate filenames from --cert command line argument
 *
 * Separates the filenames in @a cert_str into the filename elements of
 * @a certs and when required removes the '%' for hashed keys and sets
 * the hash flag element in @a certs.
 *
 * @param[in] version HAB version
 *
 * @param[in] cert_str Character string holding all certificate filenames
 *            separated by a ','.  Also includes filenames with a leading
 *            '%' requesting a hashed key.
 *
 * @param[in] certs Array holding the certificate filename and hash flag
 *
 * @pre @a cert_str and @certs are not NULL
 *
 * @pre  @a format_str is not NULL, @a certs has #MAX_CERTIFICATES_ALLOWED
 *       entries when @a version is for HAB4.
 *
 * @remark If @format_str contains unknown format an error is
 *         displayed to STDOUT and the program exits.
 *
 * @returns The number of certificates provided in @a cert_str
 */
static uint32_t
check_hab_cert_filenames(hab_version_t version,
                         const char *cert_str,
                         cert_file_t *certs);

/** Parses the certificate filenames from --cert command line argument
 *
 * Separates the filenames in @a cert_str into the filename elements of
 * @a certs and when required removes the '%' for hashed keys and sets
 * the hash flag element in @a certs.
 *
 * @param[in] cert_str Character string holding all certificate filenames
 *            separated by a ','.  Also includes filenames with a leading
 *            '%' requesting a hashed key.
 *
 * @param[in] certs Array holding the certificate filename and hash flag
 *
 * @pre @a cert_str and @certs are not NULL
 *
 * @pre  @a format_str is not NULL, @a certs has #MAX_CERTIFICATES_ALLOWED
 *       entries when @a version is for AHAB.
 *
 * @remark If @format_str contains unknown format an error is
 *         displayed to STDOUT and the program exits.
 *
 * @returns The number of certificates provided in @a cert_str
 */
static uint32_t
check_ahab_cert_filenames(const char *cert_str,
                          cert_file_t *certs);

/** prints the command line usage
 *
 * Prints the usage information for running srktool. It also shows
 * examples of command line parameters for running srktool
 *
 * @pre  This function is called when a failure to process command
 *       line successfully has occured.
 *
 * @post The usage info is send to STDOUT.
 */
static void
print_usage(void);

/** Converts digest algorithm string to digest length
 *
 * @param[in] digest_alg message digest algorithm type
 *
 * @returns length in bytes of digest algorithm based on given
 *          @a digest_alg
 */
static size_t
digest_length(uint32_t digest_alg);

/*===========================================================================
                               LOCAL VARIABLES
=============================================================================*/

/*===========================================================================
                               GLOBAL VARIABLES
=============================================================================*/

/** g_table_data
 *
 * Global array used for generating SRK table data from input certificates
 *
 */
uint8_t g_table_data[MAX_SRK_TABLE_BYTES];

/** global index into g_table_data */
uint32_t  g_index_to_table_data = SRK_TABLE_HEADER_BYTES;

/*===========================================================================
                               LOCAL FUNCTIONS
=============================================================================*/

/* ------------------- HAB4 SRK Table Functions  ------------------- */

/*--------------------------
  generate_xhab_srk_table
---------------------------*/
void
generate_xhab_srk_table(tgt_t target,
                        const char *table_filename,
                        const char *fuses_filename,
                        const char *md_alg_str,
                        const cert_file_t *certs,
                        uint32_t num_certs,
                        fuse_format_t fuse_file_format,
                        const char *sd_alg_str)
{
#ifdef DEBUG
    uint32_t i;                     /**< Loop index */
#endif
    uint32_t srk_table_bytes = 0;     /**< Size of SRK table */
    size_t   hash_bytes = 0;        /**< Size of hash digest in bytes */
    uint8_t  *hash_data = NULL;     /**< Location of hash result buffer */
    char     tmp_str[MAX_STRING_BYTES]; /**< Array for temporary stings */
    FILE     *fp_table = NULL;      /**< File ptr for output SRK table */

    /* Determine length of hash based on md_alg_str */
    hash_bytes = digest_length(digest_alg_tag(md_alg_str));
    if (generate_srk_data(target, certs, num_certs,
                          md_alg_str,
                          FALSE, &srk_table_bytes, sd_alg_str) != CST_SUCCESS)
    {
        error("Failed to generate SRK table");
    }

    /* Write SRK Table to output file */
    fp_table = fopen(table_filename, "wb");
    if (!fp_table)
    {
        snprintf(tmp_str, MAX_STRING_BYTES , "Error! Unable to open %s for writing",
                table_filename);
        error(tmp_str);
    }

    if (fwrite(g_table_data, sizeof(uint8_t), srk_table_bytes, fp_table) !=
        srk_table_bytes)
    {
        snprintf(tmp_str, MAX_STRING_BYTES , "Unable to write to file %s\n", table_filename);
        error(tmp_str);
    }
    fclose(fp_table);

    if (TGT_AHAB != target)
    {
        /* Generate SRK hash value for fuses */
        srk_table_bytes = 0;
        if (generate_srk_data(target, certs, num_certs,
                              md_alg_str,
                              TRUE, &srk_table_bytes, sd_alg_str) != CST_SUCCESS)
        {
            error("Failed to generate SRK fuses\n");
        }

        hash_data = generate_hash(g_table_data + SRK_TABLE_HEADER_BYTES,
                                srk_table_bytes - SRK_TABLE_HEADER_BYTES,
                                md_alg_str, &hash_bytes);
        if (hash_data == NULL)
        {
            error("Unable to allocate memory for hash data\n");
        }

        /* Generate SRK Table fuse value and write to output file */
        write_srk_table_fuses(fuses_filename,
                            fuse_file_format,
                            hash_bytes,
                            hash_data);
    }
    else
    {
        hash_data = generate_hash(g_table_data,
                                  srk_table_bytes,
                                  md_alg_str, &hash_bytes);
        if (hash_data == NULL)
        {
            error("Unable to allocate memory for hash data\n");
        }

        /* Generate SRK Table fuse value and write to output file */
        write_srk_table_fuses(fuses_filename,
                              fuse_file_format,
                              hash_bytes,
                              hash_data);
    }

    /* Dump for verification */
#ifdef DEBUG
    printf("SRK table binary filename = %s\n", table_filename);
    printf("Hash type                 = %s\n", md_alg_str);
    printf("Fuse binary filename      = %s\n", fuses_filename);
    printf("Hash keys at index ");
    printf("Num_certs = %d\n", num_certs);
    for (i = 0; i < num_certs; i++)
    {
        printf("Certificate %d            = %s\n", i, certs[i].filename);
        printf("Certificate %d hash       = %d\n", i, certs[i].hash);
    }
#endif
    return;
}

/*--------------------------
  write_srk_table_fuses
---------------------------*/
void
write_srk_table_fuses(const char *fuses_filename,
                      fuse_format_t fuse_file_format,
                      size_t hash_bytes,
                      uint8_t *hash_data)
{
    FILE     *fp_fuses = NULL;          /**< File ptr for SRK table digest */
    uint32_t i;                         /**< Loop index */
    char     tmp_str[MAX_STRING_BYTES]; /**< Array for temporary stings */
    uint32_t srk_table_bytes = 0;       /**< Size of SRK table */

    fp_fuses = fopen(fuses_filename, "wb");
    if (!fp_fuses)
    {
        snprintf(tmp_str, MAX_STRING_BYTES , "Unable to open %s for writing\n", fuses_filename);
        error(tmp_str);
    }

    if (fuse_file_format ==  EIGHT_FUSES_PER_WORD)
    {
        srk_table_bytes = 0;
        for (i = 0; i < hash_bytes; i++)
        {
            /* Converting hash_data into requested output format for fuses */
            g_table_data[srk_table_bytes++] = 0;
            g_table_data[srk_table_bytes++] = 0;
            g_table_data[srk_table_bytes++] = 0;
            g_table_data[srk_table_bytes++] = hash_data[i];
        }
        if (fwrite(g_table_data, sizeof(uint8_t), srk_table_bytes, fp_fuses)
            != srk_table_bytes)
        {
            snprintf(tmp_str, MAX_STRING_BYTES , "Unable to write to file %s\n", fuses_filename);
            error(tmp_str);
        }
    }
    else
    {
        if (fwrite(hash_data, sizeof(uint8_t), hash_bytes, fp_fuses)
            != hash_bytes)
        {
            printf(tmp_str, "Unable to write to file %s\n", fuses_filename);
            error(tmp_str);
        }
    }
    free(hash_data);
    fclose(fp_fuses);
}

/*--------------------------
  check_xhab_arguments
---------------------------*/
static void
check_xhab_arguments(const char *table_filename,
                     const char *fuses_filename,
                     const char *md_alg_str,
                     uint32_t num_certs)

{
    /* Check required arguments for --hab_ver=4:
     *    - Output table filename
     *    - Output fuse filename
     *    - digest algorithm
     *    - at least one certificate is provided
     */
    if ((table_filename == NULL) ||
        (fuses_filename == NULL) ||
        (md_alg_str == NULL) ||
        (num_certs == 0))
    {
        print_usage();
        exit(1);
    }
    return;
}

/*--------------------------
  generate_srk_data
---------------------------*/
cst_status_t
generate_srk_data(tgt_t target,
                  const cert_file_t *certs,
                  uint32_t num_certs,
                  const char *hash_alg,
                  bool hash_only,
                  uint32_t* total_bytes,
                  const char *sd_alg_str)
{
    X509 *cert = NULL; /**< X.509 certificate */
    EVP_PKEY *pkey; /**< Public key from X.509 cert */
    uint32_t i; /**< Loop index */
    bool hash = FALSE; /**< Compute hash flag */
    srk_entry_t  srk; /**< SRK table entry */
    char tmp_str[MAX_STRING_BYTES]; /**< Array for temporary stings */
    bool ca_flag[num_certs]; /**< holds CA flags for each cert */

    /** skip header bytes */
    g_index_to_table_data = SRK_TABLE_HEADER_BYTES;
    *total_bytes = 0;

    if ((TGT_AHAB == target) && (4 != num_certs))
    {
        snprintf(tmp_str,
                MAX_STRING_BYTES,
                "AHAB always expects 4 SRKs, %d are provided\n",
                num_certs);
        error(tmp_str);
    }

    for (i = 0; i < num_certs; i++)
    {
        hash = certs[i].hash;

        /* Caller wants to build the table of hashes for computing
         * SRK hash value for programming to efuses
         */
        if (hash_only)
        {
            hash = TRUE;
        }

        cert = read_certificate(certs[i].filename);
        if (cert == NULL)
        {
            snprintf(tmp_str,
                    MAX_STRING_BYTES,
                    "Error! Failed to read certificate data from %s\n",
                    certs[i].filename);
            error(tmp_str);
        }

        /* Get public key  */
        pkey = X509_get_pubkey(cert);

        /* Determine if cert has the CA flag set */
        if (X509_check_ca(cert) == X509_CA_CERT)
        {
            ca_flag[i] = TRUE;
        }
        else
        {
            ca_flag[i] = FALSE;
        }

        if (EVP_PKEY_id(pkey) == EVP_PKEY_RSA)
        {
            srk_entry_pkcs1(target, pkey, &srk, ca_flag[i], sd_alg_str);
        }
        else if (EVP_PKEY_EC == EVP_PKEY_id(pkey))
        {
            srk_entry_ec(target, pkey, &srk, ca_flag[i], sd_alg_str);
        }
        else
        {
            EVP_PKEY_free(pkey);
            X509_free(cert);
            error("Unsupported algorithm in X.509 certificate\n");
        }
        g_index_to_table_data = update_table_data(&srk,
                                                  hash,
                                                  hash_alg,
                                                  hash_only);
        EVP_PKEY_free(pkey);
        X509_free(cert);
        free(srk.entry);

    }

    /* Check that all CA flags are consistent */
    for(i = 0; i < num_certs - 1; i++)
    {
        if(ca_flag[i] != ca_flag[i + 1])
        {
            error("All certificates must be either CA or user certs\n");
        }
    }

    /* Finally, update header info */
    g_table_data[0] = HAB_TAG_CRT;
    if (TGT_AHAB != target)
    {
        g_table_data[1] = EXTRACT_BYTE(g_index_to_table_data, 8);
        g_table_data[2] = EXTRACT_BYTE(g_index_to_table_data, 0);
        g_table_data[3] = HAB4_DATA_STRUCT_VER;
    }
    else
    {
        g_table_data[1] = EXTRACT_BYTE(g_index_to_table_data, 0);
        g_table_data[2] = EXTRACT_BYTE(g_index_to_table_data, 8);
        g_table_data[3] = SRK_TABLE_VERSION;
    }
    /* Return size of table in total_bytes */
    *total_bytes = g_index_to_table_data;

    return CST_SUCCESS;
}

/*--------------------------
  update_table_data
---------------------------*/
static uint32_t
update_table_data(const srk_entry_t *srk,
                  bool hash,
                  const char *hash_alg,
                  bool exclude_hash_header)
{
    uint32_t i; /**< Loop index */
    uint8_t  *hash_data = NULL; /**< Location of hash digest */
    size_t   hash_bytes; /**< Hash digest size in bytes */
    uint32_t index = g_index_to_table_data; /**< local copy of global index */

    if (hash == FALSE)
    {
      /* Copy srk entry into the table */
        memcpy(&g_table_data[index], srk->entry, srk->entry_bytes);
        index += srk->entry_bytes;
    }
    else
    {
        /* Get the hash of key in g_table_data */
        hash_data = generate_hash(srk->entry, srk->entry_bytes,
                                  hash_alg, &hash_bytes);
        if (!hash_data)
        {
            error("Failed to generate SRK table hash digest");
        }
        else
        {
            /* The header is excluded when generating the hash digest
             * result for efuses
             */
            if (exclude_hash_header == FALSE)
            {
                g_table_data[index++] = HAB_KEY_HASH;
                g_table_data[index++] =
                    EXTRACT_BYTE((hash_bytes + SRK_TABLE_HEADER_BYTES), 8);
                g_table_data[index++] =
                    EXTRACT_BYTE((hash_bytes + SRK_TABLE_HEADER_BYTES), 0);
                g_table_data[index++] = digest_alg_tag(hash_alg);
            }

            /* Copy hash digest to table */
            for (i = 0; i < hash_bytes; i++)
            {
                g_table_data[index++] = hash_data[i];
            }

            /* Clean up hash_data allocated in generate_hash() */
            free(hash_data);
        }
    }
    return index;
}

/* ------------------------ Generic functions ------------------------ */

/*--------------------------
  check_hab_version
---------------------------*/
hab_version_t
check_hab_version(const char *version_str)
{
    /**< Default version is HAB4 */
    hab_version_t hab_ver = (hab_version_t)atoi(version_str);

    if (hab_ver != HAB_4)
    {
        error("Invalid HAB version");
        print_usage();
    }
    return hab_ver;
}

/*--------------------------
  check_digest_alg
---------------------------*/
uint32_t
check_digest_alg(tgt_t target, const char *alg_str)
{
    uint32_t algorithm = digest_alg_tag(alg_str);

    switch (target)
    {
        case TGT_HAB:
            if ((algorithm != HAB_ALG_SHA1) && (algorithm != HAB_ALG_SHA256))
            {
                error("Unsupported message digest algorithm");
                print_usage();
            }
            break;

        case TGT_AHAB:
            if (algorithm != HAB_ALG_SHA512)
            {
                error("Unsupported message digest algorithm");
                print_usage();
            }
            break;

        default:
            error("Target not specified");
            print_usage();
    }

    return algorithm;
}

/*--------------------------
  check_fuse_format
---------------------------*/
fuse_format_t
check_fuse_format(const char *format_str)
{
    fuse_format_t format = (fuse_format_t)atoi(format_str);

    if ((format != EIGHT_FUSES_PER_WORD) &&
        (format != THIRTY_TWO_FUSES_PER_WORD))
    {
        error("Unsupported fuse file format");
        print_usage();
    }
    return format;
}

/*--------------------------
  check_hab_cert_filenames
---------------------------*/
uint32_t
check_hab_cert_filenames(hab_version_t version,
                         const char *cert_str,
                         cert_file_t *certs)
{
    uint32_t max_certs;
    uint32_t num_certs = 0;
    uint32_t i;


    /* Initialize cert filename array */
    max_certs = MAX_CERTIFICATES_ALLOWED;

    for (i = 0; i < max_certs; i++)
    {
        certs[i].hash = FALSE;
        certs[i].filename = NULL;
    }


    /* There could be multiple certs in cmd line */
    certs[num_certs].filename = strtok((char *)cert_str,
                                        CMDLINE_ARG_CERT_SEPARATOR);

    while (certs[num_certs].filename != NULL)
    {
        if ((version == HAB_4) &&
            (certs[num_certs].filename[0] == CMDLINE_ARG_HASH_INDICATOR))
        {
            certs[num_certs].hash = TRUE;
            certs[num_certs].filename = &certs[num_certs].filename[1];
        }
        num_certs++;

        if (num_certs < max_certs)
        {
            certs[num_certs].filename = strtok(NULL,
                                               CMDLINE_ARG_CERT_SEPARATOR);
        }
        else
        {
            break;
        }
    }
    return num_certs;
}

/*--------------------------
  check_ahab_cert_filenames
---------------------------*/
uint32_t
check_ahab_cert_filenames(const char *cert_str,
                          cert_file_t *certs)
{
    uint32_t max_certs = MAX_CERTIFICATES_ALLOWED;
    uint32_t num_certs = 0;
    uint32_t i;

    /* Initialize cert filename array */
    for (i = 0; i < max_certs; i++)
    {
        certs[i].hash = FALSE;
        certs[i].filename = NULL;
    }

    /* There could be multiple certs in cmd line */
    certs[num_certs].filename = strtok((char *)cert_str,
                                        CMDLINE_ARG_CERT_SEPARATOR);

    while (certs[num_certs].filename != NULL)
    {
        num_certs++;

        if (num_certs < max_certs)
        {
            certs[num_certs].filename = strtok(NULL,
                                               CMDLINE_ARG_CERT_SEPARATOR);
        }
        else
        {
            break;
        }
    }
    return num_certs;
}

/*--------------------------
  print_usage
---------------------------*/
void print_usage(void)
{
    printf("\n Usage: \n\n");
    printf("To generate SRK Table data and the SRK Table hash for AHAB\n");
    printf("==========================================================\n\n");
    printf("srktool --ahab_ver --table <tablefile>");
    printf(" --efuses <efusefile>\n");
    printf("        --sign_digest <digestalg> --certs <srk>,<srk>,... \n");
    printf("        [--fuse_format <format>] [--license]\n\n");
    printf("  -a, --ahab_ver: \n");
    printf("      AHAB Version - set for AHAB SRK table generation\n\n");
    printf("  -t, --table <tablefile>:\n");
    printf("      Filename for output SRK table binary file\n\n");
    printf("  -e, --efuses <efusefile>:\n");
    printf("      Filename for the output SRK efuse binary file containing ");
    printf("the SRK table\n      hash\n\n");
    printf("  -s, --sign_digest <digestalg>:\n");
    printf("      Signature Digest algorithm. Either sha256, sha384 or ");
    printf("sha512\n\n");
    printf("  -c, --certs <srk1>,<srk2>,...,<srk4>:\n");
    printf("      X.509v3 certificate filenames.\n");
    printf("        - Certificates may be either DER or PEM encoded format\n");
    printf("        - Certificate filenames must be separated by a ','");
    printf("with no spaces\n");
    printf("        - A maximum of 4 certificate filenames may be provided. ");
    printf("Additional\n          certificate names are ignored\n");
    printf("  -f, --fuse_format <format>:\n");
    printf("      Optional, Data format of the SRK efuse binary file.  The\n");
    printf("      format may be selected by setting <format> to either: \n");
    printf("        - 0: 8 fuses per word, ");
    printf("ex: 00 00 00 0a 00 00 00 01 ...\n");
    printf("        - 1 (default): 32 fuses per word, ex: 0a 01 ff 8e\n\n");
    printf("   -l, --license:\n");
    printf("      Optional, displays program license information.  No ");
    printf("additional\n      arguments are required.\n\n");
    printf("   -v, --version:\n");
    printf("      Optional, displays the version of the tool.  No ");
    printf("additional\n      arguments are required.\n\n");
    printf("Examples:\n");
    printf("---------\n\n");
    printf("1. To generate an SRK table and corresponding fuse pattern from ");
    printf("3 certificates\n");
    printf("    - using PEM encoded certificate files\n");
    printf("    - using the default 32 fuse bits per word for the efuse file\n\n");
    printf("    srktool --ahab_ver --sign_digest sha384 --table table.bin ");
    printf(" --efuses fuses.bin \\ \n");
    printf("            --certs srk1_crt.pem,srk2_crt.pem,");
    printf("srk3_crt.pem\n\n");
    printf("2. To generate an SRK table and corresponding fuse pattern from ");
    printf("2 certificates\n");
    printf("    - using DER encoded certificate files\n");
    printf("    - using the optional 8 fuse bits per word for the efuse file\n\n");
    printf("    srktool --ahab_ver --sign_digest sha256 --table table.bin ");
    printf(" --efuses fuses.bin \\ \n");
    printf("            --certs srk1_crt.pem,srk2_crt.pem \\ \n");
    printf("            --fuse_format 1\n\n");

    printf("\n Usage: \n\n");
    printf("To generate SRK Table data and the SRK Table hash for HAB4\n");
    printf("==========================================================\n\n");
    printf("srktool --hab_ver <version> --table <tablefile>");
    printf(" --efuses <efusefile>\n");
    printf("        --digest <digestalg> --certs <srk>,%%<srk>,... \n");
    printf("        [--fuse_format <format>] [--license]\n\n");
    printf("  -h, --hab_ver <version>: \n");
    printf("      HAB Version - set to 4 for HAB4 SRK table generation\n\n");
    printf("  -t, --table <tablefile>:\n");
    printf("      Filename for output SRK table binary file\n\n");
    printf("  -e, --efuses <efusefile>:\n");
    printf("      Filename for the output SRK efuse binary file containing ");
    printf("the SRK table\n      hash\n\n");
    printf("  -d, --digest <digestalg>:\n");
    printf("      Message Digest algorithm. Only sha256 is supported\n\n");
    printf("  -c, --certs <srk1>,<srk2>,...,<srk4>:\n");
    printf("      X.509v3 certificate filenames.\n");
    printf("        - Certificates may be either DER or PEM encoded format\n");
    printf("        - Certificate filenames must be separated by a ','");
    printf("with no spaces\n");
    printf("        - A maximum of 4 certificate filenames may be provided. ");
    printf("Additional\n          certificate names are ignored\n");
    printf("        - Placing a %% in front of a filename replaces the public\n");
    printf("          key data in the SRK table with a corresponding hash ");
    printf("digest\n\n");
    printf("  -f, --fuse_format <format>:\n");
    printf("      Optional, Data format of the SRK efuse binary file.  The\n");
    printf("      format may be selected by setting <format> to either: \n");
    printf("        - 0: 8 fuses per word, ");
    printf("ex: 00 00 00 0a 00 00 00 01 ...\n");
    printf("        - 1 (default): 32 fuses per word, ex: 0a 01 ff 8e\n\n");
    printf("   -l, --license:\n");
    printf("      Optional, displays program license information.  No ");
    printf("additional\n      arguments are required.\n\n");
    printf("   -v, --version:\n");
    printf("      Optional, displays the version of the tool.  No ");
    printf("additional\n      arguments are required.\n\n");
    printf("Examples:\n");
    printf("---------\n\n");
    printf("1. To generate an SRK table and corresponding fuse pattern from ");
    printf("3 certificates\n");
    printf("    - using PEM encoded certificate files\n");
    printf("    - using full key for first two certs and hash digest for ");
    printf("the third\n");
    printf("    - using the default 32 fuse bits per word for the efuse file\n\n");
    printf("    srktool --hab_ver 4 --table table.bin ");
    printf(" --efuses fuses.bin \\ \n");
    printf("            --digest sha256 \\ \n");
    printf("            --certs srk1_crt.pem,srk2_crt.pem,");
    printf("%%srk3_crt.pem\n\n");
    printf("2. To generate an SRK table and corresponding fuse pattern from ");
    printf("2 certificates\n");
    printf("    - using DER encoded certificate files\n");
    printf("    - using the optional 8 fuse bits per word for the efuse file\n\n");
    printf("    srktool --hab_ver 4 --table table.bin ");
    printf(" --efuses fuses.bin \\ \n");
    printf("            --digest sha256 \\ \n");
    printf("            --certs srk1_crt.pem,srk2_crt.pem \\ \n");
    printf("            --fuse_format 1\n\n");
}

/*--------------------------
  digest_length
---------------------------*/
size_t
digest_length(uint32_t digest_alg)
{
    uint32_t length = 0;

    if (digest_alg == HAB_ALG_SHA1)
    {
        length = HASH_BYTES_SHA1;
    }
    else if (digest_alg == HAB_ALG_SHA256)
    {
        length = HASH_BYTES_SHA256;
    }
    else if (digest_alg == HAB_ALG_SHA512)
    {
        length = HASH_BYTES_SHA512;
    }

    return length;
}

/*===========================================================================
                               GLOBAL FUNCTIONS
=============================================================================*/

/** main
 *
 * Entry point main function for the app, processes input arguments and
 * call necessary local function to generate the SRK table.
 *
 * @param[in] argc, number of arguments in argv
 *
 * @param[in] argv, array of arguments.
 *
 * @pre  none
 *
 * @post  if successful, generate a HAB/AHAB SRK table.
 *
 * @returns #CST_SUCCESS on success and #CST_FAILURE otherwise.
 */

int32_t main (int32_t argc, char *argv[])
{
    /** Format for HAB4 SRK fuses */
    fuse_format_t fuse_file_format = THIRTY_TWO_FUSES_PER_WORD;
    /** Holds the certificate filenames */
    cert_file_t x509_certs[MAX_CERTIFICATES_ALLOWED];
    tgt_t target = TGT_UNDEF;      /**< Holds target deduced from command line */
    hab_version_t hab_version = 0; /**< Holds hab_version from command line */
    char *table_filename = NULL;   /**< String holding SRK table filename */
    char *fuses_filename = NULL;   /**< String holding SRK fuses filename */
    char *cert_filenames = NULL;   /**< String holding certificate arguments */
    uint32_t num_certs = 0;        /**< Count of certificate arguments */
    const char *md_alg_str = NULL; /**< Sting holding digest algorithm */
    const char *sd_alg_str = NULL; /**< Sting holding signature digest algorithm */
    int  next_option = 0;          /**< Option count from cmd line */

    /* Initialize openssl - required in order to call openssl helper
     * functions
     */
    openssl_initialize();

    /* Parse command line arguments */
    do
    {
        next_option = getopt_long(argc, argv, short_options,
                                  long_options, NULL);
        switch (next_option)
        {
            /* Display License information */
            case 'l':
                print_license();
                exit(0);
                break;
            /* Display version information */
            case 'v':
                print_version();
                exit(0);
                break;
            case 'h':
                if (TGT_UNDEF == target)
                {
                    target = TGT_HAB;
                }
                else
                {
                    print_usage();
                    error("Target already defined");
                }
                hab_version = check_hab_version(optarg);
                break;
            case 'a':
                if (TGT_UNDEF == target)
                {
                    target = TGT_AHAB;
                }
                else
                {
                    print_usage();
                    error("Target already defined");
                }
                break;
            case 't':
                table_filename = optarg;
                break;
            case 'e':
                fuses_filename = optarg;
                break;
            case 'f':
                fuse_file_format = check_fuse_format(optarg);
                break;
            case 'd':
                md_alg_str = optarg;
                break;
            case 's':
                sd_alg_str = optarg;
                break;
            case 'c':
                cert_filenames = optarg;
                break;
            /* Handle invalid options */
            case '?':
                print_usage();
                exit(1);
                break;
            default:    /* Something else: unexpected.  */
                break;
        }
    } while (next_option != -1);

    if (TGT_UNDEF == target)
    {
        print_usage();
        error("Target not specified");
    }
    else if (TGT_AHAB == target)
    {
        if (NULL == md_alg_str)
        {
            md_alg_str = ahab_md_alg_str;
        }
        check_digest_alg(target, md_alg_str);

        check_sign_digest_alg(sd_alg_str);

        num_certs = check_ahab_cert_filenames(cert_filenames,
                                              x509_certs);
        check_xhab_arguments(table_filename, fuses_filename, md_alg_str,
                             num_certs);
        generate_xhab_srk_table(target, table_filename, fuses_filename, md_alg_str,
                                x509_certs, num_certs, fuse_file_format, sd_alg_str);
    }
    else
    {
        if (NULL != sd_alg_str)
        {
            print_usage();
            error("Unsupported option for this target: -s, --sign_digest");
        }

        if (hab_version == HAB_4)
        {
            check_digest_alg(target, md_alg_str);

            num_certs = check_hab_cert_filenames(hab_version,
                                                cert_filenames,
                                                x509_certs);
            check_xhab_arguments(table_filename, fuses_filename, md_alg_str,
                                num_certs);
            generate_xhab_srk_table(target, table_filename, fuses_filename, md_alg_str,
                                    x509_certs, num_certs, fuse_file_format, "sha256");
        }
        else
        {
            print_usage();
        }
    }

    return 0;
}
