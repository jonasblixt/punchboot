![PB Logo](doc/pb_logo.png)

[![Coverity](https://scan.coverity.com/projects/16584/badge.svg)](https://scan.coverity.com/projects/jonpe960-punchboot)
[![Build Status](https://github.com/jonasblixt/punchboot/actions/workflows/build.yml/badge.svg)](https://github.com/jonasblixt/punchboot/actions/workflows/build.yml)
[![pre-commit.ci status](https://results.pre-commit.ci/badge/github/jonasblixt/punchboot/master.svg)](https://results.pre-commit.ci/latest/github/jonasblixt/punchboot/master)
[![codecov](https://codecov.io/gh/jonasblixt/punchboot/branch/master/graph/badge.svg)](https://codecov.io/gh/jonasblixt/punchboot)
[![Documentation Status](https://readthedocs.org/projects/punchboot/badge/?version=latest)](https://punchboot.readthedocs.io/en/latest/?badge=latest)
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

Punchboot is written in C and some assembler. Currently armv7a and armv8 is supported.

The directory layout is as follows:

| Folder       | Description             |
| ------------ | ----------------------- |
| /doc         | Documentation           |
| /pki         | Crypto keys for testing |
| /            | Bootloader source       |
| /src/board   | Board support           |
| /src/arch    | Architecture support    |
| /src/plat    | Platform support        |
| /src/drivers | Drivers                 |
| /tools       | Tools                   |

Supported architectures:

| Architecture | Supported               |
| ------------ | ----------------------- |
| armv7a       | Yes                     |
| armv8a       | Yes                     |
| armv7m       | Yes                     |

Supported platforms:

| Platform        | Supported            | USB | EMMC | HW Crypto | Secure Boot | Fusebox |
| --------------- | -------------------- | --- | ---- | --------- | ----------- | ------- |
| NXP imx6ul      | Yes                  | Yes | Yes  | Yes       | Yes         | Yes     |
| NXP imx8m       | Yes                  | Yes | Yes  | Yes       | No          | Yes     |
| NXP imx8x       | Yes                  | Yes | Yes  | Yes       | Yes         | Yes     |


Supported boards:

| Board                | Supported               | More info |
| -------------------- | ----------------------- | --------- |
| Jiffy                | Fully supported         | https://github.com/jonasblixt/jiffy       |
| Bebop                | Fully supported         | https://github.com/jonasblixt/bebop       |
| Technexion PICO-IMX8M| Partial support         | https://www.technexion.com/products/system-on-modules/pico/pico-compute-modules/detail/PICO-IMX8M |
| NXP IMX8QXP MEK      | Fully supported         | https://www.nxp.com/products/processors-and-microcontrollers/arm-based-processors-and-mcus/i.mx-applications-processors/i.mx-8-processors/i.mx-8-multisensory-enablement-kit:i.MX8-MEK |

Hardware accelerated signature verification

| Platform        | RSA4096 | EC secp256r1 | EC secp384r1 | EC secp521 |
| --------------- | ------- | ------------ | ------------ | ---------- |
| NXP imx6ul      | Yes     | Yes          | No           | No         |
| NXP imx8m       | Yes     | Yes          | No           | No         |
| nxp imx8x       | yes     | yes          | yes          | yes        |
| nxp imx RT      | no      | no           | no           | no         |

Hardware accelerated hash algorithms

| Platform        | MD5 | SHA256 | SHA384 | SHA512 |
| --------------- | --- | ------ | ------ | ------ |
| NXP imx6ul      | Yes | Yes    | No     | No     |
| NXP imx8m       | Yes | Yes    | No     | No     |
| NXP imx8x       | Yes | Yes    | Yes    | Yes    |
| NXP imx RT      | No  | No     | No     | No     |

## Secure Boot

### Typical and simplified secure boot flow
 - ROM loads a set of public keys, calculates the checksum of the keys and compares the result to a fused checksum
 - ROM loads punchboot, calculates checksum and verifies signature using key's in step one
 - Run punchboot
 - Punchboot loads a PBI bundle, calculates the checksum and verifies the signature using built in keys
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
Punchboot uses QEMU for all module and integration tests. The 'test' platform
and board target relies on virtio serial ports and block devices. The punchboot
cli can be built with a domain socket transport instead of USB for communicating
with an QEMU environment.

The test platform code includes gcov code that calls the QEMU semihosting API
for storing test coverage data on the host.

Building and running tests:
```
$ cp configs/test_defconfig .config
$ make
$ make check
```

## Device identity

Most modern SoC's provide some kind of unique identity, that is guaranteed to
be unique for that particular type of SoC / Vendor etc but can not be guarateed
to be globally unique.

Punchboot provides a UUID3 device identity based on a combination of the unique
data from the SoC and an allocated, random, namspace UUID per platform.

When booting a linux system this information is relayed to linux through
in-line patching of the device-tree.
The device identity can be found in '/proc/device-tree/chosen/device-uuid'

## Command mode

Command mode is entered when the system can't boot or if the bootloader is
forced by a configurable, external event to do so.

In the command mode it is possible to update the bootloader, write data to
partitions and install default settings. From v0.3 and forward
an 'authentication cookie' must be used to interact with the bootloader to
prevent malicious activity. The only command that can be executed without
authentication is listing the device information (including the device UUID)

The authentication cookie consists of the device UUID encrypted with one of
the active key pair's private key.

## punchboot tool
The punchboot CLI is used for interacting with the command mode. A summary of the features available:

- Update the bootloader it self
- Manually start system A or B
- Activate boot partitions
- Load image to ram and execute it
- Display basic device info
- Configure fuses and GPT parition tables
- Call board specific functions

The tool is written in Python and some parts in C to allow bindings for other
languages. The tool is built on top of a library to make it possible to integrate
with other tooling and environments.

The tool is distributed through PyPi. Binary wheels are available for Windows,
Linux and macos (x86_64 and arm64).

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

Example:
```
$ punchboot dev show
Bootloader version: v0.6.1-40-ga47f-dirty
Device UUID:        0b177094-6b62-3572-902e-c1de339ecb01
Board name:         pico8ml
```

Creating the authentication token using the 'createtoken.sh' script located
in the tools folder. In this example the private key is stored on a yubikey 5 HSM.

```
$ ./createtoken.sh 0B177094-6B62-3572-902E-C1DE339ECB01 pkcs11 -sha256 "pkcs11:id=%02;type=private"
engine "pkcs11" set.
Enter PKCS#11 token PIN for PIV Card Holder pin (PIV_II):
Enter PKCS#11 key PIN for SIGN key:
```

Authenticating the session:
```
$ punchboot auth token ./0B177094-6B62-3572-902E-C1DE339ECB01.token <name of a key>
Signature format: secp256r1
Hash: sha256
Authenticating using key index 0 and './0B177094-6B62-3572-902E-C1DE339ECB01.token'
Read 103 bytes
Authentication successful
```
Now the command mode is fully unlocked. The token is of course only valid for
the individual unit with that perticular UUID.

## Metrics


Measurements taken on IMX6UL, running at 528 MHz loading a 400kByte binary.

Using hardware accelerators for SHA and RSA signatures:

| Parameter         | Value    | Unit |
| ----------------- | -------- | ---- |
| Power On Reset    | 28       | ms   |
| Bootloader init   | 7        | ms   |
| Blockdev read     | 13       | ms   |
| SHA256 Hash       | 4        | ms   |
| RSA 4096 Signaure | 5        | ms   |
|                   |          |      |
| Total             | 57       | ms   |


Using libtomcrypt for SHA and RSA:

| Parameter         | Value    | Unit |
| ----------------- | -------- | ---- |
| Power On Reset    | 28       | ms   |
| Bootloader init   | 7        | ms   |
| Blockdev read     | 13       | ms   |
| SHA256 Hash       | 431      | ms   |
| RSA 4096 Signaure | 567      | ms   |
|                   |          |      |
| Total             | 1046     | ms   |


Measurements taken on IMX8QXP, loading a 14296kByte binary.

Using hardware accelerators for SHA and RSA signatures:

| Parameter            | Value    | Unit |
| -------------------- | -------- | ---- |
| Power On Reset       | 175      | ms   |
| Bootloader init      | 6.358    | ms   |
| Blockdev read / hash | 107      | ms   |
| RSA 4096 Signature   | 0.676    | ms   |
| Total                | 288      | ms   |

The POR time is off due to some unidentified problem with the SCU firmware.
 A guess would be that this metric should be in the 20ms -range.

## Contributing

1. Fork the repository
2. Implement new feature or bugfix on a branch
3. Implement test case(s) to ensure that future changes do not break legacy
4. Run checks: cp configs/test_defconfig .config && make check
5. Create pull request
