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

const char *pio_async_msg_to_string(int msg)
{
  switch(msg){
    case  PIO_MSG_INVALID:
            return "PIO_MSG_INVALID";
    case  PIO_MSG_OPEN_FILE:
            return "PIO_MSG_OPEN_FILE";
    case  PIO_MSG_CREATE_FILE:
            return "PIO_MSG_CREATE_FILE";
    case  PIO_MSG_INQ_ATT:
            return "PIO_MSG_INQ_ATT";
    case  PIO_MSG_INQ_FORMAT:
            return "PIO_MSG_INQ_FORMAT";
    case  PIO_MSG_INQ_VARID:
            return "PIO_MSG_INQ_VARID";
    case  PIO_MSG_DEF_VAR:
            return "PIO_MSG_DEF_VAR";
    case  PIO_MSG_INQ_VAR:
            return "PIO_MSG_INQ_VAR";
    case  PIO_MSG_PUT_ATT_DOUBLE:
            return "PIO_MSG_PUT_ATT_DOUBLE";
    case  PIO_MSG_PUT_ATT_INT:
            return "PIO_MSG_PUT_ATT_INT";
    case  PIO_MSG_RENAME_ATT:
            return "PIO_MSG_RENAME_ATT";
    case  PIO_MSG_DEL_ATT:
            return "PIO_MSG_DEL_ATT";
    case  PIO_MSG_INQ:
            return "PIO_MSG_INQ";
    case  PIO_MSG_GET_ATT_TEXT:
            return "PIO_MSG_GET_ATT_TEXT";
    case  PIO_MSG_GET_ATT_SHORT:
            return "PIO_MSG_GET_ATT_SHORT";
    case  PIO_MSG_PUT_ATT_LONG:
            return "PIO_MSG_PUT_ATT_LONG";
    case  PIO_MSG_REDEF:
            return "PIO_MSG_REDEF";
    case  PIO_MSG_SET_FILL:
            return "PIO_MSG_SET_FILL";
    case  PIO_MSG_ENDDEF:
            return "PIO_MSG_ENDDEF";
    case  PIO_MSG_RENAME_VAR:
            return "PIO_MSG_RENAME_VAR";
    case  PIO_MSG_PUT_ATT_SHORT:
            return "PIO_MSG_PUT_ATT_SHORT";
    case  PIO_MSG_PUT_ATT_TEXT:
            return "PIO_MSG_PUT_ATT_TEXT";
    case  PIO_MSG_INQ_ATTNAME:
            return "PIO_MSG_INQ_ATTNAME";
    case  PIO_MSG_GET_ATT_ULONGLONG:
            return "PIO_MSG_GET_ATT_ULONGLONG";
    case  PIO_MSG_GET_ATT_USHORT:
            return "PIO_MSG_GET_ATT_USHORT";
    case  PIO_MSG_PUT_ATT_ULONGLONG:
            return "PIO_MSG_PUT_ATT_ULONGLONG";
    case  PIO_MSG_GET_ATT_UINT:
            return "PIO_MSG_GET_ATT_UINT";
    case  PIO_MSG_GET_ATT_LONGLONG:
            return "PIO_MSG_GET_ATT_LONGLONG";
    case  PIO_MSG_PUT_ATT_SCHAR:
            return "PIO_MSG_PUT_ATT_SCHAR";
    case  PIO_MSG_PUT_ATT_FLOAT:
            return "PIO_MSG_PUT_ATT_FLOAT";
    case  PIO_MSG_RENAME_DIM:
            return "PIO_MSG_RENAME_DIM";
    case  PIO_MSG_GET_ATT_LONG:
            return "PIO_MSG_GET_ATT_LONG";
    case  PIO_MSG_INQ_DIM:
            return "PIO_MSG_INQ_DIM";
    case  PIO_MSG_INQ_DIMID:
            return "PIO_MSG_INQ_DIMID";
    case  PIO_MSG_PUT_ATT_USHORT:
            return "PIO_MSG_PUT_ATT_USHORT";
    case  PIO_MSG_GET_ATT_FLOAT:
            return "PIO_MSG_GET_ATT_FLOAT";
    case  PIO_MSG_SYNC:
            return "PIO_MSG_SYNC";
    case  PIO_MSG_PUT_ATT_LONGLONG:
            return "PIO_MSG_PUT_ATT_LONGLONG";
    case  PIO_MSG_PUT_ATT_UINT:
            return "PIO_MSG_PUT_ATT_UINT";
    case  PIO_MSG_GET_ATT_SCHAR:
            return "PIO_MSG_GET_ATT_SCHAR";
    case  PIO_MSG_INQ_ATTID:
            return "PIO_MSG_INQ_ATTID";
    case  PIO_MSG_DEF_DIM:
            return "PIO_MSG_DEF_DIM";
    case  PIO_MSG_GET_ATT_INT:
            return "PIO_MSG_GET_ATT_INT";
    case  PIO_MSG_GET_ATT_DOUBLE:
            return "PIO_MSG_GET_ATT_DOUBLE";
    case  PIO_MSG_PUT_ATT_UCHAR:
            return "PIO_MSG_PUT_ATT_UCHAR";
    case  PIO_MSG_GET_ATT_UCHAR:
            return "PIO_MSG_GET_ATT_UCHAR";
    case  PIO_MSG_PUT_VARS_UCHAR:
            return "PIO_MSG_PUT_VARS_UCHAR";
    case  PIO_MSG_GET_VAR1_SCHAR:
            return "PIO_MSG_GET_VAR1_SCHAR";
    case  PIO_MSG_GET_VARS_ULONGLONG:
            return "PIO_MSG_GET_VARS_ULONGLONG";
    case  PIO_MSG_GET_VARM_UCHAR:
            return "PIO_MSG_GET_VARM_UCHAR";
    case  PIO_MSG_GET_VARM_SCHAR:
            return "PIO_MSG_GET_VARM_SCHAR";
    case  PIO_MSG_GET_VARS_SHORT:
            return "PIO_MSG_GET_VARS_SHORT";
    case  PIO_MSG_GET_VAR_DOUBLE:
            return "PIO_MSG_GET_VAR_DOUBLE";
    case  PIO_MSG_GET_VARA_DOUBLE:
            return "PIO_MSG_GET_VARA_DOUBLE";
    case  PIO_MSG_GET_VAR_INT:
            return "PIO_MSG_GET_VAR_INT";
    case  PIO_MSG_GET_VAR_USHORT:
            return "PIO_MSG_GET_VAR_USHORT";
    case  PIO_MSG_PUT_VARS_USHORT:
            return "PIO_MSG_PUT_VARS_USHORT";
    case  PIO_MSG_GET_VARA_TEXT:
            return "PIO_MSG_GET_VARA_TEXT";
    case  PIO_MSG_PUT_VARS_ULONGLONG:
            return "PIO_MSG_PUT_VARS_ULONGLONG";
    case  PIO_MSG_GET_VARA_INT:
            return "PIO_MSG_GET_VARA_INT";
    case  PIO_MSG_PUT_VARM:
            return "PIO_MSG_PUT_VARM";
    case  PIO_MSG_GET_VAR1_FLOAT:
            return "PIO_MSG_GET_VAR1_FLOAT";
    case  PIO_MSG_GET_VAR1_SHORT:
            return "PIO_MSG_GET_VAR1_SHORT";
    case  PIO_MSG_GET_VARS_INT:
            return "PIO_MSG_GET_VARS_INT";
    case  PIO_MSG_PUT_VARS_UINT:
            return "PIO_MSG_PUT_VARS_UINT";
    case  PIO_MSG_GET_VAR_TEXT:
            return "PIO_MSG_GET_VAR_TEXT";
    case  PIO_MSG_GET_VARM_DOUBLE:
            return "PIO_MSG_GET_VARM_DOUBLE";
    case  PIO_MSG_PUT_VARM_UCHAR:
            return "PIO_MSG_PUT_VARM_UCHAR";
    case  PIO_MSG_PUT_VAR_USHORT:
            return "PIO_MSG_PUT_VAR_USHORT";
    case  PIO_MSG_GET_VARS_SCHAR:
            return "PIO_MSG_GET_VARS_SCHAR";
    case  PIO_MSG_GET_VARA_USHORT:
            return "PIO_MSG_GET_VARA_USHORT";
    case  PIO_MSG_PUT_VAR1_LONGLONG:
            return "PIO_MSG_PUT_VAR1_LONGLONG";
    case  PIO_MSG_PUT_VARA_UCHAR:
            return "PIO_MSG_PUT_VARA_UCHAR";
    case  PIO_MSG_PUT_VARM_SHORT:
            return "PIO_MSG_PUT_VARM_SHORT";
    case  PIO_MSG_PUT_VAR1_LONG:
            return "PIO_MSG_PUT_VAR1_LONG";
    case  PIO_MSG_PUT_VARS_LONG:
            return "PIO_MSG_PUT_VARS_LONG";
    case  PIO_MSG_GET_VAR1_USHORT:
            return "PIO_MSG_GET_VAR1_USHORT";
    case  PIO_MSG_PUT_VAR_SHORT:
            return "PIO_MSG_PUT_VAR_SHORT";
    case  PIO_MSG_PUT_VARA_INT:
            return "PIO_MSG_PUT_VARA_INT";
    case  PIO_MSG_GET_VAR_FLOAT:
            return "PIO_MSG_GET_VAR_FLOAT";
    case  PIO_MSG_PUT_VAR1_USHORT:
            return "PIO_MSG_PUT_VAR1_USHORT";
    case  PIO_MSG_PUT_VARA_TEXT:
            return "PIO_MSG_PUT_VARA_TEXT";
    case  PIO_MSG_PUT_VARM_TEXT:
            return "PIO_MSG_PUT_VARM_TEXT";
    case  PIO_MSG_GET_VARS_UCHAR:
            return "PIO_MSG_GET_VARS_UCHAR";
    case  PIO_MSG_GET_VAR:
            return "PIO_MSG_GET_VAR";
    case  PIO_MSG_PUT_VARM_USHORT:
            return "PIO_MSG_PUT_VARM_USHORT";
    case  PIO_MSG_GET_VAR1_LONGLONG:
            return "PIO_MSG_GET_VAR1_LONGLONG";
    case  PIO_MSG_GET_VARS_USHORT:
            return "PIO_MSG_GET_VARS_USHORT";
    case  PIO_MSG_GET_VAR_LONG:
            return "PIO_MSG_GET_VAR_LONG";
    case  PIO_MSG_GET_VAR1_DOUBLE:
            return "PIO_MSG_GET_VAR1_DOUBLE";
    case  PIO_MSG_PUT_VAR_ULONGLONG:
            return "PIO_MSG_PUT_VAR_ULONGLONG";
    case  PIO_MSG_PUT_VAR_INT:
            return "PIO_MSG_PUT_VAR_INT";
    case  PIO_MSG_GET_VARA_UINT:
            return "PIO_MSG_GET_VARA_UINT";
    case  PIO_MSG_PUT_VAR_LONGLONG:
            return "PIO_MSG_PUT_VAR_LONGLONG";
    case  PIO_MSG_GET_VARS_LONGLONG:
            return "PIO_MSG_GET_VARS_LONGLONG";
    case  PIO_MSG_PUT_VAR_SCHAR:
            return "PIO_MSG_PUT_VAR_SCHAR";
    case  PIO_MSG_PUT_VAR_UINT:
            return "PIO_MSG_PUT_VAR_UINT";
    case  PIO_MSG_PUT_VAR:
            return "PIO_MSG_PUT_VAR";
    case  PIO_MSG_PUT_VARA_USHORT:
            return "PIO_MSG_PUT_VARA_USHORT";
    case  PIO_MSG_GET_VAR_LONGLONG:
            return "PIO_MSG_GET_VAR_LONGLONG";
    case  PIO_MSG_GET_VARA_SHORT:
            return "PIO_MSG_GET_VARA_SHORT";
    case  PIO_MSG_PUT_VARS_SHORT:
            return "PIO_MSG_PUT_VARS_SHORT";
    case  PIO_MSG_PUT_VARA_UINT:
            return "PIO_MSG_PUT_VARA_UINT";
    case  PIO_MSG_PUT_VARA_SCHAR:
            return "PIO_MSG_PUT_VARA_SCHAR";
    case  PIO_MSG_PUT_VARM_ULONGLONG:
            return "PIO_MSG_PUT_VARM_ULONGLONG";
    case  PIO_MSG_PUT_VAR1_UCHAR:
            return "PIO_MSG_PUT_VAR1_UCHAR";
    case  PIO_MSG_PUT_VARM_INT:
            return "PIO_MSG_PUT_VARM_INT";
    case  PIO_MSG_PUT_VARS_SCHAR:
            return "PIO_MSG_PUT_VARS_SCHAR";
    case  PIO_MSG_GET_VARA_LONG:
            return "PIO_MSG_GET_VARA_LONG";
    case  PIO_MSG_PUT_VAR1:
            return "PIO_MSG_PUT_VAR1";
    case  PIO_MSG_GET_VAR1_INT:
            return "PIO_MSG_GET_VAR1_INT";
    case  PIO_MSG_GET_VAR1_ULONGLONG:
            return "PIO_MSG_GET_VAR1_ULONGLONG";
    case  PIO_MSG_GET_VAR_UCHAR:
            return "PIO_MSG_GET_VAR_UCHAR";
    case  PIO_MSG_PUT_VARA_FLOAT:
            return "PIO_MSG_PUT_VARA_FLOAT";
    case  PIO_MSG_GET_VARA_UCHAR:
            return "PIO_MSG_GET_VARA_UCHAR";
    case  PIO_MSG_GET_VARS_FLOAT:
            return "PIO_MSG_GET_VARS_FLOAT";
    case  PIO_MSG_PUT_VAR1_FLOAT:
            return "PIO_MSG_PUT_VAR1_FLOAT";
    case  PIO_MSG_PUT_VARM_FLOAT:
            return "PIO_MSG_PUT_VARM_FLOAT";
    case  PIO_MSG_PUT_VAR1_TEXT:
            return "PIO_MSG_PUT_VAR1_TEXT";
    case  PIO_MSG_PUT_VARS_TEXT:
            return "PIO_MSG_PUT_VARS_TEXT";
    case  PIO_MSG_PUT_VARM_LONG:
            return "PIO_MSG_PUT_VARM_LONG";
    case  PIO_MSG_GET_VARS_LONG:
            return "PIO_MSG_GET_VARS_LONG";
    case  PIO_MSG_PUT_VARS_DOUBLE:
            return "PIO_MSG_PUT_VARS_DOUBLE";
    case  PIO_MSG_GET_VAR1:
            return "PIO_MSG_GET_VAR1";
    case  PIO_MSG_GET_VAR_UINT:
            return "PIO_MSG_GET_VAR_UINT";
    case  PIO_MSG_PUT_VARA_LONGLONG:
            return "PIO_MSG_PUT_VARA_LONGLONG";
    case  PIO_MSG_GET_VARA:
            return "PIO_MSG_GET_VARA";
    case  PIO_MSG_PUT_VAR_DOUBLE:
            return "PIO_MSG_PUT_VAR_DOUBLE";
    case  PIO_MSG_GET_VARA_SCHAR:
            return "PIO_MSG_GET_VARA_SCHAR";
    case  PIO_MSG_PUT_VAR_FLOAT:
            return "PIO_MSG_PUT_VAR_FLOAT";
    case  PIO_MSG_GET_VAR1_UINT:
            return "PIO_MSG_GET_VAR1_UINT";
    case  PIO_MSG_GET_VARS_UINT:
            return "PIO_MSG_GET_VARS_UINT";
    case  PIO_MSG_PUT_VAR1_ULONGLONG:
            return "PIO_MSG_PUT_VAR1_ULONGLONG";
    case  PIO_MSG_PUT_VARM_UINT:
            return "PIO_MSG_PUT_VARM_UINT";
    case  PIO_MSG_PUT_VAR1_UINT:
            return "PIO_MSG_PUT_VAR1_UINT";
    case  PIO_MSG_PUT_VAR1_INT:
            return "PIO_MSG_PUT_VAR1_INT";
    case  PIO_MSG_GET_VARA_FLOAT:
            return "PIO_MSG_GET_VARA_FLOAT";
    case  PIO_MSG_GET_VARM_TEXT:
            return "PIO_MSG_GET_VARM_TEXT";
    case  PIO_MSG_PUT_VARS_FLOAT:
            return "PIO_MSG_PUT_VARS_FLOAT";
    case  PIO_MSG_GET_VAR1_TEXT:
            return "PIO_MSG_GET_VAR1_TEXT";
    case  PIO_MSG_PUT_VARA_SHORT:
            return "PIO_MSG_PUT_VARA_SHORT";
    case  PIO_MSG_PUT_VAR1_SCHAR:
            return "PIO_MSG_PUT_VAR1_SCHAR";
    case  PIO_MSG_PUT_VARA_ULONGLONG:
            return "PIO_MSG_PUT_VARA_ULONGLONG";
    case  PIO_MSG_PUT_VARM_DOUBLE:
            return "PIO_MSG_PUT_VARM_DOUBLE";
    case  PIO_MSG_GET_VARM_INT:
            return "PIO_MSG_GET_VARM_INT";
    case  PIO_MSG_PUT_VARA:
            return "PIO_MSG_PUT_VARA";
    case  PIO_MSG_PUT_VARA_LONG:
            return "PIO_MSG_PUT_VARA_LONG";
    case  PIO_MSG_GET_VARM_UINT:
            return "PIO_MSG_GET_VARM_UINT";
    case  PIO_MSG_GET_VARM:
            return "PIO_MSG_GET_VARM";
    case  PIO_MSG_PUT_VAR1_DOUBLE:
            return "PIO_MSG_PUT_VAR1_DOUBLE";
    case  PIO_MSG_GET_VARS_DOUBLE:
            return "PIO_MSG_GET_VARS_DOUBLE";
    case  PIO_MSG_GET_VARA_LONGLONG:
            return "PIO_MSG_GET_VARA_LONGLONG";
    case  PIO_MSG_GET_VAR_ULONGLONG:
            return "PIO_MSG_GET_VAR_ULONGLONG";
    case  PIO_MSG_PUT_VARM_SCHAR:
            return "PIO_MSG_PUT_VARM_SCHAR";
    case  PIO_MSG_GET_VARA_ULONGLONG:
            return "PIO_MSG_GET_VARA_ULONGLONG";
    case  PIO_MSG_GET_VAR_SHORT:
            return "PIO_MSG_GET_VAR_SHORT";
    case  PIO_MSG_GET_VARM_FLOAT:
            return "PIO_MSG_GET_VARM_FLOAT";
    case  PIO_MSG_PUT_VAR_TEXT:
            return "PIO_MSG_PUT_VAR_TEXT";
    case  PIO_MSG_PUT_VARS_INT:
            return "PIO_MSG_PUT_VARS_INT";
    case  PIO_MSG_GET_VAR1_LONG:
            return "PIO_MSG_GET_VAR1_LONG";
    case  PIO_MSG_GET_VARM_LONG:
            return "PIO_MSG_GET_VARM_LONG";
    case  PIO_MSG_GET_VARM_USHORT:
            return "PIO_MSG_GET_VARM_USHORT";
    case  PIO_MSG_PUT_VAR1_SHORT:
            return "PIO_MSG_PUT_VAR1_SHORT";
    case  PIO_MSG_PUT_VARS_LONGLONG:
            return "PIO_MSG_PUT_VARS_LONGLONG";
    case  PIO_MSG_GET_VARM_LONGLONG:
            return "PIO_MSG_GET_VARM_LONGLONG";
    case  PIO_MSG_GET_VARS_TEXT:
            return "PIO_MSG_GET_VARS_TEXT";
    case  PIO_MSG_PUT_VARA_DOUBLE:
            return "PIO_MSG_PUT_VARA_DOUBLE";
    case  PIO_MSG_PUT_VARS:
            return "PIO_MSG_PUT_VARS";
    case  PIO_MSG_PUT_VAR_UCHAR:
            return "PIO_MSG_PUT_VAR_UCHAR";
    case  PIO_MSG_GET_VAR1_UCHAR:
            return "PIO_MSG_GET_VAR1_UCHAR";
    case  PIO_MSG_PUT_VAR_LONG:
            return "PIO_MSG_PUT_VAR_LONG";
    case  PIO_MSG_GET_VARS:
            return "PIO_MSG_GET_VARS";
    case  PIO_MSG_GET_VARM_SHORT:
            return "PIO_MSG_GET_VARM_SHORT";
    case  PIO_MSG_GET_VARM_ULONGLONG:
            return "PIO_MSG_GET_VARM_ULONGLONG";
    case  PIO_MSG_PUT_VARM_LONGLONG:
            return "PIO_MSG_PUT_VARM_LONGLONG";
    case  PIO_MSG_GET_VAR_SCHAR:
            return "PIO_MSG_GET_VAR_SCHAR";
    case  PIO_MSG_GET_ATT_UBYTE:
            return "PIO_MSG_GET_ATT_UBYTE";
    case  PIO_MSG_PUT_ATT_STRING:
            return "PIO_MSG_PUT_ATT_STRING";
    case  PIO_MSG_GET_ATT_STRING:
            return "PIO_MSG_GET_ATT_STRING";
    case  PIO_MSG_PUT_ATT_UBYTE:
            return "PIO_MSG_PUT_ATT_UBYTE";
    case  PIO_MSG_INQ_VAR_FILL:
            return "PIO_MSG_INQ_VAR_FILL";
    case  PIO_MSG_DEF_VAR_FILL:
            return "PIO_MSG_DEF_VAR_FILL";
    case  PIO_MSG_DEF_VAR_DEFLATE:
            return "PIO_MSG_DEF_VAR_DEFLATE";
    case  PIO_MSG_INQ_VAR_DEFLATE:
            return "PIO_MSG_INQ_VAR_DEFLATE";
    case  PIO_MSG_INQ_VAR_SZIP:
            return "PIO_MSG_INQ_VAR_SZIP";
    case  PIO_MSG_DEF_VAR_FLETCHER32:
            return "PIO_MSG_DEF_VAR_FLETCHER32";
    case  PIO_MSG_INQ_VAR_FLETCHER32:
            return "PIO_MSG_INQ_VAR_FLETCHER32";
    case  PIO_MSG_DEF_VAR_CHUNKING:
            return "PIO_MSG_DEF_VAR_CHUNKING";
    case  PIO_MSG_INQ_VAR_CHUNKING:
            return "PIO_MSG_INQ_VAR_CHUNKING";
    case  PIO_MSG_DEF_VAR_ENDIAN:
            return "PIO_MSG_DEF_VAR_ENDIAN";
    case  PIO_MSG_INQ_VAR_ENDIAN:
            return "PIO_MSG_INQ_VAR_ENDIAN";
    case  PIO_MSG_SET_CHUNK_CACHE:
            return "PIO_MSG_SET_CHUNK_CACHE";
    case  PIO_MSG_GET_CHUNK_CACHE:
            return "PIO_MSG_GET_CHUNK_CACHE";
    case  PIO_MSG_SET_VAR_CHUNK_CACHE:
            return "PIO_MSG_SET_VAR_CHUNK_CACHE";
    case  PIO_MSG_GET_VAR_CHUNK_CACHE:
            return "PIO_MSG_GET_VAR_CHUNK_CACHE";
    case  PIO_MSG_INITDECOMP_DOF:
            return "PIO_MSG_INITDECOMP_DOF";
    case  PIO_MSG_WRITEDARRAY:
            return "PIO_MSG_WRITEDARRAY";
    case  PIO_MSG_WRITEDARRAYMULTI:
            return "PIO_MSG_WRITEDARRAYMULTI";
    case  PIO_MSG_SETFRAME:
            return "PIO_MSG_SETFRAME";
    case  PIO_MSG_ADVANCEFRAME:
            return "PIO_MSG_ADVANCEFRAME";
    case  PIO_MSG_READDARRAY:
            return "PIO_MSG_READDARRAY";
    case  PIO_MSG_SETERRORHANDLING:
            return "PIO_MSG_SETERRORHANDLING";
    case  PIO_MSG_FREEDECOMP:
            return "PIO_MSG_FREEDECOMP";
    case  PIO_MSG_CLOSE_FILE:
            return "PIO_MSG_CLOSE_FILE";
    case  PIO_MSG_DELETE_FILE:
            return "PIO_MSG_DELETE_FILE";
    case  PIO_MSG_FINALIZE:
            return "PIO_MSG_FINALIZE";
    case  PIO_MSG_GET_ATT:
            return "PIO_MSG_GET_ATT";
    case  PIO_MSG_PUT_ATT:
            return "PIO_MSG_PUT_ATT";
    case  PIO_MSG_COPY_ATT:
            return "PIO_MSG_COPY_ATT";
    case  PIO_MSG_INQ_TYPE:
            return "PIO_MSG_INQ_TYPE";
    case  PIO_MSG_INQ_UNLIMDIMS:
            return "PIO_MSG_INQ_UNLIMDIMS";
    case  PIO_MSG_EXIT:
            return "PIO_MSG_EXIT";
    default:
            return "UNKNOWN_MSG";
  } /* switch(msg) */
}
