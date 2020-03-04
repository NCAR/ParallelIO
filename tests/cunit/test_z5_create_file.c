/*
 * Tests the PIO library with multiple iosysids in use at the
 * same time.
 *
 * This is a simplified, C version of the fortran pio_iosystem_tests2.F90.
 *
 * Ed Hartnett
 */
#include <config.h>
#include <pio.h>
#include <pio_tests.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
/* The number of tasks this test should run on. */
#define TARGET_NTASKS 4

/* The name of this test. */
#define TEST_NAME "test_z5_create_file"

/* Used to define netcdf test file. */
#define PIO_TF_MAX_STR_LEN 100
#define Z5INT64 "INT64"
#define Z5INT32 "INT32"
#define Z5INT16 "INT16"
#define Z5INT8 "INT8"
#define Z5UINT64 "UINT64"
#define Z5UINT32 "UINT32"
#define Z5UINT16 "UINT16"
#define Z5UINT8 "UINT8"
#define Z5FLOAT "FLOAT"
#define Z5DOUBLE "DOUBLE"
#define ATTNAME "EMPTYPLACEHODER"
//#define DIMNAME "filename_dim"
//#define DIMNAME "pio_iosys_test_file0.z5/filename_dim"
/* This creates a netCDF file in the specified format, with some
 * sample values. */
int create_file(MPI_Comm comm, int iosysid, int format, char *filename,
                char *attname, char *dimname, int my_rank, int *ncidp)
{
    int ncid, dimid0, dimid1;
    int varidint64, varidint32, varidint16, varidint8;
    int variduint64, variduint32, variduint16, variduint8;
    int variddouble, varidfloat;
    int ret;

    /* Create the file. */
    if ((ret = PIOc_createfile(iosysid, ncidp, &format, filename, NC_CLOBBER)))
        return ret;
    MPI_Barrier(comm);
//    printf("end of PIOc_createfile ncidp:%d\n", *ncidp);
//    /* Use the ncid to set the IO system error handler. This function
//     * is deprecated. */
//    PIOc_Set_File_Error_Handling(ncid, PIO_RETURN_ERROR);
//    int method = PIOc_Set_File_Error_Handling(ncid, PIO_RETURN_ERROR);
//    if (method != PIO_RETURN_ERROR)
//        return ERR_WRONG;
//
    /* Define a dimension. */
//    dim_desc_t *dim0;
    char dimname0[] = "lat";
    int dimval0 = 384;

    char dimname1[] = "lon";
    int dimval1 = 384;

    if ((ret = PIOc_def_dim(*ncidp, dimname0, dimval0, &dimid0)))
        return ret;

    if ((ret = PIOc_def_dim(*ncidp, dimname1, dimval1, &dimid1)))
        return ret;

    int twod_dimids[2],oned_dimids[1];
    twod_dimids[0] = dimid0;
    twod_dimids[1] = dimid1;
    oned_dimids[0] = 1;
    MPI_Barrier(comm);
    /* Define a 1-D variable. */
    if ((ret = PIOc_def_var(*ncidp, Z5INT64, NC_INT64, 2, twod_dimids, &varidint64)))
        return ret;
    MPI_Barrier(comm);

    /* Define a 1-D variable. */
    if ((ret = PIOc_def_var(*ncidp, Z5INT32, NC_INT, 0, oned_dimids, &varidint32)))
        return ret;
    MPI_Barrier(comm);

    if ((ret = PIOc_def_var(*ncidp, Z5INT32, NC_INT, 2, twod_dimids, &varidint32)))
        return ret;
    MPI_Barrier(comm);
    /* Define a 1-D variable. */
    if ((ret = PIOc_def_var(*ncidp, Z5INT16, NC_SHORT, 2, twod_dimids, &varidint16)))
        return ret;
    MPI_Barrier(comm);

    /* Define a 1-D variable. */
    if ((ret = PIOc_def_var(*ncidp, Z5INT8, NC_BYTE, 2, twod_dimids, &varidint8)))
        return ret;
    MPI_Barrier(comm);
    /* Define a 1-D variable. */
    if ((ret = PIOc_def_var(*ncidp, Z5UINT64, NC_UINT64, 2, twod_dimids, &variduint64)))
        return ret;
    MPI_Barrier(comm);

    /* Define a 1-D variable. */
    if ((ret = PIOc_def_var(*ncidp, Z5UINT32, NC_UINT, 2, twod_dimids, &variduint32)))
        return ret;
    MPI_Barrier(comm);

    /* Define a 1-D variable. */
    if ((ret = PIOc_def_var(*ncidp, Z5UINT16, NC_USHORT, 2, twod_dimids, &variduint16)))
        return ret;
    MPI_Barrier(comm);

    /* Define a 1-D variable. */
    if ((ret = PIOc_def_var(*ncidp, Z5UINT8, NC_CHAR, 2, twod_dimids, &variduint8)))
        return ret;
    MPI_Barrier(comm);

    /* Define a 1-D variable. */
    if ((ret = PIOc_def_var(*ncidp, Z5DOUBLE, NC_DOUBLE, 2, twod_dimids, &variddouble)))
        return ret;
    MPI_Barrier(comm);

    /* Define a 1-D variable. */
    if ((ret = PIOc_def_var(*ncidp, Z5FLOAT, NC_FLOAT, 2, twod_dimids, &varidfloat)))
        return ret;
    MPI_Barrier(comm);

    int varidp;
    char name[]="DOUBLE";
    ret = PIOc_inq_varid(*ncidp,name, &varidp);
    fprintf(stderr,"varidp = %d\n",varidp);
    /* Write an attribute. */
    char attributename0[] = "time";
    char attributeval0[] = "noon";
    if ((ret = PIOc_put_att_text(*ncidp, varidint64, attributename0, strlen(filename), attributeval0)))
        return ret;
    
    char attributename1[] = "long";
    float attributeval1 = 42.0;
    if ((ret = PIOc_put_att_float(*ncidp, varidint64, attributename1, NC_FLOAT, 1, &attributeval1)))
        return ret;

    char attributename2[] = "intatt";
    int attributeval2 = 23;
    if ((ret = PIOc_put_att_int(*ncidp, varidint64, attributename2, NC_INT, 1, &attributeval2)))
        return ret;

    char attributename3[] = "uintatt";
    unsigned int attributeval3 = 23;
    if ((ret = PIOc_put_att_uint(*ncidp, varidint64, attributename3, NC_UINT, 1, &attributeval3)))
        return ret;

    //long int count[] = {dimval0/TARGET_NTASKS, dimval1};
    long int count[2];
    long int start[2];
    if ((dimval0/96) >= (my_rank+1)){
       start[0] = my_rank*96;
       start[1] = 0;
       count[0] = 96;
       count[1] = dimval1;}
    else{
       start[0] =start[1]=0;
       count[0] =count[1]= 0;}
    long int count1[1];
    long int start1[1];
    start1[0]=0;
    count1[0]=1;
    long int int64_array[dimval0/4][dimval1];
    char int8_t_array[dimval0/4][dimval1];
    int16_t int16_t_array[dimval0/4][dimval1];
    int32_t int32_t_array[dimval0/4][dimval1];
    int64_t int64_t_array[dimval0/4][dimval1];
    uint8_t   uint8_t_array[dimval0/4][dimval1];
    uint16_t uint16_t_array[dimval0/4][dimval1];
    uint32_t uint32_t_array[dimval0/4][dimval1];
    uint64_t uint64_t_array[dimval0/4][dimval1];
    double double_array[dimval0/4][dimval1];
    float float_array[dimval0/4][dimval1];
    for (int i = 0; i < dimval0/4; i++)
    {
        for (int j = 0; j < dimval1; j++)
        {
            int64_array[i][j] = rand()%1000+1;
            int32_t_array[i][j] = rand()%1000+1;
            int16_t_array[i][j] = rand()%1000+1;
            int8_t_array[i][j] = rand()%254+1;
            int64_t_array[i][j] = rand()%1000+1;
            uint64_t_array[i][j] = rand()%1000+1;
            uint32_t_array[i][j] = rand()%1000+1;
            uint16_t_array[i][j] = rand()%1000+1;
            uint8_t_array[i][j] = 'a'; 
            double_array[i][j] = rand()%1000+1;
            float_array[i][j] = rand()%1000+1;
        }
    }

    MPI_Barrier(comm);

    fprintf(stderr,"before ret int 8 =%d\n",ret);
    if ((ret = PIOc_put_vara_schar(*ncidp, varidint8, start, count, (int8_t *)int8_t_array)))
        ERR(ret);

    fprintf(stderr,"ret int 8 =%d\n",ret);
    MPI_Barrier(comm);
    if ((ret = PIOc_put_vara_uchar(*ncidp, variduint8, start, count, (uint8_t *)uint8_t_array)))
        ERR(ret);

    MPI_Barrier(comm);
    if ((ret = PIOc_put_vara_int(*ncidp, varidint32, start, count, (int32_t *)int32_t_array)))
        ERR(ret);

    if ((ret = PIOc_put_vara_int(*ncidp, varidint32, start, count, (int32_t *)int32_t_array)))
        ERR(ret);
    if ((ret = PIOc_put_vara_longlong(*ncidp, varidint64, start, count, (int64_t *)int64_array)))
        ERR(ret);

    if ((ret = PIOc_put_vara_short(*ncidp, varidint16, start, count, (int16_t *)int16_t_array)))
        ERR(ret);

    if ((ret = PIOc_put_vara_uint(*ncidp, variduint32, start, count, (uint32_t *)uint32_t_array)))
        ERR(ret);

    if ((ret = PIOc_put_vara_ulonglong(*ncidp, variduint64, start, count, (uint64_t *)uint64_t_array)))
        ERR(ret);

    if ((ret = PIOc_put_vara_ushort(*ncidp, variduint16, start, count, (uint16_t *)uint16_t_array)))
        ERR(ret);

    if ((ret = PIOc_put_vara_double(*ncidp, variddouble, start, count, (double *)double_array)))
        ERR(ret);

    if ((ret = PIOc_put_vara_float(*ncidp, varidfloat, start, count, (float *)float_array)))
        ERR(ret);

    MPI_Barrier(comm);
    long int int64_array_in[dimval0][dimval1];
    int8_t int8_t_array_in[dimval0][dimval1];
    int16_t int16_t_array_in[dimval0][dimval1];
    int32_t int32_t_array_in[dimval0][dimval1];
    int64_t int64_t_array_in[dimval0][dimval1];
    uint8_t   uint8_t_array_in[dimval0][dimval1];
    uint16_t uint16_t_array_in[dimval0][dimval1];
    uint32_t uint32_t_array_in[dimval0][dimval1];
    uint64_t uint64_t_array_in[dimval0][dimval1];
    float float_array_in[dimval0][dimval1];
    double double_array_in[dimval0][dimval1];
    MPI_Barrier(comm);

    /*if ((ret = PIOc_get_vara_schar(*ncidp, varidint8, start, count, (int8_t *)int8_t_array_in)))
        ERR(ret);

    if ((ret = PIOc_get_vara_uchar(*ncidp, variduint8, start, count, (uint8_t *)uint8_t_array_in)))
        ERR(ret);

    if ((ret = PIOc_get_vara_int(*ncidp, varidint32, start, count, (int32_t *)int32_t_array_in)))
        ERR(ret);

    if ((ret = PIOc_get_vara_longlong(*ncidp, varidint64, start, count, (int64_t *)int64_array_in)))
        ERR(ret);

    if ((ret = PIOc_get_vara_short(*ncidp, varidint16, start, count, (int16_t *)int16_t_array_in)))
        ERR(ret);

    if ((ret = PIOc_get_vara_uint(*ncidp, variduint32, start, count, (uint32_t *)uint32_t_array_in)))
        ERR(ret);

    if ((ret = PIOc_get_vara_ulonglong(*ncidp, variduint64, start, count, (uint64_t *)uint64_t_array_in)))
        ERR(ret);

    if ((ret = PIOc_get_vara_ushort(*ncidp, variduint16, start, count, (uint16_t *)uint16_t_array_in)))
        ERR(ret);

    if ((ret = PIOc_get_vara_double(*ncidp, variddouble, start, count, (double *)double_array_in)))
        ERR(ret);

    if ((ret = PIOc_get_vara_float(*ncidp, varidfloat, start, count, (float *)float_array_in)))
        ERR(ret);
    MPI_Barrier(comm);

    for (int x = 0; x < count[0]; x++)
    {
        for (int y = 0; y < count[1]; y++)
        {
            assert(int64_array_in[x][y] == int64_array[x][y]);
            assert(int8_t_array_in[x][y] == int8_t_array[x][y]);
            assert(int16_t_array_in[x][y] == int16_t_array[x][y]);
            assert(int32_t_array_in[x][y] == int32_t_array[x][y]);
            assert(int64_t_array_in[x][y] == int64_t_array[x][y]);
            assert(uint8_t_array_in[x][y] == uint8_t_array[x][y]);
            assert(uint16_t_array_in[x][y] == uint16_t_array[x][y]);
            assert(uint32_t_array_in[x][y] == uint32_t_array[x][y]);
            assert(uint64_t_array_in[x][y] == uint64_t_array[x][y]);
            assert(float_array_in[x][y] == float_array[x][y]);
            assert(double_array_in[x][y] == double_array[x][y]);
        }
    }
*/
    /* End define mode. */
    if ((ret = PIOc_enddef(*ncidp)))
        return ret;
    /* Close the file. */
    if ((ret = PIOc_closefile(*ncidp)))
        return ret;
    return PIO_NOERR;
}

///* This checks an already-open netCDF file. */
//int check_file(MPI_Comm comm, int iosysid, int format, int ncid, char *filename,
//               char *attname, char *dimname, int my_rank)
//{
//    int dimid;
//    int ret;
//
//    /* Check the file. */
//    if ((ret = PIOc_inq_dimid(ncid, dimname, &dimid)))
//        return ret;
//
//    return PIO_NOERR;
//}
//
///* This opens and checks a netCDF file. */
//int open_and_check_file(MPI_Comm comm, int iosysid, int iotype, int *ncid, char *fname,
//                        char *attname, char *dimname, int disable_close, int my_rank)
//{
//    int mode = PIO_WRITE;
//    int ret;
//
//    /* Open the file. */
//    if ((ret = PIOc_openfile(iosysid, ncid, &iotype, fname, mode)))
//        return ret;
//
//    /* Check the file. */
//    if ((ret = check_file(comm, iosysid, iotype, *ncid, fname, attname, dimname, my_rank)))
//        return ret;
//
//    /* Close the file, maybe. */
//    if (!disable_close)
//        if ((ret = PIOc_closefile(*ncid)))
//            return ret;
//
//    return PIO_NOERR;
//}

/* Run async tests. */
int main(int argc, char **argv)
{
    int my_rank; /* Zero-based rank of processor. */
    int ntasks; /* Number of processors involved in current execution. */
    int iosysid; /* The ID for the parallel I/O system. */
    int iosysid_world; /* The ID for the parallel I/O system. */
    int ret; /* Return code. */
    int num_flavors;
    int iotypes[NUM_FLAVORS];
    MPI_Comm test_comm;

    /* Initialize test. */
    if ((ret = pio_test_init2(argc, argv, &my_rank, &ntasks, TARGET_NTASKS, TARGET_NTASKS,
                              -1, &test_comm)))
        ERR(ERR_INIT);

    /* Test code runs on TARGET_NTASKS tasks. The left over tasks do
     * nothing. */
    if (my_rank < TARGET_NTASKS)
    {
        /* Figure out iotypes. */
        if ((ret = get_iotypes(&num_flavors, iotypes)))
            ERR(ret);

        /* Split world into odd and even. */
        MPI_Comm newcomm;
        int even = my_rank % 2 ? 0 : 1;
        if ((ret = MPI_Comm_split(test_comm, even, 0, &newcomm)))
            MPIERR(ret);

        /* Get rank in new communicator and its size. */
        int new_rank, new_size;
        if ((ret = MPI_Comm_rank(newcomm, &new_rank)))
            MPIERR(ret);
        if ((ret = MPI_Comm_size(newcomm, &new_size)))
            MPIERR(ret);

        /* Initialize PIO system. */
        if ((ret = PIOc_Init_Intracomm(newcomm, 2, 1, 0, 1, &iosysid)))
            ERR(ret);

        /* This should fail. */
        if (PIOc_finalize(iosysid + TEST_VAL_42) != PIO_EBADID)
            ERR(ERR_WRONG);

        /* Initialize another PIO system. */
        if ((ret = PIOc_Init_Intracomm(test_comm, 4, 1, 0, 1, &iosysid_world)))
            ERR(ret);

        int file0id;
        int file1id;
        int file2id;

        for (int i = 0; i < num_flavors; i++)
        {
            if (iotypes[i] == 5)
            {
                char fname0[] = "pio_iosys_test_file0";
                char fname1[] = "pio_iosys_test_file1";
                char fname2[] = "pio_iosys_test_file2";
                char* group_tmp = "/group";
                char* dimname0 = "dim0";
                char* dimname1 = "dim1";
                char* dimname2 = "dim2";
//                char* dimname = (char*) malloc (1 + strlen(fname0) + strlen(group_tmp) + strlen(dim_tmp));
//                strcpy(dimname, fname0);
//                strcat(dimname, group_tmp);
//                strcat(dimname, dim_tmp);
//                printf("dimname is %s\n", dimname);
                if ((ret = create_file(test_comm, iosysid_world, iotypes[i], fname0, ATTNAME,
                                       dimname0, my_rank, &file0id)))
                    ERR(ret);
                if ((ret = create_file(test_comm, iosysid_world, iotypes[i], fname1, ATTNAME,
                                       dimname1, my_rank, &file1id)))
                    ERR(ret);
                if ((ret = create_file(test_comm, iosysid_world, iotypes[i], fname2, ATTNAME,
                                       dimname2, my_rank, &file2id)))
                    ERR(ret);
//                printf("1st ncid=%d\n", file0id);
//                if ((ret = create_file(test_comm, iosysid_world, iotypes[i], fname1, ATTNAME,
//                                       dimname, my_rank, &file1id)))
//                    ERR(ret);
//                printf("2nd ncid=%d\n", file1id);
//                if ((ret = create_file(test_comm, iosysid_world, iotypes[i], fname2, ATTNAME,
//                                       dimname, my_rank, &file2id)))
//                    ERR(ret);
//                printf("3rd ncid=%d\n", file2id);
                MPI_Barrier(test_comm);


//                if ((ret = PIOc_def_var(z5_file0.z5id, z5_var1.varname, PIO_FLOAT, 1, &dimid, &z5_var1.varid)))
//                    return ret;
//            /* Now check the first file. */
//            int ncid;
//            if ((ret = open_and_check_file(test_comm, iosysid_world, iotypes[i], &ncid, fname0,
//                                           ATTNAME, DIMNAME, 1, my_rank)))
//                ERR(ret);
//
//            /* Now have the odd/even communicators each check one of the
//             * remaining files. */
//            int ncid2;
//            char *fname = even ? fname1 : fname2;
//            if ((ret = open_and_check_file(newcomm, iosysid, iotypes[i], &ncid2, fname,
//                                           ATTNAME, DIMNAME, 1, my_rank)))
//                ERR(ret);
//
//
//            /* Close the still-open files. */
//            if ((ret = PIOc_closefile(ncid)))
//                ERR(ret);
//            if ((ret = PIOc_closefile(ncid2)))
//                ERR(ret);

                /* Wait for everyone to finish. */

                if ((ret = MPI_Barrier(test_comm)))
                    MPIERR(ret);
            }

        } /* next iotype */

        if ((ret = MPI_Comm_free(&newcomm)))
            MPIERR(ret);

        /* Finalize PIO system. */
        if ((ret = PIOc_finalize(iosysid)))
            ERR(ret);

        /* Finalize PIO system. */
        if ((ret = PIOc_finalize(iosysid_world)))
            ERR(ret);
    } /* my_rank < TARGET_NTASKS */

    /* Finalize test. */
    if ((ret = pio_test_finalize(&test_comm)))
        return ret;

    printf("%d %s SUCCESS!!\n", my_rank, TEST_NAME);

    return 0;
}
