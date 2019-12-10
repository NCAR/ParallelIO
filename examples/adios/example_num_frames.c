#include <stdio.h>
#include <mpi.h>
#include <pio.h>
#ifdef TIMING
#include <gptl.h>
#endif

#define NUM_NETCDF_FLAVORS 5
#define NDIM 2
#define DIM_LEN 12
#define NUM_FRAMES 3

int main(int argc, char* argv[])
{
    int my_rank;
    int ntasks;
    int format[NUM_NETCDF_FLAVORS];
    int niotasks;
    int ioproc_stride = 1;
    int ioproc_start = 0;
    int dimid[NDIM];
    PIO_Offset elements_per_pe;
    int dim_len[1] = {DIM_LEN};
    int iosysid;
    int ncid;
    int varid;
    int wr_iodesc;
    PIO_Offset *compdof = NULL;
    int *buffer1 = NULL, *buffer2 = NULL, *buffer3 = NULL;
    char filename[NC_MAX_NAME + 1];
    int num_flavors = 0;

#ifdef TIMING
    GPTLinitialize();
#endif

    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &ntasks);

    if (ntasks != 4)
    {
        if (my_rank == 0)
            fprintf(stderr, "Number of processors must be 4!\n");

        MPI_Finalize();
        return 1;
    }

    niotasks = ntasks;
    PIOc_Init_Intracomm(MPI_COMM_WORLD, niotasks, ioproc_stride, ioproc_start,
                        PIO_REARR_SUBSET, &iosysid);

    elements_per_pe = DIM_LEN / ntasks;
    compdof = malloc(elements_per_pe * sizeof(PIO_Offset));

    /* Decomp : [1, 2, 3] [4, 5, 6] [7, 8, 9] [10, 11, 12] */
    for (int i = 0; i < elements_per_pe; i++)
        compdof[i] = my_rank * elements_per_pe + i + 1;

    PIOc_InitDecomp(iosysid, PIO_INT, 1, dim_len, (PIO_Offset)elements_per_pe,
                    compdof, &wr_iodesc, NULL, NULL, NULL);

    /* Buffer used to write 1st frame */
    buffer1 = malloc(elements_per_pe * sizeof(int));
    for (int i = 0; i < elements_per_pe; i++)
        buffer1[i] = compdof[i];

    /* Buffer used to write 2nd frame */
    buffer2 = malloc(elements_per_pe * sizeof(int));
    for (int i = 0; i < elements_per_pe; i++)
        buffer2[i] = compdof[i] + 100;

    /* Buffer used to write 3rd frame */
    buffer3 = malloc(elements_per_pe * sizeof(int));
    for (int i = 0; i < elements_per_pe; i++)
        buffer3[i] = compdof[i] + 200;

    free(compdof);

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

    for (int fmt = 0; fmt < num_flavors; fmt++)
    {
        sprintf(filename, "output_num_frames_%d.nc", fmt);

        PIOc_createfile(iosysid, &ncid, &(format[fmt]), filename, PIO_CLOBBER);

#if 0
        PIOc_def_dim(ncid, "time", NC_UNLIMITED, &dimid[0]);
#else
        PIOc_def_dim(ncid, "time", NUM_FRAMES, &dimid[0]);
#endif

        PIOc_def_dim(ncid, "row", DIM_LEN, &dimid[1]);
        PIOc_def_var(ncid, "foo", PIO_INT, NDIM, dimid, &varid);
        PIOc_enddef(ncid);

        /* This writes 1st frame, expected values: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12] */
        PIOc_setframe(ncid, varid, 0);
        PIOc_write_darray(ncid, varid, wr_iodesc, elements_per_pe, buffer1, NULL);

        /* This writes 2nd frame, expected values: [101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112] */
        PIOc_setframe(ncid, varid, 1);
        PIOc_write_darray(ncid, varid, wr_iodesc, elements_per_pe, buffer2, NULL);

        /* This writes 3rd frame, expected values: [201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212] */
        PIOc_setframe(ncid, varid, 2);
        PIOc_write_darray(ncid, varid, wr_iodesc, elements_per_pe, buffer3, NULL);
        PIOc_closefile(ncid);
    }

    free(buffer1);
    free(buffer2);
    free(buffer3);

    PIOc_freedecomp(iosysid, wr_iodesc);

    PIOc_finalize(iosysid);

    MPI_Finalize();

#ifdef TIMING
    GPTLfinalize();
#endif

    return 0;
}
