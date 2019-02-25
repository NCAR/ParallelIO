#ifndef _ADIOS2PIO_NM_LIB_H_ 
#define _ADIOS2PIO_NM_LIB_H_ 

#include <string>
#include <mpi.h>

using namespace std;

int ConvertBPToNC(string infilepath, string outfilename, string piotype, MPI_Comm comm_in);
void usage_nm(string prgname);
void SetDebugOutput(int val);

#endif /* #ifndef _ADIOS2PIO_NM_LIB_H_ */
