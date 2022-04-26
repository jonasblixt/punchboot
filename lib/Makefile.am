lib_LTLIBRARIES = libpunchboot.la

libpunchboot_la_SOURCES = error.c \
                          wire.c \
                          usb.c \
                          api.c \
                          api_stream.c \
                          api_partition.c \
                          api_device.c \
                          api_slc.c \
                          api_authentication.c \
                          api_boot.c \
                          api_misc.c \
                          api_board.c

if !WINDOWS
libpunchboot_la_SOURCES += socket.c
endif

libpunchboot_la_LDFLAGS = $(LIBUSB_LDFLAGS) $(BPAK_LDFLAGS)

libpunchboot_la_CFLAGS = -I$(top_srcdir)/include \
                           $(LIBUSB_CFLAGS) \
                           $(BPAK_CFLAGS) \
                           $(CODE_COVERAGE_CFLAGS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = punchboot.pc

libpunchboot_la_LIBS = $(CODE_COVERAGE_LIBS) $(LIBUSB_LIBS) $(BPAK_LIBS)

pkginclude_HEADERS = $(top_srcdir)/include/pb-tools/api.h \
                     $(top_srcdir)/include/pb-tools/error.h \
                     $(top_srcdir)/include/pb-tools/usb.h \
                     $(top_srcdir)/include/pb-tools/wire.h
