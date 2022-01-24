include (LibMPI)
include (PIOTest)

message(STATUS "===== Configuring SCORPIO Fortran tests... =====")
#==============================================================================
#  HELPER MACROS
#==============================================================================
include(SPIOUtils)
macro (add_spio_test_executable EXE_NAME IS_C_SRC EXE_LINKER_LANGUAGE)
  add_spio_executable(${EXE_NAME} ${IS_C_SRC} ${EXE_LINKER_LANGUAGE} ${ARGN})
  target_link_libraries(${EXE_NAME} PRIVATE pio_tutil)
  add_dependencies (${EXE_NAME} pio_tutil)
  add_dependencies (tests ${EXE_NAME})
endmacro()

#==============================================================================
#  GENERATE TARGET SOURCES
#==============================================================================

SET(GENERATED_SRCS pio_file_simple_tests.F90
  pio_init_finalize.F90
  pio_async_init_finalize.F90
  pio_fail.F90
  pio_file_fail.F90
  pio_large_file_tests.F90
  pio_decomp_large_tests.F90
  ncdf_simple_tests.F90
  ncdf_get_put.F90
  ncdf_fail.F90
  ncdf_eh_one_test.F90
  ncdf_inq.F90
  pio_copy.F90
  pio_set_hint.F90
  pio_async_file_tests.F90
  pio_async_copy_tests.F90
  pio_rearr.F90
  pio_rearr_opts.F90
  pio_rearr_opts2.F90
  pio_decomp_tests.F90
  pio_decomp_tests_1d.F90
  pio_decomp_tests2_1d.F90
  pio_async_decomp_tests.F90
  pio_async_decomp_tests_1d.F90
  pio_decomp_tests_2d.F90
  pio_decomp_tests_3d.F90
  pio_sync_tests.F90
  pio_iodesc_tests.F90
  pio_buf_lim_tests.F90
  pio_decomp_frame_tests.F90
  pio_decomp_fillval.F90
  pio_decomp_fillval2.F90
  pio_decomp_extra_dims.F90
  pio_iosystem_tests.F90
  pio_iosystem_tests2.F90
  pio_iosystem_tests3.F90)

foreach (SRC_FILE IN LISTS GENERATED_SRCS)
  add_custom_command (
    OUTPUT ${SRC_FILE}
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/util/pio_tf_f90gen.pl
    --annotate-source
    --out=${CMAKE_CURRENT_BINARY_DIR}/${SRC_FILE}
    ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_FILE}.in
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_FILE}.in)
endforeach ()

set(DEF_SPIO_TIMING_DIR ${CMAKE_CURRENT_BINARY_DIR}/spio_stats)
file(MAKE_DIRECTORY ${DEF_SPIO_TIMING_DIR})

# The PIO library is written using C, C++ and Fortran languages
# IBM compilers require Fortran/C/C++ mixed language programs
# to be linked with the C++ linker. Other compilers require the
# Fortran linker.
if ("${CMAKE_Fortran_COMPILER_ID}" STREQUAL "XL")
  SET(PIO_LINKER_LANGUAGE CXX)
else ()
  SET(PIO_LINKER_LANGUAGE Fortran)
endif ()
if ("${CMAKE_Fortran_COMPILER_ID}" STREQUAL "GNU")
  add_definitions(-ffree-line-length-none)
endif()

if (CMAKE_Fortran_COMPILER_ID STREQUAL "NAG")
  set ( CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -mismatch_all" )
  #    target_compile_options (gptl
  #        PRIVATE -mismatch_all)
endif ()

if (PIO_TEST_CLOSE_OPEN_FOR_SYNC)
  add_definitions(-DPIO_TEST_CLOSE_OPEN_FOR_SYNC)
endif()

#==============================================================================
#  FIND EXTERNAL LIBRARIES/DEPENDENCIES
#==============================================================================
#===== MPI =====
if (PIO_USE_MPISERIAL)
  find_package (MPISERIAL COMPONENTS Fortran REQUIRED)
else ()
  find_package (MPI REQUIRED)
endif ()

# Check for MPI Fortran module
find_path(MPIMOD_PATH
  NAMES mpi.mod MPI.mod
  HINTS ${MPI_Fortran_INCLUDE_PATH})
if (PIO_ENABLE_TIMING)
  find_package (GPTL COMPONENTS Fortran_Perf QUIET)
endif ()

check_macro (MPI_HAS_Fortran_MOD
  NAME TryMPIMod.f90
  HINTS ${CMAKE_MODULE_PATH}
  DEFINITIONS -I${MPIMOD_PATH}
  COMMENT "whether MPI Fortran module is supported")
if (${MPI_HAS_Fortran_MOD})
  message (STATUS "MPI Fortran module verified and enabled.")
else ()
  message (STATUS "MPI Fortran module failed verification and therefore disabled.")
endif ()

#===== GPTL =====
if (PIO_ENABLE_TIMING)
  find_package (GPTL COMPONENTS Fortran_Perf QUIET)
  if (GPTL_Fortran_Perf_FOUND)
    message (STATUS "Found GPTL Fortran Perf: ${GPTL_Fortran_Perf_LIBRARIES}")
  else ()
    message (STATUS "Using internal GPTL Fortran library for timing")
  endif ()
endif ()

#==============================================================================
#  DEFINE THE TARGETS AND TESTS
#==============================================================================

# Test Timeout (16 min = 960 sec)
set (DEFAULT_TEST_TIMEOUT 960)
set (DEFAULT_TEST_MAXNUMPROCS 4)

# Use environment variable PIO_TF_MAXNUMPROCS to set the maximum number
# of MPI processes used to run the tests (default is 4)
# This setting is typically used to run PIO Fortran test suite on a
# cluster (with larger number of MPI processes)
if (DEFINED ENV{PIO_TF_MAXNUMPROCS})
  set(PIO_TF_MAXNUMPROCS $ENV{PIO_TF_MAXNUMPROCS})
else ()
  set(PIO_TF_MAXNUMPROCS ${DEFAULT_TEST_MAXNUMPROCS})
endif ()

# Since the testing framework exponentially increases the number of
# I/O processes and stride we need to limit the number of combinations
# of MPI processes + I/O processes + stride to limit the total test suite
# execution time. We divide tests into multiple tiers based on their
# nightly test execution times and limit the number of I/O processes and
# strides for costly tiers.
# Cost tiers C1, C2, C3, ... : cost(C1) < cost(C2) < cost(C3), ...
# Approx cost : cost(C1) < 1s, cost(C2) < 10s, cost(C3) >= 10s

# Strides are always restricted to 1,2 for all cost tiers
# (to reduce the number of test combinations)
set (PIO_TF_STRIDE_UB "2")

# Cost tier C1 variables - low cost, very less restrictions
# Only allow upto 8 I/O procs & strides upto 2
set (PIO_TF_C1_UB "8")
set (PIO_TF_C1_MAXNUMIOPROCS ${PIO_TF_MAXNUMPROCS})
set (PIO_TF_C1_MAXSTRIDE ${PIO_TF_MAXNUMPROCS})

if (PIO_TF_C1_MAXNUMIOPROCS GREATER PIO_TF_C1_UB)
  set (PIO_TF_C1_MAXNUMIOPROCS ${PIO_TF_C1_UB})
endif ()
if (PIO_TF_C1_MAXSTRIDE GREATER PIO_TF_STRIDE_UB)
  set (PIO_TF_C1_MAXSTRIDE ${PIO_TF_STRIDE_UB})
endif ()

# Cost tier C2 variables - medium cost, some restrictions
# Only allow upto 4 I/O procs & strides upto 2
set (PIO_TF_C2_UB "4")
set (PIO_TF_C2_MAXNUMIOPROCS ${PIO_TF_MAXNUMPROCS})
set (PIO_TF_C2_MAXSTRIDE ${PIO_TF_MAXNUMPROCS})

if (PIO_TF_C2_MAXNUMIOPROCS GREATER PIO_TF_C2_UB)
  set (PIO_TF_C2_MAXNUMIOPROCS ${PIO_TF_C2_UB})
endif ()
if (PIO_TF_C2_MAXSTRIDE GREATER PIO_TF_STRIDE_UB)
  set (PIO_TF_C2_MAXSTRIDE ${PIO_TF_STRIDE_UB})
endif ()

# Cost tier C3 variables - high cost, limit combinations tested
# Only allow upto 2 I/O procs & strides upto 2
set (PIO_TF_C3_UB "2")
set (PIO_TF_C3_MAXNUMIOPROCS ${PIO_TF_MAXNUMPROCS})
set (PIO_TF_C3_MAXSTRIDE ${PIO_TF_MAXNUMPROCS})

if (PIO_TF_C3_MAXNUMIOPROCS GREATER PIO_TF_C3_UB)
  set (PIO_TF_C3_MAXNUMIOPROCS ${PIO_TF_C3_UB})
endif ()
if (PIO_TF_C3_MAXSTRIDE GREATER PIO_TF_STRIDE_UB)
  set (PIO_TF_C3_MAXSTRIDE ${PIO_TF_STRIDE_UB})
endif ()

message(STATUS "Adding Fortran tests with [1...${PIO_TF_MAXNUMPROCS}] MPI processes")

#===== Build the PIO testing framework =========
add_library(pio_tutil EXCLUDE_FROM_ALL
  ${CMAKE_CURRENT_SOURCE_DIR}/util/pio_tutil.F90)
target_link_libraries (pio_tutil piof)

#===== pio_init_finalize =====
add_spio_test_executable(pio_init_finalize FALSE ${PIO_LINKER_LANGUAGE} pio_init_finalize.F90)
target_link_libraries(pio_init_finalize PRIVATE pio_tutil)
add_dependencies (pio_init_finalize pio_tutil)

add_pio_test(init_finalize
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_init_finalize
  MINNUMPROCS 1
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS ${PIO_TF_C1_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C1_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_async_init_finalize =====
add_spio_test_executable(pio_async_init_finalize FALSE ${PIO_LINKER_LANGUAGE} pio_async_init_finalize.F90)

# Note: Async tests don't use ioprocs/stride specified by user
add_pio_test(async_init_finalize
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_async_init_finalize
  MINNUMPROCS 2
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS 1
  MINSTRIDE 1
  MAXSTRIDE 1
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_file_simple_tests =====
add_spio_test_executable(pio_file_simple_tests FALSE ${PIO_LINKER_LANGUAGE} pio_file_simple_tests.F90)

# The number of io procs/stride doesn't significantly impact this test
add_pio_test(pio_file_simple_tests
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_file_simple_tests
  MINNUMPROCS 2
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS 2
  MINSTRIDE 1
  MAXSTRIDE 1
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

if (PIO_ENABLE_LARGE_TESTS)
  #===== pio_large_file_tests =====
  add_spio_test_executable(pio_large_file_tests FALSE ${PIO_LINKER_LANGUAGE} pio_large_file_tests.F90)

  # Costly test only test with 2 I/O procs
  add_pio_test(pio_large_file_tests
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_large_file_tests
    MINNUMPROCS 2
    MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
    MINNUMIOPROCS 2
    MAXNUMIOPROCS 2
    MINSTRIDE 1
    MAXSTRIDE 1
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})

  #===== pio_decomp_large_tests =====
  add_spio_test_executable(pio_decomp_large_tests FALSE ${PIO_LINKER_LANGUAGE} pio_decomp_large_tests.F90)

  # Costly test only test with 2 I/O procs
  add_pio_test(pio_decomp_large_tests
    EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_decomp_large_tests
    MINNUMPROCS 2
    MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
    MINNUMIOPROCS 2
    MAXNUMIOPROCS 2
    MINSTRIDE 1
    MAXSTRIDE 1
    TIMEOUT ${DEFAULT_TEST_TIMEOUT})
endif ()

#===== pio_file_fail =====
add_spio_test_executable(pio_file_fail FALSE ${PIO_LINKER_LANGUAGE} pio_file_fail.F90)

# The number of io procs/stride doesn't significantly impact this test
add_pio_test(pio_file_fail
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_file_fail
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS 2
  MINSTRIDE 2
  MAXSTRIDE 2
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== ncdf_simple_tests =====
add_spio_test_executable(ncdf_simple_tests FALSE ${PIO_LINKER_LANGUAGE} ncdf_simple_tests.F90)

add_pio_test(ncdf_simple_tests
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ncdf_simple_tests
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 2
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== ncdf_get_put =====
add_spio_test_executable(ncdf_get_put FALSE ${PIO_LINKER_LANGUAGE} ncdf_get_put.F90)

add_pio_test(ncdf_get_put
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ncdf_get_put
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C3_MAXNUMIOPROCS}
  MINSTRIDE 2
  MAXSTRIDE ${PIO_TF_C3_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== ncdf_fail =====
add_spio_test_executable(ncdf_fail FALSE ${PIO_LINKER_LANGUAGE} ncdf_fail.F90)

add_pio_test(ncdf_fail_bcast
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ncdf_fail
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

add_pio_test(ncdf_fail_reduce
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ncdf_fail
  ARGUMENTS --pio-tf-err-handler=PIO_REDUCE_ERROR
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

add_pio_test(ncdf_fail_intrnl
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ncdf_fail
  ARGUMENTS --pio-tf-err-handler=PIO_INTERNAL_ERROR
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT}
  WILLFAIL)

# Error handler tests
#===== ncdf_eh_one_test =====
add_spio_test_executable(ncdf_eh_one_test FALSE ${PIO_LINKER_LANGUAGE} ncdf_eh_one_test.F90)

add_pio_test(ncdf_eh_one_test_bcast
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ncdf_eh_one_test
  ARGUMENTS --pio-tf-err-handler=PIO_BCAST_ERROR
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

add_pio_test(ncdf_eh_one_test_reduce
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ncdf_eh_one_test
  ARGUMENTS --pio-tf-err-handler=PIO_REDUCE_ERROR
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

add_pio_test(ncdf_eh_one_test_ret
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ncdf_eh_one_test
  ARGUMENTS --pio-tf-err-handler=PIO_RETURN_ERROR
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

add_pio_test(ncdf_eh_one_test_intrnl_pnet
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ncdf_eh_one_test
  ARGUMENTS --pio-tf-err-handler=PIO_INTERNAL_ERROR --pio-tf-targ-iotype=PNETCDF
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT}
  WILLFAIL)

add_pio_test(ncdf_eh_one_test_intrnl_net
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ncdf_eh_one_test
  ARGUMENTS --pio-tf-err-handler=PIO_INTERNAL_ERROR --pio-tf-targ-iotype=NETCDF
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT}
  WILLFAIL)

add_pio_test(ncdf_eh_one_test_intrnl_net4c
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ncdf_eh_one_test
  ARGUMENTS --pio-tf-err-handler=PIO_INTERNAL_ERROR --pio-tf-targ-iotype=NETCDF4C
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT}
  WILLFAIL)

add_pio_test(ncdf_eh_one_test_intrnl_net4p
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ncdf_eh_one_test
  ARGUMENTS --pio-tf-err-handler=PIO_INTERNAL_ERROR --pio-tf-targ-iotype=NETCDF4P
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT}
  WILLFAIL)

add_pio_test(ncdf_eh_one_test_intrnl_ad
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ncdf_eh_one_test
  ARGUMENTS --pio-tf-err-handler=PIO_INTERNAL_ERROR --pio-tf-targ-iotype=ADIOS
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT}
  WILLFAIL)

#===== ncdf_inq =====
add_spio_test_executable(ncdf_inq FALSE ${PIO_LINKER_LANGUAGE} ncdf_inq.F90)

add_pio_test(ncdf_inq
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/ncdf_inq
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_copy =====
add_spio_test_executable(pio_copy FALSE ${PIO_LINKER_LANGUAGE} pio_copy.F90)

add_pio_test(pio_copy
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_copy
  MINNUMPROCS 1
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS ${PIO_TF_C1_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C1_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_set_hint =====
add_spio_test_executable(pio_set_hint FALSE ${PIO_LINKER_LANGUAGE} pio_set_hint.F90)

add_pio_test(pio_set_hint
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_set_hint
  MINNUMPROCS 1
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS ${PIO_TF_C1_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C1_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_rearr =====
add_spio_test_executable(pio_rearr FALSE ${PIO_LINKER_LANGUAGE} pio_rearr.F90)

# Costly test
add_pio_test(pio_rearr
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_rearr
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C3_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C3_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_async_file_tests =====
add_spio_test_executable(pio_async_file_tests FALSE ${PIO_LINKER_LANGUAGE} pio_async_file_tests.F90)

# Async file tests don't use ioprocs/stride set by user
add_pio_test(pio_async_file_tests
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_async_file_tests
  MINNUMPROCS 3
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS 2
  MINSTRIDE 2
  MAXSTRIDE 2
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_async_copy_tests =====
add_spio_test_executable(pio_async_copy_tests FALSE ${PIO_LINKER_LANGUAGE} pio_async_copy_tests.F90)

# Async file tests don't use ioprocs/stride set by user
add_pio_test(pio_async_copy_tests
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_async_copy_tests
  MINNUMPROCS 3
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS 2
  MINSTRIDE 2
  MAXSTRIDE 2
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_rearr_opts =====
add_spio_test_executable(pio_rearr_opts FALSE ${PIO_LINKER_LANGUAGE} pio_rearr_opts.F90)

# PIO rearr opts is a costly test
add_pio_test(pio_rearr_opts
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_rearr_opts
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C3_MAXNUMIOPROCS}
  MINSTRIDE 2
  MAXSTRIDE ${PIO_TF_C3_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_rearr_opts2 =====
add_spio_test_executable(pio_rearr_opts2 FALSE ${PIO_LINKER_LANGUAGE} pio_rearr_opts2.F90)

# This is a costly test
add_pio_test(pio_rearr_opts2
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_rearr_opts2
  MINNUMPROCS 1
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS ${PIO_TF_C3_MAXNUMIOPROCS}
  MINSTRIDE 2
  MAXSTRIDE ${PIO_TF_C3_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_decomp_tests =====
add_spio_test_executable(pio_decomp_tests FALSE ${PIO_LINKER_LANGUAGE} pio_decomp_tests.F90)

add_pio_test(pio_decomp_tests
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_decomp_tests
  MINNUMPROCS 1
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_async_decomp_tests =====
add_spio_test_executable(pio_async_decomp_tests FALSE ${PIO_LINKER_LANGUAGE} pio_async_decomp_tests.F90)

# Async tests don't use ioprocs/stride set by the user
add_pio_test(pio_async_decomp_tests
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_async_decomp_tests
  MINNUMPROCS 3
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS 1
  MINSTRIDE 1
  MAXSTRIDE 1
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_decomp_tests_1d =====
add_spio_test_executable(pio_decomp_tests_1d FALSE ${PIO_LINKER_LANGUAGE} pio_decomp_tests_1d.F90)

# Costly test
add_pio_test(pio_decomp_tests_1d
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_decomp_tests_1d
  MINNUMPROCS 1
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS ${PIO_TF_C3_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C3_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_decomp_tests2_1d =====
add_spio_test_executable(pio_decomp_tests2_1d FALSE ${PIO_LINKER_LANGUAGE} pio_decomp_tests2_1d.F90)

add_pio_test(pio_decomp_tests2_1d
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_decomp_tests2_1d
  MINNUMPROCS 1
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_async_decomp_tests_1d =====
add_spio_test_executable(pio_async_decomp_tests_1d FALSE ${PIO_LINKER_LANGUAGE} pio_async_decomp_tests_1d.F90)

# Async tests don't use ioprocs/stride set by the user
add_pio_test(pio_async_decomp_tests_1d
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_async_decomp_tests_1d
  MINNUMPROCS 3
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS 1
  MINSTRIDE 1
  MAXSTRIDE 1
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_decomp_tests_2d =====
add_spio_test_executable(pio_decomp_tests_2d FALSE ${PIO_LINKER_LANGUAGE} pio_decomp_tests_2d.F90)

add_pio_test(pio_decomp_tests_2d
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_decomp_tests_2d
  MINNUMPROCS 1
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_decomp_tests_3d =====
add_spio_test_executable(pio_decomp_tests_3d FALSE ${PIO_LINKER_LANGUAGE} pio_decomp_tests_3d.F90)

add_pio_test(pio_decomp_tests_3d
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_decomp_tests_3d
  MINNUMPROCS 1
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_sync_tests =====
add_spio_test_executable(pio_sync_tests FALSE ${PIO_LINKER_LANGUAGE} pio_sync_tests.F90)

# Costly test
add_pio_test(pio_sync_tests
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_sync_tests
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C3_MAXNUMIOPROCS}
  MINSTRIDE 2
  MAXSTRIDE ${PIO_TF_C3_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_buf_lim_tests =====
add_spio_test_executable(pio_buf_lim_tests FALSE ${PIO_LINKER_LANGUAGE} pio_buf_lim_tests.F90)

add_pio_test(pio_buf_lim_tests
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_buf_lim_tests
  MINNUMPROCS 2
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_iodesc_tests =====
add_spio_test_executable(pio_iodesc_tests FALSE ${PIO_LINKER_LANGUAGE} pio_iodesc_tests.F90)

add_pio_test(pio_iodesc_tests
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_iodesc_tests
  MINNUMPROCS 1
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_decomp_frame_tests =====
add_spio_test_executable(pio_decomp_frame_tests FALSE ${PIO_LINKER_LANGUAGE} pio_decomp_frame_tests.F90)

add_pio_test(pio_decomp_frame_tests
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_decomp_frame_tests
  MINNUMPROCS 1
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_decomp_fillval =====
add_spio_test_executable(pio_decomp_fillval FALSE ${PIO_LINKER_LANGUAGE} pio_decomp_fillval.F90)

# Costly test
add_pio_test(pio_decomp_fillval
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_decomp_fillval
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C3_MAXNUMIOPROCS}
  MINSTRIDE 2
  MAXSTRIDE ${PIO_TF_C3_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_decomp_fillval2 =====
add_spio_test_executable(pio_decomp_fillval2 FALSE ${PIO_LINKER_LANGUAGE} pio_decomp_fillval2.F90)

add_pio_test(pio_decomp_fillval2
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_decomp_fillval2
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C3_MAXNUMIOPROCS}
  MINSTRIDE 2
  MAXSTRIDE ${PIO_TF_C3_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_decomp_extra_dims =====
add_spio_test_executable(pio_decomp_extra_dims FALSE ${PIO_LINKER_LANGUAGE} pio_decomp_extra_dims.F90)

# This is a costly test
add_pio_test(pio_decomp_extra_dims
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_decomp_extra_dims
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 2
  MAXNUMIOPROCS ${PIO_TF_C3_MAXNUMIOPROCS}
  MINSTRIDE 2
  MAXSTRIDE ${PIO_TF_C3_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_iosystems_test =====
add_spio_test_executable(pio_iosystem_tests FALSE ${PIO_LINKER_LANGUAGE} pio_iosystem_tests.F90)

add_pio_test(pio_iosystem_tests
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_iosystem_tests
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS ${PIO_TF_C2_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C2_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_iosystems_test2 =====
add_spio_test_executable(pio_iosystem_tests2 FALSE ${PIO_LINKER_LANGUAGE} pio_iosystem_tests2.F90)

add_pio_test(pio_iosystem_tests2
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_iosystem_tests2
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS ${PIO_TF_C1_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C1_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})

#===== pio_iosystems_test3 =====
add_spio_test_executable(pio_iosystem_tests3 FALSE ${PIO_LINKER_LANGUAGE} pio_iosystem_tests3.F90)

add_pio_test(pio_iosystem_tests3
  EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pio_iosystem_tests3
  MINNUMPROCS 4
  MAXNUMPROCS ${PIO_TF_MAXNUMPROCS}
  MINNUMIOPROCS 1
  MAXNUMIOPROCS ${PIO_TF_C1_MAXNUMIOPROCS}
  MINSTRIDE 1
  MAXSTRIDE ${PIO_TF_C1_MAXSTRIDE}
  TIMEOUT ${DEFAULT_TEST_TIMEOUT})
