cmake_minimum_required (VERSION 2.8.12)
project (PIOTests C Fortran)

#==============================================================================
#  TESTING TARGET
#==============================================================================

# # Custom "piotests" target (builds the test executables)
# add_custom_target (tests)

# # Custom "check" target that depends upon "tests"
# add_custom_target (check COMMAND ${CMAKE_CTEST_COMMAND})
# add_dependencies (check tests)

#==============================================================================
#  INCLUDE SOURCE DIRECTORIES
#==============================================================================

if (PIO_ENABLE_FORTRAN)
  add_subdirectory (general)
  add_subdirectory (unit)
  add_subdirectory (performance)
endif()

add_subdirectory (cunit)
