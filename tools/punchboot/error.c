#include <pb-tools/error.h>
#include <stdio.h>

const char *pb_error_string(int result)
{
    switch (result) {
    case PB_RESULT_OK:
        return "OK";
    case -PB_RESULT_ERROR:
        return "Error";
    case -PB_RESULT_AUTHENTICATION_FAILED:
        return "Authentication failed";
    case -PB_RESULT_NOT_AUTHENTICATED:
        return "Not authenticated";
    case -PB_RESULT_NOT_SUPPORTED:
        return "Not supported";
    case -PB_RESULT_INVALID_ARGUMENT:
        return "Invalid argument";
    case -PB_RESULT_INVALID_COMMAND:
        return "Invalid command";
    case -PB_RESULT_PART_VERIFY_FAILED:
        return "Partition verify failed";
    case -PB_RESULT_PART_NOT_BOOTABLE:
        return "Partition not bootable";
    case -PB_RESULT_NO_MEMORY:
        return "Memory error";
    case -PB_RESULT_TRANSFER_ERROR:
        return "Transfer error";
    case -PB_RESULT_NOT_FOUND:
        return "Not found";
    case -PB_RESULT_STREAM_NOT_INITIALIZED:
        return "Stream not initialized";
    case -PB_RESULT_TIMEOUT:
        return "Timeout error";
    case -PB_RESULT_KEY_REVOKED:
        return "Invalid key, key is revoked";
    case -PB_RESULT_SIGNATURE_ERROR:
        return "Signature error";
    case -PB_RESULT_MEM_ERROR:
        return "Memory error";
    case -PB_RESULT_IO_ERROR:
        return "I/O Error";
    default:
        return "";
    }
}
