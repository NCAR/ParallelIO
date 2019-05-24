#include <mpi.h>
#ifdef TIMING
#include <gptl.h>
#endif
#include <iostream>
#include <string>
#include <regex>
#include "argparser.h"

static void init_user_options(pio_tool_utils::ArgParser &ap)
{
  ap.add_opt("ifile", "Input file to be read with PIO")
    .add_opt("idir", "Directory containing input files to read with PIO")
    .add_opt("verbose", "Turn on verbose info messages");
}

static int get_user_options(
              pio_tool_utils::ArgParser &ap,
              int argc, char *argv[],
              std::string &idir,
              std::string &ifile,
              bool &verbose)
{
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

    return 0;
}

int main(int argc, char *argv[])
{
    int ret = 0;

    MPI_Init(&argc, &argv);

    MPI_Comm comm_in = MPI_COMM_WORLD;

    pio_tool_utils::ArgParser ap(comm_in);

    /* Init the standard user options for the tool */
    init_user_options(ap);

    /* Parse the user options */
    std::string idir, ifile;
    bool verbose = false;
    ret = get_user_options(ap, argc, argv,
                            idir, ifile, verbose);

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
