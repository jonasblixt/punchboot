#include <stdio.h>
#include <pb.h>
#include <plat.h>
#include <timing_report.h>

#if TIMING_REPORT >= 1

static uint32_t tr_stamps[TR_NO];

static const char *kind_strings[] =
{
    "TR_POR",
    "TR_BLINIT",
    "TR_BLOCKREAD",
    "TR_SHA",
    "TR_RSA",
    "TR_FINAL"
};


void tr_init(void)
{
}

void tr_stamp(uint32_t kind)
{
    tr_stamps[kind] = plat_get_us_tick();
}

void tr_print_result(void)
{
    printf("Boot timing report:\n\r");
    for (uint32_t i = 0; i < TR_NO; i++)
        printf("%s: %"PRIu32" us\n\r", kind_strings[i],
                                    tr_stamps[i]);
}
#else
void tr_init(void) {}
void tr_stamp(uint32_t kind) { UNUSED(kind); }
void tr_print_result(void) {}
#endif
