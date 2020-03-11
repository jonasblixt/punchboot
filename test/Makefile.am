COMMON_TEST_CFLAGS = -I $(top_srcdir)/include \
					 -I $(top_srcdir)/lib \
					 $(LIBUSB_CFLAGS) \
					 -fsanitize=address
COMMON_TEST_LDADD = libnala.la \
					$(top_builddir)/lib/libpunchboot.la \
					$(LIBUSB_LIBS)

check_LTLIBRARIES = libnala.la

libnala_la_SOURCES = nala.c

check_PROGRAMS = test_help \
				 test_ctx

test_help_SOURCES = test_help.c
test_help_LDADD = $(COMMON_TEST_LDADD)
test_help_CFLAGS = $(COMMON_TEST_CFLAGS)

test_ctx_SOURCES = test_ctx.c
test_ctx_LDADD = $(COMMON_TEST_LDADD)
test_ctx_CFLAGS = $(COMMON_TEST_CFLAGS)

TESTS = test_help \
		test_ctx
