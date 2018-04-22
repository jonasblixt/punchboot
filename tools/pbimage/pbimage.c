#include <stdio.h>
#include <strings.h>
#include <tomcrypt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pbimage.h"


int main (int argc, char **argv) {
    FILE *fp;
    hash_state md;
    int read_sz;
    unsigned char buf[1024*1024];
    unsigned char hash[32];
    unsigned int bytes = 0;
    struct pb_image_hdr hdr;
    struct pb_component_hdr comp[16];
    struct stat f_info;
    FILE *fp_key = fopen("../../pki/test_rsa_private.der","rb");
    unsigned char key_buf[4096];
    rsa_key key;
    int prng_idx;
    extern const ltc_math_descriptor ltm_desc;
    unsigned char zero_pad [512];

    bzero(zero_pad, 512);
    if (argc <= 1) {
        printf ("pbimage help:\n\n");

        printf(" pbimage manifest.yml output.pbb\n");
        exit(0);
    }

    stat (argv[1], &f_info);

    /* register prng/hash */
    if (register_prng(&sprng_desc) == -1) {
        printf("Error registering sprng");
        return EXIT_FAILURE;
    }

    ltc_mp = ltm_desc;

	const struct ltc_hash_descriptor hash_desc = sha256_desc;
	const int hash_idx = register_hash(&hash_desc);


    fp = fopen (argv[1], "rb");
    sha256_init(&md);

    bzero(&hdr, sizeof(struct pb_image_hdr));
    bzero(comp,16* sizeof(struct pb_component_hdr));

    printf ("%i %i\n",sizeof(struct pb_image_hdr), sizeof(struct pb_component_hdr));

    hdr.header_magic = PB_IMAGE_HEADER_MAGIC;
    hdr.header_version = 1;
    hdr.no_of_components = 1;

    sha256_process(&md, (unsigned char *)&hdr, sizeof(struct pb_image_hdr));
 
    comp[0].comp_header_version = 1;
    comp[0].component_type = PB_IMAGE_COMPTYPE_VMM;
    comp[0].load_addr_low = 0x87800000;
    comp[0].component_size = f_info.st_size;
    comp[0].component_offset = sizeof(struct pb_image_hdr) + 16*sizeof(struct pb_component_hdr);
    
    int padding_sz = (-comp[0].component_offset % 512);

    comp[0].component_offset += padding_sz;

    sha256_process(&md, (unsigned char *) comp, 16*sizeof(struct pb_component_hdr));
    printf ("component_offset %x\n",comp[0].component_offset);
    while ( (read_sz = fread(buf, 1, 1024*1024,fp )) >0) {
        sha256_process(&md, buf, read_sz);
        bytes += read_sz;
        printf (" - %i\n",read_sz);
    }
    sha256_done(&md, hash);
    printf ("Read %i bytes\n",bytes);
    memcpy(hdr.sha256, hash,32);

    printf ("Package SHA256:");
    for (int i = 0; i < 32; i++)
        printf ("%2.2x",hdr.sha256[i]);

    printf ("\n");

    int key_sz = fread (key_buf, 1, sizeof(key_buf), fp_key);
    printf ("Read key, %i bytes\n",key_sz);

    int err = rsa_import(key_buf, key_sz, &key) ;
    if (err == CRYPT_OK) {
        printf ("Read key\n");
    } else {
        printf ("Can't read key %i\n",err);
        exit(-1);
    }

    //unsigned char sig[4096];
    //unsigned int sig_len = 4096;
    hdr.sign_length = 1024;
    const unsigned long saltlen = 0;
    unsigned long sig_l = 1024;
    int prng_index = register_prng(&sprng_desc);
    err = rsa_sign_hash_ex(hdr.sha256, 32, hdr.sign, &sig_l, 
                LTC_LTC_PKCS_1_V1_5,NULL,prng_index,hash_idx,saltlen,&key);
    
    hdr.sign_length = (u32) sig_l;
    if (err != CRYPT_OK) {
        printf ("Signing failed!\n");
    }

    printf ("Signature %i\n",hdr.sign_length);


    rewind(fp);
    FILE *fp_out = fopen("out.pbi","wb");
    fwrite (&hdr, sizeof(struct pb_image_hdr), 1, fp_out);
    fwrite (comp, 16* sizeof(struct pb_component_hdr), 1, fp_out);

    bzero(buf,sizeof(buf));
    fwrite(buf, padding_sz, 1, fp_out);
 
    while ( (read_sz = fread(buf, 1, 1024, fp)) > 0) {
        fwrite(buf, read_sz, 1, fp_out);
    }

    fclose(fp_out);
    printf ("Done\n\r");



}
