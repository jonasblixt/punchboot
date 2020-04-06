bin_PROGRAMS = punchboot

punchboot_SOURCES  = main.c \
                     dev.c \
					 part.c \
					 boot.c \
					 board.c \
					 auth.c \
					 slc.c \
					 sha256.c \
                     utils.c \
					 uuid/clear.c \
					 uuid/compare.c \
					 uuid/isnull.c \
					 uuid/pack.c \
					 uuid/parse.c \
					 uuid/unpack.c \
					 uuid/unparse.c \
                     help.c

punchboot_CFLAGS  = -I$(top_srcdir)/include \
                    $(CODE_COVERAGE_CFLAGS) \
					$(BPAK_CFLAGS)

punchboot_LDFLAGS = $(LIBUSB_LDFLAGS) \
					$(BPAK_LDFLAGS)

punchboot_LIBS = $(CODE_COVERAGE_LIBS)

punchboot_LDADD = $(top_builddir)/lib/libpunchboot.la \
                  $(LIBUSB_LIBS) \
				  $(BPAK_LIBS)

