/**
 * Punch BOOT Image creation tool
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <3pp/bearssl/bearssl_hash.h>
#include <3pp/ini.h>
#include <pb/image.h>
#include <pb/crypto.h>
#include <pb/pb.h>

#include "crypto.h"

static const char *comp_type_str[] =
{
    "TEE",
    "VMM",
    "LINUX",
    "DT",
    "RAMDISK",
    "ATF",
    NULL,
};

static const char *pbi_output_fn = NULL;
static struct pb_image_hdr hdr;
static uint8_t signature[1024];
static struct pb_component_hdr comp[PB_IMAGE_MAX_COMP];
static void *component_data[PB_IMAGE_MAX_COMP];
static const char *pbi_key_source = NULL;

static uint32_t comp_string_to_index(const char *str, uint32_t *index)
{
    const char *s = NULL;
    uint32_t i = 0;

    do
    {
        s = comp_type_str[i];

        if (strcmp(s, str) == 0)
        {
            (*index) = i;
            return PB_OK;
        }
        i++;
    } while (s);

    return PB_ERR;
}

uint32_t pbimage_prepare(uint32_t key_index,
                         uint32_t hash_kind,
                         uint32_t sign_kind,
                         const char *key_source,
                         const char *output_fn)
{
    bzero(&hdr, sizeof(struct pb_image_hdr));
    bzero(comp, sizeof(struct pb_component_hdr) * PB_IMAGE_MAX_COMP);
    hdr.header_magic = PB_IMAGE_HEADER_MAGIC;
    hdr.header_version = 1;
    hdr.no_of_components = 0;
    hdr.key_index = key_index;
    hdr.hash_kind = hash_kind;
    hdr.sign_kind = sign_kind;

    pbi_key_source = key_source;
    pbi_output_fn = output_fn;
    return PB_OK;
}


uint32_t pbimage_append_component(const char *comp_type,
                                  uint32_t load_addr,
                                  const char *fn)
{
    FILE *fp = NULL;  
    struct stat finfo;
    uint32_t idx = hdr.no_of_components;
    uint32_t err = PB_OK;
    uint32_t padding = 0;
    void *data;

    fp = fopen(fn, "rb");

    if (fp == NULL)
        return PB_ERR_IO;

    comp[idx].load_addr_low = load_addr;

    err = comp_string_to_index(comp_type, &comp[idx].component_type);

    if (err != PB_OK)
        goto out_err;

    stat(fn, &finfo);
        
    padding = 512 - (finfo.st_size % 512);

    comp[idx].component_size = finfo.st_size + padding;

    data = malloc (comp[idx].component_size);

    if (data == NULL)
    {
        err = PB_ERR_MEM;
        goto out_err;
    }
    
    bzero(data, comp[idx].component_size);
 
    size_t read_sz = fread(data, finfo.st_size, 1, fp);

    if (read_sz != 1)
    {
        err = PB_ERR_IO;
        goto out_err2;
    }

    component_data[idx] = data;

    comp[idx].comp_header_version = PB_COMP_HDR_VERSION;

    hdr.no_of_components++;

out_err2:
    if (err != PB_OK)
        free(data);
out_err:
    fclose(fp);
    return err;
}

uint32_t pbimage_out(const char *fn)
{
    FILE *fp;
    uint32_t offset = 0;
    uint32_t ncomp = hdr.no_of_components;
    uint8_t zpad[511];
    uint8_t hash[64];
    uint32_t err;
    br_sha256_context sha256_ctx;
    br_sha384_context sha384_ctx;
    br_sha512_context sha512_ctx;

    bzero(zpad,511);
    fp = fopen(fn, "wb");

    if (fp == NULL)
        return PB_ERR_IO;

    offset = (sizeof(struct pb_image_hdr) + PB_IMAGE_SIGN_MAX_SIZE +
                PB_IMAGE_MAX_COMP * sizeof(struct pb_component_hdr));

    switch (hdr.hash_kind)
    {
        case PB_HASH_SHA256:
            br_sha256_init(&sha256_ctx);
        break;
        case PB_HASH_SHA384:
            br_sha384_init(&sha384_ctx);
        break;
        case PB_HASH_SHA512:
            br_sha512_init(&sha512_ctx);
        break;
        default:
            return PB_ERR;
    }

    /* Calculate component offsets and checksum */
    

    switch (hdr.hash_kind)
    {
        case PB_HASH_SHA256:
            br_sha256_update(&sha256_ctx, &hdr, sizeof(struct pb_image_hdr));
        break;
        case PB_HASH_SHA384:
            br_sha384_update(&sha384_ctx, &hdr, sizeof(struct pb_image_hdr));
        break;
        case PB_HASH_SHA512:
            br_sha512_update(&sha512_ctx, &hdr, sizeof(struct pb_image_hdr));
        break;
        default:
            return PB_ERR;
    }

    for (uint32_t i = 0; i < ncomp; i++)
    {
        comp[i].component_offset = offset;

        switch (hdr.hash_kind)
        {
            case PB_HASH_SHA256:
                br_sha256_update(&sha256_ctx, &comp[i],
                                              sizeof(struct pb_component_hdr));
            break;
            case PB_HASH_SHA384:

                br_sha384_update(&sha384_ctx, &comp[i],
                                              sizeof(struct pb_component_hdr));
            break;
            case PB_HASH_SHA512:

                br_sha512_update(&sha512_ctx, &comp[i],
                                              sizeof(struct pb_component_hdr));
            break;
            default:
                return PB_ERR;
        }
        offset = comp[i].component_offset + comp[i].component_size;
    }


    for (uint32_t i = 0; i < ncomp; i++)
    {

        switch (hdr.hash_kind)
        {
            case PB_HASH_SHA256:

                br_sha256_update(&sha256_ctx, component_data[i],
                                              comp[i].component_size);
            break;
            case PB_HASH_SHA384:

                br_sha384_update(&sha384_ctx, component_data[i],
                                              comp[i].component_size);
            break;
            case PB_HASH_SHA512:

                br_sha512_update(&sha512_ctx, component_data[i],
                                              comp[i].component_size);
            break;
            default:
                return PB_ERR;
        }
    }

    uint8_t hash_size;

    switch (hdr.hash_kind)
    {
        case PB_HASH_SHA256:
            br_sha256_out(&sha256_ctx, hash);
            printf("SHA256:");
            hash_size = 32;
        break;
        case PB_HASH_SHA384:
            br_sha384_out(&sha384_ctx, hash);
            printf("SHA384:");
            hash_size = 48;
        break;
        case PB_HASH_SHA512:
            br_sha512_out(&sha512_ctx, hash);
            printf("SHA512:");
            hash_size = 64;
        break;
        default:
            return PB_ERR;
    }

    for (uint32_t i = 0; i < hash_size; i++)
        printf("%2.2x ", hash[i]);
    printf("\n\n");

    err = crypto_initialize();

    if (err != PB_OK)
        return err;

    err = crypto_sign(hash,hdr.hash_kind,
                    pbi_key_source,hdr.sign_kind, signature);

    if (err != PB_OK)
    {
        printf ("Error: crypto_sign failed (%u)\n",err);
        return err;
    }

    switch(hdr.sign_kind)
    {
        case PB_SIGN_EC256:
        {
            printf ("EC256 Signature:");
            for (uint32_t i = 0; i < 64; i++)
                printf("%2.2x ", signature[i]);
        }
        break;
        case PB_SIGN_EC384:
        {
            printf ("EC384 Signature:");
            for (uint32_t i = 0; i < 96; i++)
                printf("%2.2x ", signature[i]);
        }
        break;
        case PB_SIGN_EC521:
        {
            printf ("EC521 Signature:");
            for (uint32_t i = 0; i < 128; i++)
                printf("%2.2x ", signature[i]);
        }
        break;
        case PB_SIGN_RSA4096:
        {
            printf ("RSA4096 Signature:");
            for (uint32_t i = 0; i < 512; i++)
                printf("%2.2x ", signature[i]);
        }
    }

    printf("\n");

    fwrite(&hdr, sizeof(struct pb_image_hdr), 1, fp);
    fwrite(signature, PB_IMAGE_SIGN_MAX_SIZE, 1, fp);
    fwrite(comp, PB_IMAGE_MAX_COMP*
                sizeof(struct pb_component_hdr),1,fp);

    for (uint32_t i = 0; i < ncomp; i++)
        fwrite(component_data[i],comp[i].component_size,1,fp);

    fclose (fp);
    return PB_OK;
}

