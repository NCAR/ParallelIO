 /*
 * Tests variable compression settings.
 *
 * @author Ed Hartnett
 * @date 7/21/20
 */
#include <config.h>
#include <pio.h>
#include <pio_internal.h>
#include <pio_tests.h>

/* The number of tasks this test should run on. */
#define TARGET_NTASKS 4

/* The minimum number of tasks this test should run on. */
#define MIN_NTASKS 4

/* The name of this test. */
#define TEST_NAME "test_deflate"

/* Number of processors that will do IO. */
#define NUM_IO_PROCS 1

/* Number of computational components to create. */
#define COMPONENT_COUNT 1

/* The number of dimensions in the example data. In this test, we
 * are using three-dimensional data. */
#define NDIM3 3

/* The length of our sample data along each dimension. */
#define X_DIM_LEN 4
#define Y_DIM_LEN 4

/* The number of timesteps of data to write. */
#define NUM_TIMESTEPS 2

/* The name of the variable in the netCDF output files. */
#define VAR_NAME "var_3D"

/* The meaning of life, the universe, and everything. */
#define START_DATA_VAL 42

/* The dimension names. */
char dim_name[NDIM3][PIO_MAX_NAME + 1] = {"timestep", "x", "y"};

/* Length of the dimensions in the sample data. */
int dim_len[NDIM3] = {NC_UNLIMITED, X_DIM_LEN, Y_DIM_LEN};

/* Tests with deflate. Only netcdf-4 IOTYPES support deflate. */
int run_deflate_test(int iosysid, int mpi_size, int iotype, int my_rank,
		     MPI_Comm test_comm)
{
    int ncid;
    char filename[PIO_MAX_NAME + 1];
    int varid;
    int dimid[NDIM3];
    int ioid;
    int *data;
    PIO_Offset elements_per_pe; /* Array elements per processing unit. */
    PIO_Offset *compdof;  /* The decomposition mapping. */
    int d, i;
    int ret;

    /* Create filename. */
    sprintf(filename, "%s_%d.nc", TEST_NAME, iotype);

    /* How many data elements per task? */
    elements_per_pe = X_DIM_LEN * Y_DIM_LEN / mpi_size;

    /* Allocate space for the decomposition array. */
    if (!(compdof = malloc(elements_per_pe * sizeof(PIO_Offset))))
        return PIO_ENOMEM;

    /* Describe the decomposition. This is a 1-based array, so add 1! */
    for (i = 0; i < elements_per_pe; i++)
        compdof[i] = my_rank * elements_per_pe + i + 1;

    /* Create the PIO decomposition for this test. */
    if ((ret = PIOc_InitDecomp(iosysid, PIO_FLOAT, NDIM3 - 1, &dim_len[1],
			       elements_per_pe, compdof, &ioid, NULL, NULL,
			       NULL)))
        ERR(ret);

    /* Free the mapping. */
    free(compdof);

    /* Allocate space for data for this pe. */
    if (!(data = malloc(elements_per_pe * sizeof(int))))
        return PIO_ENOMEM;
    
    /* Create some data. */
    for (i = 0; i < elements_per_pe; i++)
        data[i] = my_rank * elements_per_pe + i + 1;
    
    /* Create a test file. */
    if ((ret = PIOc_createfile(iosysid, &ncid, &iotype, filename, PIO_CLOBBER)))
        ERR(ret);

    /* Define netCDF dimensions. */
    for (d = 0; d < NDIM3; d++)
        if ((ret = PIOc_def_dim(ncid, dim_name[d], (PIO_Offset)dim_len[d],
				&dimid[d])))
            ERR(ret);

    /* Now add a var with deflation. */
    if ((ret = PIOc_def_var(ncid, VAR_NAME, PIO_INT, NDIM3, dimid, &varid)))
        ERR(ret);

    /* End define mode. */
    if ((ret = PIOc_enddef(ncid)))
	ERR(ret);

    if ((ret = PIOc_setframe(ncid, varid, 0)))
	ERR(ret);

    /* Write one record of data. */
    if ((ret = PIOc_write_darray(ncid, varid, ioid, elements_per_pe, data,
				 NULL)))
	ERR(ret);
    
    /* Close the file. */
    if ((PIOc_closefile(ncid)))
        return ret;

    {
	int ndims, nvars, ngatts, unlimdimid;

	/* Open the file again. */
	if ((ret = PIOc_openfile(iosysid, &ncid, &iotype, filename,
				 PIO_NOWRITE)))
	    ERR(ret);
	
	/* Check the file. */
	if ((ret = PIOc_inq(ncid, &ndims, &nvars, &ngatts, &unlimdimid)))
	    ERR(ret);
	if (ndims != NDIM3 || nvars != 1 || ngatts != 0 || unlimdimid != 0)
	    ERR(ERR_WRONG);
	
	
	/* Close the file. */
	if ((PIOc_closefile(ncid)))
	    return ret;
    }

    /* Free the PIO decomposition. */
    if ((ret = PIOc_freedecomp(iosysid, ioid)))
	ERR(ret);
    
    return PIO_NOERR;
}

/* Run all the tests. */
int test_all(int iosysid, int num_flavors, int *flavor, int my_rank,
	     MPI_Comm test_comm, int async)
{
    int mpi_size;
    char filename[PIO_MAX_NAME + 1];
    int ret;

    if ((ret = MPI_Comm_size(test_comm, &mpi_size)))
        MPIERR(ret);
    
    /* This will be our file name for writing out decompositions. */
    sprintf(filename, "decomp_%d.txt", my_rank);

    if (!async)
    {
        /* Use PIO to create the example file in each of the four
         * available ways. */
        for (int fmt = 0; fmt < num_flavors; fmt++)
        {
            /* Test file with deflate. */
	    if ((ret = run_deflate_test(iosysid, mpi_size, flavor[fmt],
					my_rank, test_comm)))
		return ret;
        }

    }

    return PIO_NOERR;
}

/* Run Tests for NetCDF-4 Functions. */
int main(int argc, char **argv)
{
    /* Change the 5th arg to 3 to turn on logging. */
    return run_test_main(argc, argv, MIN_NTASKS, TARGET_NTASKS, 3,
                         TEST_NAME, dim_len, COMPONENT_COUNT, NUM_IO_PROCS);
}
