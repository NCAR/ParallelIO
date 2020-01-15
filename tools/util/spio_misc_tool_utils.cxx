#include "spio_misc_tool_utils.h"
#include <stdexcept>

namespace spio_tool_utils{

  std::string iotype_to_string(PIO_IOTYPE iotype)
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

  bool gsuccess(MPI_Comm comm, int lspio_err)
  {
    bool lsucc = (lspio_err == PIO_NOERR) ? true : false;
    bool gsucc = lsucc;
    int mpierr = MPI_SUCCESS;

    mpierr = MPI_Allreduce(&lsucc, &gsucc, 1, MPI_C_BOOL, MPI_LAND, comm);
    if(mpierr != MPI_SUCCESS){
      throw std::runtime_error("MPI_Allreduce failed while trying to determine when a function was successful or not");
    }

    return gsucc;
  }

} // namespace spio_tool_utils
