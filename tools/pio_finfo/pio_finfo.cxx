#include "pio_config.h"
#include <mpi.h>
#ifdef TIMING
#include <gptl.h>
#endif
#include <iostream>
#include <string>
#include <regex>
#include "argparser.h"
#include "pio_lib_info.h"

static void init_user_options(pio_tool_utils::ArgParser &ap)
{
  ap.add_opt("ifile", "Input file to be read with PIO")
    .add_opt("idir", "Directory containing input files to read with PIO")
    .add_opt("num-iotasks", "Number of I/O tasks to use (default = total number of procs / 2)")
    .add_opt("iostride", "Stride between the I/O tasks (default = 1)")
    .add_opt("ioroot", "Rank of the root I/O process (default = 0)")
    .add_opt("verbose", "Turn on verbose info messages");
}

static int get_user_options(
              pio_tool_utils::ArgParser &ap,
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
    if (!ap.has_arg("ifile") && !ap.has_arg("idir")){
      idir = ".";
    }
    else if(ap.has_arg("ifile")){
      ifile = ap.get_arg<std::string>("ifile");
    }
    else{
      assert(ap.has_arg("idir"));
      idir = ap.get_arg<std::string>("idir");
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
          std::cerr << "Resetting the I/O root process to 0\n";
        }
        ioroot = 0;
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
          std::cerr << "Resetting the I/O stride to 1\n";
        }
        iostride = 1;
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
          std::cerr << "Resetting the number of I/O tasks to 1\n";
        }
        num_iotasks = 1;
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
      std::cout << "PIO File info tool (Version: " +
                    pio_tool_utils::pio_lib_info::get_lib_version() +
                    ")\n";
      std::cout << "==================================================\n";
      std::cout << "PIO Library info\n";
      std::cout << "------------------\n";
      std::cout << pio_tool_utils::pio_lib_info::get_lib_summary() + "\n";
    }

    pio_tool_utils::ArgParser ap(comm_in);

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
        return ret;
    }

#ifdef TIMING
#ifndef TIMING_INTERNAL
    /* Initialize the GPTL timing library. */
    if ((ret = GPTLinitialize())){
        return ret;
    }
#endif
#endif


#ifdef TIMING
#ifndef TIMING_INTERNAL
    /* Finalize the GPTL timing library. */
    if ((ret = GPTLfinalize())){
        return ret;
    }
#endif
#endif

    MPI_Finalize();

    return ret;
}
