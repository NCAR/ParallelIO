#==============================================================================
#  FIND EXTERNAL LIBRARIES
#==============================================================================

# Look for GPTL
if (PIO_ENABLE_TIMING)
  find_package (GPTL COMPONENTS C QUIET)
  find_package (GPTL COMPONENTS Fortran_Perf QUIET)
endif ()

#==============================================================================
#  INCLUDE SOURCE DIRECTORIES
#==============================================================================

# Build the internal GPTL library if its not available
if (PIO_ENABLE_TIMING)
  if (NOT GPTL_C_FOUND OR NOT GPTL_Fortran_Perf_FOUND)
    message(STATUS "Could not find external GPTL library, using the internal GPTL library instead")
    add_subdirectory (gptl)
  else ()
    message(STATUS "Found external GPTL library")
  endif ()
endif ()

# Add the C library
add_subdirectory (clib)

# Add the Fortran library
if (PIO_ENABLE_FORTRAN)
  message(STATUS "Enabling the Fortran interface...")
  add_subdirectory (flib)
endif ()
