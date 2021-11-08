#ifndef INCLUDE_PB_API_H_
#define INCLUDE_PB_API_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pb-tools/api.h>
#include <bpak/bpak.h>

struct pb_context;
struct pb_command;

typedef int (*pb_init_t) (struct pb_context *ctx);
typedef int (*pb_free_t) (struct pb_context *ctx);

typedef int (*pb_write_t) (struct pb_context *ctx, const void *bfr,
                                    size_t sz);

typedef int (*pb_read_t) (struct pb_context *ctx, void *bfr, size_t sz);

typedef int (*pb_list_devices_t) (struct pb_context *ctx);

typedef int (*pb_connect_t) (struct pb_context *ctx);
typedef int (*pb_disconnect_t) (struct pb_context *ctx);

typedef int (*pb_debug_t) (struct pb_context *ctx, int level,
                           const char *fmt, ...);
struct pb_context
{
    bool connected;
    pb_init_t init;
    pb_free_t free;
    pb_write_t write;
    pb_read_t read;
    pb_list_devices_t list;
    pb_connect_t connect;
    pb_disconnect_t disconnect;
    pb_debug_t d;
    void *transport;
};

struct pb_device_capabilities
{
    uint8_t stream_no_of_buffers;
    uint32_t stream_buffer_size;
    uint16_t operation_timeout_ms;
    uint16_t part_erase_timeout_ms;
    uint8_t bpak_stream_support;
    uint32_t chunk_transfer_max_bytes;
};

#define PB_PART_FLAG_BOOTABLE (1 << 0)
#define PB_PART_FLAG_OTP      (1 << 1)
#define PB_PART_FLAG_WRITABLE (1 << 2)
#define PB_PART_FLAG_ERASE_BEFORE_WRITE (1 << 3)

struct pb_partition_table_entry
{
    uint8_t uuid[16];     /*!< Partition UUID */
    char description[37]; /*!< Textual description of partition */
    uint64_t first_block; /*!< Partition start block */
    uint64_t last_block;  /*!< Last(inclusive) block of partition */
    uint16_t block_size;  /*!< Block size */
    uint8_t flags;        /*!< Flags */
};

int pb_api_create_context(struct pb_context **ctx, pb_debug_t debug);
int pb_api_free_context(struct pb_context *ctx);

int pb_api_device_reset(struct pb_context *ctx);

int pb_api_device_read_identifier(struct pb_context *ctx,
                                  uint8_t *device_uuid,
                                  size_t device_uuid_size,
                                  char *board_id,
                                  size_t board_id_size);

int pb_api_authenticate_password(struct pb_context *ctx,
                                    uint8_t *data,
                                    size_t size);

int pb_api_authenticate_key(struct pb_context *ctx,
                            uint32_t key_id,
                            uint8_t *data,
                            size_t size);

int pb_api_slc_read(struct pb_context *ctx,
                       uint8_t  *slc,
                       uint8_t *active_keys,
                       uint8_t *revoked_keys);

int pb_api_slc_set_configuration(struct pb_context *ctx);

int pb_api_slc_set_configuration_lock(struct pb_context *ctx);

int pb_api_slc_set_end_of_life(struct pb_context *ctx);

int pb_api_slc_revoke_key(struct pb_context *ctx, uint32_t key_id);

int pb_api_device_read_caps(struct pb_context *ctx,
                            struct pb_device_capabilities *caps);

int pb_api_auth_set_otp_password(struct pb_context *ctx,
                                 const char *password,
                                 size_t size);

int pb_api_bootloader_version(struct pb_context *ctx,
                              char *version,
                              size_t size);

int pb_api_partition_read_table(struct pb_context *ctx,
                                struct pb_partition_table_entry *out,
                                int *entries);

int pb_api_partition_install_table(struct pb_context *ctx);

int pb_api_partition_verify(struct pb_context *ctx,
                            uint8_t *uuid,
                            uint8_t *sha256,
                            uint32_t size,
                            bool bpak);


int pb_api_partition_read_bpak(struct pb_context *ctx,
                              uint8_t *uuid,
                              struct bpak_header *header);

int pb_api_partition_erase(struct pb_context *ctx, uint8_t *uuid);

int pb_api_partition_resize(struct pb_context *ctx, uint8_t *uuid,
                                size_t blocks);

int pb_api_stream_init(struct pb_context *ctx, uint8_t *uuid);

int pb_api_stream_prepare_buffer(struct pb_context *ctx,
                                 uint8_t buffer_id,
                                 void *data,
                                 uint32_t size);

int pb_api_stream_write_buffer(struct pb_context *ctx,
                               uint8_t buffer_id,
                               uint64_t offset,
                               uint32_t size);

int pb_api_stream_read_buffer(struct pb_context *ctx,
                              uint8_t buffer_id,
                              uint64_t offset,
                              uint32_t size,
                              void *data);

int pb_api_stream_finalize(struct pb_context *ctx);

int pb_api_boot_part(struct pb_context *ctx, uint8_t *uuid, bool verbose);

int pb_api_boot_ram(struct pb_context *ctx,
                    const void *bpak_image,
                    uint8_t *uuid,
                    bool verbose);

int pb_api_boot_activate(struct pb_context *ctx, uint8_t *uuid);

int pb_api_board_command(struct pb_context *ctx,
                         uint32_t board_command_id,
                         const void *request,
                         size_t request_size,
                         void *response,
                         size_t response_size);

int pb_api_board_status(struct pb_context *ctx,
                        char *status,
                        size_t size);

int pb_api_boot_status(struct pb_context *ctx,
                       uint8_t *uuid,
                       char *status_message,
                       size_t len);

#endif  // INCLUDE_PB_API_H_
