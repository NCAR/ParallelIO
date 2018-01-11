###-------------------------------------------------------------------------###
### CMakeList.txt for adios2pio
###-------------------------------------------------------------------------###

# Adding PIO definitions - defined in the root directory
ADD_DEFINITIONS(${PIO_DEFINITIONS})

# Include PIO include and lib directories
INCLUDE_DIRECTORIES(${PIO_INCLUDE_DIRS})
LINK_DIRECTORIES(${PIO_LIB_DIR})

# Compiler-specific compiler options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "PGI")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

SET(SRC adios2pio-nm.cxx)
# add_library(adios2pio-nm-lib adios2pio-nm-lib.cxx)

# target_include_directories(adios2pio-nm-lib PUBLIC ${CMAKE_SOURCE_DIR}/src/clib ${ADIOS_INCLUDE_DIRS} ${NetCDF_C_INCLUDE_DIRS} ${PnetCDF_C_INCLUDE_DIRS} ${PIO_C_EXTRA_INCLUDE_DIRS})

ADD_EXECUTABLE(adios2pio-nm ${SRC} adios2pio-nm-lib.cxx)
TARGET_LINK_LIBRARIES(adios2pio-nm pioc)


