#ifndef PLAT_IMX6UL_INCLUDE_DEFS_H
#define PLAT_IMX6UL_INCLUDE_DEFS_H

#define COUNTER_FREQUENCY          (8000000)
#define COUNTER_US_SHIFT           (3)

#define SCTR_BASE_ADDRESS          0x21dc000

#define CACHE_LINE                 32

#define PLAT_VIRT_ADDR_SPACE_SIZE  (1ull << 32)
#define PLAT_PHY_ADDR_SPACE_SIZE   (1ull << 32)

#define MAX_XLAT_TABLES            32
#define MAX_MMAP_REGIONS           32

#define HAB_RVT_BASE               0x00000100

#define HAB_RVT_ENTRY              (*(uint32_t *)(HAB_RVT_BASE + 0x04))
#define HAB_RVT_EXIT               (*(uint32_t *)(HAB_RVT_BASE + 0x08))
#define HAB_RVT_CHECK_TARGET       (*(uint32_t *)(HAB_RVT_BASE + 0x0C))
#define HAB_RVT_AUTHENTICATE_IMAGE (*(uint32_t *)(HAB_RVT_BASE + 0x10))
#define HAB_RVT_REPORT_EVENT       (*(uint32_t *)(HAB_RVT_BASE + 0x20))
#define HAB_RVT_REPORT_STATUS      (*(uint32_t *)(HAB_RVT_BASE + 0x24))
#define HAB_RVT_FAILSAFE           (*(uint32_t *)(HAB_RVT_BASE + 0x28))

#define PAD_CTL_HYS                (1 << 16)
#define PAD_CTL_PUS_47K_UP         (1 << 14)
#define PAD_CTL_PUS_100K_UP        (2 << 14)

#define PAD_CTL_PUE                (1 << 13)
#define PAD_CTL_PKE                (1 << 12)
#define PAD_CTL_SPEED_LOW          (1 << 6)
#define PAD_CTL_SPEED_MED          (2 << 6)
#define PAD_CTL_DSE_DISABLE        (0 << 3)
#define PAD_CTL_DSE_240ohm         (1 << 3)
#define PAD_CTL_DSE_120ohm         (2 << 3)
#define PAD_CTL_DSE_80ohm          (3 << 3)
#define PAD_CTL_DSE_60ohm          (4 << 3)
#define PAD_CTL_DSE_48ohm          (5 << 3)
#define PAD_CTL_DSE_40ohm          (6 << 3)
#define PAD_CTL_DSE_34ohm          (7 << 3)
#define PAD_CTL_SRE_FAST           (1 << 0)
#define PAD_CTL_SRE_SLOW           (0 << 0)

#define UART_PAD_CTRL                                                                          \
    (PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm | \
     PAD_CTL_SRE_FAST | PAD_CTL_HYS)

#define AIPS1_ARB_BASE_ADDR 0x02000000
#define AIPS2_ARB_BASE_ADDR 0x02100000
#define ATZ1_BASE_ADDR      AIPS1_ARB_BASE_ADDR
#define ATZ2_BASE_ADDR      AIPS2_ARB_BASE_ADDR
#define AIPS1_OFF_BASE_ADDR (ATZ1_BASE_ADDR + 0x80000)
#define ANATOP_BASE_ADDR    (AIPS1_OFF_BASE_ADDR + 0x48000)

#define UART2_BASE          (ATZ2_BASE_ADDR + 0xE8000)

/* b36693cd-d32e-4cd5-b2bb-91406ed68840 */
#define PLATFORM_NS_UUID \
    (const unsigned char *)"\xb3\x66\x93\xcd\xd3\x2e\x4c\xd5\xb2\xbb\x91\x40\x6e\xd6\x88\x40"

#endif // PLAT_IMX6UL_INCLUDE_DEFS_H
