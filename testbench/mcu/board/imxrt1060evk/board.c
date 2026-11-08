/*
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX RT1060 EVK. The console is LPUART1 as pb set it up, the
 * watchdog WDOG1, which pb starts. The boot state is written with the ROM's
 * FlexSPI NOR API (RM 9.13), configured with pb's own FCB: the image runs
 * from OCRAM, so nothing runs from the NOR while it is programmed.
 */

#include <plat/imxrt/fcb.h>

#include "app.h"

#define LPUART1_BASE       0x40184000
#define LPUART_STAT        0x14
#define LPUART_CTRL        0x18
#define LPUART_DATA        0x1c
#define STAT_TDRE          (1u << 23)
#define CTRL_TE            (1u << 19)

#define WDOG1_BASE         0x400b8000
#define WDOG_WCR           0x00
#define WDOG_WSR           0x02
#define WCR_WDE            (1u << 2)

#define SRC_GPR5           0x400f8030
#define PB_CM_REQUEST      0x5042434d /* "PBCM", src/board/imxrt1060evk/board.c */
#define SCB_AIRCR          0xe000ed0c

/* pb's NOR layout (src/board/imxrt1060evk/board.c): BL 256 KiB, then the
 * two 4 KiB boot state copies */
#define STATE_OFFSET(copy) (0x40000u + (copy) * 0x1000u)
#define NOR_SECTOR         0x1000u
#define NOR_PAGE           256u

const uint8_t board_sys_a_uuid[16] = { 0x2a, 0xf7, 0x55, 0xd8, 0x8d, 0xe5, 0x45, 0xd5,
                                       0xa8, 0x62, 0x01, 0x4c, 0xfa, 0x73, 0x5c, 0xe0 };
const uint8_t board_sys_b_uuid[16] = { 0xc0, 0x46, 0xcc, 0xd8, 0x0f, 0x2e, 0x40, 0x36,
                                       0x98, 0x4d, 0x76, 0xc1, 0x4d, 0xc7, 0x39, 0x92 };

/* The ROM API (RM 9.13.1) */
struct flexspi_nor_api {
    uint32_t version;
    int (*init)(uint32_t instance, void *config);
    int (*program)(uint32_t instance, void *config, uint32_t dst, const uint32_t *src);
    int (*erase_all)(uint32_t instance, void *config);
    int (*erase)(uint32_t instance, void *config, uint32_t start, uint32_t length);
    int (*read)(uint32_t instance, void *config, uint32_t *dst, uint32_t addr, uint32_t length);
    void (*clear_cache)(uint32_t instance);
};

struct rom_api {
    uint32_t version;
    const char *copyright;
    void (*run_bootloader)(void *arg);
    const uint32_t *reserved0;
    const struct flexspi_nor_api *nor;
};

#define ROM_API (*(const struct rom_api *const *)0x0020001c)

/* pb's FCB, board/imxrt1060evk/board.mk links it */
extern const struct imxrt_fcb imxrt1060evk_fcb;
static struct imxrt_fcb nor_config __attribute__((aligned(4)));
static bool nor_ready;

void board_putc(char c)
{
    while (!(mmio_read_32(LPUART1_BASE + LPUART_STAT) & STAT_TDRE))
        ;
    mmio_write_32(LPUART1_BASE + LPUART_DATA, (uint8_t)c);
}

uint32_t board_core_hz(void)
{
    /* pb runs the core at 600 MHz */
    return 600000000;
}

void board_wdog_kick(void)
{
    *(volatile uint16_t *)(WDOG1_BASE + WDOG_WSR) = 0x5555;
    *(volatile uint16_t *)(WDOG1_BASE + WDOG_WSR) = 0xaaaa;
}

bool board_wdog_running(uint32_t *timeout_ms)
{
    uint16_t wcr = *(volatile uint16_t *)(WDOG1_BASE + WDOG_WCR);

    /* WT: (WT + 1) * 0.5 s */
    *timeout_ms = ((uint32_t)(wcr >> 8) + 1) * 500;
    return (wcr & WCR_WDE) != 0;
}

static int nor_init(void)
{
    if (nor_ready)
        return 0;

    const uint8_t *src = (const uint8_t *)&imxrt1060evk_fcb;
    uint8_t *dst = (uint8_t *)&nor_config;

    for (size_t i = 0; i < sizeof(nor_config); i++)
        dst[i] = src[i];

    int rc = ROM_API->nor->init(0, &nor_config);

    if (rc != 0)
        return rc;
    nor_ready = true;
    return 0;
}

int board_state_read(unsigned int copy, void *buf, size_t len)
{
    int rc = nor_init();

    if (rc != 0)
        return rc;
    return ROM_API->nor->read(0, &nor_config, (uint32_t *)buf, STATE_OFFSET(copy), len);
}

int board_state_write(unsigned int copy, const void *buf, size_t len)
{
    int rc = nor_init();

    if (rc != 0)
        return rc;

    board_wdog_kick();
    rc = ROM_API->nor->erase(0, &nor_config, STATE_OFFSET(copy), NOR_SECTOR);
    if (rc != 0)
        return rc;

    for (size_t off = 0; off < len; off += NOR_PAGE) {
        static uint32_t page[NOR_PAGE / 4];
        const uint8_t *src = (const uint8_t *)buf + off;
        uint8_t *dst = (uint8_t *)page;

        for (size_t i = 0; i < NOR_PAGE; i++)
            dst[i] = (off + i < len) ? src[i] : 0xff;

        rc = ROM_API->nor->program(0, &nor_config, STATE_OFFSET(copy) + off, page);
        if (rc != 0)
            return rc;
    }

    ROM_API->nor->clear_cache(0);
    return 0;
}

void board_reset(bool command_mode)
{
    mmio_write_32(SRC_GPR5, command_mode ? PB_CM_REQUEST : 0);
    __asm__ volatile("dsb");
    mmio_write_32(SCB_AIRCR, (0x05fau << 16) | (1u << 2)); /* SYSRESETREQ */
    __asm__ volatile("dsb");
    for (;;)
        ;
}

void board_checks(void)
{
    check("console",
          (mmio_read_32(LPUART1_BASE + LPUART_CTRL) & CTRL_TE) != 0,
          "LPUART1 CTRL",
          mmio_read_32(LPUART1_BASE + LPUART_CTRL));
}
