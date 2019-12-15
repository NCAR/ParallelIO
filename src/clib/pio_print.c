#include "pio_config.h"
#include "pio.h"
#include "pio_internal.h"

const char *pio_iotype_to_string(int iotype)
{
  switch(iotype){
    case PIO_IOTYPE_PNETCDF:
                              return "PIO_IOTYPE_PNETCDF";
    case PIO_IOTYPE_NETCDF:
                              return "PIO_IOTYPE_NETCDF";
    case PIO_IOTYPE_NETCDF4C:
                              return "PIO_IOTYPE_NETCDF4C";
    case PIO_IOTYPE_NETCDF4P:
                              return "PIO_IOTYPE_NETCDF4P";
    case PIO_IOTYPE_ADIOS:
                              return "PIO_IOTYPE_ADIOS";
    default:
                              return "UNKNOWN";
  }
}

const char *pio_eh_to_string(int eh)
{
  switch(eh){
    case PIO_INTERNAL_ERROR:
                              return "PIO_INTERNAL_ERROR";
    case PIO_BCAST_ERROR:
                              return "PIO_BCAST_ERROR";
    case PIO_REDUCE_ERROR:
                              return "PIO_REDUCE_ERROR";
    case PIO_RETURN_ERROR:
                              return "PIO_RETURN_ERROR";
    default:
                              return "UNKNOWN";
  }
}

const char *pio_rearr_comm_type_to_string(int comm_type)
{
  switch(comm_type){
    case PIO_REARR_COMM_P2P:
                              return "PIO_REARR_COMM_P2P";
    case PIO_REARR_COMM_COLL:
                              return "PIO_REARR_COMM_COLL";
    default:
                              return "UNKNOWN";
  }
}

const char *pio_get_fname_from_file(file_desc_t *file)
{
  return (file ? (file->fname) : "UNKNOWN");
}

const char *pio_get_fname_from_file_id(int pio_file_id)
{
  file_desc_t *file = NULL;
  int ret = PIO_NOERR;

  ret = pio_get_file(pio_file_id, &file);
  if(ret == PIO_NOERR){
    return pio_get_fname_from_file(file);
  }
  else{
    return "UNKNOWN";
  }
}

const char *pio_get_vname_from_file(file_desc_t *file, int varid)
{
  return ( (file && (varid >= 0) && (varid < PIO_MAX_VARS)) ? file->varlist[varid].vname : ( (varid == PIO_GLOBAL) ? "PIO_GLOBAL" : "UNKNOWN") );
}

const char *pio_get_vname_from_file_id(int pio_file_id, int varid)
{
  file_desc_t *file = NULL;
  int ret = PIO_NOERR;

  ret = pio_get_file(pio_file_id, &file);
  if(ret == PIO_NOERR){
    return pio_get_vname_from_file(file, varid);
  }
  else{
    return "UNKNOWN";
  }
}

const char *pio_get_vnames_from_file(file_desc_t *file,
              int *varids, int varids_sz, char *buf, size_t buf_sz)
{
  if(!file || !varids || !buf || (varids_sz <= 0) || (buf_sz <= 0)){
    return "UNKNOWN";
  }

  char *pbuf = buf;
  size_t cur_buf_sz = buf_sz;

  for(int i = 0; i < varids_sz - 1; i++){
    if(cur_buf_sz <= 0){
      break;
    }
    snprintf(pbuf, cur_buf_sz, "%s, ", pio_get_vname_from_file(file, varids[i]));
    pbuf = buf + strlen(buf);
    cur_buf_sz = buf_sz - strlen(buf);
  }
  snprintf(pbuf, cur_buf_sz, "%s", pio_get_vname_from_file(file, varids[varids_sz - 1]));

  return (const char *)buf;
}

const char *pio_get_vnames_from_file_id(int pio_file_id,
              int *varids, int varids_sz, char *buf, size_t buf_sz)
{
  file_desc_t *file = NULL;
  int ret = PIO_NOERR;

  ret = pio_get_file(pio_file_id, &file);
  if(ret == PIO_NOERR){
    return pio_get_vnames_from_file(file, varids, varids_sz, buf, buf_sz);
  }
  else{
    return "UNKNOWN";
  }
}
