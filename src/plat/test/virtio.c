#include <board.h>
#include <io.h>
#include <tinyprintf.h>
#include <string.h>
#include "virtio.h"
#include "virtio_mmio.h"
#include "virtio_queue.h"


/*
 * virtio.c - queue management, feature management
 * virtio_serial.c - virtio serial implementation 
 * virtio_block.c  - virtio block device implementation
 * 
 *
 * ----- serial -----
 * uint32_t virtio_serial_read(struct virtio_serial_device *d,
 *                    uint8_t *buf,
 *                    uint32_t len)
 *
 * uint32_t virtio_serial_write(struct virtio_serial_device *d,
 *                    uint8_t *buf,
 *                    uint32_t len)
 * 
 * void virtio_serial_task(struct virtio_serial_device *d)
 *
 * struct virtio_serial_device {
 *      struct virtio_mmio_device d;
 *      struct virtio_serial_queue rx;
 *      struct virtio_serial_queue tx;
 *      struct virtio_serial_queue ctrl_rx;
 *      struct virtio_serial_queue ctrl_tx;
 *      uint8_t rx_buf[BUF_SZ][BUF_N];
 *      uint8_t tx_buf[BUF_SZ][BUF_N];
 * 
 * }
 *
 *
 *
 * struct virtio_device d;
 * 
 * d.base = ....;
 * d.device_type = 3;
 *   .
 *   .
 *   .
 *
 * uint32_t virtio_mmio_init(&d);
 * uint32_t virtio_mmio_get_features(&d);
 * uint32_t virtio_mmio_set_features(&d, features);
 * uint32_t virtio_mmio_set_driver_ok();
 * uint32_t virtio_mmio_init_queue(q, buf, nelm)
 *
 *
 * */


uint8_t tx_buf[64];
uint8_t rx_buf[64];

uint8_t ctrl_tx_buf[64];
uint8_t ctrl_rx_buf[64];

struct virtio_console_config {
    uint16_t cols;
    uint16_t rows;
    uint32_t max_nr_ports;
    uint32_t emerg_wr;
};

struct virtq_console {
    struct virtq_desc desc[1024];
    struct virtq_avail avail;
    uint16_t avail_ring[1024];
    uint8_t padding[2044];
    struct virtq_used used;
    struct virtq_used_elem used_ring[1024];
} __attribute__ ((packed));

struct virtio_console_control {
    uint32_t id; /* Port number */
    uint16_t event; /* The kind of control event */
    uint16_t value; /* Extra information for the event */
};

__a4k  struct virtq_console console_rx;
__a4k  struct virtq_console console_tx;

__a4k  struct virtq_console console_ctrl_rx;
__a4k  struct virtq_console console_ctrl_tx;

uint32_t virtio_mmio_init(struct virtio_device *d)
{
    if (pb_read32(d->base + VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976)
        return PB_ERR;

    LOG_INFO ("Found VIRTIO @ 0x%8.8X, type: 0x%8.8X, version: %i", d->base,
                pb_read32(d->base + VIRTIO_MMIO_DEVICE_ID),
                pb_read32(d->base + VIRTIO_MMIO_VERSION));


    LOG_INFO ("%lX, %lX", (uint32_t) &console_rx.avail, &console_rx.used);
    pb_write32(0 , d->base + VIRTIO_MMIO_STATUS);
    pb_write32((1 << 1) | (1 << 2) , d->base + VIRTIO_MMIO_STATUS);


    uint32_t features = pb_read32(d->base + VIRTIO_MMIO_DEVICE_FEATURES);
    LOG_INFO ("Features: 0x%8.8X",features);

    features = features & (1 << 2);

    pb_write32(features, d->base + VIRTIO_MMIO_DEVICE_FEATURES_SEL);

    LOG_INFO ("Features selected: 0x%8.8X",features);

    pb_write32((1 << 1) | (1 << 2) | (1 << 8) , d->base + VIRTIO_MMIO_STATUS);
    
    struct virtio_console_config * cconf = 
        (struct virtio_console_config *) (d->base + 0x100);

    cconf->max_nr_ports = 1;
    cconf->emerg_wr = 1;

    uint32_t q_size ;

    pb_write32(4, d->base + VIRTIO_MMIO_QUEUE_SEL);

    q_size = pb_read32(d->base + VIRTIO_MMIO_QUEUE_NUM_MAX);

    pb_write32(4096, d->base + VIRTIO_MMIO_GUEST_PAGE_SIZE);
    LOG_INFO("Queue 4 size: %lu", q_size);

    pb_write32(1024, d->base + VIRTIO_MMIO_QUEUE_NUM);
    pb_write32(4096, d->base + VIRTIO_MMIO_QUEUE_ALIGN);
    pb_write32( ((uint32_t) &console_rx) >> 12, d->base + VIRTIO_MMIO_QUEUE_PFN);

    
    console_rx.desc[0].len = 64;
    console_rx.desc[0].addr = (uint32_t) rx_buf;
    console_rx.desc[0].flags = 2;
    console_rx.desc[0].next = 0;

    console_rx.desc[1].len = 64;
    console_rx.desc[1].addr = (uint32_t) rx_buf;
    console_rx.desc[1].flags = 2;
    console_rx.desc[1].next = 0;
    console_rx.avail.idx += 2;

    pb_write32(5, d->base + VIRTIO_MMIO_QUEUE_SEL);

    q_size = pb_read32(d->base + VIRTIO_MMIO_QUEUE_NUM_MAX);

    LOG_INFO("Queue 5 size: %lu", q_size);

    pb_write32(1024, d->base + VIRTIO_MMIO_QUEUE_NUM);
    pb_write32(4096, d->base + VIRTIO_MMIO_QUEUE_ALIGN);

    pb_write32( ((uint32_t) &console_tx) >> 12 , d->base + VIRTIO_MMIO_QUEUE_PFN);

    console_tx.desc[0].len = 5;
    console_tx.desc[0].addr = (uint32_t) tx_buf;
    console_tx.desc[0].flags = 0;
    console_tx.desc[0].next = 0;
    console_tx.avail.idx += 1;


    pb_write32(2, d->base + VIRTIO_MMIO_QUEUE_SEL);

    q_size = pb_read32(d->base + VIRTIO_MMIO_QUEUE_NUM_MAX);

    LOG_INFO("Queue 2 size: %lu", q_size);

    pb_write32(1024, d->base + VIRTIO_MMIO_QUEUE_NUM);
    pb_write32(4096, d->base + VIRTIO_MMIO_QUEUE_ALIGN);

    pb_write32( ((uint32_t) &console_ctrl_rx) >> 12 , d->base + VIRTIO_MMIO_QUEUE_PFN);

    console_ctrl_rx.desc[0].len = 64;
    console_ctrl_rx.desc[0].addr = (uint32_t) ctrl_rx_buf;
    console_ctrl_rx.desc[0].flags = 2;
    console_ctrl_rx.desc[0].next = 0;

    console_ctrl_rx.desc[1].len = 64;
    console_ctrl_rx.desc[1].addr = (uint32_t) ctrl_rx_buf;
    console_ctrl_rx.desc[1].flags = 2;
    console_ctrl_rx.desc[1].next = 0;
 
    console_ctrl_rx.avail.idx += 2;

    pb_write32(3, d->base + VIRTIO_MMIO_QUEUE_SEL);

    q_size = pb_read32(d->base + VIRTIO_MMIO_QUEUE_NUM_MAX);

    LOG_INFO("Queue 3 size: %lu", q_size);

    pb_write32(1024, d->base + VIRTIO_MMIO_QUEUE_NUM);
    pb_write32(4096, d->base + VIRTIO_MMIO_QUEUE_ALIGN);

    pb_write32( ((uint32_t) &console_ctrl_tx) >> 12 , d->base + VIRTIO_MMIO_QUEUE_PFN);

    console_ctrl_tx.desc[0].len = 64;
    console_ctrl_tx.desc[0].addr = (uint32_t) ctrl_tx_buf;
    console_ctrl_tx.desc[0].flags = 0;
    console_ctrl_tx.desc[0].next = 0;

    console_ctrl_tx.avail.idx += 1;

    pb_write32((1 << 1) | (1 << 2) | (1 << 8) | (1 << 4) , d->base + VIRTIO_MMIO_STATUS);
    

    tfp_sprintf((char *) tx_buf,"Hej\n\r");


    struct virtio_console_control ctrlm;
    ctrlm.id = 1;
    ctrlm.event = 6;
    ctrlm.value = 1;

    memcpy(ctrl_tx_buf, &ctrlm, sizeof(struct virtio_console_control));
    console_ctrl_tx.desc[0].len = sizeof(struct virtio_console_control);


    LOG_INFO("tx used.idx = %i",console_tx.used.idx);
    LOG_INFO("rx used.idx = %i",console_rx.used.idx);
    LOG_INFO("tx avail.idx = %i",console_tx.avail.idx);
    LOG_INFO("rx avail.idx = %i",console_rx.avail.idx);
/*
    LOG_INFO("ctrl tx used.idx = %i",console_ctrl_tx.used.idx);
    LOG_INFO("ctrl rx used.idx = %i",console_ctrl_rx.used.idx);
    LOG_INFO("ctrl tx avail.idx = %i",console_ctrl_tx.avail.idx);
    LOG_INFO("ctrl rx avail.idx = %i",console_ctrl_rx.avail.idx);
*/
    pb_write32(4, d->base + VIRTIO_MMIO_QUEUE_NOTIFY);
    pb_write32(5, d->base + VIRTIO_MMIO_QUEUE_NOTIFY);
    pb_write32(2, d->base + VIRTIO_MMIO_QUEUE_NOTIFY);
    pb_write32(3, d->base + VIRTIO_MMIO_QUEUE_NOTIFY);

    uint32_t n = 0;
while (1) {
    volatile uint32_t isr = pb_read32(d->base + VIRTIO_MMIO_INTERRUPT_STATUS);

    if (isr) {
        LOG_INFO("ISR %u %lu",n,isr);
        pb_write32(isr, d->base + VIRTIO_MMIO_INTERRUPT_ACK);
        LOG_INFO("  %lu",isr = pb_read32(d->base + VIRTIO_MMIO_INTERRUPT_STATUS));

        LOG_INFO("tx used.idx = %i",console_tx.used.idx);
        LOG_INFO("rx used.idx = %i",console_rx.used.idx);
        LOG_INFO("tx avail.idx = %i",console_tx.avail.idx);
        LOG_INFO("rx avail.idx = %i",console_rx.avail.idx);

        LOG_INFO("rx used_ring[%u].len = %i",console_rx.used.idx,
                    console_rx.used_ring[console_rx.used.idx-1].len);

        LOG_INFO("ctrl tx used.idx = %i",console_ctrl_tx.used.idx);
        LOG_INFO("ctrl rx used.idx = %i",console_ctrl_rx.used.idx);
        LOG_INFO("ctrl tx avail.idx = %i",console_ctrl_tx.avail.idx);
        LOG_INFO("ctrl rx avail.idx = %i",console_ctrl_rx.avail.idx);

        for (uint32_t i = 0; i < 5; i++)
            tfp_printf ("%2.2X ", rx_buf[i]);
        tfp_printf("\n\r");

    }

   for (uint32_t i = 0; i < 0xffffff; i++)
        asm("nop");

    n = n + 1;
    tfp_sprintf((char *) tx_buf, "Hej %2.2X\n\r",(uint8_t) n);
    console_tx.desc[0].len = 8;
    console_tx.avail.idx += 1;

    //pb_write32(5, d->base + VIRTIO_MMIO_QUEUE_NOTIFY);

    console_rx.avail.idx += 1;
    pb_write32(4, d->base + VIRTIO_MMIO_QUEUE_NOTIFY);
}
    return PB_OK;
}
