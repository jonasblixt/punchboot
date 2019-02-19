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
#include <tomcrypt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <3pp/bearssl/bearssl_hash.h>
#include <3pp/bearssl/bearssl_rsa.h>
#include <3pp/bearssl/bearssl_x509.h>
#include <pb/image.h>

static struct pb_image_hdr hdr;
static struct pb_component_hdr comp[PB_IMAGE_MAX_COMP];
static FILE *pb_components_fp[PB_IMAGE_MAX_COMP];
static uint32_t pb_components_zpad[PB_IMAGE_MAX_COMP];

static unsigned char buf[1024*1024];

static br_sha256_context sha256_ctx;

static int pbimage_gen_output(const char *fn_sign_key,
                              uint8_t key_index,
                              const char *fn_output,
                              int no_of_components,
                              uint32_t key_mask) 
{

    int err = 0;
    FILE *fp_key = NULL;
    FILE *fp_out = NULL;
    unsigned char hash[32];
    int padding_sz[PB_IMAGE_MAX_COMP];
    int read_sz = 0;
    int bytes = 0;
    uint8_t padding_zero[511];
    br_rsa_private_key *br_k;
    br_skey_decoder_context skey_ctx;

    memset(padding_zero,0,511);

    fp_key = fopen(fn_sign_key, "rb");

    if (fp_key == NULL) {
        printf ("Error: Could not open signing key\n");
        return -1;
    }

    printf ("Generating image:\n");

    /* Load private key for signing */
    int key_sz = fread (buf, 1, sizeof(buf), fp_key);
    //err = rsa_import(buf, key_sz, &key) ;
    br_skey_decoder_init(&skey_ctx);
    br_skey_decoder_push(&skey_ctx, buf, key_sz);
    fclose(fp_key);
   
    if (err == CRYPT_OK) {
        printf (" o Read signing key, %i bytes\n",key_sz);
    } else {
        printf ("Can't read key %i\n",err);
        return err;
    }

    /* Create SHA256 */

    br_sha256_init(&sha256_ctx);
    hdr.header_magic = PB_IMAGE_HEADER_MAGIC;
    hdr.header_version = 1;
    hdr.no_of_components = no_of_components;
    hdr.key_index = key_index;
    hdr.key_revoke_mask = key_mask;

    br_sha256_update(&sha256_ctx, (void *) &hdr, sizeof(struct pb_image_hdr));
    unsigned int last_offset = sizeof(struct pb_image_hdr) + 
                PB_IMAGE_MAX_COMP*sizeof(struct pb_component_hdr);

    for (int i = 0; i < no_of_components; i++) 
    {
        printf (" - %i, LA [0x%8.8X], SZ %i\n", i, comp[i].load_addr_low,
                                                comp[i].component_size);

        comp[i].comp_header_version = PB_COMP_HDR_VERSION;

        comp[i].component_offset = last_offset;
        padding_sz[i] = (-comp[i].component_offset % 512);
        comp[i].component_offset += padding_sz[i];

        last_offset = comp[i].component_offset + 
                      comp[i].component_size;


        br_sha256_update(&sha256_ctx, (unsigned char *) &comp[i], 
                            sizeof(struct pb_component_hdr));
    }

    for (int i = 0; i < no_of_components; i++) 
    {
        while ( (read_sz = fread(buf, 1, sizeof(buf),pb_components_fp[i] )) >0) 
        {
            br_sha256_update(&sha256_ctx, buf, read_sz);
            bytes += read_sz;
        }
        if (pb_components_zpad[i]) 
        {
            br_sha256_update(&sha256_ctx, padding_zero, 
                                pb_components_zpad[i]);
            bytes += pb_components_zpad[i];
        }
    }

    br_sha256_out(&sha256_ctx, hash);
    printf (" o Done,  %i kBytes\n",bytes/1024);
    memcpy(hdr.sha256, hash,32);

    printf (" o Image SHA256:");
    for (int i = 0; i < 32; i++)
        printf ("%2.2x",hdr.sha256[i]);
    printf ("\n");

    /* Create signature */

    br_k = br_skey_decoder_get_rsa(&skey_ctx);

    hdr.sign_length = 512;

    if (br_k == NULL)
    {
        printf ("Error could not decode key file.\n");
        return -1;
    }

    if (!br_rsa_i62_pkcs1_sign(NULL, hdr.sha256, 32, br_k, hdr.sign))
    {
        printf ("Error: sign failed\n");
        return -1;
    }

    printf (" o Sign %i\n",hdr.sign_length);

   /* Create, final, output image */

    fp_out = fopen(fn_output,"wb");

    if (fp_out == NULL) {
        printf ("Error: Could not open output file for writing!\n");
        return -1;
    }

    fwrite (&hdr, sizeof(struct pb_image_hdr), 1, fp_out);
    fwrite (comp, PB_IMAGE_MAX_COMP*
                sizeof(struct pb_component_hdr), 1, fp_out);

    for (int i = 0; i < no_of_components; i++) 
    {
        bzero(buf,sizeof(buf));
        fwrite(buf, padding_sz[i], 1, fp_out);
        rewind(pb_components_fp[i]);

        while ( (read_sz = fread(buf, 1, sizeof(buf), 
                            pb_components_fp[i])) > 0) 
        {

            fwrite(buf, read_sz, 1, fp_out);
        }

        if (pb_components_zpad[i])
        {
            fwrite(padding_zero, pb_components_zpad[i], 1, fp_out);
        }
        fclose(pb_components_fp[i]);
    }

    fclose(fp_out);

    printf (" o Done\n");
    return 0;
}

static void pbimage_print_help(void) {
    printf ("pbimage:\n\n");

    printf("  pbimage -t <type> -l <addr> -f <fn> -k <key fn> -n <key index> -o <output fn> [-m <key mask>]\n");
    printf("\n   Add image <fn>, of type <type> that should be loaded\n"
            "     at address <addr> and signed with key <key_fn>.\n"
            "     Multiple images can be added by chaining several -t -l and -f args\n");
        
}


int main (int argc, char **argv) {
    int opt;
    /* register prng/hash */

    int no_of_components = 0;
    int component_type = -1;
    unsigned int load_addr = 0;
    uint8_t key_index = 255;
    uint32_t key_mask = 0;
    bool flag_type = false;
    bool flag_load = false;
    bool flag_file = false;
    bool flag_key_mask = false;
    bool flag_key_index = false;

    char *output_fn = NULL;
    char *sign_key_fn = NULL;

    struct stat finfo;

    bzero(&hdr, sizeof(struct pb_image_hdr));
    bzero(&comp, PB_IMAGE_MAX_COMP * sizeof(struct pb_component_hdr));

    if (argc <= 1) 
    {
        pbimage_print_help();
        exit(0);
    }

    while ((opt = getopt(argc, argv, "ht:l:f:n:k:o:m:")) != -1) 
    {
        switch (opt) {
            case 'h':
                pbimage_print_help();
                exit(0);
            break;
            case 't':
                flag_type = true;
                flag_file = false;
                if (strcmp(optarg, "VMM") == 0)
                    component_type = PB_IMAGE_COMPTYPE_VMM;
                else if (strcmp(optarg, "TEE") == 0)
                    component_type = PB_IMAGE_COMPTYPE_TEE;
                else if (strcmp(optarg, "DT") == 0)
                    component_type = PB_IMAGE_COMPTYPE_DT;
                else if (strcmp(optarg, "LINUX") == 0)
                    component_type = PB_IMAGE_COMPTYPE_LINUX;
                else if (strcmp(optarg, "ATF") == 0)
                    component_type = PB_IMAGE_COMPTYPE_ATF;
                else if (strcmp(optarg, "RAMDISK") == 0)
                    component_type = PB_IMAGE_COMPTYPE_RAMDISK;
                else {
                    printf ("Component type '%s' not recognized\n",optarg);
                    exit(-1);
                }
            break;
            case 'l':
                flag_load = true;
                flag_file = false;
                load_addr = (int)strtol(optarg, NULL, 0);
            break;
            case 'm':
                flag_key_mask = true;
                key_mask = (uint32_t) strtol(optarg, NULL, 0);
            break;
            case 'n':
                key_index = (uint8_t) strtol(optarg, NULL, 0);
                flag_key_index = true;
            break;
            case 'f':
                if (!flag_load || !flag_type) {
                    printf ("Error type and load address not set\n");
                    exit(-1);
                }
                pb_components_fp[no_of_components] = fopen(optarg,"rb");
                if (pb_components_fp[no_of_components] == NULL) {
                    printf ("Error could not open input file '%s'\n",optarg);
                    exit(-1);
                }
                
                stat(optarg, &finfo);
                


                pb_components_zpad[no_of_components] =
                    512 - (finfo.st_size % 512);

                comp[no_of_components].component_size = finfo.st_size +
                    pb_components_zpad[no_of_components];
                comp[no_of_components].load_addr_low = load_addr;
                comp[no_of_components].component_type = component_type;

                no_of_components++;
                flag_load = false;
                flag_type = false;
                flag_file = true;
            break;
            case 'k':
                sign_key_fn = malloc(strlen(optarg));
                strcpy(sign_key_fn, optarg);
            break;
            case 'o':
                output_fn = malloc(strlen(optarg));
                strcpy(output_fn, optarg);
            break;
            default:
                pbimage_print_help();
                exit(0);
        }
    }

    int err = -1;

    if (!flag_file || !flag_key_index) 
    {
        pbimage_print_help();
    } else {
        err = pbimage_gen_output(sign_key_fn, 
                                 key_index,
                                 output_fn, 
                                 no_of_components,
                                 key_mask);
    }

    if (sign_key_fn)
        free(sign_key_fn);
    if (output_fn)
        free(output_fn);

    exit(err);

}
