#ifndef PLAT_IMX8M_INCLUDE_DEFS_H_
#define PLAT_IMX8M_INCLUDE_DEFS_H_

#define COUNTER_FREQUENCY         (8000000)
#define COUNTER_US_SHIFT          (3)

#define SCTR_BASE_ADDRESS         0x306C0000
#define IMX_CAAM_BASE             0x30901000
#define IMX_GPT_BASE              0x302D0000
#define IMX_GPT_PR                40

#define PLAT_VIRT_ADDR_SPACE_SIZE (1ull << 32)
#define PLAT_PHY_ADDR_SPACE_SIZE  (1ull << 32)

#define MAX_XLAT_TABLES           32
#define MAX_MMAP_REGIONS          32

#define CACHE_LINE                64

#endif // PLAT_IMX8M_INCLUDE_DEFS_H_
