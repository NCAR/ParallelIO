#include <mpi.h>
#ifdef TIMING
#include <gptl.h>
#endif
#include <iostream>
#include "adios2pio-nm-lib.h"
#include "argparser.h"

static void init_user_options(adios2pio_utils::ArgParser &ap)
{
    ap.add_opt("bp-file", "data produced by PIO with ADIOS format")
      .add_opt("nc-file", "output file name after conversion")
      .add_opt("pio-format", "output PIO_IO_TYPE. Supported parameters: \"pnetcdf\",  \"netcdf\",  \"netcdf4c\",  \"netcdf4p\"");
}

static int get_user_options(
              adios2pio_utils::ArgParser &ap,
              int argc, char *argv[],
              std::string &ifile, std::string &ofile, std::string &otype)
{
    ap.parse(argc, argv);
    if (  !ap.has_arg("bp-file") ||
          !ap.has_arg("nc-file") ||
          !ap.has_arg("pio-format") )
    {
        ap.print_usage(std::cerr);
        return 1;
    }
    ifile = ap.get_arg<std::string>("bp-file");
    ofile = ap.get_arg<std::string>("nc-file");
    otype = ap.get_arg<std::string>("pio-format");
    return 0;
}

int main(int argc, char *argv[])
{
    int ret = 0;

    MPI_Init(&argc, &argv);

    MPI_Comm comm_in = MPI_COMM_WORLD;

    adios2pio_utils::ArgParser ap(comm_in);

    /* Init the standard user options for the tool */
    init_user_options(ap);

    /* Parse the user options */
    string infilepath, outfilename, piotype;
    ret = get_user_options(ap, argc, argv,
                            infilepath, outfilename, piotype);
    if (ret != 0)
    {
        return ret;
    }

#ifdef TIMING
    /* Initialize the GPTL timing library. */
    if ((ret = GPTLinitialize()))
        return ret;
#endif

    SetDebugOutput(0);
    ret = ConvertBPToNC(infilepath, outfilename, piotype, comm_in);

#ifdef TIMING
    /* Finalize the GPTL timing library. */
    if ((ret = GPTLfinalize()))
        return ret;
#endif

    MPI_Finalize();

    return ret;
}
