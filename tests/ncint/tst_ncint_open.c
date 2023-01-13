/* Test openfile function in ncint layer.

   This test simply makes sure that a file created in any mode can be reopened.


*/
#include "config.h"
#include "pio_err_macros.h"
#include "ncint.h"

#define FILE_NAME "tst_pio_udf_open.nc"
#define VAR_NAME "data_var"
#define DIM_NAME_UNLIMITED "dim_unlimited"
#define DIM_NAME_X "dim_x"
#define DIM_NAME_Y "dim_y"
#define DIM_LEN_X 4
#define DIM_LEN_Y 4
#define NDIM2 2
#define NDIM3 3
#define TEST_VAL_42 42

extern NC_Dispatch NCINT_dispatcher;

int
main(int argc, char **argv)
{
    int my_rank;
    int ntasks;

    /* Initialize MPI. */
    if (MPI_Init(&argc, &argv)) PERR;

    /* Learn my rank and the total number of processors. */
    if (MPI_Comm_rank(MPI_COMM_WORLD, &my_rank)) PERR;
    if (MPI_Comm_size(MPI_COMM_WORLD, &ntasks)) PERR;

    if (!my_rank)
        printf("\n*** Testing netCDF integration layer.\n");


    if (!my_rank)
        printf("*** testing simple use of netCDF integration layer format...\n");
    {
        int ncid, ioid;
        int dimid[NDIM3], varid;
        int dimlen[NDIM3] = {NC_UNLIMITED, DIM_LEN_X, DIM_LEN_Y};
        int iosysid;
        NC_Dispatch *disp_in;
        int i;

        /* Turn on logging for PIO library. */
        /* PIOc_set_log_level(3); */

        /* Initialize the intracomm. */
        if (nc_def_iosystem(MPI_COMM_WORLD, 1, 1, 0, 0, &iosysid)) PERR;

	for(int m=7; m < NUM_MODES; m++){
	  /* Create a file with a 3D record var. */
	  if(!my_rank)
	    printf("     cmode = %d\n", cmode[m]);
	  if (nc_create(FILE_NAME, cmode[m], &ncid)) PERR;
	  if (nc_def_dim(ncid, DIM_NAME_UNLIMITED, dimlen[0], &dimid[0])) PERR;
	  if (nc_def_dim(ncid, DIM_NAME_X, dimlen[1], &dimid[1])) PERR;
	  if (nc_def_dim(ncid, DIM_NAME_Y, dimlen[2], &dimid[2])) PERR;
	  if (nc_def_var(ncid, VAR_NAME, NC_INT, NDIM3, dimid, &varid)) PERR;
	  if (nc_enddef(ncid)) PERR;

	  if (nc_close(ncid)) PERR;

	  /* Check that our user-defined format has been added. */
	  if (nc_inq_user_format(NC_PIO, &disp_in, NULL)) PERR;
	  if (disp_in != &NCINT_dispatcher) PERR;

	  for(int n=0; n < NUM_MODES; n++){
	    /* Open the file. */
	    if (nc_open(FILE_NAME, cmode[n], &ncid)) PERR;
	  
	    /* Close file. */
	    if (nc_close(ncid)) PERR;
	  }
	  /* Free resources. */

	}
	if (nc_free_iosystem(iosysid)) PERR;
    }
    PSUMMARIZE_ERR;

    /* Finalize MPI. */
    MPI_Finalize();
    PFINAL_RESULTS;
}  
