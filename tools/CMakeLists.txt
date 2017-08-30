###-------------------------------------------------------------------------###
### CMakeList.txt for examples using pio
###-------------------------------------------------------------------------###

if (WITH_ADIOS)
  find_package (ADIOS "1.10.0")
  if (ADIOS_FOUND)
    ADD_SUBDIRECTORY(adios2pio)
  endif (ADIOS_FOUND)
endif(WITH_ADIOS)
