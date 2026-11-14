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
    /* pb's exceptions to pb's handlers. VTOR comes out of reset as the
     * SoC's CM7_INIT_VTOR, on the RT1060 the ROM's table at 0x00200000
     * (IOMUXC_GPR_GPR16), and "the program image can relocate the vectors"
     * (RM 9.5.5): the ROM isn't said to move them to the image's.
     * The DSB so an exception after it takes the new table, the ISB for
     * what follows (Arm's barrier guide for Cortex-M, AN321). The table's
     * alignment is checked in the link (link.lds). */
    mmio_write_32(ARMV7M_SCB_VTOR, (uint32_t)init_vector);
    ARM_DSB();
    ARM_ISB();

    /* Enable the caches, the D-cache write-through and empty first (F-207,
     * mmu.S) */
    armv7m_dcache_invalidate_all();
    mmio_clrsetbits_32(ARMV7M_CACR, 0, ARMV7M_CACR_FORCEWT);
    mmio_clrsetbits_32(ARMV7M_SCB_CCR, 0, ARMV7M_CCR_DC | ARMV7M_CCR_IC);
    ARM_DSB();
    ARM_ISB();

    /* Clear BSS */
    size_t bss_length = zero_region_end - zero_region_start;
    memset((void *)zero_region_start, 0, bss_length);

    main();
}
