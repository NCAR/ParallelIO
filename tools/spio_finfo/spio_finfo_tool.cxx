#include "pio_config.h"
#include <mpi.h>
#ifdef TIMING
#include <gptl.h>
#endif
#include <iostream>
#include <string>
#include <regex>
#include "argparser.h"
#include "spio_lib_info.h"
#include "spio_file_test_utils.h"
#include "spio_misc_tool_utils.h"
#include "spio_finfo.h"

/* Initialize the argument parser with the supported
 * command line options
 */
static void init_user_options(spio_tool_utils::ArgParser &ap)
{
  ap.add_opt("ifile", "Input file to be read with SCORPIO")
    .add_opt("idir", "Directory containing input files to read with SCORPIO")
    .add_opt("num-iotasks", "Number of I/O tasks to use (default = total number of procs / 2)")
    .add_opt("iostride", "Stride between the I/O tasks (default = 1)")
    .add_opt("ioroot", "Rank of the root I/O process (default = 0)")
    .add_opt("verbose", "Turn on verbose info messages");
}

/* Parse the command line options and get it */
static int get_user_options(
              spio_tool_utils::ArgParser &ap,
              int argc, char *argv[],
              MPI_Comm comm_in,
              std::string &idir,
              std::string &ifile,
              int &num_iotasks, int &iostride, int &ioroot,
              bool &verbose)
{
  int rank, sz;

  MPI_Comm_rank(comm_in, &rank);
  MPI_Comm_size(comm_in, &sz);
  verbose = false;

  ap.parse(argc, argv);

  if (ap.has_arg("verbose")){
    verbose = true;    
  }

  if(ap.has_arg("idir")){
    idir = ap.get_arg<std::string>("idir");
    if(idir.length() == 0){
      if(rank == 0){
        std::cerr << "ERROR: Parsing the \"--idir\" command line option failed. Read an empty string for the directory name\n";
      }
      return -1;
    }
  }

  if(ap.has_arg("ifile")){
    if(ap.has_arg("idir")){
      if(rank == 0){
        std::cerr << "WARNING: Both \"--ifile\" and \"--idir\" options were specified. The \"--ifile\" option will be ignored\n";
      }
    }
    ifile = ap.get_arg<std::string>("ifile");
    if(ifile.length() == 0){
      if(rank == 0){
        std::cerr << "ERROR: Parsing the \"--ifile\" command line option failed. Read an empty string for the file name\n";
      }
      return -1;
    }
  }

  /* Get I/O root process */
  if (ap.has_arg("ioroot")){
    ioroot = ap.get_arg<int>("ioroot");
    if ((ioroot < 0) || (ioroot > sz-1)){
      if (rank == 0){
        std::cerr << "WARNING: Invalid I/O process specified\n"
                  << "The specified I/O root process ("
                  << ioroot << ")"
                  << ((ioroot < 0) ? "is less than 0" : "greater than total number of MPI processes")
                  << "\n";
      }
      return -1;
    }
  }
  else{
    ioroot = 0;
  }

  /* Get I/O stride */
  if (ap.has_arg("iostride")){
    iostride = ap.get_arg<int>("iostride");
    if (iostride <= 0){
      if (rank == 0){
        std::cerr << "WARNING: Invalid I/O stride ("
                  << iostride
                  << ") provided\n";
      }
      return -1;
    }
  }
  else{
    iostride = 1;
  }

  /* Get the number of I/O tasks
   * (should be less than the number of MPI procs)
   * Also the following relationship needs to hold between num_iotasks,
   * iostride, ioroot and sz
   * ioroot + (num_iotasks - 1) * iostride <= (sz - 1)
   */
  if (ap.has_arg("num-iotasks")){
    num_iotasks = ap.get_arg<int>("num-iotasks");
    if ((num_iotasks <= 0) || (num_iotasks > sz)){
      if (rank == 0){
        std::cerr << "WARNING: Number of I/O tasks specified by the user("
                  << num_iotasks << ")"
                  << ((num_iotasks <= 0) ? " is <= 0" :  "is greater than the  number of MPI processes") << "\n";
      }
      return -1;
    }
    else if( (ioroot + (num_iotasks - 1) * iostride) > (sz - 1) ){
      /* Cannot use 1/2 of the MPI processes as I/O processes due to
       * the settings for ioroot and iostride
       */
      if (rank == 0){
        std::cerr << "WARNING: Number of I/O tasks requested ("
                  << num_iotasks << ")"
                  << " cannot be accomodated\n";
      }
      num_iotasks = ((sz - 1 - ioroot)/iostride) + 1;
      if (rank == 0){
        std::cerr << "Resetting the number of I/O tasks to "
                  << num_iotasks << "\n";
      }
    }
  }
  else{
    /* Try using 1/2 of the MPI processes as I/O processes */
    num_iotasks = sz/2;
    if(num_iotasks == 0){
      num_iotasks = 1;
    }
    else if( (ioroot + (num_iotasks - 1) * iostride) > (sz - 1) ){
      /* Cannot use 1/2 of the MPI processes as I/O processes due to
       * the settings for ioroot and iostride
       */
      num_iotasks = ((sz - 1 - ioroot)/iostride) + 1;
    }
  }

  return 0;
}

int main(int argc, char *argv[])
{
  int ret = 0;
  int rank = 0;

  MPI_Init(&argc, &argv);

  MPI_Comm comm_in = MPI_COMM_WORLD;
  MPI_Comm_rank(comm_in, &rank);

  if (rank == 0){
    /* Print out basic information header */
    std::cout << "==================================================\n";
    std::cout << "SCORPIO File info tool (Version: " +
                  spio_tool_utils::spio_lib_info::get_lib_version() +
                  ")\n";
    std::cout << "==================================================\n";
    std::cout << "SCORPIO Library info\n";
    std::cout << "------------------\n";
    std::cout << spio_tool_utils::spio_lib_info::get_lib_summary() + "\n";
    std::cout << "==================================================\n";
  }

  spio_tool_utils::ArgParser ap(comm_in);

  /* Init the standard user options for the tool */
  init_user_options(ap);

  /* Parse the user options */
  std::string idir, ifile;
  int num_iotasks = 0, iostride = 0, ioroot = -1;
  bool verbose = false;
  ret = get_user_options(ap, argc, argv, comm_in,
                          idir, ifile,
                          num_iotasks, iostride, ioroot,
                          verbose);

  if (ret != 0) {
    if (rank == 0){
      std::cerr << "Parsing user arguments failed\n";
    }
    return ret;
  }

  /* If idir and ifile are not set, nothing to do
   * e.g. --help
   */
  if ((idir.length() == 0) && (ifile.length() == 0)){
    return ret;
  }

#ifdef TIMING
#ifndef TIMING_INTERNAL
  /* Initialize the GPTL timing library. */
  ret = GPTLinitialize();
  if (ret != 0){
    if (rank == 0){
      std::cerr << "Initializing the GPTL timing library failed\n";
    }
    return ret;
  }
#endif
#endif

  if (rank == 0){
    /* Print out basic information header */
    std::cout << "==================================================\n";
    std::cout << "Processing files ...\n";
    std::cout << "==================================================\n";
  }

  /* Test the file/files using SCORPIO and print info */
  std::vector<spio_finfo> finfos;
  if (idir.length() == 0){
    assert(ifile.length() != 0);
    spio_finfo finfo = spio_finfo_utils::create_spio_finfo(comm_in, ifile);
    if(finfo.is_supported()){
      ret = spio_finfo_utils::spio_test_file(comm_in,
              num_iotasks, iostride, ioroot, verbose, finfo);
    }
    finfos.push_back(finfo);
  }
  else{
    ret = spio_finfo_utils::spio_test_files(idir, comm_in,
            num_iotasks, iostride, ioroot, verbose, finfos);
  }

  if (ret != 0) {
    if (rank == 0){
      std::cerr << "Testing files using Scorpio failed\n";
    }
    return ret;
  }

  if (rank == 0){
    std::cout << "========================================\n";
    for(std::vector<spio_finfo>::const_iterator citer = finfos.cbegin();
          citer != finfos.cend(); ++citer){
      std::vector<PIO_IOTYPE> supp_iotypes = (*citer).get_supported_iotypes();
      std::cout << "File :\t" << (*citer).get_fname().c_str() << "\n"
                << "Type :\t" << (*citer).get_type().c_str() << "\n";
      if (supp_iotypes.size() != 0){
        std::cout << "Supported Scorpio I/O types :\t"
                  << spio_tool_utils::iotypes_to_string(
                      supp_iotypes.begin(), supp_iotypes.end())
                  << "\n";
      }
      else{
        std::cout << "No supported Scorpio I/O types\n";
      }
      std::cout << "========================================\n";
    }
    std::cout << "========================================\n";
  }

#ifdef TIMING
#ifndef TIMING_INTERNAL
  /* Finalize the GPTL timing library. */
  ret = GPTLfinalize();
  if (ret != 0){
    if (rank == 0){
      std::cerr << "Finalizing the GPTL timing library failed\n";
    }
    return ret;
  }
#endif
#endif

  MPI_Finalize();

  return ret;
}
