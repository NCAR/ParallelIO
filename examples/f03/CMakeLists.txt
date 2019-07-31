###-------------------------------------------------------------------------###
### CMakeList.txt for examples using pio
###-------------------------------------------------------------------------###

# Adding PIO definitions - defined in the root directory
ADD_DEFINITIONS(${PIO_DEFINITIONS})

# Include PIO include and lib directories
INCLUDE_DIRECTORIES(${PIO_INCLUDE_DIRS})
LINK_DIRECTORIES(${PIO_LIB_DIR})

string (TOUPPER "${CMAKE_Fortran_COMPILER_ID}" CMAKE_FORTRAN_COMPILER_NAME)

set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -g -O0")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
SET(SRC examplePio.f90)
ADD_EXECUTABLE(examplePio_f90 ${SRC})

# The PIO library is written using C, C++ and Fortran languages
# IBM compilers require Fortran/C/C++ mixed language programs
# to be linked with the C++ linker. Other compilers require the
# Fortran linker.
if (CMAKE_FORTRAN_COMPILER_NAME STREQUAL "XL")
  SET(PIO_LINKER_LANGUAGE CXX)
else ()
  SET(PIO_LINKER_LANGUAGE Fortran)
endif ()
SET_PROPERTY(TARGET examplePio_f90 PROPERTY LINKER_LANGUAGE ${PIO_LINKER_LANGUAGE})
TARGET_LINK_LIBRARIES(examplePio_f90 piof pioc)
