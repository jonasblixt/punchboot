#ifndef INCLUDE_BPAK_KEY_H
#define INCLUDE_BPAK_KEY_H

#include <bpak/bpak.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \def BPAK_DECLARE_STATIC_KEY
 * Declare a 'struct bpak_key *' called '__name' with room for '__size' bytes
 * of key data on the stack. The storage is a union with the struct, so it is
 * aligned for it.
 */
#define BPAK_DECLARE_STATIC_KEY(__name, __size)           \
    union {                                               \
        struct bpak_key key;                              \
        uint8_t data[sizeof(struct bpak_key) + (__size)]; \
    } __##__name##_data;                                  \
    struct bpak_key *__name = &__##__name##_data.key;     \
    __name->size = (__size);

/**
 * A key, followed by its DER encoded data
 */
struct bpak_key {
    enum bpak_key_kind kind; /*!< Kind of key */
    uint16_t size; /*!< Size of 'data' in bytes */
    uint32_t id; /*!< Key id, the part id in a keystore */
    uint8_t rz[4]; /*!< Reserved, set to zero */
    uint8_t data[]; /*!< DER encoded key */
};

#ifdef __cplusplus
} // extern "C"
#endif

#endif // INCLUDE_BPAK_KEY_H
