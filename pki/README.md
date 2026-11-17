# Test keys

**Test keys only.** Their private keys are public: never a product's keys.

| File                                    | What, and who uses it                                                 |
| --------------------------------------- | --------------------------------------------------------------------- |
| `dev-keys.mk`                           | pb's development keys, for a board whose `makefile.mk` names no key   |
|                                         | (`doc/interfaces/board.md` 7)                                         |
| `secp256r1-pub-key.der`, `.pem`         | `pb-development` (slot 0), its key pair `secp256r1-key-pair.pem`      |
| `secp384r1-pub-key.der`, `.pem`         | `pb-development2` (slot 1), its key pair `secp384r1-key-pair.pem`     |
| `secp521r1-pub-key.der`, `.pem`         | `pb-development3` (slot 2), its key pair `secp521r1-key-pair.pem`     |
| `secp256r1-key-pair2.pem`               | A key pb doesn't have: the integration tests' unknown signer          |
| `secp256r1-priv-key.der`                | A private key in DER, which `scripts/gen-keystore.py` refuses (test)  |
| `test_rsa_public.der`                   | An RSA key, which `scripts/gen-keystore.py` refuses (test)            |
| `imx6ul_hab_testkeys/`                  | HABv4 (i.MX6UL, i.MX8M): the SRK table and fuse words, SRK1 to SRK4's |
|                                         | certificates and keys, and each SRK's CSF and IMG certificate and key |
|                                         | (`CSFn_1`, `IMGn_1`). `cst` signs with SRK1's by default. The SRK,    |
|                                         | CSF2-4 and IMG2-4 keys are encrypted with `key_pass.txt`              |
| `imx8x_ahab/`                           | AHAB (i.MX8X): the SRK table and fuse words, and SRK1 to SRK4's       |
|                                         | certificates and keys, encrypted with `keys/key_pass.txt`             |

`cst` finds a certificate's key by its name, `_crt` replaced by `_key` (and `crts/` by `keys/`), so
the key files are used without being named anywhere.

**All four SRKs stay**, also the ones no build signs with. Their hash is fused on boards (jiffy,
the pico8ml, the MEK): when SRK1 is revoked (pb's key 0 shares its revoke fuse), SRK2 to SRK4 are
what still signs a pb those boards boot, and a revocation can't be tested without them. Each set
rebuilds the fused table and hash with `srktool`. Another chain signs with the defconfig's
`*_KEY_INDEX` (n - 1 for SRKn) and its certificates, e.g. `IMX6UL_KEY_INDEX=1`
`IMX6UL_SIGN_CERT=.../CSF2_1_...` `IMX6UL_IMAGE_SIGN_CERT=.../IMG2_1_...`.
