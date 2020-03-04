# Install script for directory: /glade/work/haiyingx/ParallelIO_cz5/src/flib

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/glade/u/home/haiyingx/dev/install_pr/ParallelIO")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/libpiof.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "/glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/pio.mod"
    "/glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/pio_nf.mod"
    "/glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/pio_types.mod"
    "/glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/piolib_mod.mod"
    "/glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/pionfget_mod.mod"
    "/glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/pio_kinds.mod"
    "/glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/pio_support.mod"
    "/glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/piodarray.mod"
    "/glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/pionfatt_mod.mod"
    "/glade/work/haiyingx/ParallelIO_cz5/bld/src/flib/pionfput_mod.mod"
    )
endif()

