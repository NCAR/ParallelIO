# CMake generated Testfile for 
# Source directory: /glade/work/haiyingx/ParallelIO_cz5/tests/unit
# Build directory: /glade/work/haiyingx/ParallelIO_cz5/bld/tests/unit
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(pio_unit_test "/glade/work/haiyingx/ParallelIO_cz5/cmake/mpiexec.nwscla" "4" "/glade/work/haiyingx/ParallelIO_cz5/bld/tests/unit/pio_unit_test")
set_tests_properties(pio_unit_test PROPERTIES  TIMEOUT "60" _BACKTRACE_TRIPLES "/glade/work/haiyingx/ParallelIO_cz5/cmake/LibMPI.cmake;110;add_test;/glade/work/haiyingx/ParallelIO_cz5/tests/unit/CMakeLists.txt;54;add_mpi_test;/glade/work/haiyingx/ParallelIO_cz5/tests/unit/CMakeLists.txt;0;")
