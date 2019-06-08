#ifndef __CSF_H
#define __CSF_H
/*===========================================================================*/
/**
    @file    csf.h

    @brief   CST CSF macros, typedefs, function declarations

@verbatim
=============================================================================

              Freescale Semiconductor
        (c) Freescale Semiconductor, Inc. 2011-2015 All rights reserved.
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
                                INCLUDES
=============================================================================*/
#include "adapt_layer.h"
#include "arch_types.h"

/*===========================================================================
                                MACROS
=============================================================================*/
/* Error codes */
#define SUCCESS                          (0)
#define ERROR_INVALID_ARGUMENT          (CAL_LAST_ERROR - 1)
#define ERROR_INSUFFICIENT_ARGUMENTS    (CAL_LAST_ERROR - 2)
#define ERROR_INVALID_COMMAND           (CAL_LAST_ERROR - 3)
#define ERROR_FILE_NOT_PRESENT          (CAL_LAST_ERROR - 4)
#define ERROR_OPENING_FILE              (CAL_LAST_ERROR - 5)
#define ERROR_READING_FILE              (CAL_LAST_ERROR - 6)
#define ERROR_WRITING_FILE              (CAL_LAST_ERROR - 7)
#define ERROR_CALCULATING_HASH          (CAL_LAST_ERROR - 8)
#define ERROR_INSUFFICIENT_MEMORY       (CAL_LAST_ERROR - 9)
#define ERROR_INVALID_BLOCK_ARGUMENTS   (CAL_LAST_ERROR - 10)
#define ERROR_CMD_HEADER_NOT_FIRST      (CAL_LAST_ERROR - 11)
#define ERROR_UNSUPPORTED_ARGUMENT      (CAL_LAST_ERROR - 12)
#define ERROR_UNDEFINED_LABEL           (CAL_LAST_ERROR - 13)
#define ERROR_AUT_CSF_CMD_NOT_FOUND     (CAL_LAST_ERROR - 14)
#define ERROR_INS_CSFK_CMD_NOT_FOUND    (CAL_LAST_ERROR - 15)
#define ERROR_INVALID_SRK_TABLE         (CAL_LAST_ERROR - 16)
#define ERROR_INVALID_PKEY_CERTIFICATE  (CAL_LAST_ERROR - 17)
#define ERROR_INVALID_HASH_ALG          (CAL_LAST_ERROR - 18)
#define ERROR_INVALID_ENGINE            (CAL_LAST_ERROR - 19)
#define ERROR_INVALID_ENGINE_CFG        (CAL_LAST_ERROR - 20)
#define ERROR_CMD_IS_ALREADY_USED       (CAL_LAST_ERROR - 21)
#define ERROR_CMD_INSTALL_SRK_EXPECTED  (CAL_LAST_ERROR - 22)
#define ERROR_CMD_INSTALL_CSFK_EXPECTED (CAL_LAST_ERROR - 23)
#define ERROR_CMD_EXPECTED_AFTER_AUT_CSF (CAL_LAST_ERROR - 24)
#define ERROR_CMD_INSTALL_KEY_EXPECTED  (CAL_LAST_ERROR - 25)
#define ERROR_GENERATING_RANDOM_KEY     (CAL_LAST_ERROR - 26)
#define ERROR_IN_ENCRYPTION             (CAL_LAST_ERROR - 27)
#define ERROR_CMD_INSTALL_SECKEY_EXPECTED (CAL_LAST_ERROR - 28)

/* Strings used in generating error messages */
#define STR_IN_CMD (" in command ")
#define STR_ENG_ANY_CFG_NOT_ZERO (" is ANY but configuration not 0")
#define STR_EXCEED_MAX (" exceed max allowed")
#define STR_ILLEGAL " is illegal for given target"
#define STR_GREATER_THAN_255 (" greater than 255")
#define STR_BLKS_INVALID_LENGTH (" start offset and length together exceed file size")
#define STR_ERR_SIG_GEN ("Error in generating signature for ")
#define STR_ERR_USING_CERT (" using certificate ")
#define STR_CERTIFICATE (" Certificate")

/* Temporary files created during csf processing */
#define FILE_SIG_CSF_DATA ("csfsig.bin")
#define FILE_SIG_IMG_DATA ("imgsig.bin")

#define FILE_PLAIN_DATA   ("rawbytes.bin")
#define FILE_ENCRYPTED_DATA  ("encbytes.bin")

/* HAB4 macros */
#define HAB4 (0x40)

/* Length offset in header for HAB4 */
#define CSF_HDR_LENGTH_OFFSET (1)

/* Max size of buffer to allocate for csf cmds */
#define HAB_CSF_BYTES_MAX (768)

/**< Max. nonce bytes in 16B AES blk */
#define MAX_NONCE_BYTES             (13)

/*===========================================================================
                              TYPEDEFS
=============================================================================*/
/* Arguments type, used to identify arguments */
typedef enum arguments {
    Version,
    UID,
    HashAlgorithm,
    EngineName,
    EngineConfiguration,
    CertificateFormat,
    SignatureFormat,
    Filename,
    SourceIndex,
    VerificationIndex,
    TargetIndex,
    Blocks,
    Width,
    AddressData,
    Count,
    AddressMask,
    Bank,
    Row,
    Fuse,
    Bits,
    Features,
    BlobAddress,
    Key,
    KeyLength,
    MacBytes,
    Target,
    Source,
    Permissions,
    Offsets,
    Mode,
    Signature,
    SourceSet,
    Revocations,
    KeyIdentifier,
    ImageIndexes,
} arguments_t;

/* Commands type, used to identify arguments */
typedef enum commands {
    CmdHeader,
    CmdInstallSRK,
    CmdInstallCSFK,
    CmdInstallNOCAK,
    CmdAuthenticateCSF,
    CmdInstallKEY,
    CmdAuthenticateData,
    CmdInstallSecretKEY,
    CmdDecryptData,
    CmdWriteData,
    CmdClearMask,
    CmdSetMask,
    CmdCheckAllClear,
    CmdCheckAllSet,
    CmdCheckAnyClear,
    CmdCheckAnySet,
    CmdNOP,
    CmdSetMid,
    CmdSetEngine,
    CmdInit,
    CmdUnlock,
    CmdInstallCert,
} commands_t;

/* Enum for engine configurations */
typedef enum eng_cfg {
    ENG_CFG_DEFAULT = 0,
    ENG_CFG_IN_SWAP_8 = 1,
    ENG_CFG_IN_SWAP_16,
    ENG_CFG_IN_SWAP_32,
    ENG_CFG_OUT_SWAP_8,
    ENG_CFG_OUT_SWAP_16,
    ENG_CFG_OUT_SWAP_32,
    ENG_CFG_DSC_SWAP_8,
    ENG_CFG_DSC_SWAP_16,
    ENG_CFG_DSC_BE_8_16,
    ENG_CFG_DSC_BE_8_32,
    ENG_CFG_KEEP
} eng_cfg_t;

/* Pairs build into lists */
typedef struct pair {
    struct pair *next;
    uint32_t first;
    uint32_t second;
} pair_t;

/* Blocks build into lists */
typedef struct block {
    struct block *next;
    uint32_t base_address;
    uint32_t start;
    uint32_t length;
    char *block_filename;
} block_t;

typedef struct offsets_s {
    bool     init;
    uint32_t first;
    uint32_t second;
} offsets_t;

/* Numbers build into lists */
typedef struct number {
    struct number *next;
    uint32_t num_value;
} number_t;

/* keywords build into lists */
typedef struct keyword {
    struct keyword *next;
    char *string_value;
    uint32_t unsigned_value;
} keyword_t;

/* Value union */
typedef union value {
    char * str;
    uint32_t num;
    keyword_t *keyword;
    number_t *number;
    pair_t* pair;
    block_t* block;
} value_t;

/* Value type */
typedef enum value_type {
    KEYWORD_TYPE,
    NUMBER_TYPE,
    PAIR_TYPE,
    BLOCK_TYPE,
} value_type_t;

/* Arguments build into lists */
typedef struct argument {
    struct argument *next;
    char *name;
    arguments_t type;
    uint32_t value_count;
    value_type_t value_type;
    value_t value;
} argument_t;

/* Command structure */
typedef struct command {
    struct command *next;
    char *name;
    commands_t type;
    uint32_t argument_count;
    argument_t *argument;
    uint8_t* cert_sig_data;   /* Address of certificate/signature data
                                        only valid for aut_dat and ins_key   */
    uint32_t size_cert_sig;         /* Size of certificate/signature buffer  */
    uint32_t start_offset_cert_sig; /* Start offset of cert/sig relative to  */
                                    /*    csf start address                  */
} command_t;

/* Command handler function type */
typedef int32_t (*command_handler_f)(command_t* cmd);

/* Map of label and value, these labels appear on RHS of argument in CSF */
typedef struct map {
    char *label;
    uint32_t value;
} map_t;

/* Map of cmd, type id and handler function */
typedef struct map_cmd {
    char *name;
    commands_t type;
    command_handler_f handler;
} map_cmd_t;

/* Enum for command sequence stage */
typedef enum cmd_states
{
    /* Initial state, waiting for Header cmd */
    WAITING_FOR_HEADER_STATE = 0,
    /* Received Header cmd, waiting for Install SRK cmd */
    INSTALL_SRK_STATE,
    /* Received Install SRK cmd, waiting for Intall CFFK */
    INSTALL_CSFK_STATE,
    /* Received Install CSFK cmd, waiting for Authenticate CSF cmd */
    AUTH_CSF_STATE,
    /* Received Authenticate CSF cmd, but no subsequent Install key cmd */
    AUTH_CSF_NO_KEY_STATE,
    /* Received Authenticate CSF cmd, with a subsequent Install key cmd */
    AUTH_CSF_WITH_APP_KEY_STATE,
    /* Received Authenticate CSF cmd, with a subsequent Install secret key cmd*/
    AUTH_CSF_WITH_ENC_KEY_STATE,
    /* Received Authenticate CSF cmd, with both Install secret and
       authentication key cmds */
    AUTH_CSF_WITH_BOTH_KEY_STATE
} cmd_states_t;

/* Structure to hold dek file name and length */
typedef struct aes_key {
    int32_t key_bytes;
    char * key_file;
} aes_key_t;

typedef struct ahab_data_s {
    char      *srk_table;
    char      *srk_entry;
    uint8_t   srk_index;
    uint8_t   srk_set;
    uint8_t   revocations;
    char      *certificate;
    char      *cert_sign;
    uint8_t   permissions;
    char      *source;
    offsets_t offsets;
    char      *signature;
    char      *destination;
    char      *dek;
    int32_t   dek_length;
    uint32_t  key_identifier;
    uint32_t  image_indexes;
} ahab_data_t;

/*===========================================================================
                              EXTERN
=============================================================================*/
/*===========================================================================
                              GLOBAL VARIABLES
=============================================================================*/
extern int32_t g_error_code;             /* Global error code              */
extern uint8_t g_csf_buffer[]; /* Buffer for CSF data                  */
extern uint32_t g_csf_buffer_index;  /* Index in CSF buffer, used to keep
          track of current position in buf as data is appended to csf buffer */

extern char *g_key_certs[];         /* Array of pointers to img key files   */
extern aes_key_t g_aes_keys[];         /* Array of aes keys */
extern uint8_t g_hab_version;  /* Global to hold hab version in CSF    */
extern uint32_t g_hash_alg;          /* Holds default hash algorithm         */
extern uint32_t g_engine;            /* Holds default engine                 */
extern uint32_t g_engine_config;     /* Holds default engine configuration   */
extern uint32_t g_csf_type;          /* Holds default csf type               */
extern uint32_t g_cert_format;       /* Holds default certificate format     */
extern sig_fmt_t g_sig_format;       /* Holds default signature format       */
extern uint32_t g_unlock_rng;        /* Set if UNLOCK RNG command is present */
extern uint32_t g_init_rng;          /* Set if INIT RNG command is present   */
extern command_t *g_cmd_current;     /* Pointer to current cmd being
                                     processed in command list               */
extern command_t *g_cmd_head;        /* Pointer to head of command list      */
extern char * g_cert_dek;    /* Public key certificate to encrypt dek*/
extern uint32_t g_reuse_dek;         /* Set if DEK is provided */
extern tgt_t g_target;               /* Global to hold target                */
extern uint8_t g_ahab_version;       /* Global to hold ahab version          */
extern ahab_data_t g_ahab_data;      /* Global to hold AHAB data             */
extern func_mode_t g_mode;           /* Global to hold functional mode       */

/*===========================================================================
                              GLOBAL FUNCTIONS
=============================================================================*/

/* Set the value of label in str */
extern int32_t set_label(keyword_t *keyword);
/* Sets the id in the given argument */
extern int32_t set_argument_type(argument_t *arg);
/* Reads cert/sig file and saves data ptr in cmd->cert_sig_data */
extern int32_t save_file_data(command_t *cmd, char *file, uint8_t *data,
        size_t len, int32_t add_header, uint8_t **crt_hash, size_t *hash_len,
        int32_t hash_alg);

/* Add uid to buffer */
extern int32_t append_uid_to_buffer(number_t *uid, uint8_t *buf,
                                    int32_t *bytes_written);

/* Creates signature data file for the given data */
extern int32_t create_sig_file(char *file, char *cert_file,
        sig_fmt_t sig_fmt, uint8_t *data,
        uint32_t data_size);

/* Called by parser on each command */
extern int32_t handle_command(command_t *cmd);

/* Converts hash alg to string name */
extern char* hab_hash_alg_to_digest_name(int32_t hash_alg);

/* Converts hash alg to hash_alg_t */
extern hash_alg_t hab_hash_alg_to_hash_alg_type(int32_t hash);

/* Converts eng cfg to hab defined eng cfg for HAB4 */
extern int32_t get_hab4_engine_config(const int32_t engine,
                                      const eng_cfg_t eng_cfg);

/* AHAB signature handler function */
extern int32_t handle_ahab_signature(void);

/* Individual CSF command handler functions */
extern int32_t cmd_handler_header(command_t* cmd);
extern int32_t cmd_handler_installsrk(command_t *cmd);
extern int32_t cmd_handler_installcsfk(command_t *cmd);
extern int32_t cmd_handler_installnocak(command_t *cmd);
extern int32_t cmd_handler_authenticatecsf(command_t *cmd);
extern int32_t cmd_handler_installkey(command_t *cmd);
extern int32_t cmd_handler_authenticatedata(command_t *cmd);
extern int32_t cmd_handler_installsecretkey(command_t *cmd);
extern int32_t cmd_handler_decryptdata(command_t *cmd);
extern int32_t cmd_handler_nop(command_t *cmd);
extern int32_t cmd_handler_setengine(command_t *cmd);
extern int32_t cmd_handler_init(command_t *cmd);
extern int32_t cmd_handler_unlock(command_t *cmd);
extern int32_t cmd_handler_installcrt(command_t *cmd);
extern int32_t csf(char * output_binary_csf);

extern int yylex(void);

extern void log_error_msg(char* error_msg);
extern void log_arg_cmd(arguments_t arg, char *msg, commands_t cmd);
extern void log_cmd(commands_t cmd, char *msg);
#endif // __CSF_H
