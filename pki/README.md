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
| `imx6ul_hab_testkeys/`                  | HABv4 (i.MX6UL, i.MX8M): the SRK table and fuse words, and the CSF1   |
|                                         | and IMG1 certificates and keys `cst` signs with                       |
| `imx8x_ahab/`                           | AHAB (i.MX8X): the SRK table and fuse words, and SRK1's certificate   |
|                                         | and key, encrypted with `keys/key_pass.txt`                           |

`cst` finds a certificate's key by its name, `_crt` replaced by `_key` (and `crts/` by `keys/`), so
the key files are used without being named anywhere. The SRK tables were made from four SRKs;
only the one that signs is kept.
