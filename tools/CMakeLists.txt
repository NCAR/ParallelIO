###-------------------------------------------------------------------------###
### CMakeList.txt for examples using pio
###-------------------------------------------------------------------------###

# Disable C++11 regex usage for IBM XL compiler (16.*) on Summit
# The IBM XL compiler on Summit has a buggy support for C++11 regex
string (TOUPPER "${CMAKE_CXX_COMPILER_ID}" CMAKE_CXX_COMPILER_NAME)
cmake_host_system_information (RESULT FQDN_SITENAME QUERY FQDN)
if (FQDN_SITENAME MATCHES "^.*summit[.]olcf")
  if (CMAKE_CXX_COMPILER_NAME STREQUAL "XL")
    add_definitions(-DSPIO_NO_CXX_REGEX)
  endif()
endif()

if (WITH_ADIOS2)
  find_package (ADIOS2 ${ADIOS_MIN_VER_REQD})
  if (ADIOS2_FOUND)
    ADD_SUBDIRECTORY(adios2pio-nm)
  endif (ADIOS2_FOUND)
endif(WITH_ADIOS2)
ADD_SUBDIRECTORY(spio_finfo)
