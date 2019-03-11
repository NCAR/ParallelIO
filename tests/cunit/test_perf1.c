/*
 * Tests for PIO distributed arrays.
 *
 * @author Ed Hartnett
 * @date 2/16/17
 */
#include <config.h>
#include <pio.h>
#include <pio_internal.h>
#include <pio_tests.h>
#include <sys/time.h>

/* The number of tasks this test should run on. */
#define TARGET_NTASKS 4

/* The minimum number of tasks this test should run on. */
#define MIN_NTASKS 4

/* The name of this test. */
#define TEST_NAME "test_perf1"

/* Number of processors that will do IO. */
#define NUM_IO_PROCS 1

/* Number of computational components to create. */
#define COMPONENT_COUNT 1

/* Ranks of different arrays. */
#define NDIM2 2
#define NDIM3 3
#define NDIM4 4

/* The length of our sample data along each dimension. */
#define X_DIM_LEN 16
#define Y_DIM_LEN 16
#define Z_DIM_LEN 4

/* The number of timesteps of data to write. */
#define NUM_TIMESTEPS 1

/* The number of 4D vars. */
#define NUM_VARS 1

/* The names of variables in the netCDF output files. */
#define VAR_NAME "Billy-Bob"
#define VAR_NAME2 "Sally-Sue"

/* Test cases relating to PIOc_write_darray_multi(). */
#define NUM_TEST_CASES_WRT_MULTI 3

/* The dimension names. */
char dim_name[NDIM4][PIO_MAX_NAME + 1] = {"timestep", "x", "y", "z"};

/* Length of the dimensions in the sample data. */
int dim_len[NDIM4] = {NC_UNLIMITED, X_DIM_LEN, Y_DIM_LEN, Z_DIM_LEN};

/* Test with two rearrangers. */
#define NUM_REARRANGERS_TO_TEST 2

/* Test with several types. */
#define NUM_TYPES_TO_TEST 3

/* Create the decomposition to divide the 4-dimensional sample data
 * between tasks. For the purposes of decomposition we are only
 * concerned with 3 dimensions - we ignore the unlimited dimension.
 *
 * @param ntasks the number of available tasks
 * @param my_rank rank of this task.
 * @param iosysid the IO system ID.
 * @param dim_len_3d an array of length 3 with the dim lengths.
 * @param ioid a pointer that gets the ID of this decomposition.
 * @param pio_type the data type to use for the decomposition.
 * @returns 0 for success, error code otherwise.
 **/
int create_decomposition_3d(int ntasks, int my_rank, int iosysid, int *dim_len_3d,
                            int *ioid, int pio_type)
{
    PIO_Offset elements_per_pe;     /* Array elements per processing unit. */
    PIO_Offset *compdof;  /* The decomposition mapping. */
    int ret;

    /* How many data elements per task? */
    elements_per_pe = dim_len_3d[0] * dim_len_3d[1] * dim_len_3d[2]/ ntasks;

    /* Allocate space for the decomposition array. */
    if (!(compdof = malloc(elements_per_pe * sizeof(PIO_Offset))))
        return PIO_ENOMEM;

    /* Describe the decomposition. This is a 1-based array, so add 1! */
    for (int i = 0; i < elements_per_pe; i++)
        compdof[i] = my_rank * elements_per_pe + i + 1;

    /* Create the PIO decomposition for this test. */
    if ((ret = PIOc_InitDecomp(iosysid, pio_type, NDIM3, dim_len_3d, elements_per_pe,
                               compdof, ioid, NULL, NULL, NULL)))
        ERR(ret);


    /* Free the mapping. */
    free(compdof);

    return 0;
}


/**
 * Do some fake computation.
 *
 * @return 0 always.
 */
int
do_some_computation(long long int max_i)
{
    for (int i = 0; i < max_i; i++)
    {
        float a, b = 11.1, c = -33333.33;
        a = b * c;
        b = a * c;
        c = a * b;
    }
    return 0;
}

/**
 * Test the darray functionality. Create a netCDF file with 4
 * dimensions and some variables, and use darray to write some data.
 *
 * @param iosysid the IO system ID.
 * @param ioid the ID of the decomposition.
 * @param num_flavors the number of IOTYPES available in this build.
 * @param flavor array of available iotypes.
 * @param my_rank rank of this task.
 * @param pio_type the type of the data.
 * @param fmt indicates the IOtype.
 * @param test_multi true for testing muti-var darray write.
 * @param rearranger the rearranger
 * @returns 0 for success, error code otherwise.
 */
int test_perf1(int iosysid, int ioid, int num_flavors, int *flavor, int my_rank,
               int ntasks, int pio_type, int fmt, int test_multi, int rearranger)
{
    char filename[PIO_MAX_NAME + 1]; /* Name for the output files. */
    int dimids[NDIM4];      /* The dimension IDs. */
    int ncid;      /* The ncid of the netCDF file. */
    int varid[NUM_VARS];     /* The ID of the netCDF varable. */
    int varid2;    /* The ID of a varable of different type. */
    int wrong_varid = TEST_VAL_42;  /* A wrong ID. */
    int ret;       /* Return code. */
    PIO_Offset arraylen = X_DIM_LEN * Y_DIM_LEN * Z_DIM_LEN / ntasks;
    struct timeval starttime, endtime;
    long long startt, endt;
    long long delta;
    void *test_data;
    int test_data_int[arraylen];
    float test_data_float[arraylen];
    double test_data_double[arraylen];

    /* Initialize some data. */
    for (int f = 0; f < arraylen; f++)
    {
        test_data_int[f] = my_rank * 10 + f;
        test_data_float[f] = my_rank * 10 + f + 0.5;
        test_data_double[f] = my_rank * 100000 + f + 0.5;
    }

    /* Create the filename. */
    sprintf(filename, "data_%s_iotype_%d_pio_type_%d_test_multi_%d.nc",
            TEST_NAME, flavor[fmt], pio_type, test_multi);

    /* Select the fill value and data. */
    switch (pio_type)
    {
    case PIO_INT:
        test_data = test_data_int;
        break;
    case PIO_FLOAT:
        test_data = test_data_float;
        break;
    case PIO_DOUBLE:
        test_data = test_data_double;
        break;
    default:
        ERR(ERR_WRONG);
    }

    /* Create the netCDF output file. */
    {
        if ((ret = PIOc_createfile(iosysid, &ncid, &flavor[fmt], filename, PIO_CLOBBER)))
            ERR(ret);

        /* Define netCDF dimensions. */
        for (int d = 0; d < NDIM4; d++)
            if ((ret = PIOc_def_dim(ncid, dim_name[d], (PIO_Offset)dim_len[d], &dimids[d])))
                ERR(ret);

        /* Define the variables. */
        for (int v = 0; v < NUM_VARS; v++)
        {
            char var_name[NC_MAX_NAME + 1];
            sprintf(var_name, "var_%d", v);
            if ((ret = PIOc_def_var(ncid, var_name, pio_type, NDIM4, dimids, &varid[v])))
                ERR(ret);
        }

        /* End define mode. */
        if ((ret = PIOc_enddef(ncid)))
            ERR(ret);
    }

    /* Start the clock. */
    gettimeofday(&starttime, NULL);

    for (int t = 0; t < NUM_TIMESTEPS; t++)
    {

        /* Do some fake computation. */
        if ((ret = do_some_computation(100000)))
            ERR(ret);

        /* Write a timestep of data in each var. */
        for (int v = 0; v < NUM_VARS; v++)
        {
            /* Set the value of the record dimension. */
            if ((ret = PIOc_setframe(ncid, varid[v], t)))
                ERR(ret);

            if (!test_multi)
            {
                /* Write the data. */
                if ((ret = PIOc_write_darray(ncid, varid[v], ioid, arraylen, test_data, NULL)))
                    ERR(ret);
            }
        }
    }

    /* Close the netCDF file. */
    if ((ret = PIOc_closefile(ncid)))
        ERR(ret);

    /* Stop the clock. */
    gettimeofday(&endtime, NULL);

    /* Compute the time delta */
    startt = (1000000 * starttime.tv_sec) + starttime.tv_usec;
    endt = (1000000 * endtime.tv_sec) + endtime.tv_usec;
    delta = (endt - startt)/NUM_TIMESTEPS;
    if (!my_rank)
        printf("%d\t%d\t%d\t%d\t%lld\n", rearranger, fmt, pio_type, test_multi,
               delta);

    return PIO_NOERR;
}

/**
 * Run a performance benchmark.
 *
 * @param iosysid the IO system ID.
 * @param num_flavors number of available iotypes in the build.
 * @param flavor pointer to array of the available iotypes.
 * @param my_rank rank of this task.
 * @param test_comm the communicator the test is running on.
 * @param rearranger the rearranger
 * @returns 0 for success, error code otherwise.
 */
int run_benchmark(int iosysid, int num_flavors, int *flavor, int my_rank, int ntasks,
                  MPI_Comm test_comm, int rearranger, int num_types, int *pio_type)
{
    int ioid, ioid3;
    char filename[NC_MAX_NAME + 1];
    int dim_len_3d[NDIM3] = {Z_DIM_LEN, X_DIM_LEN, Y_DIM_LEN};
    long long delta;

    /* for (int t = 0; t < num_types; t++) */
    for (int t = 0; t < 1; t++)
    {
        float time;
        int ret; /* Return code. */

        /* This will be our file name for writing out decompositions. */
        sprintf(filename, "%s_decomp_rank_%d_flavor_%d_type_%d.nc", TEST_NAME, my_rank,
                *flavor, pio_type[t]);

        /* Decompose the data over the tasks. */
        if ((ret = create_decomposition_3d(TARGET_NTASKS, my_rank, iosysid, dim_len_3d,
                                           &ioid3, pio_type[t])))
            return ret;

        /* Run a simple performance test. */
        /* for (int fmt = 0; fmt < num_flavors; fmt++) */
        for (int fmt = 0; fmt < 2; fmt++)
        {
            /* for (int test_multi = 0; test_multi < NUM_TEST_CASES_WRT_MULTI; test_multi++) */
            for (int test_multi = 0; test_multi < 1; test_multi++)
            {
                if ((ret = test_perf1(iosysid, ioid3, num_flavors, flavor, my_rank, ntasks,
                                      pio_type[t], fmt, test_multi, rearranger)))
                    return ret;
            }
        }

        /* Free the PIO decomposition. */
        if ((ret = PIOc_freedecomp(iosysid, ioid3)))
            ERR(ret);
    }

    return PIO_NOERR;
}

int
run_some_benchmarks(MPI_Comm test_comm, int my_rank, int ntasks, int num_flavors, int *flavor,
                    int num_rearr, int *rearranger, int num_types, int *pio_type)
{
    /* Only do something on max_ntasks tasks. */
    if (my_rank < TARGET_NTASKS)
    {
        int iosysid;  /* The ID for the parallel I/O system. */
        int ioproc_stride = 1;    /* Stride in the mpi rank between io tasks. */
        int ioproc_start = 0;     /* Zero based rank of first processor to be used for I/O. */

        if (!my_rank)
            printf("rearr\tfmt\tpio_type\ttest_multi\ttime\n");
        /* for (int r = 0; r < num_rearr; r++) */
        for (int r = 0; r < 1; r++)
        {
            int ret;      /* Return code. */

            /* Initialize the PIO IO system. This specifies how
             * many and which processors are involved in I/O. */
            if ((ret = PIOc_Init_Intracomm(test_comm, TARGET_NTASKS, ioproc_stride,
                                           ioproc_start, rearranger[r], &iosysid)))
                return ret;

            /* Run tests. */
            if ((ret = run_benchmark(iosysid, num_flavors, flavor, my_rank, ntasks,
                                     test_comm, rearranger[r], num_types, pio_type)))
                return ret;

            /* Finalize PIO system. */
            if ((ret = PIOc_finalize(iosysid)))
                return ret;
        } /* next rearranger */
    } /* endif my_rank < TARGET_NTASKS */

    return PIO_NOERR;
}

/* Run benchmarks. */
int main(int argc, char **argv)
{
    int rearranger[NUM_REARRANGERS_TO_TEST] = {PIO_REARR_BOX, PIO_REARR_SUBSET};
    int my_rank;
    int ntasks;
    int num_flavors; /* Number of PIO netCDF flavors in this build. */
    int flavor[NUM_FLAVORS]; /* iotypes for the supported netCDF IO flavors. */
    int pio_type[NUM_TYPES_TO_TEST] = {PIO_INT, PIO_FLOAT, PIO_DOUBLE};
    MPI_Comm test_comm; /* A communicator for this test. */
    int ret;         /* Return code. */

    /* Initialize test. */
    if ((ret = pio_test_init2(argc, argv, &my_rank, &ntasks, MIN_NTASKS,
                              MIN_NTASKS, 3, &test_comm)))
        ERR(ERR_INIT);

    if ((ret = PIOc_set_iosystem_error_handling(PIO_DEFAULT, PIO_RETURN_ERROR, NULL)))
        return ret;

    /* Figure out iotypes. */
    if ((ret = get_iotypes(&num_flavors, flavor)))
        ERR(ret);

    /* Run a benchmark. */
    if ((ret = run_some_benchmarks(test_comm, my_rank, ntasks, num_flavors, flavor, NUM_REARRANGERS_TO_TEST,
                                   rearranger, NUM_TYPES_TO_TEST, pio_type)))
        ERR(ret);

    /* Finalize the MPI library. */
    if ((ret = pio_test_finalize(&test_comm)))
        return ret;

    printf("%d %s SUCCESS!!\n", my_rank, TEST_NAME);
    return 0;
}
