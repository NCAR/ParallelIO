#ifndef __SPIO_FILE_TEST_UTILS_H__
#define __SPIO_FILE_TEST_UTILS_H__

#include <string>
#include <vector>

extern "C"{
#include "pio_config.h"
#include "pio.h"
} // extern "C"

#include "spio_finfo.h"

namespace spio_finfo_utils{

  /* Test the file, fname, and print info about it */
  int spio_test_file(MPI_Comm comm_in,
                    int num_iotasks, int iostride, int ioroot,
                    bool verbose,
                    spio_finfo &finfo);

  /* Test files in directory, dname, and print info each file */
  int spio_test_files(const std::string &dname,
                    MPI_Comm comm_in,
                    int num_iotasks, int iostride, int ioroot,
                    bool verbose,
                    std::vector<spio_finfo> &finfos);

} // namespace spio_finfo_utils

#endif // __SPIO_FILE_TEST_UTILS_H__
