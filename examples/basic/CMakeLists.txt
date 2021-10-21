#==============================================================================
#  HELPER MACROS
#==============================================================================
macro (add_exe_with_libs_and_deps EXE_NAME IS_C_SRC)
  add_executable(${EXE_NAME} ${ARGN})
  if (${IS_C_SRC}) 
    target_link_libraries(${EXE_NAME} PRIVATE pioc)
  else ()
    # Assume Fortran
    target_link_libraries(${EXE_NAME} PRIVATE piof)
  endif ()
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
  if (GPTL_C_FOUND AND IS_C_SRC)
    target_include_directories (${EXE_NAME}
      PUBLIC ${GPTL_C_INCLUDE_DIRS})
    target_link_libraries (${EXE_NAME} PRIVATE ${GPTL_C_LIBRARIES})
  elseif (IS_C_SRC)
    target_link_libraries (${EXE_NAME} PRIVATE gptl)
  elseif (GPTL_Fortran_Perf_FOUND)
    # NOT IS_C_SRC
    target_include_directories (${EXE_NAME} PUBLIC ${GPTL_Fortran_INCLUDE_DIRS})
    target_link_libraries (${EXE_NAME} PRIVATE ${GPTL_Fortran_LIBRARIES})
  else ()
    # (NOT IS_C_SRC) AND (NOT GPTL_Fortran_Perf_FOUND)
    target_link_libraries (${EXE_NAME} PRIVATE gptl)
  endif ()
  target_compile_definitions (${EXE_NAME} PUBLIC TIMING)
  if (PIO_ENABLE_INTERNAL_TIMING)
    target_compile_definitions (${EXE_NAME} PUBLIC TIMING_INTERNAL)
  endif ()
endmacro ()

#==============================================================================
#  GENERATE SOURCE FILES
#==============================================================================
SET(TEMPSRCF90 alloc_mod.F90)
FOREACH(tempfile IN LISTS TEMPSRCF90)
ADD_CUSTOM_COMMAND(
	OUTPUT ${tempfile}
	COMMAND ${GENF90_PATH}/genf90.pl ${CMAKE_CURRENT_SOURCE_DIR}/${tempfile}.in > ${tempfile}
	DEPENDS  ${tempfile}.in
)
ENDFOREACH()

SET(SRC check_mod.F90  gdecomp_mod.F90  kinds_mod.F90  namelist_mod.F90  
            testpio.F90  utils_mod.F90 ${TEMPSRCF90})
SET(WSSRC wstest.c)

#==============================================================================
#  SET THE COMPILER OPTIONS
#==============================================================================
INCLUDE_DIRECTORIES(${PIO_INCLUDE_DIRS})
LINK_DIRECTORIES(${PIO_LIB_DIR})

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
  find_package (GPTL COMPONENTS C Fortran_Perf QUIET)
  if (GPTL_C_FOUND)
    message (STATUS "Found GPTL C: ${GPTL_C_LIBRARIES}")
  else ()
    message (STATUS "Using internal GPTL C library for timing")
  endif ()
  if (GPTL_Fortran_Perf_FOUND)
    message (STATUS "Found GPTL Fortran Perf: ${GPTL_Fortran_Perf_LIBRARIES}")
  else ()
    message (STATUS "Using internal GPTL Fortran library for timing")
  endif ()
endif ()

#==============================================================================
#  BUILD EXECUTABLES
#==============================================================================
add_exe_with_libs_and_deps(testpio FALSE ${SRC})
add_exe_with_libs_and_deps(wstest TRUE ${WSSRC})
