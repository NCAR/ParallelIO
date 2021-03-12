#include "mpi.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <cassert>
#include <stdexcept>
#include "argparser.h"

namespace spio_tool_utils{

ArgParser::ArgParser(MPI_Comm comm):NOVAL_OPT_STR("noval"),
                                    comm_(comm), is_root_(false),
                                    printed_usage_(false) 
{
  int rank;
  MPI_Comm_rank(comm, &rank);
  if (rank == COMM_ROOT){
      is_root_ = true;
  }
}

/* Add a valid command line option to the argument parser */
ArgParser &ArgParser::add_opt(const std::string &opt,
                    const std::string &help_str)
{
  opts_map_[opt] = help_str;
  return *this;
}

/* Parse the command line arguments in argv[]
 * Note: Valid options should already be set using add_opt()
 * before calling parse()
 */
void ArgParser::parse(int argc, char *argv[])
{
  assert(argv);
  prog_name_ = std::string(argv[0]);
  const std::string OPT_RGX_STR("--([^=]+)=(.+)");
  for (int i = 1; i < argc; i++){
    std::regex opt_rgx(OPT_RGX_STR.c_str());
    std::smatch match;
    std::string argvi(argv[i]);
    if (std::regex_search(argvi, match, opt_rgx) &&
        (match.size() == 3)){
      if (opts_map_.count(match.str(1)) != 1){
        if (is_root_){
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
    else{
      const std::string NOVAL_OPT_RGX_STR("--([^=]+)");
      std::regex noval_opt_rgx(NOVAL_OPT_RGX_STR.c_str());
      std::smatch noval_match;
      /* The "--help" option is provided by default */
      if ((argvi == "--help") || (argvi == "-h")){
        print_usage(std::cout);
        return;
      }
      else if (std::regex_search(argvi, noval_match, noval_opt_rgx) &&
          (noval_match.size() == 2)){
        /* No value arguments like "--verbose" */
        if (opts_map_.count(noval_match.str(1)) != 1){
          if (is_root_){
            std::cerr << "Error: Invalid option, "
                      << noval_match.str(1).c_str() << "\n";
          }
          throw std::runtime_error("Invalid option");
        }
        arg_map_[noval_match.str(1)] = NOVAL_OPT_STR;
      }
      else{
        if (is_root_){
          std::cerr << "Error: Unable to parse option : "
                    << argvi << "\n";
        }
        throw std::runtime_error("Invalid option");
      }
    }
  }
}

/* Parse the command line arguments in argv[] without using
 * C++11 regex expressions. This support is for compatibility
 * with compilers (e.g. IBM XL 16.x on Summit) that do not
 * support regex.
 * Note: Valid options should already be set using add_opt()
 * before calling parse()
 */
void ArgParser::no_regex_parse(int argc, char *argv[])
{
  assert(argv);
  prog_name_ = std::string(argv[0]);
  for (int i = 1; i < argc; i++){
    std::string argvi(argv[i]);
    std::vector<std::string> tokens;

    tokenize_cmd_line_args(argvi, tokens);
    if (tokens.size() == 2){
      if (opts_map_.count(tokens[0]) != 1){
        if (is_root_){
          std::cerr << "Error: Invalid option, "
                    << argvi << "\n";
        }
        throw std::runtime_error("Invalid option");
      }
      /* FIXME: No validation performed on the option value
       * We could ask the user to specify option type when defining an
       * option
       */
      arg_map_[tokens[0]] = tokens[1];
    }
    else if (tokens.size() == 1){
      /* The "--help" option is provided by default */
      if ((argvi == "--help") || (argvi == "-h")){
        print_usage(std::cout);
        return;
      }
      else{
        /* No value arguments like "--verbose" */
        if (opts_map_.count(tokens[0]) != 1){
          if (is_root_){
            std::cerr << "Error: Invalid option, "
                      << argvi << "\n";
          }
          throw std::runtime_error("Invalid option");
        }
        arg_map_[tokens[0]] = NOVAL_OPT_STR;
      }
    }
    else{
      if (is_root_){
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
  if (printed_usage_ || !is_root_){
    return;
  }
  /* Ensure we only print usage once */
  printed_usage_ = true;
  ostr << "Usage : " << prog_name_ << " --[OPTIONAL ARG1 NAME]=[OPTIONAL ARG1 VALUE] --[OPTIONAL ARG2 NAME]=[OPTIONAL ARG2 VALUE] ... \n";
  ostr << "Optional Arguments :\n";
  for (std::map<std::string, std::string>::const_iterator
        citer = opts_map_.cbegin();
        citer != opts_map_.cend(); ++citer){
    ostr << "--" << (*citer).first << ":\t" << (*citer).second << "\n";
  }
}

/* Tokenize command line arguments to tokens
 * Command line arguments are of the form,
 * 1) "--opt=val", tokens are "opt", "val"
 * 2) "--noval_opt", tokens are "noval_opt"
 */
void ArgParser::tokenize_cmd_line_args(const std::string &str, std::vector<std::string> &tokens) const
{
  if(str.size() < 3){
    std::cerr << "Unable to tokenize command line arg (too small) : "
              << str.c_str() << "\n";
    return;
  }

  /* All options start with "--" */
  if((str[0] != '-') || (str[1] != '-')){
    std::cerr << "Unable to tokenize command line arg (option does not start with '--') : "
              << str.c_str() << "\n";
    return;
  }

  std::stringstream tok_ostr;
  bool has_val = false;
  std::size_t idx = 2;

  /* Write the option name to the token stream */
  for(idx = 2; idx < str.size(); idx++){
    if(str[idx] == '='){
      if(tok_ostr.str().size() == 0){
        std::cerr << "Unable to tokenize command line arg (option does not have expected fmt) : "
                  << str.c_str() << "\n";
        return;
      }

      has_val = true;

      /* Add option name to token list */
      tokens.push_back(tok_ostr.str());

      /* Reset stream to read optional value */
      tok_ostr.str("");

      idx++;
      break;
    }
    else{
      tok_ostr.put(str[idx]);
    }
  }

  /* Write the optional value to the token stream */
  for(; idx < str.size(); idx++){
    tok_ostr.put(str[idx]);
  }

  if(tok_ostr.str().size() > 0){
    /* Add optional value to token list */
    tokens.push_back(tok_ostr.str());
  }
  else{
    /* Extra sanity check to ensure that "--opt=" is followed by a value */
    if(has_val){
      std::cerr << "Warning: possibly corrupted command line arg (missing value for option) : "
                << str.c_str() << "\n";
    }
  }
}

} // namespace spio_tool_utils
