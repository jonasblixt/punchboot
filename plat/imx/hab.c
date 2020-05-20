
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <stdbool.h>
#include <pb/pb.h>
#include <pb/io.h>
#include <plat/imx/hab.h>
#include <plat/imx/ocotp.h>

#define IS_HAB_ENABLED_BIT 0x02
static uint8_t _event_data[128];




#define MAX_RECORD_BYTES     (8*1024) /* 4 kbytes */

struct record
{
    uint8_t  tag;                        /* Tag */
    uint8_t  len[2];                    /* Length */
    uint8_t  par;                        /* Version */
    uint8_t  contents[MAX_RECORD_BYTES];/* Record Data */
    bool     any_rec_flag;
};

static inline uint8_t get_idx(uint8_t *list, uint8_t tgt)
{
    uint8_t idx = 0;
    int8_t element = list[idx];

    while (element != -1)
    {
        if (element == tgt)
            return idx;
        element = list[++idx];
    }
    return -1;
}

int hab_secureboot_active(bool *result)
{
    uint32_t reg;
    int ret;

    (*result) = false;

    ret = ocotp_read(0, 6, &reg);

    if (ret != PB_OK)
    {
        LOG_ERR("Secure boot fuse read error");
        return -PB_ERR;
    }

    if ((reg & IS_HAB_ENABLED_BIT) == IS_HAB_ENABLED_BIT)
    {
        (*result) = true;
    }

    return PB_OK;
}

int hab_has_no_errors(void)
{
    uint32_t index = 0;
    size_t bytes = sizeof(_event_data);
    uint32_t result;
    enum hab_config config = 0;
    enum hab_state state = 0;
    uint32_t i;

    hab_rvt_report_event_t *hab_rvt_report_event;
    hab_rvt_report_status_t *hab_rvt_report_status;

    hab_rvt_report_event =
                (hab_rvt_report_event_t *)(uintptr_t)HAB_RVT_REPORT_EVENT;
    hab_rvt_report_status =
                (hab_rvt_report_status_t *)(uintptr_t)HAB_RVT_REPORT_STATUS;

    result = hab_rvt_report_status(&config, &state);
    LOG_INFO("configuration: 0x%x, state: 0x%x", config, state);
    LOG_INFO(" result = %u", result);

    while (hab_rvt_report_event(HAB_WARNING, index, _event_data, &bytes)
                                            == HAB_SUCCESS)
    {
        LOG_WARN(" %x, event data:", index+1);

        for (i = 0; i < bytes; i++)
            printf(" 0x%x", _event_data[i]);
        printf("\n\r");

        bytes = sizeof(_event_data);
        index++;
    }
    while (hab_rvt_report_event(HAB_FAILURE, index, _event_data, &bytes)
                                            == HAB_SUCCESS)
    {
        LOG_ERR("Error %x, event data:", index+1);

        for (i = 0; i < bytes; i++)
            printf(" 0x%x", _event_data[i]);
        printf("\n\r");

        bytes = sizeof(_event_data);
        index++;
    }

    return (result == HAB_SUCCESS)?PB_OK:PB_ERR;
}

