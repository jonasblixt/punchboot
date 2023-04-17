Introduction
============

Punchboot is a booloader for embedded systems. It has a strong emphasis on
security and compactness.

The following architectures are supported:

- Armv7a
- Armv8a

The following SoC's/platform's are supported:

- nxp imx6ul
- nxp imx8x
- nxp imx8m
- qemu virt

License
-------

Punchboot is premissivily licensed under the BSD 3 licens.

Distinguishing Features
-----------------------

**Only supports signed payloads**
    Punchboot only supports signed payloads which reduces the risk for configuration
    errors and reduces the logic in the boot code.

**Highly configurable**
    Most features can be enabled or disabled through the Kconfig interface

**Host tooling**
    Punchboot-tools provides a set of tools and libraries for interacting
    with the bootloader

    * Punchboot CLI and a C library
    * Python wrapper

    These can easly be extended to support board/production specific commands.

**Optimized for speed**
    Most drivers, in particular block device drivers and hashing accelerators
    use DMA to maximize transfer speed.
