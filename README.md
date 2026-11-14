![PB Logo](doc/pb_logo.png)

[![Coverity](https://scan.coverity.com/projects/16584/badge.svg)](https://scan.coverity.com/projects/jonpe960-punchboot)
[![Build Status](https://github.com/jonasblixt/punchboot/actions/workflows/build.yml/badge.svg)](https://github.com/jonasblixt/punchboot/actions/workflows/build.yml)
[![pre-commit.ci status](https://results.pre-commit.ci/badge/github/jonasblixt/punchboot/master.svg)](https://results.pre-commit.ci/latest/github/jonasblixt/punchboot/master)
[![Donate](https://img.shields.io/badge/paypal-donate-yellow.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=4XMK2G3TPN3BQ)

# Introduction

Punchboot is a secure and fast bootloader for embedded systems. It is designed to:

 - Boot as fast as possible
 - Integrate with the SoC's secure boot functionality
 - Authenticate the next piece of software in the boot chain
 - Support A/B system partitions for atomic updates
 - Support automatic rollbacks
 - Minimize software download time in production
 - Be useful for day-to-day development

Punchboot is designed for embedded systems and therefore it has a minimalistic
apporach. There is no run-time configuration, everything is configured in
the board files.

Punchboot could be useful if you care about the following:
 - Boot speed
 - Secure boot
 - Downloading software quickly in production

## Design

Punchboot is written in C and some assembler, for armv7a, armv8a and armv7m.

The directory layout is as follows:

| Folder       | Description             |
| ------------ | ----------------------- |
| /doc         | Documentation           |
| /pki         | Crypto keys for testing |
| /            | Bootloader source       |
| /src/board   | Board support           |
| /src/arch    | Architecture support    |
| /src/plat    | Platform support        |
| /src/drivers | Drivers                 |
| /tools       | Tools                   |
| /tests       | Host tests              |
| /testbench   | Test benches for boards |

Supported architectures:

| Architecture | Supported |
| ------------ | --------- |
| armv7a       | Yes       |
| armv8a       | Yes       |
| armv7m       | Yes       |

Supported platforms:

| Platform   | USB | Storage     | Crypto              | Secure Boot | Fuses      |
| ---------- | --- | ----------- | ------------------- | ----------- | ---------- |
| NXP imx6ul | Yes | eMMC        | CAAM                | Yes         | Read/write |
| NXP imx8m  | Yes | eMMC, SD    | CAAM                | No          | Read       |
| NXP imx8x  | Yes | eMMC        | CAAM                | Yes         | Read/write |
| NXP imxrt  | Yes | FlexSPI NOR | Mbed TLS (software) | No          | Read       |

Boards in the tree:

| Board               | Platform         | State                                      |
| ------------------- | ---------------- | ------------------------------------------ |
| Jiffy               | imx6ul           | Boots Linux                                |
| NXP i.MX8M Plus EVK | imx8m (i.MX8MP)  | Boots Linux through TF-A                   |
| NXP i.MX8QXP MEK    | imx8x (i.MX8QXP) | Boots Linux through TF-A                   |
| NXP i.MX RT1060 EVK | imxrt            | Boots a bare metal application from RAM    |
| NXP i.MX8MQ EVK     | imx8m (i.MX8MQ)  | Builds, not run on a board                 |
| NXP i.MX8DXL EVK    | imx8x (i.MX8DXL) | Builds, not run on a board                 |

Jiffy: https://github.com/jonasblixt/jiffy

Images are signed with ECDSA on P-256, P-384 or P-521, over a SHA-256, SHA-384 or SHA-512
hash. The CAAM computes MD5 and SHA-256 and verifies the three curves; it computes SHA-384 and
SHA-512 where its hash block has them, which the i.MX6UL's doesn't. A build with Mbed TLS has
the algorithms its configuration selects.

## Secure Boot

### Typical and simplified secure boot flow
 - ROM loads a set of public keys, calculates the checksum of the keys and compares the result to a fused checksum
 - ROM loads punchboot, calculates checksum and verifies signature using key's in step one
 - Run punchboot
 - Punchboot loads a bpak image, calculates the checksum and verifies the signature using built in keys
 - Run next step in boot chain

Most SoC:s have a boot rom that includes meachanisms for calculating a checksum
of the bootloader and cryptographically verifying a signature using a public key
fused to the device.

Normally fuses are a limited resource and therefor a common way is to calculate
a sha256 checksum of the public key(s) and then store this checksum in fuses,
this way many different public keys can be stored in a flash memory and every
time the device boots it will compute a sha256 checksum and compare it to the
fused checksum.

Punchboot is designed to be a part of a secure boot chain. This means that
the bootloader is cryptographically signed, the ROM code of the SoC must
support a mechanism to validate this signature, otherwise there is no
root of trust.

When punchboot has been verified it, in turn, will load and verify the next
software component in the boot chain. The bootloader _only_ supports signed
binaries.

## Testing and integration tests
Punchboot is tested on the host. The 'host' platform and board build punchboot
as a native Linux process, with a file backed disk and a unix domain socket
for the command mode transport.

- C unit tests (CMocka) in tests/unit
- Integration tests (pytest) in tests/integration, which drive the host build
  through the punchboot Python package, the same way the tools talk to a
  device over USB

Everything runs in a docker container with the required toolchains and tools:
```
$ scripts/pbdev test                    # build and run all tests
$ scripts/pbdev test -DPB_SANITIZE=ON   # with AddressSanitizer and UBSan
```

See doc/guides/testing.md for details.

## Device identity

Most modern SoC's provide some kind of unique identity, that is guaranteed to
be unique for that particular type of SoC / Vendor etc but can not be guarateed
to be globally unique.

Punchboot provides a UUID3 device identity based on a combination of the unique
data from the SoC and an allocated, random, namspace UUID per platform.

When booting a linux system this information is relayed to linux through
the device tree (below). The device identity can be found in
'/proc/device-tree/chosen/pb,device-uuid'.

## Booting Linux: the device tree

The image handover (Linux, Xen), delivered directly or through TF-A, adds to `/chosen` in the
image's device tree:

| Property                                 |                                                        |
| ---------------------------------------- | ------------------------------------------------------ |
| `pb,device-uuid`                         | the device identity                                    |
| `pb,active-system`                       | the system booted (A or B), if the board has A/B       |
| `pb,boot-partition`                      | the UUID of the partition booted                       |
| `pb,boot-mode`                           | how it was chosen: `normal`, `fallback`, `speculative` |
| `pb,slc`                                 | the security life cycle state                          |
| `pb,slc-active-key`                      | the id of the key the image is signed with             |
| `pb,slc-available-keys`                  | the ids of the keys that aren't revoked                |
| `pb,version`                             | the version of the pb that booted the system           |
| `pb,status`                              | what pb booted without, a list of strings; often empty |
| `linux,initrd-start`, `linux,initrd-end` | the ramdisk, if the image has one                      |
| `bootargs` and others                    | whatever the board's callback sets                     |

`pb,boot-partition`, `pb,boot-mode`, `pb,version` and `pb,status` are new in
3.0.0: look for the property, an older pb doesn't write it. `pb,status` lists
what pb couldn't do and booted anyway: `watchdog-not-armed`, and
`board-degraded` for a failure the board reported (pb's log and `board status`
say which). It is there and empty when there is nothing to report; ignore the
strings you don't know. `pb,boot-mode` is `fallback`
when the system pb tried first didn't load or verify in this boot and another
one was booted, and `normal` for an image the host named or sent (`boot part`,
`boot bpak`). `boot bpak` without a partition has no `pb,boot-partition`. The
board's callback runs first and pb writes its properties after it: where both
set one, pb's value is what the system reads.

The ramdisk's two addresses are one cell (32 bits) each. Only a ramdisk that
reaches 4 GiB or above gets two cells (64 bits) for both.

The device tree doesn't need any of them, nor `/chosen`: pb adds what is
missing. Before that it grows the device tree in place, into the RAM after
its part, by `CONFIG_BOOT_DTB_HEADROOM` (4 KiB by default). Keep that RAM free
in the image: the boot fails if another part or pb is there. For a device
tree in dtc's block order growing only changes its size in the header; each
property added moves the rest of the tree, as it always did. On jiffy (i.MX6UL,
a 30 KB device tree) that is too little to measure. A device tree built with
the properties and free space for them (`dtc -p`) works with a headroom of 0.

Such a device tree, built for an older pb, has no room for `pb,boot-mode`,
`pb,boot-partition`, `pb,version` and `pb,status`. With a headroom of 0 it
boots as before, and each of the four that doesn't fit is left out, with a
warning in pb's log. pb fits the mode first, then the partition, then the
version, then the status, each on its own, and leaves out the same ones
whatever the mode is and whatever it has to report. To get them, give the
device tree the four properties (`pb,boot-mode` with room for `speculative`,
12 bytes; `pb,boot-partition` 37; `pb,version` its string; `pb,status` with
room for both of its strings, 34) or 196 bytes of padding for them (70 for the
partition, 37 for the mode, 31 for a version like `3.0.0`, 58 for the status).
A property of the older ones that doesn't fit fails the boot, as it always
did.

## Command mode

Command mode is entered when the system can't boot or if the bootloader is
forced by a configurable, external event to do so.

In the command mode it is possible to update the bootloader, write data to
partitions and install default settings. When the device's security life cycle
is locked, the session must be authenticated first, with a signed token or a
password. Before that the device answers only its identity: the device UUID and
the board's name.

## punchboot tool
The punchboot CLI is used for interacting with the command mode. A summary of the features available:

- Update the bootloader it self (`part write`, `part verify`)
- Manually start system A or B (`boot partition`)
- Activate boot partitions (`boot enable`, `boot disable`, `boot status`)
- Load image to ram and execute it (`boot bpak`)
- Display basic device info (`dev show`, `list`)
- Configure fuses and GPT parition tables (`slc`, `part install`)
- List and run the commands of the device's board and platform (`command list`, `command <name>`)
- Read the device's logs (`log`, `log --list`)

`punchboot --help` and `punchboot <group> --help` have the commands and their options.

The tool and the library it is built on are pure Python, so that they can be
integrated with other tooling and environments. USB goes through pyusb and
libusb.

The tool is distributed through PyPi, as one wheel for Windows, Linux and macOS.

Install using pypi:
```
$ pip install punchboot
```

## Image format
Punchboot uses the bitpacker file format (https://github.com/jonasblixt/bpak)


## Authentication token

Punchboot enforces authentication when the SLC (Security Life Cycle) is locked.
To interact with Punchboot the session must be authenticated by using a password
or a signed token.

The authentication token is generated by hashing the device UUID and signing
it with one of the active key pairs.

Example, on a locked device (the bootloader's version is shown once the session
is authenticated):
```
$ punchboot dev show
Device UUID:        0b177094-6b62-3572-902e-c1de339ecb01
Board name:         jiffy
```

Creating the authentication token using the 'createtoken.sh' script located
in the scripts folder. The UUID is written as `dev show` prints it, and the hash
goes with the key: sha256 for P-256, sha384 for P-384, sha512 for P-521. In this
example the private key is stored on a yubikey 5 HSM.

```
$ ./createtoken.sh 0b177094-6b62-3572-902e-c1de339ecb01 pkcs11 sha256 "pkcs11:id=%02;type=private"
engine "pkcs11" set.
Enter PKCS#11 token PIN for PIV Card Holder pin (PIV_II):
Enter PKCS#11 key PIN for SIGN key:
```

Authenticating the session, which prints nothing when it succeeds:
```
$ punchboot auth token ./0b177094-6b62-3572-902e-c1de339ecb01.token <name of a key>
```
Now the command mode is fully unlocked. The token is of course only valid for
the individual unit with that perticular UUID.

## Metrics

Measured with `scripts/boot-time-image -c <console>`: warm resets timed from
the host and pb's RAM log, then a signed image loaded from eMMC in 1 MiB
chunks, the hash of one chunk running while the next is read. Medians of 5
runs, with a 10 MiB image.

| Parameter                                       | i.MX6UL (jiffy) | i.MX8M Plus EVK | Unit  |
| ----------------------------------------------- | --------------- | --------------- | ----- |
| Boot ROM: load pb from eMMC (HAB on jiffy only) | 128             | 25              | ms    |
| Bootloader init (clocks, DDR, CAAM, eMMC, GPT)  | 27              | 91              | ms    |
| Image header read, ECDSA P-256 signature        | 7               | 7               | ms    |
| 10 MiB eMMC read, SHA-256 overlapped            | 132             | 115             | ms    |
| SHA-256 of the last chunk                       | 9               | 8               | ms    |
| Total, reset to verified image                  | 302             | 247             | ms    |
| eMMC read                                       | 77              | 87              | MiB/s |
| SHA-256 (CAAM)                                  | 116             | 122             | MiB/s |

jiffy: 528 MHz, HAB closed, eMMC in DDR52 8-bit. The EVK: eMMC in HS200, not
HAB closed, most of its init is LPDDR4 training (64 ms). A 29 MiB image loads
and verifies in 399 ms on jiffy and 349 ms on the EVK.

## Documentation

- [CHANGELOG.md](CHANGELOG.md): the release notes. 3.0.0 is compatible with 2.x on the wire
  and in the image format; its major version is for the porting API.
- [doc/guides/porting.md](doc/guides/porting.md): writing a board, a driver or a platform.
- [doc/guides/migration-3.0.md](doc/guides/migration-3.0.md): porting a 2.x board to 3.0.0.
- [doc/guides/build-and-test.md](doc/guides/build-and-test.md): building the firmware.
- [doc/README.md](doc/README.md): the map of the documents.

## Contributing

The conventions, for people and coding agents alike, are in [AGENTS.md](AGENTS.md); the map
of the documents is [doc/README.md](doc/README.md).

1. Fork the repository
2. Implement new feature or bugfix on a branch
3. Implement test case(s) to ensure that future changes do not break legacy
4. Run checks: scripts/pbdev test && pre-commit run --all-files
5. Create pull request
