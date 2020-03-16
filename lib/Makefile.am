lib_LTLIBRARIES = libpunchboot.la

libpunchboot_la_SOURCES = api.c \
						  protocol.c \
						  usb.c \
						  socket.c

libpunchboot_la_LDFLAGS = $(LIBUSB_LDFLAGS)

libpunchboot_la_CFLAGS = -I$(top_srcdir)/include \
						   $(LIBUSB_CFLAGS) \
            	           $(CODE_COVERAGE_CFLAGS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = punchboot.pc

libpunchboot_la_LIBS = $(CODE_COVERAGE_LIBS) $(LIBUSB_LIBS)

pkginclude_HEADERS = $(top_srcdir)/include/pb/api.h \
					 $(top_srcdir)/include/pb/error.h \
					 $(top_srcdir)/include/pb/protocol.h
