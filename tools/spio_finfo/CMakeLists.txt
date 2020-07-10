###-------------------------------------------------------------------------###
### CMakeList.txt for SCORPIO file info tool
###-------------------------------------------------------------------------###

# Adding Scorpio definitions - defined in the root directory
add_definitions(${PIO_DEFINITIONS})

# Include Scorpio include and lib directories
#include_directories(${PIO_INCLUDE_DIRS})
#link_directories(${PIO_LIB_DIR})

# Enable C++11 support
string (TOUPPER "${CMAKE_CXX_COMPILER_ID}" CMAKE_CXX_COMPILER_NAME)
if (CMAKE_CXX_COMPILER_NAME STREQUAL "CRAY")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -h std=c++11")
else ()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif ()

set(SRC ${CMAKE_SOURCE_DIR}/tools/util/argparser.cxx
    ${CMAKE_SOURCE_DIR}/tools/util/spio_misc_tool_utils.cxx
    ${CMAKE_SOURCE_DIR}/tools/util/spio_lib_info.cxx
    spio_file_test_utils.cxx
    spio_finfo.cxx
    spio_finfo_tool.cxx)
add_executable(spio_finfo.exe ${SRC})
#include_directories(
#  "${PROJECT_SOURCE_DIR}"   # to find foo/foo.h
#  "${PROJECT_BINARY_DIR}")  # to find foo/config.h

link_directories(${PIO_LIB_DIR})
target_include_directories(spio_finfo.exe PRIVATE
  ${PIO_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}/src/clib
  ${CMAKE_SOURCE_DIR}/src/clib
  ${CMAKE_SOURCE_DIR}/tools/util
  ${NETCDF_C_INCLUDE_DIRS} 
  ${PnetCDF_C_INCLUDE_DIRS} 
  ${PIO_C_EXTRA_INCLUDE_DIRS})

target_link_libraries(spio_finfo.exe
                      PRIVATE pioc)

target_compile_definitions (spio_finfo.exe
  PUBLIC MPICH_SKIP_MPICXX)
target_compile_definitions (spio_finfo.exe
  PUBLIC OMPI_SKIP_MPICXX)

# Binary utilities
install (TARGETS spio_finfo.exe DESTINATION bin)

#===== NetCDF-C =====
if (WITH_NETCDF)
find_package (NetCDF "4.3.3" COMPONENTS C)
endif ()
if (NetCDF_C_FOUND)
  target_include_directories (spio_finfo.exe
    PRIVATE ${NetCDF_C_INCLUDE_DIRS})
  target_compile_definitions (spio_finfo.exe
    PRIVATE _NETCDF)
  target_link_libraries (spio_finfo.exe
    PRIVATE ${NetCDF_C_LIBRARIES})
  if (${NetCDF_C_HAS_PARALLEL})
    target_compile_definitions (spio_finfo.exe
      PRIVATE _NETCDF4)
  endif ()
else ()
  target_compile_definitions (spio_finfo.exe
    PRIVATE _NONETCDF)
endif ()

#===== PnetCDF-C =====
if (WITH_PNETCDF)
  find_package (PnetCDF "1.8.1" COMPONENTS C)
endif ()
if (PnetCDF_C_FOUND)
  target_include_directories (spio_finfo.exe
    PRIVATE ${PnetCDF_C_INCLUDE_DIRS})
  target_compile_definitions (spio_finfo.exe
    PRIVATE _PNETCDF)
  target_link_libraries (spio_finfo.exe
    PRIVATE ${PnetCDF_C_LIBRARIES})
else ()
  target_compile_definitions (spio_finfo.exe
    PRIVATE _NOPNETCDF)
endif ()

#===== ADIOS-C =====
if (WITH_ADIOS)
  find_package (ADIOS "1.10.0")
endif ()
if (ADIOS_FOUND)
  target_include_directories (spio_finfo.exe
    PRIVATE ${ADIOS_INCLUDE_DIRS})
  target_compile_definitions (spio_finfo.exe
    PRIVATE _ADIOS)
  target_link_libraries (spio_finfo.exe
    PRIVATE ${ADIOS_LIBRARIES})
else ()
  target_compile_definitions (spio_finfo.exe
    PRIVATE _NOADIOS)
endif ()

#===== Add EXTRAs =====
target_include_directories (spio_finfo.exe
  PRIVATE ${PIO_C_EXTRA_INCLUDE_DIRS})
target_link_libraries (spio_finfo.exe
  PRIVATE ${PIO_C_EXTRA_LIBRARIES})
target_compile_options (spio_finfo.exe
  PRIVATE ${PIO_C_EXTRA_COMPILE_OPTIONS})
target_compile_definitions (spio_finfo.exe
  PRIVATE ${PIO_C_EXTRA_COMPILE_DEFINITIONS})
if (PIO_C_EXTRA_LINK_FLAGS)
  set_target_properties(spio_finfo.exe PROPERTIES
    LINK_FLAGS ${PIO_C_EXTRA_LINK_FLAGS})
endif ()
