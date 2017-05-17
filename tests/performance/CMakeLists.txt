#==============================================================================
#  DEFINE THE TARGETS AND TESTS
#==============================================================================

add_executable (pioperf EXCLUDE_FROM_ALL
  pioperformance.F90)
target_link_libraries (pioperf piof)
add_dependencies (tests pioperf)

add_executable (pioperf_rearr EXCLUDE_FROM_ALL
  pioperformance_rearr.F90)
target_link_libraries (pioperf_rearr piof)
add_dependencies (tests pioperf_rearr)

if ("${CMAKE_Fortran_COMPILER_ID}" STREQUAL "GNU")
  target_compile_options (pioperf
    PRIVATE -ffree-line-length-none)
  target_compile_options (pioperf_rearr
    PRIVATE -ffree-line-length-none)
endif()

if (CMAKE_Fortran_COMPILER_ID STREQUAL "NAG")
  set ( CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -mismatch_all" )
  #    target_compile_options (gptl
  #        PRIVATE -mismatch_all)
endif ()

if (PIO_HDF5_LOGGING)
  target_compile_definitions (pioperf
    PUBLIC LOGGING)
  target_compile_definitions (pioperf_rearr
    PUBLIC LOGGING)
endif ()
