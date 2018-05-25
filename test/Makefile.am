# This is part of the AB Dispatch package, which allow the netCDF C
# library to read and write the HYCOM AB format.

# This automake file generates the Makefile for the AB dispach layer
# test directory.

# Ed Hartnett

AM_CPPFLAGS = -I$(top_srcdir)/include

# Link to our assembled library.
AM_LDFLAGS = ${top_builddir}/src/libncsion.la

# The tests.
AB_DISPATCH_TESTS = tst_read1
check_PROGRAMS = $(AB_DISPATCH_TESTS)
TESTS = $(AB_DISPATCH_TESTS)

# The test data files.
EXTRA_DIST = surtmp_100l.b surtmp_100l.a
