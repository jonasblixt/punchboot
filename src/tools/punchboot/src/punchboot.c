/**
 * Punch BOOT bootloader cli
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <pb/pb.h>
#include <pb/params.h>
#include <pb/crypto.h>
#include <uuid.h>
#include <3pp/bearssl/bearssl_hash.h>

#define INI_IMPLEMENTATION
#include "3pp/ini.h"
#include "crc.h"
#include "utils.h"
#include "transport.h"
#include "recovery_protocol.h"

#define PB_MAX_PARAMS 128

static struct param params[PB_MAX_PARAMS];

static uint32_t check_auth(void)
{
    uint32_t err;
    bool auth;

    err = pb_is_auhenticated(&auth);

    if (err != PB_OK)
        return err;

    if (!auth)
    {
        printf("Not authenticated\n");
        return PB_ERR;
    }

    return PB_OK;
}

static int load_params(const char *fn)
{
    FILE* fp = fopen(fn, "r");
    struct stat finfo;
    void *memctx = NULL;
    int size = -1;
    char *data = NULL;
    int index;
    uint32_t n;
    const char *section_name;

    if (fp == NULL)
    {
        printf("Error: could not read %s\n", fn);
        return -1;
    }

    printf("Loading parameter file: '%s'\n", fn);

    stat(fn, &finfo);
    size = finfo.st_size;
    data = (char*) malloc(size + 1);
    size_t read_sz = fread(data, 1, size, fp);
    data[ size ] = '\0';
    fclose(fp);

    if (read_sz != size)
    {
        printf("Error: could not read file\n");
        return -1;
    }
    ini_t* ini = ini_load(data, memctx);
    free(data);

    n = 0;
    uint32_t param_count = 0;
    do
    {
        section_name = ini_section_name(ini, n);

        if (section_name)
        {
            const char *param_value = NULL;
            const char *param_type = NULL;
            const char *param_name = NULL;

            if (strcmp(section_name, "parameter") == 0)
            {
                index = ini_find_property(ini, n, "value", 0);

                if (index >= 0)
                {
                    param_value = ini_property_value(ini, n, index);
                }

                index = ini_find_property(ini, n, "type", 0);

                if (index >= 0)
                {
                    param_type = ini_property_value(ini, n, index);
                }

                index = ini_find_property(ini, n, "name", 0);

                if (index >= 0)
                {
                    param_name = ini_property_value(ini, n, index);
                }

                if (param_value && param_type && param_name)
                {
                    printf("Parameter:\n");
                    printf(" name: %s\n", param_name);
                    printf(" type: %s\n", param_type);
                    printf(" value: %s\n", param_value);

                    uint8_t *u08_ptr = (uint8_t *) params[param_count].data;
                    uint16_t *u16_ptr = (uint16_t *) params[param_count].data;
                    uint32_t *u32_ptr = (uint32_t *) params[param_count].data;
                    uint64_t *u64_ptr = (uint64_t *) params[param_count].data;

                    memcpy(params[param_count].identifier, param_name,
                        strlen(param_name) > PB_PARAM_MAX_IDENT_SIZE ?
                            PB_PARAM_MAX_IDENT_SIZE:strlen(param_name));

                    if (strcmp(param_type, "u08") == 0)
                    {
                        params[param_count].kind = PB_PARAM_U08;
                        (*u08_ptr) = strtol(param_value, NULL, 16);
                    }
                    else if (strcmp(param_type, "u16") == 0)
                    {
                        params[param_count].kind = PB_PARAM_U16;
                        (*u16_ptr) = strtol(param_value, NULL, 16);
                    }
                    else if (strcmp(param_type, "u32") == 0)
                    {
                        params[param_count].kind = PB_PARAM_U32;
                        (*u32_ptr) = strtol(param_value, NULL, 16);
                    }
                    else if (strcmp(param_type, "u64") == 0)
                    {
                        params[param_count].kind = PB_PARAM_U32;
                        (*u64_ptr) = strtol(param_value, NULL, 16);
                    }
                    else
                    {
                        return -1;
                    }

                    param_count++;
                    params[param_count].kind = PB_PARAM_END;
                }
            }
        }
        n++;
    } while (section_name);

    return PB_OK;
}


static void guid_to_uuid(uint8_t *guid, uint8_t *uuid)
{
    guid[0] = uuid[3];
    guid[1] = uuid[2];
    guid[2] = uuid[1];
    guid[3] = uuid[0];

    guid[4] = uuid[5];
    guid[5] = uuid[4];

    guid[6] = uuid[7];
    guid[7] = uuid[6];

    guid[8] = uuid[8];
    guid[9] = uuid[9];

    guid[10] = uuid[10];
    guid[11] = uuid[11];
    guid[12] = uuid[12];
    guid[13] = uuid[13];
    guid[14] = uuid[14];
    guid[15] = uuid[15];
}

static int print_gpt_table(void)
{
    struct gpt_primary_tbl gpt;
    int err;
    char str_type_uuid[37];
    uint8_t tmp_string[64];
    struct gpt_part_hdr *part;

    err = check_auth();

    if (err != PB_OK)
        return err;

    err = pb_get_gpt_table(&gpt);

    if (err != 0)
        return err;

    if (gpt.hdr.no_of_parts == 0)
        return -1;

    printf("GPT Table:\n");
    for (int i = 0; i < gpt.hdr.no_of_parts; i++)
    {
        part = &gpt.part[i];

        if (!part->first_lba)
            break;
        unsigned char guid[16];
        guid_to_uuid(guid, part->type_uuid);
        uuid_unparse_lower(guid, str_type_uuid);
        utils_gpt_part_name(part, tmp_string, 36);
        printf(" %i - [%16s] lba 0x%8.8lX - 0x%8.8lX, TYPE: %s\n", i,
                tmp_string,
                part->first_lba, part->last_lba,
                str_type_uuid);
    }

    return 0;
}

static void pb_print_param(struct param *p)
{
    printf("%-20s", p->identifier);

    switch (p->kind)
    {
        case PB_PARAM_U16:
        {
            uint16_t u16_val;
            param_get_u16(p, &u16_val);
            printf("0x%4.4X", u16_val);
        }
        break;
        case PB_PARAM_U32:
        {
            uint32_t u32_val;
            param_get_u32(p, &u32_val);
            printf("0x%8.8X", u32_val);
        }
        break;
        case PB_PARAM_STR:
        {
            char *str_val = (char *) p->data;
            printf("%s", str_val);
        }
        break;
        case PB_PARAM_UUID:
        {
            char *uuid_raw = (char *) p->data;
            char uuid_str[37];
            uuid_unparse_upper((const unsigned char*)uuid_raw, uuid_str);
            printf("%s", uuid_str);
        }
        break;
        default:
            printf("...");
    }

    printf("\n");
}

static uint32_t pb_display_device_info(void)
{
    uint32_t err = PB_ERR;
    char *version_string = NULL;
    struct param *params = NULL;

    err = pb_get_version(&version_string);

    if (err != PB_OK)
        goto display_info_error;

    if (version_string == NULL)
        goto display_info_error;

    printf("Device info:\n");
    printf(" Bootloader Version: %s\n", version_string);

    err = pb_read_params(&params);

    if (err != PB_OK)
        goto display_info_error;


    printf("\n");
    printf("Parameter           Value\n");
    printf("---------           -----\n");
    foreach_param(p, params)
        pb_print_param(p);

display_info_error:
    if (version_string)
        free(version_string);
    if (params)
        free(params);

    return err;
}

static void print_help_header(void)
{
    printf(" --- Punch BOOT " VERSION " ---\n\n");

    printf(" Common parameters:\n");
    printf("  punchboot <command> -u \"usb path\"           - Perform operations on device with 'usb path'\n");
    printf("  punchboot <command> -v                      - Verbose output\n\n");
}

static void print_boot_help(void)
{
    printf(" Bootloader:\n");
    printf("  punchboot boot -w -f <filename>                   - Install bootloader\n");
    printf("  punchboot boot -r                                 - Reset device\n");
    printf("  punchboot boot -b -s A or B [-v]                  - BOOT System A or B\n");
    printf("  punchboot boot -x -f <filename> [-s A or B] [-v]  - Load image to RAM and execute it\n");
    printf("  punchboot boot -a -s A, B or none                 - Activate system partition\n");
    printf("\n");
}


static void print_dev_help(void)
{
    printf(" Device:\n");
    printf("  punchboot dev -l                             - Display device information\n");
    printf("  punchboot dev -i [-f <filename>] [-y]        - Perform device setup\n");
    printf("  punchboot dev -w [-y]                        - Lock device setup\n");
    printf("  punchboot dev -a [-n <key id>] -f <filename> - Authenticate\n");
    printf("                -s <signature format>:<hash>\n");
    printf("  punchboot dev -b <arg0>,<arg1>,<arg2>,<arg3> - Issue board specific command\n");
    printf("\n");
}

static void print_part_help(void)
{
    printf(" Partition Management:\n");
    printf("  punchboot part -l                                 - List partitions\n");
    printf("  punchboot part -w -n <n> [-o <blk>] -f <filename> - Write 'fn' to partition 'n' with offset 'o'\n");
    printf("  punchboot part -c -n <n> [-o <blk>] -f <filename> - Verify data on partition\n");
    printf("  punchboot part -i                                 - Install default GPT table\n");
    printf("\n");
}

static void print_help(void) {
    print_help_header();
    print_boot_help();
    print_dev_help();
    print_part_help();
}

static uint32_t part_command(bool flag_list,
                             bool flag_write,
                             bool flag_c,
                             bool flag_index,
                             bool flag_install,
                             bool flag_reset,
                             uint64_t cmd_index,
                             uint64_t offset,
                             const char *fn)
{
    uint32_t err;

    err = check_auth();

    if (err != PB_OK)
        return err;

    if (flag_list)
    {
        printf("Listing partitions:\n");
        err = print_gpt_table();

        if (err != PB_OK)
            return err;
    }
    else if (flag_write && flag_index && fn)
    {
        printf("Writing %s to part %li with offset %li\n",
                            fn, cmd_index, offset);

        err = pb_flash_part(cmd_index, offset, fn);

        if (err != PB_OK)
            return err;
    }
    else if (flag_c && flag_index && fn)
    {
        printf("Verifying %s on part %li with offset %li\n",
                            fn, cmd_index, offset);

        err = pb_check_part(cmd_index, offset, fn);

        if (err != PB_OK)
            printf ("Verification FAILED\n");
        else
            printf ("Verification OK\n");

        if (err != PB_OK)
            return err;
    }
    else if (flag_install)
    {
        err = pb_install_default_gpt();

        if (err != PB_OK)
            return err;

    } else {
        print_help_header();
        print_part_help();
        err = PB_OK;
        return err;
    }

    if (flag_reset)
        err = pb_reset();

    return err;
}

static uint32_t boot_command(bool flag_s,
                             bool flag_a,
                             bool flag_b,
                             bool flag_v,
                             bool flag_write,
                             bool flag_execute,
                             bool flag_reset,
                             const char *s_arg,
                             const char *fn)
{
    uint32_t err;
    uint32_t active_system = SYSTEM_A;

    err = check_auth();

    if (err != PB_OK)
        return err;

    if (!(flag_s | flag_a | flag_write | flag_execute | flag_reset ))
    {
        print_help_header();
        print_boot_help();
        err = PB_OK;
        return err;
    }

    if (flag_s)
    {
        int part = tolower(s_arg[0]);
        if (part == 'a')
            active_system = SYSTEM_A;
        else if (part == 'b')
            active_system = SYSTEM_B;
        else
            active_system = SYSTEM_NONE;
    }

    if (flag_a)
    {
        err = pb_set_bootpart(active_system);

        if (err != PB_OK)
            return err;
    }

    if (flag_b)
    {
        err = pb_boot_part(active_system, flag_v);

        if (err != PB_OK)
            return err;
    }

    if (flag_write)
    {
        err = pb_program_bootloader(fn);

        if (err != PB_OK)
            return err;
    }

    if (flag_execute)
    {
        err = pb_execute_image(fn, active_system, flag_v);

        if (err != PB_OK)
            return err;
    }

    if (flag_reset)
        err = pb_reset();

    return err;
}

static uint32_t dev_command(bool flag_list,
                            bool flag_install,
                            bool flag_y,
                            bool flag_write,
                            bool flag_s,
                            bool flag_a,
                            bool flag_b,
                            bool flag_index,
                            uint32_t cmd_index,
                            const char *extra_args,
                            const char *s_arg,
                            const char *fn)
{
    uint32_t err;

    if (flag_list)
    {
        err = pb_display_device_info();

        if (err != PB_OK)
            return err;
    }
    else if (flag_install)
    {
        err = check_auth();

        if (err != PB_OK)
            return err;

        printf("Performing device setup\n");

        char confirm_input[5];

        if (!flag_y)
        {
            printf("\n\nWARNING: This is a permanent change, writing fuses " \
                    "can not be reverted. This could brick your device.\n"
                    "\n\nType 'yes' + <Enter> to proceed: ");
            if (fgets(confirm_input, 5, stdin) != confirm_input)
                return PB_ERR;

            if (strncmp(confirm_input, "yes", 3)  != 0)
            {
                printf("Aborted\n");
                return PB_ERR;
            }
        }

        bzero(params, sizeof(struct param) * PB_MAX_PARAMS);

        if (fn != NULL)
        {
            err = load_params(fn);

            if (err != PB_OK)
                return err;
        }
        else
            printf("No parameter file supplied\n");

        err = pb_recovery_setup(params);

        if (err != PB_OK)
        {
            printf("ERROR: Something went wrong\n");
            return err;
        }

        printf("Success\n");

    } else if (flag_write) {
        err = check_auth();

        if (err != PB_OK)
            return err;

        char confirm_input[5];

        if (!flag_y)
        {
            printf("\n\nWARNING: This is a permanent change, writing fuses " \
                    "can not be reverted. This could brick your device.\n"
                    "\n\nType 'yes' + <Enter> to proceed: ");
            if (fgets(confirm_input, 5, stdin) != confirm_input)
                return PB_ERR;

            if (strncmp(confirm_input, "yes", 3)  != 0)
            {
                printf("Aborted\n");
                return PB_ERR;
            }
        }
        err = pb_recovery_setup_lock();

        if (err != PB_OK)
        {
            printf("ERROR: Something went wrong\n");
            return PB_ERR;
        }
        printf("Success");
    }
    else if (flag_a && flag_s)
    {
        uint32_t key_index = 0;

        if (flag_index)
            key_index = cmd_index;

        char delim[] = ":";
        char *tok = strtok((char *)s_arg, delim);

        if (tok == NULL)
        {
            err = PB_ERR;
            return err;
        }

        printf("Signature format: %s\n", tok);

        uint32_t signature_kind = 0;

        if (strcmp(tok, "secp256r1") == 0)
            signature_kind = PB_SIGN_NIST256p;
        else if (strcmp(tok, "secp384r1") == 0)
            signature_kind = PB_SIGN_NIST384p;
        else if (strcmp(tok, "secp521r1") == 0)
            signature_kind = PB_SIGN_NIST521p;
        else if (strcmp(tok, "RSA4096") == 0)
            signature_kind = PB_SIGN_RSA4096;
        else
        {
            printf("Error: Invalid signature format\n");
            err = PB_ERR;
            return err;
        }


        tok = strtok(NULL, delim);

        if (tok == NULL)
        {
            err = PB_ERR;
            return err;
        }

        printf("Hash: %s\n", tok);

        uint32_t hash_kind = 0;

        if (strcmp(tok, "sha256") == 0)
            hash_kind = PB_HASH_SHA256;
        else if (strcmp(tok, "sha384") == 0)
            hash_kind = PB_HASH_SHA384;
        else if (strcmp(tok, "sha512") == 0)
            hash_kind = PB_HASH_SHA512;
        else
        {
            printf("Error: Invalid hash\n");
            err = PB_ERR;
            return err;
        }


        printf("Authenticating using key index %u and '%s'\n",
                key_index, fn);

        err = pb_recovery_authenticate(key_index, fn, signature_kind,
                                                      hash_kind);

        if (err == PB_ERR_FILE_NOT_FOUND)
            printf("Could not read '%s'\n", fn);

        if (err != PB_OK)
            printf("Authentication failed\n");
        else
            printf("Authentication successful\n");
    }
    else if (flag_b)
    {
        if (!extra_args)
        {
            printf ("Error no parameters supplied\n");
            return PB_ERR;
        }

        char delim[] = ",";
        char *data_in = strdup(extra_args);
        char *tok = strtok(data_in, delim);
        uint32_t args[4];
        uint32_t args_count = 0;

        while (tok != NULL)
        {
            args[args_count++] = strtol(tok, NULL, 0);
            tok = strtok(NULL, delim);

            if (args_count > 3)
                break;
        }
        
        err = pb_recovery_board_command(args[0], args[1], args[2], args[3]);

        if (err == PB_OK)
            printf("Command successful\n");
        else
            printf("Command failed\n");

        free(data_in);
    }
    else
    {
        print_help_header();
        print_dev_help();
        err = PB_OK;
    }

    return err;
}

int main(int argc, char **argv)
{
    extern char *optarg;
    int64_t offset = 0;
    uint8_t usb_path[16];
    uint8_t usb_path_count = 0;
    char c;
    int err = PB_ERR;

    if (argc <= 1)
    {
        print_help();
        exit(0);
    }

    int cmd_index = -1;
    bool flag_write = false;
    bool flag_list = false;
    bool flag_reset = false;
    bool flag_help = false;
    bool flag_s = false;
    bool flag_index = false;
    bool flag_install = false;
    bool flag_execute = false;
    bool flag_a = false;
    bool flag_b = false;
    bool flag_v = false;
    bool flag_y = false;
    bool flag_c = false;

    char *fn = NULL;
    char *cmd = argv[1];
    char *s_arg = NULL;

    while ((c = getopt (argc, argv, "hiwcraybu:xs:ln:f:vo:")) != -1)
    {
        switch (c)
        {
            case 'w':
                flag_write = true;
            break;
            case 'v':
                flag_v = true;
            break;
            case 'x':
                flag_execute = true;
            break;
            case 'a':
                flag_a = true;
            break;
            case 'b':
                flag_b = true;
            break;
            case 'c':
                flag_c = true;
            break;
            case 'y':
                flag_y = true;
            break;
            case 'r':
                flag_reset = true;
            break;
            case 'l':
                flag_list = true;
            break;
            case 'i':
                flag_install = true;
            break;
            case 'f':
                fn = optarg;
            break;
            case 'n':
                flag_index = true;
                cmd_index = strtol(optarg, NULL, 0);
            break;
            case 'o':
                offset = strtol(optarg, NULL, 0);
            break;
            case 'u':
            {
                usb_path_count = 0;
                char *usb_path_str = strdup(optarg);

                char delim[] = ":";
                char *tok = strtok(usb_path_str, delim);

                while (tok != NULL)
                {
                    usb_path[usb_path_count++] = strtol(tok, NULL, 0);
                    tok = strtok(NULL, delim);
                }

                free(usb_path_str);
            }
            break;
            case 's':
                flag_s = true;
                s_arg = optarg;
            break;
            case 'h':
                flag_help = true;
            break;
            default:
                exit(-1);
        }
    }

    if (flag_help)
    {
        print_help();
        exit(0);
    }

    if (transport_init(usb_path, usb_path_count) != 0)
        exit(-1);

    char *extra_args = argv[optind+1];

    if (strcmp(cmd, "dev") == 0)
    {
        err = dev_command(flag_list,
                          flag_install,
                          flag_y,
                          flag_write,
                          flag_s,
                          flag_a,
                          flag_b,
                          flag_index,
                          cmd_index,
                          extra_args,
                          s_arg,
                          fn);
    }
    else if (strcmp(cmd, "boot") == 0)
    {
        err = boot_command(flag_s,
                           flag_a,
                           flag_b,
                           flag_v,
                           flag_write,
                           flag_execute,
                           flag_reset,
                           s_arg,
                           fn);
    }
    else if (strcmp(cmd, "part") == 0)
    {
        err = part_command(flag_list,
                           flag_write,
                           flag_c,
                           flag_index,
                           flag_install,
                           flag_reset,
                           cmd_index,
                           offset,
                           fn);
    }
    else
    {
        printf("Unknown command\n");
        err = PB_ERR;
    }

    transport_exit();

    if (err != PB_OK)
        return -1;

    return 0;
}
