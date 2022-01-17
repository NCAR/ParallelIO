include (LibMPI)

message(STATUS "===== Configuring SCORPIO C/C++ tests... =====")
#==============================================================================
#  HELPER MACROS
#==============================================================================
include(SPIOUtils)

#==============================================================================
#  SET THE COMPILER OPTIONS
#==============================================================================
include_directories("${SCORPIO_SOURCE_DIR}/tests/cunit" "${SCORPIO_BINARY_DIR}/src/clib")

# Compiler-specific compiler options
string (TOUPPER "${CMAKE_C_COMPILER_ID}" CMAKE_C_COMPILER_NAME)
if (CMAKE_C_COMPILER_NAME STREQUAL "CRAY")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -h std=c99")
elseif (CMAKE_C_COMPILER_NAME STREQUAL "PGI")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -c99")
elseif (CMAKE_C_COMPILER_NAME STREQUAL "NVHPC")
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

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")

#==============================================================================
#  FIND EXTERNAL LIBRARIES/DEPENDENCIES
#==============================================================================
#===== MPI =====
if (PIO_USE_MPISERIAL)
  find_package (MPISERIAL COMPONENTS C REQUIRED)
  if (MPISERIAL_C_FOUND)
    set (CMAKE_REQUIRED_INCLUDES ${MPISERIAL_C_INCLUDE_DIRS})
  endif ()
else ()
  find_package (MPI REQUIRED)
  set (CMAKE_REQUIRED_INCLUDES ${MPI_INCLUDE_PATH})
endif ()

#===== GPTL =====
if (PIO_ENABLE_TIMING)
  find_package (GPTL COMPONENTS C QUIET)
  if (GPTL_C_FOUND)
    message (STATUS "Found GPTL C: ${GPTL_C_LIBRARIES}")
  else ()
    message (STATUS "Using internal GPTL C library for timing")
  endif ()
endif ()

#==============================================================================
#  DEFINE THE TARGETS AND TESTS
#==============================================================================

# Exclude tests that require more than 1 procs when using the MPI serial library
if (NOT PIO_USE_MPISERIAL)
  add_spio_executable (test_intercomm2 TRUE "" test_intercomm2.c test_common.c)
  add_spio_executable (test_async_simple TRUE "" test_async_simple.c test_common.c)
  add_spio_executable (test_async_3proc TRUE "" test_async_3proc.c test_common.c)
  add_spio_executable (test_async_4proc TRUE "" test_async_4proc.c test_common.c)
  add_spio_executable (test_iosystem2_simple TRUE "" test_iosystem2_simple.c test_common.c)
  add_spio_executable (test_iosystem2_simple2 TRUE "" test_iosystem2_simple2.c test_common.c)
  add_spio_executable (test_iosystem2 TRUE "" test_iosystem2.c test_common.c)
  add_spio_executable (test_iosystem3_simple TRUE "" test_iosystem3_simple.c test_common.c)
  add_spio_executable (test_iosystem3_simple2 TRUE "" test_iosystem3_simple2.c test_common.c)
  add_spio_executable (test_iosystem3 TRUE "" test_iosystem3.c test_common.c)
  add_spio_executable (test_pioc TRUE "" test_pioc.c test_common.c test_shared.c)
  add_spio_executable (test_pioc_unlim TRUE "" test_pioc_unlim.c test_common.c test_shared.c)
  add_spio_executable (test_pioc_putget TRUE "" test_pioc_putget.c test_common.c test_shared.c)
  add_spio_executable (test_pioc_fill TRUE "" test_pioc_fill.c test_common.c test_shared.c)
  add_spio_executable (test_darray TRUE "" test_darray.c test_common.c)
  add_spio_executable (test_darray_multi TRUE "" test_darray_multi.c test_common.c)
  add_spio_executable (test_darray_multivar TRUE "" test_darray_multivar.c test_common.c)
  add_spio_executable (test_darray_multivar2 TRUE "" test_darray_multivar2.c test_common.c)
  add_spio_executable (test_darray_1d TRUE "" test_darray_1d.c test_common.c)
  add_spio_executable (test_darray_3d TRUE "" test_darray_3d.c test_common.c)
  add_spio_executable (test_decomp_uneven TRUE "" test_decomp_uneven.c test_common.c)
  add_spio_executable (test_decomps TRUE "" test_decomps.c test_common.c)
  add_spio_executable (test_rearr TRUE "" test_rearr.c test_common.c)
  add_dependencies (tests test_intercomm2 test_async_simple test_async_3proc test_async_4proc
                    test_iosystem2_simple test_iosystem2_simple2 test_iosystem2 test_iosystem3_simple
                    test_iosystem3_simple2 test_iosystem3 test_pioc test_pioc_unlim test_pioc_putget
                    test_pioc_fill test_darray test_darray_multi test_darray_multivar test_darray_multivar2
                    test_darray_1d test_darray_3d test_decomp_uneven test_decomps test_rearr)
  if (PIO_USE_MALLOC)
    add_spio_executable (test_darray_async_simple TRUE "" test_darray_async_simple.c test_common.c)
    add_spio_executable (test_darray_async TRUE "" test_darray_async.c test_common.c)
    add_spio_executable (test_darray_async_many TRUE "" test_darray_async_many.c test_common.c)
    add_dependencies (tests test_darray_async_simple test_darray_async test_darray_async_many)
  endif ()
endif ()
add_spio_executable (test_spmd TRUE "" test_spmd.c test_common.c)
add_spio_executable (test_spio_ltimer TRUE "" test_spio_ltimer.cpp)
add_spio_executable (test_spio_serializer TRUE "" test_spio_serializer.cpp)
add_spio_executable (test_spio_tree TRUE "" test_spio_tree.cpp)
add_spio_executable (test_spio_file_mvcache TRUE "" test_spio_file_mvcache.cpp)
add_spio_executable (test_sdecomp_regex TRUE "" test_sdecomp_regex.cpp test_common.c)
add_spio_executable(test_req_block_wait TRUE "" test_req_block_wait.c test_common.c)
add_dependencies (tests test_spmd test_spio_ltimer test_spio_serializer test_spio_tree
                  test_spio_file_mvcache test_sdecomp_regex test_req_block_wait)

# Test Timeout in seconds.
if (PIO_VALGRIND_CHECK)
  set (DEFAULT_TEST_TIMEOUT 480)
else ()
  set (DEFAULT_TEST_TIMEOUT 240)
endif ()

# All tests need a certain number of tasks, but they should be able to
# run successfully with more than they need. Test this by providing an
# extra processor for each C test.
set (AT_LEAST_TWO_TASKS 3)
set (AT_LEAST_THREE_TASKS 4)
set (AT_LEAST_FOUR_TASKS 5)

add_test(NAME test_spio_ltimer COMMAND test_spio_ltimer)
add_test(NAME test_spio_serializer COMMAND test_spio_serializer)
add_test(NAME test_spio_tree COMMAND test_spio_tree)
add_test(NAME test_spio_file_mvcache COMMAND test_spio_file_mvcache)
add_test(NAME test_sdecomp_regex COMMAND test_sdecomp_regex)

if (PIO_USE_MPISERIAL)
  add_test(NAME test_pioc
    COMMAND test_pioc)
else ()
  add_mpi_test(test_req_block_wait1
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_req_block_wait
    NUMPROCS 1
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_req_block_wait2
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_req_block_wait
    NUMPROCS 2
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_req_block_wait3
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_req_block_wait
    NUMPROCS 3
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_req_block_wait4
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_req_block_wait
    NUMPROCS 4
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_spmd
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_spmd
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_rearr
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_rearr
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_intercomm2
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_intercomm2
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_async_simple
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_async_simple
    NUMPROCS ${AT_LEAST_TWO_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_async_3proc
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_async_3proc
    NUMPROCS ${AT_LEAST_THREE_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_async_4proc
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_async_4proc
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_iosystem2_simple
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_iosystem2_simple
    NUMPROCS ${AT_LEAST_TWO_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_iosystem2_simple2
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_iosystem2_simple2
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_iosystem2
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_iosystem2
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_iosystem3_simple
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_iosystem3_simple
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_iosystem3_simple2
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_iosystem3_simple2
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_iosystem3
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_iosystem3
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_pioc
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_pioc
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_pioc_unlim
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_pioc_unlim
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_pioc_putget
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_pioc_putget
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_pioc_fill
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_pioc_fill
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_darray
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_darray
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_darray_multi
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_darray_multi
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_darray_multivar
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_darray_multivar
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_darray_multivar2
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_darray_multivar2
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_darray_1d
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_darray_1d
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_darray_3d
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_darray_3d
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_decomp_uneven
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_decomp_uneven
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  add_mpi_test(test_decomps
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_decomps
    NUMPROCS ${AT_LEAST_FOUR_TASKS}
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  if(PIO_USE_MALLOC)
    add_mpi_test(test_darray_async_simple
      EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_darray_async_simple
      NUMPROCS ${AT_LEAST_FOUR_TASKS}
      TIMEOUT ${DEFAULT_TEST_TIMEOUT})
    add_mpi_test(test_darray_async
      EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_darray_async
      NUMPROCS ${AT_LEAST_FOUR_TASKS}
      TIMEOUT ${DEFAULT_TEST_TIMEOUT})
    add_mpi_test(test_darray_async_many
      EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/test_darray_async
      NUMPROCS ${AT_LEAST_FOUR_TASKS}
      TIMEOUT ${DEFAULT_TEST_TIMEOUT})
  endif ()
endif ()
