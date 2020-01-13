###-------------------------------------------------------------------------###
### CMakeList.txt for SCORPIO file info tool
###-------------------------------------------------------------------------###

# Adding Scorpio definitions - defined in the root directory
add_definitions(${PIO_DEFINITIONS})

# Include Scorpio include and lib directories
#include_directories(${PIO_INCLUDE_DIRS})
#link_directories(${PIO_LIB_DIR})

# Enable C++11 support
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SRC ${CMAKE_SOURCE_DIR}/tools/util/argparser.cxx
    ${CMAKE_SOURCE_DIR}/tools/util/spio_misc_tool_utils.cxx
    ${CMAKE_SOURCE_DIR}/tools/util/spio_lib_info.cxx
    spio_file_test_utils.cxx
    spio_finfo.cxx)
add_executable(spio_finfo.exe ${SRC})
#include_directories(
#  "${PROJECT_SOURCE_DIR}"   # to find foo/foo.h
#  "${PROJECT_BINARY_DIR}")  # to find foo/config.h

link_directories(${PIO_LIB_DIR})
target_include_directories(spio_finfo.exe PUBLIC
  ${PIO_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}/src/clib
  ${CMAKE_SOURCE_DIR}/src/clib
  ${CMAKE_SOURCE_DIR}/tools/util
  ${NETCDF_C_INCLUDE_DIRS} 
  ${PnetCDF_C_INCLUDE_DIRS} 
  ${PIO_C_EXTRA_INCLUDE_DIRS})

target_link_libraries(spio_finfo.exe
                      PUBLIC pioc)

# Binary utilities
install (TARGETS spio_finfo.exe DESTINATION bin)

#===== NetCDF-C =====
if (WITH_NETCDF)
find_package (NetCDF "4.3.3" COMPONENTS C)
endif ()
if (NetCDF_C_FOUND)
  target_include_directories (spio_finfo.exe
    PUBLIC ${NetCDF_C_INCLUDE_DIRS})
  target_compile_definitions (spio_finfo.exe
    PUBLIC _NETCDF)
  target_link_libraries (spio_finfo.exe
    PUBLIC ${NetCDF_C_LIBRARIES})
  if (${NetCDF_C_HAS_PARALLEL})
    target_compile_definitions (spio_finfo.exe
      PUBLIC _NETCDF4)
  endif ()
else ()
  target_compile_definitions (spio_finfo.exe
    PUBLIC _NONETCDF)
endif ()

#===== PnetCDF-C =====
if (WITH_PNETCDF)
  find_package (PnetCDF "1.8.1" COMPONENTS C)
endif ()
if (PnetCDF_C_FOUND)
  target_include_directories (spio_finfo.exe
    PUBLIC ${PnetCDF_C_INCLUDE_DIRS})
  target_compile_definitions (spio_finfo.exe
    PUBLIC _PNETCDF)
  target_link_libraries (spio_finfo.exe
    PUBLIC ${PnetCDF_C_LIBRARIES})
else ()
  target_compile_definitions (spio_finfo.exe
    PUBLIC _NOPNETCDF)
endif ()

#===== ADIOS-C =====
if (WITH_ADIOS)
  find_package (ADIOS "1.10.0")
endif ()
if (ADIOS_FOUND)
  target_include_directories (spio_finfo.exe
    PUBLIC ${ADIOS_INCLUDE_DIRS})
  target_compile_definitions (spio_finfo.exe
    PUBLIC _ADIOS)
  target_link_libraries (spio_finfo.exe
    PUBLIC ${ADIOS_LIBRARIES})
else ()
  target_compile_definitions (spio_finfo.exe
    PUBLIC _NOADIOS)
endif ()

#===== Add EXTRAs =====
target_include_directories (spio_finfo.exe
  PUBLIC ${PIO_C_EXTRA_INCLUDE_DIRS})
target_link_libraries (spio_finfo.exe
  PUBLIC ${PIO_C_EXTRA_LIBRARIES})
target_compile_options (spio_finfo.exe
  PRIVATE ${PIO_C_EXTRA_COMPILE_OPTIONS})
target_compile_definitions (spio_finfo.exe
  PUBLIC ${PIO_C_EXTRA_COMPILE_DEFINITIONS})
if (PIO_C_EXTRA_LINK_FLAGS)
  set_target_properties(spio_finfo.exe PROPERTIES
    LINK_FLAGS ${PIO_C_EXTRA_LINK_FLAGS})
endif ()
