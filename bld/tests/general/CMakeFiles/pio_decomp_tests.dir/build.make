# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.14

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Produce verbose output by default.
VERBOSE = 1

# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake

# The command to remove a file.
RM = /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /glade/work/haiyingx/ParallelIO_cz5

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /glade/work/haiyingx/ParallelIO_cz5/bld

# Include any dependencies generated for this target.
include tests/general/CMakeFiles/pio_decomp_tests.dir/depend.make

# Include the progress variables for this target.
include tests/general/CMakeFiles/pio_decomp_tests.dir/progress.make

# Include the compile flags for this target's objects.
include tests/general/CMakeFiles/pio_decomp_tests.dir/flags.make

tests/general/pio_decomp_tests.F90: ../tests/general/pio_decomp_tests.F90.in
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating pio_decomp_tests.F90"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/general && ../../../tests/general/util/pio_tf_f90gen.pl --annotate-source --out=/glade/work/haiyingx/ParallelIO_cz5/bld/tests/general/pio_decomp_tests.F90 /glade/work/haiyingx/ParallelIO_cz5/tests/general/pio_decomp_tests.F90.in

tests/general/CMakeFiles/pio_decomp_tests.dir/pio_decomp_tests.F90.o: tests/general/CMakeFiles/pio_decomp_tests.dir/flags.make
tests/general/CMakeFiles/pio_decomp_tests.dir/pio_decomp_tests.F90.o: tests/general/pio_decomp_tests.F90
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building Fortran object tests/general/CMakeFiles/pio_decomp_tests.dir/pio_decomp_tests.F90.o"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/general && /glade/u/apps/ch/opt/ncarcompilers/0.5.0/gnu/8.3.0/mpi/mpif90 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -c /glade/work/haiyingx/ParallelIO_cz5/bld/tests/general/pio_decomp_tests.F90 -o CMakeFiles/pio_decomp_tests.dir/pio_decomp_tests.F90.o

tests/general/CMakeFiles/pio_decomp_tests.dir/pio_decomp_tests.F90.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing Fortran source to CMakeFiles/pio_decomp_tests.dir/pio_decomp_tests.F90.i"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/general && /glade/u/apps/ch/opt/ncarcompilers/0.5.0/gnu/8.3.0/mpi/mpif90 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -E /glade/work/haiyingx/ParallelIO_cz5/bld/tests/general/pio_decomp_tests.F90 > CMakeFiles/pio_decomp_tests.dir/pio_decomp_tests.F90.i

tests/general/CMakeFiles/pio_decomp_tests.dir/pio_decomp_tests.F90.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling Fortran source to assembly CMakeFiles/pio_decomp_tests.dir/pio_decomp_tests.F90.s"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/general && /glade/u/apps/ch/opt/ncarcompilers/0.5.0/gnu/8.3.0/mpi/mpif90 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -S /glade/work/haiyingx/ParallelIO_cz5/bld/tests/general/pio_decomp_tests.F90 -o CMakeFiles/pio_decomp_tests.dir/pio_decomp_tests.F90.s

tests/general/CMakeFiles/pio_decomp_tests.dir/util/pio_tutil.F90.o: tests/general/CMakeFiles/pio_decomp_tests.dir/flags.make
tests/general/CMakeFiles/pio_decomp_tests.dir/util/pio_tutil.F90.o: ../tests/general/util/pio_tutil.F90
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building Fortran object tests/general/CMakeFiles/pio_decomp_tests.dir/util/pio_tutil.F90.o"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/general && /glade/u/apps/ch/opt/ncarcompilers/0.5.0/gnu/8.3.0/mpi/mpif90 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -c /glade/work/haiyingx/ParallelIO_cz5/tests/general/util/pio_tutil.F90 -o CMakeFiles/pio_decomp_tests.dir/util/pio_tutil.F90.o

tests/general/CMakeFiles/pio_decomp_tests.dir/util/pio_tutil.F90.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing Fortran source to CMakeFiles/pio_decomp_tests.dir/util/pio_tutil.F90.i"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/general && /glade/u/apps/ch/opt/ncarcompilers/0.5.0/gnu/8.3.0/mpi/mpif90 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -E /glade/work/haiyingx/ParallelIO_cz5/tests/general/util/pio_tutil.F90 > CMakeFiles/pio_decomp_tests.dir/util/pio_tutil.F90.i

tests/general/CMakeFiles/pio_decomp_tests.dir/util/pio_tutil.F90.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling Fortran source to assembly CMakeFiles/pio_decomp_tests.dir/util/pio_tutil.F90.s"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/general && /glade/u/apps/ch/opt/ncarcompilers/0.5.0/gnu/8.3.0/mpi/mpif90 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -S /glade/work/haiyingx/ParallelIO_cz5/tests/general/util/pio_tutil.F90 -o CMakeFiles/pio_decomp_tests.dir/util/pio_tutil.F90.s

# Object files for target pio_decomp_tests
pio_decomp_tests_OBJECTS = \
"CMakeFiles/pio_decomp_tests.dir/pio_decomp_tests.F90.o" \
"CMakeFiles/pio_decomp_tests.dir/util/pio_tutil.F90.o"

# External object files for target pio_decomp_tests
pio_decomp_tests_EXTERNAL_OBJECTS =

tests/general/pio_decomp_tests: tests/general/CMakeFiles/pio_decomp_tests.dir/pio_decomp_tests.F90.o
tests/general/pio_decomp_tests: tests/general/CMakeFiles/pio_decomp_tests.dir/util/pio_tutil.F90.o
tests/general/pio_decomp_tests: tests/general/CMakeFiles/pio_decomp_tests.dir/build.make
tests/general/pio_decomp_tests: src/flib/libpiof.a
tests/general/pio_decomp_tests: src/clib/libpioc.a
tests/general/pio_decomp_tests: /glade/u/apps/ch/opt/netcdf-mpi/4.7.3/mpt/2.19/gnu/8.3.0/lib/libnetcdf.so
tests/general/pio_decomp_tests: src/clib/libz5wrapper.a
tests/general/pio_decomp_tests: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/libboost_filesystem.so
tests/general/pio_decomp_tests: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/libboost_system.so
tests/general/pio_decomp_tests: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/libz.so
tests/general/pio_decomp_tests: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/libblosc.so
tests/general/pio_decomp_tests: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/liblz4.so
tests/general/pio_decomp_tests: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/libz.so
tests/general/pio_decomp_tests: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/libblosc.so
tests/general/pio_decomp_tests: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/liblz4.so
tests/general/pio_decomp_tests: src/gptl/libgptl.a
tests/general/pio_decomp_tests: /glade/u/apps/ch/opt/netcdf-mpi/4.7.3/mpt/2.19/gnu/8.3.0/lib/libnetcdff.so
tests/general/pio_decomp_tests: /glade/u/apps/ch/opt/pnetcdf/1.12.1/mpt/2.19/gnu/8.3.0/lib/libpnetcdf.a
tests/general/pio_decomp_tests: tests/general/CMakeFiles/pio_decomp_tests.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Linking CXX executable pio_decomp_tests"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/general && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/pio_decomp_tests.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
tests/general/CMakeFiles/pio_decomp_tests.dir/build: tests/general/pio_decomp_tests

.PHONY : tests/general/CMakeFiles/pio_decomp_tests.dir/build

tests/general/CMakeFiles/pio_decomp_tests.dir/clean:
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/general && $(CMAKE_COMMAND) -P CMakeFiles/pio_decomp_tests.dir/cmake_clean.cmake
.PHONY : tests/general/CMakeFiles/pio_decomp_tests.dir/clean

tests/general/CMakeFiles/pio_decomp_tests.dir/depend: tests/general/pio_decomp_tests.F90
	cd /glade/work/haiyingx/ParallelIO_cz5/bld && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /glade/work/haiyingx/ParallelIO_cz5 /glade/work/haiyingx/ParallelIO_cz5/tests/general /glade/work/haiyingx/ParallelIO_cz5/bld /glade/work/haiyingx/ParallelIO_cz5/bld/tests/general /glade/work/haiyingx/ParallelIO_cz5/bld/tests/general/CMakeFiles/pio_decomp_tests.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : tests/general/CMakeFiles/pio_decomp_tests.dir/depend

