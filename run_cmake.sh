module load gnu
module remove netcdf
module load pnetcdf netcdf-mpi cmake
cd bld
rm -rf *
CC=mpicc CXX=mpicxx FC=mpif90 cmake -DCMAKE_INSTALL_PREFIX=~/dev/install_pr/ParallelIO/ -DZ5_DIR=/glade/u/home/weile/miniconda3/envs/z5-py36/include -DWITH_ZLIB=ON -DZLIB_ROOT=/glade/u/home/weile/miniconda3/envs/z5-py36/ -DBOOST_ROOT=/glade/u/home/weile/miniconda3/envs/z5-py36 -DWITH_MARRAY=ON -DWITH_BLOSC=ON -DBLOSC_ROOT=/glade/u/home/weile/miniconda3/envs/z5-py36 -DWITH_LZ4=ON -DLZ4_ROOT=/glade/u/home/weile/miniconda3/envs/z5-py36 -DMARRAY_INCLUDE_DIR=/glade/u/home/weile/miniconda3/envs/z5-py36/include -DCMAKE_VERBOSE_MAKEFILE=ON ..
make V=1
#make test_z5_create_file V=1 >& tmp
