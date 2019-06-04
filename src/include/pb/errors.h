
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __PB_ERRORS_H__
#define __PB_ERRORS_H__


enum {
    PB_OK,
    PB_ERR,
    PB_TIMEOUT,
    PB_KEY_REVOKED_ERROR,
    PB_SIGNATURE_ERROR,
    PB_CHECKSUM_ERROR,
    PB_ERR_MEM,
    PB_ERR_IO,
    PB_ERR_FILE_NOT_FOUND,
};

#endif
