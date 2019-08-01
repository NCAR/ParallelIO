#==============================================================================
#  DEFINE THE TARGETS AND TESTS
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

add_executable (pioperf EXCLUDE_FROM_ALL
  pioperformance.F90)
set_property(TARGET pioperf PROPERTY LINKER_LANGUAGE ${PIO_LINKER_LANGUAGE})
target_link_libraries (pioperf piof)
add_dependencies (tests pioperf)

add_executable (pioperf_rearr EXCLUDE_FROM_ALL
  pioperformance_rearr.F90)
set_property(TARGET pioperf_rearr PROPERTY LINKER_LANGUAGE ${PIO_LINKER_LANGUAGE})
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
