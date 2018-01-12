###-------------------------------------------------------------------------###
### CMakeList.txt for adios2pio 
###-------------------------------------------------------------------------###

# Adding PIO definitions - defined in the root directory
ADD_DEFINITIONS(${PIO_DEFINITIONS})

# Include PIO include and lib directories
INCLUDE_DIRECTORIES(${PIO_INCLUDE_DIRS})
LINK_DIRECTORIES(${PIO_LIB_DIR})

# Compiler-specific compiler options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "PGI")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

SET(SRC adios2pio-nm.cxx)
add_library(adios2pio-nm-lib adios2pio-nm-lib.cxx)
include_directories(
  "${PROJECT_SOURCE_DIR}"   # to find foo/foo.h
  "${PROJECT_BINARY_DIR}")  # to find foo/config.h

target_include_directories(adios2pio-nm-lib PUBLIC 
	${CMAKE_SOURCE_DIR}/src/clib ${ADIOS_INCLUDE_DIRS} 
	${NETCDF_C_INCLUDE_DIRS} 
	${PnetCDF_C_INCLUDE_DIRS} 
	${PIO_C_EXTRA_INCLUDE_DIRS})

# System and compiler CPP directives
target_compile_definitions (adios2pio-nm-lib
  PUBLIC ${CMAKE_SYSTEM_DIRECTIVE})
target_compile_definitions (adios2pio-nm-lib
  PUBLIC ${CMAKE_C_COMPILER_DIRECTIVE})

ADD_EXECUTABLE(adios2pio-nm ${SRC})
TARGET_LINK_LIBRARIES(adios2pio-nm adios2pio-nm-lib pioc)

# Library
install (TARGETS adios2pio-nm-lib DESTINATION lib)

# Include/Header File
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/adios2pio-nm-lib.h DESTINATION include)

#===== NetCDF-C =====
find_package (NetCDF "4.3.3" COMPONENTS C)
if (NetCDF_C_FOUND)
  target_include_directories (adios2pio-nm-lib
    PUBLIC ${NetCDF_C_INCLUDE_DIRS})
  target_compile_definitions (adios2pio-nm-lib
    PUBLIC _NETCDF)
  target_link_libraries (adios2pio-nm-lib
    PUBLIC ${NetCDF_C_LIBRARIES})
  if (${NetCDF_C_HAS_PARALLEL})
    target_compile_definitions (adios2pio-nm-lib
      PUBLIC _NETCDF4)
  endif ()
  if (${NetCDF_C_LOGGING_ENABLED})
    target_compile_definitions (adios2pio-nm-lib
      PUBLIC NETCDF_C_LOGGING_ENABLED)
    # netcdf.h needs this to be defined to use netCDF logging.
    target_compile_definitions (adios2pio-nm-lib
      PUBLIC LOGGING)
  endif()
else ()
  target_compile_definitions (adios2pio-nm-lib
    PUBLIC _NONETCDF)
endif ()

#===== PnetCDF-C =====
if (WITH_PNETCDF)
  find_package (PnetCDF "1.7.0" COMPONENTS C)
endif ()
if (PnetCDF_C_FOUND)
  target_include_directories (adios2pio-nm-lib
    PUBLIC ${PnetCDF_C_INCLUDE_DIRS})
  target_compile_definitions (adios2pio-nm-lib
    PUBLIC _PNETCDF)
  target_link_libraries (adios2pio-nm-lib
    PUBLIC ${PnetCDF_C_LIBRARIES})

  # Check library for varn functions
  set (CMAKE_REQUIRED_LIBRARIES ${PnetCDF_C_LIBRARY})
  check_function_exists (ncmpi_get_varn PnetCDF_C_HAS_VARN)
  if (PnetCDF_C_HAS_VARN)
    target_compile_definitions(adios2pio-nm-lib
      PUBLIC USE_PNETCDF_VARN
      PUBLIC USE_PNETCDF_VARN_ON_READ)
  endif()
else ()
  target_compile_definitions (adios2pio-nm-lib
    PUBLIC _NOPNETCDF)
endif ()

#===== ADIOS-C =====
if (WITH_ADIOS)
  find_package (ADIOS "1.10.0")
endif ()
if (ADIOS_FOUND)
  target_include_directories (adios2pio-nm-lib
    PUBLIC ${ADIOS_INCLUDE_DIRS})
  target_compile_definitions (adios2pio-nm-lib
    PUBLIC _ADIOS)
  target_link_libraries (adios2pio-nm-lib
    PUBLIC ${ADIOS_LIBRARIES})
else ()
  target_compile_definitions (adios2pio-nm-lib
    PUBLIC _NOADIOS)
endif ()

#===== Add EXTRAs =====
target_include_directories (adios2pio-nm-lib
  PUBLIC ${PIO_C_EXTRA_INCLUDE_DIRS})
target_link_libraries (adios2pio-nm-lib
  PUBLIC ${PIO_C_EXTRA_LIBRARIES})
target_compile_options (adios2pio-nm-lib
  PRIVATE ${PIO_C_EXTRA_COMPILE_OPTIONS})
target_compile_definitions (adios2pio-nm-lib
  PUBLIC ${PIO_C_EXTRA_COMPILE_DEFINITIONS})
if (PIO_C_EXTRA_LINK_FLAGS)
  set_target_properties(adios2pio-nm-lib PROPERTIES
    LINK_FLAGS ${PIO_C_EXTRA_LINK_FLAGS})
endif ()

#===== Check for necessities =====
if (NOT PnetCDF_C_FOUND AND NOT NetCDF_C_FOUND)
  message (FATAL_ERROR "Must have PnetCDF and/or NetCDF C libraries")
endif ()



