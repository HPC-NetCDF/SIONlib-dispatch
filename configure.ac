#                                               -*- Autoconf -*- This
# is the autoconf script for the SIONlib Dispatch library, which
# allows netCDF to read SIONlib files.
#
# Ed Hartnett 5/25/18

# Set up autoconf.
AC_PREREQ([2.69])
AC_INIT([sionlib-dispatch], [1.0.0], [])
AC_CONFIG_SRCDIR([include/siondispatch.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

# Set up automake.
AM_INIT_AUTOMAKE([foreign subdir-objects])

# Set up libtool.
AC_MSG_NOTICE([setting up libtool])
LT_PREREQ([2.4])
LT_INIT()

# Checks for programs.
AC_PROG_CC

# Set these to get correct results from netCDF header files.
AC_DEFINE([USE_HDF4], 1, [Set for netCDF headers.])
AC_DEFINE([USE_NETCDF4], 1, [Set for netCDF headers.])

dnl # Does the user want to enable netcdf-4 logging?
dnl AC_MSG_CHECKING([whether logging is enabled])
dnl AC_ARG_ENABLE([logging],
dnl               [AS_HELP_STRING([--enable-logging],
dnl                               [Enable logging capability - for developers only  .])])
dnl test "x$enable_logging" = xyes || enable_logging=no
dnl AC_MSG_RESULT([$enable_logging])
dnl if test "x$enable_logging" = xyes; then
dnl    AC_DEFINE([AB_LOGGING], 1, [If true, turn on AB logging.])
dnl    AC_DEFINE([LOGGING], 1, [If true, turn on netCDF logging.])
dnl fi

dnl # These are data files needed for testing.
dnl AC_CONFIG_LINKS([test/surtmp_100l.a:test/surtmp_100l.a])
dnl AC_CONFIG_LINKS([test/surtmp_100l.b:test/surtmp_100l.b])

# Checks for libraries.
AC_CHECK_LIB(netcdf, nc_def_user_format)
if test x${ac_cv_lib_netcdf_nc_def_user_format} != xyes; then
   AC_MSG_ERROR([Must have netcdf])
fi

# Checks for header files.
AC_CHECK_HEADERS([stddef.h stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
#AC_FUNC_MALLOC
#AC_CHECK_FUNCS([strdup])

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 test/Makefile
                 src/Makefile])
AC_OUTPUT
