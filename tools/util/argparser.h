#include "mpi.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <cassert>
#include <stdexcept>

namespace spio_tool_utils{

class ArgParser{
  public:
    ArgParser(MPI_Comm comm);
    /* Add a valid command line option to the command
     * line parser
     */
    ArgParser &add_opt( const std::string &opt,
                        const std::string &help_str);
    /* Parse the command line arguments in argv[] */
    void parse(int argc, char *argv[]);
    /* Returns true if option, opt, specified via command
     * line arguments and is parsed by the parser
     */
    bool has_arg(const std::string &opt) const;
    /* Get the value of a command line argument already
     * parsed by the parser.
     * Note: Use has_arg() to check if the argument is
     * present before calling get_arg()
     */
    template<typename T>
    T get_arg(const std::string &opt) const;
    /* Print help message on the command line options */
    void print_usage(std::ostream &ostr) const;
  private:
    /* Valid options map, set via add_opt() */
    std::map<std::string, std::string> opts_map_;
    /* User specified arguments map, parsed from command
     * line via parse()
     */
    std::map<std::string, std::string> arg_map_;
    /* Executable name */
    std::string prog_name_;
    MPI_Comm comm_;
    const int COMM_ROOT = 0;
    bool is_root_;
    /* To prevent printing usage/help multiple times */ 
    mutable bool printed_usage_;
};

template<typename T>
T ArgParser::get_arg(const std::string &opt) const
{
  throw std::runtime_error("Unsupported argument type");
}

/* Get a string argument already parsed by the parser */
template<>
inline std::string ArgParser::get_arg<std::string>(const std::string &opt) const
{
  assert(arg_map_.count(opt) == 1);
  return arg_map_.at(opt);
}

/* Get an int argument already parsed by the parser */
template<>
inline int ArgParser::get_arg<int>(const std::string &opt) const
{
  assert(arg_map_.count(opt) == 1);
  return std::stoi(arg_map_.at(opt));
}

/* Get a float argument already parsed by the parser */
template<>
inline float ArgParser::get_arg<float>(const std::string &opt) const
{
  assert(arg_map_.count(opt) == 1);
  return std::stof(arg_map_.at(opt));
}

} // namespace spio_tool_utils
