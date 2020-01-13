#ifndef __SPIO_MISC_TOOL_UTILS_H__
#define __SPIO_MISC_TOOL_UTILS_H__

#include <string>

extern "C"{
#include "pio_config.h"
#include "pio.h"
} // extern "C"

namespace spio_tool_utils{
  
  /* Convert a PIO iotype to string */
  std::string iotype_to_string(PIO_IOTYPE iotype);

  /* Convert a range of PIO iotypes to a string */
  template<typename Iter>
  std::string iotypes_to_string(Iter begin, Iter end)
  {
    std::string str;
    while(begin + 1 != end){
      str += iotype_to_string(*begin) + ", ";
      ++begin;
    }
    if(begin != end){
      str += iotype_to_string(*begin);
    }

    return str;
  }

} // namespace spio_tool_utils

#endif // __SPIO_MISC_TOOL_UTILS_H__
