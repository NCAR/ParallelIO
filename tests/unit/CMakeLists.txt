include (LibMPI)

include_directories("${SCORPIO_SOURCE_DIR}/tests/unit")

#==============================================================================
#  HELPER MACROS
#==============================================================================
macro (add_exe_with_libs_and_deps EXE_NAME)
  add_executable(${EXE_NAME} EXCLUDE_FROM_ALL ${ARGN})
  target_link_libraries(${EXE_NAME} PRIVATE piof)
  add_dependencies (tests ${EXE_NAME})
  if (PIO_USE_MPISERIAL)
    if (MPISERIAL_Fortran_FOUND)
      target_compile_definitions (${EXE_NAME} PRIVATE _MPISERIAL)
      target_include_directories (${EXE_NAME} PUBLIC ${MPISERIAL_Fortran_INCLUDE_DIRS})
      target_link_libraries (${EXE_NAME} PRIVATE ${MPISERIAL_Fortran_LIBRARIES})
      set (WITH_PNETCDF FALSE)
      set (MPI_Fortran_INCLUDE_PATH ${MPISERIAL_Fortran_INCLUDE_DIRS})
    endif ()
  endif ()
  if (NOT ${MPI_HAS_Fortran_MOD})
    target_compile_definitions (${EXE_NAME} PUBLIC NO_MPIMOD)
  endif ()
  if (GPTL_Fortran_Perf_FOUND)
    target_include_directories (${EXE_NAME} PUBLIC ${GPTL_Fortran_INCLUDE_DIRS})
    target_link_libraries (${EXE_NAME} PRIVATE ${GPTL_Fortran_LIBRARIES})
  else ()
    target_link_libraries (${EXE_NAME} PRIVATE gptl)
  endif ()
  target_compile_definitions (${EXE_NAME} PUBLIC TIMING)
  if (PIO_ENABLE_INTERNAL_TIMING)
    target_compile_definitions (${EXE_NAME} PUBLIC TIMING_INTERNAL)
  endif ()
  SET_PROPERTY(TARGET ${EXE_NAME} PROPERTY LINKER_LANGUAGE ${PIO_LINKER_LANGUAGE})
endmacro ()

#==============================================================================
#  TEST SETUP
#==============================================================================
# Copy files required for testing
file (COPY "./input.nl"
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file (COPY "./not_netcdf.ieee"
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

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

# Check for MPI Fortran module
find_path(MPIMOD_PATH
  NAMES mpi.mod MPI.mod
  HINTS ${MPI_Fortran_INCLUDE_PATH})
if (PIO_ENABLE_TIMING)
  find_package (GPTL COMPONENTS Fortran_Perf QUIET)
endif ()

check_macro (MPI_HAS_Fortran_MOD
  NAME TryMPIMod.f90
  HINTS ${CMAKE_MODULE_PATH}
  DEFINITIONS -I${MPIMOD_PATH}
  COMMENT "whether MPI Fortran module is supported")
if (${MPI_HAS_Fortran_MOD})
  message (STATUS "MPI Fortran module verified and enabled.")
else ()
  message (STATUS "MPI Fortran module failed verification and therefore disabled.")
endif ()

#===== GPTL =====
if (PIO_ENABLE_TIMING)
  find_package (GPTL COMPONENTS Fortran_Perf QUIET)
  if (GPTL_Fortran_Perf_FOUND)
    message (STATUS "Found GPTL Fortran Perf: ${GPTL_Fortran_Perf_LIBRARIES}")
  else ()
    message (STATUS "Using internal GPTL Fortran library for timing")
  endif ()
endif ()

#==============================================================================
#  SET COMPILER OPTIONS
#==============================================================================
string (TOUPPER "${CMAKE_Fortran_COMPILER_ID}" CMAKE_FORTRAN_COMPILER_NAME)
# The PIO library is written using C, C++ and Fortran languages
# IBM compilers require Fortran/C/C++ mixed language programs
# to be linked with the C++ linker. Other compilers require the
# Fortran linker.
if (CMAKE_FORTRAN_COMPILER_NAME STREQUAL "XL")
  SET(PIO_LINKER_LANGUAGE CXX)
else ()
  SET(PIO_LINKER_LANGUAGE Fortran)
endif ()

if (CMAKE_Fortran_COMPILER_ID STREQUAL "NAG")
  set ( CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -mismatch_all" )
  #    target_compile_options (gptl
  #        PRIVATE -mismatch_all)
endif ()

#==============================================================================
#  DEFINE THE TARGETS AND TESTS
#==============================================================================

set (SRCS basic_tests.F90
  driver.F90
  global_vars.F90
  ncdf_tests.F90)

add_exe_with_libs_and_deps (pio_unit_test ${SRCS})

#==============================================================================
#  SET TARGET SPECIFIC COMPILER OPTIONS
#==============================================================================
if ("${CMAKE_Fortran_COMPILER_ID}" STREQUAL "GNU")
  target_compile_options (pio_unit_test
    PRIVATE -ffree-line-length-none)
endif()

#==============================================================================
#  TESTING
#==============================================================================
# Test Timeout in seconds.
set (DEFAULT_TEST_TIMEOUT 60)

# All tests need a certain number of tasks, but they should be able to
# run successfully with more than they need. Test this by providing an
# extra processor for each C test.
set (AT_LEAST_TWO_TASKS 3)
set (AT_LEAST_THREE_TASKS 4)
set (AT_LEAST_FOUR_TASKS 5)

if (PIO_USE_MPISERIAL)
  add_test(NAME pio_unit_test
    COMMAND pio_unit_test)
  set_tests_properties(pio_unit_test
    PROPERTIES TIMEOUT ${DEFAULT_TEST_TIMEOUT})
else ()
  add_mpi_test(pio_unit_test
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_unit_test
    NUMPROCS 4
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
endif ()

if (PIO_HDF5_LOGGING)
  target_compile_definitions (pio_unit_test
    PUBLIC LOGGING)
endif ()
