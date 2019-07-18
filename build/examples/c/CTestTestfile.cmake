# CMake generated Testfile for 
# Source directory: /home/tkurc/codar/e3sm/pio_adios1/ParallelIO/examples/c
# Build directory: /home/tkurc/codar/e3sm/pio_adios1/ParallelIO/build/examples/c
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
ADD_TEST(examplePio "/usr/lib64/openmpi/bin/mpiexec" "-np" "4" "/home/tkurc/codar/e3sm/pio_adios1/ParallelIO/build/examples/c/examplePio")
SET_TESTS_PROPERTIES(examplePio PROPERTIES  TIMEOUT "60")
ADD_TEST(example1 "/usr/lib64/openmpi/bin/mpiexec" "-np" "4" "/home/tkurc/codar/e3sm/pio_adios1/ParallelIO/build/examples/c/example1")
SET_TESTS_PROPERTIES(example1 PROPERTIES  TIMEOUT "60")
ADD_TEST(put_var "/usr/lib64/openmpi/bin/mpiexec" "-np" "4" "/home/tkurc/codar/e3sm/pio_adios1/ParallelIO/build/examples/c/put_var")
SET_TESTS_PROPERTIES(put_var PROPERTIES  TIMEOUT "60")
ADD_TEST(darray_no_async "/usr/lib64/openmpi/bin/mpiexec" "-np" "4" "/home/tkurc/codar/e3sm/pio_adios1/ParallelIO/build/examples/c/darray_no_async")
SET_TESTS_PROPERTIES(darray_no_async PROPERTIES  TIMEOUT "60")
