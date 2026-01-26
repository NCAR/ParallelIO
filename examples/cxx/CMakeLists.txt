###-------------------------------------------------------------------------###
### CMakeList.txt for examples using pio
###-------------------------------------------------------------------------###


# Adding PIO definitions - defined in the root directory
add_definitions(${PIO_DEFINITIONS})

include_directories(${PIO_INCLUDE_DIRS})
link_directories(${PIO_LIB_DIR})

set(SRC examplePio.cxx)
add_executable(examplePio_cxx ${SRC})
