/* Test read of AB format with netCDF. 
*
* Ed Hartnett */

#include <config.h>
#include <netcdf.h>
#include "siondispatch.h"
#include <nc4dispatch.h>
#include <stdio.h>
#include <math.h>

#define TEST_FILE "surtmp_100l.b"
   
extern NC_Dispatch SION_dispatcher;

int
main()
{
   /* int ncid; */
   int ret;
   
   printf("\nTesting AB format dispatch layer...");
   if ((ret = nc_def_user_format(NC_UF0, &SION_dispatcher, NULL)))
      return ret;
   /* ab_set_log_level(5); */
   nc_set_log_level(3);

   /* if ((ret = nc_open(TEST_FILE, NC_UF0, &ncid))) */
   /*    return ret; */

   /* if ((ret = nc_close(ncid))) */
   /*    return ret; */

   printf("SUCCESS!\n");
   return 0;
}

