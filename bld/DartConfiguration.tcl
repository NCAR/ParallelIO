# This file is configured by CMake automatically as DartConfiguration.tcl
# If you choose not to use CMake, this file may be hand configured, by
# filling in the required variables.


# Configuration directories and files
SourceDirectory: /glade/work/haiyingx/ParallelIO_cz5
BuildDirectory: /glade/work/haiyingx/ParallelIO_cz5/bld

# Where to place the cost data store
CostDataFile: 

# Site is something like machine.domain, i.e. pragmatic.crd
Site: cheyenne6

# Build name is osname-revision-compiler, i.e. Linux-2.4.2-2smp-c++
BuildName: Linux-mpicxx

# Subprojects
LabelsForSubprojects: 

# Submission information
SubmitURL: http://my.cdash.org/submit.php?project=PIO

# Dashboard start time
NightlyStartTime: 00:00:00 EST

# Commands for the build/test/submit cycle
ConfigureCommand: "/glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake" "/glade/work/haiyingx/ParallelIO_cz5"
MakeCommand: /glade/u/apps/ch/opt/cmake/3.14.4/bin/cmake --build . --config "${CTEST_CONFIGURATION_TYPE}" -- -i
DefaultCTestConfigurationType: Release

# version control
UpdateVersionOnly: 

# CVS options
# Default is "-d -P -A"
CVSCommand: /usr/bin/cvs
CVSUpdateOptions: -d -A -P

# Subversion options
SVNCommand: /usr/bin/svn
SVNOptions: 
SVNUpdateOptions: 

# Git options
GITCommand: /usr/bin/git
GITInitSubmodules: 
GITUpdateOptions: 
GITUpdateCustom: 

# Perforce options
P4Command: P4COMMAND-NOTFOUND
P4Client: 
P4Options: 
P4UpdateOptions: 
P4UpdateCustom: 

# Generic update command
UpdateCommand: /usr/bin/git
UpdateOptions: 
UpdateType: git

# Compiler info
Compiler: /glade/u/apps/ch/opt/ncarcompilers/0.5.0/gnu/8.3.0/mpi/mpicxx
CompilerVersion: 8.3.0

# Dynamic analysis (MemCheck)
PurifyCommand: 
ValgrindCommand: 
ValgrindCommandOptions: 
MemoryCheckType: 
MemoryCheckSanitizerOptions: 
MemoryCheckCommand: MEMORYCHECK_COMMAND-NOTFOUND
MemoryCheckCommandOptions: 
MemoryCheckSuppressionFile: 

# Coverage
CoverageCommand: /glade/u/apps/ch/opt/gnu/8.3.0/bin/gcov
CoverageExtraFlags: -l

# Cluster commands
SlurmBatchCommand: /ncar/opt/slurm/latest/bin/sbatch
SlurmRunCommand: /ncar/opt/slurm/latest/bin/srun

# Testing options
# TimeOut is the amount of time in seconds to wait for processes
# to complete during testing.  After TimeOut seconds, the
# process will be summarily terminated.
# Currently set to 25 minutes
TimeOut: 1500

# During parallel testing CTest will not start a new test if doing
# so would cause the system load to exceed this value.
TestLoad: 

UseLaunchers: 
CurlOptions: 
# warning, if you add new options here that have to do with submit,
# you have to update cmCTestSubmitCommand.cxx

# For CTest submissions that timeout, these options
# specify behavior for retrying the submission
CTestSubmitRetryDelay: 5
CTestSubmitRetryCount: 3
