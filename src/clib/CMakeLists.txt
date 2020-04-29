cmake_minimum_required (VERSION 2.8.12)
include (CheckFunctionExists)
project (PIOC C CXX)

#==============================================================================
#  DEFINE THE TARGET
#==============================================================================

add_library (pioc topology.c pio_mpi_timer.c pio_timer.c pio_file.c
  pioc_support.c pio_lists.c pio_print.c
  pioc.c pioc_sc.c pio_spmd.c pio_rearrange.c pio_nc4.c bget.c
  pio_nc.c pio_put_nc.c pio_get_nc.c pio_getput_int.c pio_msg.c pio_varm.c
  pio_darray.c pio_darray_int.c pio_sdecomps_regex.cpp)

# set up include-directories
include_directories(
  "${PROJECT_SOURCE_DIR}"   # to find foo/foo.h
  "${PROJECT_BINARY_DIR}")  # to find foo/config.h

# Include the clib source and binary directory
target_include_directories (pioc
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (pioc
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# System and compiler CPP directives
target_compile_definitions (pioc
  PRIVATE ${CMAKE_SYSTEM_DIRECTIVE})
target_compile_definitions (pioc
  PUBLIC ${CMAKE_C_COMPILER_DIRECTIVE})
target_compile_definitions (pioc
  PUBLIC MPICH_SKIP_MPICXX)
target_compile_definitions (pioc
  PUBLIC OMPI_SKIP_MPICXX)

# At least on Titan + Cray MPI, MPI_Irsends are buggy
# causing hangs during I/O
# Force Scorpio to use MPI_Isends instead of the default
# MPI_Irsends
target_compile_definitions (pioc
  PRIVATE USE_MPI_ISEND_FOR_FC)

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

#==============================================================================
#  DEFINE THE INSTALL
#==============================================================================

# Library
install (TARGETS pioc DESTINATION lib)

# Include/Header File
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/pio.h DESTINATION include)

#==============================================================================
#  DEFINE THE DEPENDENCIES
#==============================================================================
# MPI test done at top level
if (MPISERIAL_C_FOUND)
    target_compile_definitions (pioc
      PRIVATE MPI_SERIAL)
    target_include_directories (pioc
      PUBLIC ${MPISERIAL_C_INCLUDE_DIRS})
    target_link_libraries (pioc
      PUBLIC ${MPISERIAL_C_LIBRARIES})
    set (WITH_PNETCDF FALSE)
endif ()



#===== GPTL =====
if (PIO_ENABLE_TIMING)
  find_package (GPTL COMPONENTS C QUIET)
  if (GPTL_C_FOUND)
    message (STATUS "Found GPTL C: ${GPTL_C_LIBRARIES}")
    target_include_directories (pioc
      PUBLIC ${GPTL_C_INCLUDE_DIRS})
    target_link_libraries (pioc
      PUBLIC ${GPTL_C_LIBRARIES})
  else ()
    message (STATUS "Using internal GPTL C library for timing")
    target_link_libraries (pioc
      PUBLIC gptl)
  endif ()
  target_compile_definitions (pioc
    PUBLIC TIMING)
  if (PIO_ENABLE_INTERNAL_TIMING)
    target_compile_definitions (pioc
      PUBLIC TIMING_INTERNAL)
  endif ()
endif ()

#====== PIO_MICRO_TIMING ======
if (PIO_MICRO_TIMING)
  set(USE_MICRO_TIMING 1)
else ()
  set(USE_MICRO_TIMING 0)
endif ()

#===== NetCDF-C =====
if (WITH_NETCDF)
  find_package (NetCDF ${NETCDF_C_MIN_VER_REQD} COMPONENTS C)
endif ()
if (NetCDF_FOUND)
  set(PIO_USE_NETCDF 1)
  target_include_directories (pioc
    PUBLIC ${NetCDF_C_INCLUDE_DIRS})
  target_link_libraries (pioc
    PUBLIC ${NetCDF_C_LIBRARIES})
  if (${NetCDF_C_HAS_PARALLEL})
    set(PIO_USE_NETCDF4 1)
  else ()
    set(PIO_USE_NETCDF4 0)
  endif ()
  if (${NetCDF_C_LOGGING_ENABLED})
    target_compile_definitions (pioc
      PUBLIC NETCDF_C_LOGGING_ENABLED)
    # netcdf.h needs this to be defined to use netCDF logging.
    target_compile_definitions (pioc
      PUBLIC LOGGING)
  endif()
else ()
  set(PIO_USE_NETCDF 0)
  set(PIO_USE_NETCDF4 0)
endif ()

#===== PnetCDF-C =====
if (WITH_PNETCDF)
  find_package (PnetCDF ${PNETCDF_MIN_VER_REQD} COMPONENTS C)
endif ()
if (PnetCDF_FOUND)
  set(PIO_USE_PNETCDF 1)
  target_include_directories (pioc
    PUBLIC ${PnetCDF_C_INCLUDE_DIRS})
  target_link_libraries (pioc
    PUBLIC ${PnetCDF_C_LIBRARIES})

  # Check library for varn functions
  set (CMAKE_REQUIRED_LIBRARIES ${PnetCDF_C_LIBRARY})
  check_function_exists (ncmpi_get_varn PnetCDF_C_HAS_VARN)
  if (PnetCDF_C_HAS_VARN)
    target_compile_definitions(pioc
      PUBLIC USE_PNETCDF_VARN
      PUBLIC USE_PNETCDF_VARN_ON_READ)
  endif()
else ()
  set(PIO_USE_PNETCDF 0)
endif ()

#===== ADIOS-C =====
if (WITH_ADIOS2)
  find_package (ADIOS2 ${ADIOS_MIN_VER_REQD})
endif ()
if (ADIOS2_FOUND)
  set(PIO_USE_ADIOS 1)
  target_link_libraries (pioc
    PUBLIC adios2::adios2 adios2pio-nm-lib)
else ()
  set(PIO_USE_ADIOS 0)
endif ()

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
  "${PROJECT_SOURCE_DIR}/pio_config.h.in"
  "${PROJECT_BINARY_DIR}/pio_config.h"
  )

# Install PIO config Include/Header File
install (FILES ${PROJECT_BINARY_DIR}/pio_config.h DESTINATION include)

if (ADIOS_BP2NC_TEST)
  target_compile_definitions (pioc
    PUBLIC _ADIOS_BP2NC_TEST)
endif ()

#===== Add EXTRAs =====
target_include_directories (pioc
  PUBLIC ${PIO_C_EXTRA_INCLUDE_DIRS})
target_link_libraries (pioc
  PUBLIC ${PIO_C_EXTRA_LIBRARIES})
target_compile_options (pioc
  PRIVATE ${PIO_C_EXTRA_COMPILE_OPTIONS})
target_compile_definitions (pioc
  PUBLIC ${PIO_C_EXTRA_COMPILE_DEFINITIONS})
if (PIO_C_EXTRA_LINK_FLAGS)
  set_target_properties(pioc PROPERTIES
    LINK_FLAGS ${PIO_C_EXTRA_LINK_FLAGS})
endif ()

#===== Check for necessities =====
if (NOT PnetCDF_C_FOUND AND NOT NetCDF_C_FOUND)
  message (FATAL_ERROR "Must have PnetCDF and/or NetCDF C libraries")
endif ()

include(CheckTypeSize)
check_type_size("size_t" SIZEOF_SIZE_T)
CHECK_TYPE_SIZE("long long" SIZEOF_LONG_LONG)
if (NOT ${SIZEOF_SIZE_T} EQUAL ${SIZEOF_LONG_LONG})
  message (FATAL_ERROR "size_t and long long must be the same size!")
endif ()

# Hack to circumvent IBM XL (16.1.1-3) internal compiler error by
# disabling optimization for pioc_support.c
cmake_host_system_information (RESULT FQDN_SITENAME QUERY FQDN)
if (FQDN_SITENAME MATCHES "^.*summit[.]olcf")
  if (CMAKE_C_COMPILER_NAME STREQUAL "XL")
    set_source_files_properties(pioc_support.c PROPERTIES COMPILE_FLAGS "-qnoopt -qnosmp")
  endif()
endif()
