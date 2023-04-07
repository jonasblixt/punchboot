#ifndef DRIVERS_VIRTIO_QUEUE_H
#define DRIVERS_VIRTIO_QUEUE_H
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
} __packed;

struct virtq_avail {
        uint16_t flags;
        uint16_t idx;
        uint16_t ring[];
} __packed;

/* uint32_t is used here for ids for padding reasons. */
struct virtq_used_elem {
        /* Index of start of used descriptor chain. */
        uint32_t id;
        /* Total length of the descriptor chain which was written to. */
        uint32_t len;
} __packed;

struct virtq_used {
        uint16_t flags;
        uint16_t idx;
        struct virtq_used_elem ring[];
} __packed;

struct virtq {
    unsigned int num;
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;
};

/**
 * NOTE:
 *
 * For the packed virtq, desc, avail and used must be on separate pages.
 * This means that at a minimum we must occupy three 4k pages, which further
 * means that there is no point in allocating fewer than 256 entries as 256 entries
 * for the descriptor table will fill a 4k page.
 */

#define VIRTIO_QUEUE_AVAIL_OFFSET(n, a) (((16*n + a - 1) & ~(a - 1)))
#define VIRTIO_QUEUE_USED_OFFSET(n, a) (((16*n + 4 + 2*n + a - 1) & ~(a - 1)))
#define VIRTIO_QUEUE_SZ(n, a) (VIRTIO_QUEUE_USED_OFFSET(n, a) + 4 + 8*n)

#endif  // DRIVERS_VIRTIO_QUEUE_H
