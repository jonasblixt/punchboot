/*===========================================================================*/
/**
    @file    csf_cmd_misc.c

    @brief   Code signing tool's CSF command handler for commands header,
             NOP, SET, INIT and UNLOCK.

@verbatim
=============================================================================

              Freescale Semiconductor
        (c) Freescale Semiconductor, Inc. 2011,2012. All rights reserved.
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
#include <unistd.h> /* access */
#define HAB_FUTURE
#include "hab_cmd.h"
#include <openssl/pem.h>
#include "csf.h"

/*===========================================================================
                              MACROS USED LOCALLY
=============================================================================*/
#define CSF_HDR_HAB4_LENGTH (4)
#define CSF_HDR_HAB4_TAG_OFFSET (0)
#define CSF_HDR_HAB4_VERSION_OFFSET (3)

#define CSF_HDR_CSF_TYPE_OFFSET (3)
#define CSF_HDR_UID_LEN_OFFSET (4)
#define CSF_HDR_UID_OFFSET (5)


#define BYTES_IN_WORD             4    /**< Number of bytes in a word */

#define ERR_IF_INIT_MULT_TIMES(flag)                                     \
    if (flag)                                                            \
    {                                                                    \
        log_arg_cmd(arg->type, " argument already specified", cmd->type);\
        return ERROR_INVALID_ARGUMENT;                                   \
    }                                                                    \
    flag = true

#define ERR_IF_UNS_ARG(flag, arg)                                        \
        if (flag)                                                        \
        {                                                                \
            log_arg_cmd(arg, STR_ILLEGAL, cmd->type);                    \
            return ERROR_UNSUPPORTED_ARGUMENT;                           \
        }
/*===========================================================================
                             LOCAL FUNCTION DECLARATION
=============================================================================*/
static int32_t process_setengine_arguments(command_t* cmd, int32_t *engine,
            int32_t *engine_cfg, int32_t *hash_alg);

static int32_t cmd_handler_init_unlock(command_t* cmd, uint8_t cmd_id);
/*===========================================================================
                             LOCAL FUNCTION DEFINITIONS
=============================================================================*/

/**
 * process arguments list for setengine csf command
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
 * @param[out] engine, returns value for arg ENGINE
 *
 * @param[out] engine_cfg, returns value for arg ENGINECONFIGURATION
 *
 * @param[out] hash_alg, returns value for arg HASHALGORITHM
 *
 * @retval #SUCCESS  completed its task successfully
 */
static int32_t process_setengine_arguments(command_t* cmd, int32_t *engine,
            int32_t *engine_cfg, int32_t *hash_alg)
{
    uint32_t i;
    argument_t *arg = cmd->argument;     /**< Ptr to argument_t */

    for(i=0; i<cmd->argument_count; i++)
    {
        switch((arguments_t)arg->type)
        {
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
	        if (arg->value_type == NUMBER_TYPE)
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
        case HashAlgorithm:
            if(hash_alg != NULL)
                *hash_alg = arg->value.keyword->unsigned_value;
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
 * handler for header command
 *
 * @par Purpose
 *
 * For HAB4 this command saves off the default values for arguments.
 *
 * @par Operation
 *
 * @param[in] csf header cmd
 *
 * @retval #SUCCESS
 */
int32_t cmd_handler_header(command_t* cmd)
{
    uint32_t i;              /**< Loop index */
    argument_t *arg;         /**< Ptr to command's argument */
    uint8_t  version = 0;

    bool flag_target   = false;
    bool flag_version  = false;
    bool flag_mode     = false;
    bool flag_hash_alg = false;
    bool flag_eng      = false;
    bool flag_eng_cfg  = false;
    bool flag_crt_fmt  = false;
    bool flag_sig_fmt  = false;

    arg = cmd->argument;
    for(i=0; i<cmd->argument_count; i++)
    {
        switch((arguments_t)arg->type)
        {
        case Target:
            ERR_IF_INIT_MULT_TIMES(flag_target);
            g_target = arg->value.keyword->unsigned_value;
            break;

        case Version:
            ERR_IF_INIT_MULT_TIMES(flag_version);
            version = (arg->value.pair->first << 4) |
                arg->value.pair->second;
            break;

        case Mode:
            ERR_IF_INIT_MULT_TIMES(flag_mode);
            g_mode = arg->value.keyword->unsigned_value;
            break;

        case HashAlgorithm:
            ERR_IF_INIT_MULT_TIMES(flag_hash_alg);
            g_hash_alg = arg->value.keyword->unsigned_value;
            break;

        case EngineName:
            ERR_IF_INIT_MULT_TIMES(flag_eng);
            g_engine = arg->value.keyword->unsigned_value;
            break;

        case EngineConfiguration:
            ERR_IF_INIT_MULT_TIMES(flag_eng_cfg);
            /* Engine configuration could be number or keyword */
            if (arg->value_type == NUMBER_TYPE)
                g_engine_config = arg->value.number->num_value;
            else
                g_engine_config = arg->value.keyword->unsigned_value;
            break;

        case CertificateFormat:
            ERR_IF_INIT_MULT_TIMES(flag_crt_fmt);
            g_cert_format = arg->value.keyword->unsigned_value;
            break;

        case SignatureFormat:
            ERR_IF_INIT_MULT_TIMES(flag_sig_fmt);
            g_sig_format = arg->value.keyword->unsigned_value;
            break;

        default:
            return ERROR_INVALID_ARGUMENT;
        };

        arg = arg->next; /* go to next argument */
    }

    /* Validate arguments */

    /* If no target is specified, HAB is default */
    if(TGT_UNDEF == g_target)
        g_target = TGT_HAB;

    if (MODE_UNDEF == g_mode)
        g_mode = MODE_NOMINAL;
    else if (MODE_HSM == g_mode)
    {
        if (-1 != access(SIG_REQ_FILENAME, F_OK))
        {
            if (0 != remove(SIG_REQ_FILENAME))
            {
                log_arg_cmd(Mode, SIG_REQ_FILENAME, cmd->type);
                return ERROR_OPENING_FILE;
            }
        }
    }

    /* If AHAB is not targeted */
    if (TGT_AHAB != g_target)
    {
        g_hab_version = version;

        if (g_hab_version == 0)
        {
            log_arg_cmd(Version, NULL, cmd->type);
            return ERROR_INSUFFICIENT_ARGUMENTS;
        }

        if(g_hab_version >= HAB4)
        {
            g_csf_buffer[CSF_HDR_HAB4_TAG_OFFSET] = HAB_TAG_CSF;
            g_csf_buffer[CSF_HDR_HAB4_VERSION_OFFSET] = g_hab_version;
        }

        ERR_IF_UNS_ARG(flag_mode, Mode);

        if (g_engine == HAB_ENG_ANY && g_engine_config != 0)
        {
            log_arg_cmd(EngineName, STR_ENG_ANY_CFG_NOT_ZERO, cmd->type);
            return ERROR_INVALID_ARGUMENT;
        }

        if (g_hab_version >= HAB4)
        {
            /*
            * Set default for globals if not specified.
            * And return an error if the sig format is not supported.
            */
            if(SIG_FMT_UNDEF == g_sig_format)
                g_sig_format = SIG_FMT_CMS;
            else if (g_sig_format != SIG_FMT_CMS)
            {
                log_arg_cmd(SignatureFormat, " different from CMS"STR_ILLEGAL, cmd->type);
                return ERROR_UNSUPPORTED_ARGUMENT;
            }

            /*
            * Set default for globals if not specified.
            * And return an error if the cert format is not supported.
            */
            if(g_cert_format == 0)
                g_cert_format = HAB_PCL_X509;

            if(g_hash_alg == 0)
                g_hash_alg = HAB_ALG_SHA256;

            g_csf_buffer_index = CSF_HDR_HAB4_LENGTH ;
        }
    }
    else
    {
        g_ahab_version = version;

        ERR_IF_UNS_ARG(flag_hash_alg, HashAlgorithm);
        ERR_IF_UNS_ARG(flag_eng,      EngineName);
        ERR_IF_UNS_ARG(flag_eng_cfg,  EngineConfiguration);
        ERR_IF_UNS_ARG(flag_crt_fmt,  CertificateFormat);
        ERR_IF_UNS_ARG(flag_sig_fmt,  SignatureFormat);
    }

    return SUCCESS;
}

/**
 * handler for NOP command
 *
 * @par Purpose
 *
 * Generates NOP command. Support in HAB4
 *
 * @par Operation
 *
 * @param[in] cmd
 *
 * @retval #SUCCESS
 */
int32_t cmd_handler_nop(command_t* cmd)
{
    uint8_t nop_cmd[] = {
        NOP()
    };                            /**< Macro will output NOP
                                    command bytes in nop_cmd buffer */

    UNUSED(cmd);

    /* The NOP command is invalid when AHAB is targeted */
    if (TGT_AHAB == g_target)
    {
        log_cmd(cmd->type, STR_ILLEGAL);
        return ERROR_INVALID_COMMAND;
    }

    if(g_hab_version >= HAB4)
    {
        memcpy(&g_csf_buffer[g_csf_buffer_index], nop_cmd, NOP_BYTES);
        g_csf_buffer_index += NOP_BYTES;
    }

    return SUCCESS;
}


/**
 * handler for Set Engine command
 *
 * @par Purpose
 *
 * Generates Set Engine command (HAB4 only). It process argument list, validate
 * the arguments needed for the command and outputs the command bytes into
 * csf buffer.
 *
 * @par Operation
 *
 * @param[in] cmd
 *
 * @retval #SUCCESS
 */
int32_t cmd_handler_setengine(command_t* cmd)
{
    int32_t hash_alg = -1;      /**< Holds the value of hash algorithm argument */
    int32_t engine = -1;        /**< Holds the value of engine argument */
    int32_t engine_cfg = -1;    /**< Holds the value of engine config argument */
    int32_t ret_val = SUCCESS;  /**< Holds return value */

    /* The Set Engine command is invalid when AHAB is targeted */
    if (TGT_AHAB == g_target)
    {
        log_cmd(cmd->type, STR_ILLEGAL);
        return ERROR_INVALID_COMMAND;
    }

    if(g_hab_version < HAB4)
    {
        log_cmd(cmd->type, STR_ILLEGAL);
        return ERROR_INVALID_COMMAND;
    }
    ret_val = process_setengine_arguments(cmd, &engine, &engine_cfg, &hash_alg);
    if(ret_val != SUCCESS)
    {
        return ret_val;
    }

    /*
     * Return an error as the hash algorithm and engine must be provided
     * as arguments to the command Set Engine.
     */
    if(engine == -1)
    {
        log_arg_cmd(EngineName, NULL, cmd->type);
        return ERROR_INSUFFICIENT_ARGUMENTS;
    }
    if(hash_alg == -1)
    {
        log_arg_cmd(HashAlgorithm, NULL, cmd->type);
        return ERROR_INSUFFICIENT_ARGUMENTS;
    }

    /* use default from header if not provided */
    if(engine_cfg == -1)
    {
        engine_cfg = g_engine_config;
    }

    engine_cfg = get_hab4_engine_config(engine, (const eng_cfg_t)engine_cfg);

    /* Above returns only one of ERROR_INVALID_ENGINE_CFG or
     * ERROR_INVALID_ENGINE, return on error
     */
    if(engine_cfg == ERROR_INVALID_ENGINE_CFG)
    {
        log_arg_cmd(EngineConfiguration, NULL, cmd->type);
        return ERROR_INVALID_ARGUMENT;
    }
    if(engine_cfg == ERROR_INVALID_ENGINE)
    {
        log_arg_cmd(EngineName, NULL, cmd->type);
        return ERROR_INVALID_ARGUMENT;
    }

    {
        uint8_t set_eng[] = {
            SET_ENG(hash_alg, engine, engine_cfg)
        };                /**< Macro will output set eng
                                command bytes in set_eng buffer */

        memcpy(&g_csf_buffer[g_csf_buffer_index], set_eng, SET_ENG_BYTES);
        g_csf_buffer_index += SET_ENG_BYTES;
    }

    return SUCCESS;
}

/**
 * common handler for Init and Unlock commands
 *
 * @par Purpose
 *
 * Generates CSF command Init or Unlock. The actual hab cmd id is passed in as
 * an argument. Processes arguments, validate them and generates cmd bytes.
 *
 * @par Operation
 *
 * @param[in] cmd
 *
 * @param[in] hab command id
 *
 * @retval #SUCCESS
 */
static int32_t cmd_handler_init_unlock(command_t* cmd, uint8_t hab_cmd_id)
{
    uint32_t i;                /**< Loop index */
    int32_t engine = -1;       /**< Holds the value of engine argument */
    keyword_t *feature = NULL; /**< Holds features list argument */
    int32_t num_features = 0;  /**< Count of features in the argument */
    number_t *uid = NULL;      /**< Holds UID argument */
    int32_t uid_bytes = 0;     /**< Length of UID in bytes */
    argument_t *arg;           /**< Ptr to command's argument */
    int32_t cmd_len;           /**< Used to keep track of cmd length */
    uint32_t features = 0;     /**< All features are OR'ed into this word */

    arg = cmd->argument;

    for(i=0; i<cmd->argument_count; i++)
    {
        switch((arguments_t)arg->type)
        {
        case EngineName:
            engine = arg->value.keyword->unsigned_value;
            break;
        case Features:
            feature = arg->value.keyword;
            num_features = arg->value_count;
            break;
        case UID:
            uid = arg->value.number;
            uid_bytes = arg->value_count;
            break;
        default:
            log_arg_cmd(arg->type, NULL, cmd->type);
            return ERROR_INVALID_ARGUMENT;
        };

        arg = arg->next; /* go to next argument */
    }
    if(engine == -1)
    {
        log_arg_cmd(EngineName, NULL, cmd->type);
        return ERROR_INSUFFICIENT_ARGUMENTS;
    }

    /* OR'ed all feature values in features */
    while(feature != NULL)
    {
        features |= feature->unsigned_value;
        feature = feature->next;
    }

    /* Set flags if this is Unlock RNG or Init RNG command */
    if (engine == HAB_ENG_CAAM) {
        if ((hab_cmd_id == HAB_CMD_UNLK) && (features & HAB_CAAM_UNLOCK_RNG)) {
            g_unlock_rng = 1;
        }

        if ((hab_cmd_id == HAB_CMD_INIT) && (features & HAB_CAAM_INIT_RNG)) {
            g_init_rng = 1;
        }
    }

    /*
     * Add checks to make sure UID is provided for required features of an engine
     * Only OCOTP engine has UID mandatory for certain features to unlock
     */
    if(engine == HAB_ENG_OCOTP)
    {
        /* UID is needed to unlock SCS, JTAG and FIELD_RETURN */
        if(((features & HAB_OCOTP_UNLOCK_SCS) ||
            (features & HAB_OCOTP_UNLOCK_JTAG) ||
            (features & HAB_OCOTP_UNLOCK_FIELD_RETURN)) &&
            (uid == NULL))
        {
            log_arg_cmd(UID, NULL, cmd->type);
            return ERROR_INSUFFICIENT_ARGUMENTS;
        }
        /* UID must not be provided to unlock SRK_REVOKE feature */
        if((features == HAB_OCOTP_UNLOCK_SRK_REVOKE) && (uid != NULL))
        {
            log_arg_cmd(UID, NULL, cmd->type);
            return ERROR_INVALID_ARGUMENT;
        }
    }
    /*
     * Total cmd len with uid bytes. Add word length if feature is specified;
     * Features are all OR'ed into one single word (4 bytes)
     */
    cmd_len = (HDR_BYTES + uid_bytes);
    if(num_features > 0)
    {
        cmd_len += BYTES_IN_WORD;
    }

    {
        /* Append first 4 bytes for the command to csf buffer */
        uint8_t init[] = {
            HDR(hab_cmd_id, cmd_len, engine)
        };                            /**< Macro will output init
                                     command bytes in init buffer */
        memcpy(&g_csf_buffer[g_csf_buffer_index], init, HDR_BYTES);
        g_csf_buffer_index += HDR_BYTES;

        /* Push OR'ed features into buffer */
        if(num_features > 0)
        {
            uint8_t feature_value[] = {
                EXPAND_UINT32(features)
                    };                /**< Macro will 4 bytes for
                                      feature in feature_value buffer */
            memcpy(&g_csf_buffer[g_csf_buffer_index], feature_value, 4);
            g_csf_buffer_index += 4;
        }
        /* Add UID for only unlock cmd */
        if (hab_cmd_id == HAB_CMD_UNLK)
        {
            while(uid)
            {
                g_csf_buffer[g_csf_buffer_index++] = uid->num_value;
                uid = uid->next;
            }
        }
    }

    return SUCCESS;
}

/**
 * common handler for Init command
 *
 * @par Purpose
 *
 * Calls cmd_handler_init_unlock to generate init command with cmd id
 * HAB_CMD_INIT
 *
 * @par Operation
 *
 * @param[in] cmd
 *
 * @retval #SUCCESS
 *
 * @retval #ERROR_INVALID_COMMAND
 */
int32_t cmd_handler_init(command_t* cmd)
{
    /* The Init command is invalid when AHAB is targeted */
    if (TGT_AHAB == g_target)
    {
        log_cmd(cmd->type, STR_ILLEGAL);
        return ERROR_INVALID_COMMAND;
    }

    if(g_hab_version < HAB4)
    {
        log_cmd(cmd->type, STR_ILLEGAL);
        return ERROR_INVALID_COMMAND;
    }
    return cmd_handler_init_unlock(cmd, HAB_CMD_INIT);
}

/**
 * common handler for Unlock command
 *
 * @par Purpose
 *
 * Calls cmd_handler_init_unlock to generate unlock command with cmd id
 * HAB_CMD_UNLK
 *
 * @par Operation
 *
 * @param[in] cmd
 *
 * @retval #SUCCESS
 *
 * @retval #ERROR_INVALID_COMMAND
 */
int32_t cmd_handler_unlock(command_t* cmd)
{
    /* The Install Key command is invalid when AHAB is targeted */
    if (TGT_AHAB == g_target)
    {
        log_cmd(cmd->type, STR_ILLEGAL);
        return ERROR_INVALID_COMMAND;
    }

    if(g_hab_version < HAB4)
    {
        log_cmd(cmd->type, STR_ILLEGAL);
        return ERROR_INVALID_COMMAND;
    }

    return cmd_handler_init_unlock(cmd, HAB_CMD_UNLK);
}
