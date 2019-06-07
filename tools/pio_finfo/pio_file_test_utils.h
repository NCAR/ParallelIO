#ifndef __PIO_FILE_TEST_UTILS_H__
#define __PIO_FILE_TEST_UTILS_H__

#include <string>

extern "C"{
#include "pio_config.h"
#include "pio.h"
} // extern "C"

namespace pio_finfo_utils{

  /* Test the file, fname, and print info about it */
  int pio_test_file(const std::string &fname,
                    MPI_Comm comm_in,
                    int num_iotasks, int iostride, int ioroot,
                    bool verbose);

  /* Test files in directory, dname, and print info each file */
  int pio_test_files(const std::string &dname,
                    MPI_Comm comm_in,
                    int num_iotasks, int iostride, int ioroot,
                    bool verbose);

} // namespace pio_finfo_utils

#endif // __PIO_FILE_TEST_UTILS_H__
