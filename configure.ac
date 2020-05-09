dnl Punchboot tools

AC_PREREQ(2.59)
AC_INIT([punchboot-tools], [0.1.10], [Jonas Blixt, <jonpe960@gmail.com>])

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

PKG_CHECK_MODULES(BPAK, [bpak >= 0.2.0])
AC_SUBST(BPAK_CFLAGS)
AC_SUBST(BPAK_LDFLAGS)
AC_SUBST(BPAK_LIBS)

AC_CHECK_TOOL([BPAK], [bpak], [:])

AC_CANONICAL_HOST

build_linux=no
build_windows=no
build_mac=no

# Detect the target system
case "${host_os}" in
    linux*)
        build_linux=yes
        AC_DEFINE([LINUX], 1, "Linux build")
        ;;
    cygwin*|mingw*)
        build_windows=yes
        AC_DEFINE([WINDOWS], 1, "Windows build")
        ;;
    darwin*)
        build_mac=yes
        AC_DEFINE([OSX], 1, "Mac build")
        ;;
    *)
        AC_MSG_ERROR(["OS $host_os is not supported"])
        ;;
esac

# Pass the conditionals to automake
AM_CONDITIONAL([LINUX], [test "$build_linux" = "yes"])
AM_CONDITIONAL([WINDOWS], [test "$build_windows" = "yes"])
AM_CONDITIONAL([OSX], [test "$build_mac" = "yes"])

AM_PATH_PYTHON([3.0],, [:])

AC_CONFIG_FILES([Makefile
                src/Makefile
                lib/Makefile
                python/Makefile
                test/Makefile
                dist/punchboot-tools-x64.iss
                lib/punchboot.pc])

AC_CONFIG_LINKS([dist/modpath.iss:dist/modpath.iss])

AX_CODE_COVERAGE
AC_OUTPUT
