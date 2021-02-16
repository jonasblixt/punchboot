.. _Building and installing:

-----------------------
Building and installing
-----------------------

Dependencies:

  1. Normal build tools: automake, libtool, pkg-config, make
  2. libusb
  3. bpak (https://github.com/jonasblixt/bpak)

The 'autoconf-archive' package must be installed before running autoreconf.

Build library and tool::

    $ autoreconf -fi
    $ ./configure
    $ make
    $ sudo make install
    $ sudo ldconfig # This is only needed the first time to update library cache

Running tests::

    $ ./configure --enable-code-coverage
    $ make && make check

.. toctree::
   :maxdepth: 1
   :glob:

   build/*
