/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Prints the frozen parts of the wire protocol and the bpak format: struct
 * sizes, field offsets, enum values, flag bits and ids. check_abi.py compares
 * the output with abi.txt, which may only grow (D-008, compatibility.md).
 *
 * Reserved and padding fields (rz, pad) are left out, so a new field taken
 * from them only adds lines. Sentinels (PB_CMD_END, ...) are left out because
 * they move when a value is appended.
 */

#include <bpak/bpak.h>
#include <bpak/id.h>
#include <inttypes.h>
#include <pb-tools/error.h>
#include <pb-tools/wire.h>
#include <pb/bio.h>
#include <pb/slc.h>
#include <pb/utils_def.h>
#include <stddef.h>
#include <stdio.h>

#define STRUCT(s) printf("struct %s %zu\n", #s, sizeof(struct s))
#define FIELD(s, f) \
    printf("field %s.%s %zu %zu\n", #s, #f, offsetof(struct s, f), sizeof(((struct s *)0)->f))
#define VALUE(v) printf("value %s %lld\n", #v, (long long)(v))
#define HEX(v)   printf("value %s 0x%08" PRIx32 "\n", #v, (uint32_t)(v))
#define ID(v)    printf("id %s 0x%08" PRIx32 "\n", #v, (uint32_t)(v))

static void wire(void)
{
    printf("# Wire protocol (include/pb-tools/wire.h, error.h)\n");
    HEX(PB_WIRE_MAGIC);
    VALUE(PB_COMMAND_REQUEST_MAX_SIZE);
    VALUE(PB_RESULT_RESPONSE_MAX_SIZE);

    VALUE(PB_AUTH_INVALID);
    VALUE(PB_AUTH_ASYM_TOKEN);
    VALUE(PB_AUTH_PASSWORD);

    VALUE(PB_CMD_INVALID);
    VALUE(PB_CMD_DEVICE_RESET);
    VALUE(PB_CMD_DEVICE_IDENTIFIER_READ);
    VALUE(PB_CMD_DEVICE_READ_CAPS);
    VALUE(PB_CMD_SLC_SET_CONFIGURATION);
    VALUE(PB_CMD_SLC_SET_CONFIGURATION_LOCK);
    VALUE(PB_CMD_SLC_SET_EOL);
    VALUE(PB_CMD_SLC_REVOKE_KEY);
    VALUE(PB_CMD_SLC_READ);
    VALUE(PB_CMD_BOOTLOADER_VERSION_READ);
    VALUE(PB_CMD_PART_TBL_READ);
    VALUE(PB_CMD_PART_TBL_INSTALL);
    VALUE(PB_CMD_PART_VERIFY);
    VALUE(PB_CMD_PART_ACTIVATE);
    VALUE(PB_CMD_PART_BPAK_READ);
    VALUE(PB_CMD_PART_ERASE);
    VALUE(PB_CMD_AUTHENTICATE);
    VALUE(PB_CMD_AUTH_SET_PASSWORD);
    VALUE(PB_CMD_STREAM_INITIALIZE);
    VALUE(PB_CMD_STREAM_PREPARE_BUFFER);
    VALUE(PB_CMD_STREAM_WRITE_BUFFER);
    VALUE(PB_CMD_STREAM_FINALIZE);
    VALUE(PB_CMD_BOOT_PART);
    VALUE(PB_CMD_BOOT_BPAK);
    VALUE(PB_CMD_BOARD_COMMAND);
    VALUE(PB_CMD_BOARD_STATUS_READ);
    VALUE(PB_CMD_STREAM_READ_BUFFER);
    VALUE(PB_CMD_PART_RESIZE);
    VALUE(PB_CMD_BOOT_STATUS);

    VALUE(PB_RESULT_OK);
    VALUE(PB_RESULT_ERROR);
    VALUE(PB_RESULT_AUTHENTICATION_FAILED);
    VALUE(PB_RESULT_NOT_AUTHENTICATED);
    VALUE(PB_RESULT_NOT_SUPPORTED);
    VALUE(PB_RESULT_INVALID_ARGUMENT);
    VALUE(PB_RESULT_INVALID_COMMAND);
    VALUE(PB_RESULT_PART_VERIFY_FAILED);
    VALUE(PB_RESULT_PART_NOT_BOOTABLE);
    VALUE(PB_RESULT_NO_MEMORY);
    VALUE(PB_RESULT_TRANSFER_ERROR);
    VALUE(PB_RESULT_NOT_FOUND);
    VALUE(PB_RESULT_STREAM_NOT_INITIALIZED);
    VALUE(PB_RESULT_TIMEOUT);
    VALUE(PB_RESULT_KEY_REVOKED);
    VALUE(PB_RESULT_SIGNATURE_ERROR);
    VALUE(PB_RESULT_MEM_ERROR);
    VALUE(PB_RESULT_IO_ERROR);

    VALUE(PB_SLC_INVALID);
    VALUE(PB_SLC_NOT_CONFIGURED);
    VALUE(PB_SLC_CONFIGURATION);
    VALUE(PB_SLC_CONFIGURATION_LOCKED);
    VALUE(PB_SLC_EOL);

    VALUE(PB_WIRE_PART_FLAG_BOOTABLE);
    VALUE(PB_WIRE_PART_FLAG_OTP);
    VALUE(PB_WIRE_PART_FLAG_WRITABLE);
    VALUE(PB_WIRE_PART_FLAG_ERASE_BEFORE_WRITE);

    /* The part table response sends the low byte of the bio flags as is,
     * and the SLC response sends slc_t. Both are wire values. */
    VALUE(BIO_FLAG_BOOTABLE);
    VALUE(BIO_FLAG_WRITABLE);
    VALUE(BIO_FLAG_ERASE_BEFORE_WRITE);
    VALUE(BIO_FLAG_VISIBLE);
    VALUE(BIO_FLAG_READABLE);
    VALUE(SLC_INVALID);
    VALUE(SLC_NOT_CONFIGURED);
    VALUE(SLC_CONFIGURATION);
    VALUE(SLC_CONFIGURATION_LOCKED);
    VALUE(SLC_EOL);

    STRUCT(pb_command);
    FIELD(pb_command, magic);
    FIELD(pb_command, command);
    FIELD(pb_command, request);

    STRUCT(pb_result);
    FIELD(pb_result, magic);
    FIELD(pb_result, result_code);
    FIELD(pb_result, response);

    STRUCT(pb_result_device_caps);
    FIELD(pb_result_device_caps, stream_no_of_buffers);
    FIELD(pb_result_device_caps, stream_buffer_size);
    FIELD(pb_result_device_caps, operation_timeout_ms);
    FIELD(pb_result_device_caps, part_erase_timeout_ms);
    FIELD(pb_result_device_caps, bpak_stream_support);
    FIELD(pb_result_device_caps, chunk_transfer_max_bytes);

    STRUCT(pb_result_part_table_read);
    FIELD(pb_result_part_table_read, no_of_entries);

    STRUCT(pb_result_part_table_entry);
    FIELD(pb_result_part_table_entry, uuid);
    FIELD(pb_result_part_table_entry, description);
    FIELD(pb_result_part_table_entry, first_block);
    FIELD(pb_result_part_table_entry, last_block);
    FIELD(pb_result_part_table_entry, block_size);
    FIELD(pb_result_part_table_entry, flags);

    STRUCT(pb_command_stream_initialize);
    FIELD(pb_command_stream_initialize, part_uuid);

    STRUCT(pb_command_stream_prepare_buffer);
    FIELD(pb_command_stream_prepare_buffer, size);
    FIELD(pb_command_stream_prepare_buffer, id);

    STRUCT(pb_command_stream_write_buffer);
    FIELD(pb_command_stream_write_buffer, size);
    FIELD(pb_command_stream_write_buffer, offset);
    FIELD(pb_command_stream_write_buffer, buffer_id);

    STRUCT(pb_command_stream_read_buffer);
    FIELD(pb_command_stream_read_buffer, size);
    FIELD(pb_command_stream_read_buffer, offset);
    FIELD(pb_command_stream_read_buffer, buffer_id);

    STRUCT(pb_command_authenticate);
    FIELD(pb_command_authenticate, method);
    FIELD(pb_command_authenticate, size);
    FIELD(pb_command_authenticate, key_id);

    STRUCT(pb_command_verify_part);
    FIELD(pb_command_verify_part, uuid);
    FIELD(pb_command_verify_part, sha256);
    FIELD(pb_command_verify_part, size);
    FIELD(pb_command_verify_part, bpak);

    STRUCT(pb_command_activate_part);
    FIELD(pb_command_activate_part, uuid);

    STRUCT(pb_command_read_bpak);
    FIELD(pb_command_read_bpak, uuid);

    STRUCT(pb_command_erase_part);
    FIELD(pb_command_erase_part, uuid);
    FIELD(pb_command_erase_part, start_lba);
    FIELD(pb_command_erase_part, block_count);

    STRUCT(pb_command_install_part_table);
    FIELD(pb_command_install_part_table, uu);
    FIELD(pb_command_install_part_table, variant);

    STRUCT(pb_result_device_identifier);
    FIELD(pb_result_device_identifier, device_uuid);
    FIELD(pb_result_device_identifier, board_id);

    STRUCT(pb_result_slc);
    FIELD(pb_result_slc, slc);

    STRUCT(pb_result_slc_key_status);
    FIELD(pb_result_slc_key_status, active);
    FIELD(pb_result_slc_key_status, revoked);

    STRUCT(pb_command_revoke_key);
    FIELD(pb_command_revoke_key, key_id);

    STRUCT(pb_command_boot_part);
    FIELD(pb_command_boot_part, uuid);
    FIELD(pb_command_boot_part, verbose);

    STRUCT(pb_command_bpak_boot);
    FIELD(pb_command_bpak_boot, verbose);
    FIELD(pb_command_bpak_boot, uuid);

    STRUCT(pb_command_board);
    FIELD(pb_command_board, command);
    FIELD(pb_command_board, request_size);
    FIELD(pb_command_board, response_buffer_size);

    STRUCT(pb_result_board);
    FIELD(pb_result_board, size);

    STRUCT(pb_result_board_status);
    FIELD(pb_result_board_status, size);

    STRUCT(pb_result_boot_status);
    FIELD(pb_result_boot_status, uuid);
    FIELD(pb_result_boot_status, status);
}

static void bpak(void)
{
    printf("# bpak format (include/bpak/bpak.h, id.h)\n");
    HEX(BPAK_HEADER_MAGIC);
    VALUE(BPAK_MAX_PARTS);
    VALUE(BPAK_MAX_META);
    VALUE(BPAK_METADATA_BYTES);
    VALUE(BPAK_PART_ALIGN);
    VALUE(BPAK_SIGNATURE_MAX_BYTES);
    VALUE(BPAK_HASH_MAX_LENGTH);

    VALUE(BPAK_HASH_INVALID);
    VALUE(BPAK_HASH_SHA256);
    VALUE(BPAK_HASH_SHA384);
    VALUE(BPAK_HASH_SHA512);

    VALUE(BPAK_SIGN_INVALID);
    VALUE(BPAK_SIGN_RSA4096);
    VALUE(BPAK_SIGN_PRIME256v1);
    VALUE(BPAK_SIGN_SECP384r1);
    VALUE(BPAK_SIGN_SECP521r1);

    VALUE(BPAK_KEY_INVALID);
    VALUE(BPAK_KEY_PUB_RSA4096);
    VALUE(BPAK_KEY_PUB_PRIME256v1);
    VALUE(BPAK_KEY_PUB_SECP384r1);
    VALUE(BPAK_KEY_PUB_SECP521r1);
    VALUE(BPAK_KEY_PRI_RSA4096);
    VALUE(BPAK_KEY_PRI_PRIME256v1);
    VALUE(BPAK_KEY_PRI_SECP384r1);
    VALUE(BPAK_KEY_PRI_SECP521r1);

    VALUE(BPAK_COMPRESSION_NONE);
    VALUE(BPAK_COMPRESSION_HS);
    VALUE(BPAK_COMPRESSION_LZMA);

    VALUE(BPAK_FLAG_EXCLUDE_FROM_HASH);
    VALUE(BPAK_FLAG_TRANSPORT);

    ID(BPAK_ID_BPAK_TRANSPORT);
    ID(BPAK_ID_MERKLE_SALT);
    ID(BPAK_ID_MERKLE_ROOT_HASH);
    ID(BPAK_ID_KEYSTORE_PROVIDER_ID);
    ID(BPAK_ID_BPAK_VERSION);
    ID(BPAK_ID_PB_LOAD_ADDR);
    ID(BPAK_ID_BPAK_PACKAGE);
    ID(BPAK_ID_BPAK_KEY_ID);
    ID(BPAK_ID_BPAK_KEY_STORE);
    ID(BPAK_ID_BSDIFF);
    ID(BPAK_ID_BSDIFF_LZMA);
    ID(BPAK_ID_BSDIFF_NO_COMP);
    ID(BPAK_ID_BSPATCH);
    ID(BPAK_ID_BSPATCH_LZMA);
    ID(BPAK_ID_BSPATCH_NO_COMP);
    ID(BPAK_ID_MERKLE_GENERATE);
    ID(BPAK_ID_REMOVE_DATA);

    STRUCT(bpak_transport_meta);
    FIELD(bpak_transport_meta, alg_id_encode);
    FIELD(bpak_transport_meta, alg_id_decode);
    FIELD(bpak_transport_meta, data);

    STRUCT(bpak_part_header);
    FIELD(bpak_part_header, id);
    FIELD(bpak_part_header, size);
    FIELD(bpak_part_header, offset);
    FIELD(bpak_part_header, transport_size);
    FIELD(bpak_part_header, pad_bytes);
    FIELD(bpak_part_header, flags);

    STRUCT(bpak_meta_header);
    FIELD(bpak_meta_header, id);
    FIELD(bpak_meta_header, size);
    FIELD(bpak_meta_header, offset);
    FIELD(bpak_meta_header, part_id_ref);

    STRUCT(bpak_header);
    FIELD(bpak_header, magic);
    FIELD(bpak_header, meta);
    FIELD(bpak_header, parts);
    FIELD(bpak_header, metadata);
    FIELD(bpak_header, hash_kind);
    FIELD(bpak_header, signature_kind);
    FIELD(bpak_header, alignment);
    FIELD(bpak_header, payload_hash);
    FIELD(bpak_header, key_id);
    FIELD(bpak_header, keystore_id);
    FIELD(bpak_header, signature);
    FIELD(bpak_header, signature_sz);
}

int main(void)
{
    wire();
    bpak();
    return 0;
}
