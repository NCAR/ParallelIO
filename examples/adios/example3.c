/**
 * @file 
 * @brief A simple C example for the ParallelIO Library.
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
#define NDIM 1

/** The length of our sample data. There will be a total of 16
 * integers in our data, and responsibility for writing and reading
 * them will be spread between all the processors used to run this
 * example. */
#define DIM_LEN_FOO 64 
#define DIM_LEN_BAR (2*DIM_LEN_FOO)

/** The name of the dimension in the netCDF output file. */
#define DIM_NAME_FOO "x"
#define DIM_NAME_BAR "y"

/** The name of the variable in the netCDF output file. */
#define VAR_NAME_FOO "foo"
#define VAR_NAME_FOO2 "foo2"
#define VAR_NAME_BAR "bar"

/** Return code when netCDF output file does not match
 * expectations. */
#define ERR_BAD 1001

/** The meaning of life, the universe, and everything. */
#define START_DATA_VAL 42

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

/** @brief Check the output file.
 *
 *  Use netCDF to check that the output is as expected.
 *
 * @param ntasks The number of processors running the example.
 * @param filename The name of the example file to check.
 *
 * @return 0 if example file is correct, non-zero otherwise. */
int check_file(int ntasks, char *filename)
{
#ifdef _NETCDF
    int ncid;         /**< File ID from netCDF. */
    int ndims;        /**< Number of dimensions. */
    int nvars;        /**< Number of variables. */
    int ngatts;       /**< Number of global attributes. */
    int unlimdimid;   /**< ID of unlimited dimension. */
    size_t dimlen;    /**< Length of the dimension. */
    int natts;        /**< Number of variable attributes. */
    nc_type xtype;    /**< NetCDF data type of this variable. */
    int ret;          /**< Return code for function calls. */
    int dimids[NDIM]; /**< Dimension ids for this variable. */
    char dim_name[NC_MAX_NAME];   /**< Name of the dimension. */
    char var_name[NC_MAX_NAME];   /**< Name of the variable. */
    size_t start[NDIM];           /**< Zero-based index to start read. */
    size_t count[NDIM];           /**< Number of elements to read. */
    int buffer[DIM_LEN_FOO];          /**< Buffer to read in data. */
    int expected[DIM_LEN_FOO];        /**< Data values we expect to find. */

    /* Open the file. */
    if ((ret = nc_open(filename, 0, &ncid)))
        return ret;

    /* Check the metadata. */
    if ((ret = nc_inq(ncid, &ndims, &nvars, &ngatts, &unlimdimid)))
        return ret;

    if (ndims != 3 || nvars != 4 || ngatts != 0 || unlimdimid != -1)
        return ERR_BAD;

    if ((ret = nc_inq_dim(ncid, 0, dim_name, &dimlen)))
        return ret;

    if (dimlen != DIM_LEN_FOO || strcmp(dim_name, DIM_NAME_FOO))
        return ERR_BAD;

    if ((ret = nc_inq_var(ncid, 0, var_name, &xtype, &ndims, dimids, &natts)))
        return ret;

    if (xtype != NC_INT || ndims != NDIM || dimids[0] != 0 || natts != 0)
        return ERR_BAD;

    /* Use the number of processors to figure out what the data in the
     * file should look like. */
    int div = DIM_LEN_FOO / ntasks;
    for (int d = 0; d < DIM_LEN_FOO; d++)
        expected[d] = START_DATA_VAL + d/div;
    
    /* Check the data. */
    start[0] = 0;
    count[0] = DIM_LEN_FOO;
    if ((ret = nc_get_vara(ncid, 0, start, count, buffer)))
        return ret;

    for (int d = 0; d < DIM_LEN_FOO; d++)
    {
        if (buffer[d] != expected[d])
            return ERR_BAD;
    }

    /* Close the file. */
    if ((ret = nc_close(ncid)))
        return ret;
#endif

    /* Everything looks good! */
    return 0;
}

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
    int dimid_foo;
    int dimid_bar;
    int dimid_varname;

    /** Array index per processing unit. This is the number of
     * elements of the data array that will be handled by each
     * processor. In this example there are 16 data elements. If the
     * example is run on 4 processors, then arrIdxPerPe will be 4. */
    PIO_Offset elements_per_pe_foo;
    PIO_Offset elements_per_pe_bar;

    /* Length of the dimensions in the data. This simple example
     * uses one-dimensional data. The length along that dimension
     * is DIM_LEN (16). */
    int dim_len_foo[1] = {DIM_LEN_FOO};
    int dim_len_bar[1] = {DIM_LEN_BAR};

    /** The ID for the parallel I/O system. It is set by
     * PIOc_Init_Intracomm(). It references an internal structure
     * containing the general IO subsystem data and MPI
     * structure. It is passed to PIOc_finalize() to free
     * associated resources, after all I/O, but before
     * MPI_Finalize is called. */
    int iosysid;

    /** The ncid of the netCDF file created in this example. */
    int ncid;

    /** The ID of the netCDF variable in the example file. */
    int varid_foo;
    int varid_foo2;
    int varid_bar;
    int varid_varname; /* varid of another variable which is a string */

    /** The I/O description ID as passed back by PIOc_InitDecomp()
     * and freed in PIOc_freedecomp(). */
    int ioid_foo;
    int ioid_bar;

    /** A buffer for sample data. The size of this array will
     * vary depending on how many processors are involved in the
     * execution of the example code. It's length will be the same
     * as elements_per_pe.*/
    int *buffer_foo;
    float *buffer_bar;

    /** A 1-D array which holds the decomposition mapping for this
     * example. The size of this array will vary depending on how
     * many processors are involved in the execution of the
     * example code. It's length will be the same as
     * elements_per_pe. */
    PIO_Offset *compdof;

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
    /* Initialize the GPTL timing library. */
    if ((ret = GPTLinitialize ()))
        return ret;
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
    if (!(ntasks == 1 || ntasks == 2 || ntasks == 4 ||
          ntasks == 8 || ntasks == 16))
        fprintf(stderr, "Number of processors must be 1, 2, 4, 8, or 16!\n");

    if (verbose)
        printf("%d: ParallelIO Library example3 running on %d processors.\n",
               my_rank, ntasks);

    /* Keep things simple - 1 iotask per MPI process */
    niotasks = ntasks;

    PIOc_set_log_level(2);

    /* Initialize the PIO IO system. This specifies how
     * many and which processors are involved in I/O. */
    if ((ret = PIOc_Init_Intracomm(MPI_COMM_WORLD, niotasks, ioproc_stride,
                                   ioproc_start, PIO_REARR_SUBSET, &iosysid)))
        ERR(ret);

    /* Describe the 'foo' decomposition. This is a 1-based array, so add 1! */
    elements_per_pe_foo = DIM_LEN_FOO / ntasks;
    if (!(compdof = malloc(elements_per_pe_foo * sizeof(PIO_Offset))))
        return PIO_ENOMEM;

    for (int i = 0; i < elements_per_pe_foo; i++)
        compdof[i] = my_rank * elements_per_pe_foo + i + 1;

    /* Create the PIO decomposition for this example. */
    if (verbose)
        printf("rank: %d Creating decomposition for foo...\n", my_rank);

    if ((ret = PIOc_InitDecomp(iosysid, PIO_INT, NDIM, dim_len_foo, (PIO_Offset)elements_per_pe_foo,
                               compdof, &ioid_foo, NULL, NULL, NULL)))
        ERR(ret);

    free(compdof);

    /* Describe the 'bar' decomposition which is twice as big as 'foo'. This is a 1-based array, so add 1! */
    elements_per_pe_bar = DIM_LEN_BAR / ntasks;
    if (!(compdof = malloc(elements_per_pe_bar * sizeof(PIO_Offset))))
        return PIO_ENOMEM;

    for (int i = 0; i < elements_per_pe_bar; i++)
        compdof[i] = my_rank * elements_per_pe_bar + i + 1;

    /* Create the PIO decomposition for this example. */
    if (verbose)
        printf("rank: %d Creating decomposition for bar...\n", my_rank);

    if ((ret = PIOc_InitDecomp(iosysid, PIO_FLOAT, NDIM, dim_len_bar, (PIO_Offset)elements_per_pe_bar,
                               compdof, &ioid_bar, NULL, NULL, NULL)))
        ERR(ret);

    free(compdof);

    /* The number of favors may change with the build parameters. */
#ifdef _PNETCDF
    format[num_flavors++] = PIO_IOTYPE_PNETCDF;
#endif
#ifdef _NETCDF
    format[num_flavors++] = PIO_IOTYPE_NETCDF;
#endif
#ifdef _NETCDF4
    format[num_flavors++] = PIO_IOTYPE_NETCDF4C;
    format[num_flavors++] = PIO_IOTYPE_NETCDF4P;
#endif
#ifdef _ADIOS2
    format[num_flavors++] = PIO_IOTYPE_ADIOS;
#endif

    /* Use PIO to create the example file in each of the four
     * available ways. */
    for (int fmt = 0; fmt < num_flavors; fmt++)
    {
        /* Create a filename. */
        sprintf(filename, "example3_%d.nc", fmt);

        /* Create the netCDF output file. */
        if (verbose)
            printf("rank: %d Creating sample file %s with format %d...\n",
                   my_rank, filename, format[fmt]);

        if ((ret = PIOc_createfile(iosysid, &ncid, &(format[fmt]), filename, PIO_CLOBBER)))
            ERR(ret);

        /* Define netCDF dimension and variable. */
        if (verbose)
            printf("rank: %d Defining netCDF metadata...\n", my_rank);

        if ((ret = PIOc_def_dim(ncid, DIM_NAME_FOO, (PIO_Offset)dim_len_foo[0], &dimid_foo)))
            ERR(ret);

        if ((ret = PIOc_def_var(ncid, VAR_NAME_FOO, PIO_INT, NDIM, &dimid_foo, &varid_foo)))
            ERR(ret);

        if ((ret = PIOc_def_var(ncid, VAR_NAME_FOO2, PIO_INT, NDIM, &dimid_foo, &varid_foo2)))
            ERR(ret);

        if ((ret = PIOc_def_dim(ncid, DIM_NAME_BAR, (PIO_Offset)dim_len_bar[0], &dimid_bar)))
            ERR(ret);

        if ((ret = PIOc_def_var(ncid, VAR_NAME_BAR, PIO_FLOAT, NDIM, &dimid_bar, &varid_bar)))
            ERR(ret);

        if ((ret = PIOc_def_dim(ncid, "varname_len", strlen(VAR_NAME_FOO), &dimid_varname)))
            ERR(ret);

        if ((ret = PIOc_def_var(ncid, "varname", PIO_CHAR, 1, &dimid_varname, &varid_varname)))
            ERR(ret);

        if ((ret = PIOc_enddef(ncid)))
            ERR(ret);

        /* Prepare sample data. */
        if (!(buffer_foo = (int *) malloc(elements_per_pe_foo * sizeof(int))))
            return PIO_ENOMEM;

        for (int i = 0; i < elements_per_pe_foo; i++)
            buffer_foo[i] = START_DATA_VAL + my_rank;

        if (!(buffer_bar = (float *) calloc(elements_per_pe_bar, sizeof(float))))
            return PIO_ENOMEM;

        for (int i = 0; i < elements_per_pe_bar; i++)
            buffer_bar[i] = (float) START_DATA_VAL*1.0 + my_rank*1.0;

        /* Some tests */
        int test_varid = -1;
        PIOc_inq_varid(ncid, VAR_NAME_FOO, &test_varid);
        if (varid_foo != test_varid)
        {
            printf("rank: %d PIOc_inq_varid(%s) returned wrong varid=%d, expected=%d\n",
                   my_rank, VAR_NAME_FOO, test_varid, varid_foo);
        }

        int test_dimid = -1;
        PIOc_inq_dimid(ncid, DIM_NAME_FOO, &test_dimid);
        if (dimid_foo != test_dimid) {
            printf("rank: %d PIOc_inq_dimid(%s) returned wrong dimid=%d, expected=%d\n",
                   my_rank, DIM_NAME_FOO, test_dimid, dimid_foo);
        }

        char test_dimname[] = "wrongdimname";
        PIOc_inq_dimname(ncid, dimid_bar, test_dimname);
        if (strcmp(DIM_NAME_BAR, test_dimname))
        {
            printf("rank: %d PIOc_inq_dimname(%d) returned wrong dim name = '%s', expected='%s'\n",
                   my_rank, dimid_bar, test_dimname, DIM_NAME_FOO);
        }

        PIO_Offset test_dimvalue[1];
        PIOc_inq_dimlen(ncid, dimid_bar, test_dimvalue);
        if ((PIO_Offset)dim_len_bar[0] != test_dimvalue[0])
        {
            printf("rank: %d PIOc_inq_dimlen(%s) returned wrong dimension size =%lld, expected=%d\n",
                   my_rank, DIM_NAME_BAR, (long long)test_dimvalue[0], dim_len_bar[0]);
        }

        /* Test errors for some non-existent dimension names/IDs */
        int err_handling_method = PIOc_Set_IOSystem_Error_Handling(iosysid, PIO_BCAST_ERROR);

        ret = PIOc_inq_varid(ncid, "NonexistentVariable", &test_varid);
        if (ret != PIO_ENOTVAR)
        {
            printf("rank: %d PIOc_inq_varid(NonexistentVariable) returned wrong return code=%d, expected=%d\n",
                   my_rank, ret, PIO_EBADNAME);
        }

        ret = PIOc_inq_dimid(ncid, "NonexistentVariable", &test_dimid);
        if (ret != PIO_EBADDIM)
        {
            printf("rank: %d PIOc_inq_dimid(NonexistentVariable) returned wrong return code=%d, expected PIO_EBADDIM=%d\n",
                   my_rank, ret, PIO_EBADDIM);
        }

        ret = PIOc_inq_dimlen(ncid, 15, test_dimvalue);
        if (ret != PIO_EBADDIM)
        {
            printf("rank: %d PIOc_inq_dimlen(15) should have returned PIO_EBADID error. "
                   "Instead it returned error code %d and dimension size =%lld, expected=0\n",
                   my_rank, ret, (long long)test_dimvalue[0]);
        }

        PIOc_Set_IOSystem_Error_Handling(iosysid, err_handling_method);

        /* Write data to the file. */
        if (verbose)
            printf("rank: %d Writing sample data...\n", my_rank);

        if ((ret = PIOc_write_darray(ncid, varid_foo, ioid_foo, (PIO_Offset)elements_per_pe_foo,
                                     buffer_foo, NULL)))
            ERR(ret);

        if ((ret = PIOc_write_darray(ncid, varid_foo2, ioid_foo, (PIO_Offset)elements_per_pe_foo,
                                     buffer_foo, NULL)))
            ERR(ret);

        if ((ret = PIOc_write_darray(ncid, varid_bar, ioid_bar, (PIO_Offset)elements_per_pe_bar,
                                     buffer_bar, NULL)))
            ERR(ret);

        if ((ret = PIOc_put_var_text(ncid, varid_varname, VAR_NAME_FOO)))
            ERR(ret);

        if ((ret = PIOc_sync(ncid)))
            ERR(ret);

        /* Free buffer space used in this example. */
        free(buffer_foo);
        free(buffer_bar);

        /* Close the netCDF file. */
        if (verbose)
            printf("rank: %d Closing the sample data file...\n", my_rank);

        if ((ret = PIOc_closefile(ncid)))
            ERR(ret);
    }

    /* Free the PIO decomposition. */
    if (verbose)
        printf("rank: %d Freeing PIO decomposition...\n", my_rank);

    if ((ret = PIOc_freedecomp(iosysid, ioid_foo)))
        ERR(ret);

    /* Finalize the IO system. */
    if (verbose)
        printf("rank: %d Freeing PIO resources...\n", my_rank);

    if ((ret = PIOc_finalize(iosysid)))
        ERR(ret);

    /* Check the output file. */
    if (!my_rank)
    {
        for (int fmt = 0; fmt < num_flavors; fmt++)
        {
            sprintf(filename, "example3_%d.nc", fmt);
            if (format[fmt] != PIO_IOTYPE_ADIOS &&
                (ret = check_file(ntasks, filename)))
                ERR(ret);
        }
    }

    /* Finalize the MPI library. */
    MPI_Finalize();

#ifdef TIMING
    /* Finalize the GPTL timing library. */
    if ((ret = GPTLfinalize()))
        return ret;
#endif

    if (verbose)
        printf("rank: %d SUCCESS!\n", my_rank);

    return 0;
}
