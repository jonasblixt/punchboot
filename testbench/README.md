# punchboot Linux test bench

A Buildroot `BR2_EXTERNAL` tree, built out of tree, for the boards that run
Linux: punchboot from this tree, Linux, kickstart as the initramfs and a
small read only root file system that kickstart verifies and mounts with
dm-verity.

| Board | Defconfig | Status |
| ----- | --------- | ------ |
| i.MX8M Plus EVK | `configs/imx8mpevk_defconfig` | builds; punchboot can't start TF-A yet (below) |
| jiffy (i.MX6UL) | `configs/jiffy_defconfig` | boots: kickstart, dm-verity root, login (2026-10-29) |

| Component | Version |
| --------- | ------- |
| Buildroot | 2026.02.3 (LTS) |
| Toolchain | Buildroot internal: GCC 14, musl (kickstart is linked statically, see its `doc/licensing.md`) |
| Linux     | 6.18.58 (longterm) |
| TF-A      | v2.12 (imx8mpevk, BL31 only) |
| punchboot | this tree (`..`) |
| kickstart | branch `master` of `../../kickstart` |
| bpak      | branch `work-1.0.0` of `../../bpak`: libbpak for kickstart, the `bpak` python package as the host tool |
| NXP CST   | 3.4.1 (host, Debian source, jiffy's HAB signature) |
| firmware-imx | 8.27, the LPDDR4 training firmware FW2020.06 (imx8mpevk) |

No OP-TEE.

## Build

Needs the usual [Buildroot host packages](https://buildroot.org/downloads/manual/manual.html#requirement-mandatory)
(on Debian also `unzip rsync bc`).

```sh
cd testbench
make                     # imx8mpevk, in output/imx8mpevk
make BOARD=jiffy         # in output/jiffy
```

`make` downloads Buildroot into `buildroot-2026.02.3/` (checks the sha256),
configures `output/<board>` with `configs/<board>_defconfig` and builds.
Downloads go to `dl/`, shared by the boards. Other targets go to Buildroot:

```sh
make BOARD=jiffy menuconfig
make BOARD=jiffy savedefconfig      # back to configs/jiffy_defconfig
make BOARD=jiffy linux-menuconfig
make BOARD=jiffy punchboot-rebuild all   # after changing punchboot
```

punchboot and pbstate are local packages: Buildroot copies this tree
(without `build-*/` and `testbench/`), and `<pkg>-rebuild` copies it again,
uncommitted changes included.

bpak and kickstart are built from a branch of their git repositories,
`PB_TESTBENCH_{BPAK,KICKSTART}_{REPO,BRANCH}` in `external.mk`. The version
is the commit at the head of the branch, looked up with `git ls-remote` each
time make runs, so a new commit is downloaded and built as a new version.
Only commits count, not the working tree. Another branch or repository:

```sh
make BOARD=jiffy PB_TESTBENCH_BPAK_BRANCH=master
make BOARD=jiffy PB_TESTBENCH_KICKSTART_REPO=https://github.com/jonasblixt/kickstart.git
```

They have no `.hash` file (a branch head has no fixed hash), Buildroot warns
about that and checks every other download (`BR2_DOWNLOAD_FORCE_CHECK_HASHES`).
After a new commit of bpak, rebuild what used the host tool:
`make BOARD=jiffy punchboot-dirclean kickstart-dirclean all`.

Building imx8mpevk extracts firmware-imx and accepts NXP's EULA.

## Output (`output/<board>/images/`)

| File | |
| ---- | - |
| `punchboot/` | punchboot: `pb.imx` (imx8mpevk), `pb_signed.imx` and `pb_signed_uuu.imx` (jiffy, HAB signed with the test keys) |
| `boot.bpak` | the boot image, for System A/B |
| `root.bpak` | the root image, for Root A/B |
| `kickstart.cpio.gz` | the initramfs: kickstart as `/init` |
| `rootfs.squashfs` | the root file system in `root.bpak` |
| `zImage`/`Image`, `*.dtb`, `bl31.bin` | what `boot.bpak` is made of |

## Boot flow

```
ROM (HAB) -> punchboot -> [TF-A BL31] -> Linux -> kickstart (/init) -> /sbin/init of root.bpak
```

`board/common/post-image.sh` builds both images.

`boot.bpak`, verified by punchboot:

| Part | Content | jiffy | imx8mpevk |
| ---- | ------- | ----- | --------- |
| `atf` | `bl31.bin` | | 0x970000 (OCRAM) |
| `kernel` | Linux | `zImage`, 0x82000000 | `Image`, 0x42000000 |
| `dt` | device tree | 0x83f00000 | 0x50000000 |
| `ramdisk` | `kickstart.cpio.gz` | 0x84000000 | 0x50100000 |

The addresses keep clear of what punchboot itself uses while it loads
(`src/plat/*/link.lds`). The part names are the bpak ids that the boards'
boot drivers look for (`src/board/<board>/board.c`).

`root.bpak` is kickstart's root image (kickstart's README, "Root images"):
`rootfs.squashfs` and its dm-verity hash tree, the root hash in the signed
header. kickstart is built for the board's eMMC (`BR2_PACKAGE_KICKSTART_ROOT_A`
etc. in the defconfigs): Root A/B are partitions 3 and 4 and the boot state
5 and 6 in both boards' GPT, on `mmcblk0` (jiffy) and `mmcblk2` (8MP EVK,
uSDHC3). It reads the boot state, verifies the root image of the system
punchboot booted (`/chosen/pb,active-system`) and switches to it.

The root file system is read only. `/tmp` and `/run` are tmpfs, nothing
else is written. `/etc/init.d/S99pbstate` marks the booted system verified
with `pbstate-pb` (pbstate on the boot state partitions, found by their
UUIDs); remove it to test rollback.

### imx8mpevk: TF-A

punchboot's `imx8mpevk` board boots with the Linux boot driver, which jumps
to `kernel` at EL3; arm64 Linux needs BL31 for PSCI. Booting this image needs
the board to use the ATF boot driver (`src/boot/atf.c`, `CONFIG_BOOT_ATF`,
as `nanopi-neo-core2` does, F-146). TF-A v2.12's i.MX8MP BL31 takes BL33
(the kernel, x0 = DTB) from the `bl_params` list that `atf.c` passes in x0,
but only from OCRAM or TCM, and it swaps base and size in that check
(`imx8mp_bl31_setup.c`: `imx_bl31_params_parse(arg0, IMX_NS_OCRAM_SIZE,
IMX_NS_OCRAM_BASE, ...)`), so the list must be below 0x960000. Otherwise
BL31 jumps to 0x40200000 (`PLAT_NS_IMAGE_OFFSET`) without arguments.

## Running it

With the `punchboot` host tool (`pip install punchboot`) and the board in
punchboot's command mode on USB. jiffy needs an authenticated session for
most commands (`punchboot auth token <uuid>.token pb-development`).

```sh
# Once: the GPT
punchboot part install 1aad85a9-75cd-426d-8dc4-e9bdfeeb6875    # eMMC user area

# Root A, then System A from RAM: nothing else is written
punchboot part write output/<board>/images/root.bpak c284387a-3377-4c0f-b5db-1bcbcff1ba1a
punchboot boot bpak output/<board>/images/boot.bpak

# Or install System A
punchboot part write output/<board>/images/boot.bpak 2af755d8-8de5-45d5-a862-014cfa735ce0
punchboot boot enable 2af755d8-8de5-45d5-a862-014cfa735ce0
punchboot dev reset
```

Booted from RAM there is no `pb,active-system`, kickstart then takes the
enabled system from the boot state. `part write` stores a bpak with its header
in the last 4 KiB of the partition, kickstart reads that since `9dcc7b5`.

jiffy's USB must go straight to the host: on a hub (seen with a VIA VL813)
jiffy power-on-resets a few seconds into Linux (F-151).


Console: UART2 (`ttymxc1`), 115200 8N1, root login without password.

## Keys

Everything is signed with punchboot's **test** keys (`pki/`):

- `boot.bpak`: `pki/secp256r1-key-pair.pem` as `pb-development`, which the
  test keystore built into punchboot trusts. `BOOT_SIGN_KEY` selects another
  key.
- `root.bpak`: the same key, keystore id `kickstart`. kickstart's keystore
  (`BR2_PACKAGE_KICKSTART_KEYSTORE`, empty) is generated with
  `pki/secp256r1-pub-key.pem` as `pb-development`, so that the key is also in
  `/chosen/pb,slc-available-keys` (`KICKSTART_REQUIRE_PB_KEYS`).
  `ROOT_SIGN_KEY` selects another key.
- jiffy's HAB signature: `CONFIG_IMX6UL_*` in `configs/jiffy_defconfig`.

## Layout

```
Makefile                 fetches Buildroot, builds output/<board>
configs/                 <board>_defconfig
board/common/
  linux.fragment         kickstart's kernel options (initramfs, dm-verity, squashfs)
  patches/               BR2_GLOBAL_PATCH_DIR: kernel hashes
  post-image.sh          builds and signs root.bpak and boot.bpak
  rootfs-overlay/        S99pbstate, pbstate-pb
board/imx8mpevk/
  linux.fragment         on top of the arm64 defconfig: only the i.MX SoCs
board/jiffy/
  dts/                   imx6ul-jiffy.dts (from jiffy's test bench)
  linux.fragment         on top of imx_v6_v7_defconfig
  patches/linux/         FAN5355 regulator support
boot/punchboot/          punchboot from this tree (BR2_TARGET_PUNCHBOOT)
package/
  kickstart/             kickstart, the initramfs (images/kickstart.cpio.gz)
  pbstate/               pbstate from tools/pbstate
  bpak/                  libbpak (target), the bpak python package and tool (host)
  bpak-click/            host click for the bpak tool
  bpak-mbedtls/          host Mbed TLS for bpak
  python-scikit-build-core/  host build backend of the bpak python package
  imx-cst/               host NXP Code Signing Tool
```
