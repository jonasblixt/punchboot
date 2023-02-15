#ifndef PLAT_TEST_VIRTIO_QUEUE_H_
#define PLAT_TEST_VIRTIO_QUEUE_H_
/*
 *
 * Virtual I/O Device (VIRTIO) Version 1.0
 * Committee Specification 04
 * 03 March 2016
 * Copyright (c) OASIS Open 2016. All Rights Reserved.
 * Source: http://docs.oasis-open.org/virtio/virtio/v1.0/cs04/listings/
 * Link to latest version of the specification documentation: http://docs.oasis-open.org/virtio/virtio/v1.0/virtio-v1.0.html
 *
 */
#include <stdint.h>

/* This marks a buffer as continuing via the next field. */
#define VIRTQ_DESC_F_NEXT       1
/* This marks a buffer as write-only (otherwise read-only). */
#define VIRTQ_DESC_F_WRITE      2
/* This means the buffer contains a list of buffer descriptors. */
#define VIRTQ_DESC_F_INDIRECT   4

/* The device uses this in used->flags to advise the driver: don't kick me
 * when you add a buffer.  It's unreliable, so it's simply an
 * optimization. */
#define VIRTQ_USED_F_NO_NOTIFY  1
/* The driver uses this in avail->flags to advise the device: don't
 * interrupt me when you consume a buffer.  It's unreliable, so it's
 * simply an optimization.  */
#define VIRTQ_AVAIL_F_NO_INTERRUPT      1

/* Support for indirect descriptors */
#define VIRTIO_F_INDIRECT_DESC    28

/* Support for avail_event and used_event fields */
#define VIRTIO_F_EVENT_IDX        29

/* Arbitrary descriptor layouts. */
#define VIRTIO_F_ANY_LAYOUT       27

/* Virtqueue descriptors: 16 bytes.
 * These can chain together via "next". */
struct virtq_desc {
        /* Address (guest-physical). */
        uint64_t addr;
        /* Length. */
        uint32_t len;
        /* The flags as indicated above. */
        uint16_t flags;
        /* We chain unused descriptors via this, too */
        uint16_t next;
} __attribute__((packed));

struct virtq_avail {
        uint16_t flags;
        volatile uint16_t idx;
        uint16_t ring[];
} __attribute__((packed));

/* uint32_t is used here for ids for padding reasons. */
struct virtq_used_elem {
        /* Index of start of used descriptor chain. */
        uint32_t id;
        /* Total length of the descriptor chain which was written to. */
        uint32_t len;
} __attribute__((packed));

struct virtq_used {
        uint16_t flags;
        volatile uint16_t idx;
        struct virtq_used_elem ring[];
} __attribute__((packed));

struct virtq {
    unsigned int num;
    uint32_t queue_index;
    __attribute__((aligned(16))) struct virtq_desc *desc;
    __attribute__((aligned(2))) struct virtq_avail *avail;
    __attribute__((aligned(4))) struct virtq_used *used;
};

#define VIRTIO_QUEUE_SZ(n) (16*n + 4 + 2*n + 4 + 8*n)
#define VIRTIO_QUEUE_SZ2(n) (16*n + 4 + 2*n)
#define VIRTIO_QUEUE_USED_OFFSET(n, a) (((VIRTIO_QUEUE_SZ2(n) \
        +a-1) & ~(a-1)))
#define VIRTIO_QUEUE_AVAIL_OFFSET(n) (16*n)

#define VIRTIO_QUEUE_SZ_WITH_PADDING(n) VIRTIO_QUEUE_SZ2(n) + (4+8*n)

static inline void virtio_init_queue(uint8_t *buf, uint32_t n, struct virtq *q)
{
    q->num = n;
    q->desc = (struct virtq_desc *) buf;
    q->avail = (struct virtq_avail *) (buf +
                    VIRTIO_QUEUE_AVAIL_OFFSET(n));
    q->used = (struct virtq_used *) (buf +
                    VIRTIO_QUEUE_USED_OFFSET(n, 4096));
}

static inline int virtq_need_event(uint16_t event_idx, uint16_t new_idx,
                                   uint16_t old_idx)
{
    return ((uint16_t)(new_idx - event_idx - 1) <
                                                (uint16_t)(new_idx - old_idx));
}

static inline uint16_t *virtq_avail_event(struct virtq *vq)
{
    /* For backwards compat, avail event index is at *end* of used ring. */
    return (uint16_t *)&vq->used->ring[vq->num];
}


#endif  // PLAT_TEST_VIRTIO_QUEUE_H_
