#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT([makedeepzoom], [0.3.2], [joe@sortelli.com])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
LT_INIT
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/makedeepzoom.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_CHECK_FUNCS([mkdir pow strchr strdup strerror strrchr])

PKG_CHECK_MODULES(MAGICK, MagickWand MagickCore)

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
