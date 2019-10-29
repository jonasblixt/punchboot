/**
 * Punch BOOT Image creation tool
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <termios.h>
#include <openssl/pem.h>
#include <pkcs11-helper-1.0/pkcs11h-certificate.h>
#include <pkcs11-helper-1.0/pkcs11h-openssl.h>
#include <3pp/ini.h>
#include <pb/image.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include "pbimage.h"

static const char *comp_type_str[] =
{
    "TEE",
    "VMM",
    "LINUX",
    "DT",
    "RAMDISK",
    "ATF",
    "KERNEL",
    NULL,
};

static const char *pbi_output_fn = NULL;
static struct pb_image_hdr hdr;
static struct pb_component_hdr comp[PB_IMAGE_MAX_COMP];
static void *component_data[PB_IMAGE_MAX_COMP];
static EVP_PKEY *evp;
static unsigned char signature[PB_IMAGE_SIGN_MAX_SIZE];
static pkcs11h_certificate_id_list_t issuers, certs, temp;
static pkcs11h_certificate_t cert;
static pkcs11h_openssl_session_t session = NULL;

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

static PKCS11H_BOOL _pkcs11h_hooks_token_prompt(
    IN void * const global_data,
    IN void * const user_data,
    IN const pkcs11h_token_id_t token,
    IN const unsigned retry)
{
    char buf[1024];
    PKCS11H_BOOL fValidInput = FALSE;
    PKCS11H_BOOL fRet = FALSE;

    while (!fValidInput)
    {
        fprintf(stderr, "Please insert token '%s' 'ok' or 'cancel': ",
                                                    token->display);
        if (fgets (buf, sizeof (buf), stdin) == NULL)
        {
            printf("fgets failed");
        }

        buf[sizeof(buf)-1] = '\0';
        fflush(stdin);

        if (buf[strlen(buf)-1] == '\n')
        {
            buf[strlen(buf)-1] = '\0';
        }

        if (buf[strlen(buf)-1] == '\r')
        {
            buf[strlen(buf)-1] = '\0';
        }

        if (!strcmp(buf, "ok"))
        {
            fValidInput = TRUE;
            fRet = TRUE;
        }
        else if (!strcmp(buf, "cancel"))
        {
            fValidInput = TRUE;
        }
    }

    return fRet;
}

static PKCS11H_BOOL _pkcs11h_hooks_pin_prompt(
    IN void * const global_data,
    IN void * const user_data,
    IN const pkcs11h_token_id_t token,
    IN const unsigned retry,
    OUT char * const pin,
    IN const size_t pin_max)
{
    char prompt[1124];
    struct termios oflags, nflags;
    char password[64];

    snprintf (prompt, sizeof (prompt)-1, "Please enter '%s' PIN or 'cancel': ",
                                                            token->display);

#if defined(_WIN32)
    {
        size_t i = 0;
        char c;
        while (i < pin_max && (c = getch()) != '\r')
        {
            pin[i++] = c;
        }
    }

    fprintf(stderr, "\n");
#else
    /* disabling echo */
    tcgetattr(fileno(stdin), &oflags);
    nflags = oflags;
    nflags.c_lflag &= ~ECHO;
    nflags.c_lflag |= ECHONL;

    if (tcsetattr(fileno(stdin), TCSANOW, &nflags) != 0) {
        perror("tcsetattr");
        return EXIT_FAILURE;
    }

    printf("%s", prompt);

    char *r = fgets(password, sizeof(password), stdin);
    if (r == NULL)
        return false;
    password[strlen(password) - 1] = 0;

    /* restore terminal */
    if (tcsetattr(fileno(stdin), TCSANOW, &oflags) != 0) {
        perror("tcsetattr");
        return EXIT_FAILURE;
    }
#endif

    strncpy(pin, password, pin_max);
    pin[pin_max-1] = '\0';

    return strcmp (pin, "cancel") != 0;
}
uint32_t pbimage_prepare(uint32_t key_index,
                         uint32_t hash_kind,
                         uint32_t sign_kind,
                         const char *key_source,
                         const char *pkcs11_provider,
                         const char *pkcs11_key_id,
                         const char *output_fn)
{
    CK_RV rv;

    bzero(&hdr, sizeof(struct pb_image_hdr));
    bzero(comp, sizeof(struct pb_component_hdr) * PB_IMAGE_MAX_COMP);
    hdr.header_magic = PB_IMAGE_HEADER_MAGIC;
    hdr.header_version = 1;
    hdr.no_of_components = 0;
    hdr.key_index = key_index;
    hdr.hash_kind = hash_kind;
    hdr.sign_kind = sign_kind;

    if (strstr(key_source, "PKCS11"))
    {
        rv = pkcs11h_initialize();

        if (rv != CKR_OK)
        {
            printf("pkcs11h_initialize failed\n");
            return PB_ERR;
        }

        rv = pkcs11h_setTokenPromptHook(_pkcs11h_hooks_token_prompt, NULL);

        if (rv != CKR_OK)
        {
            printf("pkcs11h_setTokenPromptHook failed\n");
            return PB_ERR;
        }

        rv = pkcs11h_setPINPromptHook(_pkcs11h_hooks_pin_prompt, NULL);

        if (rv != CKR_OK)
        {
            printf("pkcs11h_setPINPromptHook failed\n");
            return PB_ERR;
        }

        printf(" PKCS11 provider '%s'\n", pkcs11_provider);

        rv = pkcs11h_addProvider(
                pkcs11_provider,
                pkcs11_provider,
                FALSE,
                PKCS11H_PRIVATEMODE_MASK_AUTO,
                PKCS11H_SLOTEVENT_METHOD_AUTO,
                0,
                FALSE);

        if (rv != CKR_OK)
        {
            printf("pkcs11h_addProvider failed\n");
            return PB_ERR;
        }

        rv = pkcs11h_certificate_enumCertificateIds(
                PKCS11H_ENUM_METHOD_CACHE,
                NULL,
                PKCS11H_PROMPT_MASK_ALLOW_ALL,
                &issuers,
                &certs);

        if (rv != CKR_OK)
        {
            printf("pkcs11h_certificate_enumCertificateIds failed\n");
            return PB_ERR;
        }

        for (temp = certs; temp != NULL; temp = temp->next)
        {
            printf(" Certificate: %s\n", temp->certificate_id->displayName);

            printf("attrCKA_ID: ");
            for (uint32_t n = 0;
                n < temp->certificate_id->attrCKA_ID_size; n++)
            {
                printf("%02x ", temp->certificate_id->attrCKA_ID[n]);
            }
            printf("\n");
        }

        if (certs == NULL)
        {
            printf("Error: No certificates found\n");
            return PB_ERR;
        }

        rv = pkcs11h_certificate_create(
                certs->certificate_id,
                NULL,
                PKCS11H_PROMPT_MASK_ALLOW_ALL,
                PKCS11H_PIN_CACHE_INFINITE,
                &cert);

        if (rv != CKR_OK)
        {
            printf("pkcs11h_certificate_create failed\n");
            return PB_ERR;
        }

        session = pkcs11h_openssl_createSession(cert);

        if (session == NULL)
        {
            printf("pkcs11h_openssl_createSession failed\n");
            return PB_ERR;
        }

        evp = pkcs11h_openssl_session_getEVP(session);

        if (evp == NULL)
        {
            printf("pkcs11h_openssl_session_getEVP failed\n");
            return PB_ERR;
        }
    }
    else
    {
        evp = EVP_PKEY_new();
        FILE *fp = fopen(key_source, "r");
        if (PEM_read_PrivateKey(fp, &evp, NULL, NULL) == NULL)
        {
            printf("Error: Could not read private key\n");
            return PB_ERR;
        }

        fclose(fp);
    }

    pbi_output_fn = output_fn;
    return PB_OK;
}

void pbimage_cleanup(void)
{
    CK_RV rv;

    if (session != NULL)
    {
        pkcs11h_openssl_freeSession(session);
        pkcs11h_certificate_freeCertificateIdList(issuers);
        pkcs11h_certificate_freeCertificateIdList(certs);

        rv = pkcs11h_terminate();

        if (rv != CKR_OK)
        {
            printf("pkcs11h_terminate failed");
        }
    }
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

    comp[idx].load_addr = load_addr;

    err = comp_string_to_index(comp_type, &comp[idx].component_type);

    if (err != PB_OK)
        goto out_err;

    stat(fn, &finfo);

    padding = 512 - (finfo.st_size % 512);

    comp[idx].component_size = finfo.st_size + padding;

    data = malloc(comp[idx].component_size);

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
    unsigned char *asn1_signature;
    size_t signature_size;

    EVP_MD_CTX* ctx;

    ctx = EVP_MD_CTX_create();

    if (ctx == NULL)
    {
        printf("EVP_MD_CTX_create failed\n");
        return PB_ERR;
    }

    bzero(zpad, 511);
    fp = fopen(fn, "wb");

    if (fp == NULL)
        return PB_ERR_IO;

    offset = (sizeof(struct pb_image_hdr) + PB_IMAGE_SIGN_MAX_SIZE +
                PB_IMAGE_MAX_COMP * sizeof(struct pb_component_hdr));

    switch (hdr.hash_kind)
    {
        case PB_HASH_SHA256:
            if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) == -1)
            {
                printf("EVP_DigestInit_ex failed\n");
                return PB_ERR;
            }

            if (EVP_DigestSignInit(ctx, NULL, EVP_sha256(), NULL, evp) == -1)
            {
                printf("EVP_DigestSignInit failed");
                return PB_ERR;
            }
        break;
        case PB_HASH_SHA384:
            if (EVP_DigestInit_ex(ctx, EVP_sha384(), NULL) == -1)
            {
                printf("EVP_DigestInit_ex failed\n");
                return PB_ERR;
            }

            if (EVP_DigestSignInit(ctx, NULL, EVP_sha384(), NULL, evp) == -1)
            {
                printf("EVP_DigestSignInit failed");
                return PB_ERR;
            }
        break;
        case PB_HASH_SHA512:
            if (EVP_DigestInit_ex(ctx, EVP_sha512(), NULL) == -1)
            {
                printf("EVP_DigestInit_ex failed\n");
                return PB_ERR;
            }

            if (EVP_DigestSignInit(ctx, NULL, EVP_sha512(), NULL, evp) == -1)
            {
                printf("EVP_DigestSignInit failed");
                return PB_ERR;
            }
        break;
        default:
            return PB_ERR;
    }

    /* Calculate component offsets and checksum */

    if (EVP_DigestSignUpdate(ctx, &hdr, sizeof(struct pb_image_hdr)) == -1)
    {
        printf("EVP_DigestSignUpdate failed");
        return PB_ERR;
    }

    for (uint32_t i = 0; i < ncomp; i++)
    {
        comp[i].component_offset = offset;

        if (EVP_DigestSignUpdate(ctx, &comp[i],
                    sizeof(struct pb_component_hdr)) == -1)
        {
            printf("EVP_DigestSignUpdate failed");
            return PB_ERR;
        }

        offset = comp[i].component_offset + comp[i].component_size;
    }

    for (uint32_t i = 0; i < ncomp; i++)
    {
        if (EVP_DigestSignUpdate(ctx, component_data[i],
                    comp[i].component_size) == -1)
        {
            printf("EVP_DigestSignUpdate failed\n");
            return PB_ERR;
        }
    }

    if (EVP_DigestSignFinal(ctx, NULL, &signature_size) == -1)
    {
        printf("EVP_DigestSignFinal failed\n");
        return PB_ERR;
    }

    if (signature_size > PB_IMAGE_SIGN_MAX_SIZE)
    {
        printf("Signature > PB_IMAGE_SIGN_MAX_SIZE\n");
        return PB_ERR;
    }

    memset(signature, 0, PB_IMAGE_SIGN_MAX_SIZE);
    asn1_signature = OPENSSL_malloc(signature_size);

    if (asn1_signature == NULL)
    {
        printf("OPENSSL_malloc failed\n");
        return PB_ERR;
    }

    if (EVP_DigestSignFinal(ctx, asn1_signature, &signature_size) == -1)
    {
        printf("EVP_DigestSignFinal failed\n");
        return PB_ERR;
    }

    /* Signature output from openssl is ASN.1 encoded
     * punchboot requires raw ECDSA signatures
     * RSA signatures should retain the ASN.1 structure
     * */
    uint8_t r_sz = 0;
    uint8_t s_sz = 0;
    uint8_t r_off = 0;
    uint8_t s_off = 0;

    switch (hdr.sign_kind)
    {
        case PB_SIGN_NIST256p:
            r_sz = asn1_signature[3];
            r_off = r_sz-32;
            s_sz = asn1_signature[3+r_sz+2];
            s_off = s_sz-32;
            memcpy(&signature[0], &asn1_signature[3+1+r_off], r_sz);
            memcpy(&signature[32], &asn1_signature[3+r_sz+2+1+s_off], s_sz);
        break;
        case PB_SIGN_NIST384p:
            r_sz = asn1_signature[3];
            r_off = r_sz-48;
            s_sz = asn1_signature[3+r_sz+2];
            s_off = s_sz-48;
            memcpy(&signature[0], &asn1_signature[4+r_off], r_sz);
            memcpy(&signature[48], &asn1_signature[3+r_sz+2+1+s_off], s_sz);
        break;
        case PB_SIGN_NIST521p:
            r_sz = asn1_signature[4];
            r_off = 66-r_sz;
            s_sz = asn1_signature[4+r_sz+2];
            s_off = 66-s_sz;
            memcpy(&signature[r_off], &asn1_signature[5], r_sz);
            memcpy(&signature[66+s_off], &asn1_signature[4+r_sz+2+1], s_sz);
        break;
        case PB_SIGN_RSA4096:
            memcpy(signature, asn1_signature, signature_size);
        break;
        default:
            printf("Unknown signature format\n");
            return PB_ERR;
    }

    fwrite(&hdr, sizeof(struct pb_image_hdr), 1, fp);
    fwrite(signature, PB_IMAGE_SIGN_MAX_SIZE, 1, fp);
    fwrite(comp, PB_IMAGE_MAX_COMP*
                sizeof(struct pb_component_hdr), 1, fp);

    for (uint32_t i = 0; i < ncomp; i++)
        fwrite(component_data[i], comp[i].component_size, 1, fp);

    fclose(fp);

    OPENSSL_free(asn1_signature);
    EVP_MD_CTX_destroy(ctx);
    return PB_OK;
}

