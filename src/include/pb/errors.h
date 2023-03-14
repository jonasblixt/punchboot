/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_ERRORS_H_
#define INCLUDE_PB_ERRORS_H_

enum pb_errors {
    PB_OK = 0,
    PB_ERR,
    PB_TIMEOUT,
    PB_KEY_REVOKED_ERROR,
    PB_SIGNATURE_ERROR,
    PB_CHECKSUM_ERROR,
    PB_ERR_MEM,
    PB_ERR_IO,
    PB_ERR_FILE_NOT_FOUND,
    PB_ERR_NOT_IMPLEMENTED,
    PB_ERR_BUF_TOO_SMALL,
    PB_ERR_ABORT,
    PB_ERR_PARAM,
    PB_ERR_NOT_SUPPORTED,
};

#endif  // INCLUDE_PB_ERRORS_H_
