###-------------------------------------------------------------------------###
### CMakeList.txt for examples using pio
###-------------------------------------------------------------------------###

if (WITH_ADIOS2)
  find_package (ADIOS2 ${ADIOS_MIN_VER_REQD})
  if (ADIOS2_FOUND)
    ADD_SUBDIRECTORY(adios2pio-nm)
  endif (ADIOS2_FOUND)
endif(WITH_ADIOS2)
ADD_SUBDIRECTORY(spio_finfo)
