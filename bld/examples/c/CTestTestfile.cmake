# CMake generated Testfile for 
# Source directory: /glade/work/haiyingx/ParallelIO_cz5/examples/c
# Build directory: /glade/work/haiyingx/ParallelIO_cz5/bld/examples/c
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(examplePio "/glade/work/haiyingx/ParallelIO_cz5/cmake/mpiexec.nwscla" "4" "/glade/work/haiyingx/ParallelIO_cz5/bld/examples/c/examplePio")
set_tests_properties(examplePio PROPERTIES  TIMEOUT "60" _BACKTRACE_TRIPLES "/glade/work/haiyingx/ParallelIO_cz5/cmake/LibMPI.cmake;110;add_test;/glade/work/haiyingx/ParallelIO_cz5/examples/c/CMakeLists.txt;48;add_mpi_test;/glade/work/haiyingx/ParallelIO_cz5/examples/c/CMakeLists.txt;0;")
add_test(example1 "/glade/work/haiyingx/ParallelIO_cz5/cmake/mpiexec.nwscla" "4" "/glade/work/haiyingx/ParallelIO_cz5/bld/examples/c/example1")
set_tests_properties(example1 PROPERTIES  TIMEOUT "60" _BACKTRACE_TRIPLES "/glade/work/haiyingx/ParallelIO_cz5/cmake/LibMPI.cmake;110;add_test;/glade/work/haiyingx/ParallelIO_cz5/examples/c/CMakeLists.txt;49;add_mpi_test;/glade/work/haiyingx/ParallelIO_cz5/examples/c/CMakeLists.txt;0;")
add_test(darray_no_async "/glade/work/haiyingx/ParallelIO_cz5/cmake/mpiexec.nwscla" "4" "/glade/work/haiyingx/ParallelIO_cz5/bld/examples/c/darray_no_async")
set_tests_properties(darray_no_async PROPERTIES  TIMEOUT "60" _BACKTRACE_TRIPLES "/glade/work/haiyingx/ParallelIO_cz5/cmake/LibMPI.cmake;110;add_test;/glade/work/haiyingx/ParallelIO_cz5/examples/c/CMakeLists.txt;51;add_mpi_test;/glade/work/haiyingx/ParallelIO_cz5/examples/c/CMakeLists.txt;0;")
