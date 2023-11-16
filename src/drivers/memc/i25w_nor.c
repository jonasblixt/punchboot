
#include <pb/pb.h>
#include <pb/bio.h>
#include <pb/delay.h>
#include <drivers/memc/i25w_nor.h>
#include <drivers/memc/imx_flexspi.h>

#define UUID_1bd35a54_485d_4fb6_a822_cb58c3c5a068 (const unsigned char *) "\x1b\xd3\x5a\x54\x48\x5d\x4f\xb6\xa8\x22\xcb\x58\xc3\xc5\xa0\x68"

#define NOR_CMD_LUT_SEQ_IDX_READ_FAST_QUAD     0
#define NOR_CMD_LUT_SEQ_IDX_READSTATUS         1
#define NOR_CMD_LUT_SEQ_IDX_WRITEENABLE        2
#define NOR_CMD_LUT_SEQ_IDX_WRITEDISABLE       3
#define NOR_CMD_LUT_SEQ_IDX_ERASESECTOR        4
#define NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_QUAD   5
#define NOR_CMD_LUT_SEQ_IDX_ERASECHIP          6
#define NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_SINGLE 7
#define NOR_CMD_LUT_SEQ_IDX_READ_NORMAL        8
#define NOR_CMD_LUT_SEQ_IDX_READID             9
#define NOR_CMD_LUT_SEQ_IDX_WRITESTATUSREG     10
#define NOR_CMD_LUT_SEQ_IDX_ENTERQPI           11
#define NOR_CMD_LUT_SEQ_IDX_EXITQPI            12
#define NOR_CMD_LUT_SEQ_IDX_READSTATUSREG      13
#define NOR_CMD_LUT_SEQ_IDX_READ_FAST          14
#define NOR_CMD_LUT_SEQ_IDX_ERASEBLOCK         15

/* IS25WP032D JEDEC IDs */
#define IS25WP032D_JEDEC_ID_ISSI     0x9d
#define IS25WP032D_JEDEC_DEVICE_TYPE 0x70
#define IS25WP032D_JEDEC_CAPACITY    0x16

/* I25W Registers ****************************************************************/
/* Status register bit definitions                                               */

#define STATUS_WIP_MASK              (1 << 0) /* Bit 0: Write in progress bit         */
#define STATUS_READY                 (0 << 0) /*   0 = Not in progress (device ready) */
#define STATUS_WIP                   (1 << 0) /*   1 = In progress (device not ready) */

#define STATUS_WEL_MASK              (1 << 1) /* Bit 1: Write enable latch status     */
#define STATUS_WEL_DISABLED          (0 << 1) /*   0 = Not Write Enabled              */
#define STATUS_WEL_ENABLED           (1 << 1) /*   1 = Write Enabled                  */

#define READ_BUF_SIZE                4
#define CMD_BUF_SIZE                 4

/* Chip Geometries ******************************************************************/

/* IS25WP032D (32Mbit / 4MB) memory capacity */
/* NOTE: All values are in bytes accept the block size since it blksize_t is only
 *       16 bits. */
#define IS25WP032D_Size              (0x400000)
#define IS25WP032D_BLOCK_SIZE_KB     (0x40)
#define IS25WP032D_SECTOR_SIZE       (0x1000)
#define IS25WP032D_PAGE_SIZE         (0x100)


static int i25w_transfer(uint8_t lut_command,
                         uint32_t address,
                         enum flexspi_command_type command_type,
                         void *data,
                         size_t data_size)
{
    struct flexspi_transfer transfer;
    transfer.deviceAddress = address;
    transfer.port = kFLEXSPI_PortA1;
    transfer.cmdType = command_type;
    transfer.SeqNumber = 1;
    transfer.seqIndex = lut_command;
    transfer.data = data;
    transfer.dataSize = data_size;

    int rc = imx_flexspi_transferblocking(&transfer);
    if (rc != 0) {
        LOG_ERR("Spi command failed with error: %i\n", rc);
        return rc;
    }

    return 0;
}

static uint8_t i25w_read_status(void)
{
    uint8_t read_buf[4];

    i25w_transfer(NOR_CMD_LUT_SEQ_IDX_READSTATUS, 0, kFLEXSPI_Read, read_buf, sizeof(read_buf));
    // TODO: Error handling?
    return read_buf[0];
}

static void i25w_wait_until_not_busy(int polling_time_us)
{
    uint8_t status = i25w_read_status();

    while ((status & STATUS_WIP) != 0) {
        pb_delay_us(polling_time_us);
        status = i25w_read_status();
    }
}

static int i25w_page_program(uint32_t address, uint8_t *data, size_t size)
{
    int rc = 0;
    uint32_t bytes_to_next_page_boundry =
        (uint32_t)IS25WP032D_PAGE_SIZE - (address & (uint32_t)(IS25WP032D_PAGE_SIZE - 1));

    if ((size > bytes_to_next_page_boundry) || (size > IS25WP032D_PAGE_SIZE)) {
        LOG_ERR("Page program failed at address 0x%x, page boundry crossed", address);
        return -PB_ERR_IO;
    }

    /* Write enable */
    i25w_transfer(NOR_CMD_LUT_SEQ_IDX_WRITEENABLE, 0, kFLEXSPI_Command, NULL, 0);
    // TODO: Error handling?

    rc = i25w_transfer(NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_QUAD,
                       address,
                       kFLEXSPI_Write,
                       (void *)data,
                       size);

    i25w_transfer(NOR_CMD_LUT_SEQ_IDX_WRITEDISABLE, 0, kFLEXSPI_Command, NULL, 0);
    // TODO: Error handling?

    if (rc != 0) {
        LOG_ERR("Page program failed at address 0x%x, status: %i\n", address, rc);
        return rc;
    }
    /* Typical page program time is 0.2ms, max 0.8ms. */
    i25w_wait_until_not_busy(100);
    // TODO: Error handling?

    return rc;
}


static int i25w_nor_read(bio_dev_t dev, lba_t lba, size_t length, void *buf)
{
    int rc = 0;
    uint8_t *buffer = (uint8_t *) buf;
    uint32_t address = lba * bio_block_size(dev);

    i25w_wait_until_not_busy(100);

    return i25w_transfer(NOR_CMD_LUT_SEQ_IDX_READ_FAST, address, kFLEXSPI_Read, buffer, length);
}

static int i25w_nor_write(bio_dev_t dev, lba_t lba, size_t length, const void *buf)
{
    int rc = 0;
    uint8_t *buffer = (uint8_t *) buf;
    uint32_t address = lba * bio_block_size(dev);
    uint32_t bytes_to_next_page_boundry =
        (uint32_t)IS25WP032D_PAGE_SIZE - (address & (uint32_t)(IS25WP032D_PAGE_SIZE - 1));


    LOG_DBG("Write buffer to flash at address: 0x%x, size: 0x%x\n", address, length);

    if ((bytes_to_next_page_boundry < IS25WP032D_PAGE_SIZE) &&
        (length > bytes_to_next_page_boundry)) {
        rc = i25w_page_program(address, buffer, bytes_to_next_page_boundry);
        buffer += bytes_to_next_page_boundry;
        address += bytes_to_next_page_boundry;
        length -= bytes_to_next_page_boundry;
    }

    while ((length > IS25WP032D_PAGE_SIZE) && (rc == 0)) {
        rc = i25w_page_program(address, buffer, IS25WP032D_PAGE_SIZE);
        buffer += IS25WP032D_PAGE_SIZE;
        address += IS25WP032D_PAGE_SIZE;
        length -= IS25WP032D_PAGE_SIZE;
    }

    if ((length > 0) && (rc == 0)) {
        rc = i25w_page_program(address, buffer, length);
    }

    return rc;
}

static int i25w_nor_erase(bio_dev_t dev)
{
    int rc;
    uint32_t addr;
    uint32_t part_start_addr = (uint32_t) bio_get_first_block(dev) * bio_block_size(dev);
    uint32_t part_end_addr = (uint32_t) bio_get_last_block(dev) * bio_block_size(dev);

    /* Write enable */
    // TODO: Error handling?

    for (addr = part_start_addr; addr < part_end_addr; addr += 64*1024) {
        LOG_DBG("Erasing 0x%08x...", addr);

        i25w_transfer(NOR_CMD_LUT_SEQ_IDX_WRITEENABLE, 0, kFLEXSPI_Command, NULL, 0);

        rc = i25w_transfer(NOR_CMD_LUT_SEQ_IDX_ERASEBLOCK, addr, kFLEXSPI_Command, NULL, 0);

        if (rc != 0)
            return rc;

        i25w_wait_until_not_busy(2000);

    }

    i25w_transfer(NOR_CMD_LUT_SEQ_IDX_WRITEDISABLE, 0, kFLEXSPI_Command, NULL, 0);
    // TODO: Error handling?
    return 0;
}

bio_dev_t i25w_nor_init(void)
{
    int rc;
    uint8_t cmd_buf[4];
    struct flexspi_transfer xfer;

    LOG_INFO("Init");
    xfer.deviceAddress = 0;
    xfer.port = kFLEXSPI_PortA1;
    xfer.cmdType = kFLEXSPI_Read;
    xfer.SeqNumber = 1;
    xfer.seqIndex = 9;
    xfer.data = cmd_buf;
    xfer.dataSize = 4;

    rc = imx_flexspi_transferblocking(&xfer);

    if (rc == 0) {
        LOG_DBG("Manufacturer: %02x Device Type %02x, Capacity: %02x", cmd_buf[0], cmd_buf[1], cmd_buf[2]);
    } else {
        return rc;
    }

    bio_dev_t d = bio_allocate(0,
                               8192, // 4 MByte, 512 byte sectors
                               512, // Sector size
                               UUID_1bd35a54_485d_4fb6_a822_cb58c3c5a068,
                               "NOR Flash");

    if (d < 0)
        return d;

    bio_set_flags(d, BIO_FLAG_WRITABLE | BIO_FLAG_VISIBLE | BIO_FLAG_ERASE_BEFORE_WRITE);

    rc = bio_set_ios(d, i25w_nor_read, i25w_nor_write);

    if (rc != 0)
        return rc;

    rc = bio_set_ios_erase(d, i25w_nor_erase);

    if (rc != 0)
        return rc;

    return d;
}
