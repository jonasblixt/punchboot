:mod:`plat` --- Platform API
============================

.. module:: plat
   :synopsis: Platform API

The platform API constitutes the minimum and mandatory functions each platform
must provide.

The 'plat_init' function in each platform is expected to perform the follwing basic initialization:

- Initialize a watchdog
- Configure the systick
- Load and decode system boot reason
- Initialize debug console
- Configure the MMU

The 'plat_init' and 'plat_board_init' functions are called early during boot,
se :ref:`Boot Process` for more details.

----------------------------------------------

Source code: :github-blob:`include/pb/plat.h`

----------------------------------------------

.. doxygenfile:: include/pb/plat.h
   :project: pb
