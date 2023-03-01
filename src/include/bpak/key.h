#ifndef INCLUDE_BPAK_KEY_H
#define INCLUDE_BPAK_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#define BPAK_DECLARE_STATIC_KEY(__name, __size) \
    uint8_t __##__name##_data[sizeof(struct bpak_key) + __size]; \
    struct bpak_key *__name = (struct bpak_key *) __##__name##_data; \
    __name->size = __size;

struct bpak_key {
    enum bpak_key_kind kind;
    uint16_t size;
    uint32_t id;
    uint8_t rz[4];
    uint8_t data[];
};

#ifdef __cplusplus
} // extern "C"
#endif

#endif // INCLUDE_BPAK_KEY_H
