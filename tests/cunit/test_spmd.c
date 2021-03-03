/*
 * This program tests some internal functions in the PIO library.
 *
 * Jim Edwards
 * Ed Hartnett, 11/23/16
 */
#include <pio.h>
#include <pio_tests.h>
#include <pio_internal.h>

/* The number of tasks this test should run on. */
#define TARGET_NTASKS 4

/* The minimum number of tasks this test should run on. */
#define MIN_NTASKS 1

/* The name of this test. */
#define TEST_NAME "test_spmd"

/* Number of test cases in inner loop of test. */
#define NUM_TEST_CASES 5

/* Test MPI_Alltoallw by having processor i send different amounts of
 * data to each processor.  The first test sends i items to processor
 * i from all processors. */
int run_spmd_tests(MPI_Comm test_comm)
{
    int my_rank;  /* 0-based rank in test_comm. */
    int ntasks;   /* Number of tasks in test_comm. */
    int num_elem; /* Number of elements in buffers. */
    int type_size; /* Size in bytes of an element. */
    int mpierr;   /* Return value from MPI calls. */
    int ret;      /* Return value. */

    /* Learn rank and size. */
    if ((mpierr = MPI_Comm_size(test_comm, &ntasks)))
        MPIERR(mpierr);
    if ((mpierr = MPI_Comm_rank(test_comm, &my_rank)))
        MPIERR(mpierr);

    /* Determine size of buffers. */
    num_elem = ntasks;

    int sbuf[ntasks];    /* The send buffer. */
    int rbuf[ntasks];    /* The receive buffer. */
    int sendcounts[ntasks]; /* Number of elements of data being sent from each task. */
    int recvcounts[ntasks]; /* Number of elements of data being sent from each task. */
    int sdispls[ntasks]; /* Displacements for sending data. */
    int rdispls[ntasks]; /* Displacements for receiving data. */
    MPI_Datatype sendtypes[ntasks]; /* MPI types of data being sent. */
    MPI_Datatype recvtypes[ntasks]; /* MPI types of data being received. */

    /* Load up the send buffer. */
    for (int i = 0; i < num_elem; i++)
        sbuf[i] = my_rank;

    /* Load up the receive buffer to make debugging easier. */
    for (int i = 0; i < num_elem; i++)
        rbuf[i] = -999;

    /* Get the size of the int type for MPI. (Should always be 4.) */
    if ((mpierr = MPI_Type_size(MPI_INT, &type_size)))
        return check_mpi(NULL, NULL, mpierr, __FILE__, __LINE__);
    assert(type_size == sizeof(int));

    /* Initialize the arrays. */
    for (int i = 0; i < ntasks; i++)
    {
        sendcounts[i] = 1;
        sdispls[i] = 0;
        sendtypes[i] = MPI_INT;
        recvcounts[i] = 1;
        rdispls[i] = i * type_size;
        recvtypes[i] = MPI_INT;
    }

    /* Perform tests for different values of msg_cnt. (BTW it hangs
     * with msg_cnt = 1!). */
    for (int msg_cnt = 0; msg_cnt < TARGET_NTASKS; msg_cnt = msg_cnt ? msg_cnt * 2 : 4)
    {
        if (!my_rank)
            printf("message count %d\n",msg_cnt);

        for (int itest = 0; itest < NUM_TEST_CASES; itest++)
        {
            rearr_comm_fc_opt_t fc;            
            fc.hs = false;
            fc.isend = false;

            /* Wait for all tasks. */
            MPI_Barrier(test_comm);

            /* Print results. */
            if (!my_rank)
                for (int e = 0; e < num_elem; e++)
                    printf("sbuf[%d] = %d\n", e, sbuf[e]);

            /* Set the parameters different for each test case. */
            if (itest == 1)
            {
                fc.hs = true;
                fc.isend = true;
            }
            else if (itest == 2)
            {
                fc.hs = false;
                fc.isend = true;
            }
            else if (itest == 3)
            {
                fc.hs = false;
                fc.isend = false;
            }
            else if (itest == 4)
            {
                fc.hs = true;
                fc.isend = false;
            }

            /* Run the swapm function. */
            if ((ret = pio_swapm(sbuf, sendcounts, sdispls, sendtypes, rbuf, recvcounts,
                                 rdispls, recvtypes, test_comm, &fc)))
                return ret;

            /* Print results. */
            /* MPI_Barrier(test_comm); */
            /* for (int e = 0; e < num_elem; e++) */
            /*     printf("%d sbuf[%d] = %d\n", my_rank, e, sbuf[e]); */
            /* MPI_Barrier(test_comm); */
            /* for (int e = 0; e < num_elem; e++) */
            /*     printf("%d rbuf[%d] = %d\n", my_rank, e, rbuf[e]); */

            /* Check that rbuf has 0, 1, ..., ntasks-1. */
            for (int e = 0; e < num_elem; e++)
                if (((int *)rbuf)[e] != e)
                    return ERR_WRONG;
        }
    }

    return 0;
}

/* Test some of the functions in the file pioc_sc.c. 
 *
 * @param test_comm the MPI communicator that the test code is running on. 
 * @returns 0 for success, error code otherwise.
 */
int run_sc_tests(MPI_Comm test_comm)
{
#define SC_ARRAY_LEN 3
    int my_rank;  /* 0-based rank in test_comm. */
    int ntasks;   /* Number of tasks in test_comm. */
    int mpierr;   /* Return value from MPI calls. */
    int array1[SC_ARRAY_LEN] = {7, 42, 14};
    int array2[SC_ARRAY_LEN] = {2, 3, 7};
    int array3[SC_ARRAY_LEN] = {90, 180, 270};
    int array4[SC_ARRAY_LEN] = {1, 180, 270};

    /* Learn rank and size. */
    if ((mpierr = MPI_Comm_size(test_comm, &ntasks)))
        MPIERR(mpierr);
    if ((mpierr = MPI_Comm_rank(test_comm, &my_rank)))
        MPIERR(mpierr);

    /* Test the gcd() function. */
    if (gcd(0, 2) != 2)
        return ERR_WRONG;
    if (gcd(2, 2) != 2)
        return ERR_WRONG;
    if (gcd(42, 2) != 2)
        return ERR_WRONG;

    /* Test the long long version. */
    if (lgcd(0, 2) != 2)
        return ERR_WRONG;
    if (lgcd(2, 2) != 2)
        return ERR_WRONG;
    if (lgcd(42, 2) != 2)
        return ERR_WRONG;

    /* Test the gcd_array() function. */
    if (gcd_array(SC_ARRAY_LEN, array1) != 7)
        return ERR_WRONG;
    if (gcd_array(SC_ARRAY_LEN, array2) != 1)
        return ERR_WRONG;
    if (gcd_array(SC_ARRAY_LEN, array3) != 90)
        return ERR_WRONG;
    if (gcd_array(SC_ARRAY_LEN, array4) != 1)
        return ERR_WRONG;

    /* Test compute_one_dim. */
    PIO_Offset start, count;
    compute_one_dim(4, 4, my_rank, &start, &count);
    if (start != my_rank || count != 1)
        return ERR_WRONG;
    compute_one_dim(400, 4, my_rank, &start, &count);
    if (start != my_rank * 100 || count != 100)
        return ERR_WRONG;
    /* Left over data will go to task 3. */
    compute_one_dim(5, 4, my_rank, &start, &count);
    if (start != my_rank || count != (my_rank == 3 ? 2 : 1))
        return ERR_WRONG;
    printf("my_rank = %d start = %lld count = %lld\n", my_rank, start, count);
    return 0;
}

/* Test some list stuff. */
int test_lists()
{
    file_desc_t *fdesc;
    
    /* Test that bad input is correctly rejected. */
    if (pio_delete_iodesc_from_list(42) != PIO_EBADID)
        return ERR_WRONG;
    if (pio_delete_iosystem_from_list(42) != PIO_EBADID)
        return ERR_WRONG;
    if (pio_delete_file_from_list(42) != PIO_EBADID)
        return ERR_WRONG;
    if (pio_get_file(42, NULL) != PIO_EINVAL)
        return ERR_WRONG;
    if (pio_get_file(42, &fdesc) != PIO_EBADID)
        return ERR_WRONG;
    return 0;
}

/* Test the ceil2() and pair() functions. */
int test_ceil2_pair()
{
    /* Test the ceil2() function. */
    if (ceil2(1) != 1)
        return ERR_WRONG;
    if (ceil2(-100) != 1)
        return ERR_WRONG;
    if (ceil2(2) != 2)
        return ERR_WRONG;
    if (ceil2(3) != 4)
        return ERR_WRONG;
    if (ceil2(16) != 16)
        return ERR_WRONG;
    if (ceil2(17) != 32)
        return ERR_WRONG;

    /* Test the pair() function. */
    if (pair(4, 0, 0) != 1)
        return ERR_WRONG;
    if (pair(4, 2, 2) != 1)
        return ERR_WRONG;
    
    return 0;
}

/* Test the function that finds an MPI type to match a PIO type. */
int test_find_mpi_type()
{
    MPI_Datatype mpi_type;
    int type_size;
    int ret;

    /* This should not work. */
    if (find_mpi_type(PIO_BYTE + 42, &mpi_type, &type_size) != PIO_EBADTYPE)
        return ERR_WRONG;

    /* Try every atomic type. */
    if ((ret = find_mpi_type(PIO_BYTE, &mpi_type, &type_size)))
        return ret;
    if (mpi_type != MPI_BYTE || type_size != 1)
        return ERR_WRONG;

    if ((ret = find_mpi_type(PIO_CHAR, &mpi_type, &type_size)))
        return ret;
    if (mpi_type != MPI_CHAR || type_size != 1)
        return ERR_WRONG;

    if ((ret = find_mpi_type(PIO_SHORT, &mpi_type, &type_size)))
        return ret;
    if (mpi_type != MPI_SHORT || type_size != 2)
        return ERR_WRONG;

    if ((ret = find_mpi_type(PIO_INT, &mpi_type, &type_size)))
        return ret;
    if (mpi_type != MPI_INT || type_size != 4)
        return ERR_WRONG;

    if ((ret = find_mpi_type(PIO_FLOAT, &mpi_type, &type_size)))
        return ret;
    if (mpi_type != MPI_FLOAT || type_size != 4)
        return ERR_WRONG;

    if ((ret = find_mpi_type(PIO_DOUBLE, &mpi_type, &type_size)))
        return ret;
    if (mpi_type != MPI_DOUBLE || type_size != 8)
        return ERR_WRONG;

    /* These should also work. */
    if ((ret = find_mpi_type(PIO_INT, &mpi_type, NULL)))
        return ret;
    if (mpi_type != MPI_INT)
        return ERR_WRONG;
    if ((ret = find_mpi_type(PIO_INT, NULL, &type_size)))
        return ret;
    if (type_size != 4)
        return ERR_WRONG;
    if ((ret = find_mpi_type(PIO_INT, NULL, NULL)))
        return ret;

#ifdef _NETCDF4
    if ((ret = find_mpi_type(PIO_UBYTE, &mpi_type, &type_size)))
        return ret;
    if (mpi_type != MPI_UNSIGNED_CHAR || type_size != 1)
        return ERR_WRONG;

    if ((ret = find_mpi_type(PIO_USHORT, &mpi_type, &type_size)))
        return ret;
    if (mpi_type != MPI_UNSIGNED_SHORT || type_size != 2)
        return ERR_WRONG;

    if ((ret = find_mpi_type(PIO_UINT, &mpi_type, &type_size)))
        return ret;
    if (mpi_type != MPI_UNSIGNED || type_size != 4)
        return ERR_WRONG;

    if ((ret = find_mpi_type(PIO_INT64, &mpi_type, &type_size)))
        return ret;
    if (mpi_type != MPI_LONG_LONG || type_size != 8)
        return ERR_WRONG;

    if ((ret = find_mpi_type(PIO_UINT64, &mpi_type, &type_size)))
        return ret;
    if (mpi_type != MPI_UNSIGNED_LONG_LONG || type_size != 8)
        return ERR_WRONG;

    if ((ret = find_mpi_type(PIO_STRING, &mpi_type, &type_size)))
        return ret;
    if (mpi_type != MPI_CHAR || type_size != 1)
        return ERR_WRONG;

#endif /* _NETCDF4 */
    return PIO_NOERR;
}

int test_misc()
{
    wmulti_buffer wmb;

    /* This should not work. */
    if (flush_buffer(TEST_VAL_42, &wmb, 0) != PIO_EBADID)
        return ERR_WRONG;

    return 0;
}

/* Test the CalcStartandCount() function for the BOX rearranger */
int test_CalcStartandCount()
{
    /*
     *  num_io_procs: the number of specified IO tasks
     *  num_aiotasks: the number of IO tasks actually used
     *
     *  [Some restrictions]
     *  1) num_aiotasks <= num_io_procs: the best case is to use all available IO tasks.
     *  2) num_aiotasks >= 1: the worse case is to use only one IO task.
     *  3) We must have minimum blocksize data (1024 bytes by default) on each used IO task (not applicable
     *     if the total size of the data is relatively small).
     *  4) For simplicity, the partition algorithm requires that num_aiotasks is continuously divisible
     *     by each outer dimension length, until the quotient is less than or equal to an inner dimension
     *     length to terminate the calculation of start and count.
     */
    int num_io_procs, num_aiotasks;

    /* For sufficiently large data, the target blocksize on each IO task should be at least 1024 bytes */
    const int minbytes = 1024;

    {
        int ndims = 2;
        int gdims[2] = {31, 777602};
        PIO_Offset start[2], count[2];
        long int tpsize = 0;
        int ret;

        long int pgdims = 1;
        for (int i = 0; i < ndims; i++)
            pgdims *= gdims[i];

        num_io_procs = 24;

        for (int iorank = 0; iorank < num_io_procs; iorank++)
        {
            if ((ret = CalcStartandCount(PIO_DOUBLE, ndims, gdims, num_io_procs, iorank,
                                         start, count, &num_aiotasks)))
                return ret;

            /* 24 < 31 */
            if (num_aiotasks != 24)
                return ERR_WRONG;

            if (iorank < num_aiotasks)
            {
                long int psize = 1;
                for (int i = 0; i < ndims; i++)
                    psize *= count[i];

                if (psize * sizeof(double) < minbytes)
                    return ERR_WRONG;

                tpsize += psize;
            }
        }

        if (tpsize != pgdims)
            return ERR_WRONG;
    }

    {
        int ndims = 2;
        int gdims[2] = {72, 777602};
        PIO_Offset start[2], count[2];
        long int tpsize = 0;
        int ret;

        long int pgdims = 1;
        for (int i = 0; i < ndims; i++)
            pgdims *= gdims[i];

        num_io_procs = 337;

        for (int iorank = 0; iorank < num_io_procs; iorank++)
        {
            if ((ret = CalcStartandCount(PIO_DOUBLE, ndims, gdims, num_io_procs, iorank,
                                         start, count, &num_aiotasks)))
                return ret;

            /* 288 / 72 = 4, 4 < 777602 */
            if (num_aiotasks != 288)
                return ERR_WRONG;

            if (iorank < num_aiotasks)
            {
                long int psize = 1;
                for (int i = 0; i < ndims; i++)
                    psize *= count[i];

                if (psize * sizeof(double) < minbytes)
                    return ERR_WRONG;

                tpsize += psize;
            }
        }

        if (tpsize != pgdims)
            return ERR_WRONG;
    }

    {
        int ndims = 3;
        int gdims[3] = {15, 360, 720};
        PIO_Offset start[3], count[3];
        long int tpsize = 0;
        int ret;

        long int pgdims = 1;
        for (int i = 0; i < ndims; i++)
            pgdims *= gdims[i];

        num_io_procs = 64;

        for (int iorank = 0; iorank < num_io_procs; iorank++)
        {
            if ((ret = CalcStartandCount(PIO_DOUBLE, ndims, gdims, num_io_procs, iorank,
                                         start, count, &num_aiotasks)))
                return ret;

            /* 60 / 15 = 4, 4 < 360 */
            if (num_aiotasks != 60)
                return ERR_WRONG;

            if (iorank < num_aiotasks)
            {
                long int psize = 1;
                for (int i = 0; i < ndims; i++)
                    psize *= count[i];

                if (psize * sizeof(double) < minbytes)
                    return ERR_WRONG;

                tpsize += psize;
            }
        }

        if (tpsize != pgdims)
            return ERR_WRONG;
    }

    {
        int ndims = 4;
        int gdims[4] = {6, 8, 20, 1024};
        PIO_Offset start[4], count[4];
        long int tpsize = 0;
        int ret;

        long int pgdims = 1;
        for (int i = 0; i < ndims; i++)
            pgdims *= gdims[i];

        num_io_procs = 256;

        for (int iorank = 0; iorank < num_io_procs; iorank++)
        {
            if ((ret = CalcStartandCount(PIO_DOUBLE, ndims, gdims, num_io_procs, iorank,
                                         start, count, &num_aiotasks)))
                return ret;

            /* 240 / 6 = 40, 40 / 8 = 5, 5 < 20 */
            if (num_aiotasks != 240)
                return ERR_WRONG;

            if (iorank < num_aiotasks)
            {
                long int psize = 1;
                for (int i = 0; i < ndims; i++)
                    psize *= count[i];

                if (psize * sizeof(double) < minbytes)
                    return ERR_WRONG;

                tpsize += psize;
            }
        }

        if (tpsize != pgdims)
            return ERR_WRONG;
    }

    return 0;
}

/* Test the GCDblocksize() function (ignoring gaps). */
int run_GCDblocksize_tests(MPI_Comm test_comm)
{
    {
        int arrlen = 1;
        PIO_Offset arr_in[1] = {0};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 1 block: [0]
         * expected result: 1 */
        if (blocksize != 1)
            return ERR_WRONG;
    }

    {
        int arrlen = 4;
        PIO_Offset arr_in[4] = {0, 1, 2, 3};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 1 block: [0 ~ 3]
         * expected result: 4 */
        if (blocksize != 4)
            return ERR_WRONG;
    }

    {
        int arrlen = 4;
        PIO_Offset arr_in[4] = {0, 2, 3, 4};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 2 blocks: [0], [2 ~ 4]
         * expected result: gcd(1, 3) = 1 */
        if (blocksize != 1)
            return ERR_WRONG;
    }

    {
        int arrlen = 4;
        PIO_Offset arr_in[4] = {0, 1, 3, 4};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 2 blocks: [0 ~ 1], [3 ~ 4]
         * expected result: gcd(2, 2) = 2 */
        if (blocksize != 2)
            return ERR_WRONG;
    }

    {
        int arrlen = 4;
        PIO_Offset arr_in[4] = {0, 1, 2, 4};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 2 blocks: [0 ~ 2], [4]
         * expected result: gcd(3, 1) = 1 */
        if (blocksize != 1)
            return ERR_WRONG;
    }

    {
        int arrlen = 4;
        PIO_Offset arr_in[4] = {0, 1, 4, 5};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 2 blocks: [0 ~ 1], [4 ~ 5]
         * expected result: gcd(2, 2) = 2 */
        if (blocksize != 2)
            return ERR_WRONG;
    }

    {
        int arrlen = 4;
        PIO_Offset arr_in[4] = {1, 2, 3, 4};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 1 block: [1 ~ 4]
         * expected result: 4 */
        if (blocksize != 4)
            return ERR_WRONG;
    }

    {
        int arrlen = 4;
        PIO_Offset arr_in[4] = {2, 3, 4, 5};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 1 block: [2 ~ 5]
         * expected result: 4 */
        if (blocksize != 4)
            return ERR_WRONG;
    }

    {
        int arrlen = 4;
        PIO_Offset arr_in[4] = {3, 2, 1, 0};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 4 blocks: [3], [2], [1], [0]
         * expected result: gcd(1, 1, 1, 1) = 1 */
        if (blocksize != 1)
            return ERR_WRONG;
    }

    {
        int arrlen = 4;
        PIO_Offset arr_in[4] = {2, 2, 2, 2};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 4 blocks: [2], [2], [2], [2]
         * expected result: gcd(1, 1, 1, 1) = 1 */
        if (blocksize != 1)
            return ERR_WRONG;
    }

    {
        int arrlen = 6;
        PIO_Offset arr_in[6] = {0, 1, 3, 2, 4, 5};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 4 blocks: [0 ~ 1], [3], [2], [4 ~ 5]
         * expected result: gcd(2, 1, 1, 2) = 1 */
        if (blocksize != 1)
            return ERR_WRONG;
    }

    {
        int arrlen = 6;
        PIO_Offset arr_in[6] = {0, 1, 4, 5, 2, 3};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 3 blocks: [0 ~ 1], [4 ~ 5], [2 ~ 3]
         * expected result: gcd(2, 2, 2) = 2 */
        if (blocksize != 2)
            return ERR_WRONG;
    }

    {
        int arrlen = 6;
        PIO_Offset arr_in[6] = {2, 3, 4, 5, 0, 1};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 2 blocks: [2 ~ 5], [0 ~ 1]
         * expected result: gcd(4, 2) = 2 */
        if (blocksize != 2)
            return ERR_WRONG;
    }

    {
        int arrlen = 6;
        PIO_Offset arr_in[6] = {3, 4, 5, 0, 1, 2};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 2 blocks: [3 ~ 5], [0 ~ 2]
         * expected result: gcd(3, 3) = 3 */
        if (blocksize != 3)
            return ERR_WRONG;
    }

    {
        int arrlen = 6;
        PIO_Offset arr_in[6] = {0, 1, 2, 3, 3, 4};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 2 blocks: [0 ~ 3], [3 ~ 4]
         * expected result: gcd(4, 2) = 2 */
        if (blocksize != 2)
            return ERR_WRONG;
    }

    {
        int arrlen = 6;
        PIO_Offset arr_in[6] = {0, 1, 1, 2, 2, 3};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 3 blocks: [0 ~ 1], [1 ~ 2], [2 ~ 3]
         * expected result: gcd(2, 2, 2) = 2 */
        if (blocksize != 2)
            return ERR_WRONG;
    }

    {
        int arrlen = 12;
        PIO_Offset arr_in[12] = {8, 9, 10, 11, 12, 13, 14, 15, 2, 3, 4, 5};
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 2 blocks: [8 ~ 15], [2 ~ 5]
         * expected result: gcd(8, 4) = 4 */
        if (blocksize != 4)
            return ERR_WRONG;
    }

    {
        int arrlen = 2422;
        PIO_Offset arr_in[2422];
        for (int i = 0; i < 2422; i++)
        {
            if (i <= 2204)
                arr_in[i] = i;
            else
                arr_in[i] = i + 2;
        }
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 2 blocks: [0 ~ 2204] [2207 ~ 2423]
         * expected result: gcd(2205, 217) = 7 */
        if (blocksize != 7)
            return ERR_WRONG;
    }

    {
        int arrlen = 2422;
        PIO_Offset arr_in[2422];
        for (int i = 0; i < 2422; i++)
        {
            if (i <= 2203)
                arr_in[i] = i;
            else
                arr_in[i] = i + 2;
        }
        PIO_Offset blocksize;

        blocksize = GCDblocksize(arrlen, arr_in);
        /* 2 blocks: [0 ~ 2203] [2206 ~ 2423]
         * expected result: gcd(2204, 218) = 2 */
        if (blocksize != 2)
            return ERR_WRONG;
    }

    return 0;
}

/* Run Tests for pio_spmd.c functions. */
int main(int argc, char **argv)
{
    int my_rank; /* Zero-based rank of processor. */
    int ntasks;  /* Number of processors involved in current execution. */
    int ret;     /* Return code. */
    MPI_Comm test_comm; /* A communicator for this test. */

    /* Initialize test. */
    if ((ret = pio_test_init2(argc, argv, &my_rank, &ntasks, MIN_NTASKS,
                              TARGET_NTASKS, 3, &test_comm)))
        ERR(ERR_INIT);

    /* Test code runs on TARGET_NTASKS tasks. The left over tasks do
     * nothing. */
    if (my_rank < TARGET_NTASKS)
    {
        /* I don't need this iosystem, but it's the only way to get
         * the logs to write. */
        int iosysid;
        if ((ret = PIOc_Init_Intracomm(test_comm, TARGET_NTASKS, 1, 0, PIO_REARR_BOX, &iosysid)))
            return ret;

        printf("%d running tests for functions in pioc_sc.c\n", my_rank);
        if ((ret = run_sc_tests(test_comm)))
             return ret;

        printf("%d running tests for GCDblocksize()\n", my_rank);
        if ((ret = run_GCDblocksize_tests(test_comm)))
            return ret;

        printf("%d running spmd test code\n", my_rank);
        if ((ret = run_spmd_tests(test_comm)))
            return ret;
        
        printf("%d running CalcStartandCount test code\n", my_rank);
        if ((ret = test_CalcStartandCount()))
            return ret;

        printf("%d running list tests\n", my_rank);
        if ((ret = test_lists()))
            return ret;

        printf("%d running ceil2/pair tests\n", my_rank);
        if ((ret = test_ceil2_pair()))
            return ret;

        printf("%d running find_mpi_type tests\n", my_rank);
        if ((ret = test_find_mpi_type()))
            return ret;

        printf("%d running misc tests\n", my_rank);
        if ((ret = test_misc()))
            return ret;

        /* Finalize PIO system. */
        if ((ret = PIOc_finalize(iosysid)))
            return ret;

    } /* endif my_rank < TARGET_NTASKS */

    /* Finalize the MPI library. */
    printf("%d %s Finalizing...\n", my_rank, TEST_NAME);
    if ((ret = pio_test_finalize(&test_comm)))
        return ret;

    printf("%d %s SUCCESS!!\n", my_rank, TEST_NAME);

    return 0;
}
