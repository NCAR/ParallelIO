###-------------------------------------------------------------------------###
### CMakeList.txt for examples using pio
###-------------------------------------------------------------------------###

if (WITH_ADIOS)
  find_package (ADIOS ${ADIOS_MIN_VER_REQD})
  if (ADIOS_FOUND)
    ADD_SUBDIRECTORY(adios2pio-nm)
  endif (ADIOS_FOUND)
endif(WITH_ADIOS)
ADD_SUBDIRECTORY(spio_finfo)
