#ifndef __TIMING_REPORT_H__
#define __TIMING_REPORT_H__

#include <stdint.h>

#define TR_NO 6

enum tr_kinds
{
    TR_POR = 0,
    TR_BLINIT = 1,
    TR_LOAD = 2,
    TR_VERIFY = 3,
    TR_FINAL = 5,
};

void tr_init(void);
void tr_stamp_begin(uint32_t kind);
void tr_stamp_end(uint32_t kind);
void tr_print_result(void);

#endif
