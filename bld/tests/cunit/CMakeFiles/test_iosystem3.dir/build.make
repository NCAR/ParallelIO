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
include tests/cunit/CMakeFiles/test_iosystem3.dir/depend.make

# Include the progress variables for this target.
include tests/cunit/CMakeFiles/test_iosystem3.dir/progress.make

# Include the compile flags for this target's objects.
include tests/cunit/CMakeFiles/test_iosystem3.dir/flags.make

tests/cunit/CMakeFiles/test_iosystem3.dir/test_iosystem3.c.o: tests/cunit/CMakeFiles/test_iosystem3.dir/flags.make
tests/cunit/CMakeFiles/test_iosystem3.dir/test_iosystem3.c.o: ../tests/cunit/test_iosystem3.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object tests/cunit/CMakeFiles/test_iosystem3.dir/test_iosystem3.c.o"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/cunit && /glade/u/apps/ch/opt/ncarcompilers/0.5.0/gnu/8.3.0/mpi/mpicc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/test_iosystem3.dir/test_iosystem3.c.o   -c /glade/work/haiyingx/ParallelIO_cz5/tests/cunit/test_iosystem3.c

tests/cunit/CMakeFiles/test_iosystem3.dir/test_iosystem3.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_iosystem3.dir/test_iosystem3.c.i"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/cunit && /glade/u/apps/ch/opt/ncarcompilers/0.5.0/gnu/8.3.0/mpi/mpicc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /glade/work/haiyingx/ParallelIO_cz5/tests/cunit/test_iosystem3.c > CMakeFiles/test_iosystem3.dir/test_iosystem3.c.i

tests/cunit/CMakeFiles/test_iosystem3.dir/test_iosystem3.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_iosystem3.dir/test_iosystem3.c.s"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/cunit && /glade/u/apps/ch/opt/ncarcompilers/0.5.0/gnu/8.3.0/mpi/mpicc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /glade/work/haiyingx/ParallelIO_cz5/tests/cunit/test_iosystem3.c -o CMakeFiles/test_iosystem3.dir/test_iosystem3.c.s

tests/cunit/CMakeFiles/test_iosystem3.dir/test_common.c.o: tests/cunit/CMakeFiles/test_iosystem3.dir/flags.make
tests/cunit/CMakeFiles/test_iosystem3.dir/test_common.c.o: ../tests/cunit/test_common.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object tests/cunit/CMakeFiles/test_iosystem3.dir/test_common.c.o"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/cunit && /glade/u/apps/ch/opt/ncarcompilers/0.5.0/gnu/8.3.0/mpi/mpicc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/test_iosystem3.dir/test_common.c.o   -c /glade/work/haiyingx/ParallelIO_cz5/tests/cunit/test_common.c

tests/cunit/CMakeFiles/test_iosystem3.dir/test_common.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_iosystem3.dir/test_common.c.i"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/cunit && /glade/u/apps/ch/opt/ncarcompilers/0.5.0/gnu/8.3.0/mpi/mpicc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /glade/work/haiyingx/ParallelIO_cz5/tests/cunit/test_common.c > CMakeFiles/test_iosystem3.dir/test_common.c.i

tests/cunit/CMakeFiles/test_iosystem3.dir/test_common.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_iosystem3.dir/test_common.c.s"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/cunit && /glade/u/apps/ch/opt/ncarcompilers/0.5.0/gnu/8.3.0/mpi/mpicc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /glade/work/haiyingx/ParallelIO_cz5/tests/cunit/test_common.c -o CMakeFiles/test_iosystem3.dir/test_common.c.s

# Object files for target test_iosystem3
test_iosystem3_OBJECTS = \
"CMakeFiles/test_iosystem3.dir/test_iosystem3.c.o" \
"CMakeFiles/test_iosystem3.dir/test_common.c.o"

# External object files for target test_iosystem3
test_iosystem3_EXTERNAL_OBJECTS =

tests/cunit/test_iosystem3: tests/cunit/CMakeFiles/test_iosystem3.dir/test_iosystem3.c.o
tests/cunit/test_iosystem3: tests/cunit/CMakeFiles/test_iosystem3.dir/test_common.c.o
tests/cunit/test_iosystem3: tests/cunit/CMakeFiles/test_iosystem3.dir/build.make
tests/cunit/test_iosystem3: src/clib/libpioc.a
tests/cunit/test_iosystem3: src/gptl/libgptl.a
tests/cunit/test_iosystem3: /glade/u/apps/ch/opt/netcdf-mpi/4.7.3/mpt/2.19/gnu/8.3.0/lib/libnetcdf.so
tests/cunit/test_iosystem3: /glade/u/apps/ch/opt/pnetcdf/1.12.1/mpt/2.19/gnu/8.3.0/lib/libpnetcdf.a
tests/cunit/test_iosystem3: src/clib/libz5wrapper.a
tests/cunit/test_iosystem3: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/libboost_filesystem.so
tests/cunit/test_iosystem3: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/libboost_system.so
tests/cunit/test_iosystem3: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/libz.so
tests/cunit/test_iosystem3: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/libblosc.so
tests/cunit/test_iosystem3: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/liblz4.so
tests/cunit/test_iosystem3: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/libz.so
tests/cunit/test_iosystem3: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/libblosc.so
tests/cunit/test_iosystem3: /glade/u/home/weile/miniconda3/envs/z5-py36/lib/liblz4.so
tests/cunit/test_iosystem3: tests/cunit/CMakeFiles/test_iosystem3.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable test_iosystem3"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/cunit && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/test_iosystem3.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
tests/cunit/CMakeFiles/test_iosystem3.dir/build: tests/cunit/test_iosystem3

.PHONY : tests/cunit/CMakeFiles/test_iosystem3.dir/build

tests/cunit/CMakeFiles/test_iosystem3.dir/clean:
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/tests/cunit && $(CMAKE_COMMAND) -P CMakeFiles/test_iosystem3.dir/cmake_clean.cmake
.PHONY : tests/cunit/CMakeFiles/test_iosystem3.dir/clean

tests/cunit/CMakeFiles/test_iosystem3.dir/depend:
	cd /glade/work/haiyingx/ParallelIO_cz5/bld && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /glade/work/haiyingx/ParallelIO_cz5 /glade/work/haiyingx/ParallelIO_cz5/tests/cunit /glade/work/haiyingx/ParallelIO_cz5/bld /glade/work/haiyingx/ParallelIO_cz5/bld/tests/cunit /glade/work/haiyingx/ParallelIO_cz5/bld/tests/cunit/CMakeFiles/test_iosystem3.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : tests/cunit/CMakeFiles/test_iosystem3.dir/depend

