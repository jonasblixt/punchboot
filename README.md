![PB Logo](doc/pb_logo.png)

[![Coverity](https://scan.coverity.com/projects/16584/badge.svg)](https://scan.coverity.com/projects/jonpe960-punchboot)
[![Build Status](https://travis-ci.org/jonpe960/punchboot.svg?branch=master)](https://travis-ci.org/jonpe960/punchboot)
[![codecov](https://codecov.io/gh/jonpe960/punchboot/branch/master/graph/badge.svg)](https://codecov.io/gh/jonpe960/punchboot)
[![Donate](https://img.shields.io/badge/paypal-donate-yellow.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=4XMK2G3TPN3BQ)

# Introduction

Punchboot is a bootloader for ARM SoC's. It is the code that run's after 
boot ROM code has executed. The primary function is to load the next
software component of the boot chain, for example an OS kernel, hypervisor
 or TEE (Trusted Execution Environment) software.

As the name implies, punchboot is fast, really fast. Infact, boot speed
is one of the primary design goals.



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


## Design

Punchboot is written in C and some assembler. Currently armv7a-ve is the only
supported architecture, but armv8 support will come soon.

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
| NXP imx8x       | Yes                  | Yes | Yes  | Yes       | No          | No      |
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


## Root of trust

Punchboot is designed to be a part of a secure boot chain. This means that
the bootloader is cryptographically signed, the ROM code of the SoC must
support a mechanism to validate this signature, otherwise there is not
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

## Device life cycle management


## Testing and integration tests

## Atomic upgrades
## Recovery mode
## PBI Image format
## pbimage tool
## punchboot tool

