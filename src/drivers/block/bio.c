#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/errors.h>
#include <drivers/block/bio.h>

struct bio_device {
    uuid_t uu;
    char description[37];
    uint64_t first_lba;
    uint64_t last_lba;
    uint32_t flags;
    size_t block_sz;
    bio_read_t read;
    bio_write_t write;
    bio_read_t async_read;
    bio_write_t async_write;
    bio_call_t async_wait;
    bio_call_t install_partition_table;
    bio_resize_call_t resize_partition;
    bool valid;
};

static struct bio_device bio_pool[CONFIG_DRIVERS_BIO_MAX_DEVS];
static unsigned int n_bios = 0;

static int check_dev(bio_dev_t dev)
{
    if (dev < 0 || dev > CONFIG_DRIVERS_BIO_MAX_DEVS)
        return -PB_ERR_PARAM;
    if (bio_pool[dev].valid == false)
        return -PB_ERR_PARAM;

    return PB_OK;
}

bio_dev_t bio_allocate(int first_lba, int last_lba, size_t block_size,
                       const uuid_t uu, const char *description)
{
    if (n_bios == CONFIG_DRIVERS_BIO_MAX_DEVS)
        return -PB_ERR_MEM;
    if (first_lba < 0 || last_lba < 0 || block_size == 0)
        return -PB_ERR_PARAM;
    if (first_lba > last_lba)
        return -PB_ERR_PARAM;

    memset(&bio_pool[n_bios], 0, sizeof(struct bio_device));
    bio_pool[n_bios].first_lba = first_lba;
    bio_pool[n_bios].last_lba = last_lba;
    bio_pool[n_bios].block_sz = block_size;
    uuid_copy(bio_pool[n_bios].uu, uu);
    strncpy(bio_pool[n_bios].description, description,
               sizeof(bio_pool[n_bios].description) - 1);
    bio_pool[n_bios].valid = true;

#if (LOGLEVEL > 1)
    char uu_str[37];
    uuid_unparse(uu, uu_str);
    LOG_INFO("%s: %i - %i, %s", uu_str, first_lba, last_lba, description);
#endif
    return n_bios++;
}

bio_dev_t bio_allocate_parent(bio_dev_t parent,
                              int first_lba,
                              int last_lba,
                              size_t block_size,
                              const uuid_t uu,
                              const char *description)
{
    int rc;
    bio_dev_t new;

    rc = check_dev(parent);
    if (rc != PB_OK)
        return rc;

    new = bio_allocate(first_lba, last_lba, block_size, uu, description);

    if (new < 0)
        return new;

    bio_pool[new].flags = bio_pool[parent].flags;

    bio_pool[new].read = bio_pool[parent].read;
    bio_pool[new].write = bio_pool[parent].write;
    bio_pool[new].async_read = bio_pool[parent].async_read;
    bio_pool[new].async_write = bio_pool[parent].async_write;
    bio_pool[new].async_wait = bio_pool[parent].async_wait;

    return new;
}

bool bio_valid(bio_dev_t dev)
{
    if (check_dev(dev) != 0)
        return false;
    return bio_pool[dev].valid;
}

int bio_set_ios(bio_dev_t dev, bio_read_t read, bio_write_t write)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;

    bio_pool[dev].read = read;
    bio_pool[dev].write = write;

    return PB_OK;
}

ssize_t bio_size(bio_dev_t dev)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;

    return (bio_pool[dev].last_lba - bio_pool[dev].first_lba + 1) * bio_pool[dev].block_sz;
}

ssize_t bio_block_size(bio_dev_t dev)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    return bio_pool[dev].block_sz;
}

static int check_lba_range(bio_dev_t dev, int lba, size_t length)
{
    if (lba < 0)
        return -1;

    size_t n_blocks = length / bio_pool[dev].block_sz;
    if (length % bio_pool[dev].block_sz)
        n_blocks++;

    if ((bio_pool[dev].first_lba + lba + n_blocks - 1) > bio_pool[dev].last_lba) {
        return -1;
    }

    if (lba < 0)
        return -1;

    return PB_OK;
}

int bio_read(bio_dev_t dev, int lba, size_t length, uintptr_t buf)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    if (bio_pool[dev].read == NULL)
        return -PB_ERR_NOT_SUPPORTED;
    if (check_lba_range(dev, lba, length) != 0)
        return -PB_ERR_PARAM;

    return bio_pool[dev].read(dev, bio_pool[dev].first_lba + lba, length, buf);
}

int bio_write(bio_dev_t dev, int lba, size_t length, const uintptr_t buf)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    if (bio_pool[dev].write == NULL)
        return -PB_ERR_NOT_SUPPORTED;
    if (check_lba_range(dev, lba, length) != 0)
        return -PB_ERR_IO;

    return bio_pool[dev].write(dev, bio_pool[dev].first_lba + lba, length, buf);
}

int bio_get_hal_flags(bio_dev_t dev)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    return (bio_pool[dev].flags >> 24) & 0xff;
}

int bio_set_hal_flags(bio_dev_t dev, uint8_t flags)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    bio_pool[dev].flags &= ~(0xff000000);
    bio_pool[dev].flags |= (flags << 24);
    return PB_OK;
}

int bio_get_flags(bio_dev_t dev)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    return (bio_pool[dev].flags & 0xffff);
}

int bio_set_flags(bio_dev_t dev, uint16_t flags)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    bio_pool[dev].flags &= ~(0x0000FFFF);
    bio_pool[dev].flags |= (flags & 0xFFFF);
    return PB_OK;
}


const char * bio_get_description(bio_dev_t dev)
{
    if (check_dev(dev) != PB_OK)
        return NULL;
    return bio_pool[dev].description;
}

const unsigned char * bio_get_uu(bio_dev_t dev)
{
    if (check_dev(dev) != PB_OK)
        return NULL;
    return bio_pool[dev].uu;
}

int bio_get_last_block(bio_dev_t dev)
{
    if (check_dev(dev) != PB_OK)
        return -PB_ERR_PARAM;
    return bio_pool[dev].last_lba;
}

int bio_get_first_block(bio_dev_t dev)
{
    if (check_dev(dev) != PB_OK)
        return -PB_ERR_PARAM;
    return bio_pool[dev].first_lba;
}

bio_dev_t bio_get_part_by_uu(const uuid_t uu)
{
    for (unsigned int i = 0; i < CONFIG_DRIVERS_BIO_MAX_DEVS; i++) {
        if (!bio_pool[i].valid)
            break;
        if (uuid_compare(uu, bio_pool[i].uu) == 0) {
            return i;
        }
    }

    return -PB_ERR_NOT_FOUND;
}

bio_dev_t bio_get_part_by_uu_str(const char *uu_str)
{
    uuid_t uu;
    if (uuid_parse(uu_str, uu) != 0)
        return -PB_ERR_PARAM;
    return bio_get_part_by_uu(uu);
}

int bio_install_partition_tables(void)
{
    int rc;

    for (bio_dev_t dev = 0; bio_valid(dev); dev++) {
        if (bio_pool[dev].install_partition_table) {
            rc = bio_pool[dev].install_partition_table(dev);

            if (rc < 0)
                return rc;
        }
    }

    return PB_OK;
}

int bio_resize_part(bio_dev_t dev, size_t length)
{
    if (check_dev(dev) != PB_OK)
        return -PB_ERR_PARAM;
    if (bio_pool[dev].resize_partition)
        return bio_pool[dev].resize_partition(dev, length);
    else
        return -PB_ERR_NOT_SUPPORTED;
}

int bio_set_install_partition_cb(bio_dev_t dev, bio_call_t cb)
{
    if (check_dev(dev) != PB_OK)
        return -PB_ERR_PARAM;
    bio_pool[dev].install_partition_table = cb;
    return PB_OK;
}

int bio_set_resize_cb(bio_dev_t dev, bio_resize_call_t cb)
{
    if (check_dev(dev) != PB_OK)
        return -PB_ERR_PARAM;
    bio_pool[dev].resize_partition = cb;
    return PB_OK;
}