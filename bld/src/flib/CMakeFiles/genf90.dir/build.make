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

# Utility rule file for genf90.

# Include the progress variables for this target.
include src/flib/CMakeFiles/genf90.dir/progress.make

src/flib/CMakeFiles/genf90: src/flib/CMakeFiles/genf90-complete


src/flib/CMakeFiles/genf90-complete: src/flib/genf90/src/genf90-stamp/genf90-install
src/flib/CMakeFiles/genf90-complete: src/flib/genf90/src/genf90-stamp/genf90-mkdir
src/flib/CMakeFiles/genf90-complete: src/flib/genf90/src/genf90-stamp/genf90-download
src/flib/CMakeFiles/genf90-complete: src/flib/genf90/src/genf90-stamp/genf90-update
src/flib/CMakeFiles/genf90-complete: src/flib/genf90/src/genf90-stamp/genf90-patch
src/flib/CMakeFiles/genf90-complete: src/flib/genf90/src/genf90-stamp/genf90-configure
src/flib/CMakeFiles/genf90-complete: src/flib/genf90/src/genf90-stamp/genf90-build
src/flib/CMakeFiles/genf90-complete: src/flib/genf90/src/genf90-stamp/genf90-install
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Completed 'genf90'"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E make_directory /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/CMakeFiles
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E touch /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/CMakeFiles/genf90-complete
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E touch /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-stamp/genf90-done

src/flib/genf90/src/genf90-stamp/genf90-install: src/flib/genf90/src/genf90-stamp/genf90-build
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "No install step for 'genf90'"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-build && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E echo_append
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-build && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E touch /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-stamp/genf90-install

src/flib/genf90/src/genf90-stamp/genf90-mkdir:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Creating directories for 'genf90'"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E make_directory /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E make_directory /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-build
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E make_directory /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E make_directory /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/tmp
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E make_directory /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-stamp
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E make_directory /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E make_directory /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-stamp
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E touch /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-stamp/genf90-mkdir

src/flib/genf90/src/genf90-stamp/genf90-download: src/flib/genf90/src/genf90-stamp/genf90-gitinfo.txt
src/flib/genf90/src/genf90-stamp/genf90-download: src/flib/genf90/src/genf90-stamp/genf90-mkdir
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Performing download step (git clone) for 'genf90'"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -P /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/tmp/genf90-gitclone.cmake
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E touch /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-stamp/genf90-download

src/flib/genf90/src/genf90-stamp/genf90-update: src/flib/genf90/src/genf90-stamp/genf90-download
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "No update step for 'genf90'"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90 && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E echo_append
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90 && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E touch /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-stamp/genf90-update

src/flib/genf90/src/genf90-stamp/genf90-patch: src/flib/genf90/src/genf90-stamp/genf90-download
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "No patch step for 'genf90'"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E echo_append
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E touch /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-stamp/genf90-patch

src/flib/genf90/src/genf90-stamp/genf90-configure: src/flib/genf90/tmp/genf90-cfgcmd.txt
src/flib/genf90/src/genf90-stamp/genf90-configure: src/flib/genf90/src/genf90-stamp/genf90-update
src/flib/genf90/src/genf90-stamp/genf90-configure: src/flib/genf90/src/genf90-stamp/genf90-patch
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "No configure step for 'genf90'"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-build && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E echo_append
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-build && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E touch /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-stamp/genf90-configure

src/flib/genf90/src/genf90-stamp/genf90-build: src/flib/genf90/src/genf90-stamp/genf90-configure
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/glade/work/haiyingx/ParallelIO_cz5/bld/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "No build step for 'genf90'"
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-build && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E echo_append
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-build && /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake -E touch /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/genf90/src/genf90-stamp/genf90-build

genf90: src/flib/CMakeFiles/genf90
genf90: src/flib/CMakeFiles/genf90-complete
genf90: src/flib/genf90/src/genf90-stamp/genf90-install
genf90: src/flib/genf90/src/genf90-stamp/genf90-mkdir
genf90: src/flib/genf90/src/genf90-stamp/genf90-download
genf90: src/flib/genf90/src/genf90-stamp/genf90-update
genf90: src/flib/genf90/src/genf90-stamp/genf90-patch
genf90: src/flib/genf90/src/genf90-stamp/genf90-configure
genf90: src/flib/genf90/src/genf90-stamp/genf90-build
genf90: src/flib/CMakeFiles/genf90.dir/build.make

.PHONY : genf90

# Rule to build all files generated by this target.
src/flib/CMakeFiles/genf90.dir/build: genf90

.PHONY : src/flib/CMakeFiles/genf90.dir/build

src/flib/CMakeFiles/genf90.dir/clean:
	cd /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib && $(CMAKE_COMMAND) -P CMakeFiles/genf90.dir/cmake_clean.cmake
.PHONY : src/flib/CMakeFiles/genf90.dir/clean

src/flib/CMakeFiles/genf90.dir/depend:
	cd /glade/work/haiyingx/ParallelIO_cz5/bld && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /glade/work/haiyingx/ParallelIO_cz5 /glade/work/haiyingx/ParallelIO_cz5/src/flib /glade/work/haiyingx/ParallelIO_cz5/bld /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib /glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/CMakeFiles/genf90.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/flib/CMakeFiles/genf90.dir/depend

