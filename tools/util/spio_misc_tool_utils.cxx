#include "spio_misc_tool_utils.h"

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

} // namespace spio_tool_utils
