cmake_minimum_required (VERSION 2.8.12)
project (PIOF Fortran)
include (CheckFunctionExists)
include (ExternalProject)
include (LibCheck)

message(STATUS "===== Configuring SCORPIO Fortran interface... =====")
#==============================================================================
#  CONFIGURE CHECKS
#==============================================================================
# Check if c_sizeof is available
check_macro (Fortran_CSIZEOF
  NAME TryCSizeOf.f90
  HINTS ${CMAKE_MODULE_PATH}
  COMMENT "whether the Fortran compiler supports c_sizeof")

# The genf90 tool generates *.F90 from *.F90.in
if (NOT DEFINED GENF90_PATH)
  message(STATUS "Using internal version of genf90 tool")
  set (GENF90_PATH ${SCORPIO_SOURCE_DIR}/src/genf90)
else ()
  if (EXISTS "${GENF90_PATH}/genf90.pl")
    message(STATUS "Using user-specified version of genf90 tool : ${GENF90_PATH}/genf90.pl")
  else ()
    message(WARNING "Could not find genf90.pl at user-specified path, GENF90_PATH = ${GENF90_PATH}")
    message(WARNING "Resetting to using internal version of genf90.pl (${SCORPIO_SOURCE_DIR}/src/genf90)")
    set (GENF90_PATH ${SCORPIO_SOURCE_DIR}/src/genf90)
  endif ()
endif ()

add_custom_target(genf90
  DEPENDS ${GENF90_PATH}/genf90.pl)

set (PIO_GenF90_SRCS pionfatt_mod.F90
  pionfput_mod.F90
  pionfget_mod.F90
  pio_support.F90
  piodarray.F90)

# Generate Fortran source from template files using genf90
foreach (SRC_FILE IN LISTS PIO_GenF90_SRCS)
  add_custom_command (OUTPUT ${SRC_FILE}
    COMMAND ${GENF90_PATH}/genf90.pl
    ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_FILE}.in > ${SRC_FILE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_FILE}.in genf90)
endforeach ()

#==============================================================================
#  DEFINE THE TARGET LIBRARY
#==============================================================================

set (PIO_Fortran_SRCS pio_nf.F90
  pio.F90
  pio_kinds.F90
  pio_types.F90
  piolib_mod.F90
  pio_nf_utils.F90)

add_library (piof ${PIO_Fortran_SRCS} ${PIO_GenF90_SRCS})
if (NOT PIO_ENABLE_FORTRAN)
  set_target_properties(piof PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif ()
add_dependencies (piof genf90)

#==============================================================================
#  SET THE COMPILER OPTIONS
#==============================================================================
# Include flib source and binary directories (for Fortran modules)
target_include_directories (piof
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# System and compiler CPP directives
target_compile_definitions (piof
  PRIVATE ${CMAKE_SYSTEM_DIRECTIVE})
target_compile_definitions (piof
  PUBLIC ${CMAKE_Fortran_COMPILER_DIRECTIVE})

# Compiler-specific compile options
if ("${CMAKE_Fortran_COMPILER_ID}" STREQUAL "GNU")
  target_compile_options (piof
    PRIVATE -ffree-line-length-none)
elseif (CMAKE_Fortran_COMPILER_ID STREQUAL "NAG")
  set ( CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -mismatch_all" )
  #    target_compile_options (piof
  #        PRIVATE -mismatch_all)
endif()

if (NOT Fortran_CSIZEOF)
  target_compile_definitions (piof
    PUBLIC NO_C_SIZEOF)
endif()

# Add user-specified include/libs/compiler/link options
target_include_directories (piof
  PUBLIC ${PIO_Fortran_EXTRA_INCLUDE_DIRS})
target_link_libraries (piof
  PUBLIC ${PIO_Fortran_EXTRA_LIBRARIES})
target_compile_options (piof
  PRIVATE ${PIO_Fortran_EXTRA_COMPILE_OPTIONS})
target_compile_definitions (piof
  PUBLIC ${PIO_Fortran_EXTRA_COMPILE_DEFINITIONS})
if (PIO_Fortran_EXTRA_LINK_FLAGS)
  set_target_properties(piof PROPERTIES
    LINK_FLAGS ${PIO_Fortran_EXTRA_LINK_FLAGS})
endif ()

#==============================================================================
#  FIND EXTERNAL LIBRARIES/DEPENDENCIES
#==============================================================================

# Find the MPI library
if (PIO_USE_MPISERIAL)
  find_package (MPISERIAL COMPONENTS Fortran REQUIRED)
  if (MPISERIAL_Fortran_FOUND)
    message(STATUS "MPI serial Fortran library dependencies : ${MPISERIAL_Fortran_LIBRARIES}")
    target_compile_definitions (piof
      PRIVATE _MPISERIAL)
    target_include_directories (piof
      PUBLIC ${MPISERIAL_Fortran_INCLUDE_DIRS})
    target_link_libraries (piof
      PUBLIC ${MPISERIAL_Fortran_LIBRARIES})

    set (WITH_PNETCDF FALSE)
    set (MPI_Fortran_INCLUDE_PATH ${MPISERIAL_Fortran_INCLUDE_DIRS})
  else ()
    message(WARNING "Could not find MPI serial library")
  endif ()
else ()
  find_package (MPI REQUIRED)
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
  if (PIO_ENABLE_TIMING AND NOT GPTL_Fortran_Perf_FOUND)
    target_compile_definitions (gptl
      PUBLIC NO_MPIMOD)
  endif()
endif ()

#===== GPTL =====
if (PIO_ENABLE_TIMING)
  find_package (GPTL COMPONENTS Fortran_Perf)
  if (GPTL_Fortran_Perf_FOUND)
    message (STATUS "GPTL Fortran library dependencies: ${GPTL_Fortran_Perf_LIBRARIES}")
    target_include_directories (piof
      PUBLIC ${GPTL_Fortran_Perf_INCLUDE_DIRS})
    target_link_libraries (piof
      PUBLIC ${GPTL_Fortran_Perf_LIBRARIES})
  else ()
    message (STATUS "Using internal GPTL Fortran library for timing")
    message (STATUS "GPTL Fortran library dependencies: -lgptl")
    target_link_libraries (piof
      PUBLIC gptl)
  endif ()
endif ()

#===== NetCDF-Fortran =====
if (WITH_NETCDF)
  find_package (NetCDF ${NETCDF_FORTRAN_MIN_VER_REQD} COMPONENTS Fortran)
  if (NetCDF_Fortran_FOUND)
    message(STATUS "NetCDF Fortran library dependencies: ${NetCDF_Fortran_LIBRARIES}")
    target_include_directories (piof
      PUBLIC ${NetCDF_Fortran_INCLUDE_DIRS})
    target_compile_definitions (piof
      PUBLIC _NETCDF)
    target_link_libraries (piof
      PUBLIC ${NetCDF_Fortran_LIBRARIES})
    if (EXISTS ${NetCDF_Fortran_INCLUDE_DIR}/netcdf_par.h)
      target_compile_definitions (piof
        PUBLIC _NETCDF4)
    endif ()
  else ()
    message(STATUS "NetCDF Fortran library not found. Disabling support for NetCDF")
    target_compile_definitions (piof
      PUBLIC _NONETCDF)
  endif ()
else ()
  message(STATUS "Disabling support for NetCDF")
  target_compile_definitions (piof PUBLIC _NONETCDF)
endif ()

#===== PnetCDF =====
if (WITH_PNETCDF)
  find_package (PnetCDF ${PNETCDF_MIN_VER_REQD} COMPONENTS Fortran)
  if (PnetCDF_FOUND)
    message(STATUS "PnetCDF Fortran library dependencies: ${PnetCDF_Fortran_LIBRARIES}")
    target_include_directories (piof
      PUBLIC ${PnetCDF_Fortran_INCLUDE_DIRS})
    target_compile_definitions (piof
      PUBLIC _PNETCDF)
    target_link_libraries (piof
      PUBLIC ${PnetCDF_Fortran_LIBRARIES})

    # Check library for varn functions
    set (CMAKE_REQUIRED_LIBRARIES ${PnetCDF_Fortran_LIBRARY})
    check_function_exists (ncmpi_get_varn PnetCDF_Fortran_HAS_VARN)
    if (PnetCDF_Fortran_HAS_VARN)
      target_compile_definitions(piof
        PUBLIC USE_PNETCDF_VARN
        PUBLIC USE_PNETCDF_VARN_ON_READ)
    endif()
  else ()
    message(STATUS "PnetCDF Fortran library not found. Disabling support for PnetCDF")
    target_compile_definitions (piof
      PUBLIC _NOPNETCDF)
  endif ()
else ()
  message(STATUS "Disabling support for PnetCDF")
  target_compile_definitions (piof PUBLIC _NOPNETCDF)
endif ()

# Link with libpioc.a
target_link_libraries(piof
  PUBLIC pioc)

# We expect PnetCDF or NetCDF to be available
if (NOT PnetCDF_Fortran_FOUND AND NOT NetCDF_Fortran_FOUND)
  message (FATAL_ERROR "SCORPIO requires PnetCDF and/or NetCDF Fortran libraries")
endif ()

#==============================================================================
#  INSTALL
#==============================================================================
set (PIO_Fortran_MODS ${CMAKE_CURRENT_BINARY_DIR}/pio.mod
  ${CMAKE_CURRENT_BINARY_DIR}/pio_nf.mod
  ${CMAKE_CURRENT_BINARY_DIR}/pio_types.mod
  ${CMAKE_CURRENT_BINARY_DIR}/piolib_mod.mod
  ${CMAKE_CURRENT_BINARY_DIR}/pionfget_mod.mod
  ${CMAKE_CURRENT_BINARY_DIR}/pio_kinds.mod
  ${CMAKE_CURRENT_BINARY_DIR}/pio_support.mod
  ${CMAKE_CURRENT_BINARY_DIR}/piodarray.mod
  ${CMAKE_CURRENT_BINARY_DIR}/pionfatt_mod.mod
  ${CMAKE_CURRENT_BINARY_DIR}/pionfput_mod.mod)

# Install libpiof.a
install (TARGETS piof DESTINATION lib)

# Install the Fortran modules
install (FILES ${PIO_Fortran_MODS} DESTINATION include)

