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

# Utility rule file for ContinuousStart.

# Include the progress variables for this target.
include CMakeFiles/ContinuousStart.dir/progress.make

CMakeFiles/ContinuousStart:
	/glade/u/apps/ch/opt/cmake/3.14.4/bin/ctest -D ContinuousStart

ContinuousStart: CMakeFiles/ContinuousStart
ContinuousStart: CMakeFiles/ContinuousStart.dir/build.make

.PHONY : ContinuousStart

# Rule to build all files generated by this target.
CMakeFiles/ContinuousStart.dir/build: ContinuousStart

.PHONY : CMakeFiles/ContinuousStart.dir/build

CMakeFiles/ContinuousStart.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/ContinuousStart.dir/cmake_clean.cmake
.PHONY : CMakeFiles/ContinuousStart.dir/clean

CMakeFiles/ContinuousStart.dir/depend:
	cd /glade/work/haiyingx/ParallelIO_cz5/bld && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /glade/work/haiyingx/ParallelIO_cz5 /glade/work/haiyingx/ParallelIO_cz5 /glade/work/haiyingx/ParallelIO_cz5/bld /glade/work/haiyingx/ParallelIO_cz5/bld /glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles/ContinuousStart.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/ContinuousStart.dir/depend

