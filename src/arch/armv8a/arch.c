#include <arch/arch_helpers.h>
#include <arch/armv8a/timer.h>
#include <pb/arch.h>
#include <pb/console.h>
#include <pb/dma.h>
#include <pb/pb.h>
#include <pb/plat.h>

/* CPUECTLR_EL1 as the ROM left it, on a Cortex-A53 or A35, which _reset
 * set SMPEN in (F-469). In .data: the BSS is cleared after _reset wrote it */
#define CPUECTLR_SMPEN    BIT(6)
#define CPUECTLR_NOT_READ UINT64_MAX /* Another core: _reset left it */
uint64_t armv8a_rom_cpuectlr __attribute__((section(".data"))) = CPUECTLR_NOT_READ;

void arch_init(void)
{
    if (armv8a_rom_cpuectlr != CPUECTLR_NOT_READ) {
        LOG_DBG("CPUECTLR_EL1 before pb: 0x%016" PRIx64, armv8a_rom_cpuectlr);

        if (!(armv8a_rom_cpuectlr & CPUECTLR_SMPEN))
            LOG_DBG("CPUECTLR_EL1.SMPEN was clear: set (F-469)");
    }

    /* What a device writes is declared and rounded with PB_DMA_ALIGN
     * (drivers.md 4.1): a core with a longer line shares lines between such
     * buffers and their neighbours. CTR_EL0's DminLine, which the range
     * functions in cache-ops.S step by */
    size_t line = SIZE_FROM_LOG2_WORDS((read_ctr_el0() >> CTR_DMINLINE_SHIFT) & CTR_DMINLINE_MASK);

    if (line > PB_DMA_ALIGN)
        LOG_ERR("D-cache line of %zu bytes, PB_DMA_ALIGN is %i", line, PB_DMA_ALIGN);
}

/* Generic exception handler */
void exception(int index)
{
    /* The exception may have hit in the middle of a RAM log only message */
    console_uart_mute(false);
    printf("*** UNHANDLED EXCEPTION ***\n\r");
    printf("Index %i\n\r", index);
#ifdef CONFIG_ARMV8_RESET_ON_EXCEPTION
    plat_reset();
#endif
}

void exception_sync(void)
{
    console_uart_mute(false);
    printf("*** SYNCHRONOUS EXCEPTION ***\n\r");
    printf("ESR: 0x%08lx\n\r", read_esr_el3());
    printf("ELR: 0x%08lx\n\r", read_elr_el3());
    printf("FAR: 0x%08lx\n\r", read_far_el3());
    printf("SCTLR: 0x%08lx\n\r", read_sctlr_el3());
#ifdef CONFIG_ARMV8_RESET_ON_EXCEPTION
    plat_reset();
#endif
}

/* arch_disable_mmu() is implemented in cache-ops.S. It has to be assembly: the
 * ordering it depends on cannot be expressed in C, see the comment there. */
