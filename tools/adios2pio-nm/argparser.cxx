#include "mpi.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <cassert>
#include <stdexcept>
#include "argparser.h"

namespace adios2pio_utils{

ArgParser::ArgParser(MPI_Comm comm):comm_(comm), is_root_(false)
{
    int rank;
    MPI_Comm_rank(comm, &rank);
    if (rank == COMM_ROOT)
    {
        is_root_ = true;
    }
}

ArgParser &ArgParser::add_opt(const std::string &opt,
                    const std::string &help_str)
{
    opts_map_[opt] = help_str;
    return *this;
}

void ArgParser::parse(int argc, char *argv[])
{
    assert(argv);
    prog_name_ = std::string(argv[0]);
    const std::string OPT_RGX_STR("--([^=]+)=(.+)");
    for (int i = 1; i < argc; i++)
    {
        std::regex opt_rgx(OPT_RGX_STR.c_str());
        std::smatch match;
        std::string argvi(argv[i]);
        if (std::regex_search(argvi, match, opt_rgx) &&
            (match.size() == 3))
        {
            if (opts_map_.count(match.str(1)) != 1)
            {
                if (is_root_)
                {
                    std::cerr << "Error: Invalid option, "
                              << match.str(1).c_str() << "\n";
                }
                throw std::runtime_error("Invalid option");
            }
            /* FIXME: No validation performed on the option value
             * We could ask the user to specify option type when defining an 
             * option
             */
            arg_map_[match.str(1)] = match.str(2);
        }
        else if ((argvi == "--help") || (argvi == "-h"))
        {
            print_usage(std::cout);
            return;
        }
        else
        {
            if (is_root_)
            {
                std::cerr << "Error: Unable to parse option : "
                          << argvi << "\n";
            }
            throw std::runtime_error("Invalid option");
        }
    }
}

bool ArgParser::has_arg(const std::string &opt) const
{
    return (arg_map_.count(opt) == 1);
}

void ArgParser::print_usage(std::ostream &ostr) const
{
    if (!is_root_)
    {
        return;
    }
    ostr << "Usage : " << prog_name_ << " [OPTIONAL ARGS] \n";
    ostr << "Optional Arguments :\n";
    for (std::map<std::string, std::string>::const_iterator
          citer = opts_map_.cbegin();
          citer != opts_map_.cend(); ++citer)
    {
        ostr << "--" << (*citer).first << ":\t" << (*citer).second << "\n";
    }
}

} // namespace adios2pio_utils
