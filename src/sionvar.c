/**
 * @file @internal Data read functions for the AB dispatch layer.
 *
 * @author Ed Hartnett
 */

#include <nc4internal.h>
#include "nc4dispatch.h"
#include "siondispatch.h"

/**
 * @internal Get coordinate variable data. AB Format coordinate
 * variables are always NC_FLOAT32.
 *
 * @param ncid File ID.
 * @param varid Variable ID.
 * @param startp Array of start indicies.
 * @param countp Array of counts.
 * @param mem_nc_type The type of these data after it is read into memory.
 * @param is_long Ignored for HDF4.
 * @param data pointer that gets the data.
 * @param memtype The type of these data after it is read into memory.
 *
 * @returns ::NC_NOERR for success
 * @returns ::NC_ERANGE Range error when converting data.
 * @author Ed Hartnett
 */
static int
get_ab_coord_vara(NC *nc, int ncid, int varid, const size_t *startp,
                  const size_t *countp, void *data, int memtype)
{
   NC_GRP_INFO_T *grp;
   NC_HDF5_FILE_INFO_T *h5;
   NC_VAR_INFO_T *var;
   NC_ATT_INFO_T *att = NULL;
   int range_error = 0;
   /* int i = 0; */
   int ret;

   /* Check inputs. */
   assert(nc);

   /* Find our metadata for this file, group, and var. */
   if ((ret = nc4_find_g_var_nc(nc, ncid, varid, &grp, &var)))
      return ret;
   h5 = (NC_HDF5_FILE_INFO_T *)(nc)->dispatchdata;
   assert(grp && h5 && var && var->name && var->ndims == 1);

   /* Coordinate data is stored in variable attribute. */
   if ((ret = nc4_find_grp_att(grp, 1, TIME_NAME, 0, &att))) 
      return ret;
   assert(att);
   float fdata[att->len];
   memcpy(fdata, att->data, att->len * sizeof(float));

   /* If NC_FLOAT is requested, just copy the data. Otherwise, do type
    * conversion - note that NC_ERANGE may result.*/
   if (memtype == NC_FLOAT)
   {
      memcpy(data, &((float *)att->data)[startp[0]], countp[0] * sizeof(float));
   }
   else
   {
      if ((ret = nc4_convert_type(att->data + startp[0] * sizeof(float), data, NC_FLOAT,
                                  memtype, countp[0] - startp[0], &range_error,
                                  NULL, 0, 0, 0)))
         return ret;
   }

   /* As per netCDF rules, data are converted even if range errors
    * occur. But the function returns an error code for this, which
    * may be ignored by caller. */
   if (range_error)
      return NC_ERANGE;
   
   return NC_NOERR;
}


/**
 * Round up to a multiple of a number. According to Alan Wallcraft: 
 * "fin*.a is assumed to contain idm*jdm 32-bit IEEE real values for
 * each array, in standard f77 element order, followed by padding
 * to a multiple of 4096 32-bit words."
 *
 * @param num Number to round up.
 * @param multiple Multiple to round to.
 *
 * @return the rounded number.
 */
static int
round_up(int num, int multiple)
{
   if (multiple == 0)
      return num;
   
   int remainder = num % multiple;
   if (remainder == 0)
      return num;
   
   return num + multiple - remainder;
}

/**
 * @internal Reverse endianness of a float.
 *
 * @param fin The input float.
 *
 * @return the reversed float.
 */
static float
reverse_float(const float fin)
{
   float val;
   char *in = (char *)&fin;
   char *out = (char *)&val;
   
   for (int b = 0; b < sizeof(float); b++)
      out[b] = in[sizeof(float) - b - 1];
   
   return val;
}

/** 
 * @internal Reverse an array of floats.
 *
 * @param bufr_in Pointer to the start of buffer of floats to convert.
 * @param bufr_out Pointer that will get reversed floats.
 * @param num Number of floats to convert.
 *
 * @return ::NC_NOERR No error.
 * @author Ed Hartnett
 */
static int
reverse_floats(float *bufr_in, float *bufr_out, size_t num)
{

   float *in = bufr_in;
   float *out = bufr_out;
   for (int n = 0; n < num; n++)
      *out++ = reverse_float(*in++);

   return NC_NOERR;
}

/**
 * Read an array of values. This is called by nc_get_vara() for
 * netCDF-4 files, as well as all the other nc_get_vara_*
 * functions. HDF4 files are handled as a special case.
 *
 * @param ncid File ID.
 * @param varid Variable ID.
 * @param startp Array of start indicies.
 * @param countp Array of counts.
 * @param ip pointer that gets the data.
 * @param memtype The type of these data after it is read into memory.

 * @returns ::NC_NOERR for success
 * @author Ed Hartnett, Dennis Heimbigner
 */
int
SION_get_vara(int ncid, int varid, const size_t *startp,
              const size_t *countp, void *ip, int memtype)
{
   NC *nc;
   NC_GRP_INFO_T *grp;
   NC_HDF5_FILE_INFO_T *h5;
   NC_VAR_INFO_T *var;
   SION_FILE_INFO_T *ab_file;   
   int i_len, j_len;
   int ret;

   LOG((2, "%s: ncid 0x%x varid %d memtype %d", __func__, ncid, varid,
        memtype));

   /* Find file info. */
   if (!(nc = nc4_find_nc_file(ncid, &h5)))
      return NC_EBADID;
   assert(nc && h5);

   /* Get the AB format metadata for this file. */
   ab_file = h5->format_file_info;   

   /* Find our netcdf metadata for this file, group, and var. */
   if ((ret = nc4_find_g_var_nc(nc, ncid, varid, &grp, &var)))
      return ret;
   assert(grp && h5 && var && var->name);

   /* Coordinate var is handled specially. */
   if (!strcmp(var->name, TIME_NAME))
      return get_ab_coord_vara(nc, ncid, varid, startp, countp, ip, memtype);

   /* Find the dimension sizes. */
   for (int d = 0; d < var->ndims; d++)
      LOG((3, "d %d var->dim[d]->name %s", d, var->dim[d]->name));
   j_len = var->dim[1]->len;
   i_len = var->dim[2]->len;

   /* Size of a record. */
   size_t rec_len = round_up(j_len * i_len, 4096) * sizeof(float);
   /* size_t num_elem = j_len * i_len; */

   /* Find each requested record. */
   for (int rec = 0; rec < countp[0]; rec++)
   {
      long rec_pos = startp[0] * rec_len;
      for (int j = 0; j < countp[1]; j++)
      {
         float *bufr;

         rec_pos += i_len * (startp[1] + j) * sizeof(float);
         if (!(bufr = malloc(countp[2] * sizeof(float))))
            return NC_ENOMEM;
         
         LOG((3, "rec %d j %d rec_pos %d rec_len %d", rec, j, rec_pos, rec_len));
         if (fseek(ab_file->a_file, rec_pos, SEEK_SET))
            return NC_EIO;
         LOG((3, "ftell %d", ftell(ab_file->a_file)));
         if ((fread(bufr, sizeof(float), countp[2], ab_file->a_file) != countp[2]))
            return NC_EIO;
         if ((ret = reverse_floats(bufr, ip, countp[2])))
            return ret;
         ip = (float *)ip + countp[2];
         free(bufr);
      }
   }

   return NC_NOERR;
}
