# Vendored code

The code in the tree that pb didn't write, where each piece comes from, which version it is and
what was changed in it (F-022). A version that no file records is inferred, and the entry says
from what. Read the entry before updating a component, and change it with the update.

Mbed TLS isn't in the tree, but the firmware and the host tests compile it: it is here too.

| Component               | Where                                             | Version                      | Licence            |
| ----------------------- | ------------------------------------------------- | ---------------------------- | ------------------ |
| bpak, a subset          | `src/lib/bpak.c`, `include/bpak/`                 | 1.0.1                        | BSD-3-Clause       |
| Mbed TLS                | not in the tree: `MBEDTLS_DIR`                    | 3.4.0                        | Apache-2.0         |
| Kconfiglib              | `scripts/kconfiglib.py`, `genconfig.py`,          | 14.1.0                       | ISC                |
|                         | `menuconfig.py`, `olddefconfig.py`                |                              |                    |
| libfdt (dtc)            | `src/lib/fdt/`, `include/fdt/`                    | inferred: 1.4.1              | BSD-2-Clause or    |
|                         |                                                   |                              | GPL-2.0+, BSD used |
| libuuid (util-linux)    | `src/lib/uuid/`, `include/uuid.h`                 | inferred: 2.16 to 2.19       | BSD-3-Clause       |
| TF-A translation tables | `src/lib/xlat_tables_v2/`, `include/xlat_tables/` | inferred: about v2.3         | BSD-3-Clause       |
| TF-A libc               | `src/lib/libc/`, `include/libc/`                  | inferred: v2.0, `inttypes.h` | BSD-3-Clause (some |
|                         |                                                   | later                        | BSD-2-Clause)      |
| TF-A export headers     | `src/boot/atf-interface/`                         | TF-A commit `1051606c3df3`   | BSD-3-Clause       |
| NXP SCFW client API     | `src/plat/imx8x/sci/`, `include/plat/imx8x/sci/`  | API 1.31                     | BSD-3-Clause       |

The tests' coverage leaves out `src/lib/fdt` and `src/lib/uuid` (`testing.md`), and the document
lint doesn't read `scripts/kconfiglib.py`, `src/lib/`, `src/plat/imx8x/sci/`,
`src/boot/atf-interface/` and `include/xlat_tables/` (`VENDORED` in `tests/doc/check_docs.py`).
Only bpak and the SCFW API were updated after their import, and two headers added to the libc.
The commits named below are in this repository.

## bpak

- **Files**: `src/lib/bpak.c`, `include/bpak/{bpak.h,id.h,keystore.h,key.h}`.
- **Upstream**: github.com/jonasblixt/bpak, the tag `v1.0.1`.
- **Version**: 1.0.1, recorded in `include/bpak/version.h`, which `scripts/sync-bpak` writes.
  Copied on 2026-11-08 (`3a14f9ef`). The dev container's bpak CLI is the same version.
- **Local changes**: formatted with pb's clang-format by the script. The copyright lines of
  `bpak.c`, `bpak.h` and `id.h` have the author's new email (`512156ad`); the next sync puts
  upstream's back unless upstream has it too. pb's own files beside them:
  `include/bpak/build_config.h`, `include/bpak/version.h`, `src/lib/internal.h`,
  `include/libc/unistd.h`.
- **Updating**: `bpak.md`, "What is vendored", and `compatibility.md` before committing.

## Mbed TLS

- **Where**: not in the tree. `docker/Dockerfile` downloads the tag's archive from
  github.com/Mbed-TLS/mbedtls, checks its SHA-256 (in the Dockerfile) and unpacks it to
  `/opt/mbedtls-3.4.0`, which `MBEDTLS_DIR` names. `src/drivers/crypto/mbedtls/makefile.mk`
  (firmware) and `CMakeLists.txt` (host tests) compile the files they need from it.
- **Version**: 3.4.0, `MBEDTLS_VERSION` in the Dockerfile (and `MBEDTLS_VERSION_STRING` in the
  container's `include/mbedtls/build_info.h`). A firmware built outside the container gets
  whatever `MBEDTLS_DIR` points to; `build-and-test.md` asks for 3.4.0.
- **Its state** (F-020): 3.4 is no long term support branch and has had no fixes since 3.5.0
  (2023). 3.0.0 ships with it, in the RT1060 EVK's builds and the host tests; the boards with a
  CAAM don't build it. It is to be replaced (Jonas, 2026-11-16), not moved to 3.6: what replaces
  it brings the hashes the RT1060 doesn't register (F-286 (10)). The release notes say so.
- **Licence**: Apache-2.0, the `LICENSE` of 3.4.0; check it again on an update.
- **Local changes**: none to Mbed TLS. Its configuration is pb's
  `src/drivers/crypto/mbedtls/mbedtls_config.h`, and the glue `mbedtls_pb.c` (`crypto.md`).
- **Updating**: the two arguments in the Dockerfile; the image tag follows (`build-and-test.md`).

## Kconfiglib

- **Files**: `scripts/kconfiglib.py`, and its tools `scripts/genconfig.py`,
  `scripts/menuconfig.py`, `scripts/olddefconfig.py`.
- **Upstream**: github.com/ulfalizer/Kconfiglib.
- **Version**: 14.1.0, `VERSION = (14, 1, 0)` in `kconfiglib.py`. Imported 2020-03-26
  (`e21a6592`); `olddefconfig.py` on 2026-07-16 (`2348c4eb`). Checked 2026-11-15: 14.1.0 is
  upstream's latest (PyPI, and the `v14.1.0` tag is the repository's last commit), and the four
  files' syntax trees are upstream's.
- **Local changes**: the tree-wide formatting of 2023-11-28 (`78756e3b`, black: quotes and line
  breaks) and the executable bit of `genconfig.py` and `menuconfig.py` (`e81c80b6`) are the only
  changes to the first three since the import. ruff leaves `scripts/` out,
  mypy `menuconfig.py` and `olddefconfig.py` (`pyproject.toml`).

## libfdt

- **Files**: `src/lib/fdt/{fdt.c,fdt_addresses.c,fdt_ro.c,fdt_rw.c,fdt_sw.c,fdt_wip.c}`,
  `libfdt_internal.h`, `version.lds`, `LICENSE`; `include/fdt/{fdt.h,libfdt.h,libfdt_env.h}`.
- **Upstream**: the `libfdt/` folder of dtc, git.kernel.org/pub/scm/utils/dtc/dtc.git.
- **Version**: inferred: 1.4.1, or a snapshot after it and before 1.4.2. No file records it.
  The errors end at `FDT_ERR_BADNCELLS` (`FDT_ERR_MAX` is 14, before `FDT_ERR_BADVALUE`), there
  is no overlay code and no `fdt_stringlist_count()`, and `fdt_resize()`, `fdt_address_cells()`
  and `fdt_getprop_namelen()` are there. Imported on 2019-01-03 as `src/3pp/fdt` (`03a97c91`).
- **Licence**: dual, GPL-2.0+ or BSD-2-Clause; pb takes the BSD terms (`src/lib/fdt/LICENSE`).
- **Local changes**: `fdt_empty_tree.c` and `fdt_strerror.c` were dropped (`version.lds` still
  lists their symbols). Casts against sign-compare warnings and `default: break;` in the
  switches of `fdt.c`, `fdt_ro.c`, `fdt_rw.c` and `fdt_sw.c`; `#include <libfdt_env.h>` for the
  quoted form; `fdt.h` reindented (`78756e3b`). A
  `.clang-format` in each folder turns formatting off.

## libuuid

- **Files**: `src/lib/uuid/{clear,compare,copy,isnull,pack,parse,unpack,unparse}.c`, `uuidP.h`,
  `uuidd.h`, `uuid.sym`, `include/uuid.h`.
- **Upstream**: libuuid of util-linux (Theodore Ts'o's, from e2fsprogs).
- **Version**: inferred: util-linux 2.16 to 2.19. `uuid.sym` has the symbol versions that came
  with the move to util-linux (2.16), and only `UUID_1.0`: 2.20 added `UUID_2.20`. It came
  through bpak's copy of libuuid on 2020-04-05 (`60615325`).
- **Local changes**: only the parsing, printing and comparing files are there, no generator.
  pb's own: `conv.c` (`uuid_to_guid()`) and `uuid3.c` (`uuid_gen_uuid3()`, fixed in
  `2d047ead`), declared near the end of `include/uuid.h`. Not formatted (`.clang-format`).

## TF-A translation tables

- **Files**: `src/lib/xlat_tables_v2/{xlat_tables_common.c,xlat_tables_private.h}`,
  `aarch32/xlat_tables.c`, `aarch64/xlat_tables.c`; `include/xlat_tables/`.
- **Upstream**: Trusted Firmware-A, git.trustedfirmware.org/TF-A/trusted-firmware-a.git.
- **Version**: inferred: about v2.3 (April 2020). The newest copyright year is 2020, in
  `xlat_tables_v2.h` and `xlat_tables_v2_helpers.h`; imported on 2020-06-22 (`ab6b8beb`).
- **Local changes**: despite the folder's name the sources are TF-A's v1 library
  (`lib/xlat_tables/`: `mmap_add()`, no `xlat_ctx_t`); the headers are both libraries'. Not
  formatted (`.clang-format`). Which lines differ from upstream is not known.

## TF-A libc

- **Files**: `src/lib/libc/{memchr,memcmp,memmove,memset,printf,snprintf,strchr,strcmp,strlen,
  strtoul}.c` and `include/libc/` but `unistd.h` and `assert.h`.
- **Upstream**: TF-A's `lib/libc` and `include/lib/libc`, which take some files from FreeBSD.
- **Version**: inferred: TF-A v2.0 (October 2018). The copyright years end in 2018; imported on
  2019-02-03 (`77302cdb`, `d7eb9189`). `inttypes.h` and `aarch32/inttypes_.h`,
  `aarch64/inttypes_.h` are from a later TF-A, v2.3 or after (Broadcom's copyright of 2020),
  imported on 2023-03-19 (`a3e8018e`).
- **Licence**: BSD-3-Clause; `endian.h` BSD-2-Clause (FreeBSD).
- **Local changes**: formatted with pb's clang-format (`78756e3b`). pb's own: `string.c`,
  `assert.c`, `putchar.c` (to the console), `strcpy.c`, `strcspn.c`, `strspn.c`,
  `include/libc/unistd.h` (for bpak), and the empty `assert.h`.

## TF-A export headers

- **Files**: `src/boot/atf-interface/` (TF-A's `include/export/`), with its `README`.
- **Version**: TF-A commit `1051606c3df3b5a0ebd4e4dad1e5e4a57e2f4d69`, the last change to the
  export headers in this copy (`commit.txt`). Imported on 2026-08-26 (`e5461b77`).
- **Local changes**: none known. clang-format leaves the folder out (`.pre-commit-config.yaml`).

## NXP SCFW client API

- **Files**: `src/plat/imx8x/sci/` (`ipc.c`, `svc/*/` the RPC clients) and
  `include/plat/imx8x/sci/` (`ipc.h`, `rpc.h`, `scfw.h`, `types.h`, `svc/*/api.h`). The pad
  definitions `include/plat/imx8x/imx8dxl_pads.h` and `imx8qxp_pads.h` came with them.
- **Upstream**: NXP's System Controller Firmware client, as NXP ships it with the SCFW porting
  kit and in its TF-A; which drop is not known.
- **Version**: API 1.31, `SCFW_API_VERSION_MAJOR` and `_MINOR` in `rpc.h`. Updated on 2025-09-25
  (`ba72c499`).
- **Local changes**: none known. pre-commit leaves both folders out.
  `src/plat/imx8x/sci/imx8_mu.c` and `imx8_mu.h` (the message unit) are TF-A's
  `plat/imx/common`, BSD-3-Clause.

## Single files from elsewhere

Files that carry code of another project, with their notice, outside the folders above. None
records a version.

| File                                                    | From                                 | Licence             |
| ------------------------------------------------------- | ------------------------------------ | ------------------- |
| `src/arch/armv7a/` and `src/arch/armv8a/`: `arch.h`,    | TF-A                                 | BSD-3-Clause        |
| `arch_helpers.h`, `misc_helpers.S`, `asm_macros.S`,     |                                      |                     |
| `asm_macros_common.S`; armv8a `arch_features.h`; armv7a |                                      |                     |
| `arm32_aeabi_divmod.c`, `arm32_aeabi_divmod_a32.S`      |                                      |                     |
| `include/pb/mmio.h`, `include/pb/assert.h`, part of     | TF-A                                 | BSD-3-Clause        |
| `include/pb/utils_def.h`                                |                                      |                     |
| `src/drivers/mmc/mmc_core.c`, `mmc_emmc.c`              | based on TF-A's MMC driver           | BSD-3-Clause        |
| `src/arch/armv7a/uldivmod.S`                            | Google, 2010                         | BSD-3-Clause or     |
|                                                         |                                      | GPL-2.0, BSD used   |
| `src/arch/armv7a/arm_asm.h`                             | ARM Ltd, 2009                        | BSD-3-Clause        |
| `src/arch/armv7a/include/arch/mmu.h`                    | Little Kernel (Travis Geiselbrecht), | MIT                 |
|                                                         | NVIDIA                               |                     |
| `src/drivers/virtio/virtio_mmio.h`, `virtio_queue.h`    | ARM Ltd (after IBM's virtio PCI);    | BSD; the OASIS      |
|                                                         | the OASIS virtio specification       | notice              |
| `src/lib/crc.c`                                         | Gary S. Brown's CRC-32 (1986), with  | Brown's: use        |
|                                                         | zlib's table                         | without restriction |
| `src/drivers/usb/imx_salvo_phy.c`, its register values  | U-Boot (F-205)                       | GPL-2.0+ upstream   |

pb builds with `-Wundef` (F-231), and the TF-A code tests its build options with `#if`, which
TF-A's build always defines. pb's makefiles define them too, with the value an undefined one had,
without changing the files: `ARM_ARCH_MAJOR` and the `ERRATA_*` (no erratum's workaround is built
in) in the arch's `makefile.mk`, `DEBUG` and `ERROR_DEPRECATED` there for assembly only
(`asflags-y`), `RECLAIM_INIT_CODE` in `src/lib/makefile.mk`. Two objects are compiled without
`-Wundef` instead: `xlat_tables_common.o` (its own `MMU_LOG_LEVEL` and `ENABLE_ASSERTIONS`) and
`uldivmod.o` (coreboot's `__COREBOOT_ARM_ARCH__`, which defined would change the code).
