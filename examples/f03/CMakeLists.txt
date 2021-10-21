###-------------------------------------------------------------------------###
### CMakeList.txt for examples using pio
###-------------------------------------------------------------------------###
#==============================================================================
#  HELPER MACROS
#==============================================================================
macro (add_exe_with_libs_and_deps EXE_NAME)
  add_executable(${EXE_NAME} ${ARGN})
  target_link_libraries(${EXE_NAME} PRIVATE piof)
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
#  SET THE COMPILER OPTIONS
#==============================================================================
# Adding PIO definitions - defined in the root directory
ADD_DEFINITIONS(${PIO_DEFINITIONS})

# Include PIO include and lib directories
INCLUDE_DIRECTORIES(${PIO_INCLUDE_DIRS})
LINK_DIRECTORIES(${PIO_LIB_DIR})

string (TOUPPER "${CMAKE_Fortran_COMPILER_ID}" CMAKE_FORTRAN_COMPILER_NAME)

set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -g -O0")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")

# The PIO library is written using C, C++ and Fortran languages
# IBM compilers require Fortran/C/C++ mixed language programs
# to be linked with the C++ linker. Other compilers require the
# Fortran linker.
if (CMAKE_FORTRAN_COMPILER_NAME STREQUAL "XL")
  SET(PIO_LINKER_LANGUAGE CXX)
else ()
  SET(PIO_LINKER_LANGUAGE Fortran)
endif ()

#==============================================================================
#  FIND EXTERNAL LIBRARIES/DEPENDENCIES
#==============================================================================

# Find the MPI library
if (PIO_USE_MPISERIAL)
  find_package (MPISERIAL COMPONENTS Fortran REQUIRED)
else ()
  find_package (MPI REQUIRED)
endif ()

# Check for MPI Fortran module
find_path(MPIMOD_PATH
  NAMES mpi.mod MPI.mod
  HINTS ${MPI_Fortran_INCLUDE_PATH})

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
#  BUILD EXECUTABLE
#==============================================================================
add_exe_with_libs_and_deps(examplePio_f90 examplePio.f90)
