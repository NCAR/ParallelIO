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

/* Convert BP dir name (that contains the ADIOS BP data)
 * to the corresponding NetCDF file name by stripping
 * off the file type extensions at the end of the BP dir name.
 *
 * BP dir names are of the form "^.*([.]nc)?[.]bp$"
 * The returned string is the BP dir name stripped off the
 * ".nc" and ".bp" extensions
 */
static std::string strip_ftype_ext(const std::string &bp_dname)
{
#ifdef SPIO_NO_CXX_REGEX
    const std::string bp_ext(".bp");
    const std::string nc_opt_ext(".nc");
    std::size_t ext_sz = bp_ext.size();
    if (bp_dname.size() > ext_sz)
    {
        std::size_t chars_to_trim = 0;
        if (bp_dname.substr(bp_dname.size() - ext_sz, ext_sz) ==  bp_ext)
        {
            chars_to_trim += ext_sz;
            ext_sz = nc_opt_ext.size();
            if (bp_dname.size() > chars_to_trim + ext_sz)
            {
                if (bp_dname.substr(bp_dname.size() - chars_to_trim - ext_sz, ext_sz)
                      == nc_opt_ext)
                {
                    chars_to_trim += ext_sz;
                }
            }

            return bp_dname.substr(0, bp_dname.size() - chars_to_trim);
        }
    }
#else
    const std::string BP_DIR_RGX_STR("(.*)([.]nc)?[.]bp");
    std::regex bp_dir_rgx(BP_DIR_RGX_STR.c_str());
    std::smatch match;
    if (std::regex_search(bp_dname, match, bp_dir_rgx) &&
        match.size() >= 2)
    {
        return match.str(1);
    }
#endif
    return std::string();
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

#ifdef SPIO_NO_CXX_REGEX
    ap.no_regex_parse(argc, argv);
#else
    ap.parse(argc, argv);
#endif
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
            ofile = strip_ftype_ext(ifile);
        }
        if (ofile.size() == 0)
        {
            ap.print_usage(std::cerr);
            return 1;
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
#ifndef TIMING_INTERNAL
    /* Initialize the GPTL timing library. */
    if ((ret = GPTLinitialize()))
        return ret;
#endif
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
#ifndef TIMING_INTERNAL
    /* Finalize the GPTL timing library. */
    if ((ret = GPTLfinalize()))
        return ret;
#endif
#endif

    MPI_Finalize();

    return ret;
}
