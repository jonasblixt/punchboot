bin_PROGRAMS = punchboot

punchboot_SOURCES  = main.c \
					 dev.c \
					 utils.c \
                     help.c

punchboot_CFLAGS  = -I$(top_srcdir)/include \
					$(CODE_COVERAGE_CFLAGS) \
					$(UUID_CFLAGS)

punchboot_LDFLAGS = $(LIBUSB_LDFLAGS)

punchboot_LIBS = $(CODE_COVERAGE_LIBS)

punchboot_LDADD = $(top_builddir)/lib/libpunchboot.la $(LIBUSB_LIBS)
