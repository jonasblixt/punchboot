COMMON_TEST_CFLAGS = -I $(top_srcdir)/include \
					 -I $(top_srcdir)/lib \
					 -I $(top_srcdir)/command/include \
					 $(LIBUSB_CFLAGS) \
					 -fsanitize=address \
					 -I /usr/include/uuid

COMMON_TEST_LDADD = libnala.la \
					$(top_builddir)/lib/libpunchboot.la \
					$(top_builddir)/command/libcommand.la \
					$(LIBUSB_LIBS) \
					-luuid

check_LTLIBRARIES = libnala.la

libnala_la_SOURCES = nala.c

check_PROGRAMS = test_help \
				 test_ctx \
				 test_alignment \
				 test_magic \
				 test_api \
				 test_authentication \
				 test_slc

test_help_SOURCES = test_help.c
test_help_LDADD = $(COMMON_TEST_LDADD)
test_help_CFLAGS = $(COMMON_TEST_CFLAGS)

test_ctx_SOURCES = test_ctx.c
test_ctx_LDADD = $(COMMON_TEST_LDADD)
test_ctx_CFLAGS = $(COMMON_TEST_CFLAGS)

test_alignment_SOURCES = test_protocol_alignment.c
test_alignment_LDADD = $(COMMON_TEST_LDADD)
test_alignment_CFLAGS = $(COMMON_TEST_CFLAGS)

test_magic_SOURCES = test_magic.c
test_magic_LDADD = $(COMMON_TEST_LDADD)
test_magic_CFLAGS = $(COMMON_TEST_CFLAGS)

test_api_SOURCES = test_api.c test_command_loop.c
test_api_LDADD = $(COMMON_TEST_LDADD)
test_api_CFLAGS = $(COMMON_TEST_CFLAGS)

test_authentication_SOURCES = test_authentication.c test_command_loop.c
test_authentication_LDADD = $(COMMON_TEST_LDADD)
test_authentication_CFLAGS = $(COMMON_TEST_CFLAGS)

test_slc_SOURCES = test_slc.c test_command_loop.c
test_slc_LDADD = $(COMMON_TEST_LDADD)
test_slc_CFLAGS = $(COMMON_TEST_CFLAGS)

TESTS = test_help \
		test_ctx \
		test_alignment \
		test_magic \
		test_api \
		test_authentication \
		test_slc
