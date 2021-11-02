###-------------------------------------------------------------------------###
### CMakeList.txt for examples using pio
###-------------------------------------------------------------------------###
#==============================================================================
#  HELPER MACROS
#==============================================================================
include(SPIOUtils)

#==============================================================================
#  SET THE COMPILER OPTIONS
#==============================================================================
# Include PIO include and lib directories
INCLUDE_DIRECTORIES(${PIO_INCLUDE_DIRS})
include_directories("${SCORPIO_SOURCE_DIR}/examples/c")
LINK_DIRECTORIES(${PIO_LIB_DIR})

# Compiler-specific compiler options
string (TOUPPER "${CMAKE_C_COMPILER_ID}" CMAKE_C_COMPILER_NAME)
if (CMAKE_C_COMPILER_NAME STREQUAL "CRAY")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -h std=c99")
elseif (CMAKE_C_COMPILER_NAME STREQUAL "PGI")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -c99")
else ()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
endif ()

string (TOUPPER "${CMAKE_CXX_COMPILER_ID}" CMAKE_CXX_COMPILER_NAME)
if (CMAKE_CXX_COMPILER_NAME STREQUAL "CRAY")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -h std=c++11")
else ()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif ()

set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -g -O0")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")

#==============================================================================
#  FIND EXTERNAL LIBRARIES/DEPENDENCIES
#==============================================================================
#===== MPI =====
if (PIO_USE_MPISERIAL)
  find_package (MPISERIAL COMPONENTS C REQUIRED)
  if (MPISERIAL_C_FOUND)
    set (CMAKE_REQUIRED_INCLUDES ${MPISERIAL_C_INCLUDE_DIRS})
  endif ()
else ()
  find_package (MPI REQUIRED)
  set (CMAKE_REQUIRED_INCLUDES ${MPI_INCLUDE_PATH})
endif ()

#===== MPE =====
find_package (MPE ${MPE_MIN_VER_REQD} COMPONENTS C)

#===== GPTL =====
if (PIO_ENABLE_TIMING)
  find_package (GPTL COMPONENTS C QUIET)
  if (GPTL_C_FOUND)
    message (STATUS "Found GPTL C: ${GPTL_C_LIBRARIES}")
  else ()
    message (STATUS "Using internal GPTL C library for timing")
  endif ()
endif ()

#==============================================================================
#  DEFINE THE TARGETS AND TESTS
#==============================================================================
add_spio_executable(examplePio TRUE "" examplePio.c)
add_spio_executable(example1 TRUE "" example1.c)
add_spio_executable(darray_no_async TRUE "" darray_no_async.c)
add_spio_executable(darray_async TRUE "" darray_async.c)
add_spio_executable(put_var TRUE "" put_var.c)

if (PIO_ENABLE_FORTRAN)
  if (MPE_C_FOUND)
    add_spio_executable(example2 TRUE "" example2.c)
    target_include_directories (example2 PUBLIC ${MPE_C_INCLUDE_DIRS})
    target_compile_definitions (example2 PUBLIC HAVE_MPE)
    target_link_libraries (example2 PRIVATE lmpe mpe pthread m)
  endif ()
endif ()

if (PIO_USE_MPISERIAL)
  add_test(NAME examplePio COMMAND examplePio)
  add_test(NAME example1 COMMAND example1)
  add_test(NAME put_var COMMAND put_var)
else ()
  add_mpi_test(examplePio EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/examplePio NUMPROCS 4 TIMEOUT 60)
  add_mpi_test(example1 EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/example1 NUMPROCS 4 TIMEOUT 60)
  add_mpi_test(put_var EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/put_var NUMPROCS 4 TIMEOUT 60)
  #add_mpi_test(darray_async EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/darray_async NUMPROCS 5 TIMEOUT 60)
  add_mpi_test(darray_no_async EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/darray_no_async NUMPROCS 4 TIMEOUT 60)
endif ()
