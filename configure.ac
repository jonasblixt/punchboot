dnl Punchboot tools

AC_PREREQ(2.59)
AC_INIT([punchboot-tools], [0.1.0], [Jonas Blixt, <jonpe960@gmail.com>])

m4_pattern_forbid([^AX_(CHECK_ENABLE_DEBUG|CODE_COVERAGE|COMPILER_FLAGS|COMPILER_FLAGS_(CFLAGS|CXXFLAGS|LDFLAGS))\b],
  [Unexpanded AX_ macro found. Please install GNU autoconf-archive])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([foreign -Wall -Werror subdir-objects])

AM_PROG_AR
LT_INIT
AC_PROG_CC

dnl this allows us specify individual liking flags for each target
AM_PROG_CC_C_O

dnl Initialize Libtool

dnl Check if Libtool is present
dnl Libtool is used for building share libraries
AC_PROG_LIBTOOL

PKG_CHECK_MODULES(LIBUSB, [libusb-1.0 >= 0.1.12])
AC_SUBST(LIBUSB_CFLAGS)
AC_SUBST(LIBUSB_LDFLAGS)
AC_SUBST(LIBUSB_LIBS)

AC_CONFIG_FILES([Makefile
                src/Makefile
                lib/Makefile
                command/Makefile
                test/Makefile
                lib/punchboot.pc])

AX_CODE_COVERAGE
AC_OUTPUT
