###-------------------------------------------------------------------------###
### CMakeList.txt for examples using pio
###-------------------------------------------------------------------------###


if(PIO_ENABLE_FORTRAN)
  add_subdirectory(f03)
  add_subdirectory(c)
else()
  add_subdirectory(c)
endif()
