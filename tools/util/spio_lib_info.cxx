#include "spio_lib_info.h"
#include "spio_misc_tool_utils.h"

namespace spio_tool_utils{
  namespace spio_lib_info{

    /* Internal namespace that contains data structures with info
     * about the library.
     * Not intented to be accessed/used directly by the user
     */
    namespace spio_lib_info_{
      static bool info_is_init_ = false;
    
      /* The following variables are valid iff
       * info_is_init_ == true
       */
      static std::vector<PIO_IOTYPE> supported_iotypes_;

      static void init_lib_info(void )
      {
        // Initialize supported io types
#ifdef _NETCDF4
        spio_lib_info_::supported_iotypes_.push_back(PIO_IOTYPE_NETCDF);
        spio_lib_info_::supported_iotypes_.push_back(PIO_IOTYPE_NETCDF4C);
        spio_lib_info_::supported_iotypes_.push_back(PIO_IOTYPE_NETCDF4P);
#elif _NETCDF
        spio_lib_info_::supported_iotypes_.push_back(PIO_IOTYPE_NETCDF);
#endif

#ifdef _PNETCDF
        spio_lib_info_::supported_iotypes_.push_back(PIO_IOTYPE_PNETCDF);
#endif

#ifdef _ADIOS
        spio_lib_info_::supported_iotypes_.push_back(PIO_IOTYPE_ADIOS);
#endif
        spio_lib_info_::info_is_init_ = true;
      }

    } // namespace spio_lib_info_

    void get_supported_iotypes(std::vector<PIO_IOTYPE> &supported_iotypes)
    {
      if(!spio_lib_info_::info_is_init_){
        spio_lib_info_::init_lib_info();
      }
      supported_iotypes = spio_lib_info_::supported_iotypes_;
    }

    std::string get_lib_version(void )
    {
      return  std::to_string(PIO_VERSION_MAJOR) + "." +
              std::to_string(PIO_VERSION_MINOR) + "." +
              std::to_string(PIO_VERSION_PATCH);
    }

    /* A summary of the Library (version, supported io types etc) */
    std::string get_lib_summary(void )
    {
      std::string str;
      std::vector<PIO_IOTYPE> supported_iotypes;
      get_supported_iotypes(supported_iotypes);

      str += "Version: " + get_lib_version() + "\n";
      str += "Supported SCORPIO iotypes = " +
              iotypes_to_string(supported_iotypes.begin(),
                supported_iotypes.end()) + "\n";
      return str;
    }

  } // namespace spio_lib_info
} // namespace spio_tool_utils
