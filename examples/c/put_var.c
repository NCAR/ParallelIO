/**
 * @file 
 * @brief A simple C example for the ParallelIO Library using PIOc_put_vara()
 *
 * This example creates a netCDF output file with one dimension and
 * one variable. It first writes, then reads the sample file using the
 * ParallelIO library. 
 *
 * This example can be run in parallel for 1, 2, 4, 8, or 16
 * processors.
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include <pio.h>
#ifdef TIMING
#include <gptl.h>
#endif

/** The number of possible output netCDF output flavors available to
 * the ParallelIO library. */
#define NUM_NETCDF_FLAVORS 5

/** The number of dimensions in the example data. In this simple
    example, we are using one-dimensional data. */
#define NDIM 2

/** The length of our sample data. There will be a total of 16x16
 * integers in our data, and responsibility for writing and reading
 * them will be spread between all the processors used to run this
 * example. */
#define DIM_LEN_X 16
#define DIM_LEN_Y 5

/** The name of the dimension in the netCDF output file. */
#define DIM_NAME_X "x"
#define DIM_NAME_Y "y"

/** The name of the variables in the netCDF output file. */
#define VAR_NAME_D0 "d0"
#define VAR_NAME_D1 "d1"
#define VAR_NAME_D2 "d2"

/** Return code when netCDF output file does not match
 * expectations. */
#define ERR_BAD 1001

/** The meaning of life, the universe, and everything. */
const int START_DATA_VAL = 42;

/** Handle MPI errors. This should only be used with MPI library
 * function calls. */
#define MPIERR(e) do { \
    MPI_Error_string(e, err_buffer, &resultlen); \
    printf("MPI error, line %d, file %s: %s\n", __LINE__, __FILE__, err_buffer); \
    MPI_Finalize(); \
    return 2; \
} while (0)

/** Handle non-MPI errors by finalizing the MPI library and exiting
 * with an exit code. */
#define ERR(e) do { \
    MPI_Finalize(); \
    return e; \
} while (0)

/** Global err buffer for MPI. When there is an MPI error, this buffer
 * is used to store the error message that is associated with the MPI
 * error. */
char err_buffer[MPI_MAX_ERROR_STRING];

/** This is the length of the most recent MPI error message, stored
 * int the global error string. */
int resultlen;

/** @brief Main execution of code.

    Executes the functions to:
    - create a new examplePioClass instance
    - initialize MPI and the ParallelIO libraries
    - create the decomposition for this example
    - create the netCDF output file
    - define the variable in the file
    - write data to the variable in the file using decomposition
    - read the data back from the file using decomposition
    - close the file
    - clean up resources

    The example can be run from the command line (on system that support it) like this:
    <pre>
    mpiexec -n 3 ./put_var
    </pre>

    The sample file created by this program is a small netCDF file. It
    has the following contents (as shown by ncdump) for a 4-processor
    run:

    <pre>
    netcdf put_var_0 {
dimensions:
        x = 16 ;
        y = 16 ;
variables:
        int d0 ;
        int d1(x) ;
        int d2(x,y) ;
data:

 d0 = 42 ;

 d1 = 42, 42, 42, 42, 42, 42, 43, 43, 43, 43, 43, 44, 44, 44, 44, 44 ;

 d2 =
  42, 42, 42, 42, 42,
  42, 42, 42, 42, 42,
  42, 42, 42, 42, 42,
  42, 42, 42, 42, 42,
  42, 42, 42, 42, 42,
  42, 42, 42, 42, 42,
  43, 43, 43, 43, 43,
  43, 43, 43, 43, 43,
  43, 43, 43, 43, 43,
  43, 43, 43, 43, 43,
  43, 43, 43, 43, 43,
  44, 44, 44, 44, 44,
  44, 44, 44, 44, 44,
  44, 44, 44, 44, 44,
  44, 44, 44, 44, 44,
  44, 44, 44, 44, 44 ;

    }
    </pre>
    
    @param [in] argc argument count (should be zero)
    @param [in] argv argument array (should be NULL)
    @retval examplePioClass* Pointer to self.
*/
int main(int argc, char* argv[])
{
    /** Set to non-zero to get output to stdout. */
    int verbose = 0;

    /** Zero-based rank of processor. */
    int my_rank;

    /** Number of processors involved in current execution. */
    int ntasks;

    /** Different output flavors. The example file is written (and
     * then read) four times. The first two flavors,
     * parallel-netcdf, and netCDF serial, both produce a netCDF
     * classic format file (but with different libraries). The
     * last two produce netCDF4/HDF5 format files, written with
     * and without using netCDF-4 parallel I/O. */
    int format[NUM_NETCDF_FLAVORS];

    /** Number of processors that will do IO. In this example we
     * will do IO from all processors. */
    int niotasks;

    /** Stride in the mpi rank between io tasks. Always 1 in this
     * example. */
    int ioproc_stride = 1;

    /** Zero based rank of first processor to be used for I/O. */
    int ioproc_start = 0;

    /** The dimension ID. */
    int dimids[2];

    /** Array index per processing unit. This is the number of
     * elements of the data array that will be handled by each
     * processor. In this example there are 16 data elements. If the
     * example is run on 4 processors, then arrIdxPerPe will be 4. */
    PIO_Offset elements_per_pe;

    /** Start/Count in two dimension for this process' output
     *  in a global array */
    PIO_Offset start[2], count[2];

    /* Length of the dimensions in the data. This example
     * uses two-dimensional data. The length
     * is DIM_LEN_X (16) by DIM_LEN_Y */
    int dim_len[2] = {DIM_LEN_X, DIM_LEN_Y};

    /** The ID for the parallel I/O system. It is set by
     * PIOc_Init_Intracomm(). It references an internal structure
     * containing the general IO subsystem data and MPI
     * structure. It is passed to PIOc_finalize() to free
     * associated resources, after all I/O, but before
     * MPI_Finalize is called. */
    int iosysid;

    /** The ncid of the netCDF file created in this example. */
    int ncid;

    /** The IDs of the netCDF variables in the example file. */
    int varidd0, varidd1, varidd2;

    /** A buffer for sample data. The size of this array will
     * vary depending on how many processors are involved in the
     * execution of the example code. It's length will be the same
     * as elements_per_pe.*/
    int *buffer;

    /** Test filename. */
    char filename[NC_MAX_NAME + 1];

    /** The number of netCDF flavors available in this build. */
    int num_flavors = 0;

    /** Used for command line processing. */
    int c;

    /** Return value. */
    int ret;

    /* Parse command line. */
    while ((c = getopt(argc, argv, "v")) != -1)
    {
        switch (c)
        {
        case 'v':
            verbose++;
            break;
        default:
            break;
        }
    }

#ifdef TIMING
#ifndef TIMING_INTERNAL
    /* Initialize the GPTL timing library. */
    if ((ret = GPTLinitialize()))
        return ret;
#endif
#endif

    /* Initialize MPI. */
    if ((ret = MPI_Init(&argc, &argv)))
        MPIERR(ret);

    if ((ret = MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN)))
        MPIERR(ret);

    /* Learn my rank and the total number of processors. */
    if ((ret = MPI_Comm_rank(MPI_COMM_WORLD, &my_rank)))
        MPIERR(ret);

    if ((ret = MPI_Comm_size(MPI_COMM_WORLD, &ntasks)))
        MPIERR(ret);

    /* Check that a valid number of processors was specified. */
    if (ntasks > 16)
        fprintf(stderr, "Number of processors must be max 16!\n");

    if (verbose)
        printf("%d: ParallelIO Library put_var running on %d processors.\n",
               my_rank, ntasks);

    /* Keep things simple - 1 iotask per MPI process */
    niotasks = ntasks;

    /* Initialize the PIO IO system. This specifies how
     * many and which processors are involved in I/O. */
    if ((ret = PIOc_Init_Intracomm(MPI_COMM_WORLD, niotasks, ioproc_stride,
                                   ioproc_start, PIO_REARR_SUBSET, &iosysid)))
        ERR(ret);

    elements_per_pe = DIM_LEN_X / ntasks;
    if (my_rank < DIM_LEN_X % ntasks)
    {
        ++elements_per_pe;
        start[0] = my_rank * elements_per_pe;
    }
    else
    {
        start[0] = my_rank * elements_per_pe + DIM_LEN_X % ntasks;
    }

    start[1] = 0;
    count[0] = elements_per_pe;
    count[1] = DIM_LEN_Y;

    /* The number of favors may change with the build parameters. */
#ifdef _PNETCDF
    format[num_flavors++] = PIO_IOTYPE_PNETCDF;
#endif
#ifdef _NETCDF4
    format[num_flavors++] = PIO_IOTYPE_NETCDF4P;
#endif
#ifdef _ADIOS2
    format[num_flavors++] = PIO_IOTYPE_ADIOS;
#endif

    /* Use PIO to create the example file in each of the five
     * available ways. */
    for (int fmt = 0; fmt < num_flavors; fmt++)
    {
        /* Create a filename. */
        sprintf(filename, "put_var_%d.nc", fmt);

        if (format[fmt] == PIO_IOTYPE_ADIOS)
            sprintf(filename, "put_var_%d.bp", fmt);

        /* Create the netCDF output file. */
        if (verbose)
            printf("rank: %d Creating sample file %s with format %d...\n",
                   my_rank, filename, format[fmt]);

        if ((ret = PIOc_createfile(iosysid, &ncid, &(format[fmt]), filename, PIO_CLOBBER)))
            ERR(ret);

        /* Define netCDF dimension and variable. */
        if (verbose)
            printf("rank: %d Defining netCDF metadata...\n", my_rank);

        if ((ret = PIOc_def_dim(ncid, DIM_NAME_X, (PIO_Offset)dim_len[0], &(dimids[0]))))
            ERR(ret);

        if ((ret = PIOc_def_dim(ncid, DIM_NAME_Y, (PIO_Offset)dim_len[1], &(dimids[1]))))
            ERR(ret);

        if ((ret = PIOc_def_var(ncid, VAR_NAME_D0, PIO_INT, 0, NULL, &varidd0)))
            ERR(ret);

        if ((ret = PIOc_def_var(ncid, VAR_NAME_D1, PIO_INT, 1, dimids, &varidd1)))
            ERR(ret);

        if ((ret = PIOc_def_var(ncid, VAR_NAME_D2, PIO_INT, 2, dimids, &varidd2)))
            ERR(ret);

        if ((ret = PIOc_enddef(ncid)))
            ERR(ret);

        /* Prepare sample data. */
        int nelems = elements_per_pe * DIM_LEN_Y;

        if (!(buffer = malloc( nelems * sizeof(int))))
            return PIO_ENOMEM;

        for (int i = 0; i < nelems; i++)
            buffer[i] = START_DATA_VAL + my_rank;

        /* Write data to the file. */
        if (verbose)
            printf("rank: %d Writing sample data...\n", my_rank);

        if ((ret = PIOc_put_var_int(ncid, varidd0, &(START_DATA_VAL)) ))
            ERR(ret);

        if ((ret = PIOc_put_vara_int(ncid, varidd1, start, count, buffer)))
            ERR(ret);

        if ((ret = PIOc_put_vara_int(ncid, varidd2, start, count, buffer)))
            ERR(ret);

        if ((ret = PIOc_sync(ncid)))
            ERR(ret);

        /* Free buffer space used in this example. */
        free(buffer);

        /* Close the netCDF file. */
        if (verbose)
            printf("rank: %d Closing the sample data file...\n", my_rank);

        if ((ret = PIOc_closefile(ncid)))
            ERR(ret);
    }

    /* Finalize the IO system. */
    if (verbose)
        printf("rank: %d Freeing PIO resources...\n", my_rank);

    if ((ret = PIOc_finalize(iosysid)))
        ERR(ret);

    /* Finalize the MPI library. */
    MPI_Finalize();

#ifdef TIMING
#ifndef TIMING_INTERNAL
    /* Finalize the GPTL timing library. */
    if ((ret = GPTLfinalize()))
        return ret;
#endif
#endif

    if (verbose)
        printf("rank: %d SUCCESS!\n", my_rank);

    return 0;
}
