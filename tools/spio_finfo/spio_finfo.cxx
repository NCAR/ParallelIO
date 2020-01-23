#include <iostream>
#include <stdexcept>
#include <cassert>
#include "spio_finfo.h"

namespace spio_finfo_utils{

std::string spio_ftype_to_string(const spio_ftype type)
{
  switch(type){
    case spio_ftype::NETCDF_CLASSIC:
          return "NETCDF_CLASSIC";
    case spio_ftype::NETCDF_64BIT_OFFSET:
          return "NETCDF_64BIT_OFFSET";
    case spio_ftype::NETCDF_64BIT_DATA:
          return "NETCDF_64BIT_DATA";
    case spio_ftype::NETCDF4:
          return "NETCDF4";
    case spio_ftype::ADIOS:
          return "ADIOS";
    default:
          return "UNKNOWN";
  }
}

spio_ftype spio_ftype_from_string(const std::string &stype)
{
  if(stype == "NETCDF_CLASSIC"){
    return spio_ftype::NETCDF_CLASSIC;
  }
  else if(stype == "NETCDF_64BIT_OFFSET"){
    return spio_ftype::NETCDF_64BIT_OFFSET;
  }
  else if(stype == "NETCDF_64BIT_DATA"){
    return spio_ftype::NETCDF_64BIT_DATA;
  }
  else if(stype == "NETCDF4"){
    return spio_ftype::NETCDF4;
  }
  else if(stype == "ADIOS"){
    return spio_ftype::ADIOS;
  }
  else{
    return spio_ftype::UNKNOWN;
  }
}

namespace hdr_magic_utils{
  /* The magic numbers are in the initial bytes of a binary file
   * Magic numbers for NetCDF files are in the first 4 bytes of the file,
   * NETCDF_CLASSIC : "CDF001"
   * NETCDF_64BIT_OFFSET : "CDF002"
   * NETCDF_64BIT_DATA : "CDF005"
   * NETCDF4 : "\211HDF"
   * e.g. For NetCDF Classic files,
   * 
   *        First byte : 43
   *        Second byte : 44
   *        Third byte : 46
   *        Fourth byte : 01
   *
   * ASCII for C = 43, D = 44, F = 46
   */
  const int HDR_MAGIC_SZ = 4;
  /* buf contains the initial bytes read from a binary file */
  static spio_ftype get_spio_ftype(char *buf, int buf_sz)
  {
    static const char NETCDF_MAGIC[] = {'C', 'D', 'F'};
    static const char NETCDF_CLASSIC_VERSION = static_cast<char>(0x01);
    static const char NETCDF_64BIT_OFFSET_VERSION = static_cast<char>(0x02);
    static const char NETCDF_64BIT_DATA_VERSION = static_cast<char>(0x05);
    /* Note: First byte is Octal 211, 0211 = 0x89 */
    static const char HDF5_MAGIC[] = {static_cast<char>(0211), 'H', 'D', 'F'};
    int ret;

    assert(sizeof(NETCDF_MAGIC) < HDR_MAGIC_SZ);
    assert(sizeof(HDF5_MAGIC) <= HDR_MAGIC_SZ);

    if(buf_sz < HDR_MAGIC_SZ){
      return spio_ftype::UNKNOWN;
    }

    ret = memcmp(buf, NETCDF_MAGIC, sizeof(NETCDF_MAGIC));
    if(ret == 0){
      /* NetCDF */
      char version = buf[sizeof(NETCDF_MAGIC)];
      if(version == NETCDF_CLASSIC_VERSION){
        return spio_ftype::NETCDF_CLASSIC;
      }
      else if(version == NETCDF_64BIT_OFFSET_VERSION){
        return spio_ftype::NETCDF_64BIT_OFFSET;
      }
      else if(version == NETCDF_64BIT_DATA_VERSION){
        return spio_ftype::NETCDF_64BIT_DATA;
      }
    }
    else{
      /* Check if the file is HDF5 */
      ret = memcmp(buf, HDF5_MAGIC, sizeof(HDF5_MAGIC));
      if(ret == 0){
        return spio_ftype::NETCDF4;
      }
      /* FIXME : Add check for ADIOS file type */
    }

    return spio_ftype::UNKNOWN;
  }

} // namespace hdr_magic_utils

spio_finfo create_spio_finfo(MPI_Comm comm, const std::string &fname)
{
  int rank = -1, sz = 0;
  int ret = PIO_NOERR;

  ret = MPI_Comm_rank(comm, &rank);
  if(ret != MPI_SUCCESS){
    throw std::runtime_error("Inquiring rank of the MPI process failed");
  }

  ret = MPI_Comm_size(comm, &sz);
  if(ret != MPI_SUCCESS){
    throw std::runtime_error("Inquiring size of the MPI communicator failed");
  }

  if(fname.length() == 0){
    throw std::runtime_error("Invalid argument provided, the filename is an empty string");
  }

  /* Open a file on rank 0 and read the header to look for magic numbers
   * The magic number is the first 
   * Magic numbers for NetCDF files are in the first 4 bytes of the file,
   * NETCDF_CLASSIC : "CDF001"
   * NETCDF_64BIT_OFFSET : "CDF002"
   * NETCDF_64BIT_DATA : "CDF005"
   * NETCDF4 : "211HDF"
   * e.g. For NetCDF Classic files,
   * 
   *        First byte : 43
   *        Second byte : 44
   *        Third byte : 46
   *        Fourth byte : 01
   *
   * ASCII for C = 43, D = 44, F = 46
   */

  spio_ftype ftype = spio_ftype::UNKNOWN;
  if(rank == 0){
    try{
      MPI_Status status;
      MPI_File fh;
      MPI_Offset file_sz = 0;

      ret = MPI_File_open(MPI_COMM_SELF, fname.c_str(),
              MPI_MODE_RDONLY, MPI_INFO_NULL, &fh);
      if(ret != MPI_SUCCESS){
        std::cerr << "Opening file, \"" << fname.c_str() << "\" failed\n";
        throw std::runtime_error("Opening file failed");
      }

      ret = MPI_File_get_size(fh, &file_sz);
      if((ret != MPI_SUCCESS) || (file_sz < hdr_magic_utils::HDR_MAGIC_SZ)){
        throw std::runtime_error("File too short");
      }

      /* First 4 bytes of header contains the magic number */
      char hdr_magic[hdr_magic_utils::HDR_MAGIC_SZ];
      ret = MPI_File_read_at(fh, 0, &hdr_magic,
              hdr_magic_utils::HDR_MAGIC_SZ, MPI_BYTE, &status);
      if(ret != MPI_SUCCESS){
        throw std::runtime_error("Error reading file header");
      }

      ftype = hdr_magic_utils::get_spio_ftype(hdr_magic,
                sizeof(hdr_magic));

      ret = MPI_File_close(&fh);
      if(ret != MPI_SUCCESS){
        throw std::runtime_error("Error closing the file");
      }
    }
    catch(...){
      ftype = spio_ftype::UNKNOWN;
    }
  }

  int buf_ftype = static_cast<int>(ftype);
  ret = MPI_Bcast(&buf_ftype, 1, MPI_INT, 0, comm);
  if(ret != MPI_SUCCESS){
    throw std::runtime_error("Broadcasting the file type failed");
  }
  ftype = static_cast<spio_ftype>(buf_ftype);

  return spio_finfo(fname, ftype);
}

} // namespace spio_finfo_utils

void spio_finfo::add_supported_iotype(PIO_IOTYPE iotype)
{
  supported_iotypes_.push_back(iotype);
}

std::string spio_finfo::get_fname(void ) const
{
  return fname_;
}

std::string spio_finfo::get_type(void ) const
{
  return spio_finfo_utils::spio_ftype_to_string(type_);
}

bool spio_finfo::is_supported(void ) const
{
  return (type_ != spio_finfo_utils::spio_ftype::UNKNOWN);
}

std::vector<PIO_IOTYPE> spio_finfo::get_supported_iotypes(void ) const
{
  return supported_iotypes_;
}

spio_finfo::spio_finfo(const std::string &fname,
  const spio_finfo_utils::spio_ftype type):fname_(fname), type_(type)
{
}

