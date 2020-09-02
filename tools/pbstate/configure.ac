dnl pbstate util

AC_PREREQ(2.59)
AC_INIT([pbstate], [0.2.0], [Jonas Blixt, <jonpe960@gmail.com>])

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

AC_ARG_VAR([PRIMARY_STATE_UUID], Primary state partition UUID)
AC_ARG_VAR([BACKUP_STATE_UUID], Backup state partition UUID)

: ${PRIMARY_STATE_UUID=f5f8c9ae-efb5-4071-9ba9-d313b082281e}
: ${BACKUP_STATE_UUID=656ab3fc-5856-4a5e-a2ae-5a018313b3ee}

AC_MSG_NOTICE([Primary state UUID: $PRIMARY_STATE_UUID])
AC_MSG_NOTICE([Backup state UUID: $BACKUP_STATE_UUID])

AC_CONFIG_FILES([Makefile
                src/Makefile
                src/pbstate.pc
                test/Makefile])
AX_CODE_COVERAGE

dnl Dependencies build
dnl AC_CONFIG_SUBDIRS([deps/dep1])

AC_OUTPUT
