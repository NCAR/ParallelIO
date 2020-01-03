/** @file
 *
 * PIO async message handling. This file contains the code which
 * runs on the IO nodes when async is in use. This code waits for
 * messages from the computation nodes, and responds to messages by
 * running the appropriate netCDF function.
 *
 * @author Ed Hartnett
 */

#include <pio_config.h>
#include <stdarg.h>
#include <pio.h>
#include <pio_internal.h>

#ifdef PIO_ENABLE_LOGGING
extern int my_rank;
extern int pio_log_level;
#endif /* PIO_ENABLE_LOGGING */

/* A global MPI communicator for waiting on async I/O service messages */
MPI_Comm pio_async_service_msg_comm = MPI_COMM_NULL;

/* Create the global MPI communicator for async service messaging.
 * This communicator is used by the async I/O service message handler to
 * communicate the iosystem id between the I/O procs. It is a dup of the
 * io_comm
 * @param io_comm The I/O communicator used for creating the async service
 * message communicator
 * @param msg_comm Pointer to the newly created (global) asynchronous I/O
 * message communicator
 * @returns PIO_NOERR on success, a pio error code otherwise
 */
int create_async_service_msg_comm(const MPI_Comm io_comm, MPI_Comm *msg_comm)
{
    int ret = PIO_NOERR;

    assert(msg_comm && (pio_async_service_msg_comm == MPI_COMM_NULL));

    LOG((2, "Creating global async I/O service msg comm"));
    *msg_comm = MPI_COMM_NULL;

    if(io_comm != MPI_COMM_NULL)
    {
        ret = MPI_Comm_dup(io_comm, &pio_async_service_msg_comm);
        if(ret != MPI_SUCCESS)
        {
            return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
        }
        *msg_comm = pio_async_service_msg_comm;
    }
    return ret;
}

/* Delete/free the global MPI communicator used for asynchronous I/O service
 * messaging
 */
void delete_async_service_msg_comm(void )
{
    if(pio_async_service_msg_comm != MPI_COMM_NULL)
    {
        LOG((2, "Deleting global async I/O service msg comm"));
        MPI_Comm_free(&pio_async_service_msg_comm);
        pio_async_service_msg_comm = MPI_COMM_NULL;
    }
}

char pio_async_msg_sign[PIO_MAX_MSGS][PIO_MAX_ASYNC_MSG_ARGS];
int init_async_msgs_sign(void )
{
    /* Format for function signatures:
     * i => integer value
     * f => float value
     * o => PIO_Offset value
     * b => bytes/char value
     * s => integer, length/size of a string or array that follows
     * S => PIO_Offset, length/size of a string or array that follows
     * m => integer, length/size of a string or array that follows,
     *      the array needs a malloc on the recv side
     * M => PIO_Offset, length/size of a string or array that follows,
     *      the array needs a malloc on the recv side
     * c => string, needs to be prefixed by L/M
     * I => integer array, needs to be prefixed by L/M
     * F => float array, needs to be prefixed by L/M
     * O => PIO_Offset array, needs to be prefixed by L/M
     * B => byte array, needs to be prefixed by L/M
     */
    strncpy(pio_async_msg_sign[PIO_MSG_INVALID], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_OPEN_FILE message sends 1 int/len + 1 string + 1 int + 1 int */
    strncpy(pio_async_msg_sign[PIO_MSG_OPEN_FILE], "scii", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_CREATE_FILE message sends 1 int/len + 1 string + 1 int + 1 int */
    strncpy(pio_async_msg_sign[PIO_MSG_CREATE_FILE], "scii", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_INQ_ATT message sends 
     * 1 int + 1 int + 1 int/len + 1 string +1 bool + 1 bool
     */
    strncpy(pio_async_msg_sign[PIO_MSG_INQ_ATT], "iiscbb", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_INQ_FORMAT sends 1 int + 1 char/byte */
    strncpy(pio_async_msg_sign[PIO_MSG_INQ_FORMAT], "ib", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_INQ_VARID sends 1 int + 1 int/len + 1 string */
    strncpy(pio_async_msg_sign[PIO_MSG_INQ_VARID], "isc", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_DEF_VAR sends 
     * 1 int + 1 int/len + 1 string + 1 int + 1 int + 1 int/len + 1 int array
     */
    strncpy(pio_async_msg_sign[PIO_MSG_DEF_VAR], "isciisI", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_INQ_VAR sends 2 ints and 5 bytes/chars */
    strncpy(pio_async_msg_sign[PIO_MSG_INQ_VAR], "iibbbbb", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_PUT_ATT_DOUBLE is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_PUT_ATT_DOUBLE], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_PUT_ATT_INT is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_PUT_ATT_INT], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_RENAME_ATT sends 2 ints + 2 * (1 int/len + 1 string) */
    strncpy(pio_async_msg_sign[PIO_MSG_RENAME_ATT], "iiscsc", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_DEL_ATT sends 2 ints + 1 len/int + 1 string */
    strncpy(pio_async_msg_sign[PIO_MSG_DEL_ATT], "iisc", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_INQ sends 1 int and 4 bytes/chars */
    strncpy(pio_async_msg_sign[PIO_MSG_INQ], "ibbbb", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_GET_ATT_TEXT is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_GET_ATT_TEXT], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_GET_ATT_SHORT is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_GET_ATT_SHORT], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_GET_ATT_LONG is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_GET_ATT_LONG], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_REDEF sends 1 int */
    strncpy(pio_async_msg_sign[PIO_MSG_REDEF], "i", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_SET_FILL sends 3 ints */
    strncpy(pio_async_msg_sign[PIO_MSG_SET_FILL], "iii", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_ENDDEF sends 1 int */
    strncpy(pio_async_msg_sign[PIO_MSG_ENDDEF], "i", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_RENAME_VAR sends 2 ints, 1 len/int, 1 string */
    strncpy(pio_async_msg_sign[PIO_MSG_RENAME_VAR], "iisc", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_PUT_ATT_SHORT is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_PUT_ATT_SHORT], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_PUT_ATT_TEXT is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_PUT_ATT_TEXT], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_INQ_ATTNAME sends 3 ints, 1 char/byte */
    strncpy(pio_async_msg_sign[PIO_MSG_INQ_ATTNAME], "iiib", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_GET_ATT_ULONGLONG is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_GET_ATT_ULONGLONG], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_GET_ATT_USHORT is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_GET_ATT_USHORT], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_PUT_ATT_ULONGLONG is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_PUT_ATT_ULONGLONG], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_PUT_ATT_USHORT is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_PUT_ATT_USHORT], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_GET_ATT_UINT is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_GET_ATT_UINT], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_GET_ATT_LONGLONG is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_GET_ATT_LONGLONG], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_PUT_ATT_SCHAR is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_PUT_ATT_SCHAR], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_PUT_ATT_FLOAT is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_PUT_ATT_FLOAT], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_RENAME_DIM sends 2 ints + 1 len/int + 1 string */
    strncpy(pio_async_msg_sign[PIO_MSG_RENAME_DIM], "iisc", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_GET_ATT_LONG is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_GET_ATT_LONG], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_INQ_DIM sends 2 ints and 2 bytes/chars */
    strncpy(pio_async_msg_sign[PIO_MSG_INQ_DIM], "iibb", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_INQ_DIMID sends 1 int + 1 int/len + 1 string + 1 bytes/char */
    strncpy(pio_async_msg_sign[PIO_MSG_INQ_DIMID], "iscb", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_PUT_ATT_USHORT is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_PUT_ATT_USHORT], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_GET_ATT_FLOAT is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_GET_ATT_FLOAT], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_SYNC sends 1 int */
    strncpy(pio_async_msg_sign[PIO_MSG_SYNC], "i", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_PUT_ATT_LONGLONG is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_PUT_ATT_LONGLONG], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_PUT_ATT_UINT is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_PUT_ATT_UINT], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_GET_ATT_SCHAR is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_GET_ATT_SCHAR], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_INQ_ATTID sends 2 ints, 1 int/len + 1 string + 1 byte/char */
    strncpy(pio_async_msg_sign[PIO_MSG_INQ_ATTID], "iiscb", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_DEF_DIM sends 1 int + 1 len/int + 1 string + 1 int */
    strncpy(pio_async_msg_sign[PIO_MSG_DEF_DIM], "isci", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_GET_ATT_INT is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_GET_ATT_INT], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_GET_ATT_DOUBLE is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_GET_ATT_DOUBLE], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_PUT_ATT_UCHAR is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_PUT_ATT_UCHAR], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_GET_ATT_UCHAR is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_GET_ATT_UCHAR], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_PUT_VARS_UCHAR is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_PUT_VARS_UCHAR], "", PIO_MAX_ASYNC_MSG_ARGS);
    /* PIO_MSG_GET_VAR1_SCHAR is not used */
    strncpy(pio_async_msg_sign[PIO_MSG_GET_VAR1_SCHAR], "", PIO_MAX_ASYNC_MSG_ARGS);
    
    /*  PIO_MSG_GET_VARS_ULONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARS_ULONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARM_UCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARM_UCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARM_SCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARM_SCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARS_SHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARS_SHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR_DOUBLE  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR_DOUBLE ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARA_DOUBLE  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARA_DOUBLE ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR_INT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR_INT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR_USHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR_USHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARS_USHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARS_USHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARA_TEXT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARA_TEXT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARS_ULONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARS_ULONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARA_INT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARA_INT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARM  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARM ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR1_FLOAT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR1_FLOAT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR1_SHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR1_SHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARS_INT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARS_INT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARS_UINT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARS_UINT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR_TEXT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR_TEXT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARM_DOUBLE  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARM_DOUBLE ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARM_UCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARM_UCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR_USHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR_USHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARS_SCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARS_SCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARA_USHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARA_USHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR1_LONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR1_LONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARA_UCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARA_UCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARM_SHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARM_SHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR1_LONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR1_LONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARS_LONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARS_LONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR1_USHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR1_USHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR_SHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR_SHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARA_INT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARA_INT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR_FLOAT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR_FLOAT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR1_USHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR1_USHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARA_TEXT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARA_TEXT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARM_TEXT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARM_TEXT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARS_UCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARS_UCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARM_USHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARM_USHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR1_LONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR1_LONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARS_USHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARS_USHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR_LONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR_LONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR1_DOUBLE  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR1_DOUBLE ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR_ULONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR_ULONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR_INT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR_INT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARA_UINT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARA_UINT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR_LONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR_LONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARS_LONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARS_LONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR_SCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR_SCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR_UINT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR_UINT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARA_USHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARA_USHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR_LONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR_LONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARA_SHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARA_SHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARS_SHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARS_SHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARA_UINT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARA_UINT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARA_SCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARA_SCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARM_ULONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARM_ULONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR1_UCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR1_UCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARM_INT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARM_INT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARS_SCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARS_SCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARA_LONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARA_LONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR1  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR1 ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR1_INT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR1_INT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR1_ULONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR1_ULONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR_UCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR_UCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARA_FLOAT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARA_FLOAT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARA_UCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARA_UCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARS_FLOAT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARS_FLOAT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR1_FLOAT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR1_FLOAT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARM_FLOAT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARM_FLOAT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR1_TEXT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR1_TEXT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARS_TEXT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARS_TEXT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARM_LONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARM_LONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARS_LONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARS_LONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARS_DOUBLE  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARS_DOUBLE ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR1  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR1 ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR_UINT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR_UINT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARA_LONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARA_LONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARA  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARA ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR_DOUBLE  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR_DOUBLE ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARA_SCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARA_SCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR_FLOAT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR_FLOAT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR1_UINT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR1_UINT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARS_UINT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARS_UINT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR1_ULONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR1_ULONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARM_UINT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARM_UINT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR1_UINT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR1_UINT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR1_INT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR1_INT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARA_FLOAT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARA_FLOAT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARM_TEXT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARM_TEXT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARS_FLOAT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARS_FLOAT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR1_TEXT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR1_TEXT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARA_SHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARA_SHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR1_SCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR1_SCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARA_ULONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARA_ULONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARM_DOUBLE  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARM_DOUBLE ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARM_INT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARM_INT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARA  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARA ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARA_LONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARA_LONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARM_UINT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARM_UINT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARM  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARM ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR1_DOUBLE  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR1_DOUBLE ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARS_DOUBLE  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARS_DOUBLE ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARA_LONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARA_LONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR_ULONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR_ULONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARM_SCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARM_SCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARA_ULONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARA_ULONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR_SHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR_SHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARM_FLOAT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARM_FLOAT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR_TEXT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR_TEXT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARS_INT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARS_INT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR1_LONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR1_LONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARM_LONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARM_LONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARM_USHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARM_USHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR1_SHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR1_SHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARS_LONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARS_LONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARM_LONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARM_LONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARS_TEXT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARS_TEXT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARA_DOUBLE  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARA_DOUBLE ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARS  sends
     *  3 ints + 
     *  1 char/byte + 1 int/len + 1 PIO_Offset array +
     *  1 char/byte + 1 int/len + 1 PIO_Offset array +
     *  1 char/byte + 1 int/len + 1 PIO_Offset array +
     *  1 int + 1 PIO_Offset + 1 PIO_Offset +
     *  1 int/len +
     *  1 Byte array (that requires mem alloc on recv)
     */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARS ], "iiibsObsObsOiooMB", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR_UCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR_UCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR1_UCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR1_UCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VAR_LONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VAR_LONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARS  sends
     * 3 ints +
     *  1 char/byte + 1 int/len + 1 PIO_Offset array +
     *  1 char/byte + 1 int/len + 1 PIO_Offset array +
     *  1 char/byte + 1 int/len + 1 PIO_Offset array +
     *  1 int + 1 PIO_Offset + 1 PIO_Offset
     */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARS ], "iiibsObsObsOioo", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARM_SHORT  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARM_SHORT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VARM_ULONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VARM_ULONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_VARM_LONGLONG  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_VARM_LONGLONG ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR_SCHAR  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR_SCHAR ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_ATT_UBYTE  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_ATT_UBYTE ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_ATT_STRING  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_ATT_STRING ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_ATT_STRING  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_ATT_STRING ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_ATT_UBYTE  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_ATT_UBYTE ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_INQ_VAR_FILL  sends 2 ints + 1 pio_offset + 2 bytes/chars */
     strncpy(pio_async_msg_sign[ PIO_MSG_INQ_VAR_FILL ], "iiobb", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_DEF_VAR_FILL  sends 3 ints + 1 pio_offset + 1 byte/char +
     *  1 int/len + 1 byte array */
     strncpy(pio_async_msg_sign[ PIO_MSG_DEF_VAR_FILL ], "iiiobMB", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_DEF_VAR_DEFLATE sends
     *  5 ints */
     strncpy(pio_async_msg_sign[ PIO_MSG_DEF_VAR_DEFLATE ], "iiiii", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_INQ_VAR_DEFLATE  sends 2 ints + 3 * (1 char/byte + 1 int)*/
     strncpy(pio_async_msg_sign[ PIO_MSG_INQ_VAR_DEFLATE ], "iibibibi", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_INQ_VAR_SZIP  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_INQ_VAR_SZIP ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_DEF_VAR_FLETCHER32  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_DEF_VAR_FLETCHER32 ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_INQ_VAR_FLETCHER32  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_INQ_VAR_FLETCHER32 ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_DEF_VAR_CHUNKING  sends
     *  4 ints, 1 char/byte + 1 int/len + 1 PIO_Offset array */
     strncpy(pio_async_msg_sign[ PIO_MSG_DEF_VAR_CHUNKING ], "iiiibsO", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_INQ_VAR_CHUNKING  sends
     *  2 ints + 2 bytes/chars */
     strncpy(pio_async_msg_sign[ PIO_MSG_INQ_VAR_CHUNKING ], "iibb", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_DEF_VAR_ENDIAN  sends 3 ints */
     strncpy(pio_async_msg_sign[ PIO_MSG_DEF_VAR_ENDIAN ], "iii", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_INQ_VAR_ENDIAN  sends 2 ints + 1 char */
     strncpy(pio_async_msg_sign[ PIO_MSG_INQ_VAR_ENDIAN ], "iib", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_SET_CHUNK_CACHE  sends 2 ints, 2 pio-offsets, 1 float */
     strncpy(pio_async_msg_sign[ PIO_MSG_SET_CHUNK_CACHE ], "iioof", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_CHUNK_CACHE sends 2 int, 3 char/byte */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_CHUNK_CACHE ], "iibbb", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_SET_VAR_CHUNK_CACHE  sends 2 ints, 2 pio_offsets, 1 float */
     strncpy(pio_async_msg_sign[ PIO_MSG_SET_VAR_CHUNK_CACHE ], "iioof", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_VAR_CHUNK_CACHE  sends 2 ints + 3 chars/bytes */
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_VAR_CHUNK_CACHE ], "iibbb", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_INITDECOMP_DOF  sends
     *  2 ints + 1 int/len + 1 int array +
     *  1 int/len + 1 pio_offset array (needs mem alloc on recv) +
     *  1 char/byte + 1 int 
     *  + 1 char/byte + 1 int/len + 1 pio_offset array +
     *  + 1 char/byte + 1 int/len + 1 pio_offset array */
     strncpy(pio_async_msg_sign[ PIO_MSG_INITDECOMP_DOF ], "iisImObibsObsO", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_WRITEDARRAY  is not used */
     strncpy(pio_async_msg_sign[ PIO_MSG_WRITEDARRAY ], "", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_WRITEDARRAYMULTI  sends
     *  1 int + 1 int
     *  1 int/len + 1 int array (needs malloc) +
     *  1 int +
     *  1 pio_offset +
     *  1 pio_offset/len + 1 array of chars/bytes (needs malloc) +
     *  1 char/byte +
     *  1 int/len + 1 int array + 1 char/byte (needs malloc) +
     *  1 char/byte +
     *  1 int/len + 1 byte/char array (needs malloc) +
     *  1 int
     */
     strncpy(pio_async_msg_sign[ PIO_MSG_WRITEDARRAYMULTI ], "iimIioMBbmIbmBi", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_SETFRAME  sends 3 ints */
     strncpy(pio_async_msg_sign[ PIO_MSG_SETFRAME ], "iii", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_ADVANCEFRAME  sends 2 ints */
     strncpy(pio_async_msg_sign[ PIO_MSG_ADVANCEFRAME ], "ii", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_READDARRAY  sends 3 ints*/
     strncpy(pio_async_msg_sign[ PIO_MSG_READDARRAY ], "iii", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_SETERRORHANDLING  sends 1 int + 1 char/byte */
     strncpy(pio_async_msg_sign[ PIO_MSG_SETERRORHANDLING ], "ib", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_FREEDECOMP  sends 2 ints */
     strncpy(pio_async_msg_sign[ PIO_MSG_FREEDECOMP ], "ii", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_CLOSE_FILE  sends 1 int */
     strncpy(pio_async_msg_sign[ PIO_MSG_CLOSE_FILE ], "i", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_DELETE_FILE  sends 1 int/len + 1 string */
     strncpy(pio_async_msg_sign[ PIO_MSG_DELETE_FILE ], "sc", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_FINALIZE  sends 1 int */
     strncpy(pio_async_msg_sign[ PIO_MSG_FINALIZE ], "i", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_GET_ATT  sends
     *  2 ints + 1 int/len + 1 string +
     *  2 ints + 2 pio_offsets
     *  1 int +  1 pio_offset*/
     strncpy(pio_async_msg_sign[ PIO_MSG_GET_ATT ], "iisciiooio", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_PUT_ATT  sends
     *  2 ints + 1 int/len + 1 stirng +
     *  1 int + 2 offsets + 1 int + 1 offset +
     *  1 offset/len + 1 char/byte array(needs malloc)*/
     strncpy(pio_async_msg_sign[ PIO_MSG_PUT_ATT ], "iisciooioMB", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_COPY_ATT  sends
     *  2 ints + 1 int/len + 1 stirng + 2 ints */
     strncpy(pio_async_msg_sign[ PIO_MSG_COPY_ATT ], "iiscii", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_INQ_TYPE  sends 2 ints + 2 chars/bytes */
     strncpy(pio_async_msg_sign[ PIO_MSG_INQ_TYPE ], "iibb", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_INQ_UNLIMDIMS  sends 1 int and 2 chars/bytes */
     strncpy(pio_async_msg_sign[ PIO_MSG_INQ_UNLIMDIMS ], "ibb", PIO_MAX_ASYNC_MSG_ARGS);
    /*  PIO_MSG_EXIT  is a local message, never sent between compute and I/O procs  */
     strncpy(pio_async_msg_sign[ PIO_MSG_EXIT ], "", PIO_MAX_ASYNC_MSG_ARGS);
    return PIO_NOERR;
}

static int send_async_msg_valist(iosystem_desc_t *ios, int msg, va_list args)
{
    int mpierr = MPI_SUCCESS;
    char *fmt = pio_async_msg_sign[msg];
    int nargs = strlen(fmt);
    int sz = 0, msz = 0;

    assert(ios && (msg > PIO_MSG_INVALID) && (msg < PIO_MAX_MSGS));

    for(int i=0; i<nargs; i++)
    {
        if(mpierr == MPI_SUCCESS)
        {
            if(fmt[i] == 'c')
            {
                if(sz == 0)
                {
                    assert(msz > 0);
                    sz = msz;
                }
                char *str = va_arg(args, char *);
                mpierr = MPI_Bcast((void *)str, sz, MPI_CHAR, ios->compmaster, ios->intercomm);
                sz = 0;
                msz = 0;
            }
            else if(fmt[i] == 's')
            {
                /* Length/Size of the first string/array that follows it */
                int iarg = va_arg(args, int);
                sz = iarg;
                assert(sz > 0);
                mpierr = MPI_Bcast(&iarg, 1, MPI_INT, ios->compmaster, ios->intercomm);
            }
            else if(fmt[i] == 'S')
            {
                /* Length/Size of the first string/array that follows it */
                PIO_Offset oarg = va_arg(args, PIO_Offset);
                /* MPI only allows int counts */
                sz = (int )oarg;
                assert(sz > 0);
                mpierr = MPI_Bcast(&oarg, 1, MPI_OFFSET, ios->compmaster, ios->intercomm);
            }
            else if(fmt[i] == 'm')
            {
                /* Length of the first string/array that follows it */
                int iarg = va_arg(args, int);
                msz = iarg;
                assert(msz > 0);
                mpierr = MPI_Bcast(&iarg, 1, MPI_INT, ios->compmaster, ios->intercomm);
            }
            else if(fmt[i] == 'M')
            {
                /* Length of the first string/array that follows it */
                PIO_Offset oarg = va_arg(args, PIO_Offset);
                /* MPI only allows int counts */
                msz = (int )oarg;
                assert(msz > 0);
                mpierr = MPI_Bcast(&oarg, 1, MPI_OFFSET, ios->compmaster, ios->intercomm);
            }
            else if(fmt[i] == 'i')
            {
                int iarg = va_arg(args, int);
                mpierr = MPI_Bcast(&iarg, 1, MPI_INT, ios->compmaster, ios->intercomm);
            }
            else if(fmt[i] == 'I')
            {
                if(sz == 0)
                {
                    assert(msz > 0);
                    sz = msz;
                }
                int *iargp = va_arg(args, int *);
                assert(sz > 0);
                mpierr = MPI_Bcast((void *)iargp, sz, MPI_INT, ios->compmaster, ios->intercomm);
                sz = 0;
                msz = 0;
            }
            else if(fmt[i] == 'f')
            {
                /* float is promoted to double in varargs */
                float farg = (float )va_arg(args, double);
                mpierr = MPI_Bcast(&farg, 1, MPI_FLOAT, ios->compmaster, ios->intercomm);
            }
            else if(fmt[i] == 'F')
            {
                if(sz == 0)
                {
                    assert(msz > 0);
                    sz = msz;
                }
                float *fargp = va_arg(args, float *);
                assert(sz > 0);
                mpierr = MPI_Bcast((void *)fargp, sz, MPI_FLOAT, ios->compmaster, ios->intercomm);
                sz = 0;
                msz = 0;
            }
            else if(fmt[i] == 'o')
            {
                PIO_Offset oarg = va_arg(args, PIO_Offset);
                mpierr = MPI_Bcast(&oarg, 1, MPI_OFFSET, ios->compmaster, ios->intercomm);
            }
            else if(fmt[i] == 'O')
            {
                if(sz == 0)
                {
                    assert(msz > 0);
                    sz = msz;
                }
                PIO_Offset *oargp = va_arg(args, PIO_Offset *);
                assert(sz > 0);
                mpierr = MPI_Bcast((void *)oargp, sz, MPI_OFFSET, ios->compmaster, ios->intercomm);
                sz = 0;
                msz = 0;
            }
            else if(fmt[i] == 'b')
            {
                /* FIXME: Individual bytes are sent as chars while a byte array is
                 * sent as an array of bytes. Distinguish explicitly between chars
                 * and bytes
                 */
                /* char is promoted to int in varargs */
                char carg = (char )va_arg(args, int);
                mpierr = MPI_Bcast(&carg, 1, MPI_CHAR, ios->compmaster, ios->intercomm);
            }
            else if(fmt[i] == 'B')
            {
                if(sz == 0)
                {
                    assert(msz > 0);
                    sz = msz;
                }
                char *cargp = va_arg(args, char *);
                assert(sz > 0);
                mpierr = MPI_Bcast(cargp, sz, MPI_BYTE, ios->compmaster, ios->intercomm);
                sz = 0;
                msz = 0;
            }
            else
            {
                LOG((1, "Invalid fmt for arg"));
                assert(0);
            }
        }
    }
    if(mpierr != MPI_SUCCESS)
    {
        LOG((1, "Error bcasting (send) async msg valist "));
        return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
    }

    return PIO_NOERR;
}

static int recv_async_msg_valist(iosystem_desc_t *ios, int msg, va_list args)
{
    int mpierr = MPI_SUCCESS;
    char *fmt = pio_async_msg_sign[msg];
    int nargs = strlen(fmt);
    int sz = 0, msz = 0;

    assert(ios && (msg > PIO_MSG_INVALID) && (msg < PIO_MAX_MSGS));

    for(int i=0; i<nargs; i++)
    {
        if(mpierr == MPI_SUCCESS)
        {
            if(fmt[i] == 'c')
            {
                char *str = NULL;
                if(sz != 0)
                {
                    str = va_arg(args, char *);
                }
                else
                {
                    assert(msz > 0);
                    sz = msz;
                    char **strp = va_arg(args, char **);
                    *strp = (char *)malloc(sz * sizeof(char ));
                    str = *strp;
                    if(!str)
                    {
                        return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                                        "Error receiving/parsing asynchronous message (msg=%d) in iosystem (iosysid=%d). Out of memory allocating %lld bytes for receiving a string", msg, ios->iosysid, (long long int) (sz * sizeof(char )));
                    }
                }
                mpierr = MPI_Bcast((void *)str, sz, MPI_CHAR, ios->compmaster, ios->intercomm);
                sz = 0;
                msz = 0;
            }
            else if(fmt[i] == 's')
            {
                /* Length of the first character string that follows it */
                int *iargp = va_arg(args, int *);
                mpierr = MPI_Bcast(iargp, 1, MPI_INT, ios->compmaster, ios->intercomm);
                sz = *iargp;
                assert(sz > 0);
            }
            else if(fmt[i] == 'S')
            {
                /* Length of the first character string that follows it */
                PIO_Offset *oargp = va_arg(args, PIO_Offset *);
                mpierr = MPI_Bcast(oargp, 1, MPI_OFFSET, ios->compmaster, ios->intercomm);
                /* MPI only allows int counts */
                sz = (int )*oargp;
                assert(sz > 0);
            }
            else if(fmt[i] == 'm')
            {
                /* Length of the first character string that follows it */
                int *iargp = va_arg(args, int *);
                mpierr = MPI_Bcast(iargp, 1, MPI_INT, ios->compmaster, ios->intercomm);
                msz = *iargp;
                assert(msz > 0);
            }
            else if(fmt[i] == 'M')
            {
                /* Length of the first character string that follows it */
                PIO_Offset *oargp = va_arg(args, PIO_Offset *);
                mpierr = MPI_Bcast(oargp, 1, MPI_OFFSET, ios->compmaster, ios->intercomm);
                /* MPI only allows int counts */
                msz = (int ) *oargp;
                assert(msz > 0);
            }
            else if(fmt[i] == 'i')
            {
                int *iargp = va_arg(args, int *);
                mpierr = MPI_Bcast(iargp, 1, MPI_INT, ios->compmaster, ios->intercomm);
            }
            else if(fmt[i] == 'I')
            {
                int *iargp = NULL;
                if(sz != 0)
                {
                    iargp = va_arg(args, int *);
                }
                else
                {
                    assert(msz > 0);
                    sz = msz;
                    int **iargpp = va_arg(args, int **);
                    *iargpp = (int *)malloc(sz * sizeof(int));
                    iargp = *iargpp;
                    if(!iargp)
                    {
                        return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                                        "Error receiving/parsing asynchronous message (msg=%d) in iosystem (iosysid=%d). Out of memory allocating %lld bytes for receiving an int array", msg, ios->iosysid, (long long int) (sz * sizeof(int )));
                    }
                }
                mpierr = MPI_Bcast(iargp, sz, MPI_INT, ios->compmaster, ios->intercomm);
                sz = 0;
                msz = 0;
            }
            else if(fmt[i] == 'f')
            {
                float *fargp = va_arg(args, float *);
                mpierr = MPI_Bcast(fargp, 1, MPI_FLOAT, ios->compmaster, ios->intercomm);
            }
            else if(fmt[i] == 'F')
            {
                float *fargp = NULL;
                if(sz != 0)
                {
                    fargp = va_arg(args, float *);
                }
                else
                {
                    assert(msz > 0);
                    sz = msz;
                    float **fargpp = va_arg(args, float **);
                    *fargpp = (float *)malloc(sz * sizeof(float));
                    fargp = *fargpp;
                    if(!fargp)
                    {
                        return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                                        "Error receiving/parsing asynchronous message (msg=%d) in iosystem (iosysid=%d). Out of memory allocating %lld bytes for receiving a float array", msg, ios->iosysid, (long long int) (sz * sizeof(float )));
                    }
                }
                mpierr = MPI_Bcast(fargp, sz, MPI_FLOAT, ios->compmaster, ios->intercomm);
                sz = 0;
                msz = 0;
            }
            else if(fmt[i] == 'o')
            {
                PIO_Offset *oargp = va_arg(args, PIO_Offset *);
                mpierr = MPI_Bcast((void *)oargp, 1, MPI_OFFSET, ios->compmaster, ios->intercomm);
            }
            else if(fmt[i] == 'O')
            {
                PIO_Offset *oargp = NULL;
                if(sz != 0)
                {
                    oargp = va_arg(args, PIO_Offset *);
                }
                else
                {
                    assert(msz > 0);
                    sz = msz;
                    PIO_Offset **oargpp = va_arg(args, PIO_Offset **);
                    *oargpp = (PIO_Offset *)malloc(sz * sizeof(PIO_Offset));
                    oargp = *oargpp;
                    if(!oargp)
                    {
                        return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                                        "Error receiving/parsing asynchronous message (msg=%d) in iosystem (iosysid=%d). Out of memory allocating %lld bytes for receiving an offset array", msg, ios->iosysid, (long long int) (sz * sizeof(PIO_Offset)));
                    }
                }
                mpierr = MPI_Bcast((void *)oargp, sz, MPI_OFFSET, ios->compmaster, ios->intercomm);
                sz = 0;
                msz = 0;
            }
            else if(fmt[i] == 'b')
            {
                /* FIXME: Individual bytes are recvd as chars while a byte array is
                 * recvd as an array of bytes. Distinguish explicitly between chars
                 * and bytes
                 */
                char *cargp = va_arg(args, char *);
                mpierr = MPI_Bcast(cargp, 1, MPI_CHAR, ios->compmaster, ios->intercomm);
            }
            else if(fmt[i] == 'B')
            {
                char *cargp = NULL;
                if(sz != 0)
                {
                    cargp = va_arg(args, char *);
                }
                else
                {
                    assert(msz > 0);
                    sz = msz;
                    char **cargpp = va_arg(args, char **);
                    *cargpp = (char *)malloc(sz * sizeof(char));
                    cargp = *cargpp;
                    if(!cargp)
                    {
                        return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                                        "Error receiving/parsing asynchronous message (msg=%d) in iosystem (iosysid=%d). Out of memory allocating %lld bytes for receiving a byte array", msg, ios->iosysid, (long long int) (sz * sizeof(char)));
                    }
                }
                mpierr = MPI_Bcast((void *)cargp, sz, MPI_BYTE, ios->compmaster, ios->intercomm);
                sz = 0;
                msz = 0;
            }
            else
            {
                LOG((1, "Invalid fmt for arg"));
                assert(0);
            }
        }
    }
    if(mpierr != MPI_SUCCESS)
    {
        LOG((1, "Error bcasting (recv) async msg valist "));
        return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
    }
    return PIO_NOERR;
}

static int send_async_msg_hdr(iosystem_desc_t *ios, int msg, int seq_num, int prev_msg)
{
    int mpierr = MPI_SUCCESS;

    assert(ios && ((msg > PIO_MSG_INVALID) && (msg < PIO_MAX_MSGS)) && !ios->ioproc);
    assert((prev_msg >= PIO_MSG_INVALID) && (prev_msg < PIO_MAX_MSGS));
    if(ios->compmaster == MPI_ROOT)
    {
        mpierr = MPI_Send(&msg, 1, MPI_INT, ios->ioroot, PIO_ASYNC_MSG_HDR_TAG, ios->union_comm);
    }

    if(mpierr == MPI_SUCCESS)
    {
        mpierr = MPI_Bcast(&seq_num, 1, MPI_INT, ios->compmaster, ios->intercomm);
    }
    if(mpierr == MPI_SUCCESS)
    {
        mpierr = MPI_Bcast(&prev_msg, 1, MPI_INT, ios->compmaster, ios->intercomm);
    }
    if(mpierr != MPI_SUCCESS)
    {
        LOG((1, "Error bcasting MPI error code"));
        return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
    }
    return PIO_NOERR;
}

int send_async_msg(iosystem_desc_t *ios, int msg, ...)
{
    int mpierr = MPI_SUCCESS, mpierr2 = MPI_SUCCESS;
    int ret = PIO_NOERR;
    
    assert(ios && (msg > PIO_MSG_INVALID) && (msg < PIO_MAX_MSGS));
    assert(strlen(pio_async_msg_sign[msg]) > 0);
    assert(ios->async);


    if(!ios->ioproc)
    {
        int seq_num = ios->async_ios_msg_info.seq_num;
        int prev_msg = ios->async_ios_msg_info.prev_msg;

        /* Send message header */
        ret = send_async_msg_hdr(ios, msg, seq_num, prev_msg);
        if(ret != PIO_NOERR)
        {
            LOG((1, "Could not bcast async msg header"));
            return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                            "Sending asynchronous message (msg=%d, seq_num=%d, prev_msg=%d) failed on iosystem (iosysid=%d). Internal error sending message header.", msg, seq_num, prev_msg, ios->iosysid);
        } 

        /* Send message */
        va_list args;
        va_start(args, msg);
        ret = send_async_msg_valist(ios, msg, args);
        if(ret != PIO_NOERR)
        {
            LOG((1, "Could not bcast async msg body"));
            return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                            "Sending asynchronous message (msg=%d, seq_num=%d, prev_msg=%d) failed on iosystem (iosysid=%d). Internal error sending message arguments.", msg, seq_num, prev_msg, ios->iosysid);
        } 
        va_end(args);

        ios->async_ios_msg_info.seq_num++;
        ios->async_ios_msg_info.prev_msg = msg;
    }

    /* Bcast error code to all procs (union_comm) from compute proc root */
    mpierr2 = MPI_Bcast(&mpierr, 1, MPI_INT, ios->comproot, ios->my_comm);
    if(mpierr2 != MPI_SUCCESS)
    {
        LOG((1, "Error bcasting MPI error code"));
        return check_mpi(ios, NULL, mpierr2, __FILE__, __LINE__);
    }
    if(mpierr != MPI_SUCCESS)
    {
        LOG((1, "Error sending async msg"));
        return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
    }

    return PIO_NOERR;
}

static int recv_async_msg_hdr(iosystem_desc_t *ios, int msg, int eseq_num, int eprev_msg)
{
    int mpierr = MPI_SUCCESS;

    assert(ios && ((msg > PIO_MSG_INVALID) && (msg < PIO_MAX_MSGS)) && ios->ioproc);
    assert(eseq_num >= PIO_MSG_START_SEQ_NUM);
    assert((eprev_msg >= PIO_MSG_INVALID) && (eprev_msg < PIO_MAX_MSGS));

    /* Message header includes message type, msg, that is already
     * received
     */

    int seq_num, prev_msg;
    mpierr = MPI_Bcast(&seq_num, 1, MPI_INT, ios->compmaster, ios->intercomm);
    if(mpierr == MPI_SUCCESS)
    {
        assert(seq_num == eseq_num);
        mpierr = MPI_Bcast(&prev_msg, 1, MPI_INT, ios->compmaster, ios->intercomm);
    }
    if(mpierr != MPI_SUCCESS)
    {
        LOG((1, "Error bcasting MPI error code"));
        return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
    }
    assert(prev_msg == eprev_msg);
    return PIO_NOERR;
}

int recv_async_msg(iosystem_desc_t *ios, int msg, ...)
{
    int ret = PIO_NOERR;
    
    assert(ios && (msg > PIO_MSG_INVALID) && (msg < PIO_MAX_MSGS));
    assert(strlen(pio_async_msg_sign[msg]) > 0);
    assert(ios->async && ios->ioproc);

    /* Recv message header */

    /* Expected seq number and parent/previous msg */
    int eseq_num = ios->async_ios_msg_info.seq_num;
    int eprev_msg = ios->async_ios_msg_info.prev_msg;

    ret = recv_async_msg_hdr(ios, msg, eseq_num, eprev_msg);
    if(ret != PIO_NOERR)
    {
        LOG((1, "Could not bcast (recv) async msg header"));
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Receiving asynchronous message (msg=%d, expected seq_num = %d, expected prev msg=%d) failed on iosystem (iosysid=%d). Internal error receiving message header", msg, eseq_num, eprev_msg, ios->iosysid);
    } 

    /* Recv message */
    va_list args;
    va_start(args, msg);
    ret = recv_async_msg_valist(ios, msg, args);
    if(ret != PIO_NOERR)
    {
        LOG((1, "Could not bcast (recv) async msg body"));
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Receiving asynchronous message (msg=%d, expected seq_num = %d, expected prev msg=%d) failed on iosystem (iosysid=%d). Internal error receiving message arguments", msg, eseq_num, eprev_msg, ios->iosysid);
    } 
    va_end(args);
    ios->async_ios_msg_info.seq_num++;
    ios->async_ios_msg_info.prev_msg = msg;

    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to handle nc_inq_type*()
 * functions.
 *
 * @param ios pointer to the iosystem info.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int inq_type_handler(iosystem_desc_t *ios)
{
    int ncid;
    int xtype;
    char name_present, size_present;
    char *namep = NULL, name[PIO_MAX_NAME + 1];
    PIO_Offset *sizep = NULL, size;
    int ret;

    LOG((1, "inq_type_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INQ_TYPE, &ret, &ncid, &xtype,
        &name_present, &size_present);
    if(ret != PIO_NOERR)
    {
        LOG((1, "Error receiving async msg for PIO_MSG_INQ_TYPE"));
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ_TYPE, on iosystem (iosysid=%d)", ios->iosysid);
    }

    /* Handle null pointer issues. */
    if (name_present)
        namep = name;
    if (size_present)
        sizep = &size;

    /* Call the function. */
    if ((ret = PIOc_inq_type(ncid, xtype, namep, sizep)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ_TYPE, on iosystem (iosysid=%d). Unable to inquire name/size of type=%x in file (%s, ncid=%d)", ios->iosysid, xtype, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "inq_type_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to find netCDF file
 * format.
 *
 * @param ios pointer to the iosystem info.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int inq_format_handler(iosystem_desc_t *ios)
{
    int ncid;
    int *formatp = NULL, format;
    char format_present;
    int ret;

    LOG((1, "inq_format_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INQ_FORMAT, &ret, &ncid, &format_present);
    if(ret != PIO_NOERR)
    {
        LOG((1, "Error received async msg for PIO_MSG_INQ_FORMAT"));
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ_FORMAT, on iosystem (iosysid=%d)", ios->iosysid);
    }

    /* Manage NULL pointers. */
    if (format_present)
        formatp = &format;

    /* Call the function. */
    if ((ret = PIOc_inq_format(ncid, formatp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ_FORMAT, on iosystem (iosysid=%d). Unable to inquire the binary format of file (%s, ncid=%d)", ios->iosysid, pio_get_fname_from_file_id(ncid), ncid);
    }

    if (formatp)
        LOG((2, "inq_format_handler format = %d", *formatp));
    LOG((1, "inq_format_handler succeeded!"));

    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to set the file fill mode.
 *
 * @param ios pointer to the iosystem info.
 * @returns 0 for success, error code otherwise.
 * @internal
 * @author Ed Hartnett
 */
int set_fill_handler(iosystem_desc_t *ios)
{
    int ncid;
    int fillmode;
    int old_modep_present;
    int old_mode, *old_modep = NULL;
    int ret;

    LOG((1, "set_fill_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_SET_FILL, &ret, &ncid, &fillmode, &old_modep_present);
    if(ret != PIO_NOERR)
    {
        LOG((1, "Error receiving async message for PIO_MSG_SET_FILL"));
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_SET_FILL, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((2, "set_fill_handler got parameters ncid = %d fillmode = %d old_modep_present = %d",
         ncid, fillmode, old_modep_present));

    /* Manage NULL pointers. */
    if (old_modep_present)
        old_modep = &old_mode;

    /* Call the function. */
    if ((ret = PIOc_set_fill(ncid, fillmode, old_modep)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_SET_FILL, on iosystem (iosysid=%d). Unable to set fillvalue mode in file (%s, ncid=%d)", ios->iosysid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "set_fill_handler succeeded!"));

    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to create a netCDF file.
 *
 * @param ios pointer to the iosystem info.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int create_file_handler(iosystem_desc_t *ios)
{
    int ncid;
    int len;
    int iotype;
    int mode;
    int ret;

    LOG((1, "create_file_handler comproot = %d", ios->comproot));
    assert(ios);

    char filename[PIO_MAX_NAME+1];
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_CREATE_FILE, &ret, &len, filename, &iotype, &mode);
    if(ret != PIO_NOERR)
    {
        LOG((1, "create_file_handler() failed, unable to receive async msg"));
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_CREATE_FILE, on iosystem (iosysid=%d)", ios->iosysid);
    }

    /* Call the create file function. */
    if ((ret = PIOc_createfile(ios->iosysid, &ncid, &iotype, filename, mode)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_CREATE_FILE, on iosystem (iosysid=%d). Unable to create file (%s) using %s (%d) iotype", ios->iosysid, filename, pio_iotype_to_string(iotype), iotype);
    }

    LOG((1, "create_file_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to close a netCDF file. It is
 * only ever run on the IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int close_file_handler(iosystem_desc_t *ios)
{
    int ncid;
    int ret;

    LOG((1, "close_file_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_CLOSE_FILE, &ret, &ncid);
    if(ret != PIO_NOERR)
    {
        LOG((1, "Error receiving async msg for PIO_MSG_CLOSE_FILE"));
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_CLOSE_FILE, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "create_file_handler got parameter ncid = %d", ncid));

    /* Call the close file function. */
    if ((ret = PIOc_closefile(ncid)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_CLOSE_FILE, on iosystem (iosysid=%d). Unable to close file (%s, ncid=%d)", ios->iosysid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "close_file_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to inq a netCDF file. It is
 * only ever run on the IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int inq_handler(iosystem_desc_t *ios)
{
    int ncid;
    int ndims, nvars, ngatts, unlimdimid;
    int *ndimsp = NULL, *nvarsp = NULL, *ngattsp = NULL, *unlimdimidp = NULL;
    char ndims_present, nvars_present, ngatts_present, unlimdimid_present;
    int ret;

    LOG((1, "inq_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INQ, &ret, &ncid, &ndims_present,
        &nvars_present, &ngatts_present, &unlimdimid_present);
    if(ret != PIO_NOERR)
    {
        LOG((1, "Error receiving async msg for PIO_MSG_INQ"));
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "inq_handler ndims_present = %d nvars_present = %d ngatts_present = %d unlimdimid_present = %d",
         ndims_present, nvars_present, ngatts_present, unlimdimid_present));

    /* NULLs passed in to any of the pointers in the original call
     * need to be matched with NULLs here. Assign pointers where
     * non-NULL pointers were passed in. */
    if (ndims_present)
        ndimsp = &ndims;
    if (nvars_present)
        nvarsp = &nvars;
    if (ngatts_present)
        ngattsp = &ngatts;
    if (unlimdimid_present)
        unlimdimidp = &unlimdimid;

    /* Call the inq function to get the values. */
    if ((ret = PIOc_inq(ncid, ndimsp, nvarsp, ngattsp, unlimdimidp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ, on iosystem (iosysid=%d). Unable to inquire number of dimensions/variables/attributes/unlimited_dimension in file (%s, ncid=%d)", ios->iosysid, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to inq unlimited dimension
 * ids of a netCDF file. It is only ever run on the IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int inq_unlimdims_handler(iosystem_desc_t *ios)
{
    int ncid;
    int nunlimdims;
    int unlimdimids;
    int *nunlimdimsp = NULL, *unlimdimidsp = NULL;
    bool nunlimdimsp_present = false, unlimdimidsp_present = false;
    int ret;

    LOG((1, "inq_unlimdims_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INQ_UNLIMDIMS, &ret, &ncid,
        &nunlimdimsp_present, &unlimdimidsp_present);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ_UNLIMDIMS, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "inq_unlimdims_handler nunlimdimsp_present = %d unlimdimidsp_present = %d",
         nunlimdimsp_present, unlimdimidsp_present));

    /* NULLs passed in to any of the pointers in the original call
     * need to be matched with NULLs here. Assign pointers where
     * non-NULL pointers were passed in. */
    if (nunlimdimsp_present)
        nunlimdimsp = &nunlimdims;
    if (unlimdimidsp_present)
        unlimdimidsp = &unlimdimids;

    /* Call the inq function to get the values. */
    if ((ret = PIOc_inq_unlimdims(ncid, nunlimdimsp, unlimdimidsp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ_UNLIMDIMS, on iosystem (iosysid=%d). Unable to inquire unlimited dimension info in file (%s, ncid=%d)", ios->iosysid, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/** Do an inq_dim on a netCDF dimension. This function is only run on
 * IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @param msg the message sent my the comp root task.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int inq_dim_handler(iosystem_desc_t *ios, int msg)
{
    int ncid;
    int dimid;
    char name_present, len_present;
    char *dimnamep = NULL;
    PIO_Offset *dimlenp = NULL;
    char dimname[PIO_MAX_NAME + 1];
    PIO_Offset dimlen;

    int ret;

    LOG((1, "inq_dim_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, msg, &ret, &ncid, &dimid, &name_present, &len_present);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ_DIM, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((2, "inq_handler name_present = %d len_present = %d", name_present,
         len_present));

    /* Set the non-null pointers. */
    if (name_present)
        dimnamep = dimname;
    if (len_present)
        dimlenp = &dimlen;

    /* Call the inq function to get the values. */
    if ((ret = PIOc_inq_dim(ncid, dimid, dimnamep, dimlenp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ_DIM, on iosystem (iosysid=%d). Unable to inquire info about dimension (dimid=%d) in file (%s, ncid=%d)", ios->iosysid, dimid, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/** Do an inq_dimid on a netCDF dimension name. This function is only
 * run on IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int inq_dimid_handler(iosystem_desc_t *ios)
{
    int ncid;
    int *dimidp = NULL, dimid;
    int id_present;
    int ret;
    int namelen;
    char name[PIO_MAX_NAME + 1];

    LOG((1, "inq_dimid_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INQ_DIMID, &ret, &ncid, &namelen, name, &id_present);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ_DIMID, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "inq_dimid_handler ncid = %d namelen = %d name = %s id_present = %d",
         ncid, namelen, name, id_present));

    /* Set non-null pointer. */
    if (id_present)
        dimidp = &dimid;

    /* Call the inq_dimid function. */
    if ((ret = PIOc_inq_dimid(ncid, name, dimidp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ_DIMID, on iosystem (iosysid=%d). Unable to inquire dimension id for dimension (dimension name=%s) in file (%s, ncid=%d)", ios->iosysid, (namelen > 0) ? name : "UNKNOWN", pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/** Handle attribute inquiry operations. This code only runs on IO
 * tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @param msg the message sent my the comp root task.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int inq_att_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    int ret;
    char name[PIO_MAX_NAME + 1];
    int namelen = PIO_MAX_NAME + 1;
    nc_type xtype, *xtypep = NULL;
    PIO_Offset len, *lenp = NULL;
    char xtype_present, len_present;

    LOG((1, "inq_att_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INQ_ATT, &ret, &ncid, &varid,
        &namelen, name, &xtype_present, &len_present);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ_ATT, on iosystem (iosysid=%d)", ios->iosysid);
    }

    /* Match NULLs in collective function call. */
    if (xtype_present)
        xtypep = &xtype;
    if (len_present)
        lenp = &len;

    /* Call the function to learn about the attribute. */
    if ((ret = PIOc_inq_att(ncid, varid, name, xtypep, lenp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ_ATT, on iosystem (iosysid=%d). Unable to inquire type/length of attribute (name=%s) of variable (name=%s, varid=%d) in file (%s, ncid=%d)", ios->iosysid, name, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/** Handle attribute inquiry operations. This code only runs on IO
 * tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @param msg the message sent my the comp root task.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int inq_attname_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    int attnum;
    char name[PIO_MAX_NAME + 1], *namep = NULL;
    char name_present;
    int ret;

    LOG((1, "inq_att_name_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INQ_ATTNAME, &ret,
        &ncid, &varid, &attnum, &name_present);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ_ATTNAME, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((2, "inq_attname_handler got ncid = %d varid = %d attnum = %d name_present = %d",
         ncid, varid, attnum, name_present));

    /* Match NULLs in collective function call. */
    if (name_present)
        namep = name;

    /* Call the function to learn about the attribute. */
    if ((ret = PIOc_inq_attname(ncid, varid, attnum, namep)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ_ATTNAME, on iosystem (iosysid=%d). Unable to inquire name of attribute with id=%d of variable (name=%s, varid=%d) in file (%s, ncid=%d)", ios->iosysid, attnum, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/** Handle attribute inquiry operations. This code only runs on IO
 * tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @param msg the message sent my the comp root task.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int inq_attid_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    char name[PIO_MAX_NAME + 1];
    int namelen = PIO_MAX_NAME + 1;
    int id, *idp = NULL;
    char id_present;
    int ret;

    LOG((1, "inq_attid_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INQ_ATTID, &ret, &ncid, &varid, &namelen, name, &id_present);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ_ATTID, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((2, "inq_attid_handler got ncid = %d varid = %d id_present = %d",
         ncid, varid, id_present));

    /* Match NULLs in collective function call. */
    if (id_present)
        idp = &id;

    /* Call the function to learn about the attribute. */
    if ((ret = PIOc_inq_attid(ncid, varid, name, idp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ_ATTID, on iosystem (iosysid=%d). Unable to inquire id of attribute with name=%s of variable (name=%s, varid=%d) in file (%s, ncid=%d)", ios->iosysid, name, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/** Handle attribute operations. This code only runs on IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @param msg the message sent my the comp root task.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int att_put_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    int ret;
    char name[PIO_MAX_NAME + 1];
    int namelen;
    PIO_Offset attlen;  /* Number of elements in att array. */
    nc_type atttype;    /* Type of att in file. */
    PIO_Offset atttype_len; /* Length in bytes of one elementy of type atttype. */
    nc_type memtype;    /* Type of att data in memory. */
    PIO_Offset memtype_len; /* Length of element of memtype. */
    void *op = NULL;
    PIO_Offset op_sz;

    LOG((1, "att_put_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_PUT_ATT, &ret, &ncid, &varid,
        &namelen, name, &atttype, &attlen, &atttype_len, &memtype, &memtype_len,
        &op_sz, &op);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_PUT_ATT, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "att_put_handler ncid = %d varid = %d namelen = %d name = %s"
         "atttype = %d attlen = %d atttype_len = %d memtype = %d memtype_len = 5d",
         ncid, varid, namelen, name, atttype, attlen, atttype_len, memtype, memtype_len));

    /* Call the function to write the attribute. */
    ret = PIOc_put_att_tc(ncid, varid, name, atttype, attlen, memtype, op);

    /* Free resources. */
    free(op);

    if (ret)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_PUT_ATT, on iosystem (iosysid=%d). Unable to put attribute with name=%s of variable (name=%s, varid=%d) in file (%s, ncid=%d)", ios->iosysid, name, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((2, "att_put_handler complete!"));
    return PIO_NOERR;
}

/** Copy attribute handler. This code only runs on IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @param msg the message sent my the comp root task.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Jayesh Krishna
 */
int att_copy_handler(iosystem_desc_t *ios)
{
    int incid, oncid;
    int ivarid, ovarid;
    int ret = PIO_NOERR;
    char name[PIO_MAX_NAME + 1];
    int namelen;

    LOG((1, "Starting att_copy_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_COPY_ATT, &ret, &incid, &ivarid,
        &namelen, name, &oncid, &ovarid);
    if(ret != PIO_NOERR){
      return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                      "Error receiving asynchronous message, PIO_MSG_COPY_ATT, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "att_copy_handler incid = %d ivarid = %d namelen = %d name = %s oncid = %d ovarid = %d",
         incid, ivarid, namelen, name, oncid, ovarid));

    /* Call the function to write the attribute. */
    ret = PIOc_copy_att(incid, ivarid, name, oncid, ovarid);
    if(ret){
      return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                      "Error processing asynchronous message, PIO_MSG_COPY_ATT, on iosystem (iosysid=%d). Unable to copy attribute with name=%s of variable %s (varid=%d) from file %s (ncid=%d) to file %s (ncid=%d)", ios->iosysid, name, pio_get_vname_from_file_id(incid, ivarid), ivarid, pio_get_fname_from_file_id(incid), incid, pio_get_fname_from_file_id(oncid), oncid);
    }

    LOG((2, "Finished att_copy_handler"));
    return PIO_NOERR;
}

/** Handle attribute operations. This code only runs on IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @param msg the message sent my the comp root task.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int att_get_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    char name[PIO_MAX_NAME + 1];
    int namelen;
    PIO_Offset attlen;
    nc_type atttype;        /* Type of att in file. */
    PIO_Offset atttype_len; /* Length in bytes of an element of attype. */
    nc_type memtype;        /* Type of att in memory. */
    PIO_Offset memtype_len; /* Length in bytes of an element of memype. */
    int *ip;
    int iotype;
    int ret;

    LOG((1, "att_get_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_GET_ATT, &ret, &ncid, &varid,
        &namelen, name, &iotype, &atttype, &attlen, &atttype_len,
        &memtype, &memtype_len);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_GET_ATT, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "att_get_handler ncid = %d varid = %d namelen = %d name = %s iotype = %d"
         " atttype = %d attlen = %d atttype_len = %d memtype = %d memtype_len = %d",
         ncid, varid, namelen, name, iotype, atttype, attlen, atttype_len, memtype, memtype_len));

    /* Allocate space for the attribute data. */
    if (!(ip = malloc(attlen * memtype_len)))
    {
        return pio_err(ios, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_GET_ATT on iosystem (iosysid=%d). Out of memory allocating %lld bytes for attribute (name=%s) data of variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, (unsigned long long) (attlen * memtype_len), name, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    /* Call the function to read the attribute. */
    ret = PIOc_get_att_tc(ncid, varid, name, memtype, ip);

    /* Free resources. */
    free(ip);

    if (ret)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_GET_ATT on iosystem (iosysid=%d). Unable to get attribute (name=%s) data of variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, name, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/** Handle var put operations. This code only runs on IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int put_vars_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    PIO_Offset typelen;  /* Length (in bytes) of this type. */
    nc_type xtype;       /* Type of the data being written. */
    char start_present;  /* Zero if user passed a NULL start. */
    char count_present;  /* Zero if user passed a NULL count. */
    char stride_present; /* Zero if user passed a NULL stride. */
    PIO_Offset *startp = NULL;
    PIO_Offset *countp = NULL;
    PIO_Offset *stridep = NULL;
    int ndims = 0;           /* Number of dimensions. */
    void *buf = NULL;           /* Buffer for data storage. */
    PIO_Offset buf_sz = 0;
    PIO_Offset num_elem; /* Number of data elements in the buffer. */
    int ret = PIO_NOERR;

    LOG((1, "put_vars_handler"));
    assert(ios);

    PIO_Offset start[PIO_MAX_DIMS], count[PIO_MAX_DIMS], stride[PIO_MAX_DIMS];
    int nstart=0, ncount=0, nstride=0;

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_PUT_VARS, &ret,
        &ncid, &varid, &ndims,
        &start_present, &nstart, start,
        &count_present, &ncount, count,
        &stride_present, &nstride, stride,
        &xtype, &num_elem, &typelen,
        &buf_sz, &buf);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_PUT_VARS, on iosystem (iosysid=%d)", ios->iosysid);
    }

    LOG((1, "put_vars_handler ncid = %d varid = %d ndims = %d "
         "start_present = %d count_present = %d stride_present = %d xtype = %d "
         "num_elem = %d typelen = %d", ncid, varid, ndims, start_present, count_present,
         stride_present, xtype, num_elem, typelen));

    /* Set the non-NULL pointers. */
    if (start_present)
        startp = start;
    if (count_present)
        countp = count;
    if (stride_present)
        stridep = stride;
    
    /* Call the function to write the data. No need to check return
     * values, they are bcast to computation tasks inside function. */
    switch(xtype)
    {
    case NC_BYTE:
        ret = PIOc_put_vars_schar(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_CHAR:
        ret = PIOc_put_vars_text(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_SHORT:
        ret = PIOc_put_vars_short(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_INT:
        ret = PIOc_put_vars_int(ncid, varid, startp, countp, stridep, buf);
        break;
    case PIO_LONG_INTERNAL:
        ret = PIOc_put_vars_long(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_FLOAT:
        ret = PIOc_put_vars_float(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_DOUBLE:
        ret = PIOc_put_vars_double(ncid, varid, startp, countp, stridep, buf);
        break;
#ifdef _NETCDF4
    case NC_UBYTE:
        ret = PIOc_put_vars_uchar(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_USHORT:
        ret = PIOc_put_vars_ushort(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_UINT:
        ret = PIOc_put_vars_uint(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_INT64:
        ret = PIOc_put_vars_longlong(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_UINT64:
        ret = PIOc_put_vars_ulonglong(ncid, varid, startp, countp, stridep, buf);
        break;
        /* case NC_STRING: */
        /*      PIOc_put_vars_string(ncid, varid, startp, countp, */
        /*                                stridep, (void *)buf); */
        /*      break; */
        /*    default:*/
        /* PIOc_put_vars(ncid, varid, startp, countp, */
        /*                   stridep, buf); */
#endif /* _NETCDF4 */
    }

    free(buf);

    if (ret)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_PUT_VARS on iosystem (iosysid=%d). Unable to put variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/** Handle var get operations. This code only runs on IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int get_vars_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    int ret = PIO_NOERR;
    PIO_Offset typelen; /** Length (in bytes) of this type. */
    nc_type xtype; /** 
                    * Type of the data being written. */
    PIO_Offset start[PIO_MAX_DIMS];
    PIO_Offset count[PIO_MAX_DIMS];
    PIO_Offset stride[PIO_MAX_DIMS];
    char start_present;
    char count_present;
    char stride_present;
    int nstart=0, ncount=0, nstride=0;
    PIO_Offset *startp = NULL, *countp = NULL, *stridep = NULL;
    int ndims = 0; /** Number of dimensions. */
    void *buf = NULL; /** Buffer for data storage. */
    PIO_Offset num_elem = 0; /** Number of data elements in the buffer. */

    LOG((1, "get_vars_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_GET_VARS, &ret,
        &ncid, &varid, &ndims,
        &start_present, &nstart, start,
        &count_present, &ncount, count,
        &stride_present, &nstride, stride,
        &xtype, &num_elem, &typelen);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_GET_VARS, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "get_vars_handler ncid = %d varid = %d ndims = %d "
         "stride_present = %d xtype = %d num_elem = %d typelen = %d",
         ncid, varid, ndims, stride_present, xtype, num_elem, typelen));

    /* Allocate room for our data. */
    if (!(buf = malloc(num_elem * typelen)))
    {
        return pio_err(ios, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_GET_VARS, on iosystem (iosysid=%d). Out of memory allocating %lld bytes for getting data for variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, (unsigned long long) (num_elem * typelen), pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    /* Set the non-NULL pointers. */
    if (start_present)
        startp = start;

    if (count_present)
        countp = count;
    
    if (stride_present)
        stridep = stride;

    /* Call the function to read the data. */
    switch(xtype)
    {
    case NC_BYTE:
        ret = PIOc_get_vars_schar(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_CHAR:
        ret = PIOc_get_vars_text(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_SHORT:
        ret = PIOc_get_vars_short(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_INT:
        ret = PIOc_get_vars_int(ncid, varid, startp, countp, stridep, buf);
        break;
    case PIO_LONG_INTERNAL:
        ret = PIOc_get_vars_long(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_FLOAT:
        ret = PIOc_get_vars_float(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_DOUBLE:
        ret = PIOc_get_vars_double(ncid, varid, startp, countp, stridep, buf);
        break;
#ifdef _NETCDF4
    case NC_UBYTE:
        ret = PIOc_get_vars_uchar(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_USHORT:
        ret = PIOc_get_vars_ushort(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_UINT:
        ret = PIOc_get_vars_uint(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_INT64:
        ret = PIOc_get_vars_longlong(ncid, varid, startp, countp, stridep, buf);
        break;
    case NC_UINT64:
        ret = PIOc_get_vars_ulonglong(ncid, varid, startp, countp, stridep, buf);
        break;
        /* case NC_STRING: */
        /*      PIOc_get_vars_string(ncid, varid, startp, countp, */
        /*                                stridep, (void *)buf); */
        /*      break; */
        /*    default:*/
        /* PIOc_get_vars(ncid, varid, startp, countp, */
        /*                   stridep, buf); */
#endif /* _NETCDF4 */
    }

    /* Free resourses. */
    free(buf);
    
    if (ret)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_GET_VARS on iosystem (iosysid=%d). Unable to get variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "get_vars_handler succeeded!"));
    return PIO_NOERR;
}

/**
 * Do an inq_var on a netCDF variable. This function is only run on
 * IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, error code otherwise.
 */
int inq_var_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    char name_present, xtype_present, ndims_present, dimids_present, natts_present;
    char name[PIO_MAX_NAME + 1], *namep = NULL;
    nc_type xtype, *xtypep = NULL;
    int *ndimsp = NULL, *dimidsp = NULL, *nattsp = NULL;
    int ndims, dimids[PIO_MAX_DIMS], natts;
    int ret;

    LOG((1, "inq_var_handler"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INQ_VAR, &ret, &ncid, &varid,
        &name_present, &xtype_present, &ndims_present,
        &dimids_present, &natts_present);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ_VAR, on iosystem (iosysid=%d)", ios->iosysid);
    }

    LOG((2,"inq_var_handler ncid = %d varid = %d name_present = %d xtype_present = %d ndims_present = %d "
         "dimids_present = %d natts_present = %d",
         ncid, varid, name_present, xtype_present, ndims_present, dimids_present, natts_present));

    /* Set the non-NULL pointers. */
    if (name_present)
        namep = name;
    if (xtype_present)
        xtypep = &xtype;
    if (ndims_present)
        ndimsp = &ndims;
    if (dimids_present)
        dimidsp = dimids;
    if (natts_present)
        nattsp = &natts;

    /* Call the inq function to get the values. */
    if ((ret = PIOc_inq_var(ncid, varid, namep, PIO_MAX_NAME + 1, xtypep, ndimsp, dimidsp, nattsp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ_VAR on iosystem (iosysid=%d). Unable to inquire name/type/number of dimensions/number of attributes about variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    if (ndims_present)
        LOG((2, "inq_var_handler ndims = %d", ndims));

    return PIO_NOERR;
}

/**
 * Do an inq_var_chunking on a netCDF variable. This function is only
 * run on IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, error code otherwise.
 */
int inq_var_chunking_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    char storage_present, chunksizes_present;
    int storage, *storagep = NULL;
    PIO_Offset chunksizes[PIO_MAX_DIMS], *chunksizesp = NULL;
    int ret;

    assert(ios);
    LOG((1, "inq_var_chunking_handler"));

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INQ_VAR_CHUNKING, &ret,
        &ncid, &varid, &storage_present, &chunksizes_present);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ_VAR_CHUNKING, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((2,"inq_var_handler ncid = %d varid = %d storage_present = %d chunksizes_present = %d",
         ncid, varid, storage_present, chunksizes_present));

    /* Set the non-NULL pointers. */
    if (storage_present)
        storagep = &storage;
    if (chunksizes_present)
        chunksizesp = chunksizes;

    /* Call the inq function to get the values. */
    if ((ret = PIOc_inq_var_chunking(ncid, varid, storagep, chunksizesp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ_VAR on iosystem (iosysid=%d). Unable to inquire chunking parameters about variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/**
 * Do an inq_var_fill on a netCDF variable. This function is only
 * run on IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, error code otherwise.
 */
int inq_var_fill_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    char fill_mode_present, fill_value_present;
    PIO_Offset type_size;
    int fill_mode, *fill_modep = NULL;
    PIO_Offset *fill_value, *fill_valuep = NULL;
    int ret = PIO_NOERR;

    assert(ios);
    LOG((1, "inq_var_fill_handler"));

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INQ_VAR_FILL, &ret,
        &ncid, &varid, &type_size, &fill_mode_present, &fill_value_present);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ_VAR_FILL, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((2,"inq_var_fill_handler ncid = %d varid = %d type_size = %lld, fill_mode_present = %d fill_value_present = %d",
         ncid, varid, type_size, fill_mode_present, fill_value_present));

    /* If we need to, alocate storage for fill value. */
    if (fill_value_present)
        if (!(fill_value = malloc(type_size)))
        {
            return pio_err(ios, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                            "Error processing asynchronous message, PIO_MSG_INQ_VAR_FILL on iosystem (iosysid=%d). Out of memory allocating %lld bytes for fillvalue associated with variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, (unsigned long long) (type_size), pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
        }

    /* Set the non-NULL pointers. */
    if (fill_mode_present)
        fill_modep = &fill_mode;
    if (fill_value_present)
        fill_valuep = fill_value;

    /* Call the inq function to get the values. */
    ret = PIOc_inq_var_fill(ncid, varid, fill_modep, fill_valuep);

    /* Free fill value storage if we allocated some. */
    if (fill_value_present)
        free(fill_value);

    if (ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ_VAR_FILL on iosystem (iosysid=%d). Unable to inquire fillvalue for variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/**
 * Do an inq_var_endian on a netCDF variable. This function is only
 * run on IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, error code otherwise.
 */
int inq_var_endian_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    char endian_present;
    int endian = 0, *endianp = NULL;
    int ret;

    assert(ios);
    LOG((1, "inq_var_endian_handler"));

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INQ_VAR_ENDIAN, &ret, &ncid, &varid, &endian_present);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ_VAR_ENDIAN, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((2,"inq_var_endian_handler ncid = %d varid = %d endian_present = %d", ncid, varid,
         endian_present));

    /* Set the non-NULL pointers. */
    if (endian_present)
        endianp = &endian;

    /* Call the inq function to get the values. */
    if ((ret = PIOc_inq_var_endian(ncid, varid, endianp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ_VAR_ENDIAN on iosystem (iosysid=%d). Unable to inquire endianness settings for variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/**
 * Do an inq_var_deflate on a netCDF variable. This function is only
 * run on IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, error code otherwise.
 */
int inq_var_deflate_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    char shuffle_present;
    char deflate_present;
    char deflate_level_present;
    int shuffle, *shufflep;
    int deflate, *deflatep;
    int deflate_level, *deflate_levelp;
    int ret;

    assert(ios);
    LOG((1, "inq_var_deflate_handler"));

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INQ_VAR_DEFLATE, &ret,
        &ncid, &varid, &shuffle_present, &shuffle,
        &deflate_present, &deflate,
        &deflate_level_present, &deflate_level);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ_VAR_DEFLATE, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((2, "inq_var_handler ncid = %d varid = %d shuffle_present = %d deflate_present = %d "
         "deflate_level_present = %d", ncid, varid, shuffle_present, deflate_present,
         deflate_level_present));

    /* Set the non-NULL pointers. */
    if (shuffle_present)
        shufflep = &shuffle;
    if (deflate_present)
        deflatep = &deflate;
    if (deflate_level_present)
        deflate_levelp = &deflate_level;

    /* Call the inq function to get the values. */
    if ((ret = PIOc_inq_var_deflate(ncid, varid, shufflep, deflatep, deflate_levelp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ_VAR_DEFLATE on iosystem (iosysid=%d). Unable to inquire deflate settings for variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/** Do an inq_varid on a netCDF variable name. This function is only
 * run on IO tasks.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int inq_varid_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    int ret;
    int namelen = PIO_MAX_NAME + 1;
    char name[PIO_MAX_NAME + 1];

    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INQ_VARID, &ret, &ncid, &namelen, name);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INQ_VARID, on iosystem (iosysid=%d)", ios->iosysid);
    }

    /* Call the inq_varid function. */
    if ((ret = PIOc_inq_varid(ncid, name, &varid)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INQ_VARID on iosystem (iosysid=%d). Unable to inquire id of variable %s in file %s (ncid=%d)", ios->iosysid, name, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to sync a netCDF file.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int sync_file_handler(iosystem_desc_t *ios)
{
    int ncid;
    int ret;

    LOG((1, "sync_file_handler"));
    assert(ios);

    /* Get the parameters for this function that the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_SYNC, &ret, &ncid);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_SYNC, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "sync_file_handler got parameter ncid = %d", ncid));

    /* Call the sync file function. */
    if ((ret = PIOc_sync(ncid)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_SYNC on iosystem (iosysid=%d). Unable to sync file %s (ncid=%d)", ios->iosysid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((2, "sync_file_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to set the record dimension
 * value for a netCDF variable.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int setframe_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    int frame;
    int ret;

    LOG((1, "setframe_handler"));
    assert(ios);

    /* Get the parameters for this function that the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_SETFRAME, &ret, &ncid, &varid, &frame);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_SETFRAME, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "setframe_handler got parameter ncid = %d varid = %d frame = %d",
         ncid, varid, frame));

    /* Call the function. */
    if ((ret = PIOc_setframe(ncid, varid, frame)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_SETFRAME on iosystem (iosysid=%d). Unable to setframe (frame = %d) for variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, frame, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((2, "setframe_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to increment the record
 * dimension value for a netCDF variable.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int advanceframe_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    int ret;

    LOG((1, "advanceframe_handler"));
    assert(ios);

    /* Get the parameters for this function that the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_ADVANCEFRAME, &ret, &ncid, &varid);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_ADVANCEFRAME, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "advanceframe_handler got parameter ncid = %d varid = %d",
         ncid, varid));

    /* Call the function. */
    if ((ret = PIOc_advanceframe(ncid, varid)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_SETFRAME on iosystem (iosysid=%d). Unable to advance frame for variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((2, "advanceframe_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to enddef a netCDF file.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int change_def_file_handler(iosystem_desc_t *ios, int msg)
{
    int ncid;
    int ret;

    LOG((1, "change_def_file_handler"));
    assert(ios);

    /* Get the parameters for this function that the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, msg, &ret, &ncid);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, %s, on iosystem (iosysid=%d)", ((msg == PIO_MSG_ENDDEF) ? "PIO_MSG_ENDDEF" : "PIO_MSG_REDEF"), ios->iosysid);
    }

    /* Call the function. */
    if (msg == PIO_MSG_ENDDEF)
        ret = PIOc_enddef(ncid);
    else
        ret = PIOc_redef(ncid);

    if (ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, %s on iosystem (iosysid=%d). Unable to %s in file %s (ncid=%d)", ((msg == PIO_MSG_ENDDEF) ? "PIO_MSG_ENDDEF" : "PIO_MSG_REDEF"), ios->iosysid, ((msg == PIO_MSG_ENDDEF) ? "end define mode" : "re-enter define mode"), pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "change_def_file_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to define a netCDF
 *  variable.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int def_var_handler(iosystem_desc_t *ios)
{
    int ncid;
    int namelen = PIO_MAX_NAME + 1;
    char name[PIO_MAX_NAME + 1];
    int ret;
    int varid;
    nc_type xtype;
    int ndims;
    int dimids_sz;
    int dimids[PIO_MAX_DIMS];

    LOG((1, "def_var_handler comproot = %d", ios->comproot));
    assert(ios);

    /* Get the parameters for this function that the he comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_DEF_VAR, &ret,
      &ncid, &namelen, name, &xtype, &ndims, &dimids_sz, dimids);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_DEF_VAR, on iosystem (iosysid=%d)", ios->iosysid);
    }

    LOG((1, "def_var_handler got parameters namelen = %d "
         "name = %s ncid = %d", namelen, name, ncid));

    /* Call the function. */
    if ((ret = PIOc_def_var(ncid, name, xtype, ndims, dimids, &varid)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_DEF_VAR on iosystem (iosysid=%d). Unable to define variable %s in file %s (ncid=%d)", ios->iosysid, name, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "def_var_handler succeeded!"));
    return PIO_NOERR;
}

/**
 * This function is run on the IO tasks to define chunking for a
 *  netCDF variable.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, error code otherwise.
 */
int def_var_chunking_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    int ndims;
    int storage;
    char chunksizes_present;
    int chunksizes_sz = 0;
    PIO_Offset chunksizes[PIO_MAX_DIMS], *chunksizesp = NULL;
    int ret;

    assert(ios);
    LOG((1, "def_var_chunking_handler comproot = %d", ios->comproot));

    /* Get the parameters for this function that the he comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_DEF_VAR_CHUNKING, &ret,
        &ncid, &varid, &storage, &ndims, &chunksizes_present,
        &chunksizes_sz, chunksizes);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_DEF_VAR_CHUNKING, on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "def_var_chunking_handler got parameters ncid = %d varid = %d storage = %d "
         "ndims = %d chunksizes_present = %d", ncid, varid, storage, ndims, chunksizes_present));

    /* Set the non-NULL pointers. */
    if (chunksizes_present)
        chunksizesp = chunksizes;

    /* Call the function. */
    if ((ret = PIOc_def_var_chunking(ncid, varid, storage, chunksizesp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_DEF_VAR_CHUNKING on iosystem (iosysid=%d). Unable to define chunking parameters for variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "def_var_chunking_handler succeeded!"));
    return PIO_NOERR;
}

/**
 * This function is run on the IO tasks to define fill mode and fill
 * value.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, error code otherwise.
 */
int def_var_fill_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    int fill_mode;
    char fill_value_present;
    PIO_Offset type_size, fill_value_sz;
    PIO_Offset *fill_valuep = NULL;
    int ret = PIO_NOERR;

    assert(ios);
    LOG((1, "def_var_fill_handler comproot = %d", ios->comproot));

    /* Get the parameters for this function that the he comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_DEF_VAR_FILL, &ret,
        &ncid, &varid, &fill_mode, &type_size, &fill_value_present,
        &fill_value_sz, &fill_valuep);

    if (ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_DEF_VAR_FILL, on iosystem (iosysid=%d)", ios->iosysid);
    }

    LOG((1, "def_var_fill_handler got parameters ncid = %d varid = %d fill_mode = %d "
         "type_size = %lld fill_value_present = %d", ncid, varid, fill_mode, type_size, fill_value_present));

    /* Call the function. */
    ret = PIOc_def_var_fill(ncid, varid, fill_mode, (fill_value_present) ? (fill_valuep) : NULL);

    /* Free memory allocated for the fill value. */
    if (fill_valuep)
        free(fill_valuep);

    if (ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_DEF_VAR_FILL on iosystem (iosysid=%d). Unable to define fill mode/value for variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "def_var_fill_handler succeeded!"));
    return PIO_NOERR;
}

/**
 * This function is run on the IO tasks to define endianness for a
 * netCDF variable.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, error code otherwise.
 */
int def_var_endian_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    int endian;
    int ret;

    assert(ios);
    LOG((1, "def_var_endian_handler comproot = %d", ios->comproot));

    /* Get the parameters for this function that the he comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_DEF_VAR_ENDIAN, &ret, &ncid, &varid, &endian);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_DEF_VAR_ENDIAN on iosystem (iosysid=%d)", ios->iosysid);
    }
      
    LOG((1, "def_var_endian_handler got parameters ncid = %d varid = %d endain = %d ",
         ncid, varid, endian));

    /* Call the function. */
    if ((ret = PIOc_def_var_endian(ncid, varid, endian)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_DEF_VAR_ENDIAN on iosystem (iosysid=%d). Unable to define endianness for variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "def_var_chunking_handler succeeded!"));
    return PIO_NOERR;
}

/**
 * This function is run on the IO tasks to define deflate settings for
 * a netCDF variable.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, error code otherwise.
 */
int def_var_deflate_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    int shuffle;
    int deflate;
    int deflate_level;
    int ret;

    assert(ios);
    LOG((1, "def_var_deflate_handler comproot = %d", ios->comproot));

    /* Get the parameters for this function that the he comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_DEF_VAR_DEFLATE, &ret,
        &ncid, &varid, &shuffle, &deflate, &deflate_level);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_DEF_VAR_DEFLATE on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "def_var_deflate_handler got parameters ncid = %d varid = %d shuffle = %d ",
         "deflate = %d deflate_level = %d", ncid, varid, shuffle, deflate, deflate_level));

    /* Call the function. */
    if ((ret = PIOc_def_var_deflate(ncid, varid, shuffle, deflate, deflate_level)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_DEF_VAR_DEFLATE on iosystem (iosysid=%d). Unable to deflate variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "def_var_deflate_handler succeeded!"));
    return PIO_NOERR;
}

/**
 * This function is run on the IO tasks to define chunk cache settings
 * for a netCDF variable.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, error code otherwise.
 */
int set_var_chunk_cache_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    PIO_Offset size;
    PIO_Offset nelems;
    float preemption;
    int ret; /* Return code. */

    assert(ios);
    LOG((1, "set_var_chunk_cache_handler comproot = %d", ios->comproot));

    /* Get the parameters for this function that the he comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_SET_VAR_CHUNK_CACHE, &ret,
        &ncid, &varid, &size, &nelems, &preemption);
    LOG((1, "set_var_chunk_cache_handler got params ncid = %d varid = %d size = %d "
         "nelems = %d preemption = %g", ncid, varid, size, nelems, preemption));

    if (ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_SET_VAR_CHUNK_CACHE on iosystem (iosysid=%d)", ios->iosysid);
    }

    /* Call the function. */
    if ((ret = PIOc_set_var_chunk_cache(ncid, varid, size, nelems, preemption)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_SET_VAR_CHUNK_CACHE on iosystem (iosysid=%d). Unable to set cache size for chunking variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "def_var_chunk_cache_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to define a netCDF
 * dimension.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int def_dim_handler(iosystem_desc_t *ios)
{
    int ncid;
    int len, namelen;
    char name[PIO_MAX_NAME + 1];
    int ret;
    int dimid;

    LOG((1, "def_dim_handler comproot = %d", ios->comproot));
    assert(ios);

    /* Get the parameters for this function that the he comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_DEF_DIM, &ret, &ncid, &namelen, name, &len);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_DEF_DIM on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((2, "def_dim_handler got parameters namelen = %d "
         "name = %s len = %d ncid = %d", namelen, name, len, ncid));

    /* Call the function. */
    if ((ret = PIOc_def_dim(ncid, name, len, &dimid)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_DEF_DIM on iosystem (iosysid=%d). Unable to define dim %s in file %s (ncid=%d)", ios->iosysid, name, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "def_dim_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to rename a netCDF
 * dimension.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int rename_dim_handler(iosystem_desc_t *ios)
{
    int ncid;
    int namelen;
    char name[PIO_MAX_NAME + 1];
    int ret;
    int dimid;

    LOG((1, "rename_dim_handler"));
    assert(ios);

    /* Get the parameters for this function that the he comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_RENAME_DIM, &ret,
        &ncid, &dimid, &namelen, name);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_RENAME_DIM on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((2, "rename_dim_handler got parameters namelen = %d "
         "name = %s ncid = %d dimid = %d", namelen, name, ncid, dimid));

    /* Call the function. */
    if ((ret = PIOc_rename_dim(ncid, dimid, name)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_RENAME_DIM on iosystem (iosysid=%d). Unable to rename dim (dimid=%d) to %s in file %s (ncid=%d)", ios->iosysid, dimid, name, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "rename_dim_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to rename a netCDF
 * dimension.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int rename_var_handler(iosystem_desc_t *ios)
{
    int ncid;
    int namelen;
    char name[PIO_MAX_NAME + 1];
    int ret;
    int varid;

    LOG((1, "rename_var_handler"));
    assert(ios);

    /* Get the parameters for this function that the he comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_RENAME_VAR, &ret,
        &ncid, &varid, &namelen, name);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_RENAME_VAR on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((2, "rename_var_handler got parameters namelen = %d "
         "name = %s ncid = %d varid = %d", namelen, name, ncid, varid));

    /* Call the function. */
    if ((ret = PIOc_rename_var(ncid, varid, name)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_RENAME_VAR on iosystem (iosysid=%d). Unable to rename variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "rename_var_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to rename a netCDF
 * attribute.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int rename_att_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    int namelen, newnamelen;
    char name[PIO_MAX_NAME + 1], newname[PIO_MAX_NAME + 1];
    int ret;

    LOG((1, "rename_att_handler"));
    assert(ios);

    /* Get the parameters for this function that the he comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_RENAME_ATT, &ret, &ncid, &varid,
        &namelen, name, &newnamelen, newname);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_RENAME_ATT on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((2, "rename_att_handler got parameters namelen = %d name = %s ncid = %d varid = %d "
         "newnamelen = %d newname = %s", namelen, name, ncid, varid, newnamelen, newname));

    /* Call the function. */
    if ((ret = PIOc_rename_att(ncid, varid, name, newname)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_RENAME_ATT on iosystem (iosysid=%d). Unable to rename attribute %s to %s of variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, name, newname, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "rename_att_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to delete a netCDF
 * attribute.
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int delete_att_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    int namelen;
    char name[PIO_MAX_NAME + 1];
    int ret;

    LOG((1, "delete_att_handler"));
    assert(ios);

    /* Get the parameters for this function that the he comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_DEL_ATT, &ret, &ncid, &varid,
        &namelen, name);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_DEL_ATT on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((2, "delete_att_handler namelen = %d name = %s ncid = %d varid = %d ",
         namelen, name, ncid, varid));

    /* Call the function. */
    if ((ret = PIOc_del_att(ncid, varid, name)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_DEL_ATT on iosystem (iosysid=%d). Unable to delete attribute %s of variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, name, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "delete_att_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to open a netCDF file.
 *
 *
 * @param ios pointer to the iosystem_desc_t.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int open_file_handler(iosystem_desc_t *ios)
{
    int ncid;
    int len;
    int iotype;
    int mode;
    int ret = PIO_NOERR;

    LOG((1, "open_file_handler comproot = %d", ios->comproot));
    assert(ios);

    /* Get the parameters for this function that the he comp master
     * task is broadcasting. */
    char filename[PIO_MAX_NAME+1];
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_OPEN_FILE, &ret, &len, filename, &iotype, &mode);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_OPEN_FILE on iosystem (iosysid=%d)", ios->iosysid);
    }

    LOG((2, "open_file_handler got parameters len = %d filename = %s iotype = %d mode = %d",
         len, filename, iotype, mode));

    /* Call the open file function */
    ret = PIOc_openfile_retry(ios->iosysid, &ncid, &iotype, filename, mode, 0);
    if (ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_OPEN_FILE on iosystem (iosysid=%d). Unable to open file %s (ncid=%d)", ios->iosysid, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to delete a netCDF file.
 *
 * @param ios pointer to the iosystem_desc_t data.
 *
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int delete_file_handler(iosystem_desc_t *ios)
{
    char filename[PIO_MAX_NAME+1];
    int len = 0;
    int ret;

    LOG((1, "delete_file_handler comproot = %d", ios->comproot));
    assert(ios);

    /* Get the parameters for this function that the he comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_DELETE_FILE, &ret, &len, filename);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_DELETE_FILE on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "delete_file_handler got parameters len = %d filename = %s",
         len, filename));

    /* Call the delete file function. */
    if ((ret = PIOc_deletefile(ios->iosysid, filename)))
    {
        const char *fname = (len > 0) ? filename : "UNKNOWN";
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_DELETE_FILE on iosystem (iosysid=%d). Unable to delete file %s", ios->iosysid, fname);
    }

    LOG((1, "delete_file_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to initialize a decomposition.
 *
 * @param ios pointer to the iosystem_desc_t data.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 */
int initdecomp_dof_handler(iosystem_desc_t *ios)
{
    int iosysid;
    int pio_type;
    int ndims;
    int maplen;
    int ioid;
    PIO_Offset *compmap = NULL;
    char rearranger_present;
    int rearranger;
    int *rearrangerp = NULL;
    char iostart_present;
    PIO_Offset *iostartp = NULL;
    char iocount_present;
    PIO_Offset *iocountp = NULL;
    int ret; /* Return code. */

    LOG((1, "initdecomp_dof_handler called"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    int dims[PIO_MAX_DIMS];
    int niostart = 0, niocount = 0;
    PIO_Offset iostart[PIO_MAX_DIMS];
    PIO_Offset iocount[PIO_MAX_DIMS];

    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_INITDECOMP_DOF, &ret,
        &iosysid, &pio_type, &ndims, dims, &maplen, &compmap,
        &rearranger_present, &rearranger, &iostart_present,
        &niostart, iostart, &iocount_present, &niocount,
        iocount);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_INITDECOMP_DOF on iosystem (iosysid=%d)", ios->iosysid);
    }

    LOG((2, "initdecomp_dof_handler iosysid = %d pio_type = %d ndims = %d maplen = %d "
         "rearranger_present = %d iostart_present = %d iocount_present = %d ",
         iosysid, pio_type, ndims, maplen, rearranger_present, iostart_present, iocount_present));

    if (rearranger_present)
        rearrangerp = &rearranger;
    if (iostart_present)
        iostartp = iostart;
    if (iocount_present)
        iocountp = iocount;

    /* Call the function. */
    /* The I/O procs don't have any user data, they gather data from compute
     * procs and write it out. So ignore compmap
     */
    maplen = 0;
    ret = PIOc_InitDecomp(iosysid, pio_type, ndims, dims, maplen, compmap, &ioid, rearrangerp,
                          iostartp, iocountp);

    if(compmap)
    {
        free(compmap);
    }

    if (ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_INITDECOMP_DOF on iosystem (iosysid=%d). Initializing PIO decomposition failed, pio_type = %d ndims = %d maplen = %d rearranger_present = %d iostart_present = %d iocount_present = %d", ios->iosysid, pio_type, ndims, maplen, rearranger_present, iostart_present, iocount_present);
    }
    
    LOG((1, "PIOc_InitDecomp returned %d", ret));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to do darray writes.
 *
 * @param ios pointer to the iosystem_desc_t data.
 *
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int write_darray_multi_handler(iosystem_desc_t *ios)
{
    int ncid;
    file_desc_t *file;     /* Pointer to file information. */
    int nvars;
    int ioid;
    io_desc_t *iodesc;     /* The IO description. */
    char frame_present;
    int *framep = NULL;
    int *frame = NULL;
    PIO_Offset arraylen;
    void *array = NULL;
    char fillvalue_present;
    void *fillvaluep = NULL;
    void *fillvalue = NULL;
    int flushtodisk;
    int ret;

    LOG((1, "write_darray_multi_handler"));
    assert(ios);

    int varids_sz = 0;
    int *varids = NULL;
    PIO_Offset array_sz = 0;
    int nframes = 0, nfillvalues = 0;

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_WRITEDARRAYMULTI, &ret,
        &ncid, &nvars, &varids_sz, &varids, &ioid, &arraylen,
        &array_sz, &array, &frame_present, &nframes, &frame,
        &fillvalue_present, &nfillvalues, &fillvalue, &flushtodisk);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_WRITEDARRAYMULTI on iosystem (iosysid=%d)", ios->iosysid);
    }

    LOG((1, "write_darray_multi_handler ncid = %d nvars = %d ioid = %d arraylen = %d "
         "frame_present = %d fillvalue_present flushtodisk = %d", ncid, nvars,
         ioid, arraylen, frame_present, fillvalue_present, flushtodisk));

    /* Get file info based on ncid. */
    if ((ret = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_WRITEDARRAYMULTI on iosystem (iosysid=%d). Unable to inquire internal structure associated with file id (ncid=%d)", ios->iosysid, ncid);

    }
    
    /* Get decomposition information. */
    if (!(iodesc = pio_get_iodesc_from_id(ioid)))
    {
        return pio_err(ios, file, PIO_EBADID, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_WRITEDARRAYMULTI on iosystem (iosysid=%d). Unable to inquire I/O decomposition associated with ioid (ioid=%d)", ios->iosysid, ioid);
    }

    /* Was a frame array provided? */
    if (frame_present)
        framep = frame;

    /* Was a fillvalue array provided? */
    if (fillvalue_present)
        fillvaluep = fillvalue;

    /* Call the function from IO tasks. Errors are handled within
     * function. */
    ret = PIOc_write_darray_multi(ncid, varids, ioid, nvars, arraylen,
                            array, framep, fillvaluep, flushtodisk);

    /* Free resources. */
    if(varids_sz > 0)
    {
        free(varids);
    }
    if(nframes > 0)
    {
        free(frame);
    }
    if(nfillvalues > 0)
    {
        free(fillvalue);
    }
    free(array);

    if (ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_WRITEDARRAYMULTI on iosystem (iosysid=%d). Unable to write multiple variables (%d vars, ioid=%d) to file %s (ncid=%d)", ios->iosysid, nvars, ioid, pio_get_fname_from_file_id(ncid), ncid);
    }
    
    LOG((1, "write_darray_multi_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to...
 * NOTE: not yet implemented
 *
 * @param ios pointer to the iosystem_desc_t data.
 *
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int readdarray_handler(iosystem_desc_t *ios)
{
    int ncid, varid, ioid;
    int ierr;

    LOG((1, "read_darray_handler"));
    assert(ios);

    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_READDARRAY, &ierr, &ncid, &varid, &ioid);
    if(ierr != PIO_NOERR)
    {
        return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_READDARRAY on iosystem (iosysid=%d)", ios->iosysid);
    }

    LOG((1, "PIOc_read_darray(ncid=%d, varid=%d, ioid=%d, 0, NULL)", ncid, varid, ioid));
    /* On the I/O procs we don't have any user buffers,
     * i.e., arraylen == 0
     */
    ierr = PIOc_read_darray(ncid, varid, ioid, 0, NULL);
    if (ierr != PIO_NOERR)
    {
        return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_READDARRAY on iosystem (iosysid=%d). Unable to read variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to set the error handler.
 *
 * @param ios pointer to the iosystem_desc_t data.
 *
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int seterrorhandling_handler(iosystem_desc_t *ios)
{
    int method;
    bool old_method_present;
    int old_method;
    int *old_methodp = NULL;
    int ret;

    LOG((1, "seterrorhandling_handler comproot = %d", ios->comproot));
    assert(ios);

    /* Get the parameters for this function that the he comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_SETERRORHANDLING, &ret, &method, &old_method_present);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_SETERRORHANDLING on iosystem (iosysid=%d)", ios->iosysid);
    }

    LOG((1, "seterrorhandling_handler got parameters method = %d old_method_present = %d",
         method, old_method_present));

    if (old_method_present)
        old_methodp = &old_method;

    /* Call the function. */
    if ((ret = PIOc_set_iosystem_error_handling(ios->iosysid, method, old_methodp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_SETERRORHANDLING on iosystem (iosysid=%d). Unable to set the iosystem error handler", ios->iosysid);
    }

    LOG((1, "seterrorhandling_handler succeeded!"));
    return PIO_NOERR;
}

/**
 * This function is run on the IO tasks to set the chunk cache
 * parameters for netCDF-4.
 *
 * @param ios pointer to the iosystem_desc_t data.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @author Ed Hartnett
 */
int set_chunk_cache_handler(iosystem_desc_t *ios)
{
    int iosysid;
    int iotype;
    PIO_Offset size;
    PIO_Offset nelems;
    float preemption;
    int ret; /* Return code. */

    LOG((1, "set_chunk_cache_handler called"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_SET_CHUNK_CACHE, &ret, &iosysid,
        &iotype, &size, &nelems, &preemption);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_SET_CHUNK_CACHE on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "set_chunk_cache_handler got params iosysid = %d iotype = %d size = %d "
         "nelems = %d preemption = %g", iosysid, iotype, size, nelems, preemption));

    /* Call the function. */
    if ((ret = PIOc_set_chunk_cache(iosysid, iotype, size, nelems, preemption)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_SET_CHUNK_CACHE on iosystem (iosysid=%d). Unable to set the iosystem chunk cache info", ios->iosysid);
    }

    LOG((1, "set_chunk_cache_handler succeeded!"));
    return PIO_NOERR;
}

/**
 * This function is run on the IO tasks to get the chunk cache
 * parameters for netCDF-4.
 *
 * @param ios pointer to the iosystem_desc_t data.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @author Ed Hartnett
 */
int get_chunk_cache_handler(iosystem_desc_t *ios)
{
    int iosysid;
    int iotype;
    char size_present, nelems_present, preemption_present;
    PIO_Offset size, *sizep;
    PIO_Offset nelems, *nelemsp;
    float preemption, *preemptionp;
    int ret; /* Return code. */

    LOG((1, "get_chunk_cache_handler called"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_GET_CHUNK_CACHE, &ret,
        &iosysid, &iotype, &size_present, &nelems_present, &preemption_present);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_GET_CHUNK_CACHE on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "get_chunk_cache_handler got params iosysid = %d iotype = %d size_present = %d "
         "nelems_present = %d preemption_present = %g", iosysid, iotype, size_present,
         nelems_present, preemption_present));

    /* Set the non-NULL pointers. */
    if (size_present)
        sizep = &size;
    if (nelems_present)
        nelemsp = &nelems;
    if (preemption_present)
        preemptionp = &preemption;

    /* Call the function. */
    if ((ret = PIOc_get_chunk_cache(iosysid, iotype, sizep, nelemsp, preemptionp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_GET_CHUNK_CACHE on iosystem (iosysid=%d). Unable to get the iosystem chunk cache info", ios->iosysid);
    }

    LOG((1, "get_chunk_cache_handler succeeded!"));
    return PIO_NOERR;
}

/**
 * This function is run on the IO tasks to get the variable chunk
 * cache parameters for netCDF-4.
 *
 * @param ios pointer to the iosystem_desc_t data.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @author Ed Hartnett
 */
int get_var_chunk_cache_handler(iosystem_desc_t *ios)
{
    int ncid;
    int varid;
    char size_present, nelems_present, preemption_present;
    PIO_Offset size, *sizep;
    PIO_Offset nelems, *nelemsp;
    float preemption, *preemptionp;
    int ret; /* Return code. */

    LOG((1, "get_var_chunk_cache_handler called"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_GET_VAR_CHUNK_CACHE, &ret,
        &ncid, &varid, &size_present, &nelems_present, &preemption_present);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_GET_VAR_CHUNK_CACHE on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "get_var_chunk_cache_handler got params ncid = %d varid = %d size_present = %d "
         "nelems_present = %d preemption_present = %g", ncid, varid, size_present,
         nelems_present, preemption_present));

    /* Set the non-NULL pointers. */
    if (size_present)
        sizep = &size;
    if (nelems_present)
        nelemsp = &nelems;
    if (preemption_present)
        preemptionp = &preemption;

    /* Call the function. */
    if ((ret = PIOc_get_var_chunk_cache(ncid, varid, sizep, nelemsp, preemptionp)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_GET_VAR_CHUNK_CACHE on iosystem (iosysid=%d). Unable to get chunk cache info for variable %s (varid=%d) in file %s (ncid=%d)", ios->iosysid, pio_get_vname_from_file_id(ncid, varid), varid, pio_get_fname_from_file_id(ncid), ncid);
    }

    LOG((1, "get_var_chunk_cache_handler succeeded!"));
    return PIO_NOERR;
}

/** 
 * This function is run on the IO tasks to free the decomp hanlder.
 *
 * @param ios pointer to the iosystem_desc_t data.
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @author Ed Hartnett
 */
int freedecomp_handler(iosystem_desc_t *ios)
{
    int iosysid;
    int ioid;
    int ret; /* Return code. */

    LOG((1, "freedecomp_handler called"));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_FREEDECOMP, &ret, &iosysid, &ioid);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_FREEDECOMP on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((2, "freedecomp_handler iosysid = %d ioid = %d", iosysid, ioid));

    /* Call the function. */
    ret = PIOc_freedecomp(iosysid, ioid);
    LOG((1, "PIOc_freedecomp returned %d", ret));
    if (ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_FREEDECOMP on iosystem (iosysid=%d). Unable to free I/O decomposition (ioid=%d)", ios->iosysid, ioid);
    }

    return PIO_NOERR;
}

/** 
 * Handle the finalize call.
 *
 * @param ios pointer to the iosystem info
 * @param index
 * @returns 0 for success, PIO_EIO for MPI Bcast errors, or error code
 * from netCDF base function.
 * @internal
 * @author Ed Hartnett
 */
int finalize_handler(iosystem_desc_t *ios, int index)
{
    int iosysid;
    int ret;

    LOG((1, "finalize_handler called index = %d", index));
    assert(ios);

    /* Get the parameters for this function that the the comp master
     * task is broadcasting. */
    PIO_RECV_ASYNC_MSG(ios, PIO_MSG_FINALIZE, &ret, &iosysid);
    if(ret != PIO_NOERR)
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error receiving asynchronous message, PIO_MSG_FINALIZE on iosystem (iosysid=%d)", ios->iosysid);
    }
    LOG((1, "finalize_handler got parameter iosysid = %d", iosysid));

    /* Call the function. */
    LOG((2, "finalize_handler calling PIOc_finalize for iosysid = %d",
         iosysid));
    if ((ret = PIOc_finalize(iosysid)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "Error processing asynchronous message, PIO_MSG_FINALIZE on iosystem (iosysid=%d). Unable to finalize I/O system", ios->iosysid);
    }

    LOG((1, "finalize_handler succeeded!"));
    return PIO_NOERR;
}

/**
 * This function is called by the IO tasks.  This function will not
 * return, unless there is an error.
 *
 * @param io_rank
 * @param component_count number of computation components
 * @param iosys pointer to pointer to iosystem info
 * @param io_comm MPI communicator for IO
 * @returns 0 for success, error code otherwise.
 * @author Ed Hartnett
 */
int pio_msg_handler2(int io_rank, int component_count, iosystem_desc_t **iosys,
                     MPI_Comm io_comm)
{
    iosystem_desc_t *my_iosys;
    int msgs[component_count];
    int msg = PIO_MSG_INVALID;
    MPI_Request req[component_count];
    MPI_Status status;
    int index;
    int mpierr;
    int ret = PIO_NOERR;
    int open_components = component_count;

    LOG((1, "pio_msg_handler2 called"));
    assert(iosys);

    /* Have IO comm rank 0 (the ioroot) register to receive
     * (non-blocking) for a message from each of the comproots. */
    if (!io_rank)
    {
        for (int cmp = 0; cmp < component_count; cmp++)
        {
            my_iosys = iosys[cmp];
            LOG((1, "about to call MPI_Irecv union_comm = %d", my_iosys->union_comm));
            if ((mpierr = MPI_Irecv(&msgs[cmp], 1, MPI_INT, my_iosys->comproot,
                                    PIO_ASYNC_MSG_HDR_TAG, my_iosys->union_comm, &req[cmp])))
                return check_mpi(NULL, NULL, mpierr, __FILE__, __LINE__);
            LOG((1, "MPI_Irecv req[%d] = %d", cmp, req[cmp]));
        }
    }

    /* If the message is not -1, keep processing messages. */
    do
    {
        LOG((3, "pio_msg_handler2 at top of loop"));

        /* Wait until any one of the requests are complete. Once it
         * returns, the Waitany function automatically sets the
         * appropriate member of the req array to MPI_REQUEST_NULL. */
        if (!io_rank)
        {
            LOG((1, "about to call MPI_Waitany req[0] = %d MPI_REQUEST_NULL = %d",
                 req[0], MPI_REQUEST_NULL));
            for (int c = 0; c < component_count; c++)
                LOG((2, "req[%d] = %d", c, req[c]));
            if ((mpierr = MPI_Waitany(component_count, req, &index, &status)))
                return check_mpi(NULL, NULL, mpierr, __FILE__, __LINE__);
            LOG((3, "Waitany returned index = %d req[%d] = %d", index, index, req[index]));
        }

        /* Broadcast the index of the computational component that
         * originated the request to the rest of the IO tasks. */
        LOG((3, "About to do Bcast of index = %d io_comm = %d", index, io_comm));
        if ((mpierr = MPI_Bcast(&index, 1, MPI_INT, 0, io_comm)))
            return check_mpi(NULL, NULL, mpierr, __FILE__, __LINE__);
        LOG((3, "index MPI_Bcast complete index = %d", index));

        /* Set the correct iosys depending on the index. */
        my_iosys = iosys[index];
        msg = msgs[index];

        /* Broadcast the msg value to the rest of the IO tasks. */
        LOG((3, "about to call msg MPI_Bcast my_iosys->io_comm = %d", my_iosys->io_comm));
        if ((mpierr = MPI_Bcast(&msg, 1, MPI_INT, 0, my_iosys->io_comm)))
            return check_mpi(NULL, NULL, mpierr, __FILE__, __LINE__);
        LOG((1, "pio_msg_handler2 msg MPI_Bcast complete msg = %d", msg));

        /* Handle the message. This code is run on all IO tasks. */
        switch (msg)
        {
        case PIO_MSG_INQ_TYPE:
            ret = inq_type_handler(my_iosys);
            break;
        case PIO_MSG_INQ_FORMAT:
            ret = inq_format_handler(my_iosys);
            break;
        case PIO_MSG_CREATE_FILE:
            ret = create_file_handler(my_iosys);
            LOG((2, "returned from create_file_handler"));
            break;
        case PIO_MSG_SYNC:
            ret = sync_file_handler(my_iosys);
            break;
        case PIO_MSG_ENDDEF:
        case PIO_MSG_REDEF:
            LOG((2, "calling change_def_file_handler"));
            ret = change_def_file_handler(my_iosys, msg);
            LOG((2, "returned from change_def_file_handler"));
            break;
        case PIO_MSG_OPEN_FILE:
            ret = open_file_handler(my_iosys);
            break;
        case PIO_MSG_CLOSE_FILE:
            ret = close_file_handler(my_iosys);
            break;
        case PIO_MSG_DELETE_FILE:
            ret = delete_file_handler(my_iosys);
            break;
        case PIO_MSG_RENAME_DIM:
            ret = rename_dim_handler(my_iosys);
            break;
        case PIO_MSG_RENAME_VAR:
            ret = rename_var_handler(my_iosys);
            break;
        case PIO_MSG_RENAME_ATT:
            ret = rename_att_handler(my_iosys);
            break;
        case PIO_MSG_DEL_ATT:
            ret = delete_att_handler(my_iosys);
            break;
        case PIO_MSG_DEF_DIM:
            ret = def_dim_handler(my_iosys);
            break;
        case PIO_MSG_DEF_VAR:
            ret = def_var_handler(my_iosys);
            break;
        case PIO_MSG_DEF_VAR_CHUNKING:
            ret = def_var_chunking_handler(my_iosys);
            break;
        case PIO_MSG_DEF_VAR_FILL:
            ret = def_var_fill_handler(my_iosys);
            break;
        case PIO_MSG_DEF_VAR_ENDIAN:
            ret = def_var_endian_handler(my_iosys);
            break;
        case PIO_MSG_DEF_VAR_DEFLATE:
            ret = def_var_deflate_handler(my_iosys);
            break;
        case PIO_MSG_INQ_VAR_ENDIAN:
            ret = inq_var_endian_handler(my_iosys);
            break;
        case PIO_MSG_SET_VAR_CHUNK_CACHE:
            ret = set_var_chunk_cache_handler(my_iosys);
            break;
        case PIO_MSG_GET_VAR_CHUNK_CACHE:
            ret = get_var_chunk_cache_handler(my_iosys);
            break;
        case PIO_MSG_INQ:
            ret = inq_handler(my_iosys);
            break;
        case PIO_MSG_INQ_UNLIMDIMS:
            ret = inq_unlimdims_handler(my_iosys);
            break;
        case PIO_MSG_INQ_DIM:
            ret = inq_dim_handler(my_iosys, msg);
            break;
        case PIO_MSG_INQ_DIMID:
            ret = inq_dimid_handler(my_iosys);
            break;
        case PIO_MSG_INQ_VAR:
            ret = inq_var_handler(my_iosys);
            break;
        case PIO_MSG_INQ_VAR_CHUNKING:
            ret = inq_var_chunking_handler(my_iosys);
            break;
        case PIO_MSG_INQ_VAR_FILL:
            ret = inq_var_fill_handler(my_iosys);
            break;
        case PIO_MSG_INQ_VAR_DEFLATE:
            ret = inq_var_deflate_handler(my_iosys);
            break;
        case PIO_MSG_GET_ATT:
            ret = att_get_handler(my_iosys);
            break;
        case PIO_MSG_PUT_ATT:
            ret = att_put_handler(my_iosys);
            break;
        case PIO_MSG_COPY_ATT:
            ret = att_copy_handler(my_iosys);
            break;
        case PIO_MSG_INQ_VARID:
            ret = inq_varid_handler(my_iosys);
            break;
        case PIO_MSG_INQ_ATT:
            ret = inq_att_handler(my_iosys);
            break;
        case PIO_MSG_INQ_ATTNAME:
            ret = inq_attname_handler(my_iosys);
            break;
        case PIO_MSG_INQ_ATTID:
            ret = inq_attid_handler(my_iosys);
            break;
        case PIO_MSG_GET_VARS:
            ret = get_vars_handler(my_iosys);
            break;
        case PIO_MSG_PUT_VARS:
            ret = put_vars_handler(my_iosys);
            break;
        case PIO_MSG_INITDECOMP_DOF:
            ret = initdecomp_dof_handler(my_iosys);
            break;
        case PIO_MSG_WRITEDARRAYMULTI:
            ret = write_darray_multi_handler(my_iosys);
            break;
        case PIO_MSG_SETFRAME:
            ret = setframe_handler(my_iosys);
            break;
        case PIO_MSG_ADVANCEFRAME:
            ret = advanceframe_handler(my_iosys);
            break;
        case PIO_MSG_READDARRAY:
            ret = readdarray_handler(my_iosys);
            break;
        case PIO_MSG_SETERRORHANDLING:
            ret = seterrorhandling_handler(my_iosys);
            break;
        case PIO_MSG_SET_CHUNK_CACHE:
            ret = set_chunk_cache_handler(my_iosys);
            break;
        case PIO_MSG_GET_CHUNK_CACHE:
            ret = get_chunk_cache_handler(my_iosys);
            break;
        case PIO_MSG_FREEDECOMP:
            ret = freedecomp_handler(my_iosys);
            break;
        case PIO_MSG_SET_FILL:
            ret = set_fill_handler(my_iosys);
            break;
        case PIO_MSG_FINALIZE:
            ret = finalize_handler(my_iosys, index);
            break;
        default:
            LOG((0, "unknown message received %d", msg));
            return PIO_EINVAL;
        }

        /* If an error was returned by the handler, do nothing! */
        LOG((3, "pio_msg_handler2 checking error ret = %d", ret));

        /* Listen for another msg from the component whose message we
         * just handled. */
        if (!io_rank && (msg != PIO_MSG_FINALIZE))
        {
            my_iosys = iosys[index];
            LOG((3, "pio_msg_handler2 about to Irecv index = %d comproot = %d union_comm = %d",
                 index, my_iosys->comproot, my_iosys->union_comm));
            if ((mpierr = MPI_Irecv(&msgs[index], 1, MPI_INT, my_iosys->comproot, PIO_ASYNC_MSG_HDR_TAG, my_iosys->union_comm,
                                    &req[index])))
                return check_mpi(NULL, NULL, mpierr, __FILE__, __LINE__);
            LOG((3, "pio_msg_handler2 called MPI_Irecv req[%d] = %d", index, req[index]));
        }

        LOG((3, "pio_msg_handler2 done msg = %d open_components = %d",
             msg, open_components));

        /* If there are no more open components, exit. */
        if (msg == PIO_MSG_FINALIZE)
        {
            open_components -= 1;
            if(open_components == 0)
            {
                /* No more open components, will exit the loop */
                msg = PIO_MSG_EXIT;
                /* Delete the global MPI communicator used for messaging */
                delete_async_service_msg_comm();
            }
        }
    } while(msg != PIO_MSG_EXIT);

    LOG((3, "returning from pio_msg_handler2"));
    return PIO_NOERR;
}
