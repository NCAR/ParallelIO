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

# Utility rule file for doc.

# Include the progress variables for this target.
include doc/CMakeFiles/doc.dir/progress.make

doc/CMakeFiles/doc:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating API documentation with Doxygen"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/doc && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E copy /glade/work/haiyingx/ParallelIO_cz5/doc/customdoxygen.css /glade/work/haiyingx/ParallelIO_cz5/bld/doc
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/doc && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E copy /glade/work/haiyingx/ParallelIO_cz5/doc/DoxygenLayout.xml /glade/work/haiyingx/ParallelIO_cz5/bld/doc
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/doc && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E copy /glade/work/haiyingx/ParallelIO_cz5/doc/doxygen.sty /glade/work/haiyingx/ParallelIO_cz5/bld/doc
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/doc && /usr/bin/doxygen /glade/work/haiyingx/ParallelIO_cz5/bld/doc/Doxyfile

doc: doc/CMakeFiles/doc
doc: doc/CMakeFiles/doc.dir/build.make

.PHONY : doc

# Rule to build all files generated by this target.
doc/CMakeFiles/doc.dir/build: doc

.PHONY : doc/CMakeFiles/doc.dir/build

doc/CMakeFiles/doc.dir/clean:
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/doc && $(CMAKE_COMMAND) -P CMakeFiles/doc.dir/cmake_clean.cmake
.PHONY : doc/CMakeFiles/doc.dir/clean

doc/CMakeFiles/doc.dir/depend:
	cd /glade/work/haiyingx/ParallelIO_cz5/bld && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /glade/work/haiyingx/ParallelIO_cz5 /glade/work/haiyingx/ParallelIO_cz5/doc /glade/work/haiyingx/ParallelIO_cz5/bld /glade/work/haiyingx/ParallelIO_cz5/bld/doc /glade/work/haiyingx/ParallelIO_cz5/bld/doc/CMakeFiles/doc.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : doc/CMakeFiles/doc.dir/depend

