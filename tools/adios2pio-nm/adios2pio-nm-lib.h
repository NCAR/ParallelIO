#ifndef _ADIOS2PIO_NM_LIB_H_ 
#define _ADIOS2PIO_NM_LIB_H_ 

#include <string>
#include <mpi.h>

using namespace std;

int ConvertBPToNC(const string &infilepath,
                  const string &outfilename,
                  const string &piotype, const string &rearr, MPI_Comm comm_in);
int MConvertBPToNC(const string &bppdir, const string &piotype, const string &rearr,
                    MPI_Comm comm);
void SetDebugOutput(int val);

#endif /* #ifndef _ADIOS2PIO_NM_LIB_H_ */
