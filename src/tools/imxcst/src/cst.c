/*===========================================================================*/
/**
    @file    cst.c

    @brief   Code signing tool's main file, calls parser to parse CSF
             commands and creates output binary csf with csf commands data,
             certificates and signatures generated while processing csf
             commands.

@verbatim
=============================================================================

              Freescale Semiconductor
      (c) Freescale Semiconductor, Inc. 2011-2015. All rights reserved.
      Copyright 2018-2019 NXP

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
#include <strings.h> /* strcasecmp */
#include <getopt.h>
#include <errno.h>
#define HAB_FUTURE
#include "hab_cmd.h"
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include "openssl_helper.h"
#include "csf.h"
/*===========================================================================
                                MACROS
=============================================================================*/
/* Max size of buffer for signature bytes */
#define SIGNATURE_BUFFER_SIZE (1024)

#define MAX_ERROR_STR_LEN     (512)
#define MIN_NUM_CLI_ARGS      3 /* Minimum number of command line arguments */
#define WORD_ALIGN(x) (((x + (4-1)) / 4) * 4) /**< Aligns x to next word */
#define RNG_SEED_BYTES        (128) /* MAX bytes to seed RNG */

#define CST_FAILURE_EXIT_CODE (1) /* code returned from main on failure */
/*===========================================================================
                                EXTERNS
=============================================================================*/
/* parser function */
extern int32_t yyparse(void);
extern FILE *yyin;
extern int g_no_ca;
/*===========================================================================
                  INSTANTIATE GLOBAL VARIABLES
=============================================================================*/

const char *g_tool_name = "CST"; /**< Global holds tool name */

/**
 * Points to the input CSF text file
 */
char * g_in_csf_file = NULL;

/**
 * The errors generated while processing csf will be stored in g_error_code,
 *  it always show the last error.
 */
int32_t g_error_code = SUCCESS;

/**
 * The buffer of max size HAB_CSF_BYTES_MAX to place CSF commands
 */
uint8_t g_csf_buffer[HAB_CSF_BYTES_MAX];

/**
 * Always shows the position in g_csf_buffer where data can be written. It
 * also gives the size of csf data (commands only)
 */
uint32_t g_csf_buffer_index = 0;

/**
 * AHAB data
 */
ahab_data_t g_ahab_data = {
    NULL, NULL, 0, 0, 0, NULL, NULL, 0, NULL, {false, 0, 0}, NULL, NULL, NULL, 0, 0, 0
};

/**
 * Array of size HAB_KEY_PUBLIC_MAX for key cert file names.
 */
char *g_key_certs[HAB_KEY_PUBLIC_MAX] = {
    NULL, NULL, NULL, NULL, NULL
};

/**
 * Array of AES keys.
 */
aes_key_t g_aes_keys[HAB_KEY_SECRET_MAX] = {
    {0, NULL},
    {0, NULL},
    {0, NULL},
    {0, NULL}
};

/**
 * Shows the target specified. Can be either HAB or AHAB only.
 */
tgt_t g_target = TGT_UNDEF;

/**
 * Shows the hab version specified in csf.
 */
uint8_t g_hab_version = 0x00;

/**
 * Store the ahab version for future usage
 */
uint8_t g_ahab_version = 0x00;

/**
 * Store the functional mode
 */
func_mode_t g_mode = MODE_UNDEF;

/**
 * Shows default hash algorithm specified in header command of csf file.
 */
uint32_t g_hash_alg = 0;

/**
 * Shows default engine pecified in header command of csf file.
 */
uint32_t g_engine = 0;

/**
 * Shows default engine configuration specified in header command of csf file.
 */
uint32_t g_engine_config = 0;

/**
 * Shows default certificate format specified in header command of csf file.
 */
uint32_t g_cert_format = 0;

/**
 * Shows default signature format specified in header command of csf file.
 */
sig_fmt_t g_sig_format = SIG_FMT_UNDEF;

/**
 * Set if Unlock RNG command is present in CSF
 */
uint32_t g_unlock_rng = 0;

/**
 * Set if Init RNG command is present in CSF
 */
uint32_t g_init_rng = 0;

/**
 * Points to the head of commands list.
 */
command_t *g_cmd_head = NULL;

/**
 * Points to the current command being processed from the commands list.
 */
command_t *g_cmd_current = NULL;

/**
 * Used to check the correct order of the parsed CSF commands
 */
uint8_t g_cmd_seq_stage = WAITING_FOR_HEADER_STATE;

/**
 * Points to the argument passed on command line for public key
 * certificate for encrypting the dek
 */
char * g_cert_dek = NULL;

/**
 * Set if a DEK is provided to encrypt the image
 */
uint32_t g_reuse_dek = 0;
/**
 * Set to skip user agreement prompt
 */
uint32_t g_skip = 0;

/*===========================================================================
                  LOCAL VARIABLES
=============================================================================*/
/** Valid short command line option letters. */
const char* const short_options = "lvh:lvhdso:i:c";

/** Valid long command line options. */
const struct option long_options[] =
{
    {"license", no_argument, 0, 'l'},
    {"version", no_argument,  0, 'v'},
    {"help", no_argument, 0, 'h'},
    {"output", required_argument,  0, 'o'},
    {"input", required_argument,  0, 'i'},
    {"cert", optional_argument,  0, 'c'},
    {"dek", no_argument,  0, 'd'},
    {"skip", no_argument,  0, 's'},
    {NULL, 0, NULL, 0}
};

/**
 * Map of argument string specified in csf to argument Id.
 */
static map_t argument_map[] = {
    {"Version", (uint32_t)Version},
    {"UID", (uint32_t)UID},
    {"HashAlgorithm", (uint32_t)HashAlgorithm},
    {"Engine", (uint32_t)EngineName},
    {"EngineConfiguration", (uint32_t)EngineConfiguration},
    {"CertificateFormat", (uint32_t)CertificateFormat},
    {"SignatureFormat", (uint32_t)SignatureFormat},
    {"File", (uint32_t)Filename},
    {"SourceIndex", (uint32_t)SourceIndex},
    {"VerificationIndex", (uint32_t)VerificationIndex},
    {"TargetIndex", (uint32_t)TargetIndex},
    {"Blocks", (uint32_t)Blocks},
    {"Width", (uint32_t)Width},
    {"AddressData", (uint32_t)AddressData},
    {"Count", (uint32_t)Count},
    {"AddressMask", (uint32_t)AddressMask},
    {"Bank", (uint32_t)Bank},
    {"Row", (uint32_t)Row},
    {"Fuse", (uint32_t)Fuse},
    {"Bits", (uint32_t)Bits},
    {"Features", (uint32_t)Features},
    {"BlobAddress", (uint32_t)BlobAddress},
    {"Key", (uint32_t)Key},
    {"KeyLength", (uint32_t)KeyLength},
    {"MacBytes", (uint32_t)MacBytes},
    {"Target", (uint32_t)Target},
    {"Source", (uint32_t)Source},
    {"Permissions", (uint32_t)Permissions},
    {"Offsets", (uint32_t)Offsets},
    {"Mode", (uint32_t)Mode},
    {"Signature", (uint32_t)Signature},
    {"SourceSet", (uint32_t)SourceSet},
    {"Revocations", (uint32_t)Revocations},
    {"KeyIdentifier", (uint32_t)KeyIdentifier},
    {"ImageIndexes", (uint32_t)ImageIndexes},
};

/**
 * number of arguments.
 */
static uint32_t argument_count = sizeof(argument_map)/sizeof(argument_map[0]);

/**
 * Map of command string specified in csf to command id and command handler
 * function.
 */
static map_cmd_t command_map[] = {
    {"Header", CmdHeader, cmd_handler_header},
    {"InstallSRK", CmdInstallSRK, cmd_handler_installsrk},
    {"InstallCSFK", CmdInstallCSFK, cmd_handler_installcsfk},
    {"InstallNOCAK", CmdInstallNOCAK, cmd_handler_installnocak},
    {"AuthenticateCSF", CmdAuthenticateCSF, cmd_handler_authenticatecsf},
    {"InstallKey", CmdInstallKEY, cmd_handler_installkey},
    {"AuthenticateData", CmdAuthenticateData, cmd_handler_authenticatedata},
    {"InstallSecretKey", CmdInstallSecretKEY, cmd_handler_installsecretkey},
    {"DecryptData", CmdDecryptData, cmd_handler_decryptdata},
    {"NOP", CmdNOP, cmd_handler_nop},
    {"SetEngine", CmdSetEngine, cmd_handler_setengine},
    {"Init", CmdInit, cmd_handler_init},
    {"Unlock", CmdUnlock, cmd_handler_unlock},
    {"InstallCertificate", CmdInstallCert, cmd_handler_installcrt},
};

/**
 * Number of commands.
 */
static uint32_t command_count = sizeof(command_map)/sizeof(command_map[0]);


/**
 * Map to label string specified in csf to its value from hab_types.h.
 */
static map_t label_map[] = {
    {"sha256", (uint32_t)HAB_ALG_SHA256},
    {"sha1", (uint32_t)HAB_ALG_SHA1},
    {"sha512", (uint32_t)HAB_ALG_SHA512},
    {"any", (uint32_t)HAB_ENG_ANY},
    {"scc", (uint32_t)HAB_ENG_SCC},
    {"rtic", (uint32_t)HAB_ENG_RTIC},
    {"caam", (uint32_t)HAB_ENG_CAAM},
    {"sahara", (uint32_t)HAB_ENG_SAHARA},
    {"csu", (uint32_t)HAB_ENG_CSU},
    {"srtc", (uint32_t)HAB_ENG_SRTC},
    {"sjc", (uint32_t)HAB_ENG_SJC},
    {"wdog", (uint32_t)HAB_ENG_WDOG},
    {"src", (uint32_t)HAB_ENG_SRC},
    {"spba", (uint32_t)HAB_ENG_SPBA},
    {"iim", (uint32_t)HAB_ENG_IIM},
    {"iomux", (uint32_t)HAB_ENG_IOMUX},
    {"dcp", (uint32_t)HAB_ENG_DCP},
    {"rtl", (uint32_t)HAB_ENG_RTL},
    {"sw", (uint32_t)HAB_ENG_SW},
    {"x509", (uint32_t)HAB_PCL_X509},
    {"CMS", (uint32_t)SIG_FMT_CMS},
    {"srk", (uint32_t)HAB_IDX_SRK},
    {"csfk", (uint32_t)HAB_IDX_CSFK},
    {"OCOTP", (uint32_t)HAB_ENG_OCOTP},
    {"SNVS", (uint32_t)HAB_ENG_SNVS},
    {"DTCP", (uint32_t)HAB_ENG_DTCP},
    {"HDCP", (uint32_t)HAB_ENG_HDCP},
    {"ROM", (uint32_t)HAB_ENG_ROM},
    {"JTAG", (uint32_t)HAB_OCOTP_UNLOCK_JTAG},
    {"SRKREVOKE", (uint32_t)HAB_OCOTP_UNLOCK_SRK_REVOKE},
    {"SCS", (uint32_t)HAB_OCOTP_UNLOCK_SCS},
    {"FIELDRETURN", (uint32_t)HAB_OCOTP_UNLOCK_FIELD_RETURN},
    {"MID", (uint32_t)HAB_CAAM_UNLOCK_MID},
    {"RNG", (uint32_t)HAB_CAAM_UNLOCK_RNG},
    {"MFG", (uint32_t)HAB_CAAM_UNLOCK_MFG},
    {"LPSWR", (uint32_t)HAB_SNVS_UNLOCK_LP_SWR},
    {"ZMKWRITE", (uint32_t)HAB_SNVS_UNLOCK_ZMK_WRITE},
    {"Open", (uint32_t)HAB_CFG_OPEN},
    {"Closed", (uint32_t)HAB_CFG_CLOSED},
    {"HABRSASHA1", (uint32_t)HAB_ALG_SHA1},
    {"HABRSASHA256", (uint32_t)HAB_ALG_SHA256},
    {"INSWAP8", (uint32_t)ENG_CFG_IN_SWAP_8},
    {"INSWAP16", (uint32_t)ENG_CFG_IN_SWAP_16},
    {"INSWAP32", (uint32_t)ENG_CFG_IN_SWAP_32},
    {"OUTSWAP8", (uint32_t)ENG_CFG_OUT_SWAP_8},
    {"OUTSWAP16", (uint32_t)ENG_CFG_OUT_SWAP_16},
    {"OUTSWAP32", (uint32_t)ENG_CFG_OUT_SWAP_32},
    {"DSCSWAP8", (uint32_t)ENG_CFG_DSC_SWAP_8},
    {"DSCSWAP16", (uint32_t)ENG_CFG_DSC_SWAP_16},
    {"DSCBE816", (uint32_t)ENG_CFG_DSC_BE_8_16},
    {"DSCBE832", (uint32_t)ENG_CFG_DSC_BE_8_32},
    {"KEEP", (uint32_t)ENG_CFG_KEEP},
    {"HAB", (uint32_t)TGT_HAB},
    {"AHAB", (uint32_t)TGT_AHAB},
    {"HSM", (uint32_t)MODE_HSM},
    {"OEM", (uint32_t)SRK_SET_OEM},
    {"NXP", (uint32_t)SRK_SET_NXP},
};

/**
 * Number of labels
 */
static uint32_t label_count = sizeof(label_map)/sizeof(label_map[0]);

/**
 * Buffer to log more meaningful error messages of max len MAX_ERROR_STR_LEN.
 * +1 extra to allow for null terminator.
 */
char error_log[MAX_ERROR_STR_LEN+1];

/*===========================================================================
                          LOCAL FUNCTION DECLARATIONS
=============================================================================*/
static int update_offsets_in_csf(uint8_t * buf, command_t *cmd_csf,
                          uint32_t csf_len);
static void print_usage(void);
static void process_cmdline_args(int argc, char* argv[], char **out_bin_csf);
static void print_error_msg(const int32_t error_code);
static void prompt_key_reuse_msg(void);
static int32_t check_command_sequence(command_t *cmd);

#if defined _WIN32 || defined __CYGWIN__
#define GETLINE_MINSIZE 16
int getline(char **lineptr, size_t *n, FILE *fp) {
    int ch;
    int i = 0;
    char free_on_err = 0;
    char *p;

    errno = 0;
    if (lineptr == NULL || n == NULL || fp == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (*lineptr == NULL) {
        *n = GETLINE_MINSIZE;
        *lineptr = (char *)malloc( sizeof(char) * (*n));
        if (*lineptr == NULL) {
            errno = ENOMEM;
            return -1;
        }
        free_on_err = 1;
    }

    for (i=0; ; i++) {
        ch = fgetc(fp);
        while (i >= (*n) - 2) {
            *n *= 2;
            p = realloc(*lineptr, sizeof(char) * (*n));
            if (p == NULL) {
                if (free_on_err)
                    free(*lineptr);
                errno = ENOMEM;
                return -1;
            }
            *lineptr = p;
        }
        if (ch == EOF) {
            if (i == 0) {
                if (free_on_err)
                    free(*lineptr);
                return -1;
            }
            (*lineptr)[i] = '\0';
            *n = i;
            return i;
        }

        if (ch == '\n') {
            (*lineptr)[i] = '\n';
            (*lineptr)[i+1] = '\0';
            *n = i+1;
            return i+1;
        }
        (*lineptr)[i] = (char)ch;
    }
}

#endif

/*===========================================================================
                           GLOBAL FUNCTION DEFINITION
=============================================================================*/

/** logs error msg
 *
 * @par Purpose
 *
 * Appends error_msg to error_log.
 *
 * @par Operation
 *
 * @param[in] error_msg, Null terminated string for error messages,
 *
 * @retval None
 */
void log_error_msg(char* error_msg)
{
    size_t log_len = strlen(error_log);
    size_t input_len = strlen(error_msg);

    if (log_len + input_len > MAX_ERROR_STR_LEN)
        input_len = MAX_ERROR_STR_LEN - log_len;

    strncat(error_log, error_msg, input_len);
}

/** logs argument name and command name given their types
 *
 * @par Purpose
 *
 * The function generates string of type "arg-name msg in command cmd-name",
 * it can be called for generating text for invalid argument error string and
 * for other error types.
 * Ex strings that can be generated:
 * 1. log_arg_cmd(Engine, NULL, AuthenticateData) can be called for
 * ERROR_INVALID_ARGUMENT and output text looks like "Invalid argument Engine
 * in command AuthenticateData"
 * 2. log_arg_cmd(Engine, STR_ENG_ANY_CFG_NOT_ZERO, AuthenticateData) can be
 * called for ERROR_INVALID_ARGUMENT and output text looks like "Invalid
 * argument Engine is ANY but configuration not 0 in command AuthenticateData"
 *
 * @par Operation
 *
 * @param[in] arg, expects argument type,
 *
 * @param[in] msg, if not NULL then it will be included after argument name
 *
 * @param[in] cmd, expects command type,
 *
 * @retval None
 */
void log_arg_cmd(arguments_t arg, char * msg, commands_t cmd)
{
    uint32_t i;               /**< Loop counter */

    for (i=0; i<argument_count; i++)
    {
        if (argument_map[i].value == arg)
        {
            log_error_msg(argument_map[i].label);
        }
    }

    if (msg != NULL)
        log_error_msg(msg);

    log_error_msg(STR_IN_CMD);

    for (i=0; i<command_count; i++)
    {
        if (command_map[i].type == cmd)
        {
            log_error_msg(command_map[i].name);
        }
    }
}

/** logs command name with error msg
 *
 * @par Purpose
 *
 * The function generates string of type "cmd-name error msg",
 * it can be called for generating text for invalid command error strings
 * Ex strings that can be generated:
 * 1. log_cmd(CmdUnlock, STR_ILLEGAL) can be called for
 * ERROR_INVALID_COMMAND and output text looks like "Invalid command Unlock is
 * illegal for given HAB version"
 *
 * @par Operation
 *
 * @param[in] cmd, expects command type,
 *
 * @param[in] msg, if not NULL then it will be included after command name
 *
 * @retval None
 */
void log_cmd(commands_t cmd, char * msg)
{
    uint32_t i;               /**< Loop counter */

    for (i=0; i<command_count; i++)
    {
        if (command_map[i].type == cmd)
        {
            log_error_msg(command_map[i].name);
        }
    }

    if (msg != NULL)
        log_error_msg(msg);
}

/** set label
 *
 * @par Purpose
 *
 * Every keyword has a string (label) and an uint32_t associated with it.
 * The labels appear in CSF file and the parser will use this function to set
 * the unsigned_value associated with the label using map_label.
 *
 * @par Operation
 *
 * @param[in][out] keyword, type keyword_t [in] is string_value and [out] is
 *                         uint32_t value,
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval #ERROR_UNDEFINED_LABEL  if keyword does not match with entries in label_map
 */
int32_t set_label(keyword_t *keyword)
{
    uint32_t i;               /**< Loop counter */

    /* find the keyword in label_map */
    for (i=0; i<label_count; i++)
        if (!strcasecmp(keyword->string_value, label_map[i].label))
            break;

    if (i<label_count)
    {
        keyword->unsigned_value = label_map[i].value;
        return SUCCESS;
    }

    log_error_msg(keyword->string_value);
    return ERROR_UNDEFINED_LABEL;
}

/** append_uid_to_buffer
 *
 * @par Purpose
 *
 * Helpful function that converts list of number_t into array of uint8_t
 *
 * @par Operation
 *
 * @param[in] uid, list of bytes of type number_t
 *
 * @param[in] buf, buffer to append uid
 *
 * @param[in] bytes_written, bytes written in buf
 *
 * @retval #SUCCESS  completed its task successfully
 */
int32_t append_uid_to_buffer(number_t *uid, uint8_t *buf,
                             int32_t *bytes_written)
{
    int32_t i;       /**< Loop index */

    i=0;

    while(uid != NULL)
    {
        buf[i] = uid->num_value;
        uid = uid->next;
        i++;
    }

    *bytes_written = i;
    return SUCCESS;
}

/** set argument id
 *
 * @par Purpose
 *
 * Every argument has a name and an Id associated with it.
 * The argument name appear in CSF file and the parser will use this function
 * to set the type in the given arg using argument_map.
 *
 * @par Operation
 *
 * @param[in][out] arg, type argument_t [in] is name and [out] is type
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval #ERROR_INVALID_ARGUMENT  if argument does not match with entries in argument_map
 */
int32_t set_argument_type(argument_t *arg)
{
    uint32_t i;               /**< Loop counter */

    for (i=0; i<argument_count; i++)
        if (!strcasecmp(arg->name, argument_map[i].label))
            break;

    if (i<argument_count)
    {
        arg->type = argument_map[i].value;
        return SUCCESS;
    }

    log_error_msg(arg->name);
    return ERROR_INVALID_ARGUMENT;
}

/** check command sequence
 *
 * @par Purpose
 *
 * Called by handle command. This function checks that the commands found
 * in the CSF are used in the appropriate order and for the correct HAB version.
 * If there is a fault then it will return an error code.
 *
 * @par Operation
 *
 * @param[in] cmd, command type
 *
 * @retval SUCCESS  completed its task successfully
 *
 * @retval ERROR_CMD_IS_ALREADY_USED if a command is repeated illegally
 *
 * @retval ERROR_CMD_INSTALL_SRK_EXPECTED an Install SRK cmd is not found
 *         or is used illegally
 *
 * @retval ERROR_CMD_INSTALL_CSFK_EXPECTED and Install CSFK  is not found
 *         or is used illegally
 *
 * @retval ERROR_CMD_EXPECTED_AFTER_AUT_CSF an illegal command is used after
 *         the Autheticate CSF command
 *
 * @retval ERROR_CMD_INSTALL_KEY_EXPECTED a Install Key command is expected
 *         but was not found.
 */
static int32_t check_command_sequence(command_t *cmd)
{

    /* True for each commands : CmdHeader must be the first
     * to define g_target, g_hab_version and other global defaults.
     */
    if ((cmd->type != CmdHeader) &&
       (g_cmd_seq_stage == WAITING_FOR_HEADER_STATE))
    {
        return ERROR_CMD_HEADER_NOT_FIRST;
    }

    if (TGT_AHAB == g_target)
    {
        /* If AHAB is targeted, no specific order is expected (except header) */
        return SUCCESS;
    }

    switch (cmd->type)
    {
    case CmdHeader:
    /* Need to check header command appears only once. */
        if (g_cmd_seq_stage == WAITING_FOR_HEADER_STATE)
        {
            g_cmd_seq_stage = INSTALL_SRK_STATE;
        }
        else
        {
            return ERROR_CMD_IS_ALREADY_USED;
        }
        break;
    case CmdInstallSRK:
    /* Check that next command is CmdInstallSRK */
        if (g_cmd_seq_stage > INSTALL_SRK_STATE)
        {
            return ERROR_CMD_IS_ALREADY_USED;
        }
        else
        {
            g_cmd_seq_stage = INSTALL_CSFK_STATE;
        }
        break;
    case CmdInstallCSFK:
    case CmdInstallNOCAK:
    /* Check that next command is CmdInstallCSFK or CmdInstallNOCAK*/
        if (g_cmd_seq_stage > INSTALL_CSFK_STATE)
        {
            return ERROR_CMD_IS_ALREADY_USED;
        }
        else if (g_cmd_seq_stage < INSTALL_CSFK_STATE)
        {
            return ERROR_CMD_INSTALL_SRK_EXPECTED;
        }
        else
        {
            g_cmd_seq_stage = AUTH_CSF_STATE;
        }
        break;
    case CmdAuthenticateCSF:
    /* Check that next command is CmdAuthenticateCSF */
        if (g_cmd_seq_stage > AUTH_CSF_STATE)
        {
            return ERROR_CMD_IS_ALREADY_USED;
        }
        else if (g_cmd_seq_stage < AUTH_CSF_STATE)
        {
            return ERROR_CMD_INSTALL_CSFK_EXPECTED;
        }
        else
        {
            g_cmd_seq_stage = AUTH_CSF_NO_KEY_STATE;
        }
        break;
    case CmdInstallKEY:
     /* Check that CmdInstallKey is used after CmdAuthenticateCSF.
     * This must be called before any CmdAuthenticateData.
     */
        if (g_cmd_seq_stage >= AUTH_CSF_NO_KEY_STATE)
        {
            if (g_cmd_seq_stage == AUTH_CSF_WITH_ENC_KEY_STATE)
            {
                g_cmd_seq_stage = AUTH_CSF_WITH_BOTH_KEY_STATE;
            }
            else
            {
                g_cmd_seq_stage = AUTH_CSF_WITH_APP_KEY_STATE;
            }
        }
        else
        {
            return ERROR_CMD_EXPECTED_AFTER_AUT_CSF;
        }
        break;
    case CmdAuthenticateData:
    /* Check that CmdAuthenticateData is used after CmdAuthenticateCSF.
     * This must be called after a CmdInstallKey or CmdInstallNOCAK.
     * Note that it does not ensure that the installed key is the one
     * that is going to be used by the authenticate data command !
     */
        if (g_cmd_seq_stage < AUTH_CSF_NO_KEY_STATE)
        {
            return ERROR_CMD_EXPECTED_AFTER_AUT_CSF;
        }
        else if ((g_no_ca == 0) &&
                 ((g_cmd_seq_stage == AUTH_CSF_NO_KEY_STATE) ||
                  (g_cmd_seq_stage == AUTH_CSF_WITH_ENC_KEY_STATE)))
        {
            return ERROR_CMD_INSTALL_KEY_EXPECTED;
        }
        break;
    case CmdInstallSecretKEY:
    /* Check that CmdInstallSecretKey is used after CmdAuthenticateCSF.
     * This must be called before any CmdDecryptData.
     */
        if (g_hab_version <= HAB4)
        {
            return ERROR_INVALID_COMMAND;
        }
        else if (g_cmd_seq_stage >= AUTH_CSF_NO_KEY_STATE)
        {
            if (g_cmd_seq_stage == AUTH_CSF_WITH_APP_KEY_STATE)
            {
                g_cmd_seq_stage = AUTH_CSF_WITH_BOTH_KEY_STATE;
            }
            else
            {
                g_cmd_seq_stage = AUTH_CSF_WITH_ENC_KEY_STATE;
            }

        }
        else
        {
            return ERROR_CMD_EXPECTED_AFTER_AUT_CSF;
        }
        break;
    case CmdDecryptData:
    /* Check that CmdDecryptData is used after CmdAuthenticateCSF.
     * This must be called after a CmdInstallSecretKey.
     */
        if (g_hab_version <= HAB4)
        {
            return ERROR_INVALID_COMMAND;
        }
        else if (g_cmd_seq_stage < AUTH_CSF_NO_KEY_STATE)
        {
            return ERROR_CMD_EXPECTED_AFTER_AUT_CSF;
        }
        else if ((g_cmd_seq_stage == AUTH_CSF_NO_KEY_STATE) ||
                 (g_cmd_seq_stage == AUTH_CSF_WITH_APP_KEY_STATE))
        {
            return ERROR_CMD_INSTALL_SECKEY_EXPECTED;
        }
        seed_prng(RNG_SEED_BYTES);
        break;
    case CmdNOP:
        break;
    case CmdInit:
    case CmdUnlock:
    /* Must be used after CmdAuthenticateCSF for HAB4.
     */
        if (g_cmd_seq_stage < AUTH_CSF_NO_KEY_STATE)
        {
            return ERROR_CMD_EXPECTED_AFTER_AUT_CSF;
        }
        break;
    case CmdSetEngine:
    /* These commands can be used at any time after CmdHeader for HAB4.
     */
        break;
    default:
        return ERROR_INVALID_COMMAND;
        break;
    }

    return SUCCESS;
}

/** handle command
 *
 * @par Purpose
 *
 * Called by csf parser after a command and its arguments are processed. This
 * function scan command_map looking for a match for command name. If found it
 * will call the handler function for that command. If no match then it will
 * return an error code.
 *
 * @par Operation
 *
 * @param[in] cmd, type command_t [in] is name and [out] is Id
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval ERROR_CMD_IS_ALREADY_USED if a command is repeated illegally
 *
 * @retval ERROR_CMD_INSTALL_SRK_EXPECTED an Install SRK cmd is not found
 *         or is used illegally
 *
 * @retval ERROR_CMD_INSTALL_CSFK_EXPECTED and Install CSFK  is not found
 *         or is used illegally
 *
 * @retval ERROR_CMD_EXPECTED_AFTER_AUT_CSF an illegal command is used after
 *         the Autheticate CSF command
 *
 * @retval ERROR_CMD_INSTALL_KEY_EXPECTED a Install Key command is expected
 *         but was not found.
 */
int32_t handle_command(command_t *cmd)
{
    uint32_t i;               /**< Loop counter */
    int32_t ret = ERROR_INVALID_COMMAND;

    for (i=0; i<command_count; i++)
    {
        if (!strcasecmp(cmd->name, command_map[i].name))
            break;
    }
    /* If found then set cmd type and call the handler */
    if (i<command_count)
    {
        cmd->type = command_map[i].type;

        /* check if the command is correctly placed in the sequence */
        ret = check_command_sequence(cmd);

        if (ret == SUCCESS)
        {
            return (command_map[i].handler(cmd));
        }
    }

    log_error_msg(cmd->name);
    return ret;
}

/** hab_hash_alg_to_hash_alg_type
 *
 * @par Purpose
 *
 * Converts hab hash algorithm to hash_alg_t enum. hash_alg_t is defined in
 * adapt_layer.h.
 *
 * @par Operation
 *
 * @param[in] hash, hab hash algorithm macro
 *
 * @retval equivalent hash_alg_t
 */
hash_alg_t hab_hash_alg_to_hash_alg_type(int32_t hash)
{
    hash_alg_t hash_alg;           /**< For returning hash algorithm */

    hash_alg = SHA_256;

    switch(hash)
    {
    case HAB_ALG_SHA1:
        hash_alg = SHA_1;
        break;
    case HAB_ALG_SHA512:
        hash_alg = SHA_512;
        break;
    case HAB_ALG_SHA256:
    default:
        hash_alg = SHA_256;
        break;
    };

    return hash_alg;
}

/** hab_hash_alg_to_digest_name
 *
 * @par Purpose
 *
 * Converts hab defined hash algorithms to one of "sha1", "sha256" or "sha512"
 * character strings.
 *
 * @par Operation
 *
 * @param[in] hash_alg,  One of hab's hash algorithm definition
 *
 * @retval returns ptr to char for digest name
 */
char* hab_hash_alg_to_digest_name(int32_t hash_alg)
{
    char * hash_name = NULL; /**< Ptr to return address of digest string */

    switch(hab_hash_alg_to_hash_alg_type(hash_alg))
    {
    case SHA_1:
        hash_name = HASH_ALG_SHA1;
        break;
    case SHA_256:
        hash_name = HASH_ALG_SHA256;
        break;
    case SHA_512:
        hash_name = HASH_ALG_SHA512;
        break;
    default:
        hash_name = HASH_ALG_INVALID;
        break;
    };

    return hash_name;
}

/** Returns HAB4 engine config value
 *
 * @par Purpose
 *
 * Takes in eng cfg of type eng_cfg_t and engine and return one of HAB4
 * defines for engine configuration.
 *
 * @par Operation
 *
 * @param[in] engine, engine
 *
 * @param[in] eng_cfg, configuration for engine
 *
 * @retval one of HAB4 defined engine configurations, on all errors it
 *           returns ERROR_INVALID_ENGINE_CFG or ERROR_INVALID_ENGINE.
 */
int32_t get_hab4_engine_config(const int32_t engine, const eng_cfg_t eng_cfg)
{
    int32_t config = 0;

    switch (engine)
    {
    case HAB_ENG_SAHARA:
        switch (eng_cfg)
        {
        case ENG_CFG_IN_SWAP_8:
            config = HAB_SAHARA_IN_SWAP8;
            break;
        case ENG_CFG_IN_SWAP_16:
            config = HAB_SAHARA_IN_SWAP16;
            break;
        case ENG_CFG_DSC_BE_8_16:
            config = HAB_SAHARA_DSC_BE8_16;
            break;
        case ENG_CFG_DSC_BE_8_32:
            config = HAB_SAHARA_DSC_BE8_32;
            break;
        case ENG_CFG_DEFAULT:
            config = 0;
            break;
        default:
            config = ERROR_INVALID_ENGINE_CFG;
            break;
        };
        break;
    case HAB_ENG_CAAM:
        switch (eng_cfg)
        {
        case ENG_CFG_IN_SWAP_8:
            config = HAB_CAAM_IN_SWAP8;
            break;
        case ENG_CFG_IN_SWAP_16:
            config = HAB_CAAM_IN_SWAP16;
            break;
        case ENG_CFG_OUT_SWAP_8:
            config = HAB_CAAM_OUT_SWAP8;
            break;
        case ENG_CFG_OUT_SWAP_16:
            config = HAB_CAAM_OUT_SWAP16;
            break;
        case ENG_CFG_DSC_SWAP_8:
            config = HAB_CAAM_DSC_SWAP8;
            break;
        case ENG_CFG_DSC_SWAP_16:
            config = HAB_CAAM_DSC_SWAP16;
            break;
        case ENG_CFG_DEFAULT:
            config = 0;
            break;
        default:
            config = ERROR_INVALID_ENGINE_CFG;
            break;
        };
        break;
    case HAB_ENG_DCP:
        switch (eng_cfg)
        {
        case ENG_CFG_IN_SWAP_8:
            config = HAB_DCP_IN_SWAP8;
            break;
        case ENG_CFG_IN_SWAP_32:
            config = HAB_DCP_IN_SWAP32;
            break;
        case ENG_CFG_OUT_SWAP_8:
            config = HAB_DCP_OUT_SWAP8;
            break;
        case ENG_CFG_OUT_SWAP_32:
            config = HAB_DCP_OUT_SWAP32;
            break;
        case ENG_CFG_DEFAULT:
            config = 0;
            break;
        default:
            config = ERROR_INVALID_ENGINE_CFG;
            break;
        };
        break;
    case HAB_ENG_RTIC:
        switch (eng_cfg)
        {
        case ENG_CFG_IN_SWAP_8:
            config = HAB_RTIC_IN_SWAP8;
            break;
        case ENG_CFG_IN_SWAP_16:
            config = HAB_RTIC_IN_SWAP16;
            break;
        case ENG_CFG_OUT_SWAP_8:
            config = HAB_RTIC_OUT_SWAP8;
            break;
        case ENG_CFG_KEEP:
            config = HAB_RTIC_KEEP;
            break;
        case ENG_CFG_DEFAULT:
            config = 0;
            break;
        default:
            config = ERROR_INVALID_ENGINE_CFG;
            break;
        };
        break;
    case HAB_ENG_SW:
    case HAB_ENG_ANY:
        config = 0;
        break;
    default:
        config = ERROR_INVALID_ENGINE;
        break;
    };

    return config;
}

/** save data file
 *
 * @par Purpose
 *
 * Allocates a buffer to read data from input file. The pointer to buffer is
 * stored in cert_sig_data and size of buffer in size_cert_sig of cmd. Later
 * the data will be appended to output binary csf file. The function also
 * returns hash bytes of data if crt_hash is not NULL.
 *
 * @par Operation
 *
 * @param[in] cmd, csf command where the file data (certificate or signature)
 *            pointer and size are saved
 * @param[in] file, cert or signature file name, NULL if data is provided.
 *
 * @param[in] cert_data, if file is NULL the data ptr for sig, cert is
 *            provided and function will save the pointer.
 *
 * @param[in] cert_len, length of cert_data in bytes.
 *
 * @param[in] add_header, if 1 then it add 4 header bytes at the beginning
 *            of cert/sig data in the buffer.
 *
 * @param[out] crt_hash, if not NULL then function returns pointer to hash
 *             data, caller is responsible for freeing it.
 *
 * @param[out] hash_len, pointer to return hash_len, if crt_hash is not null
 *             then hash_len cannot be null.
 *
 * @param[in] hash_alg, hash algorithm to use in generating hash
 *
 * @retval #SUCCESS if everything goes fine
 *
 * @retval #ERROR_FILE_NOT_PRESENT if file is not present
 *
 * @retval #ERROR_READING_FILE, fread returns incorrect number of bytes read
 *
 * @retval #ERROR_CALCULATING_HASH, error in function get_hash
 */
int32_t save_file_data(command_t *cmd, char *file, uint8_t *cert_data,
                       size_t cert_len, int32_t add_header, uint8_t **crt_hash,
                       size_t *hash_len, int32_t hash_alg)
{
    uint32_t file_size;         /**< File size is read into it, useful in
                                    calulating length field of cmd header */
    int32_t ret_val = SUCCESS; /**< To return and keep track of errors */
    uint8_t *data = NULL;      /**< Ptr to buffer for reading in file data */
    int32_t header_bytes;      /**< Needed for the size of header */
    int32_t data_size;         /**< For total size of certificate or
                                    signature */
    uint8_t hdr_tag;           /**< Used in HDR macro, set to either CRT or
                                    SIG */
    FILE *fh = NULL;           /**< Used for file handle */

    hdr_tag = (cmd->type == CmdAuthenticateCSF ||
        cmd->type == CmdAuthenticateData) ? HAB_TAG_SIG : HAB_TAG_CRT;

    if (cmd->type == CmdDecryptData)
    {
        hdr_tag = HAB_TAG_MAC;
    }
    /* header_bytes will be either 0 or HDR_BYTES */
    header_bytes = (add_header == 0) ? 0 : HDR_BYTES;

    data_size = header_bytes;

    if (file == NULL && cert_data == NULL)
    {
        /* Nothing to do, at least one of file/cert_data should be valid
         * to continue. Control should never come here as this function
         * is not an API and only called by functions from frontend only.
         * We can return SUCCESS as it does not matter.
         */
        return SUCCESS;
    }
    if (file)
    {
        fh = fopen(file, "rb");
        if (fh == NULL)
        {
            log_error_msg(file);
            return ERROR_FILE_NOT_PRESENT;
        }

        /* Get the size of the file */
        fseek(fh, 0, SEEK_END);
        file_size = ftell(fh);
        rewind(fh);
    }
    else
    {
        file_size = cert_len;
    }

    /**
     * data_size now have either 0 or HDR_BYTES, add file_size to it.
     * This will be the size of data we need to allocate.
     */

    if (g_hab_version >= HAB4)
    {
        /**
         * With HAB4, each sig and cert has to start at a word boundary due
         * to a constraint in hab library. By allocating size in multiple of
         * 4 we will make sure the next sig/cert will start on word boundary.
         * The first sig/cert will always start on word boundary as csf
         * commands are full word sizes.
         */
        data_size += WORD_ALIGN(file_size);
    }

    data = malloc(data_size);
    do {
        if (data == NULL)
        {
            ret_val =  ERROR_INSUFFICIENT_MEMORY;
            break;
        }

        /* Read from file into data buffer at offset header_bytes */
        memset(data, 0, data_size);
        if (file)
        {
            if (fread(data+header_bytes, 1, file_size, fh) != file_size)
            {
                log_error_msg(file);
                ret_val = ERROR_READING_FILE;
                break;
            }
        }
        else
        {
            memcpy(data+header_bytes, cert_data, cert_len);
        }
        /* Insert header bytes if required */
        if (header_bytes != 0)
        {
            uint8_t hdr[] = {
                HDR(hdr_tag, (file_size + header_bytes), g_hab_version)
            };       /**< Macro will output header bytes into hdr buffer */

            memcpy(data, hdr, header_bytes);
        }

        /* save the data ptr and size in cmd */
        cmd->cert_sig_data = data;
        cmd->size_cert_sig = data_size;

        /* Generate hash if asked for and return hash data ptr in crt_hash */
        if (crt_hash != NULL && hash_len != NULL)
        {
            *crt_hash = generate_hash(data, (file_size + header_bytes),
                hab_hash_alg_to_digest_name(hash_alg), hash_len);

            if (crt_hash == NULL)
            {
                if (file)
                    log_error_msg(file);
                else
                    log_error_msg(STR_CERTIFICATE);

                ret_val = ERROR_CALCULATING_HASH;
                break;
            }
        }
    } while(0);

    if (fh)
        fclose(fh);

    if (ret_val != SUCCESS && data)
        free(data);
    return ret_val;
}

/** creates signature file
 *
 * @par Purpose
 *
 * Function calls adapt_layer api to generate signature data for the given
 * data, signature format, signature algorithm and certificate. The
 * generated signature data is saved into the file.
 *
 * @par Operation
 *
 * @param[in] file, filename for the signature
 *
 * @param[in] cert_file, certificate file of signing key.
 *
 * @param[in] sig_fmt, signature format of type sig_fmt_t defined in
 *            adapt_layer.h
 *
 * @param[in] data, data to sign
 *
 * @param[in] data_size, size of input data
 *
 * @retval #SUCCESS if everything goes fine
 *
 * @retval #ERROR_OPENING_FILE, fopen returns NULL
 *
 * @retval #ERROR_READING_FILE, fread returns incorrect number of bytes read
 *
 * @retval #ERROR_CALCULATING_HASH, error in function get_hash
 */
int32_t create_sig_file(char *file, char *cert_file,
        sig_fmt_t sig_fmt, uint8_t *data,
        uint32_t data_size)
{
    uint8_t sig[SIGNATURE_BUFFER_SIZE];  /**< Signature buffer on stack */
    hash_alg_t hash;    /**< Hash algorithm to pass into adaptation layer API */
    int32_t ret_val = SUCCESS; /**< Return and keep track of error status */
    FILE *fh = NULL;           /**< File pointer */

    hash = hab_hash_alg_to_hash_alg_type(g_hash_alg);
    /**
     * sig_size as input to gen_sig_data shows the size of buffer for
     * signature data and gen_sig_data returns actual size of signature
     * data in this argument
     */
    uint32_t sig_size = SIGNATURE_BUFFER_SIZE;

    do {
        /**
         * Create the binary file for storing data to sign. This file
         * will be an input to gen_sig_data API
         */
        fh = fopen(file, "wb");
        if (fh == NULL)
        {
            log_error_msg(file);
            ret_val = ERROR_OPENING_FILE;
            break;
        }

        if (fwrite(data, 1, data_size, fh) !=
            data_size)
        {
            log_error_msg(file);
            ret_val = ERROR_WRITING_FILE;
            break;
        }

        fclose(fh);

        /**
         * Calling gen_sig_data to generate signature for data in file using
         * certificate in cert_file. The signature data will be returned in
         * sig and size of signature data in sig_size
         */
        ret_val = gen_sig_data(file, cert_file, hash, sig_fmt,
            sig, (size_t *)&sig_size, g_mode);
        if (ret_val != SUCCESS)
        {
            log_error_msg(STR_ERR_SIG_GEN);
            log_error_msg(file);
            log_error_msg(STR_ERR_USING_CERT);
            log_error_msg(cert_file);
            break;
        }

        /**
         * We use the same file to store signature data, opening it again
         * in write mode will re-create the file.
         */
        fh = fopen(file, "wb");
        if (fh == NULL)
        {
            log_error_msg(file);
            ret_val = ERROR_OPENING_FILE;
            break;
        }
        if (fwrite(sig, 1, sig_size, fh) != sig_size)
        {
            log_error_msg(file);
            ret_val = ERROR_WRITING_FILE;
            break;
        }
        fclose(fh);
    } while(0);

    return ret_val;
}

/*===========================================================================
                           LOCAL FUNCTION DEFINITION
=============================================================================*/
/** update_offsets_in_csf
 *
 * @par Purpose
 *
 * This function is used in the second pass to update csf buffer with offsets
 * to signatures and certificates and csf length in header.
 *
 * @par Operation
 *
 * @param[in] buf,  pointer to csf data
 *
 * @param[in] cmd_csf,  pointer to command for authenticate csf
 *
 * @param[in] csf_len,  length of csf data
 *
 * @retval #SUCCESS if everything goes fine
 */
static int update_offsets_in_csf(uint8_t * buf, command_t * cmd_csf, uint32_t csf_len)
{
    int32_t cert_sig_offset = 0;    /**< Used to keep track of certificate or
                                         Signature offsets in the cmd */
    command_t * cmd;                /**< Ptr to command_t, used in the loop to
                                       go through cmd list and update offsets
                                       to certificate and signature data */

    /* Update offsets in csf, set header.length */
    if (g_hab_version >= HAB4)
    {
        buf[CSF_HDR_LENGTH_OFFSET] = ((csf_len & 0xFF00) >> 8);
        buf[CSF_HDR_LENGTH_OFFSET+1] = (csf_len & 0xFF);
    }

    /* Set sig/cert offsets in csf_buffer */
    cmd = g_cmd_head;
    cert_sig_offset = csf_len;
    while(cmd != NULL)
    {
        if (cmd->start_offset_cert_sig > 0)
        {
            uint8_t offset_bytes[] = {
                EXPAND_UINT32(cert_sig_offset)
            };         /**< Macro puts offets into the buffer */

            memcpy(&buf[cmd->start_offset_cert_sig], offset_bytes, 4);
        }

        cert_sig_offset += cmd->size_cert_sig;
        cmd = cmd->next;
    }
    /* done with update offsets in csf */

    return SUCCESS;
}

/** prints the command line usage
 *
 * Prints the usage information for running cst. It also shows
 * examples of command line parameters to cst
 *
 * @pre  This function is called from process_cmdline_args() if failed to
 *       process command line successfully.
 *
 * @post The usage info will be printed out on console window.
 */
static void print_usage(void)
{
    printf("Usage: \n\n");
    printf("To generate output binary CSF using Code Signing Tool \n");
    printf("===================================================== \n\n");
    printf("cst --output <bin_csf> --input <input_csf> \n\n");
    printf("-o, --output <binary csf>:\n");
    printf("    Output binary CSF filename\n\n");
    printf("-i, --input <csf text file>:\n");
    printf("    Input CSF text filename\n\n");
    printf("-c, --cert <public key certificate>:\n");
    printf("    Optional, Input public key certificate to encrypt the dek\n\n");
    printf("-l, --license:\n");
    printf("    Optional, displays program license information.  No ");
    printf("additional\n    arguments are required\n\n");
    printf("-v, --version:\n");
    printf("    Optional, displays the version of the tool.  No additional\n");
    printf("    arguments are required\n\n");
    printf("-h, --help:\n");
    printf("    Optional, displays usage information.  No additional\n");
    printf("    arguments are required\n\n");
    printf("Examples:\n");
    printf("---------\n\n");
    printf("1. To generate out_csf.bin file from input hab4.csf, use\n");
    printf("    cst -o out_csf.bin -i hab4.csf \n\n");
    printf("2. To generate out_csf.bin file from input hab4.csf and");
    printf("      output a plaintext dek, use\n");
    printf("    cst -o out_csf.bin -i hab4.csf \n\n");
    printf("3. To generate out_csf.bin file from input hab4.csf and \n");
    printf("    encrypt the dek with cert.pem, use\n");
    printf("    cst -o out_csf.bin -c cert.pem -i hab4.csf \n\n");
    printf("4. To print program license information, use\n");
    printf("    cst --license \n\n");
}

/** Process command line arguments for code signing tool (cst)
 *
 * @par Purpose
 *
 * Process command line arguments for cst and calls respective
 * functions based no commands specified at command line.
 *
 * @param[in] argc, number of arguments in argv
 *
 * @param[in] argv, arguments list
 *
 * @param[out] out_bin_csf, ptr to return name for binary csf
 */
static void process_cmdline_args(int argc, char* argv[], char **out_bin_csf)
{
    int  next_option = 0;        /**< Option count from cmd line */

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
            /* Display usage */
            case 'h':
                print_usage();
                exit(0);
                break;
            /* Option o - output csf binary file */
            case 'o':
                *out_bin_csf = optarg;
                break;
            /* Option i - input csf text file */
            case 'i':
                g_in_csf_file = optarg;
                break;
            /* Option c - input public key cert used for
                  encrypting dek */
            case 'c':
                g_cert_dek = optarg;
                break;
            /* Option d - input data encryption key */
            case 'd':
                g_reuse_dek = 1;
                break;
            /* Option s - skip user prompt */
            case 's':
                g_skip = 1;
                break;
            case '?':
                print_usage();
                exit(1);
                break;
            default:    /* Something else: unexpected.  */
                break;
        }
    } while (next_option != -1);

    /* Check for minimum number of arguments */
    if (argc  < MIN_NUM_CLI_ARGS)
    {
        print_usage();
        exit(1);
    }
}

/** main function of cst application
 *
 * @par Purpose
 *
 * Main CST function.
 * Call parser to parse CSF file.
 * Generates CSF signature for data in csf_buffer.
 * Attached the csf signature buffer to csf cmd structure.
 * Creates output csf binary file with csf_buffer data, signatures and
 *  certificates.
 * Print out error messages on stdout.
 *
 * @par Operation
 *
 * @param[in] argc, number of arguments in argv
 *
 * @param[in] argv, csf output binary file to create should be passed in argv[1]
 */
int32_t main(int32_t argc, char* argv[])
{
    int32_t ret_val = SUCCESS;      /**< Used for keeping track of error
                                         values returned by functions */
    FILE *fo = NULL;                /**< File pointer for output csf binary */
    FILE *fi = NULL;                /**< File pointer for input csf text file */
    command_t *cmd = NULL;          /**< Ptr to command_t, used in the loop to
                                       go through cmd list and update offsets
                                       to certificate and signature data */

    command_t *cmd_csf = NULL;      /**< Ptr to save address of authenticate
                                         csf command */

    command_t *cmd_csfk = NULL;      /**< Ptr to save address of install csf
                                         key command */

    char *out_bin_csf = NULL;       /**< Ptr to filename for output binary csf,
                                         an argument passed to cst */

    command_t unlk_cmd;
    argument_t unlk_args[2];
    keyword_t unlk_keywords[2];

    memset(g_csf_buffer, 0, HAB_CSF_BYTES_MAX);

    memset(error_log, 0, MAX_ERROR_STR_LEN);

    openssl_initialize();
    process_cmdline_args(argc, argv, &out_bin_csf);

    if (g_reuse_dek)
    {
        prompt_key_reuse_msg();
    }

    if (out_bin_csf == NULL)
    {
        /* Can't proceed without output binary file name */
        printf("Missing --o argument\n");
        return 0;
    }

    if (g_in_csf_file == NULL)
    {
        printf("Missing --i argument\n");
        return 0;
    }

    /* Open CSF text file to be read by parser */
    fi = fopen(g_in_csf_file, "r");
    if (!fi)
    {
        printf("Unable to open %s", g_in_csf_file);
        return 0;
    }
    // set lex to read from file handler instead of defaulting to STDIN
    yyin = fi;
    if ((ret_val = yyparse()) == SUCCESS)
    {
        /*
         * If the header specified CAAM as the engine,
         * we add the Unlock RNG command by default. The
         * only reasons to not add it are:
         *  - It was already in CSF input file
         *  - CSF input file contains Init RNG command
         */
        if ((g_engine == HAB_ENG_CAAM) &&
            ((g_init_rng == 0) && (g_unlock_rng == 0)))
        {
            unlk_args[0].next = &unlk_args[1];
            unlk_args[0].name = "Engine";
            unlk_args[0].type = EngineName;
            unlk_args[0].value_count = 1;
            unlk_args[0].value_type = KEYWORD_TYPE;
            unlk_args[0].value.str = "";
            unlk_args[0].value.keyword = &unlk_keywords[0];
            unlk_keywords[0].next = NULL;
            unlk_keywords[0].string_value = "CAAM";
            unlk_keywords[0].unsigned_value = HAB_ENG_CAAM;

            unlk_args[1].next = NULL;
            unlk_args[1].name = "Features";
            unlk_args[1].type = Features;
            unlk_args[1].value_count = 1;
            unlk_args[1].value_type = KEYWORD_TYPE;
            unlk_args[1].value.str = "";
            unlk_args[1].value.keyword = &unlk_keywords[1];
            unlk_keywords[1].next = NULL;
            unlk_keywords[1].string_value = "RNG";
            unlk_keywords[1].unsigned_value = HAB_CAAM_UNLOCK_RNG;

            unlk_cmd.next = NULL;
            unlk_cmd.name = "Unlock";
            unlk_cmd.type = CmdUnlock;
            unlk_cmd.argument_count = 2;
            unlk_cmd.argument = &unlk_args[0];
            unlk_cmd.cert_sig_data = NULL;
            unlk_cmd.size_cert_sig = 0;
            unlk_cmd.start_offset_cert_sig = 0;

            cmd_handler_unlock(&unlk_cmd);

            /* Add unlock command to end of command list */
            cmd = g_cmd_head;
            while (cmd->next != NULL) {
                cmd = cmd->next;
            }
            cmd->next = &unlk_cmd;
        }

        if (TGT_AHAB == g_target)
        {
            g_ahab_data.destination = out_bin_csf;
            return handle_ahab_signature();
        }

        /* Parsing completed successfully, generate signature data for csf
            and replace the dummy one with it
        */
        do {

            cmd = g_cmd_head;

            while(cmd != NULL)
            {
                if (cmd->type == CmdAuthenticateCSF)
                {
                    cmd_csf = cmd;
                }
                else if (cmd->type == CmdInstallCSFK)
                {
                    cmd_csfk = cmd;
                }
                else if (cmd->type == CmdInstallNOCAK)
                {
                    cmd_csfk = cmd;
                }
                cmd = cmd->next;
            }
            if (cmd_csf == NULL)
            {
                ret_val = ERROR_AUT_CSF_CMD_NOT_FOUND;
                break;
            }
            if (cmd_csfk == NULL)
            {
                ret_val = ERROR_INS_CSFK_CMD_NOT_FOUND;
                break;
            }

            update_offsets_in_csf(g_csf_buffer, cmd_csf, g_csf_buffer_index);

            /* create signature for csf data into FILE_SIG_CSF_DATA */
            ret_val = create_sig_file(FILE_SIG_CSF_DATA,
                g_key_certs[HAB_IDX_CSFK],
                (g_hab_version >= HAB4) ? SIG_FMT_CMS : SIG_FMT_PKCS1,
                g_csf_buffer,
                g_csf_buffer_index);

            if (ret_val != SUCCESS)
            {
                break;
            }

            ret_val = save_file_data(cmd_csf, FILE_SIG_CSF_DATA, NULL, 0,
                (g_hab_version >= HAB4), NULL, NULL, g_hash_alg);
            if (ret_val != SUCCESS)
            {
                break;
            }

            fo = fopen(out_bin_csf, "wb");
            if (fo == NULL )
            {
                log_error_msg(out_bin_csf);
                ret_val = ERROR_OPENING_FILE;
                break;
            }

            /* write g_csf_buffer to output file */
            if (fwrite(g_csf_buffer, 1, g_csf_buffer_index, fo) !=
                g_csf_buffer_index)
            {
                log_error_msg(out_bin_csf);
                ret_val = ERROR_WRITING_FILE;
                break;
            }

            /* append sigs & certs to output file */
            if (g_hab_version >= HAB4)
            {
                cmd = g_cmd_head;
                while(cmd != NULL)
                {
                    if (cmd->cert_sig_data == NULL)
                    {
                        cmd = cmd->next;
                        continue;
                    }
                    if (fwrite(cmd->cert_sig_data, 1, cmd->size_cert_sig, fo) !=
                            cmd->size_cert_sig)
                    {
                        log_error_msg(out_bin_csf);
                        ret_val = ERROR_WRITING_FILE;

                        break;
                    }
                    cmd = cmd->next;
                }
            }
            /* Reached here means everything work good and csf data generated
             * in file out_bin_csf, log the name for later use */
            log_error_msg(out_bin_csf);

        } while(0);
    }

    if (fo)
        fclose(fo);
    if (fi)
        fclose(fi);

    remove(FILE_SIG_CSF_DATA);
    remove(FILE_SIG_IMG_DATA);

    remove(FILE_PLAIN_DATA);
    remove(FILE_ENCRYPTED_DATA);

    fflush(NULL);

    if (g_error_code != SUCCESS)
    {
        print_error_msg(g_error_code);
    }
    else
    {
        print_error_msg(ret_val);
    }

    /* Return a non-zero value on error otherwise 0 */
    if (g_error_code || ret_val)
    {
        return CST_FAILURE_EXIT_CODE;
    }
    return SUCCESS;
}

/** Prints error msg
 *
 * @par Purpose
 *
 * Prints error message uinsg passed error_code and error_log.
 *
 * @par Operation
 *
 * @param[in] error_code, one of error codes defined in csf.h file
 *
 * @retval None
 */
static void print_error_msg(const int32_t error_code)
{
    switch (error_code)
    {
    case SUCCESS:
        printf("CSF Processed successfully and signed data available in %s\n", error_log);
        break;
    case ERROR_INVALID_ARGUMENT:
        printf("Invalid argument: %s\n", error_log);
        break;
    case ERROR_INSUFFICIENT_ARGUMENTS:
        printf("Missing mandatory argument %s\n", error_log);
        break;
    case ERROR_INVALID_COMMAND:
        printf("Invalid command: %s\n", error_log);
        break;
    case ERROR_FILE_NOT_PRESENT:
        printf("File not present %s\n", error_log);
        break;
    case ERROR_OPENING_FILE:
        printf("Failed opening file %s\n", error_log);
        break;
    case ERROR_READING_FILE:
        printf("Failed reading file %s\n", error_log);
        break;
    case ERROR_WRITING_FILE:
        printf("Failed writing file %s\n", error_log);
        break;
    case ERROR_CALCULATING_HASH:
        printf("Error calculating hash for %s\n", error_log);
        break;
    case ERROR_INSUFFICIENT_MEMORY:
        printf("Memory overrun, failed to allocated enough memory %s\n", error_log);
        break;
    case ERROR_INVALID_BLOCK_ARGUMENTS:
        printf("Invalid Block arguments, %s\n", error_log);
        break;
    case ERROR_CMD_HEADER_NOT_FIRST:
        printf("Error in CSF: command [Header] must be the first, before" \
               " a command [%s]\n", error_log);
        break;
    case ERROR_UNSUPPORTED_ARGUMENT:
        printf("Unsupported argument: %s\n", error_log);
        break;
    case ERROR_UNDEFINED_LABEL:
        printf("Undefined label: %s\n", error_log);
        break;
    case ERROR_AUT_CSF_CMD_NOT_FOUND:
        printf("Failed to find authenticate CSF command %s\n", error_log);
        break;
    case ERROR_INS_CSFK_CMD_NOT_FOUND:
        printf("Failed to find install csf key command %s\n", error_log);
        break;
    case ERROR_INVALID_SRK_TABLE:
        printf("Super Root Key table is invalid in file %s\n", error_log);
        break;
    case ERROR_INVALID_PKEY_CERTIFICATE:
        printf("Public key certificate is invalid in file %s\n", error_log);
        break;
    case ERROR_CMD_IS_ALREADY_USED:
        printf("Error in CSF: multiple instances of command [%s] cannot " \
               "be used\n", error_log);
        break;
    case ERROR_CMD_INSTALL_SRK_EXPECTED:
        printf("Error in CSF: command [%s] misplaced, the command " \
               "[Install SRK] is expected after [Header]\n", error_log);
        break;
    case ERROR_CMD_INSTALL_CSFK_EXPECTED:
        printf("Error in CSF: command [%s] misplaced, the command " \
               "[Install CSFK] is expected after [Install SRK]\n", error_log);
        break;
    case ERROR_CMD_EXPECTED_AFTER_AUT_CSF:
        printf("Error in CSF: command [%s] expected after [Authenticate CSF] " \
               "which is expected after [Install CSFK]\n", error_log);
        break;
    case ERROR_CMD_INSTALL_KEY_EXPECTED:
        printf("Error in CSF: a command [Install key] is expected prior to " \
               "the usage of the key by an [Authenticate data] command\n");
        break;
    case ERROR_CMD_INSTALL_SECKEY_EXPECTED:
        printf("Error in CSF: a command [Install Secret key] is expected prior to " \
               "the usage of the key by a [Decrypt data] command\n");
        break;
    case ERROR_GENERATING_RANDOM_KEY:
        printf("An error reported by backend in generating random key %s\n", error_log);
        break;
    case ERROR_IN_ENCRYPTION:
        printf("An error is reported by backend during encryption %s\n", error_log);
        break;
    default:
        printf("Undefined error\n");
    }
}
/** Prompt user agreement for key reuse
 *
 * @par Purpose
 *
 * Prompt user agreement for resusing a data encryption key
 *
 * @par Operation
 *
 * @retval None
 */
static void prompt_key_reuse_msg(void )
{
    printf("NOTICE: Key-reuse for encryption undermines security of otherwise"
           " trustworthy systems.\nEncrypting multiple data sets with the"
           " same key, leaks enough information for an attacker to decrypt"
           " any data set without knowing the encryption key.\n"
           "The key-reuse feature is only meant to be used in development"
           " and not production. ");
    if (!g_skip)
    {
        char *buffer = malloc(sizeof(char) * 32);
        size_t bufsize = 32;
        size_t chars_read = 0;
        printf("\n\nBy using this option, you understand the risk"
                   " and assume full responsability.");
        do {
            printf("\nDo want to continue? (y/n)");
            chars_read = getline(&buffer, &bufsize, stdin);
        } while (!((buffer[0] == 'Y' || buffer[0] == 'y') && chars_read == 2));
    }
}

