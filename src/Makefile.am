# This is part of the AB Dispatch package, which allow the netCDF C
# library to read and write with SIONlib.

# This automake file generates the Makefile for the SIONlib dispach
# layer source directory.

# Ed Hartnett 5/25/18

AM_CPPFLAGS = -I$(top_srcdir)/include 

# This is our output. 
lib_LTLIBRARIES = libncsion.la
libncsion_la_LDFLAGS = -version-info 1:0:0
libncsion_la_SOURCES = siondispatch.c sionvar.c sionfile.c sionfunc.c





