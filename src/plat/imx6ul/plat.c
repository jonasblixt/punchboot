/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <pb/mmio.h>
#include <pb/plat.h>
#include <pb/console.h>
#include <drivers/timer/imx_gpt.h>
#include <drivers/fuse/imx_ocotp.h>
#include <drivers/wdog/imx_wdog.h>
#include <drivers/uart/imx_uart.h>
#include <plat/imx6ul/imx6ul.h>
#include <xlat_tables.h>
#include <uuid.h>
#include <platform_defs.h>
#include <board_defs.h>

IMPORT_SYM(uintptr_t, _code_start, code_start);
IMPORT_SYM(uintptr_t, _code_end, code_end);
IMPORT_SYM(uintptr_t, _data_region_start, data_start);
IMPORT_SYM(uintptr_t, _data_region_end, data_end);
IMPORT_SYM(uintptr_t, _ro_data_region_start, ro_data_start);
IMPORT_SYM(uintptr_t, _ro_data_region_end, ro_data_end);
IMPORT_SYM(uintptr_t, _stack_start, stack_start);
IMPORT_SYM(uintptr_t, _stack_end, stack_end);
IMPORT_SYM(uintptr_t, _zero_region_start, rw_nox_start);
IMPORT_SYM(uintptr_t, _no_init_end, rw_nox_end);

static struct imx6ul_platform plat;
static int boot_reason;

static const mmap_region_t imx_mmap[] =
{
    /* Boot ROM API*/
    MAP_REGION_FLAT(0x00000000, (128 * 1024), MT_MEMORY | MT_RO | MT_EXECUTE),
    /* Needed for HAB*/
    MAP_REGION_FLAT(0x00900000, (256 * 1024), MT_MEMORY | MT_RW),
    /* AIPS-1 */
    MAP_REGION_FLAT(0x02000000, (1024 * 1024), MT_DEVICE | MT_RW),
    /* AIPS-2 */
    MAP_REGION_FLAT(0x02100000, (1024 * 1024), MT_DEVICE | MT_RW),
    {0}
};

int plat_boot_reason(void)
{
    return -PB_ERR_NOT_IMPLEMENTED;
}

const char *plat_boot_reason_str(void)
{
    return "";
}

int plat_get_unique_id(uint8_t *output, size_t *length)
{
    union {
        uint32_t uid[2];
        uint8_t uid_bytes[8];
    } plat_unique;

    if (*length < sizeof(plat_unique.uid_bytes))
        return -PB_ERR_BUF_TOO_SMALL;
    *length = sizeof(plat_unique.uid_bytes);

    imx_ocotp_read(0, 1, &plat_unique.uid[0]);
    imx_ocotp_read(0, 2, &plat_unique.uid[1]);
    memcpy(output, plat_unique.uid_bytes, sizeof(plat_unique.uid_bytes));
    return PB_OK;
}


void plat_reset(void)
{
    imx_wdog_reset_now();
}

uint32_t plat_get_us_tick(void)
{
    return imx_gpt_get_tick();
}

void plat_wdog_kick(void)
{
    imx_wdog_kick();
}

/* UART Interface */

static int plat_console_init(void)
{
    /* Configure UART */
    mmio_write_32(0x020E0094, 0);
    mmio_write_32(0x020E0098, 0);
    mmio_write_32(0x020E0320, UART_PAD_CTRL);
    mmio_write_32(0x020E0324, UART_PAD_CTRL);

    imx_uart_init(0x021E8000, MHz(80), 115200);

    static const struct console_ops ops = {
        .putc = imx_uart_putc,
    };

    console_init(0x021E8000, &ops);

    return PB_OK;
}


static int plat_mmu_init(void)
{
    /* Configure MMU */
    reset_xlat_tables();

    mmap_add_region(code_start, code_start,
                    code_end - code_start,
                    MT_RO | MT_MEMORY | MT_EXECUTE);

    mmap_add_region(data_start, data_start,
                    data_end - data_start,
                    MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(ro_data_start, ro_data_start,
                    ro_data_end - ro_data_start,
                    MT_RO | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(stack_start, stack_start,
                    stack_end - stack_start,
                    MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(rw_nox_start, rw_nox_start,
                    rw_nox_end - rw_nox_start,
                    MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    /* Add the rest of the RAM */
    mmap_add_region(rw_nox_end, rw_nox_end,
                    BOARD_RAM_END - rw_nox_end,
                    MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add(imx_mmap);

    init_xlat_tables();

    enable_mmu_svc_mon(0);

    return PB_OK;
}

int plat_init(void)
{
    /* Unmask wdog in SRC control reg */
    mmio_write_32(0x020D8000, 0);
    imx_wdog_init(0x020BC000, CONFIG_WATCHDOG_TIMEOUT);
    imx_gpt_init(0x02098000, MHz(24));

    /**
     * TODO: Some imx6ul can run at 696 MHz and some others at 528 MHz
     *   implement handeling of that.
     *
     */

    /*** Configure ARM Clock ***/
    /* Select step clock, so we can change arm PLL */
    mmio_clrsetbits_32(0x020C400C, 0, 1 << 2);


    /* Power down */
    mmio_write_32(0x020C8000, (1<<12));

    /* Configure divider and enable */
    /* f_CPU = 24 MHz * 88 / 4 = 528 MHz */
    mmio_write_32(0x020C8000, (1<<13) | 88);


    /* Wait for PLL to lock */
    while (!(mmio_read_32(0x020C8000) & (1<<31)))
        {};

    /* Select re-connect ARM PLL */
    mmio_clrsetbits_32(0x020C400C, 1 << 2, 0);

    /*** End of ARM Clock config ***/

    /* Ungate all clocks */
    mmio_write_32(0x020C4000 + 0x68, 0xFFFFFFFF); /* Ungate usdhc clk*/
    mmio_write_32(0x020C4000 + 0x6C, 0xFFFFFFFF); /* Ungate usdhc clk*/
    mmio_write_32(0x020C4000 + 0x70, 0xFFFFFFFF); /* Ungate usdhc clk*/
    mmio_write_32(0x020C4000 + 0x74, 0xFFFFFFFF); /* Ungate usdhc clk*/
    mmio_write_32(0x020C4000 + 0x78, 0xFFFFFFFF); /* Ungate usdhc clk*/
    mmio_write_32(0x020C4000 + 0x7C, 0xFFFFFFFF); /* Ungate usdhc clk*/
    mmio_write_32(0x020C4000 + 0x80, 0xFFFFFFFF); /* Ungate usdhc clk*/


    uint32_t csu = 0x21c0000;
    /* Allow everything */
    for (int i = 0; i < 40; i ++) {
        *((uint32_t *)csu + i) = 0xffffffff;
    }

    plat_console_init();
    imx_ocotp_init(0x021BC000, 8);
    imx_wdog_kick();
    plat_mmu_init();

    return PB_OK;
}

int plat_board_init(void)
{
    return board_init(&plat);
}
