#ifndef PLAT_IMX6UL_INCLUDE_DEFS_H_
#define PLAT_IMX6UL_INCLUDE_DEFS_H_

#define COUNTER_FREQUENCY (8000000)
#define COUNTER_US_SHIFT (3)

#define SCTR_BASE_ADDRESS 0x21dc000
#define IMX_CAAM_BASE 0x31430000
#define IMX_EHCI_BASE 0x0218400

#define CACHE_LINE 32

#define PLAT_VIRT_ADDR_SPACE_SIZE	(1ull << 32)
#define PLAT_PHY_ADDR_SPACE_SIZE	(1ull << 32)

#define MAX_XLAT_TABLES			32
#define MAX_MMAP_REGIONS		32

#endif  // PLAT_IMX6UL_INCLUDE_DEFS_H_
