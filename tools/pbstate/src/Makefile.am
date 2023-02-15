lib_LTLIBRARIES = libpbstate.la

libpbstate_la_SOURCES = crc.c \
                        pbstate.c

libpbstate_la_CFLAGS = -DPRIMARY_STATE_UUID=\"$(PRIMARY_STATE_UUID)\" \
                       -DBACKUP_STATE_UUID=\"$(BACKUP_STATE_UUID)\"

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pbstate.pc

pkginclude_HEADERS = pbstate.h

bin_PROGRAMS = pbstate

pbstate_SOURCES  = main.c

pbstate_LDADD = libpbstate.la

