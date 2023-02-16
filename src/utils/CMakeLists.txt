include (LibMPI)

include_directories("${CMAKE_SOURCE_DIR}/src/utils")
include_directories("${CMAKE_SOURCE_DIR}/src/clib")
include_directories("${CMAKE_BINARY_DIR}/src/clib")
include_directories("${CMAKE_BINARY_DIR}")

# Compiler-specific compiler options
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu99")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "PGI")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -gnu99")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gnu99")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu99")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu99")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")

#==============================================================================
#  PREPARE FOR TESTING
#==============================================================================

# Don't run these tests if we are using MPI SERIAL.
if (NOT PIO_USE_MPISERIAL)
 if(PIO_ENABLE_NETCDF_INTEGRATION)
    MESSAGE("Building piocopy")
    add_executable (piocopy EXCLUDE_FROM_ALL mpi_argp.c dimmap.c piocopy.c utils.c nciter.c list.c chunkspec.c)
    target_link_libraries (piocopy pioc)
  endif()
endif ()
