lib_LTLIBRARIES = libpunchboot.la

libpunchboot_la_SOURCES = protocol.c \
						  error.c \
                          wire.c \
                          usb.c \
                          socket.c

libpunchboot_la_LDFLAGS = $(LIBUSB_LDFLAGS)

libpunchboot_la_CFLAGS = -I$(top_srcdir)/include \
                           $(LIBUSB_CFLAGS) \
                           $(CODE_COVERAGE_CFLAGS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = punchboot.pc

libpunchboot_la_LIBS = $(CODE_COVERAGE_LIBS) $(LIBUSB_LIBS)

pkginclude_HEADERS = $(top_srcdir)/include/pb-tools/api.h \
                     $(top_srcdir)/include/pb-tools/error.h \
                     $(top_srcdir)/include/pb-tools/wire.h
