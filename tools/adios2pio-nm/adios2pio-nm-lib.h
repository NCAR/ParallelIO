#ifndef _ADIOS2PIO_NM_LIB_H_ 
#define _ADIOS2PIO_NM_LIB_H_ 

#include <string>

using namespace std;

int ConvertBPToNC(string infilepath, string outfilename, string piotype);
void usage(string prgname, int mpirank);

#endif /* #ifndef _ADIOS2PIO_NM_LIB_H_ */
