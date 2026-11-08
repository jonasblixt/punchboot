#ifndef INCLUDE_BPAK_KEYSTORE_H_
#define INCLUDE_BPAK_KEYSTORE_H_

#include <bpak/bpak.h>
#include <bpak/key.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Callback that checks the header of a keystore package, returns BPAK_OK to
 * accept it
 */
typedef int (*bpak_check_header_t)(struct bpak_header *header, void *user);

/**
 * A keystore in memory, for example generated with 'bpak generate keystore'
 */
struct bpak_keystore {
    uint32_t id; /*!< Keystore id, 'keystore-provider-id' */
    uint8_t no_of_keys; /*!< Number of keys in 'keys' */
    bool verified; /*!< Keys can only be used when this is true */
    struct bpak_key *keys[]; /*!< The keys */
};

/**
 * Get a key from a keystore
 *
 * @param[in] ks Keystore
 * @param[in] id Key id
 * @param[out] k Pointer to the key
 *
 * @return BPAK_OK on success, -BPAK_FAILED if the keystore is not verified
 *         or -BPAK_KEY_NOT_FOUND if it does not contain the key
 */
int bpak_keystore_get(struct bpak_keystore *ks, uint32_t id, struct bpak_key **k);
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
 * NOTE: bpak_keystore_load_key_from_file allocates a key struct and
 *  the user must free this memory.
 *
 * The payload hash of the keystore is verified, the keystore is not signed.
 *
 * Returns BPAK_OK on success, -BPAK_BAD_PAYLOAD_HASH if the keystore is
 *  corrupted, -BPAK_KEYSTORE_ID_MISMATCH, -BPAK_KEY_NOT_FOUND if it does not
 *  contain the key or a negative number.
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
