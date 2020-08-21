#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fs.h>

#include "crc.h"
#include "gpt.h"

static const uint64_t __gpt_header_signature = 0x5452415020494645ULL;

static inline uint32_t efi_crc32(const void *buf, uint32_t sz)
{
    return (crc32(~0L, buf, sz) ^ ~0L);
}

static void uuid_to_guid(const uint8_t *uuid, uint8_t *guid)
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

static int gpt_has_valid_header(struct gpt_header *hdr)
{
    if (hdr->signature != __gpt_header_signature)
        return -GPT_INVALID_HEADER;

    uint32_t crc_tmp = hdr->hdr_crc;
    hdr->hdr_crc = 0;

    if (efi_crc32((uint8_t*) hdr, sizeof(struct gpt_header) -
                                   GPT_HEADER_RSZ) != crc_tmp)
    {
        return -GPT_INVALID_HEADER;
    }

    hdr->hdr_crc = crc_tmp;

    return GPT_OK;
}

static int gpt_has_valid_part_array(struct gpt_header *hdr,
                                    struct gpt_part_hdr *part)
{
    uint32_t crc_tmp = hdr->part_array_crc;

    if (efi_crc32((uint8_t *) part, sizeof(struct gpt_part_hdr) *
                        hdr->no_of_parts) != crc_tmp)
    {
        return -GPT_PART_CRC_ERROR;
    }

    hdr->part_array_crc = crc_tmp;

    return GPT_OK;
}

static int gpt_is_valid(struct gpt_header *hdr, struct gpt_part_hdr *part)
{
    int rc = GPT_OK;

    rc = gpt_has_valid_header(hdr);

    if (rc != GPT_OK)
        return rc;

    return gpt_has_valid_part_array(hdr, part);
}

/* Convert 2 chars to a hex number */
static unsigned char str_to_hex(const char* str)
{
    char buffer[3];
    unsigned char hex = 0;
    unsigned int i = 0;

    strncpy(buffer, str, 2);
    buffer[2] = 0;
    return (unsigned char)strtol(buffer, NULL, 16);
}

int gpt_part_by_uuid(struct gpt_table *gpt, const char *uuid_str,
                        struct gpt_part_hdr **part)
{
    unsigned char guid[16];
    unsigned char uuid[16];
    unsigned int i = 0;
    unsigned int hex = 0;
    unsigned int uuid_str_len = strlen(uuid_str);

    for (; i < uuid_str_len && hex < sizeof(uuid);)
    {
        if (uuid_str[i] == '-')
        {
            i++;
            continue;
        }

        uuid[hex++] = str_to_hex(&uuid_str[i]);
        i += 2;
    }

    printf("%s -> %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n", uuid_str, uuid[0], uuid[2], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7], uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

    uuid_to_guid((uint8_t *) uuid, guid);

    for (i = 0; i < gpt->hdr.no_of_parts; i++)
    {
        struct gpt_part_hdr *p = &gpt->part[i];

        if (p->first_lba == 0)
            return GPT_INVALID_HEADER;

        if (memcmp(p->uuid, guid, 16) == 0)
        {
            (*part) = p;
            return GPT_OK;
        }
    }

    return GPT_ERROR;
}

int gpt_init(const char *device, struct gpt_table **gpt_)
{
    struct gpt_table *gpt = malloc(sizeof(struct gpt_table));
    int rc = GPT_OK;
    uint64_t no_of_blocks;
    off_t offset;
    size_t sz;
    int fd;

    if (!gpt)
        return GPT_ERROR;

    memset(gpt, 0, sizeof(*gpt));
    *gpt_ = gpt;
    gpt->device = device;

    fd = open(device, O_RDONLY);

    if (fd == -1)
    {
        rc = -GPT_ERROR;
        goto err_free_out;
    }

    if (ioctl(fd, BLKGETSIZE, &no_of_blocks) != 0)
    {
        rc = -GPT_ERROR;
        goto err_close_fd_out;
    }

    /* Read primary and backup GPT headers and parition tables */

    if (lseek(fd, 512, SEEK_SET) != 512)
    {
        rc = -GPT_ERROR;
        goto err_close_fd_out;
    }

    sz = read(fd, &gpt->hdr, sizeof(gpt->hdr));

    if (sz != sizeof(gpt->hdr))
    {
        rc = -GPT_ERROR;
        goto err_close_fd_out;
    }

    if ((unsigned int)lseek(fd, gpt->hdr.entries_start_lba*512, SEEK_SET) != \
                  gpt->hdr.entries_start_lba*512)
    {
        rc = -GPT_ERROR;
        goto err_close_fd_out;
    }

    sz = read(fd, gpt->part, sizeof(struct gpt_part_hdr)*128);

    if (sz != sizeof(struct gpt_part_hdr)*128)
    {
        rc = -GPT_ERROR;
        goto err_close_fd_out;
    }

    if (gpt_is_valid(&gpt->hdr, gpt->part) == GPT_OK)
    {
        rc = GPT_OK;
        goto ok_close_fd_out;
    }

    /* Try backup table */

    offset = no_of_blocks*512 - (sizeof(struct gpt_header)/512);

    if (lseek(fd, offset, SEEK_SET) != offset)
    {
        rc = -GPT_ERROR;
        goto err_close_fd_out;
    }

    if ((unsigned int)lseek(fd, gpt->hdr.entries_start_lba*512, SEEK_SET) != \
                  gpt->hdr.entries_start_lba*512)

    sz = read(fd, gpt->part, sizeof(struct gpt_part_hdr)*128);

    if (sz != sizeof(struct gpt_part_hdr)*128)
    {
        rc = -GPT_ERROR;
        goto err_close_fd_out;
    }

    sz = read(fd, &gpt->hdr, sizeof(gpt->hdr));

    if (sz != sizeof(gpt->hdr))
    {
        rc = -GPT_ERROR;
        goto err_close_fd_out;
    }

    if (gpt_is_valid(&gpt->hdr, gpt->part) != GPT_OK)
    {
        rc = -GPT_ERROR;
        goto err_close_fd_out;
    }

    rc = GPT_OK;

ok_close_fd_out:
    close(fd);
    return rc;
err_close_fd_out:
    close(fd);
err_free_out:
    free(gpt);
    return rc;
}

int gpt_part_name(struct gpt_table *gpt, uint8_t part_index,
                    char *buf, size_t size)
{
    if (part_index > gpt->hdr.no_of_parts)
        return -GPT_ERROR;

    struct gpt_part_hdr *p = &gpt->part[part_index];

    if (p->first_lba == 0)
        return -GPT_ERROR;

    snprintf(buf, size, "%sp%u", gpt->device, (part_index+1));
    return GPT_OK;
}

int gpt_free(struct gpt_table *gpt_table)
{
    free(gpt_table);
    return GPT_OK;
}

int gpt_get_partname(struct gpt_table *gpt, char *uuid, char *part_name)
{
    char part_name_buf[64];
    uint8_t guid[16];

    for (uint8_t p = 0; p < gpt->hdr.no_of_parts; p++)
    {
        if (gpt->part[p].first_lba == 0)
            break;

        uuid_to_guid(gpt->part[p].uuid, guid);
        gpt_part_name(gpt, p, part_name_buf, sizeof(part_name_buf));

        if (memcmp(guid, uuid, sizeof(guid)) == 0)
        {
            gpt_part_name(gpt, p, part_name_buf, sizeof(part_name_buf));
            memcpy(part_name, part_name_buf, strlen(part_name_buf));
            return GPT_OK;
        }
    }

    return -GPT_ERROR;
}
