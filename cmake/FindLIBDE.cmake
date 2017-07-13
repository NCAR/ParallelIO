# - Try to find LIBDE
#
# This can be controlled by setting the LIBDE_PATH (or, equivalently, the 
# LIBDE environment variable).
#
# Once done, this will define:
#
#   LIBDE_FOUND        (BOOL) - system has LIBDE
#   LIBDE_IS_SHARED    (BOOL) - whether library is shared/dynamic
#   LIBDE_INCLUDE_DIR  (PATH) - Location of the C header file
#   LIBDE_INCLUDE_DIRS (LIST) - the LIBDE include directories
#   LIBDE_LIBRARY      (FILE) - Path to the C library file
#   LIBDE_LIBRARIES    (LIST) - link these to use LIBDE
#
include (LibFind)

# Define LIBDE package
define_package_component (LIBDE
                          INCLUDE_NAMES db-file.h de-error.h de-job.h record-op-file.h
                          LIBRARY_NAMES de)

# SEARCH FOR PACKAGE
if (NOT LIBDE_FOUND)

    # Search for the package
    find_package_component(LIBDE)

endif ()
