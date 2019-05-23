#include "mpi.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <cassert>
#include <stdexcept>

namespace pio_tool_utils{

class ArgParser{
  public:
    ArgParser(MPI_Comm comm);
    ArgParser &add_opt( const std::string &opt,
                        const std::string &help_str);
    void parse(int argc, char *argv[]);
    bool has_arg(const std::string &opt) const;
    template<typename T>
    T get_arg(const std::string &opt) const;
    void print_usage(std::ostream &ostr) const;
  private:
    std::map<std::string, std::string> opts_map_;
    std::map<std::string, std::string> arg_map_;
    std::string prog_name_;
    MPI_Comm comm_;
    const int COMM_ROOT = 0;
    bool is_root_;
    mutable bool printed_usage_;
};

template<typename T>
T ArgParser::get_arg(const std::string &opt) const
{
    throw std::runtime_error("Unsupported argument type");
}

template<>
inline std::string ArgParser::get_arg<std::string>(const std::string &opt) const
{
    assert(arg_map_.count(opt) == 1);
    return arg_map_.at(opt);
}

template<>
inline int ArgParser::get_arg<int>(const std::string &opt) const
{
    assert(arg_map_.count(opt) == 1);
    return std::stoi(arg_map_.at(opt));
}

template<>
inline float ArgParser::get_arg<float>(const std::string &opt) const
{
    assert(arg_map_.count(opt) == 1);
    return std::stof(arg_map_.at(opt));
}

} // namespace pio_tool_utils
