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

Booting an image
================

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

When a bootable parition is active punchboot will try to boot from if::

    $ punchboot boot --activate 2af755d8-8de5-45d5-a862-014cfa735ce0

Partition management
====================

Listing the punchboot partition table::

    $ punchboot part --list
    Partition UUID                          Flags      Size      Name
    --------------                          -----      ----      ----
    9eef7544-bf68-4bf7-8678-da117cbccba8    --W-----   1024 kB   eMMC boot0
    4ee31690-0c9b-4d56-a6a6-e6d6ecfd4d54    --W-----   1024 kB   eMMC boot1
    8d75d8b9-b169-4de6-bee0-48abdc95c408    --r-----   1024 kB   eMMC RPMB
    2af755d8-8de5-45d5-a862-014cfa735ce0    B-W-----   30   MB   System A
    c046ccd8-0f2e-4036-984d-76c14dc73992    B-W-----   30   MB   System B
    c284387a-3377-4c0f-b5db-1bcbcff1ba1a    --W-----   128  MB   Root A
    ac6a1b62-7bd0-460b-9e6a-9a7831ccbfbb    --W-----   128  MB   Root B
    f5f8c9ae-efb5-4071-9ba9-d313b082281e    --r-----   512  B    PB State Primary
    656ab3fc-5856-4a5e-a2ae-5a018313b3ee    --r-----   512  B    PB State Backup
    4581af22-99e6-4a94-b821-b60c42d74758    --W-----   30   MB   Root overlay A
    da2ca04f-a693-4284-b897-3906cfa1eb13    --W-----   30   MB   Root overlay B
    23477731-7e33-403b-b836-899a0b1d55db    --W-----   128  kB   RoT extension A
    6ffd077c-32df-49e7-b11e-845449bd8edd    --W-----   128  kB   RoT extension B
    9697399d-e2da-47d9-8eb5-88daea46da1b    --W-----   128  MB   System storage A
    c5b8b41c-0fb5-494d-8b0e-eba400e075fa    --W-----   128  MB   System storage B
    c5b8b41c-0fb5-494d-8b0e-eba400e075fa    --W-----   1024 MB   Mass storage

Punchboot has a flat list of partitions, this means that there can be more than
one physical device represented in this table. In this example the eMMC boot and
RPMB partitions are available in the same way as 'user' partitions. They how ever
requires the driver to configure the memory in a particular way for accesses to
those areas.

Install the default partition layout::

    $ punchboot part --install

Writing data to a partition::

    $ punchboot part --write rootfs.ext4 --part c284387a-3377-4c0f-b5db-1bcbcff1ba1a

Verifying written data::

    $ punchboot part --verify rootfs.ext4 --part c284387a-3377-4c0f-b5db-1bcbcff1ba1a

The verify command will compute a sha256 checksum of the input file and then
request that the bootloader does the same for data starting at the begining of
the partition and extending to the size of the input file. When the computaion is
done the sha's will be compared, and if the match the partition is considered verified.

Showing installed BPAK packages::

    $ punchboot part --show

This command will check for and display bpak headers on all partitions. At least
the boot image that punchboot loads should have a bpak header.

Example output::

    Partition: 2af755d8-8de5-45d5-a862-014cfa735ce0 (System A)
    Hash:      sha512
    Signature: secp521r1

    Metadata:
        ID         Size   Meta ID              Part Ref   Data
        fb2f1f3f   16     bpak-package                    76dff3ba-5529-4964-bf49-4e8856e93242
        d1e64a4b   8      pb-load-addr         ec103b08   Entry: 0x82000000
        d1e64a4b   8      pb-load-addr         56f91b86   Entry: 0x80800000
        d1e64a4b   8      pb-load-addr         f4cdac1f   Entry: 0x81000000
        d1e64a4b   8      pb-load-addr         a697d988   Entry: 0x80000000
        d1e64a4b   8      pb-load-addr         06f78ab9   Entry: 0xfe000000
        2d44bbfb   32     bpak-transport       ec103b08   Encode: 9f7aacf9, Decode: b5964388
        2d44bbfb   32     bpak-transport       56f91b86   Encode: 9f7aacf9, Decode: b5964388
        2d44bbfb   32     bpak-transport       f4cdac1f   Encode: 9f7aacf9, Decode: b5964388
        2d44bbfb   32     bpak-transport       a697d988   Encode: 9f7aacf9, Decode: b5964388
        2d44bbfb   32     bpak-transport       06f78ab9   Encode: 9f7aacf9, Decode: b5964388

    Parts:
        ID         Size         Z-pad  Flags          Transport Size
        56f91b86   100484       380    --------       100484      
        a697d988   37016        360    --------       37016       
        f4cdac1f   654336       0      --------       654336      
        ec103b08   14563336     504    --------       14563336    
        06f78ab9   415368       376    --------       415368      

Dumping data::

    $ punchboot part --dump output.dat --part 2af755d8-8de5-45d5-a862-014cfa735ce0

For security reasons it's not possible to dump data from partitions unles
the partitions has explicitly been configured as 'dumpable'.

Authentication
==============

Punchboot supports two authentication mechanisms, either a token based or a
password based. Read more about those in the punchboot documentation (...)

Authenticating using a token file::

    $ punchboot auth --token token.dat --key-id pb

Authenticating using a password::

    $ punchboot auth --password <the password>

