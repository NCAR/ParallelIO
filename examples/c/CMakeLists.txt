###-------------------------------------------------------------------------###
### CMakeList.txt for examples using pio
###-------------------------------------------------------------------------###

# Include PIO include and lib directories
INCLUDE_DIRECTORIES(${PIO_INCLUDE_DIRS})
include_directories("${CMAKE_SOURCE_DIR}/examples/c")
LINK_DIRECTORIES(${PIO_LIB_DIR})

# Compiler-specific compiler options
string (TOUPPER "${CMAKE_C_COMPILER_ID}" CMAKE_C_COMPILER_NAME)
if (CMAKE_C_COMPILER_NAME STREQUAL "CRAY")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -h std=c99")
elseif (CMAKE_C_COMPILER_NAME STREQUAL "PGI")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -c99")
else ()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
endif ()

string (TOUPPER "${CMAKE_CXX_COMPILER_ID}" CMAKE_CXX_COMPILER_NAME)
if (CMAKE_CXX_COMPILER_NAME STREQUAL "CRAY")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -h std=c++11")
else ()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif ()

set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -g -O0")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")

ADD_EXECUTABLE(examplePio EXCLUDE_FROM_ALL examplePio.c)
TARGET_LINK_LIBRARIES(examplePio pioc)
add_dependencies(tests examplePio)

ADD_EXECUTABLE(example1 example1.c)
TARGET_LINK_LIBRARIES(example1 pioc)
add_dependencies(tests example1)

ADD_EXECUTABLE(darray_no_async darray_no_async.c)
TARGET_LINK_LIBRARIES(darray_no_async pioc)
add_dependencies(tests darray_no_async)

ADD_EXECUTABLE(darray_async darray_async.c)
TARGET_LINK_LIBRARIES(darray_async pioc)
add_dependencies(tests darray_async)

ADD_EXECUTABLE(put_var put_var.c)
TARGET_LINK_LIBRARIES(put_var pioc)
add_dependencies(tests put_var)

if (PIO_USE_MPISERIAL)
  add_test(NAME examplePio COMMAND examplePio)
  add_test(NAME example1 COMMAND example1)
  add_test(NAME put_var COMMAND put_var)
else ()
  add_mpi_test(examplePio EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/examplePio NUMPROCS 4 TIMEOUT 60)
  add_mpi_test(example1 EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/example1 NUMPROCS 4 TIMEOUT 60)
  add_mpi_test(put_var EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/put_var NUMPROCS 4 TIMEOUT 60)
  #add_mpi_test(darray_async EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/darray_async NUMPROCS 5 TIMEOUT 60)
  add_mpi_test(darray_no_async EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/darray_no_async NUMPROCS 4 TIMEOUT 60)
endif ()


#===== MPE =====
find_package (MPE ${MPE_MIN_VER_REQD} COMPONENTS C)
if (PIO_ENABLE_FORTRAN)
  if (MPE_C_FOUND)
    SET(SRC example2.c)
    ADD_EXECUTABLE(example2 ${SRC})
    TARGET_LINK_LIBRARIES(example2 pioc)
    target_include_directories (example2
      PUBLIC ${MPE_C_INCLUDE_DIRS})
    target_compile_definitions (example2
      PUBLIC HAVE_MPE)
    target_link_libraries (example2 pioc lmpe mpe pthread m)
  endif ()
endif ()
