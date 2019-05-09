#include <stdio.h>
#include <pb/pb.h>
#include <pb/recovery.h>
#include <pb/gpt.h>
#include <pb/image.h>
#include <uuid.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include "recovery_protocol.h"
#include "transport.h"
#include "utils.h"

/* br_ecdsa_asn1_to_raw copied from BearSSL library */
/* see bearssl_ec.h */
size_t br_ecdsa_asn1_to_raw(void *sig, size_t sig_len)
{
	/*
	 * Note: this code is a bit lenient in that it accepts a few
	 * deviations to DER with regards to minimality of encoding of
	 * lengths and integer values. These deviations are still
	 * unambiguous.
	 *
	 * Signature format is a SEQUENCE of two INTEGER values. We
	 * support only integers of less than 127 bytes each (signed
	 * encoding) so the resulting raw signature will have length
	 * at most 254 bytes.
	 */

	unsigned char *buf, *r, *s;
	size_t zlen, rlen, slen, off;
	unsigned char tmp[254];

	buf = sig;
	if (sig_len < 8) {
		return 0;
	}

	/*
	 * First byte is SEQUENCE tag.
	 */
	if (buf[0] != 0x30) {
		return 0;
	}

	/*
	 * The SEQUENCE length will be encoded over one or two bytes. We
	 * limit the total SEQUENCE contents to 255 bytes, because it
	 * makes things simpler; this is enough for subgroup orders up
	 * to 999 bits.
	 */
	zlen = buf[1];
	if (zlen > 0x80) {
		if (zlen != 0x81) {
			return 0;
		}
		zlen = buf[2];
		if (zlen != sig_len - 3) {
			return 0;
		}
		off = 3;
	} else {
		if (zlen != sig_len - 2) {
			return 0;
		}
		off = 2;
	}

	/*
	 * First INTEGER (r).
	 */
	if (buf[off ++] != 0x02) {
		return 0;
	}
	rlen = buf[off ++];
	if (rlen >= 0x80) {
		return 0;
	}
	r = buf + off;
	off += rlen;

	/*
	 * Second INTEGER (s).
	 */
	if (off + 2 > sig_len) {
		return 0;
	}
	if (buf[off ++] != 0x02) {
		return 0;
	}
	slen = buf[off ++];
	if (slen >= 0x80 || slen != sig_len - off) {
		return 0;
	}
	s = buf + off;

	/*
	 * Removing leading zeros from r and s.
	 */
	while (rlen > 0 && *r == 0) {
		rlen --;
		r ++;
	}
	while (slen > 0 && *s == 0) {
		slen --;
		s ++;
	}

	/*
	 * Compute common length for the two integers, then copy integers
	 * into the temporary buffer, and finally copy it back over the
	 * signature buffer.
	 */
	zlen = rlen > slen ? rlen : slen;
	sig_len = zlen << 1;
	memset(tmp, 0, sig_len);
	memcpy(tmp + zlen - rlen, r, rlen);
	memcpy(tmp + sig_len - slen, s, slen);
	memcpy(sig, tmp, sig_len);
	return sig_len;
}

uint32_t pb_recovery_authenticate(uint32_t key_index, const char *fn)
{
    uint32_t err;
    uint8_t cookie_buffer[PB_RECOVERY_AUTH_COOKIE_SZ];

    memset (cookie_buffer,0,PB_RECOVERY_AUTH_COOKIE_SZ);

    FILE *fp = fopen(fn,"rb");
    int read_sz = fread (cookie_buffer,1,PB_RECOVERY_AUTH_COOKIE_SZ,fp);

    printf ("Read %u bytes\n",read_sz);

    read_sz = br_ecdsa_asn1_to_raw(cookie_buffer, read_sz);

    printf ("%u\n",read_sz);

    err = pb_write(PB_CMD_AUTHENTICATE,key_index,0,0,0, cookie_buffer, read_sz);

    if (err != PB_OK)
        return err;

    err = pb_read_result_code();

    if (err != PB_OK)
        return err;

    return PB_OK;
}

uint32_t pb_recovery_setup_lock(void)
{
    uint32_t err;

    err = pb_write(PB_CMD_SETUP_LOCK,0,0,0,0, NULL, 0);

    if (err != PB_OK)
        return err;

    err = pb_read_result_code();

    if (err != PB_OK)
        return err;

    return PB_OK;
}

uint32_t pb_recovery_setup(struct param *params)
{

    uint32_t err = PB_ERR;
    uint32_t param_count = 0;

    while (params[param_count].kind != PB_PARAM_END)
        param_count++;

    err = pb_write(PB_CMD_SETUP,param_count,0,0,0, (uint8_t *) params,
                            (sizeof(struct param)*param_count));

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_read_params(struct param **params)
{
    uint32_t sz;
    uint32_t err;

    err = pb_write(PB_CMD_GET_PARAMS,0,0,0,0,NULL,0);

    if (err != PB_OK)
        return err;

    err = pb_read((uint8_t *) &sz, sizeof(uint32_t));

    if (err != PB_OK)
        return err;

    (*params) = malloc (sz);

    err = pb_read((uint8_t *) (*params), sz);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_install_default_gpt(void) 
{
    if (pb_write(PB_CMD_WRITE_DFLT_GPT,0,0,0,0, NULL, 0) != PB_OK)
        return PB_ERR;
    return pb_read_result_code();
}


uint32_t pb_reset(void) 
{
    uint32_t err = PB_ERR;

    err = pb_write(PB_CMD_RESET,0,0,0,0, NULL, 0);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_boot_part(uint8_t part_no, bool verbose) 
{
    uint32_t err = PB_ERR;

    err = pb_write(PB_CMD_BOOT_PART,part_no,verbose,0,0, NULL, 0);
    
    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_set_bootpart(uint8_t bootpart)
{
    uint32_t err = PB_ERR;

    err = pb_write(PB_CMD_BOOT_ACTIVATE,bootpart,0,0,0, NULL, 0);
    
    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_get_version(char **out)
{
    uint32_t sz = 0;
    int err;

    err = pb_write(PB_CMD_GET_VERSION,0,0,0,0, NULL, 0);

    if (err)
        return err;

    err = pb_read((uint8_t*) &sz, 4);

    *out = malloc(sz+1);
    bzero (*out, sz+1);

    if (err)
    {
        free(*out);
        return err;
    }

    err = pb_read((uint8_t*) *out, sz);

    if (err)
    {
        free(*out);
        return err;
    }

    out[sz] = 0;

    err = pb_read_result_code();

    if (err != PB_OK)
        free(*out);

    return err;
}

uint32_t pb_get_gpt_table(struct gpt_primary_tbl *tbl) 
{
    uint32_t tbl_sz = 0;
    int err;

    err = pb_write(PB_CMD_GET_GPT_TBL,0,0,0,0, NULL, 0);

    if (err)
        return err;

    err = pb_read((uint8_t*) &tbl_sz, 4);
    
    if (err)
        return err;

    err = pb_read((uint8_t*) tbl, tbl_sz);

    if (err)
        return err;

    return pb_read_result_code();
}

uint32_t pb_flash_part (uint8_t part_no, const char *f_name) 
{
    int read_sz = 0;
    int sent_sz = 0;
    int buffer_id = 0;
    int err;
    FILE *fp = NULL; 
    unsigned char *bfr = NULL;

    struct pb_cmd_prep_buffer bfr_cmd;
    struct pb_cmd_write_part wr_cmd;

    fp = fopen (f_name,"rb");

    if (fp == NULL) 
    {
        printf ("Could not open file: %s\n",f_name);
        return PB_ERR;
    }

    bfr =  malloc(1024*1024*8);
    
    if (bfr == NULL) 
    {
        printf ("Could not allocate memory\n");
        return PB_ERR;
    }

    wr_cmd.lba_offset = 0;
    wr_cmd.part_no = part_no;

    while ((read_sz = fread(bfr, 1, 1024*1024*4, fp)) >0) {
       bfr_cmd.no_of_blocks = read_sz / 512;
        if (read_sz % 512)
            bfr_cmd.no_of_blocks++;
        
        bfr_cmd.buffer_id = buffer_id;
        pb_write(PB_CMD_PREP_BULK_BUFFER,0,0,0,0, 
                (uint8_t *) &bfr_cmd, sizeof(struct pb_cmd_prep_buffer));

        err = pb_write_bulk(bfr, bfr_cmd.no_of_blocks*512, &sent_sz);
        if (err != 0) {
            printf ("Bulk xfer error, err=%i\n",err);
            goto err_xfer;
        }

        err = pb_read_result_code();

        if (err != PB_OK)
            return err;

        wr_cmd.no_of_blocks = bfr_cmd.no_of_blocks;
        wr_cmd.buffer_id = buffer_id;
        buffer_id = !buffer_id;

        pb_write(PB_CMD_WRITE_PART,0,0,0,0, (uint8_t *) &wr_cmd,
                    sizeof(struct pb_cmd_write_part));

        err = pb_read_result_code();

        if (err != PB_OK)
            return err;

        wr_cmd.lba_offset += bfr_cmd.no_of_blocks;
    }

err_xfer:
    free(bfr);
    fclose(fp);
    return err;
}


uint32_t pb_program_bootloader (const char *f_name) 
{
    int read_sz = 0;
    int sent_sz = 0;
    int err;
    FILE *fp = NULL; 
    unsigned char bfr[1024*1024*1];
    uint32_t no_of_blocks = 0;
    struct stat finfo;
    struct pb_cmd_prep_buffer buffer_cmd;

    fp = fopen (f_name,"rb");

    if (fp == NULL) 
    {
        printf ("Could not open file: %s\n",f_name);
        return -1;
    }

    stat(f_name, &finfo);

    no_of_blocks = finfo.st_size / 512;

    if (finfo.st_size % 512)
        no_of_blocks++;

    buffer_cmd.buffer_id = 0;
    buffer_cmd.no_of_blocks = no_of_blocks;
    
    pb_write(PB_CMD_PREP_BULK_BUFFER,0,0,0,0, (uint8_t *) &buffer_cmd,
                                    sizeof(struct pb_cmd_prep_buffer));

    while ((read_sz = fread(bfr, 1, sizeof(bfr), fp)) >0) 
    {
        err = pb_write_bulk(bfr, read_sz, &sent_sz);
        if (err != 0) 
            return -1;
    }
    
    fclose(fp);

    err = pb_read_result_code();

    if (err != PB_OK)
        return err;

    err = pb_write(PB_CMD_FLASH_BOOTLOADER, no_of_blocks,0,0,0, NULL, 0);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}




uint32_t pb_execute_image (const char *f_name, uint32_t active_system, 
                            bool verbose) 
{
    int read_sz = 0;
    int sent_sz = 0;
    int err = 0;
    FILE *fp = NULL; 
    unsigned char *bfr = NULL;
    uint32_t data_remaining;
    uint32_t bytes_to_send;
    struct pb_pbi pbi;
    uint8_t zero_padding[511];

    memset(zero_padding,0,511);

    fp = fopen (f_name,"rb");

    if (fp == NULL) {
        printf ("Could not open file: %s\n",f_name);
        return -1;
    }

    bfr =  malloc(1024*1024);
    
    if (bfr == NULL) {
        printf ("Could not allocate memory\n");
        return -1;
    }

    read_sz = fread(&pbi, 1, sizeof(struct pb_pbi), fp);

    err = pb_write(PB_CMD_BOOT_RAM, active_system,verbose,0,0,
                            (uint8_t *) &pbi, sizeof(struct pb_pbi));
    
    if (err != PB_OK)
        return err;

    err = pb_read_result_code();

    if (err != PB_OK)
        return err;

    for (uint32_t i = 0; i < pbi.hdr.no_of_components; i++) {

        fseek (fp, pbi.comp[i].component_offset, SEEK_SET);
        data_remaining = pbi.comp[i].component_size;
        
        while ((read_sz = fread(bfr, 1, 1024*1024, fp)) >0) {

            if (read_sz > data_remaining)
                bytes_to_send = data_remaining;
            else
                bytes_to_send = read_sz;

            err = pb_write_bulk(bfr, bytes_to_send, &sent_sz);

            data_remaining = data_remaining - bytes_to_send;
            
            if (!data_remaining)
                break;

            if (err != 0) {
                printf ("USB: Bulk xfer error, err=%i\n",err);
                goto err_xfer;
            }
        }
    }

    err = pb_read_result_code();

    if (err != PB_OK)
        return err;

err_xfer:


    free(bfr);
    fclose(fp);
    return err;
}

