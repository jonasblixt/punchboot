Basic usage
===========

Checking if a device is connected::

    $ punchboot dev --show
    Bootloader version: v0.9.5
    Device UUID:        0bc6545a-a1a2-3fc7-9e4e-dbd79ff22d54
    Board name:         imx8qxmek

Output shows the bootloader version, a device unique identifier and board name.

Resetting the board::

    $ punchboot dev --reset

This causes the board to reboot

It's possible to make the tool wait/block until a device is connected using the
'--wait' option.

Waiting for a board connection::

    $ punchboot dev --show --wait 10

This will wait for a maximum of ten seconds before failing and setting an error
code. This is useful when calling the punchboot tool from a script.

Booting and image
=================

Puchboot supports two different loading modes, either load and boot something 
from and onboard flash or ram load and image over usb.

Booting from a GPT partition::

    $ punchboot boot --boot 2af755d8-8de5-45d5-a862-014cfa735ce0

When issuing this command punchboot will try to load and verify a boot image from
GPT partition '2af755d8-8de5-45d5-a862-014cfa735ce0'.

Booting over USB::

    $ punchboot boot --load boot.bpak

The contents of 'boot.bpak' will be loaded into ram and if verification is successful
punchboot will jump to the boot image.

Verbose boot can be invoked by adding '--verbose-boot'.

Activating a bootable partition
===============================


