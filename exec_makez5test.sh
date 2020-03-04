make clean
rm -rf pio_iosys_test_file0
rm -f tmp
rm -f .tmp.swp
make test_z5_create_file D_Z5=1 V=1
make test_darray_3d D_Z5=1 V=1
#mpiexec_mpt -n 4 ./test_z5_create_file >& tmp
#vim tmp
