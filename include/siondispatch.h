/**
 * @file
 * This header file contains the prototypes for the SIONlib versions
 * of the netCDF functions.
 *
 * Ed Hartnett 5/25/18
 */
#ifndef _SIONDISPATCH_H
#define _SIONDISPATCH_H

#include "config.h"
#include <stddef.h> /* size_t, ptrdiff_t */
#include <netcdf.h>
#include <ncdispatch.h>

#define SION_NDIMS3 3
#define SION_NDIMS1 1
#define NUM_SION_VAR_ATTS 4
#define TIME_NAME "day"
#define SPAN_NAME "span"
#define MIN_NAME "min"
#define MAX_NAME "max"
#define I_NAME "i"
#define J_NAME "j"

/* This is the metadata we need to keep track of for each
   netcdf-4/HDF5 file. */
typedef struct  SION_FILE_INFO
{
   FILE *a_file;
   FILE *b_file;
} SION_FILE_INFO_T;

#define MAX_B_LINE_LEN 80
#define MAX_HEADER_ATTS 10

#if defined(__cplusplus)
extern "C" {
#endif

   extern int SION_open(const char *path, int mode, int basepe, size_t *chunksizehintp,
                        int use_parallel, void* parameters, NC_Dispatch*, NC*);

   extern int SION_close(int ncid);

   extern int SION_inq_format(int ncid, int *formatp);

   extern int SION_inq_format_extended(int ncid, int *formatp, int *modep);

   extern int SION_get_vara(int ncid, int varid, const size_t *start, const size_t *count,
                            void *value, nc_type);

   extern int ab_set_log_level(int new_level);

#if defined(__cplusplus)
}
#endif

#endif /*_SIONDISPATCH_H */
