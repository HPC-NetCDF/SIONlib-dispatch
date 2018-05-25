/**
 * @file
 * @internal The AB file functions.
 *
 * @author Ed Hartnett
 */

#include "config.h"
#include <errno.h>  /* netcdf functions sometimes return system errors */
#include "nc.h"
#include "nc4internal.h"
#include "siondispatch.h"
#include <strings.h>
#include <math.h>
#include <libgen.h>

#define SION_DIMSIZE_STRING "i/jdm ="
#define SION_MAX_DIM_DIGITS 10
#define UNITS_NAME "units"
#define PNAME_NAME "long_name"
#define SNAME_NAME "standard_name"
#define CONVENTIONS "Conventions"
#define CF_VERSION "CF-1.0"
   
extern int nc4_vararray_add(NC_GRP_INFO_T *grp, NC_VAR_INFO_T *var);

/** @internal These flags may not be set for open mode. */
static const int ILLEGAL_OPEN_FLAGS = (NC_MMAP|NC_64BIT_OFFSET|NC_MPIIO|NC_MPIPOSIX|NC_DISKLESS);

static void
trim(char *s)
{
    char *p = s;
    int l = strlen(p);

    while(isspace(p[l - 1]))
       p[--l] = 0;
    while(*p && isspace(*p))
       ++p, --l;

    memmove(s, p, l + 1);
}   

/**
 * @internal Parse the B file for metadata info.
 *
 * @param h5 Pointer to file info.
 * @param num_header_atts Pointer that gets the number of header
 * attributes.
 * @param header_att Pointer to an array of fixed size which gets the
 * header atts.
 * @param var_name Pointer that gets variable name.
 * @param t_len Pointer that gets length of time dimension.
 * @param i_len Pointer that gets length of the i dimension.
 * @param j_len Pointer that gets length of the j dimension.
 * @param time Pointer to a pointer that gets array of time values,
 * of length t_len. Must be freed by caller.
 * @param span Pointer to a pointer that gets array of span values,
 * of length t_len. Must be freed by caller.
 * @param min Pointer to a pointer that gets array of minimum values,
 * of length t_len. Must be freed by caller.
 * @param max Pointer to a pointer that gets array of maximum values,
 * of length t_len. Must be freed by caller.
 *
 * @author Ed Hartnett
 */
static int
parse_b_file(NC_HDF5_FILE_INFO_T *h5, int *num_header_atts,
             char header_att[MAX_HEADER_ATTS][MAX_B_LINE_LEN],
             char *var_name, int *t_len, int *i_len, int *j_len, float **time,
             float **span, float **min, float **max)
{
   SION_FILE_INFO_T *ab_file;
   char line[MAX_B_LINE_LEN + 1];
   int header = 1;
   int time_start_pos = 0;
   int time_count = 0;

   /* Check inputs. */
   assert(h5 && h5->format_file_info && t_len && i_len && j_len && num_header_atts
          && header_att && var_name && time && span && min && max);

   /* Get the AB-specific file metadata. */
   ab_file = h5->format_file_info;
   assert(ab_file->b_file);

   /* Start record and header atts count at zero. */
   *t_len = 0;
   *num_header_atts = 0;

   /* Read the B file line by line. */
   while(fgets(line, sizeof(line), ab_file->b_file))
   {
      /* Is this line blank? */
      int blank = 1;
      for (int p = 0; p < strlen(line); p++)
         if (!isspace(line[p]))
         {
            blank = 0;
            break;
         }

      /* Skip blank lines. */
      if (blank)
         continue;

      /* Have we reached last line of header? */
      if (!(strncmp(line, SION_DIMSIZE_STRING, sizeof(SION_DIMSIZE_STRING) - 1)))
      {
         char *tok = line;
         char i_val[SION_MAX_DIM_DIGITS + 1];
         char j_val[SION_MAX_DIM_DIGITS + 1];
         int tok_count = 0;

         /* Get the i/j values. */
         while ((tok = strtok(tok, " ")) != NULL)
         {
            if (tok_count == 2)
               strncpy(i_val, tok, SION_MAX_DIM_DIGITS);
            if (tok_count == 3)
               strncpy(j_val, tok, SION_MAX_DIM_DIGITS);
            tok_count++;
            tok = NULL;
         }
         LOG((3, "i_val %s j_val %s", i_val, j_val));
         sscanf(i_val, "%d", i_len);
         sscanf(j_val, "%d", j_len);

         /* Remember we are done with header. */
         header = 0;
      }
      
      if (header)
      {
         LOG((3, "header = %d %s", header, line));
         if (*num_header_atts < MAX_HEADER_ATTS)
         {
            char hdr[MAX_B_LINE_LEN + 1];
            /* Lose last char - a line feed. */
            strncpy(hdr, line, strlen(line) - 1);
            trim(hdr);
            LOG((3, "hdr %s!", hdr));
            strncpy(header_att[*num_header_atts], hdr, MAX_B_LINE_LEN);
            (*num_header_atts)++;
         }
      }
      else
      {
         if (!time_start_pos)
            time_start_pos = ftell(ab_file->b_file);
         (*t_len)++;
      }
   }
   (*t_len)--;

   /* Allocate storage for the time, span, min, and max values. */
   if (!(*time = malloc(*t_len * sizeof(float))))
      return NC_ENOMEM;
   if (!(*span = malloc(*t_len * sizeof(float))))
      return NC_ENOMEM;
   if (!(*min = malloc(*t_len * sizeof(float))))
      return NC_ENOMEM;
   if (!(*max = malloc(*t_len * sizeof(float))))
      return NC_ENOMEM;

   /* Now go back and get the time info. */
   fseek(ab_file->b_file, time_start_pos, SEEK_SET);
   while(fgets(line, sizeof(line), ab_file->b_file))
   {
      char *tok = line;
      int tok_count = 0;
      int var_named = 0;

      /* Is this line blank? */
      int blank = 1;
      for (int p = 0; p < strlen(line); p++)
         if (!isspace(line[p]))
         {
            blank = 0;
            break;
         }

      /* Skip blank lines. */
      if (blank)
         continue;

      /* Get the time values. */
      while ((tok = strtok(tok, " ")) != NULL)
      {
         LOG((3, "tok_count %d tok %s", tok_count, tok));
         if (tok_count == 0 && !var_named)
         {
            strncpy(var_name, tok, strlen(tok) - strlen(index(tok, ':')));
            var_named++;
         }
         else if (tok_count == 3)
         {
            sscanf(tok, "%f", &(*time)[time_count]);
         }
         else if (tok_count == 4)
         {
            sscanf(tok, "%f", &(*span)[time_count]);
         }
         else if (tok_count == 5)
         {
            sscanf(tok, "%f", &(*min)[time_count]);
         }
         else if (tok_count == 6)
         {
            sscanf(tok, "%f", &(*max)[time_count]);
         }
         
         tok_count++;
         tok = NULL;
      }
      time_count++;
      LOG((3, "%s", line));
   }
   
   return NC_NOERR;
}

/**
 * @internal Add an attribute to the netCDF-4 internal data model.
 *
 * @param h5 Pointer to the netCDF-4 file metadata.
 * @param var Pointer to the netCDF-4 variable metadata. NULL for
 * global attributes.
 * @param name Name of the attribute.
 * @param xtype Type of the attribute.
 * @param len Number of elements in the attribute array.
 * @param op Pointer to attribute array data array of length len and
 * type xtype.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_ENOMEM Out of memory.
 * @author Ed Hartnett
 */
static int
nc4_put_att(NC_HDF5_FILE_INFO_T *h5, NC_VAR_INFO_T *var, char *name, nc_type xtype,
            size_t len, const void *op)
{
   NC_ATT_INFO_T *att;
   NC_ATT_INFO_T **attlist;
   size_t type_size;
   int ret;

   /* Check inputs. */
   assert(h5 && name);
   if (strlen(name) > NC_MAX_NAME)
      return NC_EMAXNAME;

   /* Choose the attribute list to add to, a variable or the global
    * list. */
   attlist = var ? &var->att : &h5->root_grp->att;

   /* Add to the end of the list of atts. */
   if ((ret = nc4_att_list_add(attlist, &att)))
      return ret;
   att->attnum = var ? var->natts++ : h5->root_grp->natts++;
   att->created = NC_TRUE;
      
   /* Add attribute metadata. */
   if (!(att->name = strndup(name, NC_MAX_NAME)))
      return NC_ENOMEM;
   att->nc_typeid = xtype;
   att->len = len;
   LOG((4, "att->name %s att->nc_typeid %d att->len %d", att->name,
        att->nc_typeid, att->len));

   /* Find the size of the type. */
   if ((ret = nc4_get_typelen_mem(h5, xtype, 0, &type_size)))
      return ret;
   LOG((3, "type_size %d", type_size));
      
   /* Allocate memory to hold the data. */
   if (!(att->data = malloc(type_size * att->len)))
      return NC_ENOMEM;
      
   /* Copy the attribute data. */
   memcpy(att->data, op, att->len * type_size);
   return NC_NOERR;
}

/**
 * @internal Add global attributes for the AB file.
 *
 * @param h5 Pointer to file info.
 * @param num_header_atts The number of global attributes to add.
 * @param header_att Fixed size array with global atts.
 *
 * @return NC_NOERR No error.
 * @return NC_ENOMEM Out of memory.
 * @author Ed Hartnett
 */
static int
add_ab_global_atts(NC_HDF5_FILE_INFO_T *h5, int num_header_atts,
                   char header_att[MAX_HEADER_ATTS][MAX_B_LINE_LEN])
{
   int ret;

   /* One attribute for each header record in the B file. */
   for (int a = 0; a < num_header_atts; a++)
   {
      char att_name[NC_MAX_NAME + 1];
      
      /* Come up with a name. */
      sprintf(att_name, "att_%d", a);

      /* Put the att in the metadata. */
      if ((ret = nc4_put_att(h5, NULL, att_name, NC_CHAR, strlen(header_att[a]),
                             header_att[a])))
         return ret;
   }

   /* Some attributes from force2nc.f. */
   if ((ret = nc4_put_att(h5, NULL, CONVENTIONS, NC_CHAR, strlen(CF_VERSION),
                          CF_VERSION)))
      return ret;
   
   return NC_NOERR;
}

/**
 * @internal Add dimensions for the AB file.
 *
 * @param h5 Pointer to file info.
 * @param dim Pointer to array of NC_DIM_INFO_T pointers.
 * @param dim_len Pointer to array of lengths of the dims.
 *
 * @author Ed Hartnett
 */
static int
add_ab_dims(NC_HDF5_FILE_INFO_T *h5, NC_DIM_INFO_T **dim, int *dim_len)
{
   int ret;
   char dim_name[SION_NDIMS3][NC_MAX_NAME + 1] = {TIME_NAME, J_NAME, I_NAME};
   
   for (int d = 0; d < SION_NDIMS3; d++)
   {
      if ((ret = nc4_dim_list_add(&h5->root_grp->dim, &dim[d])))
         return ret;
      if (!(dim[d]->name = strndup(dim_name[d], NC_MAX_NAME)))
         return NC_ENOMEM;
      dim[d]->dimid = h5->root_grp->nc4_info->next_dimid++;
      dim[d]->hash = hash_fast(dim_name[d], strlen(dim_name[d]));
      dim[d]->len = dim_len[d];
   }

   return NC_NOERR;
}

/**
 * @internal Add a variable to the metadata structures.
 *
 * @param h5 Pointer to file info.
 * @param var Pointer to the variable.
 * @param var_name Pointer that gets variable name.
 * 
 * @return NC_NOERR No error.
 * @return NC_ENOMEM Out of memory.
 * @return NC_EINVAL Invalid input.
 * @author Ed Hartnett
 */
static int
add_ab_var(NC_HDF5_FILE_INFO_T *h5, NC_VAR_INFO_T **varp, char *var_name,
           nc_type xtype, int ndims, const int *dimids, int use_fill_value)
{
   NC_VAR_INFO_T *var;   
   int ret;

   /* Check inputs. */
   assert(h5 && varp && var_name);
   if (ndims < 0)
      return NC_EINVAL;
   if (ndims && !dimids)
      return NC_EINVAL;
   
   /* Create and init a variable metadata struct for the data variable. */
   if ((ret = nc4_var_add(varp)))
      return ret;
   var = *varp;
   var->varid = h5->root_grp->nvars++;
   var->created = NC_TRUE;
   var->written_to = NC_TRUE;

   /* Add the var to the variable array, growing it as needed. */
   if ((ret = nc4_vararray_add(h5->root_grp, var)))
      return ret;

   /* Remember var name. */
   if (!(var->name = strndup(var_name, NC_MAX_NAME)))
      return NC_ENOMEM;

   /* Create hash for names for quick lookups. */
   var->hash = hash_fast(var->name, strlen(var->name));

   /* Fill special type_info struct for variable type information. */
   if (!(var->type_info = calloc(1, sizeof(NC_TYPE_INFO_T)))) 
      return NC_ENOMEM;
   var->type_info->nc_typeid = xtype;

   /* Indicate that the variable has a pointer to the type */
   var->type_info->rc++;

   /* Get the size of the type. */
   if ((ret = nc4_get_typelen_mem(h5, var->type_info->nc_typeid, 0,
                                     &var->type_info->size))) 
      return ret;

   /* Allocate storage for the fill value. */
   if (use_fill_value)
   {
      if (!(var->fill_value = malloc(var->type_info->size))) 
         return NC_ENOMEM;
      *((float *)var->fill_value) = powf(2, 100);
   }
   
   /* AB files are always contiguous. */
   var->contiguous = NC_TRUE;

   /* Store dimension info in this variable. */
   var->ndims = ndims;
   if (!(var->dim = malloc(sizeof(NC_DIM_INFO_T *) * var->ndims))) 
      return NC_ENOMEM;
   if (!(var->dimids = malloc(sizeof(int) * var->ndims))) 
      return NC_ENOMEM;
   for (int d = 0; d < var->ndims; d++)
   {
      NC_DIM_INFO_T *dim;
      NC_GRP_INFO_T *dim_grp;
      var->dimids[d] = dimids[d];
      if ((ret = nc4_find_dim(h5->root_grp, dimids[d], &dim, &dim_grp)))
         return ret;
      var->dim[d] = dim;
   }
   
   return NC_NOERR;
}

/**
 * @internal Use the name of the variable to determine some attribute
 * values. These values are from
 * hycom/ALL/force/src_2.1.27/force2nc.f.
 *
 * @param var_name Variable name
 * @param pname Pointer to storage that gets the long_name.
 * @param sname Pointer to storage that gets the standard name.
 * @param units Pointer to storage that gets the units.
 *
 * @return ::NC_NOERR No error.
 * @author Ed Hartnett.
 */
static int
ab_find_var_atts(char *var_name, char *pname, char *sname, char *units)
{
#define NUM_ENTRIES 10
   struct ab_att
   {
      char var_name[NC_MAX_NAME + 1];
      char pname[NC_MAX_NAME + 1];
      char sname[NC_MAX_NAME + 1];
      char units[NC_MAX_NAME + 1];
   };
   struct ab_att dict[NUM_ENTRIES] = {
      {"radflx", " surf. rad. flux ", "surface_net_downward_radiation_flux", "w/m2"},
      {"shwflx", " surf. shw. flux  ", "surface_net_downward_shortwave_flux", "w/m2"},
      {"vapmix", " vapor mix. ratio ", "specific_humidity", "kg/kg"},
      {"airtmp", " air temperature  ", "air_temperature", "degC"},
      {"surtmp", " sea surf. temp.  ", "sea_surface_temperature", "degC"},
      {"seatmp", " sea surf. temp.  ", "sea_surface_temperature", "degC"},
      {"precip", " precipitation    ", "lwe_precipitation_rate", "m/s"},
      {"wndspd", " 10m wind speed   ", "wind_speed", "m/s"},
      {"tauewd", " Ewd wind stress  ", "eastward_wind_stress", "N/m^2"},
      {"taunwd", " Nwd wind stress  ", "northward_wind_stress", "N/m^2"}
   };

   for (int i = 0; i < NUM_ENTRIES; i++)
   {
      if (!strcmp(var_name, dict[i].var_name))
      {
         strncpy(pname, dict[i].pname, NC_MAX_NAME);
         strncpy(sname, dict[i].sname, NC_MAX_NAME);
         strncpy(units, dict[i].units, NC_MAX_NAME);
      }
   }
   return NC_NOERR;
}

/**
 * @internal Add attributes to an AB variable.
 *
 * @param h5 Pointer to file info.
 * @param var Pointer to the variable.
 * @param t_len Length of time dimension, and all variable attribute
 * arrays.
 * @param 
 * @param var_name Pointer that gets variable name.
 * 
 * @return NC_NOERR No error.
 * @return NC_ENOMEM Out of memory.
 * @author Ed Hartnett
 */
static int
add_ab_var_atts(NC_HDF5_FILE_INFO_T *h5, NC_VAR_INFO_T *var, int t_len,
                float *time, float *span, float *min, float *max)
{
   char att_name[NUM_SION_VAR_ATTS][NC_MAX_NAME + 1] = {TIME_NAME, SPAN_NAME,
                                                      MIN_NAME, MAX_NAME};
   float *att_data[NUM_SION_VAR_ATTS] = {time, span, min, max};
   char pname[NC_MAX_NAME + 1] = "";
   char sname[NC_MAX_NAME + 1] = "";
   char units[NC_MAX_NAME + 1] = "";
   int ret;

   /* Check inputs. */
   assert(h5 && var && t_len > 0 && time && span && min && max);
   LOG((2, "%s", __func__));

   /* Put the four float array attributes. */
   for (int a = 0; a < NUM_SION_VAR_ATTS; a++)
      if ((ret = nc4_put_att(h5, var, att_name[a], NC_FLOAT, t_len, att_data[a])))
         return ret;

   if ((ret = ab_find_var_atts(var->name, pname, sname, units)))
      return ret;
   LOG((3, "var->name %s pname %s sname %s units %s", var->name, pname, sname, units));

   /* Write other atts if known. */
   if (strlen(pname))
      if ((ret = nc4_put_att(h5, var, PNAME_NAME, NC_CHAR, strlen(pname), pname)))
         return ret;

   if (strlen(sname))
      if ((ret = nc4_put_att(h5, var, SNAME_NAME, NC_CHAR, strlen(sname), sname)))
         return ret;

   if (strlen(units))
      if ((ret = nc4_put_att(h5, var, UNITS_NAME, NC_CHAR, strlen(units), units)))
         return ret;

   return NC_NOERR;
}

/**
 * @internal Open an AB format file. The .b file should be given as
 * the path. A matching .a file will be expected in the same
 * directory.
 *
 * @param path The file name of the new file.
 * @param mode The open mode flag.
 * @param nc Pointer that gets the NC file info struct.
 *
 * @return ::NC_NOERR No error.
 * @return NC_ENOMEM Out of memory.
 * @author Ed Hartnett
 */
static int
ab_open_file(const char *path, int mode, NC *nc)
{
   NC_HDF5_FILE_INFO_T *h5;
   NC_VAR_INFO_T *var;
   NC_VAR_INFO_T *time_var;
   NC_DIM_INFO_T *dim[SION_NDIMS3];
   SION_FILE_INFO_T *ab_file;
   char *a_path;
   char *dot_loc;
   int num_header_atts;
   char header_att[MAX_HEADER_ATTS][MAX_B_LINE_LEN];
   int t_len, i_len, j_len;
   float *time;
   float *span;
   float *min;
   float *max;
   char var_name[NC_MAX_NAME + 1];
   int dimids[SION_NDIMS3] = {0, 1, 2};
   int time_dimid = 0;
   int ret;

   /* Check inputs. */
   assert(nc && path);
   LOG((1, "%s: path %s mode %d", __func__, path, mode));

   /* B file name must end in .b. */
   if (!(dot_loc = rindex(path, '.')))
      return NC_EINVAL;
   if (strcmp(dot_loc, ".b"))
      return NC_EINVAL;

   /* Get the A file name. */
   if (!(a_path = strdup(path)))
      return NC_ENOMEM;
   a_path[strlen(path) - 1] = 'a';
   
   /* Add necessary structs to hold file metadata. */
   if ((ret = nc4_nc4f_list_add(nc, path, mode)))
      return ret;
   h5 = (NC_HDF5_FILE_INFO_T *)nc->dispatchdata;
   assert(h5 && h5->root_grp);
   h5->no_write = NC_TRUE;
   h5->root_grp->nc4_info->controller = nc;

   /* Allocate data to hold AB specific file data. */
   if (!(ab_file = malloc(sizeof(SION_FILE_INFO_T))))
      return NC_ENOMEM;
   h5->format_file_info = ab_file;

   /* Open the A file. */
   LOG((3, "a_file path %s", a_path));
   if (!(ab_file->a_file = fopen(a_path, "r")))
      return NC_EIO;

   /* Open the B file. */
   if (!(ab_file->b_file = fopen(path, "r")))
      return NC_EIO;

   /* Parse the B file. */
   if ((ret = parse_b_file(h5, &num_header_atts, header_att, var_name, &t_len,
                              &i_len, &j_len, &time, &span, &min, &max)))
      return ret;
   LOG((3, "num_header_atts %d var_name %s t_len %d i_len %d j_len %d",
        num_header_atts, var_name, t_len, i_len, j_len));

   for (int h = 0; h < num_header_atts; h++)
   {
      LOG((3, "h %d header_att %s!", h, header_att[h]));
   }
   for (int t = 0; t < t_len; t++)
   {
      LOG((3, "t %d time %f span %f min %f max %f", t, time[t], span[t],
           min[t], max[t]));
   }

   /* Add the global attributes. */
   if ((ret = add_ab_global_atts(h5, num_header_atts, header_att)))
      return ret;

   /* Add the dimensions. */
   int dim_lens[SION_NDIMS3] = {t_len, j_len, i_len};
   if ((ret = add_ab_dims(h5, dim, dim_lens)))
      return ret;

   /* Add the coordinate variable. */
   if ((ret = add_ab_var(h5, &time_var, TIME_NAME, NC_FLOAT, SION_NDIMS1, &time_dimid, 0)))
      return ret;

   /* Add the data variable. */
   if ((ret = add_ab_var(h5, &var, var_name, NC_FLOAT, SION_NDIMS3, dimids, 1)))
      return ret;

   /* Variable attributes. */
   if ((ret = add_ab_var_atts(h5, var, t_len, time, span, min, max)))
      return ret;
   
   /* Free resources. */
   free(a_path);
   free(time);
   free(span);
   free(min);
   free(max);

#ifdef LOGGING
   /* This will print out the names, types, lens, etc of the vars and
      atts in the file, if the logging level is 2 or greater. */
   log_metadata_nc(h5->root_grp->nc4_info->controller);
#endif
   return NC_NOERR;
}

/**
 * @internal Open a AB file.
 *
 * @param path The file name of the file.
 * @param mode The open mode flag.
 * @param basepe Ignored by this function.
 * @param chunksizehintp Ignored by this function.
 * @param use_parallel Must be 0 for sequential, access. Parallel
 * access not supported for AB.
 * @param parameters pointer to struct holding extra data (e.g. for
 * parallel I/O) layer. Ignored if NULL.
 * @param dispatch Pointer to the dispatch table for this file.
 * @param nc_file Pointer to an instance of NC.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EINVAL Invalid input.
 * @author Ed Hartnett
 */
int
SION_open(const char *path, int mode, int basepe, size_t *chunksizehintp,
          int use_parallel, void *parameters, NC_Dispatch *dispatch,
          NC *nc_file)
{
   assert(nc_file && path);

   LOG((1, "%s: path %s mode %d params %x", __func__, path, mode,
        parameters));

   /* Check inputs. */
   assert(path && !use_parallel);

   /* Check the mode for validity */
   if (mode & ILLEGAL_OPEN_FLAGS)
      return NC_EINVAL;

   /* We don't maintain a separate internal ncid for AB format. */
   nc_file->int_ncid = nc_file->ext_ncid;

   /* Open the file. */
   return ab_open_file(path, mode, nc_file);
}

/**
 * @internal Close the AB file.
 *
 * @param ncid File ID.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @author Ed Hartnett
 */
int
SION_close(int ncid)
{
   NC_GRP_INFO_T *grp;
   NC *nc;
   NC_HDF5_FILE_INFO_T *h5;
   SION_FILE_INFO_T *ab_file;
   int ret;

   LOG((1, "%s: ncid 0x%x", __func__, ncid));

   /* Find our metadata for this file. */
   if ((ret = nc4_find_nc_grp_h5(ncid, &nc, &grp, &h5)))
      return ret;
   assert(nc && h5 && h5->format_file_info);

   /* Get the AB specific info. */
   ab_file = h5->format_file_info;

   /* Close the A/B files. */
   fclose(ab_file->a_file);
   fclose(ab_file->b_file);

   /* Free AB file info struct. */
   free(h5->format_file_info);

   /* Delete all the list contents for vars, dims, and atts, in each
    * group. */
   if ((ret = nc4_rec_grp_del(&h5->root_grp, h5->root_grp)))
      return ret;

   /* Free the nc4_info struct; above code should have reclaimed
      everything else */
   free(h5);

   return NC_NOERR;
}
