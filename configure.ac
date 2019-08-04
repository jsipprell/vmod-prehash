AC_PREREQ(2.59)
AC_COPYRIGHT([Your mom])
AC_INIT([libvmod_prehash], [1.1.1])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build])
m4_ifndef([VARNISH_VMODS], AC_MSG_ERROR([Needs varnish.m4]))
AM_CONFIG_HEADER(include/config.h)

AC_CANONICAL_SYSTEM
AC_LANG(C)

AM_INIT_AUTOMAKE([foreign])
AM_SILENT_RULES([yes])

AC_GNU_SOURCE
AC_PROG_CC
AC_PROG_CC_STDC
if test "x$ac_cv_prog_cc_c99" = xno; then
  AC_MSG_ERROR([Could not find a C99 compatible compiler])
fi
AC_PROG_CPP

AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET

LD_VERSION_SCRIPT([symbols.map])

AC_CHECK_PROGS(RST2MAN, [rst2man rst2man.py], "no")

AC_HEADER_STDC
AC_CHECK_HEADERS([sys/stdlib.h])

m4_ifndef([PKG_CHECK_VAR], [
AC_DEFUN([PKG_CHECK_VAR],
  [AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
  AC_ARG_VAR([$1], [value of $3 for $2, overriding pkg-config])dnl
  _PKG_CONFIG([$1], [variable="][$3]["], [$2])
  AS_VAR_COPY([$1], [pkg_cv_][$1])
  AS_VAR_IF([$1], [""], [$5], [$4])dnl
  ])# PKG_CHECK_VAR
])

VARNISH_PREREQ([6.2.0])
VARNISH_VMODS([prehash])

AC_CHECK_SIZEOF([void*])
AS_CASE($target_cpu,
  [x86_64], [have_atomicptr=$GCC],
  [i?86], [have_atomicptr=$GCC],
  [have_atomicptr=])

AS_VAR_IF(have_atomicptr, ["yes"], [dnl
AC_DEFINE([HAVE_ATOMICPTR], [1], [Define if the target architecture's pointers are atomic if aligned])], [])

AC_CONFIG_FILES([
  Makefile
  src/Makefile
])

AC_OUTPUT
