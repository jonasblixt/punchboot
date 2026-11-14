#include <arch/arch.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <string.h>

IMPORT_SYM(uintptr_t, _zero_region_start, zero_region_start);
IMPORT_SYM(uintptr_t, _zero_region_end, zero_region_end);
IMPORT_SYM(uintptr_t, _stack_start, stack_start);
IMPORT_SYM(uintptr_t, _stack_end, stack_end);

static void armv7m_entry(void);
static void armv7m_default_handler(void);

const uintptr_t init_vector[] __section(".vectors") = {
    stack_end,
    (uintptr_t)armv7m_entry,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
    (uintptr_t)armv7m_default_handler,
};

static __section(".vector_handlers") void armv7m_default_handler(void)
{
    /* The exception may have hit in the middle of a RAM log only message */
    console_uart_mute(false);
    printf("%s", __func__);
    while (1)
        ;
}

static __section(".vector_handlers") void armv7m_entry(void)
{
    /* Enable the caches, the D-cache write-through */
    mmio_clrsetbits_32(ARMV7M_CACR, 0, ARMV7M_CACR_FORCEWT);
    mmio_clrsetbits_32(ARMV7M_SCB_CCR, 0, ARMV7M_CCR_DC | ARMV7M_CCR_IC);
    ARM_DSB();
    ARM_ISB();

    /* Clear BSS */
    size_t bss_length = zero_region_end - zero_region_start;
    memset((void *)zero_region_start, 0, bss_length);

    main();
}
