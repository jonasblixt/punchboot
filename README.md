![PB Logo](doc/pb_logo.png)

[![Coverity](https://scan.coverity.com/projects/16584/badge.svg)](https://scan.coverity.com/projects/jonasblixt-punchboot)
[![Build Status](https://travis-ci.com/jonasblixt/punchboot.svg?branch=master)](https://travis-ci.org/jonasblixt/punchboot)
[![codecov](https://codecov.io/gh/jonasblixt/punchboot/branch/master/graph/badge.svg)](https://codecov.io/gh/jonasblixt/punchboot)
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

## Building

The easiest way is using docker.

Building the docker image:

```
$ docker build -f pb.Dockerfile -t pb_docker_env .
```

Building the jiffy-board target:

```
$ ./run_docker.sh
$ cp configs/jiffy_defconfig .config
$ make
```

## Run test suite

Run the built in tests:

```
$ ./run_docker.sh
$ cp configs/test_defconfig .config
$ make
$ make check
```

The dockerfile in the top directory details the dependencies on ubuntu xenial

## Design

Punchboot is written in C and some assembler. Currently armv7a and armv8 is supported.

The directory layout is as follows:

| Folder       | Description             |
| ------------ | ----------------------- |
| /doc         | Documentation           |
| /pki         | Crypto keys for testing |
| /            | Bootloader source       |
| /board       | Board support           |
| /arch        | Architecture support    |
| /plat        | Platform support        |
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
| Jiffy                | Fully supported         | https://github.com/jonasblixt/jiffy       |
| Bebop                | Fully supported         | https://github.com/jonasblixt/bebop       |
| Technexion PICO-IMX8M| Partial support         | https://www.technexion.com/products/system-on-modules/pico/pico-compute-modules/detail/PICO-IMX8M |
| Rockpro64            | Planned                 | https://www.pine64.org/?page_id=61454 |
| NanoPi-NEO-core      | Planned                 | http://www.nanopi.org/NanoPi-NEO-Core_Feature.html |
| NanoPi-NEO-core2     | Planned                 | http://www.nanopi.org/NanoPi-NEO-Core2_Feature.html |
| NXP IMX8QXP MEK      | Fully supported         | https://www.nxp.com/products/processors-and-microcontrollers/arm-based-processors-and-mcus/i.mx-applications-processors/i.mx-8-processors/i.mx-8-multisensory-enablement-kit:i.MX8-MEK |

Hardware accelerated signature verification

| Platform        | RSA4096 | EC secp256r1 | EC secp384r1 | EC secp521 |
| --------------- | ------- | ------------ | ------------ | ---------- |
| NXP imx6ul      | Yes     | Yes          | No           | No         |
| NXP imx8m       | Yes     | Yes          | No           | No         |
| NXP imx8x       | Yes     | Yes          | Yes          | Yes        |

Hardware accelerated hash algorithms

| Platform        | MD5 | SHA256 | SHA384 | SHA512 |
| --------------- | --- | ------ | ------ | ------ |
| NXP imx6ul      | Yes | Yes    | No     | No     |
| NXP imx8m       | Yes | Yes    | No     | No     |
| NXP imx8x       | Yes | Yes    | Yes    | Yes    |

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
## A/B paritions and atomic upgrades
To support a robust way of upgrading the system the simplest way is to have two copies
of the system software; System A and System B. When system A is active System B can be
reprogrammed and activated only when it is verified. This is known as "Atomic Upgrade"

Punchboot uses a special config partition to store the current state. The state
includes information about which system is active, boot count tries and error bits.

### Automatic rollback

Sometimes upgrades fail. Punchboot supports a mechanism for so called automatic 
rollbacks. 

![PB Rollback](doc/rollback.png)

The left most column describes a simplified way a linux system could initiate 
an upgrade. In this case System A is active and System B is to be prepared and 
eventually activated.

The new software is written to System B and verified then system A verified flag,
error bits must be reset and boot try counter is programmed to a desired try-count. 
A cleared OK bit but set counter constitutes and upgrade state and punchboot 
will try to start this system and decrement the counter unless the counter has reched zero.

If the counter reaches zero the error bit is set and System A is automatically 
activated again (Rollback event)

At this point the upgrade is staged, and the OK bit of System A can be cleared 
and finally the system is reset.

Punchboot recognizes that none of the System partitions has the OK bit set but 
System B has a non-zero counter. System B is started.

When returning back to the upgrade application in linux final checks can be 
performed, for example checking connectivity and such before finally setting 
the OK bit of system B and thus permanently activate System B

## Device identity

Most modern SoC's provide some kind of unique identity, that is guaranteed to 
be unique for that particular type of SoC / Vendor etc but can not be guarateed 
to be globally unique.

Punchboot provides a UUID3 device identity based on a combination of the unique 
data from the SoC and an allocated, random, namspace UUID per platform.

When booting a linux system this information is relayed to linux through 
in-line patching of the device-tree.
The device identity can be found in '/proc/device-tree/chosen/device-uuid'

## Allocated UUID's

Platform namespace UUID's

| Platform        | UUID                                 |
| --------------- | ------------------------------------ |
| NXP imx6ul      | aeda39be-792b-4de5-858a-4c357b9b6302 |
| NXP imx8m       | 3292d7d2-2825-4100-90c3-968f2960c9f2 |
| NXP imx8x       | aeda39be-792b-4de5-858a-4c357b9b6302 |

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
The punchboot CLI is used for interacting with the command mode. A summary of
 the features available:
 - Update the bootloader it self
 - Manually start system A or B
 - Activate boot partitions
 - Load image to ram and execute it
 - Display basic device info
 - Configure fuses and GPT parition tables

The tools and CLI are from version 0.7.0 separated into another repository:
https://github.com/jonasblixt/punchboot-tools

## Image format
Punchboot uses the bitpacker file format (https://github.com/jonasblixt/bpak)


## Authentication token

Punchboot enforces authentication when a device is enforcing secure boot. 
It is still possible to access the USB recovert mode after authentication. 
When the device enters command mode it is still possible to issue the ' dev -l '
 command to get the device UUID.

The authentication token is generated by hashing the device UUID and signing
it with one of the active key pairs.

Example:

```
$ punchboot dev --show

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
 
$ punchboot auth --token ./0B177094-6B62-3572-902E-C1DE339ECB01.token --key-id 0xa90f9680

Signature format: secp256r1
Hash: sha256
Authenticating using key index 0 and './0B177094-6B62-3572-902E-C1DE339ECB01.token'
Read 103 bytes
Authentication successful
```
Now the command mode is fully unlocked. The token is ofcourse only valid for 
the individual unit with that perticular UUID.

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

## Contributing

1. Fork the repository
2. Implement new feature or bugfix on a branch
3. Implement test case(s) to ensure that future changes do not break legacy
4. Run checks: cp configs/test_defconfig .config && make check
5. Create pull request

