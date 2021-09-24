#!/bin/sh

# Get/Generate the Dashboard Model
if [ $# -eq 0 ]; then
	model=Experimental
else
	model=$1
fi

module load gcc/11.1.0-5ikoznk
module load cmake/3.20.5-yjp2hz6

export PATH=/nfs/gce/projects/climate/software/mpich/3.4.2/gcc-11.1.0/bin:$PATH

export PNETCDFROOT=/nfs/gce/projects/climate/software/pnetcdf/1.12.2/mpich-3.4.2/gcc-11.1.0
export NETCDFROOT=/nfs/gce/projects/climate/software/netcdf/4.8.1c-4.3.1cxx-4.5.3f-parallel/mpich-3.4.2/gcc-11.1.0
export HDF5ROOT=/nfs/gce/projects/climate/software/hdf5/1.12.1/mpich-3.4.2/gcc-11.1.0
export ZLIBROOT=/nfs/gce/software/spack/opt/spack/linux-ubuntu18.04-x86_64/gcc-7.5.0/zlib-1.2.11-smoyzzo

export USE_MALLOC=ON
export ENABLE_TESTS=ON

export CFLAGS="-fsanitize=address -fno-omit-frame-pointer -O2 -g"
export CXXFLAGS="-fsanitize=address -fno-omit-frame-pointer -O2 -g"
export FFLAGS="-fsanitize=address -fno-omit-frame-pointer -O2 -g -fbacktrace -fcheck=bounds -ffpe-trap=invalid,zero,overflow"

export CC=mpicc
export CXX=mpicxx
export FC=mpif90

export PIO_DASHBOARD_PROJECT_NAME=ACME_PIO
export PIO_DASHBOARD_SITE=anlgce-`hostname`
export PIO_DASHBOARD_ROOT=${PWD}
export CTEST_SCRIPT_DIRECTORY=${PIO_DASHBOARD_ROOT}/src
export PIO_DASHBOARD_SOURCE_DIR=${CTEST_SCRIPT_DIRECTORY}
export PIO_COMPILER_ID=gcc-`gcc --version | head -n 1 | cut -d' ' -f4`

echo "CTEST_SCRIPT_DIRECTORY="${CTEST_SCRIPT_DIRECTORY}
echo "PIO_DASHBOARD_SOURCE_DIR="${PIO_DASHBOARD_SOURCE_DIR}

if [ ! -d src ]; then
  git clone --branch develop https://github.com/E3SM-Project/scorpio src
fi
cd src

ctest -S CTestScript.cmake,${model} -VV
