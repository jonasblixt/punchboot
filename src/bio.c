#include <pb/bio.h>
#include <pb/errors.h>
#include <pb/pb.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* A device's flags word: BIO_FLAG_* in bits 15:0, the HAL's in 31:24 */
#define BIO_FLAGS_MASK      0x0000FFFF
#define BIO_HAL_FLAGS_SHIFT 24
#define BIO_HAL_FLAGS_MASK  0xFF000000

struct bio_device {
    uuid_t uu;
    char description[37];
    lba_t first_lba;
    lba_t last_lba;
    uint32_t flags;
    size_t block_sz;
    bio_read_t read;
    bio_write_t write;
    bio_erase_t erase;
    bio_call_t install_partition_table;
    bio_call_t write_done;
    uintptr_t private;
    /* A slot below n_bios that isn't valid is a released partition's: its
     * handle is dead, and the slot isn't used again before the reset
     * (bio.md 4) */
    bool valid;
    /* A partition: made with bio_allocate_parent(), of the device 'parent' */
    bool is_part;
    bio_dev_t parent;
    /* The device a table is on: what its partitions start with, the flags
     * word it had when the table's install callback was set. The core takes
     * WRITABLE from the device after that (bio.md 2.4), and a partition that
     * an install makes later starts as one made at the set up */
    uint32_t table_flags;
    /* A partition of the table that is being installed, which the new table
     * hasn't named yet */
    bool pending;
};

static struct bio_device bio_pool[CONFIG_BIO_MAX_DEVS];
static unsigned int n_bios = 0;
/* The partitions that were released: none before the first install that
 * changes a table, so none on the boot path */
static unsigned int n_released;

/* The install that runs (bio_install_partition_table()): the device whose
 * table is installed, and whether it was written */
static struct {
    bool active;
    bio_dev_t dev;
    bool written;
} install;

/* The block buffer: where the core makes a length that isn't whole blocks
 * whole, so that a driver sees whole blocks only (bio.md 3.1, 3.2). It holds
 * the largest block (bio_allocate()), and a driver's device writes into it:
 * it starts on a cache line and owns its lines (drivers.md 4.3) */
#define BIO_BLOCK_BUF_ALIGN 64
static uint8_t block_buf[round_up(CONFIG_BIO_MAX_BLOCK_SIZE, BIO_BLOCK_BUF_ALIGN)] __section(".no_"
                                                                                             "init")
    __aligned(BIO_BLOCK_BUF_ALIGN);

static int check_dev(bio_dev_t dev)
{
    if (dev < 0 || dev >= CONFIG_BIO_MAX_DEVS)
        return -PB_ERR_PARAM;
    if (bio_pool[dev].valid == false)
        return -PB_ERR_PARAM;

    return PB_OK;
}

void bio_reset(void)
{
    memset(bio_pool, 0, sizeof(bio_pool));
    memset(&install, 0, sizeof(install));
    n_bios = 0;
    n_released = 0;
}

/* A write or an erase of the device whose table is being installed, or of
 * one of its partitions: from here the table on the medium isn't the one
 * the partitions were made of */
static void install_note_write(bio_dev_t dev)
{
    if (!install.active)
        return;

    if (dev == install.dev || (bio_pool[dev].is_part && bio_pool[dev].parent == install.dev))
        install.written = true;
}

/* Releases a partition: its handle is dead, -PB_ERR_PARAM for whoever holds
 * it, and its slot stays taken, so that the handle never comes to mean
 * another device (bio.md 4) */
static void release(bio_dev_t dev)
{
    bio_pool[dev].valid = false;
    bio_pool[dev].pending = false;
    n_released++;
}

bio_dev_t bio_allocate(lba_t first_lba,
                       lba_t last_lba,
                       size_t block_size,
                       const uuid_t uu,
                       const char *description)
{
    if (n_bios == CONFIG_BIO_MAX_DEVS)
        return -PB_ERR_MEM;
    /* The largest block a device may have (bio.md 2.3) */
    if (block_size == 0 || block_size > CONFIG_BIO_MAX_BLOCK_SIZE)
        return -PB_ERR_PARAM;
    if (first_lba > last_lba)
        return -PB_ERR_PARAM;
    /* A UUID names at most one device (bio.md 2.6) */
    if (bio_get_part_by_uu(uu) >= 0)
        return -PB_ERR_PARAM;

    memset(&bio_pool[n_bios], 0, sizeof(struct bio_device));
    bio_pool[n_bios].first_lba = first_lba;
    bio_pool[n_bios].last_lba = last_lba;
    bio_pool[n_bios].block_sz = block_size;
    uuid_copy(bio_pool[n_bios].uu, uu);
    strncpy(bio_pool[n_bios].description, description, sizeof(bio_pool[n_bios].description) - 1);
    bio_pool[n_bios].valid = true;
    bio_pool[n_bios].parent = -1;

#if LOG_MAX_LEVEL >= 2
    char uu_str[37];
    uuid_unparse(uu, uu_str);
    LOG_INFO("%s: %i - %i, %s", uu_str, first_lba, last_lba, description);
#endif
    return n_bios++;
}

bio_dev_t bio_allocate_parent(bio_dev_t parent,
                              lba_t first_lba,
                              lba_t last_lba,
                              size_t block_size,
                              const uuid_t uu,
                              const char *description)
{
    int rc;
    bio_dev_t new;

    rc = check_dev(parent);
    if (rc != PB_OK)
        return rc;

    /* A partition lies inside its parent (bio.md 2.2): its blocks are the
     * parent's, as the driver counts them */
    if (first_lba < bio_pool[parent].first_lba || last_lba > bio_pool[parent].last_lba)
        return -PB_ERR_PARAM;

    /* An install (bio.md 4): a partition that the new table leaves as it
     * was, the same UUID and the same blocks, keeps its handle and its
     * flags. One that changed is released, and the new one gets a new
     * handle below */
    if (install.active && install.dev == parent) {
        bio_dev_t old = bio_get_part_by_uu(uu);

        if (old >= 0 && bio_pool[old].pending) {
            struct bio_device *d = &bio_pool[old];

            if (d->first_lba == first_lba && d->last_lba == last_lba && d->block_sz == block_size) {
                d->pending = false;
                /* The name is the new table's. strncpy() fills the field,
                 * whose last byte stays the terminator */
                strncpy(d->description, description, sizeof(d->description) - 1);
                return old;
            }

            release(old);
        }
    }

    new = bio_allocate(first_lba, last_lba, block_size, uu, description);

    if (new < 0)
        return new;

    bio_pool[new].is_part = true;
    bio_pool[new].parent = parent;

    /* A table's partitions start with the flags its device had when the
     * table was set up, see struct bio_device */
    if (bio_pool[parent].install_partition_table)
        bio_pool[new].flags = bio_pool[parent].table_flags;
    else
        bio_pool[new].flags = bio_pool[parent].flags;

    /* A partition whose UUID a released one of this device had, the one it
     * replaces or one of a table before: it starts with no flag which that
     * one didn't have. What a board took from a partition, VISIBLE from one
     * no host may reach (command-mode.md 5.3), isn't given back by an
     * install: no hook of the board's runs again to take it (bio.md 4).
     * The slots are in the order they were given out, so the first one
     * found is the last that was released */
    for (bio_dev_t i = new - 1; n_released > 0 && i >= 0; i--) {
        const struct bio_device *d = &bio_pool[i];

        if (!d->valid && d->is_part && d->parent == parent && uuid_compare(d->uu, uu) == 0) {
            bio_pool[new].flags &= (d->flags | BIO_HAL_FLAGS_MASK);
            break;
        }
    }

    bio_pool[new].read = bio_pool[parent].read;
    bio_pool[new].write = bio_pool[parent].write;
    bio_pool[new].erase = bio_pool[parent].erase;
    bio_pool[new].private = bio_pool[parent].private;

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

int bio_set_ios_erase(bio_dev_t dev, bio_erase_t erase)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;

    bio_pool[dev].erase = erase;

    return PB_OK;
}

int bio_set_private(bio_dev_t dev, uintptr_t priv)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;

    bio_pool[dev].private = priv;

    return PB_OK;
}

uintptr_t bio_get_private(bio_dev_t dev)
{
    if (check_dev(dev) != PB_OK)
        return 0;
    return bio_pool[dev].private;
}

int64_t bio_size(bio_dev_t dev)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;

    return bio_get_no_of_blocks(dev) * (int64_t)bio_pool[dev].block_sz;
}

ssize_t bio_block_size(bio_dev_t dev)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    return bio_pool[dev].block_sz;
}

static int check_lba_range(bio_dev_t dev, lba_t lba, size_t length)
{
    /* 64-bit arithmetic, so that neither 'first_lba + lba' nor the block
     * count can wrap around to a range that looks valid */
    uint64_t n_blocks = length / bio_pool[dev].block_sz;
    uint64_t part_blocks = (uint64_t)bio_pool[dev].last_lba - bio_pool[dev].first_lba + 1;

    if (length % bio_pool[dev].block_sz)
        n_blocks++;

    if (lba >= part_blocks || n_blocks > part_blocks - lba) {
        return -1;
    }

    return PB_OK;
}

int bio_read(bio_dev_t dev, lba_t lba, size_t length, void *buf)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    if (bio_pool[dev].read == NULL)
        return -PB_ERR_NOT_SUPPORTED;
    if (check_lba_range(dev, lba, length) != 0) {
        LOG_ERR("Range error, lba=%u, length=%zu", lba, length);
        return -PB_ERR_PARAM;
    }
    /* Nothing to read: the driver isn't called (bio.md 3.1) */
    if (length == 0)
        return PB_OK;

    /* A driver sees whole blocks only (bio.md 3.1). The range check counted
     * the last, partial block, so that it is inside the device */
    const struct bio_device *d = &bio_pool[dev];
    size_t tail = length % d->block_sz;
    size_t whole = length - tail;
    lba_t first = d->first_lba + lba;

    if (whole > 0) {
        rc = d->read(dev, first, whole, buf);
        if (rc != PB_OK)
            return rc;
    }

    if (tail > 0) {
        /* The last block is read into the block buffer, and the bytes asked
         * for are copied: exactly 'length' bytes are written to 'buf' */
        rc = d->read(dev, first + (lba_t)(whole / d->block_sz), d->block_sz, block_buf);
        if (rc != PB_OK)
            return rc;
        memcpy((uint8_t *)buf + whole, block_buf, tail);
    }

    return PB_OK;
}

int bio_write(bio_dev_t dev, lba_t lba, size_t length, const void *buf)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    if (bio_pool[dev].write == NULL)
        return -PB_ERR_NOT_SUPPORTED;
    if (check_lba_range(dev, lba, length) != 0) {
        LOG_ERR("Range error, lba=%u, length=%zu", lba, length);
        return -PB_ERR_PARAM;
    }
    /* Nothing to write: the driver isn't called (bio.md 3.2) */
    if (length == 0)
        return PB_OK;

    install_note_write(dev);

    /* A driver sees whole blocks only (bio.md 3.2), as in bio_read() */
    const struct bio_device *d = &bio_pool[dev];
    size_t tail = length % d->block_sz;
    size_t whole = length - tail;
    lba_t first = d->first_lba + lba;

    if (whole > 0) {
        rc = d->write(dev, first, whole, buf);
        if (rc != PB_OK)
            return rc;
    }

    if (tail > 0) {
        /* The last block is written from the block buffer: the bytes given,
         * then zeros. Nothing is read back to keep what the block held */
        memcpy(block_buf, (const uint8_t *)buf + whole, tail);
        memset(block_buf + tail, 0, d->block_sz - tail);
        rc = d->write(dev, first + (lba_t)(whole / d->block_sz), d->block_sz, block_buf);
        if (rc != PB_OK)
            return rc;
    }

    return PB_OK;
}

int bio_erase(bio_dev_t dev, lba_t first_lba, size_t count)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    if (bio_pool[dev].erase == NULL)
        return -PB_ERR_NOT_SUPPORTED;
    if (count == 0)
        return PB_OK;
    if (count > SIZE_MAX / bio_pool[dev].block_sz ||
        check_lba_range(dev, first_lba, count * bio_pool[dev].block_sz) != 0) {
        LOG_ERR("Range error, lba=%u, count=%zu", first_lba, count);
        return -PB_ERR_PARAM;
    }

    install_note_write(dev);

    return bio_pool[dev].erase(dev, bio_pool[dev].first_lba + first_lba, count);
}

int bio_get_hal_flags(bio_dev_t dev)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    return (bio_pool[dev].flags & BIO_HAL_FLAGS_MASK) >> BIO_HAL_FLAGS_SHIFT;
}

int bio_set_hal_flags(bio_dev_t dev, uint8_t flags)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    bio_pool[dev].flags &= ~(BIO_HAL_FLAGS_MASK);
    bio_pool[dev].flags |= ((uint32_t)flags << BIO_HAL_FLAGS_SHIFT);
    return PB_OK;
}

int bio_get_flags(bio_dev_t dev)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    return (bio_pool[dev].flags & BIO_FLAGS_MASK);
}

int bio_set_flags(bio_dev_t dev, uint16_t flags)
{
    int rc;

    rc = check_dev(dev);
    if (rc != PB_OK)
        return rc;
    bio_pool[dev].flags &= ~(BIO_FLAGS_MASK);
    bio_pool[dev].flags |= (flags & BIO_FLAGS_MASK);
    return PB_OK;
}

int bio_clear_set_flags(bio_dev_t dev, uint16_t clear_flags, uint16_t set_flags)
{
    int flags = bio_get_flags(dev);

    if (flags < 0)
        return flags;

    return bio_set_flags(dev, ((uint16_t)flags & ~clear_flags) | set_flags);
}

const char *bio_get_description(bio_dev_t dev)
{
    if (check_dev(dev) != PB_OK)
        return NULL;
    return bio_pool[dev].description;
}

const unsigned char *bio_get_uu(bio_dev_t dev)
{
    if (check_dev(dev) != PB_OK)
        return NULL;
    return bio_pool[dev].uu;
}

/* 64 bits: every lba_t, and a count of up to 2^32 blocks, beside the
 * negative error (bio.md 2.6) */
int64_t bio_get_last_block(bio_dev_t dev)
{
    if (check_dev(dev) != PB_OK)
        return -PB_ERR_PARAM;
    return bio_pool[dev].last_lba;
}

int64_t bio_get_first_block(bio_dev_t dev)
{
    if (check_dev(dev) != PB_OK)
        return -PB_ERR_PARAM;
    return bio_pool[dev].first_lba;
}

int64_t bio_get_no_of_blocks(bio_dev_t dev)
{
    if (check_dev(dev) != PB_OK)
        return -PB_ERR_PARAM;
    return (int64_t)bio_pool[dev].last_lba - bio_pool[dev].first_lba + 1;
}

bio_dev_t bio_get_part_by_uu(const uuid_t uu)
{
    if (uu == NULL)
        return -PB_ERR_NOT_FOUND;

    /* Every slot that was given out: a released partition's can lie between
     * two devices (bio.md 2.6) */
    for (unsigned int i = 0; i < n_bios; i++) {
        if (!bio_pool[i].valid)
            continue;
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

/*
 * An installed table is in effect at once (bio.md 3.5, 4). The table's
 * callback writes the table and then makes its partitions with
 * bio_allocate_parent(), as it does at the set up. Around it:
 *
 * - The device's partitions are pending. bio_allocate_parent() takes one
 *   that the new table leaves as it was out of that state, with its handle
 *   and its flags, and releases one whose blocks changed.
 * - Afterwards the ones that are still pending are released: they are gone
 *   from the table. A partition the new table adds has a new handle.
 * - A callback that failed before it wrote anything (a variant the board
 *   doesn't have, a layout that doesn't fit) changed nothing: the old table
 *   stands, with its partitions.
 * - A callback that failed after it wrote leaves what it made. The table on
 *   the medium isn't the old one any more, or not for sure, so the old
 *   partitions it didn't name are released. When the pool was full that is
 *   the new table less the partitions that got no handle, and a reset has
 *   them all; when a write failed the device has no partitions, and the
 *   install is run again (command-mode.md 5.3).
 */
int bio_install_partition_table(const uuid_t part_uu, int variant)
{
    bio_dev_t dev = bio_get_part_by_uu(part_uu);
    int rc;

    if (dev < 0) {
        return dev;
    }

    LOG_DBG("Installing part tbl variant %i", variant);

    if (bio_pool[dev].install_partition_table == NULL)
        return -PB_ERR_NOT_SUPPORTED;

    /* One at a time: a callback doesn't install */
    if (install.active)
        return -PB_ERR_STATE;

    for (unsigned int i = 0; i < n_bios; i++) {
        if (bio_pool[i].valid && bio_pool[i].is_part && bio_pool[i].parent == dev)
            bio_pool[i].pending = true;
    }

    install.active = true;
    install.dev = dev;
    install.written = false;

    rc = bio_pool[dev].install_partition_table(dev, variant);

    install.active = false;

    for (unsigned int i = 0; i < n_bios; i++) {
        if (!bio_pool[i].pending)
            continue;

        if (install.written) {
            LOG_INFO("Released %u, %s", i, bio_pool[i].description);
            release((bio_dev_t)i);
        } else {
            bio_pool[i].pending = false;
        }
    }

    return rc;
}

int bio_set_install_partition_cb(bio_dev_t dev, bio_call_t cb)
{
    if (check_dev(dev) != PB_OK)
        return -PB_ERR_PARAM;
    bio_pool[dev].install_partition_table = cb;
    /* What the table's partitions start with, see struct bio_device */
    bio_pool[dev].table_flags = bio_pool[dev].flags;
    return PB_OK;
}

int bio_write_done(bio_dev_t dev)
{
    if (check_dev(dev) != PB_OK)
        return -PB_ERR_PARAM;

    if (bio_pool[dev].write_done)
        return bio_pool[dev].write_done(dev, 0);

    return PB_OK;
}

int bio_set_write_done_cb(bio_dev_t dev, bio_call_t cb)
{
    if (check_dev(dev) != PB_OK)
        return -PB_ERR_PARAM;
    bio_pool[dev].write_done = cb;
    return PB_OK;
}
