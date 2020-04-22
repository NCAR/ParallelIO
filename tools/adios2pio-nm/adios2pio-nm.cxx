#include <mpi.h>
#ifdef TIMING
#include <gptl.h>
#endif
#include <iostream>
#include <regex>
#include "adios2pio-nm-lib.h"
#include "argparser.h"

static void init_user_options(spio_tool_utils::ArgParser &ap)
{
    ap.add_opt("bp-file", "data produced by PIO with ADIOS format")
      .add_opt("idir", "Directory containing data output from PIO (in ADIOS format)")
      .add_opt("nc-file", "output file name after conversion")
      .add_opt("pio-format", "output PIO_IO_TYPE. Supported parameters: \"pnetcdf\",  \"netcdf\",  \"netcdf4c\",  \"netcdf4p\"")
      .add_opt("reduce-memory-usage", "Reduce memory usage (execution time will likely increase)")
      .add_opt("verbose", "Turn on verbose info messages");
}

static int get_user_options(
              spio_tool_utils::ArgParser &ap,
              int argc, char *argv[],
              std::string &idir,
              std::string &ifile, std::string &ofile,
              std::string &otype,
              int &mem_opt,
              int &debug_lvl)
{
    const std::string DEFAULT_PIO_FORMAT("pnetcdf");
    mem_opt = 0;
    debug_lvl = 0;

    ap.parse(argc, argv);
    if (!ap.has_arg("bp-file") && !ap.has_arg("idir"))
    {
        ap.print_usage(std::cerr);
        return 1;
    }
    if (ap.has_arg("bp-file"))
    {
        ifile = ap.get_arg<std::string>("bp-file");
        if (ap.has_arg("nc-file"))
        {
            ofile = ap.get_arg<std::string>("nc-file");
        }
        else
        {
            const std::string BP_DIR_RGX_STR("(.*)([.]nc)?[.]bp");
            std::regex bp_dir_rgx(BP_DIR_RGX_STR.c_str());
            std::smatch match;
            if (std::regex_search(ifile, match, bp_dir_rgx) &&
                match.size() >= 2)
            {
                ofile = match.str(1);
            }
            else
            {
                ap.print_usage(std::cerr);
                return 1;
            }
        }
    }
    else
    {
        assert(ap.has_arg("idir"));
        idir = ap.get_arg<std::string>("idir");
    }

    if (ap.has_arg("pio-format"))
    {
        otype = ap.get_arg<std::string>("pio-format");
    }
    else
    {
        otype = DEFAULT_PIO_FORMAT;
    }

    if (ap.has_arg("reduce-memory-usage"))
    {
        mem_opt = 1;
    }

    if (ap.has_arg("verbose"))
    {
        debug_lvl = 1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int ret = 0;

    MPI_Init(&argc, &argv);

    MPI_Comm comm_in = MPI_COMM_WORLD;

    spio_tool_utils::ArgParser ap(comm_in);

    /* Init the standard user options for the tool */
    init_user_options(ap);

    /* Parse the user options */
    string idir, infilepath, outfilename, piotype;
    int mem_opt = 0;
    int debug_lvl = 0;
    ret = get_user_options(ap, argc, argv,
                            idir, infilepath, outfilename,
                            piotype, mem_opt, debug_lvl);

    if (ret != 0)
    {
        return ret;
    }

#ifdef TIMING
    /* Initialize the GPTL timing library. */
    if ((ret = GPTLinitialize()))
        return ret;
#endif

    SetDebugOutput(debug_lvl);
    MPI_Barrier(comm_in);
    if (idir.size() == 0)
    {
        ret = ConvertBPToNC(infilepath, outfilename, piotype, mem_opt, comm_in);
    }
    else
    {
        ret = MConvertBPToNC(idir, piotype, mem_opt, comm_in);
    }
    MPI_Barrier(comm_in);

#ifdef TIMING
    /* Finalize the GPTL timing library. */
    if ((ret = GPTLfinalize()))
        return ret;
#endif

    MPI_Finalize();

    return ret;
}
