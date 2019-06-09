![PB Logo](doc/pb_logo.png)

[![Coverity](https://scan.coverity.com/projects/16584/badge.svg)](https://scan.coverity.com/projects/jonpe960-punchboot)
[![Build Status](https://travis-ci.org/jonpe960/punchboot.svg?branch=master)](https://travis-ci.org/jonpe960/punchboot)
[![codecov](https://codecov.io/gh/jonpe960/punchboot/branch/master/graph/badge.svg)](https://codecov.io/gh/jonpe960/punchboot)
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

Releases history:

| Version | Release date | Changes                                          |
| ------- | ------------ | ------------------------------------------------ |
| v0.4    | 2019-06-19   | Use separate config partitions instead of GPT headers for storing the state  |
|         |              | Improve test/build system reliability            |
|         |              | Docker image can now build all targets and run tests |
|         |              | NXP's cst 3.2.0 is now released under BSD3 a minimal version is included in the tools folder |
|         |              | Increased usb flashing speed by interleaving emmc and usb DMA requests |
|         |              | Added script for creating authentication tokens  |
|         |              | Various bugfixes and improvements                |
| v0.3    | 2019-05-09   | Support for authentication of recovery mode      |
|         |              | Support optional verbose boot mode               |
|         |              | pbimage support for PKCS11/Yubikey backend       |
|         |              | USB VID/PID allocated from pid.codes, 1209:2019  |
|         |              | Support for linux/initramfs                      |
|         |              | Various bugfixes and improvements                |
| v0.2    | 2019-03-18   | Support for EC signatures, improved boot speed   |
| v0.1    | 2019-02-25   | First release                                    |

## Building

The easiest way is using docker.

First generate the docker image using the top most makefile

```
    make docker
```

Build the cst tool (for NXP targets)

```
docker run -it -u $(id -u $USER) -v <path to punchboot top dir>:/pb/ pb_docker_env make -C /pb/src/tools/imxcst/src
```

Building the jiffy-board target:

```
docker run -it -u $(id -u $USER) -v <path to punchboot top dir>:/pb/ pb_docker_env make -C /pb/src BOARD=jiffy LOGLEVEL=3
```

## Run test suite

Run the built in tests:

```
docker run -it -u $(id -u $USER) -v <path to punchboot top dir>:/pb/ pb_docker_env make -C /pb/ tests
```

The dockerfile in the top directory details the dependencies on ubuntu xenial

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
| Jiffy                | Fully supported         | https://github.com/jonpe960/jiffy       |
| Bebop                | Fully supported         | https://github.com/jonpe960/bebop       |
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
$ export BOARD=test 
$ export LOGLEVEL=3
$ make clean && make && make test
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

GPT Partitions

| Partition       | UUID                                 |
| --------------- | ------------------------------------ |
| System A        | 2af755d8-8de5-45d5-a862-014cfa735ce0 |
| System B        | c046ccd8-0f2e-4036-984d-76c14dc73992 |
| Root A          | c284387a-3377-4c0f-b5db-1bcbcff1ba1a |
| Root B          | ac6a1b62-7bd0-460b-9e6a-9a7831ccbfbb |
| Config Primary  | f5f8c9ae-efb5-4071-9ba9-d313b082281e |
| Config Backup   | 656ab3fc-5856-4a5e-a2ae-5a018313b3ee |

Platform namespace UUID's

| Platform        | UUID                                 |
| --------------- | ------------------------------------ |
| NXP imx6ul      | aeda39be-792b-4de5-858a-4c357b9b6302 |
| NXP imx8m       | 3292d7d2-2825-4100-90c3-968f2960c9f2 |
| NXP imx8x       | aeda39be-792b-4de5-858a-4c357b9b6302 |

## Recovery mode

Recovery mode is entered when the system can't boot or if the bootloader is 
forced by a configurable, external event to do so.

In the recovery mode it is possible to update the bootloader, write data to 
partitions and install default settings. From v0.3 and forward 
an 'authentication cookie' must be used to interact with the bootloader to 
prevent malicious activity. The only command that can be executed without 
authentication is listing the device information (including the device UUID)

The authentication cookie consists of the device UUID encrypted with one of 
the active key pair's private key. 

## punchboot tool
The punchboot CLI is used for interacting with the recovery mode. A summary of
 the features available:
 - Update the bootloader it self
 - Manually start system A or B
 - Activate boot partitions
 - Load image to ram and execute it
 - Display basic device info
 - Configure fuses and GPT parition tables

## pbimage tool
The pbimage tool is used to create a punchboot compatible image. The tool uses
a manifest file to describe which binaries should be included and what signing
key that should be used.

Example image manifest:

```
[pbimage]
key_index = 0
key_source = ../pki/secp256r1-key-pair.pem
hash_kind = SHA256
sign_kind = EC256
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

Signing an image using a yubikey, through opensc:

```
[pbimage]
key_index = 1
key_source = PKCS11
pkcs11_key_id = 02
pkcs11_provider = /usr/lib/x86_64-linux-gnu/opensc-pkcs11.so
hash_kind = SHA384
sign_kind = EC384
output = acu6c.pbi


[component]
type = ATF
load_addr = 0x80000000
file = bl31.bin

[component]
type = DT
load_addr = 0x80080000
file = imx8qxpmek.dtb

[component]
type = LINUX
load_addr = 0x80100000
file = Image

[component]
type = RAMDISK
load_addr = 0x80b00000
file = rootfs.cpio
```
## Punchboot Image Format (pbi)

PBI image

| Region     | Alignment | Offset | Comment            |
| ---------- | --------- | :----: | ------------------ |
| Header     | 512 bytes | 0b     | Header data        |
| Signature  |           | 512b   | Signature data     |
| Components |           |        | Component data     |


### Header

| Field            | Type     | Description                                |
| ---------------- | -------- | ------------------------------------------ |
| header_magic     | uint32_t | Magic value                                |
| haeder_version   | uint32_t | Header version                             |
| no_of_components | uint32_t | Number of components in the image          |
| key_index        | uint32_t | Which key pair was used to sign the image  |
| hash_kind        | uint32_t | Hash algorithm that was used to hash image |
| sign_kind        | uint32_t | Signature format                           |
| reserved         |          | Reserved, for future use                   |

Each board makefile contains a list of key's. The order they are added maps 
to the key_index parameter. 

Supported hashes:

| Hash     | hash_kind |
| -------- | :-------: |
| SHA256   | 2         |
| SHA384   | 3         |
| SHA512   | 4         |

Supported signature formats

| Signature format | sign_kind |
| ---------------- | :-------: |
| secp256          | 2         |
| secp384          | 3         |
| secp521          | 4         |
| rsa4096          | 1         |

### Component

| Field               | Type     | Description                               |
| ------------------- | -------- | ----------------------------------------- |
| comp_header_version | uint32_t | Version of component header               |
| component_type      | uint32_t | Component type                            |
| load_addr           | uint64_t | Where the component should be loaded into RAM |
| component_size      | uint32_t | Component size in bytes                   |
| component_offset    | uint32_t | Component offset within the PBI file      |
| reserved            |          | Reserved for future use                   |

Supported component types:

| Component type | component_type |
| -------------- | :------------: |
| TEE            | 0              |
| VMM            | 1              |
| LINUX          | 2              |
| DT             | 3              |
| RAMDISK        | 4              |
| ATF            | 5              |
| KERNEL         | 6              |

## Authentication token

Punchboot enforces authentication when a device is enforcing secure boot. 
It is still possible to access the USB recovert mode after authentication. 
When the device enters recovery mode it is still possible to issue the ' dev -l '
 command to get the device UUID.

The authentication token is generated by hashing the device UUID and signing
it with one of the active key pairs.

Example:

```
$ punchboot dev -l

Device info:
 Bootloader Version: PB v0.3-82-gea3a-dirty

Parameter           Value
---------           -----
Platform            NXP IMX8M
Device UUID         0B177094-6B62-3572-902E-C1DE339ECB01
Board               Pico8ml

```
Creating the authentication token using the 'createtoken.sh' script located 
in the tools folder. In this example the private key is stored on a yubikey 5 HSM.

```
$ ./createtoken.sh 0B177094-6B62-3572-902E-C1DE339ECB01 pkcs11 -sha384 "pkcs11:id=%02;type=private"

engine "pkcs11" set.
Enter PKCS#11 token PIN for PIV Card Holder pin (PIV_II):
Enter PKCS#11 key PIN for SIGN key:
 
$ punchboot dev -a -s secp256r1:sha256 -n 0 -f ./0B177094-6B62-3572-902E-C1DE339ECB01.token

Signature format: secp256r1
Hash: sha256
Authenticating using key index 0 and './0B177094-6B62-3572-902E-C1DE339ECB01.token'
Read 103 bytes
Authentication successful
```
Now the recovery mode is fully unlocked. The token is ofcourse only valid for 
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

