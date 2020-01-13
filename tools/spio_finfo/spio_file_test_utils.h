#ifndef __SPIO_FILE_TEST_UTILS_H__
#define __SPIO_FILE_TEST_UTILS_H__

#include <string>

extern "C"{
#include "pio_config.h"
#include "pio.h"
} // extern "C"

namespace spio_finfo_utils{

  /* Test the file, fname, and print info about it */
  int spio_test_file(const std::string &fname,
                    MPI_Comm comm_in,
                    int num_iotasks, int iostride, int ioroot,
                    bool verbose);

  /* Test files in directory, dname, and print info each file */
  int spio_test_files(const std::string &dname,
                    MPI_Comm comm_in,
                    int num_iotasks, int iostride, int ioroot,
                    bool verbose);

} // namespace spio_finfo_utils

#endif // __SPIO_FILE_TEST_UTILS_H__
