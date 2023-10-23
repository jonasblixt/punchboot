#include <string.h>
#include <pb/pb.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <arch/arch.h>

IMPORT_SYM(uintptr_t, _zero_region_start, zero_region_start);
IMPORT_SYM(uintptr_t, _zero_region_end, zero_region_end);
IMPORT_SYM(uintptr_t, _stack_start, stack_start);
IMPORT_SYM(uintptr_t, _stack_end, stack_end);
IMPORT_SYM(uintptr_t, _sdata, data_start_ram);
IMPORT_SYM(uintptr_t, _edata, data_end_ram);
IMPORT_SYM(uintptr_t, _code_end, code_end);

static void armv7m_entry(void);
static void armv7m_default_handler(void);

const uintptr_t init_vector[] __section(".vectors") =
{
    stack_end,
    (uintptr_t) armv7m_entry,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
    (uintptr_t) armv7m_default_handler,
};

static __section(".vector_handlers") void armv7m_default_handler(void)
{
    printf("%s", __func__);
    while(1);
}

static __section(".vector_handlers") void armv7m_entry(void)
{
    /* Enable caches */
    mmio_clrsetbits_32(0xe000e000+0x0f9c, 0, (1 << 2));
    mmio_clrsetbits_32(0xe000e000+0x0d14, 0, (1 << 16) | (1 << 17));
    ARM_DSB();
    ARM_ISB();

    /* Clear BSS */
    size_t bss_length = zero_region_end - zero_region_start;
    memset((void *) zero_region_start, 0, bss_length);

#ifdef CONFIG_EXECUTE_IN_FLASH
    /* Copy data */
    size_t data_length = data_end_ram - data_start_ram;
    memcpy((void *)data_start_ram, (void *)code_end, data_length);
#endif

    main();
}
