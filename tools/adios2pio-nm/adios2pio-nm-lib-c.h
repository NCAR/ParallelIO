#ifndef _ADIOS2PIO_NM_LIB_C_H_
#define _ADIOS2PIO_NM_LIB_C_H_

#include <mpi.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int C_API_ConvertBPToNC(const char *infilepath, const char *outfilename, const char *piotype, int mem_opt, MPI_Comm comm_in);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _ADIOS2PIO_NM_LIB_C_H_ */
