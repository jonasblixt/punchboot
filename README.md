![PB Logo](doc/pb_logo.png)

[![Coverity](https://scan.coverity.com/projects/16584/badge.svg)](https://scan.coverity.com/projects/jonpe960-punchboot)
[![Build Status](https://travis-ci.org/jonpe960/punchboot.svg?branch=master)](https://travis-ci.org/jonpe960/punchboot)
[![codecov](https://codecov.io/gh/jonpe960/punchboot/branch/master/graph/badge.svg)](https://codecov.io/gh/jonpe960/punchboot)
[![Donate](https://img.shields.io/badge/paypal-donate-yellow.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=4XMK2G3TPN3BQ)

# Introduction

Punchboot is a bootloader for ARM SoC's. It is designed to:

 - Boot as fast as possible
 - Integrate with the SoC's secure boot
 - Authenticate the next piece of software in the boot chain
 - Support A/B system paritions for atomic updates
 - Support automatic rollbacks
 - Minimize software download time in production
 - Be useful for day-to-day development

Punchboot is designed for embedded systems and therfore it has a minimalistic 
apporach. There is no run-time configuration, everything is configured in 
the board files.

Punchboot could be useful if you care about the following:
 - Boot speed
 - Secure boot
 - Downloading software quickly in production

Releases history:

| Version | Release date | Changes                                          |
| ------- | ------------ | ------------------------------------------------ |
| v0.2    | 2019-03-18   | Support for EC signatures, improved boot speed   |
| v0.1    | 2019-02-25   | First release                                    |

## Design

Punchboot is written in C and some assembler. Currently armv7a and armv8 is supported.

The directory layout is as follows:

| Folder       | Description             |
| ------------ | ----------------------- |
| /doc         | Documentation           |
| /pki         | Crypto keys for testing |
| /src         | Bootloader source       |
| /src/board   | Board support           |
| /src/arch    | Architecture support    |
| /src/plat    | Platform support        |
| /tools       | Tools                   |

Supported architectures:

| Architecture | Supported               |
| ------------ | ----------------------- |
| armv7a       | Yes                     |
| armv8a       | Yes                     |

Supported platforms:

| Platform        | Supported            | USB | EMMC | HW Crypto | Secure Boot | Fusebox |
| --------------- | -------------------- | --- | ---- | --------- | ----------- | ------- |
| NXP imx6ul      | Yes                  | Yes | Yes  | Yes       | Yes         | Yes     |
| NXP imx8m       | Yes                  | Yes | Yes  | Yes       | No          | Yes     |
| NXP imx8x       | Yes                  | Yes | Yes  | Yes       | Yes         | Yes     |
| Rockchip RK3399 | Planned              |     |      |           |             |         |
| Allwinner H3    | Planned              |     |      |           |             |         |
| Allwinner H5    | Planned              |     |      |           |             |         |


Supportd boards:

| Board                | Supported               | More info |
| -------------------- | ----------------------- | --------- |
| Jiffy                | Fully supported         | TBA       |
| Bebop                | Fully supported         | TBA       |
| Technexion PICO-IMX8M| Partial support         | https://www.technexion.com/products/system-on-modules/pico/pico-compute-modules/detail/PICO-IMX8M |
| Rockpro64            | Planned                 | https://www.pine64.org/?page_id=61454 |
| NanoPi-NEO-core      | Planned                 | http://www.nanopi.org/NanoPi-NEO-Core_Feature.html |
| NanoPi-NEO-core2     | Planned                 | http://www.nanopi.org/NanoPi-NEO-Core2_Feature.html |
| NXP IMX8QXP MEK      | Fully supported         | https://www.nxp.com/products/processors-and-microcontrollers/arm-based-processors-and-mcus/i.mx-applications-processors/i.mx-8-processors/i.mx-8-multisensory-enablement-kit:i.MX8-MEK |

Hardware accelerated signature verification

| Platform        | RSA4096 | EC secp256r1 | EC secp384r1 | EC secp521 |
| --------------- | ------- | ------------ | ------------ | ---------- |
| NXP imx6ul      | Yes     | Yes          | Planned      | Planned    |
| NXP imx8m       | Yes     | Yes          | Planned      | Planned    |
| NXP imx8x       | Yes     | Yes          | Yes          | Yes        |

## Root of trust

Punchboot is designed to be a part of a secure boot chain. This means that
the bootloader is cryptographically signed, the ROM code of the SoC must
support a mechanism to validate this signature, otherwise there is no
root of trust.

When punchboot has been verified it, in turn, will load and verify the next
software component in the boot chain. The bootloader _only_ supports signed
binaries. 

### Early boot stages and verification of punchboot binary

Most SoC:s have a boot rom that includes meachanisms for calculating a checksum
of the bootloader and cryptographically verifying a signature using a public key
 fused to the device.

Normally fuses are a limited resource and therefor a common way is to calculate
 a sha256 checksum of the public key(s) and then store this checksum in fuses,
 this way many different public keys can be stored in a flash memory and every
time the device boots it will compute a sha256 checksum and compare it to the
fused checksum.

### IMX6 boot rom
The IMX6 SoC:s bootrom support 4 public key's and a method of revoking keys.
This way it is possible to change the signing key for devices that are already
in the field to another, known key pair.

### Key management in punchboot

The punchboot security model requires support for three different signing keys
for the bootloader binary.

 - Production key
 - Development key
 - Field key 1
 - Field key 2

#### Production key
The production key is used during the device manufacutring. Ideally the root key
checksums should be fused by the SoC vendor before shipping to the manufacturing
plant where punchboot will be installed. This way it is substantially more
difficult to introduce compromised parts in the supply chain.

This key is revoked after final unit test and assembly

#### Development key
This key should be considered insecure, the idea is that both parts of the key
can be accessed by the development teams to support low level development and 
debugging.

#### Field key(s)
Production units that are shipped from the factory will use field keys. The private key part
is obviously a secure asset and should probably only live in a HSM environment with
very limited access.

## Testing and integration tests
Punchboot uses QEMU for all module and integration tests. The 'test' platform
 and board target relies on virtio serial ports and block devices. The punchboot
 cli can be built with a domain socket transport instead of USB for communicating
 with an QEMU environment.

The test platform code includes gcov code that calls the QEMU semihosting API
 for storing test coverage data on the host.

Building and running tests:
```
$ export BOARD=test 
$ export LOGLEVEL=3
$ make clean && make && make test
```
## Atomic upgrades
To allow atomic updates as a minimum there needs to be two system partitions
 one that is active and another that can be updated and verified before 
 switching over.

Punchboot uses the 'bootable' attribute in the GPT partition header to indicate
which partion is bootable or not.

## Recovery mode
Recovery mode is entered when the system can't boot or if the bootloader is
 forced by a configurable, external event.

In the recovery mode it is possible to update the bootloader, write data to
partitions and install default settings.

## pbimage tool
The pbimage tool is used to create a punchboot compatible image. The tool uses
 a manifest file to describe which binaries should be included and what signing
 key that should be used.

Example image manifest:

```
[pbimage]
key_index = 1
key_source = ../pki/prod_rsa_private.der
output = test_image.pbi

[component]
type = ATF
load_addr = 0x80000000
file = bl31.bin

[component]
type = DT
load_addr = 0x82000000
file = linux.dtb

[component]
type = LINUX
load_addr = 0x82020000
file = Image

```

## punchboot tool
The punchboot CLI is used for interacting with the recovery mode. A summary of
 the features available:
 - Update the bootloader it self
 - Manually start system A or B
 - Activate boot partitions
 - Load image to ram and execute it
 - Display basic device info
 - Configure fuses and GPT parition tables

## Metrics


Measurements taken on IMX6UL, running at 528 MHz loading a 400kByte binary.

Using hardware accelerators for SHA and RSA signatures:

| Parameter         | Value    | Unit |
| ----------------- |:--------:| ---- |
| Power On Reset    | 28       | ms   |
| Bootloader init   | 7        | ms   |
| Blockdev read     | 13       | ms   |
| SHA256 Hash       | 4        | ms   |
| RSA 4096 Signaure | 5        | ms   |
|                   |          |      |
| Total             | 57       | ms   |


Using libtomcrypt for SHA and RSA:

| Parameter         | Value    | Unit |
| ----------------- |:--------:| ---- |
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
| -------------------- |:--------:| ---- |
| Power On Reset       | 175      | ms   |
| Bootloader init      | 6.358    | ms   |
| Blockdev read / hash | 107      | ms   |
| RSA 4096 Signature   | 0.676    | ms   |
| Total                | 288      | ms   |

The POR time is off due to some unidentified problem with the SCU firmware.
 A guess would be that this metric should be in the 20ms -range.

