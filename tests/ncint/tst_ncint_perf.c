/* Test netcdf integration layer.

   This is a performance test of intercomm mode in PIO, using the
   netCDF integration layer.

   Ed Hartnett
   12/7/19
*/

#include "config.h"
#include <pio.h>
#include <sys/time.h>
#include "pio_err_macros.h"

#define FILE_PREFIX "tst_ncint_perf"
#define VAR_NAME "data_var"
#define DIM_NAME_UNLIMITED "dim_unlimited"
#define DIM_NAME_X "dim_x"
#define DIM_NAME_Y "dim_y"
#define DIM_LEN_X 3072
#define DIM_LEN_Y 1536
/* #define DIM_LEN_X 3 */
/* #define DIM_LEN_Y 4 */
#define NDIM2 2
#define NDIM3 3
#define NUM_TIMESTEPS 1
#define NUM_MODES 8


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
        printf("\n*** Testing netCDF integration PIO performance.\n");
    if (!my_rank)
        printf("*** testing simple intercomm use of netCDF integration layer...\n");
    {
        int ncid, ioid;
        int dimid[NDIM3], varid;
        int dimlen[NDIM3] = {NC_UNLIMITED, DIM_LEN_X, DIM_LEN_Y};
        int iosysid;
        size_t elements_per_pe;
        size_t *compdof; /* The decomposition mapping. */
        int *my_data;
        int num_io_procs;
        int i;
        int found_format;
        /* Turn on logging for PIO library. */
/*         PIOc_set_log_level(4); 
         if (!my_rank) 
         nc_set_log_level(3);  */
        if (ntasks <= 16)
            num_io_procs = 1;
        else if (ntasks <= 64)
            num_io_procs = 4;
        else if (ntasks <= 128)
            num_io_procs = 16;
        else if (ntasks <= 512)
            num_io_procs = 64;
        else if (ntasks <= 1024)
            num_io_procs = 128;
        else if (ntasks <= 2048)
            num_io_procs = 256;

        /* Initialize the intracomm. */
        if (nc_def_iosystem(MPI_COMM_WORLD, num_io_procs, 1, 0, PIO_REARR_BOX, &iosysid))
            PERR;

        {
            struct timeval starttime, endtime;
            long long startt, endt;
            long long delta;
            float num_megabytes = DIM_LEN_X * DIM_LEN_Y * sizeof(int) / (float)1000000 * NUM_TIMESTEPS;
            float delta_in_sec;
            float mb_per_sec;
#if NUM_MODES == 1
            int cmode[NUM_MODES] = {NC_PIO|NC_NETCDF4|NC_MPIIO};
            char mode_name[NUM_MODES][NC_MAX_NAME] = {"netcdf4 parallel           "};
            int expected_format[NUM_MODES] = {NC_PIO|NC_FORMAT_NETCDF4};
#elif NUM_MODES == 2
            int cmode[NUM_MODES] = {NC_PIO,
                                    NC_PIO|NC_64BIT_OFFSET};
            char mode_name[NUM_MODES][NC_MAX_NAME] = {"classic serial     ",
                                                      "64bit offset serial"};
            int expected_format[NUM_MODES] = {NC_PIO|NC_FORMAT_CLASSIC,
                                              NC_PIO|NC_FORMAT_64BIT_OFFSET};
#elif NUM_MODES == 3
            int cmode[NUM_MODES] = {NC_PIO,
                                    NC_PIO|NC_64BIT_OFFSET,
                                    NC_PIO|NC_64BIT_DATA};
            char mode_name[NUM_MODES][NC_MAX_NAME] = {"classic serial     ",
                                                      "64bit offset serial",
                                                      "64bit data serial"};
            int expected_format[NUM_MODES] = {NC_PIO|NC_FORMAT_CLASSIC,
                                              NC_PIO|NC_FORMAT_64BIT_OFFSET,
                                              NC_PIO|NC_FORMAT_64BIT_DATA};
#elif NUM_MODES == 4
            int cmode[NUM_MODES] = {NC_PIO,
                                    NC_PIO|NC_64BIT_OFFSET,
                                    NC_PIO|NC_64BIT_DATA,
                                    NC_PIO|NC_PNETCDF};
            char mode_name[NUM_MODES][NC_MAX_NAME] = {"classic serial     ",
                                                      "64bit offset serial",
                                                      "64bit data serial  ",
                                                      "classic pnetcdf        "};
            int expected_format[NUM_MODES] = {NC_PIO|NC_FORMAT_CLASSIC,
                                              NC_PIO|NC_FORMAT_64BIT_OFFSET,
                                              NC_PIO|NC_FORMAT_64BIT_DATA,
                                              NC_PIO|NC_FORMAT_CLASSIC};
#elif NUM_MODES == 6
            int cmode[NUM_MODES] = {NC_PIO,
                                    NC_PIO|NC_64BIT_OFFSET,
                                    NC_PIO|NC_64BIT_DATA,
                                    NC_PIO|NC_PNETCDF,
                                    NC_PIO|NC_PNETCDF|NC_64BIT_OFFSET,
                                    NC_PIO|NC_PNETCDF|NC_64BIT_DATA};
            char mode_name[NUM_MODES][NC_MAX_NAME] = {"classic serial     ",
                                                      "64bit offset serial",
                                                      "64bit data serial  ",
                                                      "classic pnetcdf        ",
                                                      "64bit offset pnetcdf   ",
                                                      "64bit data pnetcdf     "};
            int expected_format[NUM_MODES] = {NC_PIO|NC_FORMAT_CLASSIC,
                                              NC_PIO|NC_FORMAT_64BIT_OFFSET,
                                              NC_PIO|NC_FORMAT_64BIT_DATA,
                                              NC_PIO|NC_FORMAT_CLASSIC,
                                              NC_PIO|NC_FORMAT_64BIT_OFFSET,
                                              NC_PIO|NC_FORMAT_64BIT_DATA};
#elif NUM_MODES == 8
            int cmode[NUM_MODES] = {NC_PIO,
                                    NC_PIO|NC_64BIT_OFFSET,
                                    NC_PIO|NC_64BIT_DATA,
                                    NC_PIO|NC_PNETCDF,
                                    NC_PIO|NC_PNETCDF|NC_64BIT_OFFSET,
                                    NC_PIO|NC_PNETCDF|NC_64BIT_DATA,
                                    NC_PIO|NC_NETCDF4,
                                    NC_PIO|NC_NETCDF4|NC_MPIIO};
            char mode_name[NUM_MODES][NC_MAX_NAME] = {"classic serial         ",
                                                      "64bit offset serial    ",
                                                      "64bit data serial      ",
                                                      "classic pnetcdf        ",
                                                      "64bit offset pnetcdf   ",
                                                      "64bit data pnetcdf     ",
                                                      "netcdf4 serial         ",
                                                      "netcdf4 parallel       "};
            int expected_format[NUM_MODES] = {NC_PIO|NC_FORMAT_CLASSIC,
                                              NC_PIO|NC_FORMAT_64BIT_OFFSET,
                                              NC_PIO|NC_FORMAT_64BIT_DATA,
                                              NC_PIO|NC_FORMAT_CLASSIC,
                                              NC_PIO|NC_FORMAT_64BIT_OFFSET,
                                              NC_PIO|NC_FORMAT_64BIT_DATA,
                                              NC_PIO|NC_FORMAT_NETCDF4,
                                              NC_PIO|NC_FORMAT_NETCDF4};
#else

            int cmode[NUM_MODES] = {NC_PIO, 
                                    NC_PIO|NC_NETCDF4,
                                    NC_PIO|NC_NETCDF4|NC_MPIIO,
                                    NC_PIO|NC_PNETCDF};
            char mode_name[NUM_MODES][NC_MAX_NAME + 1] = {"classic serial   ",
                                                          "netCDF-4 serial  ",
                                                          "netCDF-4 parallel I/O",
                                                          "pnetcdf              "};

#endif
            int t, m;

            /* Print header. */
            if (!my_rank)
                printf("access,\t\t\tntasks,\tnio,\trearr,\ttime(s),\tdata size (MB),\t"
                       "performance(MB/s)\n");

            for (m = 0; m < NUM_MODES; m++)
            {
                /* Create a file with a 3D record var. */
                char filename[strlen(FILE_PREFIX)+16];
                sprintf(filename,"%s%d.nc",FILE_PREFIX,cmode[m]);
                /* Turn on logging for PIO library. */
//                PIOc_set_log_level(2); 
//                if (!my_rank) 
//                    nc_set_log_level(2); 
                if (nc_create(filename, cmode[m], &ncid)) PERR;
                if (nc_def_dim(ncid, DIM_NAME_UNLIMITED, dimlen[0], &dimid[0])) PERR;
                if (nc_def_dim(ncid, DIM_NAME_X, dimlen[1], &dimid[1])) PERR;
                if (nc_def_dim(ncid, DIM_NAME_Y, dimlen[2], &dimid[2])) PERR;
                if (nc_def_var(ncid, VAR_NAME, NC_INT, NDIM3, dimid, &varid)) PERR;
                if (nc_enddef(ncid)) PERR;

                /* Calculate a decomposition for distributed arrays. */
                elements_per_pe = DIM_LEN_X * DIM_LEN_Y / ntasks;
                /* printf("my_rank %d elements_per_pe %ld\n", my_rank, elements_per_pe); */

                if (!(compdof = malloc(elements_per_pe * sizeof(size_t))))
                    PERR;
                for (i = 0; i < elements_per_pe; i++)
                {
                    compdof[i] = my_rank * elements_per_pe + i;
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

                /* Start the clock. */
                gettimeofday(&starttime, NULL);

                /* Write some data with distributed arrays. */
                for (t = 0; t < NUM_TIMESTEPS; t++)
                    if (nc_put_vard_int(ncid, varid, ioid, t, my_data)) PERR;

                /* check the file format */
                if (nc_inq_format_extended(ncid, NULL, &found_format)) PERR;
                if (found_format != expected_format[m]) {
                        printf("expected format 0x%x found format 0x%x\n",expected_format[m], 
                           found_format);
                    PERR;
                }

                if (nc_close(ncid)) PERR;
//                PIOc_set_log_level(0); 
//                if (!my_rank) 
//                    nc_set_log_level(0); 
                /* Stop the clock. */
                gettimeofday(&endtime, NULL);

                /* Compute the time delta */
                startt = (1000000 * starttime.tv_sec) + starttime.tv_usec;
                endt = (1000000 * endtime.tv_sec) + endtime.tv_usec;
                delta = (endt - startt)/NUM_TIMESTEPS;
                delta_in_sec = (float)delta / 1000000;
                mb_per_sec = num_megabytes / delta_in_sec;
                if (my_rank == num_io_procs)
                    printf("%s,\t%d,\t%d,\t%d,\t%8.3f,\t%8.1f,\t%8.3f\n", mode_name[m],
                           ntasks, num_io_procs, 1, delta_in_sec, num_megabytes,
                           mb_per_sec);
                
                free(my_data);
                if (nc_free_decomp(ioid)) PERR;

            } /* next mode flag */
        }
        if (nc_free_iosystem(iosysid)) PERR;

    }
    if (!my_rank)
        PSUMMARIZE_ERR;

    /* Finalize MPI. */
    MPI_Finalize();
    PFINAL_RESULTS;
}
