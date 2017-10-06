###-------------------------------------------------------------------------###
### CMakeList.txt for examples using pio
###-------------------------------------------------------------------------###

# Include PIO include and lib directories
INCLUDE_DIRECTORIES(${PIO_INCLUDE_DIRS})
include_directories("${CMAKE_SOURCE_DIR}/examples/adios")
LINK_DIRECTORIES(${PIO_LIB_DIR})

set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -g -O0")
# Compiler-specific compiler options
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c99")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "PGI")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -c99")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -c99")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c99")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c99")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")

ADD_EXECUTABLE(example3 example3.c)
TARGET_LINK_LIBRARIES(example3 pioc)
add_dependencies(tests example3)

ADD_EXECUTABLE(timesteps timesteps.c)
TARGET_LINK_LIBRARIES(timesteps pioc)
add_dependencies(tests timesteps)

ADD_EXECUTABLE(timesteps_double timesteps_double.c)
TARGET_LINK_LIBRARIES(timesteps_double pioc)
add_dependencies(tests timesteps_double)


###-------------------------------------------------------------------------###
### CMakeList.txt for examples using pio
###-------------------------------------------------------------------------###

if (PIO_ENABLE_FORTRAN)
  if (WITH_ADIOS)

    # Adding PIO definitions - defined in the root directory
    ADD_DEFINITIONS(${PIO_DEFINITIONS})

    # Include PIO include and lib directories
    INCLUDE_DIRECTORIES(${PIO_INCLUDE_DIRS})
    LINK_DIRECTORIES(${PIO_LIB_DIR})

    set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -g -O0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
    SET(SRC examplePioADIOS.f90)
    ADD_EXECUTABLE(examplePioADIOS_f90 ${SRC})
    TARGET_LINK_LIBRARIES(examplePioADIOS_f90 piof pioc)
  endif(WITH_ADIOS)

endif(PIO_ENABLE_FORTRAN)
