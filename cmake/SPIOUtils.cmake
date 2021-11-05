#==============================================================================
#  HELPER MACROS
#==============================================================================
# Add a SCORPIO executable target (add_executable for SCORPIO applications)
#
# The macro adds library dependencies and compiler flags based on whether
# MPI serial library, GPTL etc are used
#
# Prereq: MPI (MPI serial) & GPTL libraries are searched for (find_library() )
# prior to invoking this macro to add SCORPIO applications
macro (add_spio_executable EXE_NAME IS_C_SRC EXE_LINKER_LANGUAGE)
  add_executable(${EXE_NAME} ${ARGN})
  if (${IS_C_SRC})
    target_link_libraries(${EXE_NAME} PRIVATE pioc)
  else ()
    # Assume Fortran executable
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
  if (GPTL_C_FOUND AND ${IS_C_SRC})
    target_include_directories (${EXE_NAME}
      PUBLIC ${GPTL_C_INCLUDE_DIRS})
    target_link_libraries (${EXE_NAME} PRIVATE ${GPTL_C_LIBRARIES})
  elseif (${IS_C_SRC})
    target_link_libraries (${EXE_NAME} PRIVATE gptl)
  elseif (GPTL_Fortran_Perf_FOUND)
    # NOT IS_C_SRC
    target_include_directories (${EXE_NAME} PUBLIC ${GPTL_Fortran_Perf_INCLUDE_DIRS})
    target_link_libraries (${EXE_NAME} PRIVATE ${GPTL_Fortran_Perf_LIBRARIES})
  else ()
    # (NOT IS_C_SRC) AND (NOT GPTL_Fortran_Perf_FOUND)
    target_link_libraries (${EXE_NAME} PRIVATE gptl)
  endif ()
  target_compile_definitions (${EXE_NAME} PUBLIC TIMING)
  if (PIO_ENABLE_INTERNAL_TIMING)
    target_compile_definitions (${EXE_NAME} PUBLIC TIMING_INTERNAL)
  endif ()
  if (NOT ("${EXE_LINKER_LANGUAGE}" STREQUAL ""))
    set_property(TARGET ${EXE_NAME} PROPERTY LINKER_LANGUAGE ${EXE_LINKER_LANGUAGE})
  endif ()
endmacro ()

