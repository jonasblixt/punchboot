/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <plat/imx6ul/hab.h>
#include <stdio.h>

static uint8_t _event_data[128];

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

    hab_rvt_report_event = (hab_rvt_report_event_t *)(uintptr_t)HAB_RVT_REPORT_EVENT;
    hab_rvt_report_status = (hab_rvt_report_status_t *)(uintptr_t)HAB_RVT_REPORT_STATUS;

    LOG_DBG("Calling hab_rvt_report_status <%p> <%p>", hab_rvt_report_status, hab_rvt_report_event);

    result = hab_rvt_report_status(&config, &state);
    LOG_INFO("configuration: 0x%x, state: 0x%x", config, state);
    LOG_INFO(" result = %u", result);

    while (hab_rvt_report_event(HAB_WARNING, index, _event_data, &bytes) == HAB_SUCCESS) {
        LOG_WARN(" %x, event data:", index + 1);

        for (i = 0; i < bytes; i++)
            printf(" 0x%x", _event_data[i]);
        printf("\n\r");

        bytes = sizeof(_event_data);
        index++;
    }

    /* The index counts the events of the status asked for */
    index = 0;

    while (hab_rvt_report_event(HAB_FAILURE, index, _event_data, &bytes) == HAB_SUCCESS) {
        LOG_ERR("Error %x, event data:", index + 1);

        for (i = 0; i < bytes; i++)
            printf(" 0x%x", _event_data[i]);
        printf("\n\r");

        bytes = sizeof(_event_data);
        index++;
    }

    return (result == HAB_SUCCESS) ? PB_OK : -PB_ERR;
}
