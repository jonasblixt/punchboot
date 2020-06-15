#ifndef INCLUDE_VM_H_
#define INCLUDE_VM_H_

#if CONFIG_ARCH_ARMV8
#define __MMU_INITIAL_MAPPING_PHYS_OFFSET 0
#define __MMU_INITIAL_MAPPING_VIRT_OFFSET 8
#define __MMU_INITIAL_MAPPING_SIZE_OFFSET 16
#define __MMU_INITIAL_MAPPING_FLAGS_OFFSET 24
#define __MMU_INITIAL_MAPPING_SIZE        40
#elif CONFIG_ARCH_ARMV7
#define __MMU_INITIAL_MAPPING_PHYS_OFFSET 0
#define __MMU_INITIAL_MAPPING_VIRT_OFFSET 4
#define __MMU_INITIAL_MAPPING_SIZE_OFFSET 8
#define __MMU_INITIAL_MAPPING_FLAGS_OFFSET 12
#define __MMU_INITIAL_MAPPING_SIZE        20
#else
    #error "Unknown arch"
#endif

/* flags for initial mapping struct */
#define MMU_INITIAL_MAPPING_TEMPORARY     (0x1)
#define MMU_INITIAL_MAPPING_FLAG_UNCACHED (0x2)
#define MMU_INITIAL_MAPPING_FLAG_DEVICE   (0x4)
#define MMU_INITIAL_MAPPING_FLAG_DYNAMIC  (0x8)  /* entry has to be patched up by platform_reset */

#ifndef __ASSEMBLY__

#include <stdint.h>

typedef uintptr_t addr_t;
typedef uintptr_t vaddr_t;
typedef uintptr_t paddr_t;

struct mmu_initial_mapping
{
    paddr_t phys;
    vaddr_t virt;
    size_t  size;
    unsigned int flags;
    const char *name;
};

extern struct mmu_initial_mapping mmu_initial_mappings[];
#endif  // __ASSEMBLY__
#endif  // INCLUDE_VM_H_
