###-------------------------------------------------------------------------###
### CMakeList.txt for examples using pio
###-------------------------------------------------------------------------###


# Adding PIO definitions - defined in the root directory
add_definitions(${PIO_DEFINITIONS})


# Include PIO include and lib directories
include_directories(${PIO_INCLUDE_DIRS})
link_directories(${PIO_LIB_DIR})

# Add NetCDF C and Fortran library directories if set
if(DEFINED NetCDF_C_LIBDIR)
  link_directories(${NetCDF_C_LIBDIR})
endif()
if(DEFINED NetCDF_Fortran_LIBDIR)
  link_directories(${NetCDF_Fortran_LIBDIR})
endif()

set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -g -O0")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
if(${PIO_BUILD_TIMING})
  set(TIMING_LINK_LIB timing)
endif()

# Ensure NetCDF Fortran and C libraries are linked
set(SRC examplePio.F90)
add_executable(examplePio_f90 ${SRC})
target_link_libraries(examplePio_f90 piof pioc ${TIMING_LINK_LIB})

set(SRC exampleAsyncPio.F90)
add_executable(exampleAsyncPio_f90 ${SRC})
target_link_libraries(exampleAsyncPio_f90 piof pioc ${TIMING_LINK_LIB})