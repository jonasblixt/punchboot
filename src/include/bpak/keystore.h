#ifndef INCLUDE_BPAK_KEYSTORE_H_
#define INCLUDE_BPAK_KEYSTORE_H_

#include <bpak/bpak.h>

#define BPAK_KEYSTORE_UUID "5df103ef-e774-450b-95c5-1fef51ceec28"

enum bpak_key_kind
{
    BPAK_KEY_INVALID,
    BPAK_KEY_PUB_RSA4096,
    BPAK_KEY_PUB_PRIME256v1,
    BPAK_KEY_PUB_SECP384r1,
    BPAK_KEY_PUB_SECP521r1,
    BPAK_KEY_PRI_RSA4096,
    BPAK_KEY_PRI_PRIME256v1,
    BPAK_KEY_PRI_SECP384r1,
    BPAK_KEY_PRI_SECP521r1,
};

struct bpak_key
{
    enum bpak_key_kind kind;
    uint16_t size;
    uint32_t id;
    uint8_t data[];
};
struct bpak_keystore
{
    uint32_t id;
    uint8_t no_of_keys;
    bool verified;
    struct bpak_key *keys[];
};

int bpak_keystore_empty(struct bpak_keystore *ks, uint32_t id);

int bpak_keystore_verify(struct bpak_keystore *ks,
                         struct bpak_keystore *internal_ks,
                         uint8_t key_id);

int bpak_keystore_add(struct bpak_keystore *ks, struct bpak_key *k);
int bpak_keystore_get(struct bpak_keystore *ks, uint8_t id,
                        struct bpak_key **k);

int bpak_keystore_add_pem(struct bpak_keystore *ks, const char *filename);
int bpak_keystore_add_der(struct bpak_keystore *ks, const char *filename);

#endif  // INCLUDE_BPAK_KEYSTORE_H_
