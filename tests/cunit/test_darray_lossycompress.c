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

/* The number of tasks this test should run on. */
#define TARGET_NTASKS 4

/* The minimum number of tasks this test should run on. */
#define MIN_NTASKS 4

/* The name of this test. */
#define TEST_NAME "test_darray_lossycompress"

/* Number of processors that will do IO. */
#define NUM_IO_PROCS 1

/* Number of computational components to create. */
#define COMPONENT_COUNT 1

/* The number of dimensions in the example data. In this test, we
 * are using three-dimensional data. */
#define NDIM 3

/* But sometimes we need arrays of the non-record dimensions. */
#define NDIM2 2

/* The length of our sample data along each dimension. */
#define X_DIM_LEN 4
#define Y_DIM_LEN 4

/* The number of timesteps of data to write. */
#define NUM_TIMESTEPS 2

/* The names of variables in the netCDF output files. */
#define VAR_NAME "Billy-Bob"
#define VAR_NAME2 "Sally-Sue"

/* Test cases relating to PIOc_write_darray_multi(). */
#define NUM_TEST_CASES_WRT_MULTI 3

/* Test with and without specifying a fill value to
 * PIOc_write_darray(). */
#define NUM_TEST_CASES_FILLVALUE 1

/* This struct allows us to treat float as uint32_t
 * types. */
union FU {
    float f;
    uint32_t u;
};

/* This struct allows us to treat double points as uint64_t
 * types. */
union DU {
    double d;
    uint64_t u;
};

/* The dimension names. */
char dim_name[NDIM][PIO_MAX_NAME + 1] = {"timestep", "x", "y"};

/* Length of the dimensions in the sample data. */
int dim_len[NDIM] = {NC_UNLIMITED, X_DIM_LEN, Y_DIM_LEN};

/**
 * Test the darray functionality. Create a netCDF file with 3
 * dimensions and 1 PIO_INT variable, and use darray to write some
 * data.
 *
 * @param iosysid the IO system ID.
 * @param ioid the ID of the decomposition.
 * @param num_flavors the number of IOTYPES available in this build.
 * @param flavor array of available iotypes.
 * @param my_rank rank of this task.
 * @param pio_type the type of the data.
 * @returns 0 for success, error code otherwise.
 */
int test_darray(int iosysid, int ioid, int num_flavors, int *flavor, int my_rank,
		int pio_type)
{
    char filename[PIO_MAX_NAME + 1]; /* Name for the output files. */
    int dimids[NDIM];      /* The dimension IDs. */
    int ncid;      /* The ncid of the netCDF file. */
    int ncid2;     /* The ncid of the re-opened netCDF file. */
    int varid;     /* The ID of the netCDF varable. */
    int varid2;     /* The ID of a netCDF varable of different type. */
    int wrong_varid = TEST_VAL_42;  /* A wrong ID. */
    int ret;       /* Return code. */
    MPI_Datatype mpi_type;
    int type_size; /* size of a variable of type pio_type */
    int other_type; /* another variable of the same size but different type */
    PIO_Offset arraylen = 4;
    void *fillvalue, *ofillvalue;
    void *test_data;
    void *test_data_in;
    int fillvalue_int = NC_FILL_INT;
    int test_data_int[arraylen];
    int test_data_int_in[arraylen];
    float fillvalue_float = NC_FILL_FLOAT;
    float test_data_float[arraylen];
    float test_data_float_in[arraylen] ;
    double fillvalue_double = NC_FILL_DOUBLE;
    double test_data_double[arraylen];
    double test_data_double_in[arraylen];
#ifdef NC_NOQUANTIZE
    //    union FU fin;

    //xpect[0].u = 0x3f8e3000;
    //xpect[1].u = 0x3f800fff;
    //xpect[2].u = 0x41200000;
    //xpect[3].u = 0x4640efff;
//    xpect[4].u = 0x3dfcd000;
    //double_xpect[0].u = 0x3ff1c60000000000;
    //double_xpect[1].u = 0x3ff001ffffffffff;
    //double_xpect[2].u = 0x4023fe0000000000;
    //double_xpect[3].u = 0x41d265ffffffffff;
//    double_xpect[4].u = 0x42dc120000000000;
#endif
    /* Initialize some data. */
    for (int f = 0; f < arraylen; f++)
    {
	test_data_int[f] = my_rank * 10 + f;
    }
    test_data_float[0] = 1.11111111;
    test_data_float[1] = 1.0;
    test_data_float[2] = 9.99999999;
    test_data_float[3] = 12345.67;
//    test_data_float[4] = .1234567;

    test_data_double[0] = 1.1111111;
    test_data_double[1] = 1.0;
    test_data_double[2] = 9.999999999;
    test_data_double[3] = 1234567890.12345;
//    test_data_double[4] = 123456789012345.0;




    /* Use PIO to create the example file in each of the four
     * available ways. */
    for (int fmt = 0; fmt < num_flavors; fmt++)
    {
	if(flavor[fmt] == PIO_IOTYPE_NETCDF4P || flavor[fmt] == PIO_IOTYPE_NETCDF4C)
	{
	    /* Add a couple of extra tests for the
	     * PIOc_write_darray_multi() function. */
	    for (int test_multi = 0; test_multi < NUM_TEST_CASES_WRT_MULTI; test_multi++)
	    {
		/* Test with/without providing a fill value to PIOc_write_darray(). */
		for (int provide_fill = 0; provide_fill < NUM_TEST_CASES_FILLVALUE; provide_fill++)
		{
		    /* Create the filename. */
		    sprintf(filename, "data_%s_iotype_%d_pio_type_%d_test_multi_%d_provide_fill_%d.nc", TEST_NAME,
			    flavor[fmt], pio_type, test_multi, provide_fill);
		    /* Select the fill value and data. */
		    switch (pio_type)
		    {
		    case PIO_INT:
			fillvalue = provide_fill ? &fillvalue_int : NULL;
			test_data = test_data_int;
			test_data_in = test_data_int_in;
			break;
		    case PIO_FLOAT:
			fillvalue = provide_fill ? &fillvalue_float : NULL;
			test_data = test_data_float;
			test_data_in = test_data_float_in;
			break;
		    case PIO_DOUBLE:
			fillvalue = provide_fill ? &fillvalue_double : NULL;
			test_data = test_data_double;
			test_data_in = test_data_double_in;
			break;
		    default:
			ERR(ERR_WRONG);
		    }

		    /* Create the netCDF output file. */
		    if ((ret = PIOc_createfile(iosysid, &ncid, &flavor[fmt], filename, PIO_CLOBBER)))
			ERR(ret);

		    /* Define netCDF dimensions and variable. */
		    for (int d = 0; d < NDIM; d++)
			if ((ret = PIOc_def_dim(ncid, dim_name[d], (PIO_Offset)dim_len[d], &dimids[d])))
			    ERR(ret);

		    /* Define a variable. */
		    if ((ret = PIOc_def_var(ncid, VAR_NAME, pio_type, NDIM, dimids, &varid)))
			ERR(ret);
#ifdef NC_NOQUANTIZE
		    if(pio_type == PIO_REAL || pio_type == PIO_DOUBLE)
		    {
		        if ((ret = PIOc_def_var_quantize(ncid, varid, PIO_QUANTIZE_BITGROOM, 3)))
		            ERR(ret);
		    }
		    else
		    {
		        /* this should fail */
		        if ((ret = PIOc_def_var_quantize(ncid, varid, PIO_QUANTIZE_BITROUND, 5) != PIO_EINVAL ))
		            ERR(ret);

		    }
#endif
		    if ((ret = PIOc_def_var_deflate(ncid, varid, 0, 1, 1)))
			ERR(ret);


		    /* Define a variable with a different type but same size. */
		    if ((ret = find_mpi_type(pio_type, &mpi_type, &type_size)))
			ERR(ret);
		    if (type_size == NETCDF_INT_FLOAT_SIZE)
			other_type = pio_type == PIO_INT ? PIO_FLOAT : PIO_INT;
//		else if(type_size == NETCDF_DOUBLE_INT64_SIZE)
//		    other_type = pio_type == PIO_INT64 ? PIO_DOUBLE : PIO_INT64;
		    else
			other_type = 0; /* skip the test */
		    switch (other_type)
		    {
		    case PIO_INT:
			ofillvalue = provide_fill ? &fillvalue_int : NULL;
			break;
		    case PIO_FLOAT:
			ofillvalue = provide_fill ? &fillvalue_float : NULL;
			break;
		    default:
			break;
		    }
		    if (other_type && (ret = PIOc_def_var(ncid, VAR_NAME2, other_type, NDIM, dimids, &varid2)))
			ERR(ret);

		    /* End define mode. */
		    if ((ret = PIOc_enddef(ncid)))
			ERR(ret);

		    /* Set the value of the record dimension. */
		    if ((ret = PIOc_setframe(ncid, varid, 0)))
			ERR(ret);
		    if (other_type && (ret = PIOc_setframe(ncid, varid2, 0)))
			ERR(ret);

		    int frame = 0;
		    int flushtodisk = test_multi - 1;
		    if (!test_multi)
		    {
			/* These should not work. */
			if (PIOc_write_darray(ncid + TEST_VAL_42, varid, ioid, arraylen, test_data, fillvalue) != PIO_EBADID)
			    ERR(ERR_WRONG);
			if (PIOc_write_darray(ncid, varid, ioid + TEST_VAL_42, arraylen, test_data, fillvalue) != PIO_EBADID)
			    ERR(ERR_WRONG);
			if (PIOc_write_darray(ncid, varid, ioid, arraylen - 1, test_data, fillvalue) != PIO_EINVAL)
			    ERR(ERR_WRONG);
			if (PIOc_write_darray(ncid, TEST_VAL_42, ioid, arraylen, test_data, fillvalue) != PIO_ENOTVAR)
			    ERR(ERR_WRONG);

			/* This should work - library type conversion */
			if (other_type && (ret = PIOc_write_darray(ncid, varid2, ioid, arraylen, test_data, ofillvalue)))
			    ERR(ret);

			/* Write the data. */
//                        printf("test_data[0] = %f\n",test_data_float[0]);
			if ((ret = PIOc_write_darray(ncid, varid, ioid, arraylen, test_data, fillvalue)))
			    ERR(ret);

		    }
		    else
		    {
			int varid_big = PIO_MAX_VARS + TEST_VAL_42;

			/* These will not work. */
			if (PIOc_write_darray_multi(ncid + TEST_VAL_42, &varid, ioid, 1, arraylen, test_data, &frame,
						    fillvalue, flushtodisk) != PIO_EBADID)
			    ERR(ERR_WRONG);
			if (PIOc_write_darray_multi(ncid, NULL, ioid, 1, arraylen, test_data, &frame,
						    fillvalue, flushtodisk) != PIO_EINVAL)
			    ERR(ERR_WRONG);
			if (PIOc_write_darray_multi(ncid, &varid, ioid + TEST_VAL_42, 1, arraylen, test_data, &frame,
						    fillvalue, flushtodisk) != PIO_EBADID)
			    ERR(ERR_WRONG);
			if (PIOc_write_darray_multi(ncid, &varid, ioid, -1, arraylen, test_data, &frame,
						    fillvalue, flushtodisk) != PIO_EINVAL)
			    ERR(ERR_WRONG);
			if (PIOc_write_darray_multi(ncid, &varid_big, ioid, 1, arraylen, test_data, &frame,
						    fillvalue, flushtodisk) != PIO_ENOTVAR)
			    ERR(ERR_WRONG);
			if (PIOc_write_darray_multi(ncid, &wrong_varid, ioid, 1, arraylen, test_data, &frame,
						    fillvalue, flushtodisk) != PIO_ENOTVAR)
			    ERR(ERR_WRONG);
//		    pio_setloglevel(0);

		    /* This should work - library type conversion */
			if (other_type && (ret = PIOc_write_darray_multi(ncid, &varid2, ioid, 1, arraylen, test_data, &frame,
									 fillvalue, flushtodisk)))
			    ERR(ret);

			/* Write the data with the _multi function. */
			if ((ret = PIOc_write_darray_multi(ncid, &varid, ioid, 1, arraylen, test_data, &frame,
							   fillvalue, flushtodisk)))
			    ERR(ret);
		    }

		    /* Close the netCDF file. */
		    if ((ret = PIOc_closefile(ncid)))
			ERR(ret);


		    /* Reopen the file. */
		    if ((ret = PIOc_openfile(iosysid, &ncid2, &flavor[fmt], filename, PIO_NOWRITE)))
			ERR(ret);

		    PIO_Offset dimlen;
		    /* check the unlimited dim size - it should be 1 */
		    if ((ret = PIOc_inq_dimlen(ncid2, dimids[0], &dimlen)))
			ERR(ret);
		    if (dimlen != 1)
			ERR(ERR_WRONG);

		    /* These should not work. */
		    if (PIOc_read_darray(ncid2 + TEST_VAL_42, varid, ioid, arraylen,
					 test_data_in) != PIO_EBADID)
			ERR(ERR_WRONG);
		    if (PIOc_read_darray(ncid2, varid, ioid + TEST_VAL_42, arraylen,
					 test_data_in) != PIO_EBADID)
			ERR(ERR_WRONG);

		    /* Set the record number. */
		    if ((ret = PIOc_setframe(ncid2, varid, 0)))
			ERR(ret);

		    /* Read the data. */
		    if ((ret = PIOc_read_darray(ncid2, varid, ioid, arraylen, test_data_in)))
			ERR(ret);

		    /* /\* Read the data. *\/ */
		    /* if ((ret = PIOc_get_vard(ncid2, varid, ioid, 0, (void *)test_data_in))) */
		    /*     ERR(ret); */

		    /* Check the results. */
                    /* HOW does one test a lossy compression algorythm? */
#ifdef NC_NOQUANTIZE
		    for (int f = 0; f < arraylen; f++)
		    {
			switch (pio_type)
			{
			case PIO_INT:
			    if (test_data_int_in[f] != test_data_int[f])
				return ERR_WRONG;
			    break;
			case PIO_FLOAT:
			    /*
			      We do not expect an exact match for lossy data - so how do we test?
			    if (test_data_float_in[f] != test_data_float[f])
				return ERR_WRONG;
			    */
			    break;
			case PIO_DOUBLE:
			  /*
			    if (test_data_double_in[f] != test_data_double[f])
				return ERR_WRONG;
			    */
			  break;
			default:
			  ERR(ERR_WRONG);
			}
		    }
#else
                    printf("Quantize support was not available in the netcdf build and thus is not tested\n");
#endif

		    /* Try to write, but it won't work, because we opened file read-only. */
		    if (!test_multi)
		    {
			if (PIOc_write_darray(ncid2, varid, ioid, arraylen, test_data, fillvalue) != PIO_EPERM)
			    ERR(ERR_WRONG);
		    }
		    else
		    {
			if (PIOc_write_darray_multi(ncid2, &varid, ioid, 1, arraylen, test_data, &frame,
						    fillvalue, flushtodisk) != PIO_EPERM)
			    ERR(ERR_WRONG);
		    }
		    /* Close the netCDF file. */
		    if ((ret = PIOc_closefile(ncid2)))
			ERR(ret);
		} /* next fillvalue test case */
	    } /* next test multi */
	}/* only netcdf4 iotypes allowed */
    } /* next iotype */

    return PIO_NOERR;
}

/**
 * Run all the tests.
 *
 * @param iosysid the IO system ID.
 * @param num_flavors number of available iotypes in the build.
 * @param flavor pointer to array of the available iotypes.
 * @param my_rank rank of this task.
 * @param test_comm the communicator the test is running on.
 * @returns 0 for success, error code otherwise.
 */
int test_all_darray(int iosysid, int num_flavors, int *flavor, int my_rank,
		    MPI_Comm test_comm)
{
#define NUM_TYPES_TO_TEST 2
    int ioid;
    char filename[PIO_MAX_NAME + 1];
    int pio_type[NUM_TYPES_TO_TEST] = {PIO_FLOAT, PIO_DOUBLE};
    int dim_len_2d[NDIM2] = {X_DIM_LEN, Y_DIM_LEN};
    int ret; /* Return code. */

    for (int t = 0; t < NUM_TYPES_TO_TEST; t++)
    {
	/* This will be our file name for writing out decompositions. */
	sprintf(filename, "%s_decomp_rank_%d_flavor_%d_type_%d.nc", TEST_NAME, my_rank,
		*flavor, pio_type[t]);

	/* Decompose the data over the tasks. */
	if ((ret = create_decomposition_2d(TARGET_NTASKS, my_rank, iosysid, dim_len_2d,
					   &ioid, pio_type[t])))
	    return ret;

	/* Run a simple darray test. */
	if ((ret = test_darray(iosysid, ioid, num_flavors, flavor, my_rank, pio_type[t])))
	    return ret;

	/* Free the PIO decomposition. */
	if ((ret = PIOc_freedecomp(iosysid, ioid)))
	    ERR(ret);
    }

    return PIO_NOERR;
}

/* Run tests for darray functions. */
int main(int argc, char **argv)
{
#define NUM_REARRANGERS_TO_TEST 2
    int rearranger[NUM_REARRANGERS_TO_TEST] = {PIO_REARR_BOX, PIO_REARR_SUBSET};
    int my_rank;
    int ntasks;
    int num_flavors; /* Number of PIO netCDF flavors in this build. */
    int flavor[NUM_FLAVORS]; /* iotypes for the supported netCDF IO flavors. */
    MPI_Comm test_comm; /* A communicator for this test. */
    int ret;         /* Return code. */

    /* Initialize test. */
    if ((ret = pio_test_init2(argc, argv, &my_rank, &ntasks, MIN_NTASKS,
			      MIN_NTASKS, -1, &test_comm)))
	ERR(ERR_INIT);

    if ((ret = PIOc_set_iosystem_error_handling(PIO_DEFAULT, PIO_RETURN_ERROR, NULL)))
	return ret;

    /* Only do something on max_ntasks tasks. */
    if (my_rank < TARGET_NTASKS)
    {
	int iosysid;  /* The ID for the parallel I/O system. */
	int ioproc_stride = 1;    /* Stride in the mpi rank between io tasks. */
	int ioproc_start = 0;     /* Zero based rank of first processor to be used for I/O. */
	int ret;      /* Return code. */

	/* Figure out iotypes. */
	if ((ret = get_iotypes(&num_flavors, flavor)))
	    ERR(ret);

	for (int r = 0; r < NUM_REARRANGERS_TO_TEST; r++)
	{
	    /* Initialize the PIO IO system. This specifies how
	     * many and which processors are involved in I/O. */
	    if ((ret = PIOc_Init_Intracomm(test_comm, TARGET_NTASKS, ioproc_stride,
					   ioproc_start, rearranger[r], &iosysid)))
		return ret;

	    /* Run tests. */
	    if ((ret = test_all_darray(iosysid, num_flavors, flavor, my_rank, test_comm)))
		return ret;

	    /* Finalize PIO system. */
	    if ((ret = PIOc_free_iosystem(iosysid)))
		return ret;
	} /* next rearranger */
    } /* endif my_rank < TARGET_NTASKS */

    /* Finalize the MPI library. */
    if ((ret = pio_test_finalize(&test_comm)))
	return ret;
    /* if ((ret = pio_test_finalize2(&test_comm, TEST_NAME))) */
    /*     return ret; */

    printf("%d %s SUCCESS!!\n", my_rank, TEST_NAME);
    return 0;
}
