#include "nala.h"
#include <pb-tools/wire.h>

TEST(alignment)
{
    ASSERT_EQ(sizeof(struct pb_command) % 64, 0);

    ASSERT_EQ(sizeof(struct pb_result) % 64, 0);
}

TEST(result_structs)
{
    struct pb_result result;

    ASSERT(sizeof(struct pb_result_device_caps) <= sizeof(result.response));
    ASSERT(sizeof(struct pb_result_device_identifier) <= sizeof(result.response));
}

TEST(command_structs)
{
    struct pb_command command;

    ASSERT(sizeof(struct pb_command_authenticate) <= sizeof(command.request));
    ASSERT(sizeof(struct pb_command_stream_write_buffer) <= sizeof(command.request));
    ASSERT(sizeof(struct pb_command_stream_prepare_buffer) <= sizeof(command.request));
    ASSERT(sizeof(struct pb_command_stream_initialize) <= sizeof(command.request));
    ASSERT(sizeof(struct pb_command_verify_part) <= sizeof(command.request));
}

TEST(various)
{
}
