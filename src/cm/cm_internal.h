/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Command mode's own: what its files share (src/cm/). Not an interface of
 * pb's, nothing outside src/cm/ includes it.
 *
 * cm_main.c has the session, the transfers, the gate, the dispatch of a
 * command and the task. Each command's handler is cm_cmd_*(), in the file of
 * its subject: cm_auth.c, cm_part.c (the partitions and the stream),
 * cm_boot.c, cm_slc.c, cm_board.c and cm_device.c. A handler reads its
 * request from cm_cmd and leaves its last result in cm_result, which the
 * dispatch sends, unless the handler says otherwise (command-mode.md 4.4).
 */

#ifndef SRC_CM_CM_INTERNAL_H
#define SRC_CM_CM_INTERNAL_H

#include <pb-tools/wire.h>
#include <pb/bio.h>
#include <pb/cm.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include <pb/slc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <uuid.h>

#define CM_BUF_SIZE       (CONFIG_CM_BUF_SIZE_KiB * 1024)
/* Hosts send authentication and named command data in one transfer of this
 * size, whatever the data's size */
#define CM_DATA_XFER_SIZE 1024

/* The session's state, cm_main.c's */
extern struct pb_command cm_cmd; /* The command that is run */
extern struct pb_result cm_result; /* Its result */
extern bool cm_authenticated;
extern uint8_t cm_buffer[2][CM_BUF_SIZE]; /* The stream's buffers */
extern slc_t cm_slc; /* The life cycle, as last read */
extern uint8_t cm_hash[CRYPTO_MD_MAX_SZ];
extern uuid_t cm_device_uu;
extern const struct cm_config *cm_cfg;
/* A command ended command mode: the task leaves when it is done */
extern bool cm_leave_requested;

/* The transfers (cm_main.c, command-mode.md 4.4, 4.6) */
int cm_error_to_wire(int error_code);
int cm_write(const void *buf, size_t length);
int cm_read(void *buf, size_t length);
int cm_drain(size_t length);
void cm_disconnect(void);

/* cm_auth.c */
int cm_cmd_auth(void);

/* cm_part.c */
void cm_stream_end(void);
bool cm_part_hidden(const uuid_t uu);
int cm_cmd_bpak_read(void);
int cm_cmd_part_verify(void);
int cm_cmd_part_erase(struct pb_command_erase_part *erase_cmd);
int cm_cmd_part_tbl_install(const struct pb_command_install_part_table *install_cmd);
int cm_cmd_part_tbl_read(void);
int cm_cmd_stream_init(void);
int cm_cmd_stream_prep_buffer(void);
int cm_cmd_stream_read(void);
int cm_cmd_stream_write(void);
int cm_cmd_stream_final(void);

/* cm_boot.c. The boots send their results themselves */
int cm_cmd_boot_bpak(void);
int cm_cmd_boot_part(void);
int cm_cmd_part_activate(void);
int cm_cmd_boot_status(void);

/* cm_slc.c */
int cm_cmd_slc_read(void);
int cm_cmd_slc_set_configuration(void);
int cm_cmd_slc_set_configuration_lock(void);
int cm_cmd_slc_set_eol(void);
int cm_cmd_slc_revoke_key(void);

/* cm_board.c */
int cm_cmd_board(void);
int cm_cmd_command_list(void);
int cm_cmd_board_status(void);

/* cm_device.c */
int cm_cmd_version_read(void);
int cm_cmd_read_caps(void);
int cm_cmd_device_identifier_read(void);
int cm_cmd_log_read(void);

#endif // SRC_CM_CM_INTERNAL_H
