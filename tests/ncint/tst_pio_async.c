/* Test netcdf integration layer.

   This is a very simple test of async mode in PIO, using the netCDF
   integration layer.

   Ed Hartnett
*/

#include "config.h"
#include <pio.h>
#include "pio_err_macros.h"
#include "ncint.h"

#define FILE_NAME "tst_pio_async.nc"
#define VAR_NAME "data_var"
#define DIM_NAME_UNLIMITED "dim_unlimited"
#define DIM_NAME_X "dim_x"
#define DIM_NAME_Y "dim_y"
#define DIM_LEN_X 3
#define DIM_LEN_Y 4
#define NDIM2 2
#define NDIM3 3

extern NC_Dispatch NCINT_dispatcher;

/* Number of computational components to create. */
#define COMPONENT_COUNT 1

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
        printf("*** testing simple async use of netCDF integration layer...\n");
    {
        int ncid, ioid;
        int dimid[NDIM3], varid;
        int dimlen[NDIM3] = {NC_UNLIMITED, DIM_LEN_X, DIM_LEN_Y};
        int iosysid;
        size_t elements_per_pe;
        size_t *compdof; /* The decomposition mapping. */
        int *my_data;
        int *data_in;
        int num_procs2[COMPONENT_COUNT] = {3};
        int num_io_procs = 1;
        int i;

        /* Turn on logging for PIO library. */
        /* PIOc_set_log_level(4); */
        /* if (!my_rank) */
        /*     nc_set_log_level(3); */

        /* Initialize the intracomm. The IO task will not return from
         * this call until the PIOc_finalize() is called by the
         * compute tasks. */
        if (nc_def_async(MPI_COMM_WORLD, num_io_procs, NULL, COMPONENT_COUNT,
                         num_procs2, NULL, NULL, NULL, PIO_REARR_BOX, &iosysid))
            PERR;

        if (my_rank)
        {
            int m;
            /* Create a file with a 3D record var. */
          for( m=0; m<NUM_MODES; m++){
            if(my_rank==1)
              printf("     cmode = %d\n", cmode[m]);
            if (nc_create(FILE_NAME, cmode[m], &ncid)) PERR;
            if (nc_def_dim(ncid, DIM_NAME_UNLIMITED, dimlen[0], &dimid[0])) PERR;
            if (nc_def_dim(ncid, DIM_NAME_X, dimlen[1], &dimid[1])) PERR;
            if (nc_def_dim(ncid, DIM_NAME_Y, dimlen[2], &dimid[2])) PERR;
            if (nc_def_var(ncid, VAR_NAME, NC_INT, NDIM3, dimid, &varid)) PERR;
            if (nc_enddef(ncid)) PERR;

            /* Calculate a decomposition for distributed arrays. */
            elements_per_pe = DIM_LEN_X * DIM_LEN_Y / (ntasks - num_io_procs);
            /* printf("my_rank %d elements_per_pe %ld\n", my_rank, elements_per_pe); */
            if (!(compdof = malloc(elements_per_pe * sizeof(size_t))))
                PERR;
            for (i = 0; i < elements_per_pe; i++)
            {
                compdof[i] = (my_rank - num_io_procs) * elements_per_pe + i;
                /* printf("my_rank %d compdof[%d]=%ld\n", my_rank, i, compdof[i]); */
            }

            /* Create the PIO decomposition for this test. */
            if (nc_def_decomp(iosysid, PIO_INT, NDIM2, &dimlen[1], elements_per_pe,
                              compdof, &ioid, 1, NULL, NULL)) PERR;
            free(compdof);

            /* Create some data on this processor. */
            if (!(my_data = malloc(elements_per_pe * sizeof(int)))) PERR;
            for (i = 0; i < elements_per_pe; i++)
                my_data[i] = my_rank * 10 + i;

            /* Write some data with distributed arrays. */
            if (nc_put_vard_int(ncid, varid, ioid, 0, my_data)) PERR;
            if (nc_close(ncid)) PERR;

            /* Reopen the file using netCDF integration. */
            {
                int ndims, nvars, ngatts, unlimdimid;
                nc_type xtype_in;
                char var_name_in[NC_MAX_NAME + 1];
                char dim_name_in[NC_MAX_NAME + 1];
                int natts_in;
                int dimids_in[NDIM3];
                size_t dim_len_in;

                /* Open the file. */
                if (nc_open(FILE_NAME, cmode[m], &ncid)) PERR;

                /* Check the file. */
                if (nc_inq(ncid, &ndims, &nvars, &ngatts, &unlimdimid)) PERR;
                if (ndims != 3 || nvars != 1 || ngatts != 0 ||
                    unlimdimid != 0) PERR;
                if (nc_inq_var(ncid, 0, var_name_in, &xtype_in, &ndims,
                               dimids_in, &natts_in)) PERR;
                if (strcmp(var_name_in, VAR_NAME) || xtype_in != NC_INT || ndims != NDIM3
                    || dimids_in[0] != 0 || dimids_in[1] != 1 || dimids_in[2] != 2 ||
                    natts_in != 0) PERR;
                if (nc_inq_dim(ncid, 0, dim_name_in, &dim_len_in)) PERR;
                if (strcmp(dim_name_in, DIM_NAME_UNLIMITED) || dim_len_in != 1) PERR;
                if (nc_inq_dim(ncid, 1, dim_name_in, &dim_len_in)) PERR;
                if (strcmp(dim_name_in, DIM_NAME_X) || dim_len_in != DIM_LEN_X) PERR;
                if (nc_inq_dim(ncid, 2, dim_name_in, &dim_len_in)) PERR;
                if (strcmp(dim_name_in, DIM_NAME_Y) || dim_len_in != DIM_LEN_Y) PERR;

                /* Read distributed arrays. */
                if (!(data_in = malloc(elements_per_pe * sizeof(int)))) PERR;
                if (nc_get_vard_int(ncid, varid, ioid, 0, data_in)) PERR;

                /* Check results. */
                for (i = 0; i < elements_per_pe; i++)
                    if (data_in[i] != my_data[i]) PERR;

                /* Close file. */
                if (nc_close(ncid)) PERR;

                /* Free resources. */
                free(data_in);
            }

            free(my_data);
            if (nc_free_decomp(ioid)) PERR;
          }
            if (nc_free_iosystem(iosysid)) PERR;
        }
    }
    if (!my_rank)
        PSUMMARIZE_ERR;

    /* Finalize MPI. */
    MPI_Finalize();
    PFINAL_RESULTS;
}
