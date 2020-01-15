#include "spio_file_test_utils.h"
#include "spio_lib_info.h"
#include "spio_misc_tool_utils.h"

#include <iostream>
#include <string>
#include <vector>
#include <regex>

extern "C"{
#include <sys/types.h>
#include <dirent.h>
} // extern "C"

namespace spio_finfo_utils{

  /* Test the file, fname, and print info about it */
  int spio_test_file(const std::string &fname,
                    MPI_Comm comm_in,
                    int num_iotasks, int iostride, int ioroot,
                    bool verbose)
  {
    int ret = PIO_NOERR;
    int iosysid, ncid, iotype;
    int rank;

    MPI_Comm_rank(comm_in, &rank);

    ret = PIOc_Init_Intracomm(comm_in, num_iotasks, iostride, ioroot,
                              PIO_REARR_BOX, &iosysid);
    if (ret != PIO_NOERR){
      if (rank == 0){
        std::cerr << "ERROR: Initializing the SCORPIO library failed\n";
      }
      return -1;
    }

    int prev_handler;
    ret = PIOc_set_iosystem_error_handling(iosysid, PIO_RETURN_ERROR,
            &prev_handler);
    if (ret != PIO_NOERR){
      if (rank == 0){
        std::cerr << "ERROR: Unable to set error handler for the iosystem to PIO_RETURN_ERROR\n";
      }
      return -1;
    }

    std::vector<PIO_IOTYPE> valid_iotypes;
    std::vector<PIO_IOTYPE> supported_iotypes;
    spio_tool_utils::spio_lib_info::get_supported_iotypes(supported_iotypes);

    for (std::vector<PIO_IOTYPE>::iterator iter = supported_iotypes.begin();
          iter != supported_iotypes.end(); ++iter){
      iotype = *iter;
      if(verbose){
        std::cout << "LOG : openfile " << fname.c_str() << ", with iotype = "
                  << spio_tool_utils::iotype_to_string(*iter).c_str() << "\n"
                  << std::flush;
      }

      /* PIOc_openfile2() does not retry opening files with serial NetCDF
       * iotype, while PIOc_openfile() retries opening files opened with
       * any iotype with the serial NetCDF iotype on failure
       */
      ret = PIOc_openfile2(iosysid, &ncid, &iotype, fname.c_str(), PIO_NOWRITE);
      if (!spio_tool_utils::gsuccess(comm_in, ret)){
        continue;
      }

      if(verbose){
        std::cout << "LOG : closefile " << fname.c_str() << ", with iotype = "
                  << spio_tool_utils::iotype_to_string(*iter).c_str() << "\n"
                  << std::flush;
      }

      ret = PIOc_closefile(ncid);
      if (!spio_tool_utils::gsuccess(comm_in, ret)){
        continue;
      }

      /* Add iotypes that can be used to open/close the file successfully */
      valid_iotypes.push_back(*iter);
    }

    if(verbose){
      std::cout << "LOG : Done open/close with all iotypes\n";
    }

    if (rank == 0){
      if (valid_iotypes.size() == 0){
        std::cout << fname.c_str() << ":\t" << "No supported I/O types\n";
      }
      else{
        std::cout << fname.c_str() << ":\t" << "Supported I/O types = "
                  << spio_tool_utils::iotypes_to_string(valid_iotypes.begin(),
                      valid_iotypes.end()) + "\n";
      }
    }

    ret = PIOc_finalize(iosysid);
    if (ret != PIO_NOERR){
      if (rank == 0){
        std::cerr << "ERROR: Finalizing the SCORPIO library failed\n";
      }
      return -1;
    }

    return 0;
  }

  /* Test files in directory, dname, and print info each file */
  int spio_test_files(const std::string &dname,
                    MPI_Comm comm_in,
                    int num_iotasks, int iostride, int ioroot,
                    bool verbose)
  {
    struct dirent *pde = NULL;
    int ret = 0;
    int rank;

    MPI_Comm_rank(comm_in, &rank);

    DIR *pdir = opendir(dname.c_str());
    if (!pdir){
      if (rank == 0){
        std::cerr << "ERROR: Unable to open directory, "
                  << dname.c_str() << "\n";
      }
      return -1;
    }
    while ((pde = readdir(pdir)) != NULL){
      /* Only process regular files - no recursive search */
      if (pde->d_type == DT_REG){
        const std::string PATH_SEP("/");
        std::string full_path_name = dname +
                                      PATH_SEP +
                                      std::string(pde->d_name);
        ret = spio_test_file(full_path_name.c_str(), comm_in,
                            num_iotasks, iostride, ioroot, verbose);
        if (ret != 0){
          break;
        }
      }
    }

    closedir(pdir);
    return ret;
  }

} // namespace spio_finfo_utils
