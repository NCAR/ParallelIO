
# This is part of the PIO library.
# This is the CMake build file for the main directory.
# Jim Edwards

cmake_minimum_required(VERSION 3.18)
project(PIO LANGUAGES C)

# The project version number (modern style)
set(VERSION_MAJOR 2 CACHE STRING "Project major version number.")
set(VERSION_MINOR 6 CACHE STRING "Project minor version number.")
set(VERSION_PATCH 9 CACHE STRING "Project patch version number.")
mark_as_advanced(VERSION_MAJOR VERSION_MINOR VERSION_PATCH)

set(PIO_VERSION_MAJOR ${VERSION_MAJOR})
set(PIO_VERSION_MINOR ${VERSION_MINOR})
set(PIO_VERSION_PATCH ${VERSION_PATCH})

set(PACKAGE_VERSION "${PIO_VERSION_MAJOR}.${PIO_VERSION_MINOR}.${PIO_VERSION_PATCH}")

# This provides cmake_print_variables() function for debugging.
include(CMakePrintHelpers)

# This provides check_symbol_exists
include(CheckSymbolExists)
include(CheckCSourceCompiles)

# Determine the configure date.

if(DEFINED ENV{SOURCE_DATE_EPOCH})
  execute_process(
    COMMAND "date" "-u" "-d" "@$ENV{SOURCE_DATE_EPOCH}"
    OUTPUT_VARIABLE CONFIG_DATE
  )
else()
  execute_process(
    COMMAND date
    OUTPUT_VARIABLE CONFIG_DATE
  )
endif()

FUNCTION(is_enabled feature ret_val)
  IF(${feature})
    SET(${ret_val} "yes" PARENT_SCOPE)
    SET("PIO_${ret_val}" 1 PARENT_SCOPE)
  ELSE()
    SET(${ret_val} "no" PARENT_SCOPE)
    SET("PIO_${ret_val}" 0 PARENT_SCOPE)
  ENDIF(${feature})
ENDFUNCTION()
# A function used to create autotools-style 'yes/no' definitions.
# If a variable is set, it 'yes' is returned. Otherwise, 'no' is
# returned.
#
# Also creates a version of the ret_val prepended with 'NC',
# when feature is true, which is used to generate netcdf_meta.h.
FUNCTION(is_disabled feature ret_val)
  IF(${feature})
    SET(${ret_val} "no" PARENT_SCOPE)
  ELSE()
    SET(${ret_val} "yes" PARENT_SCOPE)
    SET("PIO_${ret_val}" 1 PARENT_SCOPE)
  ENDIF(${feature})
ENDFUNCTION()


# Always detect NetCDF C and PnetCDF for C library
find_package(NetCDF QUIET COMPONENTS C)
if(NOT NetCDF_C_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(NetCDF_C REQUIRED IMPORTED_TARGET "netcdf")
  if(NetCDF_C_FOUND OR NetCDF_C_INCLUDE_DIRS)
    set(NetCDF_C_FOUND TRUE CACHE BOOL "NetCDF C found via pkg-config" FORCE)
  endif()
endif()

# NetCDF feature tests (after NetCDF_C detection)
if(NetCDF_C_FOUND AND NetCDF_C_INCLUDE_DIRS)
  # Test for NetCDF-4 capability
  set(CMAKE_REQUIRED_INCLUDES ${NetCDF_C_INCLUDE_DIRS})
  CHECK_C_SOURCE_COMPILES("
#include <netcdf_meta.h>
#if !defined(NC_VERSION_MAJOR) || NC_VERSION_MAJOR < 4
  choke me
#endif
int main() {return 0;}" HAVE_NETCDF4)

  # Test for NetCDF parallel I/O capability
  set(CMAKE_REQUIRED_INCLUDES ${NetCDF_C_INCLUDE_DIRS})
  CHECK_C_SOURCE_COMPILES("
#include <netcdf_meta.h>
#if !NC_HAS_PARALLEL
  choke me
#endif
int main() {return 0;}" HAVE_NETCDF_PAR)
else()
  set(HAVE_NETCDF4 FALSE)
  set(HAVE_NETCDF_PAR FALSE)
  endif()


# The size of the data buffer for write/read_darray().
set(PIO_BUFFER_SIZE 134217728)

#==============================================================================
#  USER-DEFINED OPTIONS (set with "-DOPT=VAL" from command line)
#==============================================================================


#===== Library Options =====
option(PIO_ENABLE_FORTRAN "Enable the Fortran library builds" ON)
option(PIO_ENABLE_TIMING "Enable the use of the GPTL timing library" ON)
option(PIO_ENABLE_LOGGING "Enable debug logging (large output possible)" OFF)
option(PIO_ENABLE_DOC "Enable building PIO documentation" ON)
option(PIO_ENABLE_COVERAGE "Enable code coverage" OFF)
option(PIO_ENABLE_EXAMPLES "Enable PIO examples" ON)
option(PIO_ENABLE_NETCDF_INTEGRATION "Enable netCDF integration" OFF)
option(PIO_INTERNAL_DOC "Enable PIO developer documentation" OFF)
option(PIO_TEST_BIG_ENDIAN "Enable test to see if machine is big endian" ON)
option(PIO_USE_MPIIO "Enable support for MPI-IO auto detect" ON)
option(PIO_USE_MPISERIAL "Enable mpi-serial support (instead of MPI)" OFF)
option(PIO_USE_PNETCDF_VARD "Use pnetcdf put_vard" OFF)
option(WITH_PNETCDF "Require the use of PnetCDF" ON)
option(PIO_USE_GDAL "Enable support for Geospatial Data Abstraction Library" OFF)  
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

if(APPLE)
  # The linker on macOS does not include `common symbols` by default
  # Passing the -c flag includes them and fixes an error with undefined symbols (err_buffer, resultlen)
  set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> -c <TARGET>")
endif()


# Set a variable that appears in the config.h.in file.
if(PIO_USE_PNETCDF_VARD)
  set(USE_VARD 1)
else()
  set(USE_VARD 0)
endif()


# Set a variable that appears in the config.h.in file.
if(PIO_USE_GDAL)
  find_package(GDAL REQUIRED)
  if(GDAL_FOUND)
    set(PIO_ENABLE_GDAL 1)
  endif()
else()
  set(PIO_ENABLE_GDAL 0)
endif()


# Set a variable that appears in the config.h.in file.
if(PIO_ENABLE_LOGGING)
  set(ENABLE_LOGGING 1)
  set(HAS_LOGGING "yes")
else()
  set(ENABLE_LOGGING 0)
  set(HAS_LOGGING "no")
endif()


# Set a variable that appears in the config.h.in file.
if(PIO_ENABLE_NETCDF_INTEGRATION)
  set(NETCDF_INTEGRATION 1)
else()
  set(NETCDF_INTEGRATION 0)
endif()

#==============================================================================
#  PREPEND TO CMAKE MODULE PATH
#==============================================================================

#===== Local modules =====
list (APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

#===== External modules =====
if (PIO_ENABLE_FORTRAN)
  enable_language(Fortran)
  if (NOT DEFINED USER_CMAKE_MODULE_PATH)
    message (STATUS "Importing CMake_Fortran_utils")
    execute_process(
      COMMAND git clone https://github.com/CESM-Development/CMake_Fortran_utils
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      OUTPUT_QUIET
      ERROR_QUIET)
    find_path (USER_CMAKE_MODULE_PATH
      NAMES mpiexec.cmake
      HINTS ${CMAKE_BINARY_DIR}/CMake_Fortran_utils)
    if (USER_CMAKE_MODULE_PATH)
      message (STATUS "Importing CMake_Fortran_utils - success")
    else ()
      message (FATAL_ERROR "Failed to import CMake_Fortran_utils")
    endif ()
  endif ()
  set (USER_CMAKE_MODULE_PATH ${USER_CMAKE_MODULE_PATH}
  CACHE STRING "Location of the CMake_Fortran_utils")
  list (APPEND CMAKE_MODULE_PATH ${USER_CMAKE_MODULE_PATH})
endif ()

INCLUDE (CheckTypeSize)


#===== MPI =====
if(PIO_USE_MPISERIAL)
  find_package(MPISERIAL COMPONENTS C REQUIRED)
  if(MPISERIAL_C_FOUND)
    set(CMAKE_REQUIRED_INCLUDES ${MPISERIAL_C_INCLUDE_DIRS})
  endif()
  set(USE_MPI_SERIAL 1)
else()
  find_package(MPI REQUIRED)
  set(CMAKE_REQUIRED_INCLUDES ${MPI_INCLUDE_PATH})
  set(USE_MPI_SERIAL 0)
endif()

set(CMAKE_EXTRA_INCLUDE_FILES "mpi.h")
check_type_size("MPI_Offset" SIZEOF_MPI_OFFSET)
set(CMAKE_EXTRA_INCLUDE_FILES)


#===== Library Variables =====
set(PIO_FILESYSTEM_HINTS IGNORE CACHE STRING "Filesystem hints (lustre or gpfs)")


#===== Testing Options =====
option(PIO_ENABLE_TESTS "Enable the testing builds" ON)
option(PIO_VALGRIND_CHECK "Enable memory leak check using valgrind" OFF)

#==============================================================================
#  BACKWARDS COMPATIBILITY
#==============================================================================

# Old NETCDF_DIR variable --> NetCDF_PATH
if (DEFINED NETCDF_DIR)
  set (NetCDF_PATH ${NETCDF_DIR}
    CACHE STRING "Location of the NetCDF library installation")
endif ()

# Old PNETCDF_DIR variable --> PnetCDF_PATH
if (DEFINED PNETCDF_DIR)
  set (PnetCDF_PATH ${PNETCDF_DIR}
    CACHE STRING "Location of the PnetCDF library installation")
endif ()

#==============================================================================
#  HELPFUL GLOBAL VARIABLES
#==============================================================================

# System Name
string (TOUPPER "${CMAKE_SYSTEM_NAME}" CMAKE_SYSTEM_NAME_CAPS)
set (CMAKE_SYSTEM_DIRECTIVE "${CMAKE_SYSTEM_NAME_CAPS}"
  CACHE STRING "System name preprocessor directive")

# C Compiler Name
string (TOUPPER "${CMAKE_C_COMPILER_ID}" CMAKE_C_COMPILER_NAME)
if (CMAKE_C_COMPILER_NAME STREQUAL "XL")
  set (CMAKE_C_COMPILER_NAME "IBM")
endif ()
set (CMAKE_C_COMPILER_DIRECTIVE "CPR${CMAKE_C_COMPILER_NAME}"
  CACHE STRING "C compiler name preprocessor directive")

# Fortran Compiler Name
if (PIO_ENABLE_FORTRAN)
  string (TOUPPER "${CMAKE_Fortran_COMPILER_ID}" CMAKE_Fortran_COMPILER_NAME)
  if (CMAKE_Fortran_COMPILER_NAME STREQUAL "XL")
    set (CMAKE_Fortran_COMPILER_NAME "IBM")
  endif ()
  set (CMAKE_Fortran_COMPILER_DIRECTIVE "CPR${CMAKE_Fortran_COMPILER_NAME}"
    CACHE STRING "Fortran compiler name preprocessor directive")
endif()
#==============================================================================
#  SET CODE COVERAGE COMPILER FLAGS
#==============================================================================

# Only support GNU compilers at this time
if (PIO_ENABLE_COVERAGE)
  if (CMAKE_C_COMPILER_NAME STREQUAL "GNU")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
  else ()
    message (WARNING "The C compiler is non-GNU: coverage of C code could NOT be enabled")
  endif ()
  if (CMAKE_Fortran_COMPILER_NAME STREQUAL "GNU")
    set (CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -fprofile-arcs -ftest-coverage")
  else ()
    message (WARNING "The Fortran compiler is non-GNU: coverage of Fortran code could NOT be enabled")
  endif ()
endif ()

# Allow argument mismatch in gfortran versions > 10 for mpi library compatibility
if (CMAKE_Fortran_COMPILER_NAME STREQUAL "GNU")
  if ("${CMAKE_Fortran_COMPILER_VERSION}" VERSION_LESS 10)
    message (WARNING "gfortran version is ${CMAKE_Fortran_COMPILER_VERSION}")
  else()
    set (CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -fallow-argument-mismatch")
  endif()
endif()
# Include this so we can check values in netcdf_meta.h.
INCLUDE(CheckCSourceCompiles)



message("Fortran Library build is ${PIO_ENABLE_FORTRAN}")

# Always detect NetCDF Fortran if Fortran enabled

if(PIO_ENABLE_FORTRAN)
  find_package(NetCDF QUIET COMPONENTS C Fortran)
  if(NOT NetCDF_Fortran_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NetCDF_Fortran REQUIRED IMPORTED_TARGET "netcdf-fortran")
  endif()
  # Propagate NetCDF Fortran variables to subdirectories
  if(NetCDF_Fortran_FOUND)
    set(NetCDF_Fortran_INCLUDE_DIRS "${NetCDF_Fortran_INCLUDE_DIRS}" CACHE PATH "NetCDF Fortran include directory" FORCE)
    set(NetCDF_Fortran_LIBRARIES "${NetCDF_Fortran_LIBRARIES}" CACHE STRING "NetCDF Fortran libraries" FORCE)
  endif()
endif()
if(WITH_PNETCDF)
  find_program(PNETCDF_CONFIG pnetcdf-config REQUIRED)

  execute_process(
    COMMAND "${PNETCDF_CONFIG}" --includedir
    OUTPUT_VARIABLE PNETCDF_INCLUDEDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  execute_process(
    COMMAND "${PNETCDF_CONFIG}" --libdir
    OUTPUT_VARIABLE PNETCDF_LIBDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )


  execute_process(
    COMMAND "${PNETCDF_CONFIG}" --libs
    OUTPUT_VARIABLE PNETCDF_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  # If PNETCDF_LIBS is empty but PNETCDF_LIBDIR is set, set a default
  if(NOT PNETCDF_LIBS AND PNETCDF_LIBDIR)
    set(PNETCDF_LIBS "-L${PNETCDF_LIBDIR} -lpnetcdf")
  endif()

  # Propagate PnetCDF variables to subdirectories
  set(PNETCDF_INCLUDEDIR "${PNETCDF_INCLUDEDIR}" CACHE PATH "PnetCDF include directory" FORCE)
  set(PNETCDF_LIBDIR "${PNETCDF_LIBDIR}" CACHE PATH "PnetCDF library directory" FORCE)
  set(PNETCDF_LIBS "${PNETCDF_LIBS}" CACHE STRING "PnetCDF libraries" FORCE)

  # Modern CMake: create imported interface target for PnetCDF
  add_library(PnetCDF::PnetCDF INTERFACE IMPORTED)
  target_include_directories(PnetCDF::PnetCDF INTERFACE "${PNETCDF_INCLUDEDIR}")
  target_link_directories(PnetCDF::PnetCDF INTERFACE "${PNETCDF_LIBDIR}")
  target_link_libraries(PnetCDF::PnetCDF INTERFACE ${PNETCDF_LIBS})
endif()

# Set PnetCDF_C_FOUND to TRUE if we have the include dir and libs, and propagate to subdirectories
if(PNETCDF_INCLUDEDIR AND PNETCDF_LIBDIR AND PNETCDF_LIBS)
  set(PnetCDF_C_FOUND TRUE CACHE BOOL "PnetCDF C found via pnetcdf-config" FORCE)
else()
  set(PnetCDF_C_FOUND FALSE CACHE BOOL "PnetCDF C not found" FORCE)
endif()

# Did we find pnetCDF? If so, set _PNETCDF in config.h.
if (PnetCDF_C_FOUND)
  set(_PNETCDF 1)
endif ()

#==============================================================================
#  INCLUDE SOURCE DIRECTORIES
#==============================================================================


# Libraries
add_subdirectory(src)

#==============================================================================
#  TESTING TARGET
#==============================================================================


# Custom "piotests" target (builds the test executables)
add_custom_target(tests)
if(PIO_ENABLE_FORTRAN)
  add_dependencies(tests pioc piof)
else()
  add_dependencies(tests pioc)
endif()

# Custom "check" target that depends upon "tests"
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
add_dependencies(check tests)


# Tests
if(PIO_ENABLE_TESTS)
  enable_testing()
  include(CTest)
  add_subdirectory(tests)
  if(PIO_ENABLE_EXAMPLES)
    add_subdirectory(examples)
  endif()
endif()

# Documentation
if(PIO_ENABLE_DOC)
  add_subdirectory(doc)
endif()

SET(STATUS_PNETCDF ${PnetCDF_C_FOUND})

###
# Check to see if netcdf-4 capability is present in netcdf-c.
###
CHECK_C_SOURCE_COMPILES("
#include <netcdf_meta.h>
#if !defined(NC_VERSION_MAJOR) || NC_VERSION_MAJOR < 4
  choke me
#endif
int main() {return 0;}" HAVE_NETCDF4)

###
# Check to see if netcdf-4 parallel I/O capability is present in
# netcdf-c. (Really we should be checking NC_HAS_PARALLEL4, but that
# was only recently introduced, so we will go with NC_HAS_PARALLEL.)
###
set(CMAKE_REQUIRED_INCLUDES ${NetCDF_C_INCLUDE_DIRS})
CHECK_C_SOURCE_COMPILES("
#include <netcdf_meta.h>
#if !NC_HAS_PARALLEL
      choke me
#endif
int main() {return 0;}" HAVE_NETCDF_PAR)

# Set this synonym for HAVE_NETCDF_PAR. It is defined in config.h.
if (HAVE_NETCDF_PAR)
  set(_NETCDF4 1)
endif ()


###
# Check to see if szip write capability is present in netcdf-c.
###
if(NetCDF_C_FOUND AND NetCDF_C_INCLUDE_DIRS)
  set(CMAKE_REQUIRED_INCLUDES ${NetCDF_C_INCLUDE_DIRS})
  CHECK_C_SOURCE_COMPILES("
#include <netcdf_meta.h>
#if !NC_HAS_SZIP_WRITE
      choke me
#endif
int main() {return 0;}" USE_SZIP)
else()
  set(USE_SZIP FALSE)
endif()

###
# Check to see if parallel filters are supported by HDF5/netcdf-c.
###

if(HAVE_NETCDF_PAR AND NetCDF_C_FOUND AND NetCDF_C_INCLUDE_DIRS)
  set(CMAKE_REQUIRED_INCLUDES ${NetCDF_C_INCLUDE_DIRS})
  CHECK_C_SOURCE_COMPILES("
#include <netcdf_meta.h>
#if !NC_HAS_PAR_FILTERS
    choke me
#endif
int main() {return 0;}" HAVE_PAR_FILTERS)
else()
  set(HAVE_PAR_FILTERS 0)
endif()

###
# Check to see if this is netcdf-c-4.7.2, which won't work.
###
CHECK_C_SOURCE_COMPILES("
#include <netcdf_meta.h>
#if NC_VERSION_MAJOR == 4 && NC_VERSION_MINOR == 7 && NC_VERSION_PATCH == 2
#else
      choke me
#endif
int main() {return 0;}" HAVE_472)
if (HAVE_472)
  message (FATAL_ERROR "PIO cannot build with netcdf-c-4.7.2, please upgrade your netCDF library")
endif ()

###
# Check to see if dispatch table is supported for netcdf integration.
###
CHECK_C_SOURCE_COMPILES("
#include <netcdf_meta.h>
#if NC_DISPATCH_VERSION < 2
      choke me
#endif
#if NC_DISPATCH_VERSION > 5
    choke me
#endif
int main() {return 0;}" HAVE_DISPATCH)

if (NETCDF_INTEGRATION)
  if (NOT HAVE_DISPATCH)
    message (FATAL_ERROR "The netcdf-c netcdf integration layer is incompatible with the one in this ParallelIO version")
  endif ()
  set(HAVE_NETCDF_INTEGRATION 1)
else ()
  set(HAVE_NETCDF_INTEGRATION 0)
endif ()

# Configure testing with MPIEXEC.
if (NOT WITH_MPIEXEC)
  set(WITH_MPIEXEC mpiexec)
endif()
#set(MPIEXEC "${WITH_MPIEXEC}" CACHE INTERNAL "")
set(MPIEXEC "${WITH_MPIEXEC}")
set_property(GLOBAL PROPERTY WITH_MPIEXEC "${WITH_MPIEXEC}")

#####
# Configure and print the libpio.settings file.
#####

# Get system configuration, Use it to determine osname, os release, cpu. These
# will be used when committing to CDash.
find_program(UNAME NAMES uname)
IF(UNAME)
  macro(getuname name flag)
    execute_process(COMMAND "${UNAME}" "${flag}" OUTPUT_VARIABLE "${name}")
  endmacro(getuname)
  getuname(osname -s)
  getuname(osrel  -r)
  getuname(cpu    -m)
  set(TMP_BUILDNAME "${osname}-${osrel}-${cpu}")
ENDIF()

# Set
SET(prefix ${CMAKE_INSTALL_PREFIX})
SET(exec_prefix ${CMAKE_INSTALL_PREFIX})
SET(libdir ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
SET(includedir ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})
SET(CC ${CMAKE_C_COMPILER})

# Set variables to mirror those used by autoconf.
# This way we don't need to maintain two separate template
# files.
SET(host_cpu "${cpu}")
SET(host_vendor "${osname}")
SET(host_os "${osrel}")
SET(abs_top_builddir "${CMAKE_CURRENT_BINARY_DIR}")
SET(abs_top_srcdir "${CMAKE_CURRENT_SOURCE_DIR}")

SET(CC_VERSION "${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
SET(FC_VERSION "${CMAKE_Fortran_COMPILER_ID} ${CMAKE_Fortran_COMPILER_VERSION}")
# Build *FLAGS for libpio.settings.  (CFLAGS, CPPFLAGS, FFLAGS promoted from src)
SET(LDFLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS_${CMAKE_BUILD_TYPE}}")

is_disabled(BUILD_SHARED_LIBS enable_static)
is_enabled(BUILD_SHARED_LIBS enable_shared)
is_enabled(HAVE_PAR_FILTERS have_par_filters)
is_enabled(USE_SZIP HAS_SZIP_WRITE)
is_enabled(STATUS_PNETCDF HAS_PNETCDF)
is_enabled(HAVE_H5Z_SZIP HAS_SZLIB)
is_enabled(HAVE_NETCDF4 HAS_NETCDF4)
is_enabled(HAVE_NETCDF_PAR HAS_NETCDF4_PAR)
is_enabled(HAVE_NETCDF_INTEGRATION HAS_NETCDF_INTEGRATION)
is_enabled(PIO_ENABLE_FORTRAN HAS_PIO_FORTRAN)

if(HAVE_PAR_FILTERS)
  SET(PIO_HAS_PAR_FILTERS 1)
endif()


# Generate file from template.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libpio.settings.in"
  "${CMAKE_CURRENT_BINARY_DIR}/libpio.settings"
  @ONLY)

# Read in settings file, print out.
# Avoid using system-specific calls so that this
# might also work on Windows.
FILE(READ "${CMAKE_CURRENT_BINARY_DIR}/libpio.settings"
  LIBPIO_SETTINGS)
MESSAGE(STATUS ${LIBPIO_SETTINGS})

# Set RPATH for shared libraries
set(CMAKE_MACOSX_RPATH 1)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")


# Install libpio.settings file into same location as the libraries.
install(FILES "${PIO_BINARY_DIR}/libpio.settings"
  DESTINATION lib
  COMPONENT libraries)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/PIOConfigVersion.cmake"
  VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
  COMPATIBILITY AnyNewerVersion)


configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PIOConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/PIOConfig.cmake"
  INSTALL_DESTINATION lib/cmake/PIO)


install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/PIOConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/PIOConfigVersion.cmake"
  DESTINATION lib/cmake/PIO)


#####
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/clib/pio_meta.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/src/clib/pio_meta.h"
  @ONLY
)
# Configure a header file to pass some of the CMake settings to the source code
configure_file(
  "${PROJECT_SOURCE_DIR}/cmake_config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  @ONLY
)
