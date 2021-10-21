#==============================================================================
#  CONFIGURE CHECKS
#==============================================================================

# Test if the machine is big/little endian
if (PIO_TEST_BIG_ENDIAN)
  include (TestBigEndian)
  test_big_endian (PIO_BIG_ENDIAN_TEST_RESULT)
  if (PIO_BIG_ENDIAN_TEST_RESULT)
    set (PIO_BIG_ENDIAN ON  CACHE BOOL "Whether machine is big endian")
  else ()
    set (PIO_BIG_ENDIAN OFF CACHE BOOL "Whether machine is big endian")
  endif ()
else ()
  set (PIO_BIG_ENDIAN OFF CACHE BOOL "Whether machine is big endian")
endif ()

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
    add_subdirectory (gptl)
  endif ()
endif ()

# Add the C library
add_subdirectory (clib)

# Add the Fortran library
if (PIO_ENABLE_FORTRAN)
  add_subdirectory (flib)
endif ()
