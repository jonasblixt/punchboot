/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb.h>
#include <io.h>
#include <tinyprintf.h>
#include <plat/test/virtio_block.h>
#include <plat/test/virtio_queue.h>

uint32_t virtio_block_init(struct virtio_block_device *d)
{

	if (virtio_mmio_init(&d->dev) != PB_OK)
		return PB_ERR;

	d->config = (struct virtio_blk_config *) (d->dev.base + 0x100);
		

	d->config->blk_size = 512;
	d->config->writeback = 0;
	d->config->capacity = 32768;
	d->config->geometry.cylinders = 0;
	d->config->geometry.heads = 0;
	d->config->geometry.sectors = 0;
	d->config->topology.physical_block_exp = 1;
	d->config->topology.alignment_offset = 0;
	d->config->topology.min_io_size = 512;
	d->config->topology.opt_io_size = 512;

    virtio_init_queue(d->_q_data, 1024, &d->q);
    virtio_mmio_init_queue(&d->dev, &d->q, 0, 1024);
 
	virtio_mmio_driver_ok(&d->dev);

    return PB_OK;
}

uint32_t virtio_block_write(struct virtio_block_device *d,
							uint32_t lba,
							uint8_t *buf,
							uint32_t no_of_blocks)
{

    __a16b struct virtio_blk_req r;
    __a16b uint8_t status = VIRTIO_BLK_S_UNSUPP;

	r.type = VIRTIO_BLK_T_OUT;
	r.sector_low = lba;
	r.sector_hi = 0;
	struct virtq *q = &d->q;
    uint16_t idx = q->avail->idx;

    q->desc[idx].addr = (uintptr_t) &r;
    q->desc[idx].len = sizeof(struct virtio_blk_req);
    q->desc[idx].flags = VIRTQ_DESC_F_NEXT;
    q->desc[idx].next = (idx + 1) % q->num;
    idx = (idx + 1) % q->num;

	q->desc[idx].addr = (uintptr_t) (buf);
	q->desc[idx].len = 512*no_of_blocks;
	q->desc[idx].flags = VIRTQ_DESC_F_NEXT;
	q->desc[idx].next = (idx + 1) % q->num;
	idx = (idx + 1) % q->num;

    q->desc[idx].addr = (uintptr_t) &status;
    q->desc[idx].len = 1;
    q->desc[idx].flags = VIRTQ_DESC_F_WRITE;
    q->desc[idx].next = 0;
    idx = (idx + 1) % q->num;

    q->avail->ring[idx] = idx;
    q->avail->idx += 3;
    
	virtio_mmio_notify_queue(&d->dev, &d->q);
	
    while(q->avail->idx != q->used->idx)
		asm("nop");

	if (status == VIRTIO_BLK_S_OK)
		return PB_OK;
	
	LOG_ERR("Failed, lba=%lu, no_of_blocks=%lu",lba,no_of_blocks);
	return PB_ERR;
}


uint32_t virtio_block_read(struct virtio_block_device *d,
							uint32_t lba,
							uint8_t *buf,
							uint32_t no_of_blocks)
{

    __a16b struct virtio_blk_req r;
    __a16b uint8_t status = VIRTIO_BLK_S_UNSUPP;

	r.type = VIRTIO_BLK_T_IN;
	r.reserved = 0;
	r.sector_low = lba;
	r.sector_hi = 0;
/*
	LOG_INFO ("lba_hi: %8.8lX, lba_low: %8.8lX",
			(uint32_t) (lba >> 32) & 0xFFFFFFFF,
			(uint32_t) (lba & 0xFFFFFFFF));
*/
	struct virtq *q = &d->q;
    uint16_t idx = q->avail->idx;

    q->desc[idx].addr = (uint32_t) &r;
    q->desc[idx].len = sizeof(struct virtio_blk_req);
    q->desc[idx].flags = VIRTQ_DESC_F_NEXT;
    q->desc[idx].next = (idx + 1) % q->num;
    idx = (idx + 1) % q->num;

	q->desc[idx].addr = (uint32_t) buf;
	q->desc[idx].len = 512*no_of_blocks;
	q->desc[idx].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
	q->desc[idx].next = (idx + 1) % q->num;
	idx = (idx + 1) % q->num;

    q->desc[idx].addr = (uint32_t) &status;
    q->desc[idx].len = 1;
    q->desc[idx].flags = VIRTQ_DESC_F_WRITE;
    q->desc[idx].next = 0;
    idx = (idx + 1) % q->num;

    q->avail->ring[idx] = idx;
    q->avail->idx += 3;

	virtio_mmio_notify_queue(&d->dev, &d->q);

	while(q->avail->idx != q->used->idx)
		asm("nop");	

	if (status == VIRTIO_BLK_S_OK)
		return PB_OK;
	
	LOG_ERR("Failed, lba=%lu, no_of_blocks=%lu",lba,no_of_blocks);
	return PB_ERR;
}


