/**
 * @file Tests the PIO library with multiple iosysids in use at the
 * same time.
 *
 * This is a simplified, C version of the fortran
 * pio_iosystem_tests3.F90.
 *
 * @author Ed Hartnett
 */
#include <pio.h>
#include <pio_tests.h>

/* The number of tasks this test should run on. */
#define TARGET_NTASKS 4

/* The name of this test. */
#define TEST_NAME "test_iosystem3"

/* Used to define netcdf test file. */
#define PIO_TF_MAX_STR_LEN 100
#define ATTNAME "filename"
#define DIMNAME "filename_dim"

/* Used to devide up the tasks into MPI groups. */
#define OVERLAP_NUM_RANGES 2
#define EVEN_NUM_RANGES 1

/** This creates a netCDF file in the specified format, with some
 * sample values. */
int
create_file(MPI_Comm comm, int iosysid, int format, char *filename,
	    char *attname, char *dimname, int my_rank)
{
    int ncid, varid, dimid;
    int ret;

    /* Create the file. */
    if ((ret = PIOc_createfile(iosysid, &ncid, &format, filename, NC_CLOBBER)))
	return ret;
    printf("%d file created ncid = %d\n", my_rank, ncid);

    /* Define a dimension. */
    printf("%d defining dimension %s\n", my_rank, dimname);
    if ((ret = PIOc_def_dim(ncid, dimname, PIO_TF_MAX_STR_LEN, &dimid)))
	return ret;

    /* Define a 1-D variable. */
    printf("%d defining variable %s\n", my_rank, attname);
    if ((ret = PIOc_def_var(ncid, attname, NC_CHAR, 1, &dimid, &varid)))
    	return ret;

    /* Write an attribute. */
    if ((ret = PIOc_put_att_text(ncid, varid, attname, strlen(filename), filename)))
    	return ret;

    /* End define mode. */
    printf("%d ending define mode ncid = %d\n", my_rank, ncid);
    if ((ret = PIOc_enddef(ncid)))
	return ret;
    printf("%d define mode ended ncid = %d\n", my_rank, ncid);

    /* Close the file. */
    printf("%d closing file ncid = %d\n", my_rank, ncid);
    if ((ret = PIOc_closefile(ncid)))
	return ret;
    printf("%d closed file ncid = %d\n", my_rank, ncid);
    
    return PIO_NOERR;
}

/** This checks an already-open netCDF file. */
int
check_file(MPI_Comm comm, int iosysid, int format, int ncid, char *filename,
	   char *attname, char *dimname, int my_rank)
{
    int dimid;
    int ret;

    /* Check the file. */
    if ((ret = PIOc_inq_dimid(ncid, dimname, &dimid)))
	return ret;
    printf("%d dimid = %d\n", my_rank, dimid);

    return PIO_NOERR;
}

/** This opens and checks a netCDF file. */
int open_and_check_file(MPI_Comm comm, int iosysid, int iotype, int *ncid, char *fname,
		    char *attname, char *dimname, int disable_close, int my_rank)
{
    int mode = PIO_WRITE;
    int ret;

    /* Open the file. */
    if ((ret = PIOc_openfile(iosysid, ncid, &iotype, fname, mode)))
	return ret;

    /* Check the file. */
    if ((ret = check_file(comm, iosysid, iotype, *ncid, fname, attname, dimname, my_rank)))
    	return ret;

    /* Close the file, maybe. */
    if (!disable_close)
	if ((ret = PIOc_closefile(*ncid)))
	    return ret;
	
    return PIO_NOERR;
}

/** Run async tests. */
int
main(int argc, char **argv)
{
    int my_rank; /* Zero-based rank of processor. */
    int ntasks; /* Number of processors involved in current execution. */
    int iosysid; /* The ID for the parallel I/O system. */
    int iosysid_world; /* The ID for the parallel I/O system. */
    MPI_Group world_group; /* An MPI group of world. */
    MPI_Group even_group; /* An MPI group of 0 and 2. */
    MPI_Group overlap_group; /* An MPI group of 0, 1, and 3. */
    MPI_Comm even_comm = MPI_COMM_NULL; /* Communicator for tasks 0, 2 */
    MPI_Comm overlap_comm = MPI_COMM_NULL; /* Communicator for tasks 0, 1, 2. */
    int ret; /* Return code. */

    int iotypes[NUM_FLAVORS] = {PIO_IOTYPE_PNETCDF, PIO_IOTYPE_NETCDF,
				PIO_IOTYPE_NETCDF4C, PIO_IOTYPE_NETCDF4P};

    /* Initialize test. */
    if ((ret = pio_test_init(argc, argv, &my_rank, &ntasks, TARGET_NTASKS)))
	ERR(ERR_INIT);

    /* Initialize PIO system on world. */
    if ((ret = PIOc_Init_Intracomm(MPI_COMM_WORLD, 4, 1, 0, 1, &iosysid_world)))
	ERR(ret);

    /* Get MPI_Group of world comm. */
    if ((ret = MPI_Comm_group(MPI_COMM_WORLD, &world_group)))
	ERR(ret);

    /* Create a group with tasks 0 and 2. */
    int even_ranges[EVEN_NUM_RANGES][3] = {0, 2, 2};
    if ((ret = MPI_Group_range_incl(world_group, EVEN_NUM_RANGES, even_ranges, &even_group)))
	ERR(ret);

    /* Create a communicator from the even_group. */
    if ((ret = MPI_Comm_create(MPI_COMM_WORLD, even_group, &even_comm)))
	ERR(ret);

    /* Create a group with tasks 0, 1, and 3. */
    int overlap_ranges[OVERLAP_NUM_RANGES][3] = {{1, 3, 2}, {0, 0, 1}};
    if ((ret = MPI_Group_range_incl(world_group, OVERLAP_NUM_RANGES,
				    overlap_ranges, &overlap_group)))
	ERR(ret);
    
    /* Create a communicator from the overlap_group. */
    if ((ret = MPI_Comm_create(MPI_COMM_WORLD, overlap_group, &overlap_comm)))
	ERR(ret);
    printf("%d overlap_comm = %d\n", my_rank, overlap_comm);

    /* Split world into odd and even. */
    MPI_Comm newcomm;
    int even = my_rank % 2 ? 0 : 1;
    if ((ret = MPI_Comm_split(MPI_COMM_WORLD, even, 0, &newcomm)))
	MPIERR(ret);
    printf("%d newcomm = %d even = %d\n", my_rank, newcomm, even);

    /* Get rank in new communicator and its size. */
    int new_rank, new_size;
    if ((ret = MPI_Comm_rank(newcomm, &new_rank)))
	MPIERR(ret);
    if ((ret = MPI_Comm_size(newcomm, &new_size)))
	MPIERR(ret);
    printf("%d newcomm = %d new_rank = %d new_size = %d\n", my_rank, newcomm,
	   new_rank, new_size);

    /* Initialize PIO system. */
    if ((ret = PIOc_Init_Intracomm(newcomm, 2, 1, 0, 1, &iosysid)))
	ERR(ret);

    for (int i = 0; i < NUM_FLAVORS; i++)
    {
	char fname0[] = "pio_iosys_test_file0.nc"; 
	char fname1[] = "pio_iosys_test_file1.nc"; 
	char fname2[] = "pio_iosys_test_file2.nc";
	printf("\n\n%d i = %d\n", my_rank, i);

	if ((ret = create_file(MPI_COMM_WORLD, iosysid_world, iotypes[i], fname0, ATTNAME,
			       DIMNAME, my_rank)))
	    ERR(ret);

	if ((ret = create_file(MPI_COMM_WORLD, iosysid_world, iotypes[i], fname1, ATTNAME,
			       DIMNAME, my_rank)))
	    ERR(ret);

	if ((ret = create_file(MPI_COMM_WORLD, iosysid_world, iotypes[i], fname2, ATTNAME,
			       DIMNAME, my_rank)))
	    ERR(ret);

	MPI_Barrier(MPI_COMM_WORLD);

	/* Now check the first file. */
	int ncid;
	if ((ret = open_and_check_file(MPI_COMM_WORLD, iosysid_world, iotypes[i], &ncid, fname0,
				       ATTNAME, DIMNAME, 1, my_rank)))
	    ERR(ret);

	/* Now have the odd/even communicators each check one of the
	 * remaining files. */
	int ncid2;
	char *fname = even ? fname1 : fname2;
	printf("\n***\n");
	if ((ret = open_and_check_file(newcomm, iosysid, iotypes[i], &ncid2, fname,
				       ATTNAME, DIMNAME, 1, my_rank)))
	    ERR(ret);
	

	/* Close the still-open files. */
	if ((ret = PIOc_closefile(ncid)))
	    ERR(ret);
	if ((ret = PIOc_closefile(ncid2)))
	    ERR(ret);
    } /* next iotype */

    /* Finalize PIO system. */
    if ((ret = PIOc_finalize(iosysid)))
	ERR(ret);

    /* Finalize PIO system. */
    if ((ret = PIOc_finalize(iosysid_world)))
	ERR(ret);

    /* Free MPI resources used by test. */
    if ((ret = MPI_Group_free(&overlap_group)))
	ERR(ret);
    if ((ret = MPI_Group_free(&even_group)))
	ERR(ret);
    if ((ret = MPI_Group_free(&world_group)))
	ERR(ret);
    if (overlap_comm != MPI_COMM_NULL)
	if ((ret = MPI_Comm_free(&overlap_comm)))
	    ERR(ret);
    if (even_comm != MPI_COMM_NULL)
    	if ((ret = MPI_Comm_free(&even_comm)))
    	    ERR(ret);

    /* Finalize test. */
    printf("%d %s finalizing...\n", my_rank, TEST_NAME);
    if ((ret = pio_test_finalize()))
	ERR(ERR_AWFUL);

    printf("%d %s SUCCESS!!\n", my_rank, TEST_NAME);

    return 0;
}
