/*===========================================================================*/
/**
    @file    csf_cmd_ins_key.c

    @brief   Code signing tool's CSF command handler for commands
             install key, install csfk and install srk.

@verbatim
=============================================================================

              Freescale Semiconductor
        (c) Freescale Semiconductor, Inc. 2011-2015. All rights reserved.
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
#define HAB_FUTURE
#include <hab_cmd.h>
#include <csf.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl_helper.h>
/*===========================================================================
                                MACROS
=============================================================================*/
#define HAB4_INSTALL_KEY_CMD_CERT_OFFSET    (8)     /**< Offset to cert data */
/*===========================================================================
                          LOCAL FUNCTION DECLARATIONS
=============================================================================*/
static int32_t process_installkey_arguments(command_t* cmd, char** cert_file,
        int32_t* src_index, int32_t* tgt_index,int32_t* hash_alg,
        int32_t* cert_format, char** key_file, int32_t *key_length,
        uint32_t* blob_address, char** src, int32_t *perm, char** sign,
        int32_t *src_set, int32_t *revocations, uint32_t *key_identifier,
        uint32_t *image_indexes);

static int32_t hab4_install_key(int32_t src_index, int32_t tgt_index,
        int32_t hash_alg, int32_t cert_format, uint8_t* crt_hash,
        size_t hash_len, uint8_t* buf, int32_t* cmd_len);

static int32_t hab4_install_secret_key(int32_t src_index, int32_t tgt_index,
        uint32_t blob_address, uint8_t *buf, int32_t *cmd_len);

extern int g_no_ca;
/*===========================================================================
                             LOCAL FUNCTION DEFINITIONS
=============================================================================*/
/**
 * process arguments list for install key command
 *
 * @par Purpose
 *
 * Scan through the arguments list for the command and return requested
 * argument values. The sender will send a valid pointer to return the
 * value. If pointer is NULL then sender is not interested in that argument.
 *
 * @par Operation
 *
 * @param[in] cmd, the command is only used to get arguments list
 *
 * @param[out] cert_file, returns pointer to string for arg FILENAME
 *
 * @param[out] src_index, returns value for arg SOURCEINDEX
 *
 * @param[out] tgt_index, returns value for arg TARGETINDEX
 *
 * @param[out] hash_alg, returns value for arg HASHALGORITHM
 *
 * @param[out] cert_format, returns value for arg CERTIFICATEFORMAT
 *
 * @param[out] key_file, returns pointer to string for arg KEY
 *
 * @param[out] key_length, returns value for arg KEYLENGTH
 *
 * @param[out] blob_address, returns value for arg BLOBADDRESS
 *
 * @retval #SUCCESS  completed its task successfully
 */
static int32_t process_installkey_arguments(command_t* cmd, char** cert_file,
        int32_t* src_index, int32_t* tgt_index, int32_t* hash_alg,
        int32_t* cert_format, char** key_file, int32_t *key_length,
        uint32_t *blob_address, char** src, int32_t *perm, char** sign,
        int32_t *src_set, int32_t *revocations, uint32_t *key_identifier,
        uint32_t *image_indexes)
{
    uint32_t i;                      /**< Loop index        */
    argument_t *arg = cmd->argument; /**< Ptr to argument_t */

    for(i=0; i<cmd->argument_count; i++)
    {
        switch((arguments_t)arg->type)
        {
        case Filename:
            if(cert_file != NULL)
                *cert_file = arg->value.keyword->string_value;
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case SourceIndex:
        case VerificationIndex:
            if(src_index != NULL)
                *src_index = arg->value.number->num_value;
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case TargetIndex:
            if(tgt_index != NULL)
                *tgt_index = arg->value.number->num_value;
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case HashAlgorithm:
            if(hash_alg != NULL)
                *hash_alg = arg->value.keyword->unsigned_value;
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case CertificateFormat:
            if (cert_format != NULL)
                *cert_format = arg->value.keyword->unsigned_value;
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case Key:
            if(key_file != NULL)
            {
                *key_file = arg->value.keyword->string_value;
            }
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case KeyLength:
            if (key_length != NULL)
            {
                *key_length = arg->value.number->num_value;
            }
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case BlobAddress:
            if (blob_address != NULL)
            {
                *blob_address = arg->value.number->num_value;
            }
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case Source:
            if(src != NULL)
                *src = arg->value.keyword->string_value;
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case Permissions:
            if (perm != NULL)
            {
                *perm = arg->value.number->num_value;
            }
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case Signature:
            if(sign != NULL)
                *sign = arg->value.keyword->string_value;
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case SourceSet:
            if (src_set != NULL)
            {
                *src_set = arg->value.keyword->unsigned_value;
            }
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case Revocations:
            if (revocations != NULL)
            {
                *revocations = arg->value.number->num_value;
            }
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case KeyIdentifier:
            if (key_identifier != NULL)
            {
                *key_identifier = arg->value.number->num_value;
            }
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case ImageIndexes:
            if (image_indexes != NULL)
            {
                *image_indexes = arg->value.number->num_value;
            }
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        default:
            log_arg_cmd(arg->type, NULL, cmd->type);
            return ERROR_UNSUPPORTED_ARGUMENT;
        };

        arg = arg->next; /* go to next argument */
    }

    return SUCCESS;
}

/**
 * Updates buf[] with HAB4 install key command
 *
 * @par Purpose
 *
 * This function is called for HAB4 to generate command bytes for install srk,
 * install csfk and install key (imgk) commands. The function copies crt_hash
 * bytes to the buffer at offset INS_KEY_BASE_BYTES and calls INS_KEY macro
 * to generate bytes for install key command and copies them into buf. Function
 * returns command length in cmd_len argument.
 *
 * @par Operation
 *
 * @param[in] src_index, source index to use in the command
 *
 * @param[in] tgt_index, target index to use in the command
 *
 * @param[in] hash_alg, hash algorithm to use for calculating hash length
 *
 * @param[in] cert_format, certificate format an argument for the command
 *
 * @param[in] crt_hash, the hash bytes of the certificate, this will be
 *            appended to the command if pointer is not null.
 *
 * @param[out] buf, address of buffer where csf command will be generated
 *
 * @param[out] cmd_len, returns length of entire command
 *
 * @retval #SUCCESS  completed its task successfully
 */
static int32_t hab4_install_key(int32_t src_index, int32_t tgt_index, int32_t hash_alg,
             int32_t cert_format, uint8_t* crt_hash, size_t hash_len, uint8_t* buf,
             int32_t* cmd_len)
{
    int32_t flag = HAB_CMD_INS_KEY_CLR;   /**< Let flag be set for relative
                                               addresses */

    if(tgt_index == HAB_IDX_CSFK)
        flag |= HAB_CMD_INS_KEY_CSF;

    *cmd_len = INS_KEY_BASE_BYTES;

    if(crt_hash && hash_alg != HAB_ALG_ANY)
    {
        /* include hash bytes */
        memcpy(&buf[INS_KEY_BASE_BYTES], crt_hash, hash_len);

        *cmd_len += hash_len;
    }
    {
        uint8_t ins_key_cmd[] = {
            INS_KEY(*cmd_len, flag, cert_format, hash_alg,
                src_index, tgt_index, 0)
        };                     /**< Macro will output install key
                                    command bytes in aut_csf buffer */
        memcpy(buf, ins_key_cmd, INS_KEY_BASE_BYTES);
    }
    return SUCCESS;
}

/**
 * Updates buf[] install secret key command
 *
 * @par Purpose
 *
 * This function is called for HAB ver > 4.0 to generate command bytes for
 * install secret key command. The function copies the command bytes into buf.
 * Function returns command length in cmd_len argument.
 *
 * @par Operation
 *
 * @param[in] src_index, source index to use in the command
 *
 * @param[in] tgt_index, target index to use in the command
 *
 * @param[in] blob_address, 32 bit absolute address of blob location
 *
 * @param[out] buf, address of buffer where csf command will be generated
 *
 * @param[out] cmd_len, returns length of entire command
 *
 * @pre function should be called for HAB version >= 4.1
 *
 * @retval #SUCCESS  completed its task successfully
 */
static int32_t hab4_install_secret_key(int32_t src_index, int32_t tgt_index,
          uint32_t blob_address, uint8_t *buf, int32_t *cmd_len)
{
    *cmd_len = INS_KEY_BASE_BYTES;

    {
        uint8_t ins_key_cmd[] = {
            INS_KEY(INS_KEY_BASE_BYTES, HAB_CMD_INS_KEY_ABS, HAB_PCL_BLOB,
                HAB_ALG_ANY, src_index, tgt_index, blob_address)
        };                     /**< Macro will output install key
                                    command bytes in ins_key_cmd buffer */
        memcpy(buf, ins_key_cmd, INS_KEY_BASE_BYTES);
    }
    return SUCCESS;
}
/*===========================================================================
                             GLOBAL FUNCTION DEFINITIONS
=============================================================================*/
/**
 * Handler to install srk command
 *
 * @par Purpose
 *
 * Collects necessary arguments from csf file, validate the arguments, set
 * default values for arguments if missing from csf file.
 * For HAB4 only, it calls hab4_install_key to generate install key command
 * into csf buffer. It also calls save_file_data to read certificate data into
 * memory and save the pointer of memory into command.
 *
 * @par Operation
 *
 * @param[in] cmd, the csf command
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval #ERROR_INSUFFICIENT_ARGUMENTS, if necassary args are missing in csf
 *
 * @retval #ERROR_INVALID_ARGUMENT, passed in arguments are invalid or do not
 *          make sense
 *
 * @retval Errors returned by hab4_install_key
 */
int32_t cmd_handler_installsrk(command_t* cmd)
{
    int32_t ret_val = SUCCESS;  /**< Used for returning error value */
    int32_t src_index = -1;     /**< Hold cmd's source index argument value */
    int32_t hash_alg = -1;      /**< Holds hash algorithm argument value */
    int32_t cert_format = -1;   /**< Holds certificate format argument value */
    int32_t cmd_len = 0;        /**< Used to keep track of cmd length */
    int32_t src_set = -1;       /**< Holds source set argument value */
    int32_t revocations = -1;   /**< Holds revocation mask argument value */

    /* get the arguments */
    /* srk key cert is at index 0 */

    if (TGT_AHAB == g_target)
    {
        ret_val = process_installkey_arguments(cmd, &g_ahab_data.srk_table,
            &src_index, NULL, NULL, NULL, NULL, NULL, NULL, &g_ahab_data.srk_entry, NULL, NULL,
            &src_set, &revocations, NULL, NULL);
    }
    else
    {
        ret_val = process_installkey_arguments(cmd, &g_key_certs[HAB_IDX_SRK],
            &src_index, NULL, &hash_alg, &cert_format, NULL, NULL, NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL);
    }

    if(ret_val != SUCCESS)
    {
        return ret_val;
    }

    do {
        if (TGT_AHAB == g_target)
        {
            if(NULL == g_ahab_data.srk_table)
            {
                log_arg_cmd(Filename, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            if(NULL == g_ahab_data.srk_entry)
            {
                log_arg_cmd(Source, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            if (src_index == -1)
            {
                log_arg_cmd(SourceIndex, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            else if (src_index < 0 || src_index > 3)
            {
                log_arg_cmd(SourceIndex, " must be between 0 and 3", cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
            else
            {
                g_ahab_data.srk_index = src_index;
            }
            if (src_set == -1)
            {
                log_arg_cmd(SourceSet, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            else if ((src_set != SRK_SET_OEM) && (src_set != SRK_SET_NXP))
            {
                log_arg_cmd(SourceSet, " must be equal to OEM or NXP", cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
            else
            {
                g_ahab_data.srk_set = (src_set == SRK_SET_NXP) ?
                                      HEADER_FLAGS_SRK_SET_NXP :
                                      HEADER_FLAGS_SRK_SET_OEM;
            }
            if (revocations == -1)
            {
                log_arg_cmd(Revocations, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            else if (revocations < 0 || revocations > 0xF)
            {
                log_arg_cmd(Revocations, " must define a 4-bit bitmask", cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
            else
            {
                g_ahab_data.revocations = revocations;
            }
        }
        else
        if(g_hab_version >= HAB4)
        {
            /* validate the arguments */
            if(g_key_certs[HAB_IDX_SRK] == NULL)
            {
                log_arg_cmd(Filename, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            if(src_index == -1)
            {
                log_arg_cmd(SourceIndex, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            /* certificate format is not an option */
            if(cert_format != -1)
            {
                log_arg_cmd(CertificateFormat, NULL, cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
            /* set the defaults if not provided */
            if(cert_format == -1)
                cert_format = HAB_PCL_SRK;
            if(hash_alg == -1)
                hash_alg = g_hash_alg;

            /* Read data from cert and save the data pointer into command */
            ret_val = save_file_data(cmd, g_key_certs[HAB_IDX_SRK], NULL, 0,
                0, NULL, NULL, hash_alg);
            if(ret_val != SUCCESS)
                break;

            /* Check for valid tag for Super Root Key table saved at
             * cmd->cert_sig_data
             */
            if(cmd->cert_sig_data[0] != HAB_TAG_CRT)
            {
                ret_val = ERROR_INVALID_SRK_TABLE;
                log_error_msg(g_key_certs[HAB_IDX_SRK]);
                break;
            }
            cmd->start_offset_cert_sig = g_csf_buffer_index +
                HAB4_INSTALL_KEY_CMD_CERT_OFFSET;

            /* generate INS_SRK command */
            ret_val = hab4_install_key(src_index, HAB_IDX_SRK,
                hash_alg, cert_format, NULL, 0,
                &g_csf_buffer[g_csf_buffer_index], &cmd_len);
            if(ret_val != SUCCESS)
                break;

            g_csf_buffer_index += cmd_len;
        }
    } while(0);

    return ret_val;
}

/**
 * Handler to install csfk command
 *
 * @par Purpose
 *
 * Collects necessary arguments from csf file, validate the arguments, set
 * default values for arguments if missing from csf file.
 * For HAB4 only, it calls hab4_install_key to generate install key command
 * into csf buffer. It also calls save_file_data to read certificate data into
 * memory and save the pointer of memory into command.
 *
 * @par Operation
 *
 * @param[in] cmd, the csf command
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval #ERROR_INSUFFICIENT_ARGUMENTS, if necassary args are missing in csf
 *
 * @retval #ERROR_INVALID_ARGUMENT, passed in arguments are invalid or do not
 *          make sense
 *
 * @retval Errors returned by hab4_install_key
 */
int32_t cmd_handler_installcsfk(command_t* cmd)
{
    int32_t ret_val = SUCCESS;  /**< Used for returning error value */
    int32_t cert_format = -1;   /**< Holds certificate format argument value */
    int32_t cmd_len = 0;        /**< Used to keep track of cmd length */
    uint8_t *cert_data = NULL;  /**< DER encoded certificate data */
    int32_t cert_len = 0;       /**< length of certificate data */

    /* The Install CSFK command is invalid when AHAB is targeted */
    if (TGT_AHAB == g_target)
    {
        log_cmd(cmd->type, STR_ILLEGAL);
        return ERROR_INVALID_COMMAND;
    }

    /* get the arguments */
    /* csf key is at index 1 */
    ret_val = process_installkey_arguments(cmd, &g_key_certs[HAB_IDX_CSFK],
        NULL, NULL, NULL, &cert_format, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL);

    if(ret_val != SUCCESS)
    {
        return ret_val;
    }

    /* generate install key csf command */
    do {
        if(g_hab_version >= HAB4)
        {
            /* validate the arguments */
            if(g_key_certs[HAB_IDX_CSFK] == NULL)
            {
                log_arg_cmd(Filename, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            if(cert_format == HAB_PCL_SRK)
            {
                log_arg_cmd(CertificateFormat, NULL, cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
            /* set the defaults if not provided */
            if(cert_format == -1)
                cert_format = g_cert_format;

            /* Read data from cert and save the data pointer into command */
            cert_len = get_der_encoded_certificate_data(
                g_key_certs[HAB_IDX_CSFK], &cert_data);
            if(cert_len == 0)
            {
                ret_val = ERROR_INVALID_PKEY_CERTIFICATE;
                log_error_msg(g_key_certs[HAB_IDX_CSFK]);
                break;
            }

            ret_val = save_file_data(cmd, NULL, cert_data, cert_len,
                1, NULL, NULL, HAB_ALG_ANY);
            if(ret_val != SUCCESS)
                break;

            cmd->start_offset_cert_sig = g_csf_buffer_index +
                HAB4_INSTALL_KEY_CMD_CERT_OFFSET;

            /* generate INS_CSFK command */
            ret_val = hab4_install_key(HAB_IDX_SRK,
                HAB_IDX_CSFK, HAB_ALG_ANY, cert_format, NULL, 0,
                &g_csf_buffer[g_csf_buffer_index], &cmd_len);
            if(ret_val != SUCCESS)
                break;
            g_csf_buffer_index += cmd_len;
        }
    } while(0);

    return ret_val;
}

/**
 * Handler to install NOCAk command
 *
 * @par Purpose
 *
 * Collects necessary arguments from csf file, validate the arguments, set
 * default values for arguments if missing from csf file.
 * For HAB4 only, this is the same as cmd_handler_installcsfk(), except it
 * does not generate and write the install key command in the csf buffer.
 *
 * @par Operation
 *
 * @param[in] cmd, the csf command
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval #ERROR_INSUFFICIENT_ARGUMENTS, if necassary args are missing in csf
 *
 * @retval #ERROR_INVALID_ARGUMENT, passed in arguments are invalid or do not
 *          make sense
 *
 * @retval Errors returned by hab4_install_key
 */
int32_t cmd_handler_installnocak(command_t* cmd)
{
    int32_t ret_val = SUCCESS;  /**< Used for returning error value */
    int32_t cert_format = -1;   /**< Holds certificate format argument value */
    int32_t cmd_len = 0;        /**< Used to keep track of cmd length */
    uint8_t *cert_data = NULL;  /**< DER encoded certificate data */
    int32_t cert_len = 0;       /**< length of certificate data */

   /* The Install NOCAK command is invalid when AHAB is targeted */
    if (TGT_AHAB == g_target)
    {
        log_cmd(cmd->type, STR_ILLEGAL);
        return ERROR_INVALID_COMMAND;
    }

    g_no_ca = 1;
    /* get the arguments */
    /* csf key is at index 1 */
    ret_val = process_installkey_arguments(cmd, &g_key_certs[HAB_IDX_CSFK],
        NULL, NULL, NULL, &cert_format, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL);

    if(ret_val != SUCCESS)
    {
        return ret_val;
    }

    /* generate install key csf command */
    do {
        if(g_hab_version >= HAB4)
        {
            /* validate the arguments */
            if(g_key_certs[HAB_IDX_CSFK] == NULL)
            {
                log_arg_cmd(Filename, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            if(cert_format == HAB_PCL_SRK)
            {
                log_arg_cmd(CertificateFormat, NULL, cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
            /* set the defaults if not provided */
            if(cert_format == -1)
                cert_format = g_cert_format;

            /* Read data from cert and save the data pointer into command */
            cert_len = get_der_encoded_certificate_data(
                g_key_certs[HAB_IDX_CSFK], &cert_data);
            if(cert_len == 0)
            {
                ret_val = ERROR_INVALID_PKEY_CERTIFICATE;
                log_error_msg(g_key_certs[HAB_IDX_CSFK]);
                break;
            }

            ret_val = save_file_data(cmd, NULL, cert_data, cert_len,
                1, NULL, NULL, HAB_ALG_ANY);
            if(ret_val != SUCCESS)
                break;

            cmd->start_offset_cert_sig = g_csf_buffer_index +
                HAB4_INSTALL_KEY_CMD_CERT_OFFSET;

            g_csf_buffer_index += cmd_len;
        }
    } while(0);

    return ret_val;
}

/**
 * Handler to install imgk command
 *
 * @par Purpose
 *
 * Collects necessary arguments from csf file, validate the arguments, set
 * default values for arguments if missing from csf file.
 * For HAB4 it calls hab4_install_key to generate install key command
 * into csf buffer. It also calls save_file_data to read certificate data into
 * memory and save the pointer of memory into command.
 *
 * @par Operation
 *
 * @param[in] cmd, the csf command
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval #ERROR_INSUFFICIENT_ARGUMENTS, if necassary args are missing in csf
 *
 * @retval #ERROR_INVALID_ARGUMENT, passed in arguments are invalid or do not
 *          make sense
 *
 * @retval Errors returned by hab4_install_key
 */
int32_t cmd_handler_installkey(command_t* cmd)
{
    int32_t ret_val = SUCCESS;  /**< Used for returning error value */
    int32_t vfy_index = -1;     /**< Holds verification index argument value */
    int32_t tgt_index = -1;     /**< Holds target index argument value */
    int32_t hash_alg = -1;      /**< Holds hash algorithm argument value */
    int32_t cert_format = -1;   /**< Holds certificate format argument value */
    uint8_t *crt_hash = NULL;   /**< Buffer for certificate hash bytes */
    size_t hash_len = 0;        /**< Number of hash bytes in crt_hash */
    int32_t cmd_len = 0;        /**< Used to keep track of cmd length */
    char * img_key_crt = NULL;  /**< Points to image key file name */
    uint8_t *cert_data = NULL;  /**< DER encoded certificate data */
    int32_t cert_len = 0;       /**< length of certificate data */

    /* The Install Key command is invalid when AHAB is targeted */
    if (TGT_AHAB == g_target)
    {
        log_cmd(cmd->type, STR_ILLEGAL);
        return ERROR_INVALID_COMMAND;
    }

    /* get the arguments */
    ret_val = process_installkey_arguments(cmd, &img_key_crt,
        &vfy_index, &tgt_index, &hash_alg, &cert_format, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL);

    if(ret_val != SUCCESS)
    {
        return ret_val;
    }

    /* generate install key csf command */
    do {
        if(g_hab_version >= HAB4)
        {
            /* validate the arguments */
            if(img_key_crt == NULL)
            {
                log_arg_cmd(Filename, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            if(vfy_index == -1)
            {
                log_arg_cmd(VerificationIndex, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            if(tgt_index == -1)
            {
                log_arg_cmd(TargetIndex, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            if(tgt_index == HAB_IDX_SRK || tgt_index == HAB_IDX_CSFK)
            {
                log_arg_cmd(TargetIndex, NULL, cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
            if(cert_format == HAB_PCL_SRK)
            {
                log_arg_cmd(CertificateFormat, NULL, cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
            if(tgt_index >= HAB_KEY_PUBLIC_MAX)
            {
                log_arg_cmd(TargetIndex, STR_EXCEED_MAX, cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
            /* set the defaults if not provided */
            if(cert_format == -1)
                cert_format = g_cert_format;
            if(hash_alg == -1)
                hash_alg = HAB_ALG_ANY;

            /* Save the file name pointer at tgt_index of g_key_certs */
            g_key_certs[tgt_index] = img_key_crt;

            /* Read data from cert and save the data pointer into command */
            cert_len = get_der_encoded_certificate_data(img_key_crt,
                &cert_data);
            if(cert_len == 0)
            {
                ret_val = ERROR_INVALID_PKEY_CERTIFICATE;
                log_error_msg(img_key_crt);
                break;
            }

            ret_val = save_file_data(cmd, NULL, cert_data, cert_len,
                1, NULL, NULL, HAB_ALG_ANY);
            if(ret_val != SUCCESS)
                break;

            cmd->start_offset_cert_sig = g_csf_buffer_index +
                HAB4_INSTALL_KEY_CMD_CERT_OFFSET;

            /* generate INS_IMGK command */
            ret_val = hab4_install_key(vfy_index, tgt_index,
                hash_alg, cert_format, crt_hash, hash_len,
                &g_csf_buffer[g_csf_buffer_index], &cmd_len);
            if(ret_val != SUCCESS)
                break;

            g_csf_buffer_index += cmd_len;
        }
    } while(0);

    if(crt_hash)
        free (crt_hash);

    return ret_val;
}

/**
 * Handler to install secret key command
 *
 * @par Purpose
 *
 * Collects necessary arguments from csf file, validate the arguments, set
 * default values for arguments if missing from csf file.
 * This command is applicable from HAB 4.1 onwards and only on processors
 * which include CAAM and SNVS. Each instance of this command generates a
 * CSF command to install a secret key in CAAM's secret key store with
 * protocol set to HAB_PCL_BLOB. The blob is unwrapped using a master key
 * encryption key (KEK) supplied by SNVS. A random key is generated.
 * The key is  encrypted by the CST back end, only if a a certificate was provided.
 * This file is intended for later use by the mfgtool
 * to create the blob. The encryption is done with public key certificate
 * passed to CST on command line. Crt_hash is not generated for this command
 * as it is not required by HAB.
 *
 * @par Operation
 *
 * @param[in] cmd, the csf command
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval #ERROR_INSUFFICIENT_ARGUMENTS, if necassary args are missing in csf
 *
 * @retval #ERROR_INVALID_ARGUMENT, passed in arguments are invalid or do not
 *          make sense
 */
int32_t cmd_handler_installsecretkey(command_t* cmd)
{
    int32_t ret_val = SUCCESS;      /**< Used for returning error value */
    int32_t vfy_index = -1;         /**< Holds verification index argument value */
    int32_t tgt_index = -1;         /**< Holds target index argument value */
    int32_t cmd_len = 0;            /**< Used to keep track of cmd length */
    char * secret_key = NULL;       /**< Points to secret key file name */
    uint32_t blob_address = 0;      /**< Memory location for blob data */
    int32_t key_length = -1;        /**< Holds key length argument value */
    uint32_t key_identifier = 0;    /**< Holds key identifier value (default 0) */
    uint32_t images_indexes = 0xFFFFFFFF; /**< Holds indexes of image to be encrypted (default all) */

    /* get the arguments */
    if (TGT_AHAB != g_target) {
        /* This command is supported from HAB 4.1 onwards */
        if(g_hab_version <= HAB4)
        {
            ret_val = ERROR_INVALID_COMMAND;
            return ret_val;
        }

        ret_val = process_installkey_arguments(cmd, NULL, &vfy_index, &tgt_index,
            NULL, NULL, &secret_key, &key_length, &blob_address, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL);
    }
    else {
        ret_val = process_installkey_arguments(cmd, NULL, NULL, NULL,
            NULL, NULL, &secret_key, &key_length, NULL, NULL, NULL, NULL,
            NULL, NULL, &key_identifier, &images_indexes);
    }

    if(ret_val != SUCCESS)
    {
        return ret_val;
    }

    /* generate install secret key command */
    do {
        /* validate the arguments */

        /* Output key file is a must */
        if(secret_key == NULL)
        {
            log_arg_cmd(Key, NULL, cmd->type);
            ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
            break;
        }

        if (TGT_AHAB != g_target) {
            /* Target index is a must */
            if(tgt_index == -1)
            {
                log_arg_cmd(TargetIndex, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            /* Blob address is also needed */
            if(blob_address == 0)
            {
                log_arg_cmd(BlobAddress, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            /* Target index cannot be greater than max allowed */
            if(tgt_index >= HAB_KEY_SECRET_MAX)
            {
                log_arg_cmd(TargetIndex, STR_EXCEED_MAX, cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
            /* set the defaults if not provided */
            if(vfy_index == -1)
            {
                vfy_index = HAB_SNVS_OTPMK;
            }
            else
            {
                if(vfy_index > HAB_SNVS_CMK)
                {
                    log_arg_cmd(VerificationIndex, STR_EXCEED_MAX, cmd->type);
                    ret_val = ERROR_INVALID_ARGUMENT;
                    break;
            }
            }
        }

       /* Valid values for Key_length: 128, 192 and 256 */
        if(key_length == -1)
        {
            /* Default to 128 bits */
            key_length = AES_KEY_LEN_128;
        }
        else
        {
            /* Check for invalid length */
            if((key_length != AES_KEY_LEN_128) &&
               (key_length != AES_KEY_LEN_192) &&
               (key_length != AES_KEY_LEN_256))
            {
                log_arg_cmd(KeyLength, STR_ILLEGAL, cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
        }

        if (TGT_AHAB != g_target) {
            /* Calculate dek length in bytes and save secret_key name */
            g_aes_keys[tgt_index].key_bytes = (key_length / BYTE_SIZE_BITS);
            g_aes_keys[tgt_index].key_file = secret_key;

            /* Generate Install key command for the secret key */
            ret_val = hab4_install_secret_key(vfy_index, tgt_index,
                blob_address, &g_csf_buffer[g_csf_buffer_index], &cmd_len);
            if(ret_val != SUCCESS)
                break;

            g_csf_buffer_index += cmd_len;
        }
        else {
            g_ahab_data.dek = secret_key;
            g_ahab_data.dek_length = (key_length / BYTE_SIZE_BITS);
            g_ahab_data.key_identifier = key_identifier;
            g_ahab_data.image_indexes = images_indexes;
        }
    } while(0);

    return ret_val;
}

/**
 * Handler to install Certificate command
 *
 * @par Purpose
 *
 * Collects necessary arguments from csf file, validate the arguments, set
 * default values for arguments if missing from csf file.
 * This command is applicable from AHAB onwards.
 *
 * @par Operation
 *
 * @param[in] cmd, the csf command
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval #ERROR_INSUFFICIENT_ARGUMENTS, if necassary args are missing in csf
 *
 * @retval #ERROR_INVALID_ARGUMENT, passed in arguments are invalid or do not
 *          make sense
 */
int32_t cmd_handler_installcrt(command_t* cmd)
{
    int32_t ret_val;          /**< Used for returning error value   */
    int32_t permissions = -1; /**< Holds permissions argument value */

    /* The Install Certificate command is invalid when AHAB is not targeted */
    if (TGT_AHAB != g_target)
    {
        log_cmd(cmd->type, STR_ILLEGAL);
        return ERROR_INVALID_COMMAND;
    }

    /* get the arguments */
    ret_val = process_installkey_arguments(cmd, &g_ahab_data.certificate,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &permissions, &g_ahab_data.cert_sign,
        NULL, NULL, NULL, NULL);

    if (SUCCESS != ret_val)
    {
        return ret_val;
    }

    do {
        /* validate the arguments */
        if (NULL == g_ahab_data.certificate)
        {
            log_arg_cmd(Filename, NULL, cmd->type);
            ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
            break;
        }
        if (-1 == permissions)
        {
            log_arg_cmd(Permissions, NULL, cmd->type);
            ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
            break;
        }
        else if (0xFF < permissions)
        {
            log_arg_cmd(Permissions, STR_GREATER_THAN_255, cmd->type);
            ret_val = ERROR_INVALID_ARGUMENT;
            break;
        }
        else
        {
            g_ahab_data.permissions = EXTRACT_BYTE(permissions, 0);
        }
    } while(0);

    return ret_val;
}
