/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * pb/log_def.h with no level compiled in (LOGLEVEL 0, no RAM log): each LOG
 * macro is a statement, so one after an if or an else is no empty body
 * (-Wempty-body, F-109). Compiled with -Werror by unit.log_disabled_is_a_statement.
 */

#include <pb/log_def.h>

int log_disabled(int x);

int log_disabled(int x)
{
    if (x == 1)
        LOG_ERR("one");
    else
        LOG_WARN("not one");

    if (x == 2)
        LOG_INFO("two");
    else
        LOG_DBG("not two");

    return x;
}
