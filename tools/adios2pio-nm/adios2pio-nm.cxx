#include <mpi.h>
#ifdef TIMING
#include <gptl.h>
#endif
#include "adios2pio-nm-lib.h"

int main(int argc, char *argv[])
{
    int ret = 0;

    MPI_Init(&argc, &argv);

    int mpirank;
    MPI_Comm comm_in = MPI_COMM_WORLD;
    MPI_Comm_rank(comm_in, &mpirank);

    if (argc < 4)
    {
        if (!mpirank)
            usage_nm(argv[0]);
        return 1;
    }

    string infilepath  = argv[1];
    string outfilename = argv[2];
    string piotype     = argv[3];

#ifdef TIMING
    /* Initialize the GPTL timing library. */
    if ((ret = GPTLinitialize()))
        return ret;
#endif

    SetDebugOutput(0);
    ret = ConvertBPToNC(infilepath, outfilename, piotype, comm_in);

#ifdef TIMING
    /* Finalize the GPTL timing library. */
    if ((ret = GPTLfinalize()))
        return ret;
#endif

    MPI_Finalize();

    return ret;
}
