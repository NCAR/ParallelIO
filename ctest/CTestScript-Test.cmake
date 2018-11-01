#==============================================================================
#
#  This is the CTest script for generating test results for submission to the 
#  CTest Dashboard site: my.cdash.org.
#
#  Example originally stolen from:
#    http://www.vtk.org/Wiki/CTest:Using_CTEST_and_CDASH_without_CMAKE
#==============================================================================

#-------------------------------------------
#-- Get the common build information
#-------------------------------------------

set (CTEST_SITE              $ENV{PIO_DASHBOARD_SITE})
set (CTEST_BUILD_NAME        $ENV{PIO_DASHBOARD_BUILD_NAME})
set (CTEST_SOURCE_DIRECTORY  $ENV{PIO_DASHBOARD_SOURCE_DIR})
set (CTEST_BINARY_DIRECTORY  $ENV{PIO_DASHBOARD_BINARY_DIR})

# -----------------------------------------------------------  
# -- Run CTest- TESTING ONLY (Appended to existing TAG)
# -----------------------------------------------------------  

## -- Start
ctest_start("${CTEST_SCRIPT_ARG}" APPEND)

## -- TEST
if (DEFINED ENV{ADIOS_CTEST})
ctest_test(INCLUDE "pio_unit_test|^init|pio_file|ncdf|pio_rearr\
|pio_decomp|pio_sync_tests|pio_buf_lim_tests|pio_iodesc_tests\
|pio_iosystem_tests|examplePio|example1|darray_no_async")
else ()
ctest_test()
endif ()

## Don't submit!  Submission handled by main CTestScript
