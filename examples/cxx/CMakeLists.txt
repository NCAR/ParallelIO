###-------------------------------------------------------------------------###
### CMakeList.txt for examples using pio
###-------------------------------------------------------------------------###

#==============================================================================
#  HELPER MACROS
#==============================================================================
macro (add_exe_with_libs_and_deps EXE_NAME)
  add_executable(${EXE_NAME} ${ARGN})
  target_link_libraries(${EXE_NAME} PRIVATE pioc)
  if (GPTL_C_FOUND)
    target_include_directories (${EXE_NAME}
      PUBLIC ${GPTL_C_INCLUDE_DIRS})
    target_link_libraries (${EXE_NAME} PRIVATE ${GPTL_C_LIBRARIES})
  else ()
    message (STATUS "Using internal GPTL C library for timing")
    target_link_libraries (${EXE_NAME} PRIVATE gptl)
  endif ()
  target_compile_definitions (${EXE_NAME} PUBLIC TIMING)
  if (PIO_ENABLE_INTERNAL_TIMING)
    target_compile_definitions (${EXE_NAME} PUBLIC TIMING_INTERNAL)
  endif ()
endmacro ()

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

#===== GPTL =====
if (PIO_ENABLE_TIMING)
  find_package (GPTL COMPONENTS C QUIET)
  if (GPTL_C_FOUND)
    message (STATUS "Found GPTL C: ${GPTL_C_LIBRARIES}")
  else ()
    message (STATUS "Using internal GPTL C library for timing")
  endif ()
endif ()

#==============================================================================
#  SET THE COMPILER OPTIONS
#==============================================================================
# Adding PIO definitions - defined in the root directory
ADD_DEFINITIONS(${PIO_DEFINITIONS})

INCLUDE_DIRECTORIES(${PIO_INCLUDE_DIRS})
LINK_DIRECTORIES(${PIO_LIB_DIR})

#==============================================================================
#  BUILD EXECUTABLE
#==============================================================================
SET(SRC examplePio.cxx)
add_exe_with_libs_and_deps(examplePio_cxx ${SRC})
