#include <mpi.h>
#include "adios2pio-nm-lib.h"

int main (int argc, char *argv[])
{
    int ret = 0;

    MPI_Init(&argc, &argv);

    if (argc < 4) {
		int mpirank;
    	MPI_Comm_rank(MPI_COMM_WORLD, &mpirank);
        usage(argv[0],mpirank);
        return 1;
    }
    
    string infilepath  = argv[1];
    string outfilename = argv[2];
	string piotype     = argv[3];

	ret = ConvertBPToNC(infilepath,outfilename,piotype);

    MPI_Finalize();
    return ret;
}



#if 0

int main (int argc, char *argv[])
{
    int ret = 0;

	MPI_Comm w_comm = MPI_COMM_WORLD;
	int w_mpirank;
	int w_nproc;

    MPI_Init(&argc, &argv);
    MPI_Comm_set_errhandler(w_comm, MPI_ERRORS_RETURN);
    MPI_Comm_rank(w_comm, &w_mpirank);
    MPI_Comm_size(w_comm, &w_nproc);

    if (argc < 4) {
        usage(argv[0]);
        return 1;
    }
    
    string infilepath  = argv[1];
    string outfilename = argv[2];

	/* 
 	 * Check if the number of nodes is less than or equal to the number of BP files.
 	 * If not, create a new comm.
 	 *
 	 */
	int num_files = GetNumOfFiles(infilepath); 
	int io_proc   = 0;
	if (num_files<w_nproc) {
		fprintf(stderr, "Warning: #files: %d < #procs: %d\n",num_files,w_nproc); 
		fflush(stderr);
		if (w_mpirank<num_files) /* I/O nodes */
			io_proc = 1; 
		MPI_Comm_split(w_comm,io_proc,w_mpirank,&comm);
    	MPI_Comm_rank(comm, &mpirank);
    	MPI_Comm_size(comm, &nproc);
	} else {
		io_proc = 1;
		comm    = w_comm;
		mpirank = w_mpirank;
		nproc   = w_nproc;
	}
	

#ifdef TIMING
    /* Initialize the GPTL timing library. */
    if ((ret = GPTLinitialize ()))
        return ret;
#endif

    TimerInitialize();

    try {
		if (io_proc) {
        	enum PIO_IOTYPE pio_iotype = GetIOType(argv[3]);
        	InitPIO();
        	ConvertBPFile(infilepath, outfilename, pio_iotype);
        	PIOc_finalize(iosysid);
        	TimerReport(comm);
		}
		MPI_Barrier(w_comm);
    } catch (std::invalid_argument &e) {
        std::cout << e.what() << std::endl;
        usage(argv[0]);
        return 2;
    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
        return 3;
    }

    MPI_Finalize();
    TimerFinalize();

#ifdef TIMING
    /* Finalize the GPTL timing library. */
    if ((ret = GPTLfinalize ()))
        return ret;
#endif

    return ret;
}

#endif
