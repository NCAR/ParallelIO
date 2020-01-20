#ifndef __SPIO_FINFO_H__
#define __SPIO_FINFO_H__

#include <string>
#include <vector>

extern "C"{
#include "pio_config.h"
#include "pio.h"
} // extern "C"

#include "mpi.h"

/* Fwd decl reqd for create_spio_finfo() */
class spio_finfo;

namespace spio_finfo_utils{
/* The different file types */
enum class spio_ftype{
  NETCDF_CLASSIC=1,
  NETCDF_64BIT_OFFSET,
  NETCDF_64BIT_DATA,
  NETCDF4,
  ADIOS,
  UNKNOWN
};

/* Convert file type to string */
std::string spio_ftype_to_string(const spio_ftype type);
/* Convert the file type string to the corresponding file type */
spio_ftype spio_ftype_from_string(const std::string &stype);

/* Create a spio_finfo class that will contain all information about a file */
spio_finfo create_spio_finfo(MPI_Comm comm, const std::string &fname);

} // namespace spio_finfo_utils

/* This class contains all the required information about a file */
class spio_finfo{
  public:
    void add_supported_iotype(PIO_IOTYPE iotype);
    std::string get_fname(void ) const;
    std::string get_type(void ) const;
    bool is_supported(void ) const;
    std::vector<PIO_IOTYPE> get_supported_iotypes(void ) const;
    spio_finfo(const spio_finfo &finfo) = default;
    spio_finfo &operator=(const spio_finfo &finfo) = default;
  private:
    /* The constructor is private because we want the class to be
     * created via create_spio_finfo(). Creating this class includes
     * setting the type of the file by reading it
     */
    spio_finfo(const std::string &fname,
      const spio_finfo_utils::spio_ftype type);
    friend spio_finfo spio_finfo_utils::create_spio_finfo(MPI_Comm comm,
      const std::string &fname);
    std::string fname_;
    spio_finfo_utils::spio_ftype type_;
    std::vector<PIO_IOTYPE> supported_iotypes_;
}; // class spio_finfo

#endif // __SPIO_FINFO_H__
