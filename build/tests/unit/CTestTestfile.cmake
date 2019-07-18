# CMake generated Testfile for 
# Source directory: /home/tkurc/codar/e3sm/pio_adios1/ParallelIO/tests/unit
# Build directory: /home/tkurc/codar/e3sm/pio_adios1/ParallelIO/build/tests/unit
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
ADD_TEST(pio_unit_test "/usr/lib64/openmpi/bin/mpiexec" "-np" "4" "/home/tkurc/codar/e3sm/pio_adios1/ParallelIO/build/tests/unit/pio_unit_test")
SET_TESTS_PROPERTIES(pio_unit_test PROPERTIES  TIMEOUT "60")
