#include "config.h"
#include "mpi.h"
#include "pio_timer.h"
#include "pio_internal.h"

int mpi_mtimer_init(void )
{
    int mpi_is_initialized = false;
    MPI_Initialized(&mpi_is_initialized);
    if(!mpi_is_initialized){
        return PIO_EINTERNAL;
    }
    return PIO_NOERR;
}

int mpi_mtimer_finalize(void )
{
    /* Nothing to do here */
}

double mpi_mtimer_get_wtime(void )
{
    return MPI_Wtime();
}
