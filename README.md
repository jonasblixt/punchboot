![PB Logo](doc/pb_logo.png)


# Introduction

Punchboot is a bootloader for ARM SoC's. It is the code that run's after 
boot ROM code has executed. The primary function is to load the next
software component of the boot chain, for example an OS kernel, hypervisor
 or TEE (Trusted Execution Environment) software.

As the name implies, punchboot is fast, really fast. Infact, boot speed
is one of the primary design goals.

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
| armv7a-ve    | Yes                     |
| armv8        | Planned                 |

Supported platforms:

| Platform     | Supported            |
| ------------ | -------------------- |
| NXP imx6ul   | Yes                  |
| NXP imx8m    | Planned              |
| Allwinner H3 | Planned              |
| Allwinner H5 | Planned              |


Supportd boards:

| Board                | Supported               | More info |
| -------------------- | ----------------------- | --------- |
| Jiffy                | Fully supported         | TBA       |
| Bebop                | Fully supported         | TBA       |
| Wandboard PI-8M-LITE | Planned                 | https://www.wandboard.org/products/wandpi-8m/wand-pi-8m-lite/ |
| NanoPi-NEO-core      | Planned                 | http://www.nanopi.org/NanoPi-NEO-Core_Feature.html |
| NanoPi-NEO-core2     | Planned                 | http://www.nanopi.org/NanoPi-NEO-Core2_Feature.html |


### Root of trust

Punchboot is designed to be a part of a secure boot chain. This means that
the bootloader is cryptographically signed, the ROM code of the SoC must
support a mechanism to validate this signature, otherwise there is not
root of trust.

When punchboot has been verified it, in turn, will load and verify the next
software component in the boot chain. The bootloader _only_ supports signed
binaries. 

### Atomic upgrades
### Secure key storage
### Secure log
### Recovery mode
### PBI Image format
### pbimage tool
### punchboot tool

