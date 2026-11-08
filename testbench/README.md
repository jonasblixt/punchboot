# punchboot Linux test bench

A Buildroot `BR2_EXTERNAL` tree, built out of tree, for the boards that run
Linux: punchboot from this tree, Linux, kickstart as the initramfs and a
small read only root file system that kickstart verifies and mounts with
dm-verity.

| Board | Defconfig | Status |
| ----- | --------- | ------ |
| i.MX8M Plus EVK | `configs/imx8mpevk_defconfig` | boots through TF-A: kickstart, dm-verity root, login (2026-10-30) |
| jiffy (i.MX6UL) | `configs/jiffy_defconfig` | boots: kickstart, dm-verity root, login (2026-10-29) |

| Component | Version |
| --------- | ------- |
| Buildroot | 2026.02.3 (LTS) |
| Toolchain | Buildroot internal: GCC 14, musl (kickstart is linked statically, see its `doc/licensing.md`) |
| Linux     | 6.18.58 (longterm) |
| TF-A      | v2.12 (imx8mpevk, imx8qxmek with `board/imx8qxmek/patches`, BL31 only) |
| punchboot | this tree (`..`) |
| kickstart | branch `master` of `../../kickstart` |
| bpak      | branch `master` of `../../bpak`: libbpak for kickstart, the `bpak` python package as the host tool |
| NXP CST   | 3.4.1 (host, Debian source, jiffy's HAB and the MEK's AHAB signature) |
| firmware-imx | 8.27, the LPDDR4 training firmware FW2020.06 (imx8mpevk) |
| imx-sc-firmware, imx-seco | 1.18.0, 5.9.4.1: the SCU firmware and the SECO firmware (B0) |
| | in the MEK's boot container |

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

Building imx8mpevk extracts firmware-imx and accepts NXP's EULA, imx8qxmek the same for
imx-sc-firmware and imx-seco.

## Output (`output/<board>/images/`)

| File | |
| ---- | - |
| `punchboot/` | punchboot: `pb.imx` (imx8mpevk), `pb_signed.imx` and `pb_signed_uuu.imx` (jiffy, HAB |
| | signed with the test keys), `pb.imx` and `pb_signed.imx` (imx8qxmek: the boot container with |
| | the SECO and SCU firmware, AHAB signed) |
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

punchboot's `imx8mpevk` board boots with the ATF boot driver
(`src/boot/atf.c`): it loads BL31 to 0x970000 and starts it with a
`bl_params` list in x0 that has Linux as BL33 (EL2, x0 = the device tree).
Three things it takes on this SoC:

- TF-A v2.12's i.MX8MP BL31 only takes the list from OCRAM or TCM, and it
  swaps base and size in that check (`imx8mp_bl31_setup.c`:
  `imx_bl31_params_parse(arg0, IMX_NS_OCRAM_SIZE, IMX_NS_OCRAM_BASE, ...)`),
  so the list must be below 0x960000; otherwise BL31 jumps to 0x40200000
  (`PLAT_NS_IMAGE_OFFSET`) without arguments. pb keeps it in `.data`.
- The system counter must run (pb starts it, `src/plat/imx8m/plat.c`):
  BL31's `udelay()` and Linux's arch timer count it.
- Linux 6.18 powers the audio mix itself, which leaves its AIPS5 bridge at
  the reset defaults (TF-A only sets it up when it powers the domain), and the
  first access takes an SError. `board/imx8mpevk/dts/freescale/imx8mp-evk-pb.dts`
  is the EVK's device tree without AIPS5 (audio only). Newer kernels set the
  bridge up (`drivers/bus/imx-aipstz.c` with a node in `imx8mp.dtsi`).

The FEC port's PHY (`30be0000.ethernet`, eth0) doesn't answer on MDIO; the
EQOS port (eth1) probes.

### Linux's kernel log in pb (ramoops)

Both boards reserve 1 MiB at 0xbff00000 for pstore/ramoops: eight 128 KiB
dmesg records, no console zone (the `ramoops` node in the board's DTS,
`PSTORE_RAM` without compression in `board/common/linux.fragment`), with the
same values as `CONFIG_BOOT_LINUX_RAMOOPS_*` in punchboot's
`configs/<board>_defconfig`. Linux saves its whole kernel log (every level,
from `Booting Linux`, up to 128 KiB) in a record at a panic, an oops, an
emergency restart and a reboot (`max-reason = <4>`). After a reset into
command mode `punchboot log linux` prints the newest record, starting with
its `====<sec>.<usec>-D` header (the time Linux saved it, from the RTC) and
the reason (`Panic#1 Part1`, `Shutdown#1 Part1`, `Emergency#2 Part1`).
Records stay until Linux erases them (`rm` in `/sys/fs/pstore` after `mount
-t pstore pstore /sys/fs/pstore`, where Linux shows them as
`dmesg-ramoops-N`), so after a reset that saved nothing (power, a hard hang)
the newest record is from an earlier boot: check the time.

A hang: `pb-watchdog` (`package/pb-watchdog`, instead of BusyBox's daemon)
keeps the watchdog alive every 5 s and sets a 2 s pretimeout, and the
kernel's panic pretimeout governor panics Linux 2 s before the watchdog
resets the board. A panic with `panic=5` saves two records, at the panic and
at the emergency restart 5 s later; `linux` shows the second, which has the
panic too.

| Reset | jiffy | imx8mpevk |
| ----- | ----- | --------- |
| `reboot` | `Shutdown#1` | lost (F-156) |
| panic, `panic=5` restart | `Emergency#2` | lost (F-156) |
| hang (`kill -STOP` pb-watchdog), the watchdog | `Panic#1`, pretimeout, 60 s | `Panic#1`, pretimeout, 58 s |

On the EVK, Linux's PCA9450 driver restarts through the PMIC with a cold
reset, which power cycles the DRAM (F-156). A watchdog timeout asserts
WDOG_B, which `imx8mp-evk-pb.dts` makes a warm reset of the PMIC
(`nxp,wdog_b-warm-reset`), so the pretimeout panic's record is kept: the
watchdog resets the board 2 s after it, before Linux's own restart (5 s).
ramoops' probe costs at most 5.6 ms on jiffy and 0.8 ms on the EVK, with an
old record to save (kernel timestamps, measured with the console zone).

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

The 8MP EVK can be reset from the host, through the FT4232H on its debug USB
(`pip install pyftdi`): `scripts/imx8mpevk-reset`. A warm reset, the boot
switches still decide what it boots.


Console: UART2 (`ttymxc1`), 115200 8N1, root login without password.

## Boot time

### Measuring

`scripts/boottime` works over the board's serial console (nothing else may
have it open):

```sh
make BOARD=jiffy PB_BOOT_TIME_MARK=1      # init prints "initmark <uptime>" first
# write System A and Root A, then
punchboot boot enable 2af755d8-8de5-45d5-a862-014cfa735ce0
punchboot dev reset

scripts/boottime measure /dev/ttyUSB0 -n 5      # reset -> pb -> init -> login, medians
scripts/boottime dmesg /dev/ttyUSB0 dmesg.txt
scripts/boottime initcalls dmesg.txt            # with initcall_debug, below
scripts/boottime sizes output/jiffy/build/linux-6.18.58
scripts/boottime console /dev/ttyUSB0 'uname -a' 'ip addr'
scripts/boottime release /dev/ttyUSB0           # no active system, back to command mode
make BOARD=jiffy                                # without the mark again
```

`measure` reboots from the Linux shell: from the kernel's `reboot:
Restarting system` (the watchdog reset follows) to pb's banner, to the
`initmark` line and to the login prompt. The host's timestamps carry up to
16 ms from the FTDI latency timer; the kernel's uptime at init is the
board's own. `release` is the way back to command mode once a system boots
by itself: `pbstate-pb -s none` and a reboot (jiffy can't force command
mode). pb's own times (init, load, verify) come from
`../scripts/boot-time-image -c <console>`, which keeps pb in command mode.

`initcall_debug`: pb writes `bootargs`, so it goes into the kernel, with
`printk.devkmsg=on` for kickstart's lines (temporarily, in the board's
`linux.fragment`):

```
# CONFIG_CMDLINE_FROM_BOOTLOADER is not set
CONFIG_CMDLINE_EXTEND=y
CONFIG_CMDLINE="initcall_debug printk.devkmsg=on"
```

### jiffy

Warm resets (`reboot` in Linux, the watchdog), System A enabled, timed on the
console from the host (FTDI latency timer 16 ms) to a temporary first
`inittab` line that prints `/proc/uptime`, medians of 5 runs (2026-10-29):

| Kernel | zImage | Reset to BusyBox init | Kernel uptime at init |
| ------ | ------ | --------------------- | --------------------- |
| `imx_v6_v7_defconfig` | 10.2 MB | 2939 ms | 1520 ms |
| only jiffy's hardware (`board/jiffy/linux.fragment`) | 4.6 MB | 2108 ms | 1220 ms |
| and no jitter entropy RNG, RAM disks or preallocated loop devices | 4.6 MB | 1629 ms | 740 ms |
| and LZ4 instead of gzip (`BR2_LINUX_KERNEL_LZ4`) | 5.4 MB | **1326 ms** | 730 ms |

From the reset (the slim LZ4 kernel; from the console timing, pb's RAM log
and `initcall_debug`):

| Step | ms |
| ---- | -- |
| Boot ROM with HAB, to pb's banner (128 when reset from pb) | 144 |
| pb init: clocks, DDR, CAAM, eMMC, GPT | 27 |
| pb loads and verifies `boot.bpak` (77 MiB/s read, SHA-256 overlapped) | ~80 |
| zImage decompressing itself (LZ4; gzip was ~650), the kernel before its clock | ~350 |
| kernel to `Run /init` | ~650 |
| kickstart: boot state 18, signature 54, dm-verity and mount 41, exec init | ~120 |

`initcall_debug` (it needs the command line in the kernel, pb sets
`bootargs`: `CONFIG_CMDLINE_EXTEND` with `initcall_debug`) found the jitter
entropy RNG at 464 ms, more than half of all initcalls. Of the rest: the FEC
41 ms (10 + 10 ms PHY reset from the DTS), the CAAM 45 ms, the four UARTs
21 ms, GPIO 20 ms. To the login prompt it is another ~6 s, `udhcpc` (~3 s
without a lease) and `S99pbstate` (~2.7 s).

### imx8mpevk

Warm resets as for jiffy, medians of 5 (2026-10-30). Reset to pb's banner is
~420 ms here: arm64 Linux resets through TF-A's PSCI, not a watchdog right
away; pb's own boot ROM time is 25 ms (README Metrics).

| Kernel | Image | Reset to BusyBox init | Kernel uptime at init |
| ------ | ----- | --------------------- | --------------------- |
| arm64 `defconfig`, other SoC families off | 25.9 MB | 1939 ms | 880 ms |
| only the EVK's hardware, no jitter RNG (`board/imx8mpevk/linux.fragment`) | 14.6 MB | 1323 ms | 400 ms |
| and unused devices off in the DTS (`imx8mp-evk-pb.dts`) | 14.6 MB | 1243 ms | 320 ms |
| and the CAAM, thermal, RTC and cpufreq built in | 14.7 MB | **1218 ms** | 290 ms |

The `Image` isn't compressed on arm64: pb reads it at ~89 MiB/s, ~165 ms for
14.7 MB. `initcall_debug`: all initcalls 241 ms; the SD slot (uSDHC2) took
32 ms to probe empty and the display nodes' dependency cycles 13 ms, both off
in the DTS now; the FEC 24 ms (its PHY doesn't answer). On arm64 the jitter
RNG comes with `CRYPTO_ECHAINIV`, which the defconfig sets.

pb now instantiates the CAAM's RNG (Linux runs non-secure here and can't), which
adds 27 ms to every boot after these measurements; Linux has `rng-caam` and its
random pool is ready at 0.15 s. Open: the FEC's PHY doesn't answer on MDIO.

## Adding a board

Every board that runs Linux gets a defconfig here and goes through both
lists, bring-up and boot time, in that order. Record what each step measured
in the board's section under "Boot time" and commit per step.

### Bring-up

1. pb: the board must boot Linux the right way: the Linux boot driver on
   armv7, the ATF boot driver (`src/boot/atf.c`) with TF-A BL31 on aarch64.
   Check where the TF-A platform takes BL33 from (see "imx8mpevk: TF-A").
2. `configs/<board>_defconfig`: copy the closest one. The SoC's kernel
   defconfig, `board/common/linux.fragment` and a `board/<board>/linux.fragment`,
   the DTB (in tree, or a DTS in `board/<board>/dts`; it needs nothing for pb,
   which adds `/chosen` and its properties), TF-A, firmware the
   board needs (DDR training), `BR2_TARGET_PUNCHBOOT_BOARD`, the console in
   `BR2_TARGET_GENERIC_GETTY_PORT`, `BR2_ROOTFS_POST_SCRIPT_ARGS="<board>"`.
3. kickstart's devices: the eMMC's `mmcblkN` from the `mmc` aliases of the
   board's DTS (`aliases` in the SoC's `.dtsi`), Root A/B and the boot state
   from the board's GPT in `src/board/<board>/board.c`.
4. `board/common/post-image.sh`: a case for the board with load addresses
   clear of pb (`src/plat/*/link.lds`, pb's RAM and DRAM buffers), clear of
   each other (the kernel's size after decompression, 2 MiB aligned on
   arm64) and, for BL31, of TF-A's `BL31_BASE`. The partition sizes are
   checked there.
5. Build, write System A and Root A (`part write`, `part verify`) and boot
   it from command mode with the console captured (`punchboot boot partition
   --verbose-boot` gives the kernel's log). Check: the login prompt, `/` on
   `/dev/mapper/kickstart-root`, `/proc/device-tree/chosen/pb,active-system`,
   `pbstate-pb -i` (S99pbstate marks it verified), eth0, the watchdog
   daemon, `dmesg` without errors.
6. The board's row in the table at the top, its parts in "Boot flow".

Known traps: a USB hub between the host and the board's command mode port
can reset the board once Linux runs (F-151, jiffy); `part write` stores a
bpak's header at the end of the partition; jiffy needs `punchboot auth token`
after every reset into command mode.

### Boot time

Measure with `scripts/boottime measure` (above) after every step, 5 runs,
and check the system again (step 5 of the bring-up) before going on.

1. Baseline: the SoC's defconfig as it is.
2. A kernel for the board's hardware only, in `board/<board>/linux.fragment`.
   List what the board's DTS enables (`&node { status = "okay" }` and the
   nodes on by default in the SoC's `.dtsi`) and what probes (`dmesg`), and
   see what takes room with `scripts/boottime sizes`. Ask which of these the
   board needs before turning them off: USB devices (HID, storage, serial),
   file systems, IPv6, devices on the UARTs, loadable modules. Then off:
   other SoCs (also the ARMv6/other architecture variants), display, sound,
   media, radios, CAN, PCI, ATA, SCSI, MTD, USB, HID, SPI, PWM, staging,
   other boards' PMICs, other Ethernet vendors, network file systems, FUSE,
   kprobes, BPF, io_uring, debugfs, the initramfs decompressors other than
   gzip. Check every line took (`grep` the kernel's `.config`): an option
   that something still selects stays on (`VT` and `IO_URING` need
   `CONFIG_EXPERT`). Build in (`=y`) every driver the board needs: the root
   file system has no mdev/udev, nothing loads modules (on the 8MP EVK the
   EQOS Ethernet, the CAAM, thermal, RTC and cpufreq were modules).
3. The board's DTS: turn off the devices the test bench doesn't use (an SD
   slot nobody boots from, display, GPU/NPU/VPU, PCIe, USB, CAN, sound), in
   a `<board>-pb.dts` that includes the upstream one. On the 8MP EVK an
   empty SD slot took 32 ms to probe, the display nodes' dependency cycles
   13 ms. Watch `dmesg` for deferred probes and probe timeouts afterwards
   (an interrupt controller whose power domain went with its block).
4. `initcall_debug` (above), `scripts/boottime initcalls`. Look for the
   jitter entropy RNG (`jent_mod_init`, 464 ms on jiffy): it comes with
   `CRYPTO_DRBG`, which `CRYPTO_ECDH`/`ECC`/`GENIV` pull in (on arm64
   `CRYPTO_ECHAINIV` from the defconfig); turn off what selects it. RAM disks (`BLK_DEV_RAM`) and preallocated loop devices
   (`BLK_DEV_LOOP_MIN_COUNT=0`) too. Look at deferred probes and long driver
   probes, but keep delays the hardware needs (PHY resets).
5. armv7: LZ4 for the zImage (`BR2_LINUX_KERNEL_LZ4`). arm64 boots an
   uncompressed `Image`, there its size is pb's read time (~11-13 ms per MB
   on the eMMCs here).
6. Take `initcall_debug` and `PB_BOOT_TIME_MARK` out again, write the clean
   images and leave the board as it was (`scripts/boottime release`).

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
  post-build.sh          the boot time mark in /etc/inittab (PB_BOOT_TIME_MARK=1)
  post-image.sh          builds and signs root.bpak and boot.bpak
  rootfs-overlay/        S99pbstate, pbstate-pb
board/imx8mpevk/
  dts/                   imx8mp-evk-pb.dts: the EVK's device tree without AIPS5 and unused devices
  linux.fragment         on top of the arm64 defconfig: only the i.MX SoCs
board/jiffy/
  dts/                   imx6ul-jiffy.dts (from jiffy's test bench)
  linux.fragment         on top of imx_v6_v7_defconfig
  patches/linux/         FAN5355 regulator support
boot/punchboot/          punchboot from this tree (BR2_TARGET_PUNCHBOOT)
package/
  kickstart/             kickstart, the initramfs (images/kickstart.cpio.gz)
  pb-watchdog/           the watchdog daemon with a pretimeout (S15watchdog)
  pbstate/               pbstate from tools/pbstate
  bpak/                  libbpak (target), the bpak python package and tool (host)
  bpak-click/            host click for the bpak tool
  bpak-mbedtls/          host Mbed TLS for bpak
  python-scikit-build-core/  host build backend of the bpak python package
  imx-cst/               host NXP Code Signing Tool
scripts/
  boottime               boot time over the console: measure, dmesg, initcalls, sizes
  imx8mpevk-reset        reset the 8MP EVK through its FT4232H
```
