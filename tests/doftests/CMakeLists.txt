#==============================================================================
#  DEFINE THE TARGETS AND TESTS
#==============================================================================

add_executable (dofcopy EXCLUDE_FROM_ALL
  dofcopy.F90)
target_link_libraries (dofcopy piof)

if ("${CMAKE_Fortran_COMPILER_ID}" STREQUAL "GNU")
  target_compile_options (dofcopy
    PRIVATE -ffree-line-length-none)
endif()

if (CMAKE_Fortran_COMPILER_ID STREQUAL "NAG")
  set ( CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -mismatch_all" )
  #    target_compile_options (gptl
  #        PRIVATE -mismatch_all)
endif ()
