#ifndef INCLUDE_BPAK_KEYSTORE_H_
#define INCLUDE_BPAK_KEYSTORE_H_

#include <bpak/bpak.h>
#include <bpak/key.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*bpak_check_header_t)(struct bpak_header *header, void *user);

struct bpak_keystore {
    uint32_t id;
    uint8_t no_of_keys;
    bool verified;
    struct bpak_key *keys[];
};

int bpak_keystore_get(struct bpak_keystore *ks, uint32_t id,
                      struct bpak_key **k);
/**
 * Load key from a keystore archive
 *
 *  `filename`     Path to a bpak keystore archive
 *  `keystore_id`  The keystore archive should have a matching
 *                          'keystore-provider-id' meta data.
 *  `key_id`       Key within the keystore to extract
 *  `check_header` Optional callback to verify other meta data in the
 *                      keystore header.
 *  `user`         Optional context pointer passed to 'check_header'
 *  `output`       Key output
 *
 * NOTE: bpak_keystore_load_from_file allocates a key struct and
 *  the user must free this memory.
 *
 * Returns BPAK_OK on success or a negative number.
 */
int bpak_keystore_load_key_from_file(const char *filename,
                                     uint32_t keystore_id,
                                     uint32_t key_id,
                                     bpak_check_header_t check_header,
                                     void *user,
                                     struct bpak_key **output);
#ifdef __cplusplus
} // extern "C"
#endif

#endif // INCLUDE_BPAK_KEYSTORE_H_
