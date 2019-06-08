/*===========================================================================*/
/**
    @file    csf_cmd_aut_dat.c

    @brief   Code signing tool's CSF command handler for commands
             authenticate data and authenticate csf.

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
#define HAB4_AUT_DAT_CMD_SIG_OFFSET    (8) /**< Offset to signature data */
#define BYTES_64KB               (0x10000) /**< Define for 64KB */
#define BYTES_16MB             (0x1000000) /**< Define for 16MB */
/*===========================================================================
                          LOCAL FUNCTION DECLARATIONS
=============================================================================*/
static int32_t process_authenticatedata_arguments(command_t* cmd, block_t **block,
        int32_t *vfy_index, int32_t *engine, int32_t *engine_cfg,
        sig_fmt_t *sig_format, int32_t *hash_alg,
        size_t *mac_bytes, char** src, offsets_t *offsets, char** sign);

static int32_t hab4_authenticate_data(sig_fmt_t sig_format, int32_t engine,
        int32_t engine_cfg, int32_t vfy_index, block_t *block, uint8_t *buf,
        int32_t *cmd_len, size_t* size_blocks);

static int32_t validate_block_arguments(block_t *block_list);

int32_t write_encrypted_data_to_blocks(const char* file, block_t * block);

int32_t read_data_to_encrypt_from_blocks(const char* file, block_t * block);

static size_t length_field_bytes(size_t msg_bytes);

static int32_t generate_and_save_aead_data(uint8_t * nonce,
                                    size_t nonce_bytes,
                                    uint8_t * mac,
                                    size_t mac_bytes,
                                    const char * out_file);

int g_no_ca = 0;
/*===========================================================================
                          LOCAL FUNCTION DEFINITIONS
=============================================================================*/
/**
 * process arguments list for authenticate data and authenticate csf
 * commands
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
 * @param[out] block, returns block ptr of arg->value.block for arg BLOCKS
 *
 * @param[out] vfy_index, returns value for arg VERIFICATIONINDEX
 *
 * @param[out] engine, returns value for arg ENGINE
 *
 * @param[out] engine_cfg, returns value for arg ENGINECONFIGURATION
 *
 * @param[out] sig_format, returns value for arg SIGNATUREFORMAT
 *
 * @param[out] hash_alg, returns value for arg HASHALGORITHM
 *
 * @param[out] mac bytes, returns value for arg MacLength
 *
 * @retval #SUCCESS  completed its task successfully
 */
static int32_t process_authenticatedata_arguments(command_t* cmd,
        block_t **block, int32_t *vfy_index, int32_t *engine,
        int32_t *engine_cfg, sig_fmt_t *sig_format, int32_t *hash_alg,
        size_t *mac_bytes, char** src,
        offsets_t *offsets, char** sign)
{
    uint32_t i;                          /**< Loop index        */
    argument_t *arg = cmd->argument;     /**< Ptr to argument_t */

    for(i=0; i<cmd->argument_count; i++)
    {
        switch((arguments_t)arg->type)
        {
        case Blocks:
            if(block != NULL)
                *block = arg->value.block;
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case VerificationIndex:
            if(vfy_index != NULL)
                *vfy_index = arg->value.number->num_value;
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case EngineName:
            if(engine != NULL)
                *engine = arg->value.keyword->unsigned_value;
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case EngineConfiguration:
            if(engine_cfg != NULL)
            {
                /* Engine configuration could be number or keyword */
                if(arg->value_type == NUMBER_TYPE)
                    *engine_cfg = arg->value.number->num_value;
                else
                    *engine_cfg = arg->value.keyword->unsigned_value;
            }
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case SignatureFormat:
            if(sig_format != NULL)
                *sig_format = arg->value.keyword->unsigned_value;
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
        case MacBytes:
            if(mac_bytes != NULL)
            {
                *mac_bytes = arg->value.number->num_value;
            }
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case Filename:
            if(src != NULL)
                *src = arg->value.keyword->string_value;
            else
            {
                log_arg_cmd(arg->type, NULL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            break;
        case Offsets:
            if(offsets != NULL)
            {
                offsets->first  = arg->value.pair->first;
                offsets->second = arg->value.pair->second;
                offsets->init   = true;
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
        default:
            log_arg_cmd(arg->type, NULL, cmd->type);
            return ERROR_UNSUPPORTED_ARGUMENT;
        };

        arg = arg->next; /* go to next argument */
    }

    return SUCCESS;
}

/**
 * Validates each block arguments in the argument list
 *
 * @par Purpose
 *
 * Validate arguments for each block in the block list.
 *
 * @par Operation
 *
 * @param[in] block_list, pointer to block list
 *
 * @retval #SUCCESS  all arguments validated to be correct
 *
 * @retval #ERROR_FILE_NOT_PRESENT file to get block data is not present
 *
 * @retval #ERROR_INVALID_BLOCK_ARGUMENTS on any other check fails
 */
int32_t validate_block_arguments(block_t *block_list)
{
    int32_t ret_val = SUCCESS;
    block_t *block = block_list;

    while(block != NULL)
    {
        FILE *fh = fopen(block->block_filename, "rb");
        uint32_t file_size = 0;

        if(fh == NULL)
        {
            log_error_msg(block->block_filename);
            ret_val = ERROR_FILE_NOT_PRESENT;
            break;
        }

        /* Get the size of the file */
        fseek(fh, 0, SEEK_END);
        file_size = ftell(fh);
        fclose(fh);

        if((block->start + block->length) > file_size)
        {
            log_arg_cmd(Blocks, STR_BLKS_INVALID_LENGTH, CmdAuthenticateData);

            ret_val = ERROR_INVALID_BLOCK_ARGUMENTS;
            break;
        }
        block = block->next;
    }

    return ret_val;
}

/**
 * Updates g_csf_buffer with authenticate csf command
 *
 * @par Purpose
 *
 * Collects necessary arguments from csf file, validate the arguments, set
 * default values for arguments if missing from csf file.
 * Calls Macro AUT_CSF to update g_csf_buffer with authenticate csf command.
 * Finally generates dummy csf signature and assigns the buffer address
 * with signature data to the command.
 *
 * @par Operation
 *
 * @param[in] cmd, the csf command
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval Errors returned by create_sig_file and save_file_data functions
 */
int32_t cmd_handler_authenticatecsf(command_t* cmd)
{
    int32_t ret_val = SUCCESS;  /**< Used for return value */
    sig_fmt_t sig_format = SIG_FMT_UNDEF; /**< Holds sig format argument value */
    int32_t engine = -1;        /**< Holds engine argument value */
    int32_t engine_cfg = -1;    /**< Holds engine configuration arg value */

    /* The Authenticate CSF command is invalid when AHAB is targeted */
    if (TGT_AHAB == g_target)
    {
        log_cmd(cmd->type, STR_ILLEGAL);
        return ERROR_INVALID_COMMAND;
    }

    /* get the arguments */
    ret_val = process_authenticatedata_arguments(cmd, NULL,
        NULL, &engine, &engine_cfg, &sig_format, NULL, NULL, NULL, NULL, NULL);

    if(ret_val != SUCCESS)
    {
        return ret_val;
    }

    /* generate authenticate csf command */
    do {

        if(g_hab_version >= HAB4)
        {
            /* validate the arguments */
            if(engine == HAB_ENG_ANY && engine_cfg != 0)
            {
                log_arg_cmd(EngineName, STR_ENG_ANY_CFG_NOT_ZERO, cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
            /* set the defaults if not provided */
            if(SIG_FMT_UNDEF == sig_format)
                sig_format = g_sig_format;
            else if (sig_format != SIG_FMT_CMS)
            {
                log_arg_cmd(SignatureFormat, " different from CMS"STR_ILLEGAL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }
            if(engine == -1)
                engine = g_engine;
            if(engine_cfg == -1)
                engine_cfg = g_engine_config;

            engine_cfg = get_hab4_engine_config(engine, (const eng_cfg_t)engine_cfg);

            /* Above returns only one of ERROR_INVALID_ENGINE_CFG or
             * ERROR_INVALID_ENGINE, return on error
             */
            if(engine_cfg == ERROR_INVALID_ENGINE_CFG)
            {
                log_arg_cmd(EngineConfiguration, NULL, cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
            if(engine_cfg == ERROR_INVALID_ENGINE)
            {
                log_arg_cmd(EngineName, NULL, cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }

            /* generate AUT_CSF command */
            {
                uint8_t aut_csf[] =  {
                    AUT_CSF(HAB_CMD_AUT_DAT_CLR,
                        (SIG_FMT_CMS == sig_format) ? HAB_PCL_CMS : HAB_ALG_PKCS1,
                        engine, engine_cfg, 0)
                };                 /**< Macro will output authenticate csf
                                        command bytes in aut_csf buffer */

                memcpy(&g_csf_buffer[g_csf_buffer_index], aut_csf,
                    AUT_CSF_BYTES);

                cmd->start_offset_cert_sig = g_csf_buffer_index + HAB4_AUT_DAT_CMD_SIG_OFFSET;

                g_csf_buffer_index += AUT_CSF_BYTES;
            }
        }
        /**
         * Dummy signature for now, will be regenerated once csf processing
         * is completed. Required to advance g_csf_buffer_index.
         */
        ret_val = create_sig_file(FILE_SIG_CSF_DATA, g_key_certs[HAB_IDX_CSFK],
                    (g_hab_version >= HAB4) ? SIG_FMT_CMS : SIG_FMT_PKCS1,
                    g_csf_buffer, HAB_CSF_BYTES_MAX);
        if(ret_val != SUCCESS)
        {
            break;
        }

        /**
         * Finally save_file_data to read the sig file and attaches data
         * buffer to the cmd.
         */
        ret_val = save_file_data(cmd, FILE_SIG_CSF_DATA, NULL, 0,
            (g_hab_version >= HAB4), NULL, NULL, g_hash_alg);
        if(ret_val != SUCCESS)
        {
            break;
        }
    } while(0);

    return ret_val;
}

/**
 * Updates g_csf_buffer with authenticate data command
 *
 * @par Purpose
 *
 * Called by cmd_handler_authenticatedata. Call AUT_IMG macro to
 * generate data for AUTHENTICATE IMAGE command then updates the buffer
 * with block address and length pairs. It returns cmd len in cmd_len argument
 * and length of blocks data in bytes in size_blocks argument.
 *
 * @par Operation
 *
 * @param[in] sig_format, signature format passed to AUT_IMG macro
 *
 * @param[in] engine, engine passed to AUT_IMG macro
 *
 * @param[in] engine_cfg, engine configuration passed to AUT_IMG macro
 *
 * @param[in] vfy_index, verification key index passed to AUT_IMG macro
 *
 * @param[in] block, pointer to block list, runs through the list and add
 *                    block address and length to buf.
 *
 * @param[out] buf, address of buffer where csf command will be generated
 *
 * @param[out] cmd_len, returns length of entire command
 *
 * @param[out] size_blocks, returns size in bytes for the blocks
 *
 * @pre  cmd_len set to 0 before calling into this function.
 *
 * @retval #SUCCESS  completed its task successfully
 */
static int32_t hab4_authenticate_data(sig_fmt_t sig_format, int32_t engine,
    int32_t engine_cfg, int32_t vfy_index, block_t *block, uint8_t *buf,
    int32_t *cmd_len, size_t* size_blocks)
{
    int32_t num_blocks = 0;        /**< Count for number of blocks */

    *cmd_len += AUT_DAT_BASE_BYTES;
    *size_blocks = 0;
    while (block != NULL)
    {
        uint8_t start[] = {
            EXPAND_UINT32(block->base_address)
        };                /**< Macro will initialize start buffer with
                              4 bytes address */
        uint8_t length[] = {
            EXPAND_UINT32(block->length)
        };                /**< Macro will initialize length buffer with
                              4 bytes length */

        memcpy(&buf[*cmd_len], start, 4);
        memcpy(&buf[*cmd_len + 4], length, 4);

        *cmd_len += 8;
        *size_blocks += block->length;
        block = block->next;
        num_blocks ++;
    }
    {
        uint8_t pcl;

        switch (sig_format) {
        case SIG_FMT_CMS:
            pcl = HAB_PCL_CMS;
            break;
        case SIG_FMT_AEAD:
            pcl = HAB_PCL_AEAD;
            break;
        default:
            return ERROR_INVALID_ARGUMENT;
        }

        uint8_t aut_dat[] =  {
            AUT_IMG(num_blocks, HAB_CMD_AUT_DAT_CLR, vfy_index,
                pcl,
                engine, engine_cfg, 0)
        };                /**< Macro will output authenticate data
                                        command bytes in aut_dat buffer */

        memcpy(buf, aut_dat, AUT_DAT_BASE_BYTES);
    }

    return SUCCESS;
}

/**
 * Updates g_csf_buffer with authenticate data command
 *
 * @par Purpose
 *
 * Collects necessary arguments from csf file, validate the arguments,
 * set default values for arguments if missing from csf file.
 * Updates g_csf_buffer with authenticate data command.
 * Finally generates image signature and assigns the buffer
 * address with signature data to the command.
 *
 * @par Operation
 *
 * @param[in] cmd, the csf command
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval Errors returned by create_sig_file and save_file_data functions
 */
int32_t cmd_handler_authenticatedata(command_t* cmd)
{
    int32_t ret_val = SUCCESS;   /**< Used for return value */
    sig_fmt_t sig_format = SIG_FMT_UNDEF; /**< Holds sig format argument value */
    int32_t engine = -1;         /**< Holds engine argument value */
    int32_t engine_cfg = -1;     /**< Holds engine config argument value */
    int32_t vfy_index = -1;      /**< Holds verify index argument value */
    int32_t hash_alg = -1;       /**< Holds hash algorithm argument value */
    block_t *block = NULL;       /**< Holds address of block list argument */
    char* cert_file;             /**< Ptr to name of certificate file */
    size_t blocks_data_size=0;  /**< Bytes occupied by block data in cmd */
    uint8_t *data = NULL;        /**< Mem to read data blocks */
    int32_t offset_in_data;      /**< Used in the loop for reading block
                                      data */
    int32_t cmd_len = 0;         /**< Used to track command length */
    FILE * fh = NULL;            /**< File pointer to open and read block
                                      data from binary files */

    /* get the arguments */
    if (TGT_AHAB == g_target)
    {
        ret_val = process_authenticatedata_arguments(
                      cmd,  NULL, NULL, NULL,
                      NULL, NULL, NULL,
                      NULL, &g_ahab_data.source, &g_ahab_data.offsets,
                      &g_ahab_data.signature);
    }
    else
    {
        ret_val = process_authenticatedata_arguments(cmd, &block, &vfy_index,
            &engine, &engine_cfg, &sig_format, &hash_alg, NULL, NULL, NULL, NULL);
    }

    if(ret_val != SUCCESS)
    {
        return ret_val;
    }

    /* generate authenticate data command */
    do {

        if (TGT_AHAB == g_target)
        {
            if(NULL == g_ahab_data.source)
            {
                log_arg_cmd(Filename, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            if(false == g_ahab_data.offsets.init)
            {
                log_arg_cmd(Offsets, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
        }
        else
        {
            /* validate the arguments */
            if(engine == HAB_ENG_ANY && engine_cfg != 0)
            {
                log_arg_cmd(EngineName, STR_ENG_ANY_CFG_NOT_ZERO, cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
            if(vfy_index == -1)
            {
                log_arg_cmd(VerificationIndex, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            if(block == NULL )
            {
                log_arg_cmd(Blocks, NULL, cmd->type);
                ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
                break;
            }
            ret_val = validate_block_arguments(block);
            if(ret_val != SUCCESS)
            {
                break;
            }

            if(g_hab_version >= HAB4)
            {
                /* validate the arguments */
                if(vfy_index == HAB_IDX_SRK)
                {
                    if (g_no_ca == 0)
                    {
                        log_arg_cmd(VerificationIndex, NULL, cmd->type);

                        ret_val = ERROR_INVALID_ARGUMENT;
                        break;
                    }
                }
                if(vfy_index == HAB_IDX_CSFK)
                {
                    log_arg_cmd(VerificationIndex, NULL, cmd->type);

                    ret_val = ERROR_INVALID_ARGUMENT;
                    break;
                }
                /* set the defaults if not provided */
                if(SIG_FMT_UNDEF == sig_format)
                    sig_format = g_sig_format;
                else if (sig_format != SIG_FMT_CMS)
                {
                    log_arg_cmd(SignatureFormat, " different from CMS"STR_ILLEGAL, cmd->type);
                    return ERROR_UNSUPPORTED_ARGUMENT;
                }
                if(engine == -1)
                    engine = g_engine;
                if(engine_cfg == -1)
                    engine_cfg = g_engine_config;

                engine_cfg = get_hab4_engine_config(engine, (const eng_cfg_t)engine_cfg);

                /* Above returns only one of ERROR_INVALID_ENGINE_CFG or
                * ERROR_INVALID_ENGINE, return on error
                */
                if(engine_cfg == ERROR_INVALID_ENGINE_CFG)
                {
                    log_arg_cmd(EngineConfiguration, NULL, cmd->type);
                    ret_val = ERROR_INVALID_ARGUMENT;
                    break;
                }
                if(engine_cfg == ERROR_INVALID_ENGINE)
                {
                    log_arg_cmd(EngineName, NULL, cmd->type);
                    ret_val = ERROR_INVALID_ARGUMENT;
                    break;
                }

                /* generate AUT_IMG command */
                ret_val = hab4_authenticate_data(sig_format, engine, engine_cfg,
                    vfy_index, block, &g_csf_buffer[g_csf_buffer_index], &cmd_len,
                    &blocks_data_size);
                if(ret_val != SUCCESS)
                {
                    break;
                }
                cmd->start_offset_cert_sig = g_csf_buffer_index +
                    HAB4_AUT_DAT_CMD_SIG_OFFSET;

                g_csf_buffer_index += cmd_len;
            }

            if (g_no_ca == 0)
            {
                cert_file = g_key_certs[vfy_index];

                if (NULL == cert_file) {
                    log_arg_cmd(VerificationIndex, NULL, cmd->type);
                    ret_val = ERROR_INVALID_ARGUMENT;
                    break;
                }
            }
            else
            {
                cert_file = g_key_certs[HAB_IDX_CSFK];
            }
            /* Get the total size for the blocks data and allocate memory */
            offset_in_data = 0;
            data = malloc(blocks_data_size);
            if(data == NULL)
            {
                ret_val = ERROR_INSUFFICIENT_MEMORY;
                break;
            }
            /* Read the data from block files into data buffer */
            while(block != NULL)
            {
                fh = fopen(block->block_filename, "rb");
                if(fh == NULL)
                {
                    log_error_msg(block->block_filename);
                    ret_val = ERROR_OPENING_FILE;
                    break;
                }

                fseek(fh, block->start, SEEK_SET);
                if(fread(data+offset_in_data, 1, block->length, fh)
                    != block->length)
                {
                    log_error_msg(block->block_filename);
                    ret_val = ERROR_READING_FILE;
                    break;
                }
                offset_in_data += block->length;
                fclose(fh);
                block = block->next;
            }
            if(ret_val != SUCCESS)
            {
                break;
            }

            /* Generate signature for the data */
            ret_val = create_sig_file(FILE_SIG_IMG_DATA, cert_file,
                (g_hab_version >= HAB4) ? SIG_FMT_CMS : SIG_FMT_PKCS1,
                data, blocks_data_size);

            if(ret_val != SUCCESS)
            {
                break;
            }
            /* Save the signature data into command */
            ret_val = save_file_data(cmd, FILE_SIG_IMG_DATA, NULL, 0,
                (g_hab_version >= HAB4), NULL, NULL, g_hash_alg);
            if(ret_val != SUCCESS)
            {
                break;
            }
        }
    } while(0);

    return ret_val;
}

/**
 * Read plain text data to encrypt from files specified in blocks and writes it
 * to file.
 *
 * @par Purpose
 *
 * This function reads all given data blocks to encrypt and writes it to the file.
 * At the end of this function all data for encryption specified in block array
 * are written to the file.
 *
 * @par Operation
 *
 * @param[in] file, filename to write data for encryption
 *
 * @param[in] block, array of blocks
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval Errors when fopen, fread or fwrite fails
 */
int32_t read_data_to_encrypt_from_blocks(const char* file, block_t * block)
{
    int32_t ret_val = SUCCESS;    /**< Used for return value */
    FILE *fh = NULL;              /**< File handle for input reading */
    FILE *fho = NULL;             /**< File handle for output writing */
    uint32_t bytes_to_read;        /**< Variable to keep track bytes to read */
    uint32_t offset;               /**< Offset to start of block in file */
    uint32_t bytes_read_this_iter; /**< Bytes read in a while loop */
    uint8_t input_buffer[FILE_BUF_SIZE];   /**< Mem to read data blocks */

    fho = fopen(file, "wb");
    if(fho == NULL)
    {
        log_error_msg((char*)file);
        return (ERROR_OPENING_FILE);
    }

    /* Copy blocks for encryption into file */
    while(block != NULL)
    {
        bytes_to_read = block->length;
        offset = block->start;

        /*
         * Open the file in read mode
         */
        fh = fopen(block->block_filename, "rb");
        if(fh == NULL)
        {
            log_error_msg(block->block_filename);
            ret_val = ERROR_OPENING_FILE;
            break;
        }

        /*
         * The loop below reads size of input_buffer amount of data per
         * iteration from the file, and writes it to the
         * file.
         */
        while(1)
        {
            if(bytes_to_read < FILE_BUF_SIZE)
            {
                bytes_read_this_iter = bytes_to_read;
            }
            else
            {
                bytes_read_this_iter = FILE_BUF_SIZE;
            }

            /* Seek to the current offset, init to block->start */
            fseek(fh, offset, SEEK_SET);

            /* Read data into input buffer */
            if(fread(input_buffer, 1, bytes_read_this_iter, fh) !=
                bytes_read_this_iter)
            {
                log_error_msg(block->block_filename);
                ret_val = ERROR_READING_FILE;
                break;
            }

            /* Append data to temp file */
            if(fwrite(input_buffer, 1, bytes_read_this_iter, fho) !=
                bytes_read_this_iter)
            {
                log_error_msg((char*) file);
                ret_val = ERROR_WRITING_FILE;
                break;
            }

            /* Update offset and bytes_to_read */
            offset += bytes_read_this_iter;
            bytes_to_read -= bytes_read_this_iter;

            /* Exit the loop if done */
            if(bytes_to_read <= 0)
                break;
        }

        fclose(fh);
        fh = NULL;

        if(ret_val != CAL_SUCCESS)
            break;

        /* Go for next block */
        block = block->next;
    }

    fclose(fho);
    fho = NULL;

    return ret_val;
}

/**
 * Write encrypted data from file into the target files specified in blocks
 *
 * @par Purpose
 *
 * Loops through all blocks and write back encrypted data replacing the plain
 * text data. At the end of this function all files specified in block array
 * are updated with encrypted data.
 *
 * @par Operation
 *
 * @param[in] file, input file with encrypted data
 *
 * @param[in] block, array of blocks
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval Errors when fopen, fread or fwrite fails
 */
int32_t write_encrypted_data_to_blocks(const char* file, block_t * block)
{
    int32_t ret_val = SUCCESS;    /**< Used for return value */
    FILE * fh = NULL;             /**< File handle for input reading */
    FILE * fho = NULL;            /**< File handle for output writing */
    uint32_t bytes_to_read;        /**< Variable to keep track bytes to read */
    uint32_t offset;               /**< Offset to start of block in file */
    uint32_t bytes_read_this_iter; /**< Bytes read in a while loop */
    uint8_t input_buffer[FILE_BUF_SIZE];  /**< Mem to read data blocks */

    fh = fopen(file, "rb");
    if(fh == NULL)
    {
        log_error_msg((char*)file);
        return ERROR_OPENING_FILE;
    }

    /* Start encryption of data specified in blocks */
    while(block != NULL)
    {
        bytes_to_read = block->length;
        offset = block->start;

        /*
         * Open the file in read write mode
         */
        fho = fopen(block->block_filename, "rb+");
        if(fho == NULL)
        {
            log_error_msg(block->block_filename);
            ret_val = ERROR_OPENING_FILE;
            break;
        }

        /*
         * The loop below reads size of input_buffer amount of data per
         * iteration from the file, and writes it to the
         * file.
         */
        while(1)
        {
            if(bytes_to_read < FILE_BUF_SIZE)
            {
                bytes_read_this_iter = bytes_to_read;
            }
            else
            {
                bytes_read_this_iter = FILE_BUF_SIZE;
            }

            /* Seek to the current offset, init to block->start */
            fseek(fho, offset, SEEK_SET);

            /* Read encrypted data into input_buffer */
            if(fread(input_buffer, 1, bytes_read_this_iter, fh) !=
                bytes_read_this_iter)
            {
                log_error_msg((char*)file);
                ret_val = ERROR_READING_FILE;
                break;
            }

            /* Write encrypted data to block file */
            if(fwrite(input_buffer, 1, bytes_read_this_iter, fho) !=
                bytes_read_this_iter)
            {
                log_error_msg(block->block_filename);
                ret_val = ERROR_WRITING_FILE;
                break;
            }

            /* Update offset and bytes_to_read */
            offset += bytes_read_this_iter;
            bytes_to_read -= bytes_read_this_iter;

            /* Exit the loop if done */
            if(bytes_to_read <= 0)
                break;
        }

        fclose(fho);
        fho = NULL;

        if(ret_val != CAL_SUCCESS)
            break;

        /* Go for next block */
        block = block->next;
    }

    fclose(fh);
    fh = NULL;

    return ret_val;
}

/**
 * Returns length field bytes of aead structure on give message length
 *
 * @par Purpose
 *
 * Based on input msg_bytes function returns the length field of
 * aead structure
 *
 * @par Operation
 *
 * @param[in] msg_bytes, message length in bytes
 *
 * @retval length in bytes
 */
size_t length_field_bytes(size_t msg_bytes)
{
    uint32_t len_bytes;

    /* Note: value of L = 1 is reserved */
    if (msg_bytes < BYTES_64KB)
    {
        len_bytes = 2;
    }
    else if (msg_bytes < BYTES_16MB)
    {
        len_bytes = 3;
    }
    else
    {
        len_bytes = 4;
    }
    return len_bytes;
}

/**
 * Generate HAB AEAD data using nonce and mac bytes
 *
 * @par Purpose
 *
 * Generates HAB AEAD data structure using nonce and MAC bytes and saves the
 * binary AEAD structure into the given out_file.
 *
 * @par Operation
 *
 * @param[in] nonce nonce data bytes
 *
 * @param[in] nonce_bytes size of nonce in bytes
 *
 * @param[in] mac MAC
 *
 * @param[in] mac_bytes size of MAC in bytes
 *
 * @param[in] out_file file name to save the AEAD data bytes
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval Errors return errors on failures due to any file io or mem alloc
 */
int32_t generate_and_save_aead_data(uint8_t * nonce,
                                    size_t nonce_bytes,
                                    uint8_t * mac,
                                    size_t mac_bytes,
                                    const char * out_file)
{
    int32_t ret_val = SUCCESS;

    FILE * fh = NULL;

    size_t aead_bytes;

    uint8_t * aead = NULL;

    do {
        /* Calculate AEAD bytes */
        aead_bytes = HDR_BYTES + nonce_bytes + mac_bytes;

        /* Allocate buffer for aead data */
        aead = malloc(aead_bytes);
        if(aead == NULL)
        {
            ret_val = ERROR_INSUFFICIENT_MEMORY;
            break;
        }

        /* Construct aead data structure */
        aead[0] = 0x00;                   /**< first byte of header always 0 */
        aead[1] = nonce_bytes;                /**< second byte size of nonce */
        aead[2] = 0x00;                             /**< third byte always 0 */
        aead[3] = mac_bytes;                          /**< 4th byte mac size */
        memcpy(&aead[4], nonce, nonce_bytes);    /**< next comes nonce_bytes */
        memcpy(&aead[4+nonce_bytes], mac, mac_bytes);   /**< and finally mac */

        /* Write aead data to FILE_SIG_IMG_DATA */
        fh = fopen(out_file, "wb");
        if(fh == NULL)
        {
            log_error_msg(FILE_SIG_IMG_DATA);
            ret_val = ERROR_OPENING_FILE;
            break;
        }
        if(fwrite(aead, 1, aead_bytes, fh) != aead_bytes)
        {
            log_error_msg(FILE_SIG_IMG_DATA);
            ret_val = ERROR_WRITING_FILE;
            break;
        }
    }while (0);

    if(fh)
        fclose(fh);
    if(aead)
        free(aead);

    return ret_val;
}

/**
 * Updates g_csf_buffer with authenticate data command for decryption
 *
 * @par Purpose
 *
 * Collects necessary arguments from csf file, validate the arguments,
 * set default values for arguments if missing from csf file.
 * Updates g_csf_buffer with authenticate data command.
 * Encrypts data and saves output MAC data with hdr in the CSF and
 * points aut_start of aut_dat cmd to location of MAC data in CSF.
 *
 * @par Operation
 *
 * @param[in] cmd, the csf command
 *
 * @retval #SUCCESS  completed its task successfully
 *
 * @retval Errors returned by create_sig_file and save_file_data functions
 */
int32_t cmd_handler_decryptdata(command_t* cmd)
{
    int32_t ret_val = SUCCESS;     /**< Used for return value */
    int32_t engine = -1;           /**< Holds engine argument value */
    int32_t engine_cfg = -1;       /**< Holds engine config argument value */
    int32_t vfy_index = -1;        /**< Holds verify index argument value */
    int32_t mac_bytes = -1;        /**< Holds MAC length argument value */
    block_t *block = NULL;         /**< Holds address of block list argument */
    block_t *tmpblock = NULL;      /**< Holds address of block list argument */
    size_t blocks_data_size=0;    /**< Bytes occupied by block data in cmd */
    int32_t cmd_len = 0;           /**< Used to track command length */
    uint8_t *mac = NULL;            /**< Ptr to MAC */
    size_t nonce_bytes = 0;       /**< Length of nonce */
    uint8_t nonce[MAX_NONCE_BYTES];/**< Buffer to hold nonce bytes */

    /* The Decrypt Data command is invalid when AHAB is targeted */
    if (TGT_AHAB == g_target)
    {
        log_cmd(cmd->type, STR_ILLEGAL);
        return ERROR_INVALID_COMMAND;
    }

    /* This command is supported from HAB 4.1 onwards */
    if(g_hab_version <= HAB4)
    {
        ret_val = ERROR_INVALID_COMMAND;
        return ret_val;
    }

    /* get the arguments */
    ret_val = process_authenticatedata_arguments(cmd, &block,
        &vfy_index, &engine, &engine_cfg, NULL, NULL, (size_t *)&mac_bytes, NULL, NULL, NULL);
    if(ret_val != SUCCESS)
    {
        return ret_val;
    }

    /* generate authenticate data command */
    do {

        /* Check for mandatory arguments */
        if(vfy_index == -1)
        {
            log_arg_cmd(VerificationIndex, NULL, cmd->type);
            ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
            break;
        }

        if(block == NULL )
        {
            log_arg_cmd(Blocks, NULL, cmd->type);
            ret_val = ERROR_INSUFFICIENT_ARGUMENTS;
            break;
        }

        /* validate the arguments */
        ret_val = validate_block_arguments(block);
        if(ret_val != SUCCESS)
        {
            break;
        }

        if((engine == HAB_ENG_ANY) && (engine_cfg != 0))
        {
            log_arg_cmd(EngineName, STR_ENG_ANY_CFG_NOT_ZERO, cmd->type);
            ret_val = ERROR_INVALID_ARGUMENT;
            break;
        }

        if(vfy_index >= HAB_KEY_SECRET_MAX)
        {
            log_arg_cmd(VerificationIndex, NULL, cmd->type);

            ret_val = ERROR_INVALID_ARGUMENT;
            break;
        }

        /*
         * Check to make sure vfy_index is same as tgt_index
         * specified with previous install secret key command
         */
        if (g_aes_keys[vfy_index].key_file == NULL)
        {
            log_arg_cmd(VerificationIndex, NULL, cmd->type);
            ret_val = ERROR_INVALID_ARGUMENT;
            break;
        }

        /* set the defaults if not provided */
        if(engine == -1)
            engine = g_engine;
        if(engine_cfg == -1)
            engine_cfg = g_engine_config;
        if(mac_bytes == -1)
            mac_bytes = 16;

        /* Valid mac_bytes are 4, 6, 8, 10, 12, 14 and 16 */
        if((mac_bytes < 4) || (mac_bytes > 16) || (mac_bytes % 2))
        {
            log_arg_cmd(MacBytes, NULL, cmd->type);

            ret_val = ERROR_INVALID_ARGUMENT;
            break;
        }

        /* Allocate MAC */
        mac = malloc(mac_bytes);
        if(mac == NULL)
        {
            ret_val = ERROR_INSUFFICIENT_MEMORY;
            break;
        }

        /* Get the engine config value for HAB4 */
        engine_cfg = get_hab4_engine_config(engine, (const eng_cfg_t)engine_cfg);

        /* Above returns only one of ERROR_INVALID_ENGINE_CFG or
         * ERROR_INVALID_ENGINE, return on error
         */
        if(engine_cfg == ERROR_INVALID_ENGINE_CFG)
        {

            log_arg_cmd(EngineConfiguration, NULL, cmd->type);
            ret_val = ERROR_INVALID_ARGUMENT;
            break;
        }
        if(engine_cfg == ERROR_INVALID_ENGINE)
        {
            log_arg_cmd(EngineName, NULL, cmd->type);
            ret_val = ERROR_INVALID_ARGUMENT;
            break;
        }

        /*
         * Validate block len to be a multiple of 16
         */
        tmpblock = block;
        while(tmpblock != NULL)
        {
            /* Validate block length to be an exact  multiple of 16 */
            if(tmpblock->length % 16)
            {
                log_arg_cmd(Blocks, "Block length not a multiple of 16",
                    cmd->type);
                ret_val = ERROR_INVALID_ARGUMENT;
                break;
            }
            tmpblock = tmpblock->next;
        }

        if(ret_val != SUCCESS)
            break;

        /* generate AUT_IMG command */
        ret_val = hab4_authenticate_data(SIG_FMT_AEAD, engine, engine_cfg,
            vfy_index, block, &g_csf_buffer[g_csf_buffer_index], &cmd_len,
            &blocks_data_size);
        if(ret_val != SUCCESS)
        {
            break;
        }

        /* Save the offset to store aead data in the output buffer */
        cmd->start_offset_cert_sig = g_csf_buffer_index +
            HAB4_AUT_DAT_CMD_SIG_OFFSET;

        g_csf_buffer_index += cmd_len;

        /* Calculate nonce bytes */
        nonce_bytes = AES_BLOCK_BYTES - FLAG_BYTES -
            length_field_bytes(blocks_data_size);

        /* Read data from blocks for encryption into FILE_PLAIN_DATA */
        ret_val = read_data_to_encrypt_from_blocks(FILE_PLAIN_DATA, block);
        if(ret_val != SUCCESS)
        {
            break;
        }

        /*
         * Call backend to encrypt data from FILE_PLAIN_DATA and save
         * encrypted data in FILE_ENCRYPTED_DATA
         */
        ret_val = gen_auth_encrypted_data(FILE_PLAIN_DATA,
                    FILE_ENCRYPTED_DATA,
                    AES_CCM,
                    NULL,
                    0,
                    nonce,
                    nonce_bytes,
                    mac,
                    mac_bytes,
                    g_aes_keys[vfy_index].key_bytes,
                    g_cert_dek,
                    g_aes_keys[vfy_index].key_file,
                    g_reuse_dek);
        if(ret_val != CAL_SUCCESS)
        {
            log_error_msg("CRYPTO API Failure");
            break;
        }

        /* Replace plain text in blocks filenames with encrypted data */
        ret_val = write_encrypted_data_to_blocks(FILE_ENCRYPTED_DATA, block);
        if(ret_val != CAL_SUCCESS)
        {
            break;
        }

        /* Generate AEAD using nonce and mac and save the result in file */
        ret_val = generate_and_save_aead_data(nonce, nonce_bytes, mac,
            mac_bytes, FILE_SIG_IMG_DATA);
        if(ret_val != SUCCESS)
        {
            break;
        }

        /* Attach the aead data from file to command */
        ret_val = save_file_data(cmd, FILE_SIG_IMG_DATA, NULL, 0,
            (g_hab_version >= HAB4), NULL, NULL, g_hash_alg);
        if(ret_val != SUCCESS)
        {
            break;
        }

        /*
         * Set the key index to NULL to prevent same secret key used by
         * another decrypt data command.
         */
        g_aes_keys[vfy_index].key_file = NULL;
        g_aes_keys[vfy_index].key_bytes = 0;
    } while(0);

    return ret_val;
}

