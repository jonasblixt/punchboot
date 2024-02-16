#ifndef INCLUDE_PLAT_IMX8M_HAB_H
#define INCLUDE_PLAT_IMX8M_HAB_H

#define HAB_RVT_ENTRY              ((unsigned long)*(uint32_t *)(HAB_RVT_BASE + 0x08))
#define HAB_RVT_EXIT               ((unsigned long)*(uint32_t *)(HAB_RVT_BASE + 0x10))
#define HAB_RVT_AUTHENTICATE_IMAGE ((unsigned long)*(uint32_t *)(HAB_RVT_BASE + 0x20))
#define HAB_RVT_REPORT_EVENT       ((unsigned long)*(uint32_t *)(HAB_RVT_BASE + 0x40))
#define HAB_RVT_REPORT_STATUS      ((unsigned long)*(uint32_t *)(HAB_RVT_BASE + 0x48))

#endif
