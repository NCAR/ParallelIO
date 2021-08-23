cmake_minimum_required (VERSION 2.8.12)
project (SCORPIO C CXX Fortran)
#cmake_policy(VERSION 3.5.2)

message(STATUS "===== Configuring SCORPIO... =====")

# The project version number.
set(VERSION_MAJOR   1   CACHE STRING "Project major version number.")
set(VERSION_MINOR   2   CACHE STRING "Project minor version number.")
set(VERSION_PATCH   2   CACHE STRING "Project patch version number.")
mark_as_advanced(VERSION_MAJOR VERSION_MINOR VERSION_PATCH)

#==============================================================================
#  ADD CMAKE MODULES IN SCORPIO TO CMAKE MODULE PATH
#==============================================================================

#===== Local modules =====
list (APPEND CMAKE_MODULE_PATH ${SCORPIO_SOURCE_DIR}/cmake)

#===== External modules =====
if (DEFINED USER_CMAKE_MODULE_PATH)
  set (USER_CMAKE_MODULE_PATH ${USER_CMAKE_MODULE_PATH}
    CACHE STRING "Location of the CMake_Fortran_utils")
  list (APPEND CMAKE_MODULE_PATH ${USER_CMAKE_MODULE_PATH})
endif ()

#==============================================================================
#  USER-DEFINED OPTIONS (set with "-DOPT=VAL" from command line)
#==============================================================================

#===== Library Options =====
option (PIO_ENABLE_FORTRAN   "Enable the Fortran library builds"            ON)
option (PIO_ENABLE_TIMING    "Enable the use of the GPTL timing library"    ON)
option (PIO_ENABLE_INTERNAL_TIMING    "Gather and print GPTL timing stats"  OFF)
option (PIO_ENABLE_IO_STATS  "Gather and print I/O performance stats"       ON)
option (PIO_ENABLE_LOGGING   "Enable debug logging (large output possible)" OFF)
option (PIO_ENABLE_DOC       "Enable building SCORPIO documentation"        ON)
option (PIO_ENABLE_COVERAGE  "Enable code coverage"                         OFF)
option (PIO_ENABLE_TOOLS     "Enable SCORPIO tools"                         ON)
option (PIO_ENABLE_EXAMPLES  "Enable SCORPIO examples"                      OFF)
option (PIO_INTERNAL_DOC     "Enable SCORPIO developer documentation"       OFF)
option (PIO_TEST_BIG_ENDIAN  "Enable test to see if machine is big endian"  ON)
option (PIO_USE_MPISERIAL    "Enable mpi-serial support (instead of MPI)"   OFF)
option (PIO_USE_MALLOC       "Use native malloc (instead of bget package)"  OFF)
option (PIO_MICRO_TIMING     "Enable internal micro timers"                 OFF)
option (PIO_SAVE_DECOMPS     "Dump the decomposition information"           OFF)
option (PIO_LIMIT_CACHED_IO_REGIONS  "Limit the number of non-contiguous regions in an IO process" OFF)
option (WITH_PNETCDF         "Require the use of PnetCDF"                   ON)
option (WITH_NETCDF          "Require the use of NetCDF"                    ON)
option (WITH_ADIOS2          "Require the use of ADIOS 2.x"                 OFF)
option (ADIOS_BP2NC_TEST     "Enable testing of BP to NetCDF conversion"    OFF)
#===== Testing Options =====
option (PIO_ENABLE_TESTS  "Enable the testing builds"                       OFF)
option (PIO_ENABLE_LARGE_TESTS  "Enable large (file, processes) tests"      OFF)
option (PIO_VALGRIND_CHECK  "Enable memory leak check using valgrind"       OFF)
#===== Dependent Options =====
include(CMakeDependentOption)
cmake_dependent_option (PIO_TEST_CLOSE_OPEN_FOR_SYNC  "SCORPIO fortran tests will close+open for sync" ON "WITH_ADIOS2" OFF)

# Backwards Compatibility - allow users to define [NETCDF|PNETCDF]_DIR instead
# of [NetCDF|PnetCDF]_PATH
# Old NETCDF_DIR variable --> NetCDF_PATH
if (DEFINED NETCDF_DIR)
  set (NetCDF_PATH ${NETCDF_DIR}
    CACHE STRING "Location of the NetCDF library installation")
  message(DEPRECATION "Using NETCDF_DIR to specify the install path to NetCDF is deprecated. Use NetCDF_PATH instead")
endif ()

# Old PNETCDF_DIR variable --> PnetCDF_PATH
if (DEFINED PNETCDF_DIR)
  set (PnetCDF_PATH ${PNETCDF_DIR}
    CACHE STRING "Location of the PnetCDF library installation")
  message(DEPRECATION "Using PNETCDF_DIR to specify the install path to PnetCDF is deprecated. Use PnetCDF_PATH instead")
endif ()

#==============================================================================
#  SET VARIABLES FOR pio_config.h (generated from pio_config.h.in)
#==============================================================================
# The tools and examples do not build with the MPI serial library
if(PIO_USE_MPISERIAL)
  if(PIO_ENABLE_TOOLS)
    message(WARNING "Scorpio tools are not supported by the MPI serial library, disabling tools")
    set(PIO_ENABLE_TOOLS 0)
  endif()
  if(PIO_ENABLE_EXAMPLES)
    message(WARNING "Scorpio examples are not supported by the MPI serial library, disabling building examples")
    set(PIO_ENABLE_EXAMPLES 0)
  endif()
endif()

if(PIO_ENABLE_IO_STATS)
  set(ENABLE_IO_STATS 1)
  message(STATUS "Enabling SCORPIO I/O performance statistics collection (default)")
else()
  message(STATUS "Disabling Scorpio I/O performance statistics collection")
  set(ENABLE_IO_STATS 0)
endif()

if(PIO_USE_MALLOC)
  set(USE_MALLOC 1)
  message(STATUS "Using malloc to allocate memory for caching data in SCORPIO (default)")
else()
  set(USE_MALLOC 0)
  message(STATUS "Using BGET to allocate memory for caching data in SCORPIO")
endif()

if(PIO_ENABLE_LOGGING)
  set(ENABLE_LOGGING 1)
  message(STATUS "Enabling debug logging in SCORPIO")
else()
  set(ENABLE_LOGGING 0)
  message(STATUS "Disabling debug logging in SCORPIO (default)")
endif()

if(PIO_USE_MPISERIAL)
  set(USE_MPI_SERIAL 1)
  message(STATUS "Configuring SCORPIO to use the MPI serial library")
else()
  set(USE_MPI_SERIAL 0)
  message(STATUS "Disabling use/check of the MPI serial library (default)")
endif()

if(PIO_SAVE_DECOMPS)
  set(SAVE_DECOMPS 1)
  if(DEFINED PIO_SAVE_DECOMPS_REGEX)
    message (STATUS "Saving I/O decompositions using regex : " ${PIO_SAVE_DECOMPS_REGEX})
  else()
    set(PIO_SAVE_DECOMPS_REGEX "*")
    message (STATUS "Saving ALL I/O decompositions")
  endif()
else()
  set(SAVE_DECOMPS 0)
  set(PIO_SAVE_DECOMPS_REGEX "")
  message (STATUS "Disabling saving I/O decompositions (default)")
endif()

if(PIO_LIMIT_CACHED_IO_REGIONS)
  set(LIMIT_CACHED_IO_REGIONS 1)
  if(NOT DEFINED PIO_MAX_CACHED_IO_REGIONS)
    set(PIO_MAX_CACHED_IO_REGIONS 65536)
  endif()
  message(STATUS "Limiting number of cached I/O regions to PIO_MAX_CACHED_IO_REGIONS = ${PIO_MAX_CACHED_IO_REGIONS}")
else()
  set(LIMIT_CACHED_IO_REGIONS 0)
  set(PIO_MAX_CACHED_IO_REGIONS 0)
  message(STATUS "No limit on the number of cached I/O regions (default)")
endif()

if(DEFINED PIO_MAX_LUSTRE_OSTS)
  message (STATUS "Limiting the number of Lustre OSTs used to PIO_MAX_LUSTRE_OSTS = " ${PIO_MAX_LUSTRE_OSTS})
else()
  cmake_host_system_information (RESULT FQDN_SITENAME QUERY FQDN)
  if(FQDN_SITENAME MATCHES "^cori")
    set(PIO_MAX_LUSTRE_OSTS 72)
    message (STATUS "Limiting the number of Lustre OSTs used to PIO_MAX_LUSTRE_OSTS = " ${PIO_MAX_LUSTRE_OSTS} " (default for Cori)")
  elseif(FQDN_SITENAME MATCHES "^theta")
    set(PIO_MAX_LUSTRE_OSTS 48)
    message (STATUS "Limiting the number of Lustre OSTs used to  PIO_MAX_LUSTRE_OSTS = " ${PIO_MAX_LUSTRE_OSTS} " (default for Theta)")
  else()
    set(PIO_MAX_LUSTRE_OSTS 0)
    message (STATUS "Limit on the number of Lustre OSTs, PIO_MAX_LUSTRE_OSTS, is not set (default)")
  endif()
endif()

if(DEFINED PIO_STRIPING_UNIT)
  message (STATUS "Using filesystem striping unit, PIO_STRIPING_UNIT = " ${PIO_STRIPING_UNIT})
else()
  cmake_host_system_information (RESULT FQDN_SITENAME QUERY FQDN)
  if(FQDN_SITENAME MATCHES "^cori" OR
     FQDN_SITENAME MATCHES "^theta")
    set(PIO_STRIPING_UNIT 1048576)
    message (STATUS "Using filesystem striping unit, PIO_STRIPING_UNIT = " ${PIO_STRIPING_UNIT} " (default for Cori/Theta)")
  elseif(FQDN_SITENAME MATCHES "^.*[.]olcf")
    set(PIO_STRIPING_UNIT 16777216)
    message (STATUS "Using filesystem striping unit, PIO_STRIPING_UNIT = " ${PIO_STRIPING_UNIT} " (default for OLCF machines)")
  else()
    set(PIO_STRIPING_UNIT 0)
    message (STATUS "Filesystem striping unit is not set (default)")
  endif()
endif()

#==============================================================================
#  DETECT SYSTEM/COMPILERS (and set compiler specific options)
#==============================================================================

# System Name
string (TOUPPER "${CMAKE_SYSTEM_NAME}" CMAKE_SYSTEM_NAME_CAPS)
set (CMAKE_SYSTEM_DIRECTIVE "${CMAKE_SYSTEM_NAME_CAPS}"
  CACHE STRING "System name preprocessor directive")

# Set compiler options as global variables
# C Compiler Name
string (TOUPPER "${CMAKE_C_COMPILER_ID}" CMAKE_C_COMPILER_NAME)
if (CMAKE_C_COMPILER_NAME STREQUAL "XL")
  set (CMAKE_C_COMPILER_NAME "IBM")
endif ()
set (CMAKE_C_COMPILER_DIRECTIVE "CPR${CMAKE_C_COMPILER_NAME}"
  CACHE STRING "C compiler name preprocessor directive")

# Fortran Compiler Name
string (TOUPPER "${CMAKE_Fortran_COMPILER_ID}" CMAKE_Fortran_COMPILER_NAME)
if (CMAKE_Fortran_COMPILER_NAME STREQUAL "XL")
  set (CMAKE_Fortran_COMPILER_NAME "IBM")
endif ()
set (CMAKE_Fortran_COMPILER_DIRECTIVE "CPR${CMAKE_Fortran_COMPILER_NAME}"
  CACHE STRING "Fortran compiler name preprocessor directive")

# Check Min version of compilers required
# Regex is only available in GCC 4.9+ (>= 4.9)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9")
    message (WARNING "C++11 regex support is disabled since the compiler(" ${CMAKE_CXX_COMPILER_ID} ", version=" ${CMAKE_CXX_COMPILER_VERSION} ") does not support it")
    add_definitions(-DSPIO_NO_CXX_REGEX)
  endif()
endif ()

# Disable C++11 regex usage for IBM XL and PGI compilers
# The IBM XL compiler (16.1.1-8) and the PGI compiler (20.1) on Summit,
# and the PGI compiler (19.10) on compy have a buggy support for
# C++11 regex
string (TOUPPER "${CMAKE_CXX_COMPILER_ID}" CMAKE_CXX_COMPILER_NAME)
if ((CMAKE_CXX_COMPILER_NAME STREQUAL "XL") OR
    (CMAKE_CXX_COMPILER_NAME STREQUAL "PGI"))
  message (WARNING "C++11 regex support is disabled since some versions of the compiler(" ${CMAKE_CXX_COMPILER_ID} ") on Summit/Compy do not support it")
  add_definitions(-DSPIO_NO_CXX_REGEX)
endif()

# Min versions of libraries required
set (MPE_MIN_VER_REQD "2.4.8")
set (NETCDF_C_MIN_VER_REQD "4.3.3")
set (NETCDF_FORTRAN_MIN_VER_REQD "4.3.3")
set (PNETCDF_MIN_VER_REQD "1.8.1")
set (ADIOS_MIN_VER_REQD "2.7.0")

# Setting code coverage compiler flags (only GNU is supported for now)
if (PIO_ENABLE_COVERAGE)
  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    message (STATUS "Enabling code coverage for C...")
  else ()
    message (WARNING "The C compiler is non-GNU, coverage of C code could NOT be enabled (only GNU is supported for now)")
  endif ()
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    message (STATUS "Enabling code coverage for C++...")
  else ()
    message (WARNING "The C++ compiler is non-GNU, coverage of C++ code could NOT be enabled (only GNU is supported for now)")
  endif ()
  if (CMAKE_Fortran_COMPILER_ID STREQUAL "GNU")
    set (CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -fprofile-arcs -ftest-coverage")
    message (STATUS "Enabling code coverage for Fortran...")
  else ()
    message (WARNING "The Fortran compiler is non-GNU, coverage of Fortran code could NOT be enabled (only GNU is supported for now)")
  endif ()
else ()
  message(STATUS "Disabling code coverage... (use -DPIO_ENABLE_COVERAGE:BOOL=ON to enable coverage, only GNU is supported for now)")
endif ()

#==============================================================================
# ADDING SOURCE & TOOLS
#==============================================================================

# Libraries
add_subdirectory (src)

# Tools
if (PIO_ENABLE_TOOLS)
  message(STATUS "Enabling SCORPIO tools... (default, use -DPIO_ENABLE_TOOLS:BOOL=OFF to disable tools)")
  add_subdirectory(tools)
else ()
  message(STATUS "Disabling SCORPIO tools... (use -DPIO_ENABLE_TOOLS:BOOL=ON to enable tools)")
endif ()

#==============================================================================
#  TESTING TARGET
#==============================================================================

# Tests
if (PIO_ENABLE_TESTS)
  message(STATUS "Enabling SCORPIO tests... ")
  # Custom "piotests" target (builds the test executables)
  add_custom_target (tests)

  # Custom "check" target that depends upon "tests"
  add_custom_target (check COMMAND ${CMAKE_CTEST_COMMAND})
  add_dependencies (check tests)

  enable_testing()
  include (CTest)
  add_subdirectory (tests)
else ()
  message(STATUS "Disabling SCORPIO tests... (default, use -DPIO_ENABLE_TESTS:BOOL=ON to enable tests)")
endif ()

#==============================================================================
#  EXAMPLES
#==============================================================================

if (PIO_ENABLE_EXAMPLES)
  message(STATUS "Enabling SCORPIO Examples...")
  add_subdirectory (examples)
else ()
  message(STATUS "Disabling SCORPIO Examples... (default, use -DPIO_ENABLE_EXAMPLES:BOOL=ON to enable examples)")
endif ()

#==============================================================================
#  DOCUMENTATION
#==============================================================================

if (PIO_ENABLE_DOC)
  message(STATUS "Enabling SCORPIO Documentation...")
  add_subdirectory (doc)
else ()
  message(STATUS "Disabling SCORPIO Documentation... (default, use -DPIO_ENABLE_DOC:BOOL=ON to enable documentation)")
endif ()
