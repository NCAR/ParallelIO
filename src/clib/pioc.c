/**
 * @file
 * Some initialization and support functions.
 * @author Jim Edwards
 * @date  2014
 *
 * @see http://code.google.com/p/parallelio/
 */

#include <pio_config.h>
#include <pio.h>
#include <pio_internal.h>
#ifdef PIO_MICRO_TIMING
#include "pio_timer.h"
#endif
#include "pio_sdecomps_regex.h"

bool fortran_order = false;

/** The default error handler used when iosystem cannot be located. */
int default_error_handler = PIO_INTERNAL_ERROR;

/** The target blocksize for each io task when the box rearranger is
 * used (see pio_sc.c). */
extern int blocksize;

/**
 * Check to see if PIO has been initialized.
 *
 * @param iosysid the IO system ID
 * @param active pointer that gets true if IO system is active, false
 * otherwise.
 * @returns 0 on success, error code otherwise
 * @author Jim Edwards
 */
int PIOc_iosystem_is_active(int iosysid, bool *active)
{
    iosystem_desc_t *ios;

    LOG((1, "PIOc_iosystem_is_active(iosysid=%d)", iosysid));

    /* Get the ios if there is one. */
    ios = pio_get_iosystem_from_id(iosysid);

    if (active)
    {
        if (!ios || (ios->comp_comm == MPI_COMM_NULL && ios->io_comm == MPI_COMM_NULL))
            *active = false;
        else
            *active = true;
    }

    return PIO_NOERR;
}

/**
 * Check to see if PIO file is open.
 *
 * @param ncid the ncid of an open file
 * @returns 1 if file is open, 0 otherwise.
 * @author Jim Edwards
 */
int PIOc_File_is_Open(int ncid)
{
    file_desc_t *file;

    LOG((1, "PIOc_File_is_Open(ncid=%d)", ncid));

    /* If get file returns non-zero, then this file is not open. */
    if (pio_get_file(ncid, &file))
        return 0;
    else
        return 1;
}

static const char *PIO_error_handler_to_string(int eh)
{
    if(eh == PIO_INTERNAL_ERROR){
        return "PIO_INTERNAL_ERROR";
    } else if(eh == PIO_BCAST_ERROR){
        return "PIO_BCAST_ERROR";
    } else if(eh == PIO_REDUCE_ERROR){
        return "PIO_REDUCE_ERROR";
    } else if(eh == PIO_RETURN_ERROR){
        return "PIO_RETURN_ERROR";
    } else{
        return "UNKNOWN ERROR";
    }
}

/**
 * Set the error handling method to be used for subsequent pio library
 * calls, returns the previous method setting. Note that this changes
 * error handling for the IO system that was used when this file was
 * opened. Other files opened with the same IO system will also he
 * affected by this call. This function is supported but
 * deprecated. New code should use PIOc_set_iosystem_error_handling().
 * This method has no way to return an error, so any failure will
 * result in MPI_Abort.
 *
 * @param ncid the ncid of an open file
 * @param method the error handling method
 * @returns old error handler
 * @ingroup PIO_error_method
 * @author Jim Edwards
 */
int PIOc_Set_File_Error_Handling(int ncid, int method)
{
    file_desc_t *file;
    int oldmethod;

    LOG((1, "PIOc_Set_File_Error_Handling(ncid=%d, method=%d)", ncid, method));

    /* Get the file info. */
    if (pio_get_file(ncid, &file))
        piodie(__FILE__, __LINE__, "Seting file error handler failed. Invalid file id (ncid) provided. Could not find file corresponding to ncid=%d", ncid);

    /* Check that valid error handler was provided. */
    if (method != PIO_INTERNAL_ERROR && method != PIO_BCAST_ERROR &&
        method != PIO_RETURN_ERROR && method != PIO_REDUCE_ERROR)
        piodie(__FILE__, __LINE__, "Setting file error handler failed on file (%s). Invalid error handler method (%d:%s) provided.", pio_get_fname_from_file(file), method, PIO_error_handler_to_string(method));

    /* Get the old method. */
    oldmethod = file->iosystem->error_handler;

    /* Set the error hanlder. */
    file->iosystem->error_handler = method;

    return oldmethod;
}

/**
 * Increment the unlimited dimension of the given variable.
 *
 * @param ncid the ncid of the open file
 * @param varid the variable ID
 * @returns 0 on success, error code otherwise
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_advanceframe(int ncid, int varid)
{
    file_desc_t *file;
    iosystem_desc_t *ios;
    int ret;

    LOG((1, "PIOc_advanceframe ncid = %d varid = %d", ncid, varid));

    /* Get the file info. */
    if ((ret = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ret, __FILE__, __LINE__,
                        "Advancing frame failed. Invalid file id (%d) provided. Could not find a file associated with the file id.", ncid);
    }
    ios = file->iosystem;

    /* Check inputs. */
    if (varid < 0 || varid >= PIO_MAX_VARS)
    {
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Advancing frame failed on file (%s). Invalid var id (%d) provided. Variable id is not in expected range [0:%lld]", pio_get_fname_from_file(file), varid, (long long int) PIO_MAX_VARS);
    }

    LOG((1, "PIOc_advanceframe file=%s (ncid = %d), var=%s (varid = %d)", pio_get_fname_from_file(file), ncid, pio_get_vname_from_file(file, varid), varid));
    /* If using async, and not an IO task, then send parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_ADVANCEFRAME;

        PIO_SEND_ASYNC_MSG(ios, msg, &ret, ncid, varid);
        if(ret != PIO_NOERR)
        {
            return pio_err(ios, file, ret, __FILE__, __LINE__,
                            "Advancing frame failed on file (%s) for var (%s). Error sending async msg PIO_MSG_ADVANCEFRAME (iosysid=%d)", pio_get_fname_from_file(file), pio_get_vname_from_file(file, varid), ios->iosysid);
        }
    }

    /* Increment the record number. */
    file->varlist[varid].record++;

    return PIO_NOERR;
}

/**
 * Set the unlimited dimension of the given variable
 *
 * @param ncid the ncid of the file.
 * @param varid the varid of the variable
 * @param frame the value of the unlimited dimension.  In c 0 for the
 * first record, 1 for the second
 * @return PIO_NOERR for no error, or error code.
 * @ingroup PIO_setframe
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_setframe(int ncid, int varid, int frame)
{
    file_desc_t *file;
    iosystem_desc_t *ios;
    int ret;

    LOG((1, "PIOc_setframe ncid = %d varid = %d frame = %d", ncid,
         varid, frame));

    /* Reset all invalid frame values to -1. We reset the frame number
     * instead of returning an error since PIO does not define a value'
     * for frame numbers for variables with no record dimension
     */
    if(frame < 0)
    {
        LOG((2, "Resetting invalid frame number %d to -1", frame));
        frame = -1;
    }

    /* Get file info. */
    if ((ret = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ret, __FILE__, __LINE__,
                        "Setting frame failed. Invalid file id (%d) provided. Could not find file corresponding to the file id", ncid);
    }
    ios = file->iosystem;

    LOG((1, "PIOc_setframe file=%s (ncid = %d) var=%s (varid = %d) frame = %d",
              pio_get_fname_from_file(file), ncid, pio_get_vname_from_file(file, varid), varid, frame));

    /* Check inputs. */
    if (varid < 0 || varid >= PIO_MAX_VARS)
    {
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Setting frame failed on file (%s). Invalid var id (%d) provided. Variable id is not in expected range [0,%lld]", pio_get_fname_from_file(file), varid, (long long int) PIO_MAX_VARS);
    }

    /* If using async, and not an IO task, then send parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_SETFRAME;
        PIO_SEND_ASYNC_MSG(ios, msg, &ret, ncid, varid, frame);
        if(ret != PIO_NOERR)
        {
            return pio_err(ios, file, ret, __FILE__, __LINE__,
                            "Setting frame failed on file (%s) for var (%s). Error sending async msg PIO_MSG_SETFRAME (iosysid=%d)", pio_get_fname_from_file(file), pio_get_vname_from_file(file, varid), ios->iosysid);
        }
    }

    /* Set the record dimension value for this variable. This will be
     * used by the write_darray functions. */
    file->varlist[varid].record = frame;

    return PIO_NOERR;
}

/**
 * Get the number of IO tasks set.
 *
 * @param iosysid the IO system ID
 * @param numiotasks a pointer taht gets the number of IO
 * tasks. Ignored if NULL.
 * @returns 0 on success, error code otherwise
 * @author Ed Hartnett
 */
int PIOc_get_numiotasks(int iosysid, int *numiotasks)
{
    iosystem_desc_t *ios;

    LOG((1, "PIOc_get_numiotasks(iosysid=%d)", iosysid));

    if (!(ios = pio_get_iosystem_from_id(iosysid)))
    {
        return pio_err(NULL, NULL, PIO_EBADID, __FILE__, __LINE__,
                        "Getting number of I/O tasks on IO system failed. Invalid io system id (%d) provided", iosysid);
    }

    if (numiotasks)
        *numiotasks = ios->num_iotasks;

    return PIO_NOERR;
}

/**
 * Get the local size of the variable.
 *
 * @param ioid IO description ID of an IO decomposition that represents the
 *        variable layout.
 * @returns the local size (size on the local process) of the variable.
 * @author Jim Edwards
 */
int PIOc_get_local_array_size(int ioid)
{
    io_desc_t *iodesc;

    LOG((1, "PIOc_get_local_array_size(ioid=%d)", ioid));

    if (!(iodesc = pio_get_iodesc_from_id(ioid)))
        piodie(__FILE__, __LINE__, "Getting local array size failed. Invalid iodesc id provided. Could not get iodesc corresponding to ioid = %d", ioid);

    return iodesc->ndof;
}

/**
 * Set the error handling method used for subsequent calls. This
 * function is deprecated. New code should use
 * PIOc_set_iosystem_error_handling(). This method has no way to
 * return an error, so any failure will result in MPI_Abort.
 *
 * @param iosysid the IO system ID
 * @param method the error handling method
 * @returns old error handler
 * @ingroup PIO_error_method
 * @author Jim Edwards
 */
int PIOc_Set_IOSystem_Error_Handling(int iosysid, int method)
{
    iosystem_desc_t *ios;
    int oldmethod;

    LOG((1, "PIOc_Set_IOSystem_Error_Handling(iosysid=%d, method=%d)", iosysid, method));

    /* Get the iosystem info. */
    if (iosysid != PIO_DEFAULT)
        if (!(ios = pio_get_iosystem_from_id(iosysid)))
            piodie(__FILE__, __LINE__, "Setting error handler for the IO system failed. Invalid iosystem id (%d) provided. Could not find IO system corresponding to the iosystem id", iosysid);

    /* Set the error handler. */
    if (PIOc_set_iosystem_error_handling(iosysid, method, &oldmethod))
        piodie(__FILE__, __LINE__, "Setting error handler for the IO system (id = %d) failed. Internal error.", iosysid);

    return oldmethod;
}

/**
 * Set the error handling method used for subsequent calls for this IO
 * system.
 *
 * @param iosysid the IO system ID. Passing PIO_DEFAULT instead
 * changes the default error handling for the library.
 * @param method the error handling method
 * @param old_method pointer to int that will get old method. Ignored
 * if NULL.
 * @returns 0 for success, error code otherwise.
 * @ingroup PIO_error_method
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_set_iosystem_error_handling(int iosysid, int method, int *old_method)
{
    iosystem_desc_t *ios = NULL;
    int ret = PIO_NOERR;

    LOG((1, "PIOc_set_iosystem_error_handling iosysid = %d method = %d", iosysid,
         method));

    /* Find info about this iosystem. */
    if (iosysid != PIO_DEFAULT)
        if (!(ios = pio_get_iosystem_from_id(iosysid)))
        {
            return pio_err(NULL, NULL, PIO_EBADID, __FILE__, __LINE__,
                            "Setting error handler for the IO system failed. Invalid iosystem id (%d) provided. Could not find an iosystem assocaited with the id", iosysid);
        }

    /* Check that valid error handler was provided. */
    if (method != PIO_INTERNAL_ERROR && method != PIO_BCAST_ERROR &&
        method != PIO_REDUCE_ERROR && method != PIO_RETURN_ERROR)
    {
        return pio_err(ios, NULL, PIO_EINVAL, __FILE__, __LINE__,
                        "Setting error handler for the IO system failed. Invalid error handler method (%d:%s) provided for iosystem (iosysid=%d)", method, PIO_error_handler_to_string(method), iosysid);
    }

    /* If using async, and not an IO task, then send parameters. */
    if (iosysid != PIO_DEFAULT)
        if (ios->async)
        {
            int msg = PIO_MSG_SETERRORHANDLING;
            bool old_method_present = old_method ? true : false;

            PIO_SEND_ASYNC_MSG(ios, msg, &ret, method, old_method_present);
            if(ret != PIO_NOERR)
            {
                return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                                "Setting error handler for the IO system failed. Error sending async msg PIO_MSG_SETERRORHANDLING (iosysid=%d)", iosysid);
            }
        }

    /* Return the current handler. */
    if (old_method)
        *old_method = iosysid == PIO_DEFAULT ? default_error_handler : ios->error_handler;

    /* Set new error handler. */
    if (iosysid == PIO_DEFAULT)
        default_error_handler = method;
    else
        ios->error_handler = method;

    return PIO_NOERR;
}

/* Create a unique string/name using information provided by the user */
int pio_create_uniq_str(iosystem_desc_t *ios, io_desc_t *iodesc, char *str, int len, const char *prefix, const char *suffix)
{
    static int counter = 0;
    const char *DEFAULT_PREFIX = "pio";
    const char *DEFAULT_SUFFIX = ".dat";
    const int HUNDRED = 100;
    const char *INT_FMT_LT_HUNDRED = "%2.2d";
    const int TEN_THOUSAND = 1000;
    const char *INT_FMT_LT_TEN_THOUSAND = "%4.4d";
    const int MILLION = 1000000;
    const char *INT_FMT_LT_MILLION = "%6.6d";

    assert(str && (len > 0));

    if(!prefix)
    {
        prefix = DEFAULT_PREFIX;
    }
    if(!suffix)
    {
        suffix = DEFAULT_SUFFIX;
    }

    int rem_len = len;
    char *sptr = str;

    /* Add prefix */
    int prefix_len = strlen(prefix);
    assert(rem_len > prefix_len);
    snprintf(sptr, rem_len, "%s", prefix);
    rem_len -= prefix_len;
    sptr += prefix_len;

    if(ios)
    {
        /* Add ios specific info into the str */
        assert(ios->num_comptasks < MILLION);
        assert(rem_len > 0);
        const char *num_comptasks_fmt = (ios->num_comptasks < HUNDRED) ? (INT_FMT_LT_HUNDRED) : ((ios->num_comptasks < TEN_THOUSAND) ? (INT_FMT_LT_TEN_THOUSAND): INT_FMT_LT_MILLION);
        const char *num_iotasks_fmt = num_comptasks_fmt;
        snprintf(sptr, rem_len, num_comptasks_fmt, ios->num_comptasks);
        rem_len = len - strlen(str);
        sptr = str + strlen(str);
        snprintf(sptr, rem_len, "%s", "tasks");
        rem_len = len - strlen(str);
        sptr = str + strlen(str);
        snprintf(sptr, rem_len, num_iotasks_fmt, ios->num_iotasks);
        rem_len = len - strlen(str);
        sptr = str + strlen(str);
        snprintf(sptr, rem_len, "%s", "io");
        rem_len = len - strlen(str);
        sptr = str + strlen(str);
    }

    if(iodesc)
    {
        /* Add iodesc specific info into the str */
        assert(iodesc->ndims < MILLION);
        assert(rem_len > 0);
        const char *ndims_fmt = (iodesc->ndims < HUNDRED) ? (INT_FMT_LT_HUNDRED) : ((iodesc->ndims < TEN_THOUSAND) ? (INT_FMT_LT_TEN_THOUSAND): INT_FMT_LT_MILLION);
        snprintf(sptr, rem_len, ndims_fmt, iodesc->ndims);
        rem_len = len - strlen(str);
        sptr = str + strlen(str);
        snprintf(sptr, rem_len, "%s", "dims");
        rem_len = len - strlen(str);
        sptr = str + strlen(str);
    }

    /* Add counter - to make the str unique */
    assert(counter < MILLION);
    const char *counter_fmt = (counter < HUNDRED) ? (INT_FMT_LT_HUNDRED) : ((counter < TEN_THOUSAND) ? (INT_FMT_LT_TEN_THOUSAND): INT_FMT_LT_MILLION);
    snprintf(sptr, rem_len, counter_fmt, counter);
    rem_len = len - strlen(str);
    sptr = str + strlen(str);

    counter++;

    /* Add suffix */
    int suffix_len = strlen(suffix);
    assert(rem_len > suffix_len);
    snprintf(sptr, rem_len, "%s", suffix);
    rem_len -= suffix_len;
    sptr += suffix_len;

    return PIO_NOERR;
}

/**
 * Initialize the decomposition used with distributed arrays. The
 * decomposition describes how the data will be distributed between
 * tasks.
 *
 * Internally, this function will:
 * <ul>
 * <li>Allocate and initialize an iodesc struct for this
 * decomposition. (This also allocates an io_region struct for the
 * first region.)
 * <li>(Box rearranger only) If iostart or iocount are NULL, call
 * CalcStartandCount() to determine starts/counts. Then call
 * compute_maxIObuffersize() to compute the max IO buffer size needed.
 * <li>Create the rearranger.
 * <li>Assign an ioid and add this decomposition to the list of open
 * decompositions.
 * </ul>
 *
 * @param iosysid the IO system ID.
 * @param pio_type the basic PIO data type used.
 * @param ndims the number of dimensions in the variable, not
 * including the unlimited dimension.
 * @param gdimlen an array length ndims with the sizes of the global
 * dimensions.
 * @param maplen the local length of the compmap array.
 * @param compmap a 1 based array of offsets into the array record on
 * file. A 0 in this array indicates a value which should not be
 * transfered.
 * @param ioidp pointer that will get the io description ID.
 * @param rearranger pointer to the rearranger to be used for this
 * decomp or NULL to use the default.
 * @param iostart An array of start values for block cyclic
 * decompositions for the SUBSET rearranger. Ignored if block
 * rearranger is used. If NULL and SUBSET rearranger is used, the
 * iostarts are generated.
 * @param iocount An array of count values for block cyclic
 * decompositions for the SUBSET rearranger. Ignored if block
 * rearranger is used. If NULL and SUBSET rearranger is used, the
 * iostarts are generated.
 * @returns 0 on success, error code otherwise
 * @ingroup PIO_initdecomp
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_InitDecomp(int iosysid, int pio_type, int ndims, const int *gdimlen, int maplen,
                    const PIO_Offset *compmap, int *ioidp, const int *rearranger,
                    const PIO_Offset *iostart, const PIO_Offset *iocount)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    io_desc_t *iodesc;     /* The IO description. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function calls. */
    int ierr;              /* Return code. */

#ifdef TIMING
    GPTLstart("PIO:PIOc_initdecomp");
#endif
    LOG((1, "PIOc_InitDecomp iosysid = %d pio_type = %d ndims = %d maplen = %d",
         iosysid, pio_type, ndims, maplen));

    /* Get IO system info. */
    if (!(ios = pio_get_iosystem_from_id(iosysid)))
    {
        return pio_err(NULL, NULL, PIO_EBADID, __FILE__, __LINE__,
                        "Initializing the PIO decomposition failed. Invalid io system id (%d) provided. Could not find an iosystem associated with the id", iosysid);
    }

    /* Caller must provide these. */
    if (!gdimlen || !compmap || !ioidp)
    {
        return pio_err(ios, NULL, PIO_EINVAL, __FILE__, __LINE__,
                        "Initializing the PIO decomposition failed. Invalid pointers (NULL) to gdimlen(%s) or compmap(%s) or ioidp (%s) provided", (gdimlen) ? "not NULL" : "NULL", (compmap) ? "not NULL" : "NULL", (ioidp) ? "not NULL" : "NULL");
    }

    /* Check the dim lengths. */
    for (int i = 0; i < ndims; i++)
        if (gdimlen[i] <= 0)
        {
            return pio_err(ios, NULL, PIO_EINVAL, __FILE__, __LINE__,
                            "Initializing the PIO decomposition failed. Invalid value for global dimension lengths provided. The global length of dimension %d is provided as %d (expected > 0)", i, gdimlen[i]);
        }

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_INITDECOMP_DOF; /* Message for async notification. */
        char rearranger_present = rearranger ? true : false;
        int amsg_rearranger = (rearranger) ? (*rearranger) : 0;
        char iostart_present = iostart ? true : false;
        char iocount_present = iocount ? true : false;
        PIO_Offset *amsg_iostart = NULL, *amsg_iocount = NULL;

        if(!iostart_present)
        {
            amsg_iostart = calloc(ndims, sizeof(PIO_Offset));
        }
        if(!iocount_present)
        {
            amsg_iocount = calloc(ndims, sizeof(PIO_Offset));
        }
        if(!amsg_iostart || !amsg_iocount)
        {
            return pio_err(ios, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                            "Initializing the PIO decomposition failed. Out of memory allocating %lld bytes for start array and %lld bytes for count array for sending asynchronous message, PIO_MSG_INITDECOMP_DOF, on iosystem (iosysid=%d)", (unsigned long long) (ndims * sizeof(PIO_Offset)), (unsigned long long) (ndims * sizeof(PIO_Offset)), ios->iosysid);
        }

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, iosysid, pio_type, ndims,
            gdimlen, maplen, compmap, rearranger_present, amsg_rearranger,
            iostart_present, ndims,
            (iostart_present) ? iostart : amsg_iostart,
            iocount_present, ndims,
            (iocount_present) ? iocount : amsg_iocount);

        if(!iostart_present)
        {
            free(amsg_iostart);
        }
        if(!iocount_present)
        {
            free(amsg_iocount);
        }
    }

    /* Allocate space for the iodesc info. This also allocates the
     * first region and copies the rearranger opts into this
     * iodesc. */
    if ((ierr = malloc_iodesc(ios, pio_type, ndims, &iodesc)))
    {
        return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                        "Initializing the PIO decomposition failed. Out of memory allocating memory for I/O descriptor");
    }

    /* Remember the maplen. */
    iodesc->maplen = maplen;

    /* Remember the map. */
    if (!(iodesc->map = malloc(sizeof(PIO_Offset) * maplen)))
    {
        return pio_err(ios, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                        "Initializing the PIO decomposition failed. Out of memory allocating %lld bytes to store I/O decomposition map", (unsigned long long) (sizeof(PIO_Offset) * maplen));
    }
    for (int m = 0; m < maplen; m++)
        iodesc->map[m] = compmap[m];

    /* Remember the dim sizes. */
    if (!(iodesc->dimlen = malloc(sizeof(int) * ndims)))
    {
        return pio_err(ios, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                        "Initializing the PIO decomposition failed. Out of memory allocating %lld bytes for dimension sizes in the I/O decomposition map", (unsigned long long) (sizeof(int) * ndims));
    }
    for (int d = 0; d < ndims; d++)
        iodesc->dimlen[d] = gdimlen[d];

    /* Set the rearranger. */
    if (!rearranger)
        iodesc->rearranger = ios->default_rearranger;
    else
        iodesc->rearranger = *rearranger;
    LOG((2, "iodesc->rearranger = %d", iodesc->rearranger));

    /* Is this the subset rearranger? */
    if (iodesc->rearranger == PIO_REARR_SUBSET)
    {
        iodesc->num_aiotasks = ios->num_iotasks;
        LOG((2, "creating subset rearranger iodesc->num_aiotasks = %d",
             iodesc->num_aiotasks));
        if ((ierr = subset_rearrange_create(ios, maplen, (PIO_Offset *)compmap, gdimlen,
                                            ndims, iodesc)))
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Initializing the PIO decomposition failed. Error creating the SUBSET rearranger");
        }
    }
    else /* box rearranger */
    {
        if (ios->ioproc)
        {
            /*  Unless the user specifies the start and count for each
             *  IO task compute it. */
            if (iostart && iocount)
            {
                LOG((3, "iostart and iocount provided"));
                for (int i = 0; i < ndims; i++)
                {
                    iodesc->firstregion->start[i] = iostart[i];
                    iodesc->firstregion->count[i] = iocount[i];
                }
                iodesc->num_aiotasks = ios->num_iotasks;
            }
            else
            {
                /* Compute start and count values for each io task. */
                LOG((2, "about to call CalcStartandCount pio_type = %d ndims = %d", pio_type, ndims));
                if ((ierr = CalcStartandCount(pio_type, ndims, gdimlen, ios->num_iotasks,
                                             ios->io_rank, iodesc->firstregion->start,
                                             iodesc->firstregion->count, &iodesc->num_aiotasks)))
                {
                    return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                                    "Initializing the PIO decomposition failed. Internal error calculating start/count for the decomposition");
                }
            }

            /* Compute the max io buffer size needed for an iodesc. */
            if ((ierr = compute_maxIObuffersize(ios->io_comm, iodesc)))
            {
                return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                                "Initializing the PIO decomposition failed. Internal error computing max io buffer size needed for the decomposition");
            }
            LOG((3, "compute_maxIObuffersize called iodesc->maxiobuflen = %d",
                 iodesc->maxiobuflen));
        }

        /* Depending on array size and io-blocksize the actual number
         * of io tasks used may vary. */
        if ((mpierr = MPI_Bcast(&(iodesc->num_aiotasks), 1, MPI_INT, ios->ioroot,
                                ios->my_comm)))
            return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
        LOG((3, "iodesc->num_aiotasks = %d", iodesc->num_aiotasks));

        /* Compute the communications pattern for this decomposition. */
        if (iodesc->rearranger == PIO_REARR_BOX)
            if ((ierr = box_rearrange_create(ios, maplen, compmap, gdimlen, ndims, iodesc)))
            {
                return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                                "Error initializing the PIO decomposition. Error creating the BOX rearranger");
            }
    }

    /* Add this IO description to the list. */
    MPI_Comm comm = MPI_COMM_NULL;
#ifdef _ADIOS2
    comm = ios->union_comm;
#endif
    if(ios->async)
    {
        /* For asynchronous I/O service, the iodescs (iodesc ids) need to
         * be unique across the union_comm (union of I/O and compute comms)
         */
        comm = ios->union_comm;
    }
    *ioidp = pio_add_to_iodesc_list(iodesc, comm);

    /* Check whether we have exceeded the maximum number of ioids (PIO_IODESC_MAX_IDS).
     * This limit is necessary since each file uses a sparse pointer array (with a fixed
     * size of PIO_IODESC_MAX_IDS) to look up a data buffer per ioid.
     * FIXME: Replace the sparse array with a map or hash map to get rid of this limit
     */
    if (*ioidp - PIO_IODESC_START_ID + 1 > PIO_IODESC_MAX_IDS)
    {
        return pio_err(ios, NULL, PIO_EINTERNAL, __FILE__, __LINE__,
                       "Initializing the PIO decomposition failed. Maximum number of ioids (limit = %d) has been reached", PIO_IODESC_MAX_IDS);
    }

#if PIO_SAVE_DECOMPS
    if(pio_save_decomps_regex_match(*ioidp, NULL, NULL))
    {
        char filename[PIO_MAX_NAME];
        ierr = pio_create_uniq_str(ios, iodesc, filename, PIO_MAX_NAME, "piodecomp", ".dat");
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Initializing the PIO decomposition failed. Creating a unique file name for saving the decomposition failed");
        }
        LOG((2, "Saving decomp map to %s", filename));
        PIOc_writemap(filename, *ioidp, ndims, gdimlen, maplen, (PIO_Offset *)compmap, ios->my_comm);
        iodesc->is_saved = true;
    }
#endif

#if PIO_ENABLE_LOGGING
    /* Log results. */
    LOG((2, "iodesc ioid = %d nrecvs = %d ndof = %d ndims = %d num_aiotasks = %d "
         "rearranger = %d maxregions = %d needsfill = %d llen = %d maxiobuflen  = %d",
         iodesc->ioid, iodesc->nrecvs, iodesc->ndof, iodesc->ndims, iodesc->num_aiotasks,
         iodesc->rearranger, iodesc->maxregions, iodesc->needsfill, iodesc->llen,
         iodesc->maxiobuflen));
    if (ios->ioproc)
    {
        if (iodesc->rearranger == PIO_REARR_SUBSET)
        {
            for (int j = 0; j < iodesc->llen; j++)
                LOG((3, "rindex[%d] = %lld", j, iodesc->rindex[j]));
        }
        else
        {
            int totalrecv = 0;
            for (int j = 0; j < iodesc->nrecvs; j++)
                totalrecv += iodesc->rcount[j];

            for (int j = 0; j < totalrecv; j++)
                LOG((3, "rindex[%d] = %lld", j, iodesc->rindex[j]));
        }
    }
#endif /* PIO_ENABLE_LOGGING */            

    /* This function only does something if pre-processor macro
     * PERFTUNE is set. */
    performance_tune_rearranger(ios, iodesc);

#ifdef TIMING
    GPTLstop("PIO:PIOc_initdecomp");
#endif
    return PIO_NOERR;
}

/**
 * Initialize the decomposition used with distributed arrays. The
 * decomposition describes how the data will be distributed between
 * tasks.
 *
 * @param iosysid the IO system ID.
 * @param pio_type the basic PIO data type used.
 * @param ndims the number of dimensions in the variable, not
 * including the unlimited dimension.
 * @param gdimlen an array length ndims with the sizes of the global
 * dimensions.
 * @param maplen the local length of the compmap array.
 * @param compmap a 0 based array of offsets into the array record on
 * file. A -1 in this array indicates a value which should not be
 * transfered.
 * @param ioidp pointer that will get the io description ID.
 * @param rearranger the rearranger to be used for this decomp or 0 to
 * use the default. Valid rearrangers are PIO_REARR_BOX and
 * PIO_REARR_SUBSET.
 * @param iostart An array of start values for block cyclic
 * decompositions. If NULL ???
 * @param iocount An array of count values for block cyclic
 * decompositions. If NULL ???
 * @returns 0 on success, error code otherwise
 * @ingroup PIO_initdecomp
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_init_decomp(int iosysid, int pio_type, int ndims, const int *gdimlen, int maplen,
                     const PIO_Offset *compmap, int *ioidp, int rearranger,
                     const PIO_Offset *iostart, const PIO_Offset *iocount)
{
    PIO_Offset compmap_1_based[maplen];
    int *rearrangerp = NULL;

    LOG((1, "PIOc_init_decomp iosysid = %d pio_type = %d ndims = %d maplen = %d",
         iosysid, pio_type, ndims, maplen));

    /* If the user specified a non-default rearranger, use it. */
    if (rearranger)
        rearrangerp = &rearranger;

    /* Add 1 to all elements in compmap. */
    for (int e = 0; e < maplen; e++)
    {
        LOG((3, "zero-based compmap[%d] = %d", e, compmap[e]));
        compmap_1_based[e] = compmap[e] + 1;
    }

    /* Call the legacy version of the function. */
    return PIOc_InitDecomp(iosysid, pio_type, ndims, gdimlen, maplen, compmap_1_based,
                           ioidp, rearrangerp, iostart, iocount);
}

/**
 * This is a simplified initdecomp which can be used if the memory
 * order of the data can be expressed in terms of start and count on
 * the file. In this case we compute the compdof.
 *
 * @param iosysid the IO system ID
 * @param pio_type
 * @param ndims the number of dimensions
 * @param gdimlen an array length ndims with the sizes of the global
 * dimensions.
 * @param start start array
 * @param count count array
 * @param pointer that gets the IO ID.
 * @returns 0 for success, error code otherwise
 * @ingroup PIO_initdecomp
 * @author Jim Edwards
 */
int PIOc_InitDecomp_bc(int iosysid, int pio_type, int ndims, const int *gdimlen,
                       const long int *start, const long int *count, int *ioidp)

{
    iosystem_desc_t *ios;
    int n, i, maplen = 1;
    PIO_Offset prod[ndims], loc[ndims];
    int rearr = PIO_REARR_SUBSET;

    LOG((1, "PIOc_InitDecomp_bc iosysid = %d pio_type = %d ndims = %d"));

    /* Get the info about the io system. */
    if (!(ios = pio_get_iosystem_from_id(iosysid)))
    {
        return pio_err(NULL, NULL, PIO_EBADID, __FILE__, __LINE__,
                        "Initializing the PIO decomposition failed. Invalid io system id (%d) provided", iosysid);
    }

    /* Check for required inputs. */
    if (!gdimlen || !start || !count || !ioidp)
    {
        return pio_err(ios, NULL, PIO_EINVAL, __FILE__, __LINE__,
                        "Initializing the PIO decomposition failed. Invalid (NULL) pointers to gdimlen (%s) or start (%s) or count (%s) or ioidp (%s) provided", (gdimlen) ? "not NULL" : "NULL", (start) ? "not NULL" : "NULL", (count) ? "not NULL" : "NULL", (ioidp) ? "not NULL" : "NULL");
    }

    /* Check that dim, start, and count values are not obviously
     * incorrect. */
    for (int i = 0; i < ndims; i++)
        if (gdimlen[i] <= 0 || start[i] < 0 || count[i] < 0 || (start[i] + count[i]) > gdimlen[i])
        {
            return pio_err(ios, NULL, PIO_EINVAL, __FILE__, __LINE__,
                            "Initializing the PIO decomposition failed. Invalid arguments provided : gdimlen[%d]=%d (expected > 0), start[%d]=%ld (expected >= 0), count[%d]=%ld (expected >= 0), start[%d] + count[%d] = %ld (expected <= gdimlen[%d])", i, gdimlen[i], i, start[i], i, count[i], i, i, start[i]+count[i], i);
        }

    /* Find the maplen. */
    for (i = 0; i < ndims; i++)
        maplen *= count[i];

    /* Get storage for the compmap. */
    PIO_Offset compmap[maplen];

    /* Find the compmap. */
    prod[ndims - 1] = 1;
    loc[ndims - 1] = 0;
    for (n = ndims - 2; n >= 0; n--)
    {
        prod[n] = prod[n + 1] * gdimlen[n + 1];
        loc[n] = 0;
    }
    for (i = 0; i < maplen; i++)
    {
        compmap[i] = 1;
        for (n = ndims - 1; n >= 0; n--)
            compmap[i] += (start[n] + loc[n]) * prod[n];

        n = ndims - 1;
        loc[n] = (loc[n] + 1) % count[n];
        while (loc[n] == 0 && n > 0)
        {
            n--;
            loc[n] = (loc[n] + 1) % count[n];
        }
    }

    return PIOc_InitDecomp(iosysid, pio_type, ndims, gdimlen, maplen, compmap, ioidp,
                           &rearr, NULL, NULL);
}

#ifdef _ADIOS2
static unsigned long adios2_io_cnt = 0;

unsigned long get_adios2_io_cnt()
{
    return adios2_io_cnt++;
}
#endif

/**
 * Library initialization used when IO tasks are a subset of compute
 * tasks.
 *
 * This function creates an MPI intracommunicator between a set of IO
 * tasks and one or more sets of computational tasks.
 *
 * The caller must create all comp_comm and the io_comm MPI
 * communicators before calling this function.
 *
 * Internally, this function does the following:
 *
 * <ul>
 * <li>Initialize logging system (if PIO_ENABLE_LOGGING is set).
 * <li>Allocates and initializes the iosystem_desc_t struct (ios).
 * <li>MPI duplicated user comp_comm to ios->comp_comm and
 * ios->union_comm.
 * <li>Set ios->my_comm to be ios->comp_comm. (Not an MPI
 * duplication.)
 * <li>Find MPI rank in comp_comm, determine ranks of IO tasks,
 * determine whether this task is one of the IO tasks.
 * <li>Identify the root IO tasks.
 * <li>Create MPI groups for IO tasks, and for computation tasks.
 * <li>On IO tasks, create an IO communicator (ios->io_comm).
 * <li>Assign an iosystemid, and put this iosystem_desc_t into the
 * list of open iosystems.
 * <li>Initialize the bget buffer, unless PIO_USE_MALLOC was used.
 * </ul>
 *
 * When complete, there are three MPI communicators (ios->comp_comm,
 * ios->union_comm, and ios->io_comm), and two MPI groups
 * (ios->compgroup and ios->iogroup) that must be freed by MPI.
 *
 * @param comp_comm the MPI_Comm of the compute tasks.
 * @param num_iotasks the number of io tasks to use.
 * @param stride the offset between io tasks in the comp_comm.
 * @param base the comp_comm index of the first io task.
 * @param rearr the rearranger to use by default, this may be
 * overriden in the PIO_init_decomp(). The rearranger is not used
 * until the decomposition is initialized.
 * @param iosysidp index of the defined system descriptor.
 * @return 0 on success, otherwise a PIO error code.
 * @ingroup PIO_init
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_Init_Intracomm(MPI_Comm comp_comm, int num_iotasks, int stride, int base,
                        int rearr, int *iosysidp)
{
    iosystem_desc_t *ios;
    int ustride;
    int num_comptasks; /* The size of the comp_comm. */
    int mpierr;        /* Return value for MPI calls. */
    int ret;           /* Return code for function calls. */

/* TIMING_INTERNAL implies that the timing statistics are gathered/
 * displayed by pio
 */
#ifdef TIMING
#ifdef TIMING_INTERNAL
    pio_init_gptl();
#endif
    GPTLstart("PIO:PIOc_Init_Intracomm");
#endif
    /* Turn on the logging system. */
    pio_init_logging();

#ifdef PIO_MICRO_TIMING
    /* Initialize the timer framework - MPI_Wtime() + output from root proc */
    ret = mtimer_init(PIO_MICRO_MPI_WTIME_ROOT);
    if(ret != PIO_NOERR)
    {
        return pio_err(NULL, NULL, PIO_EINTERNAL, __FILE__, __LINE__,
                        "PIO Init failed, initializing PIO micro timers failed (ret=%d)", ret);
    }
#endif

    /* Find the number of computation tasks. */
    if ((mpierr = MPI_Comm_size(comp_comm, &num_comptasks)))
        return check_mpi(NULL, NULL, mpierr, __FILE__, __LINE__);

    /* Check the inputs. */
    if (!iosysidp ||
        num_iotasks < 1 || num_iotasks > num_comptasks ||
        stride < 1 ||
        base < 0 || base >= num_comptasks ||
        stride * (num_iotasks - 1) >= num_comptasks)
    {
        return pio_err(NULL, NULL, PIO_EINVAL, __FILE__, __LINE__,
                        "PIO Init failed. Invalid arguments provided. Pointer to iosysid is %s (expected not NULL), num_iotasks=%d (expected >= 1 && <= num_comptasks, %d), stride = %d (expected >= 1), base = %d (expected >= 0 && < num_comptasks, %d), stride * (num_iotasks - 1) = %d (expected < num_comptasks, %d)", (iosysidp) ? "not NULL" : "NULL", num_iotasks, num_comptasks, stride, base, num_comptasks, stride * (num_iotasks - 1), num_comptasks);
    }

    LOG((1, "PIOc_Init_Intracomm comp_comm = %d num_iotasks = %d stride = %d base = %d "
         "rearr = %d", comp_comm, num_iotasks, stride, base, rearr));

    /* Allocate memory for the iosystem info. */
    if (!(ios = calloc(1, sizeof(iosystem_desc_t))))
    {
        return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                        "PIO Init failed. Out of memory allocating %lld bytes for I/O system descriptor", (unsigned long long) sizeof(iosystem_desc_t));
    }

    ios->io_comm = MPI_COMM_NULL;
    ios->intercomm = MPI_COMM_NULL;
    ios->error_handler = default_error_handler;
    ios->default_rearranger = rearr;
    ios->num_iotasks = num_iotasks;
    ios->num_comptasks = num_comptasks;

    /* For non-async, the IO tasks are a subset of the comptasks. */
    ios->num_uniontasks = num_comptasks;

    /* Initialize the rearranger options. */
    init_rearr_opts(ios);
    
    /* Copy the computation communicator into union_comm. */
    if ((mpierr = MPI_Comm_dup(comp_comm, &ios->union_comm)))
        return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);

#ifdef _ADIOS2
    /* Initialize ADIOS for each io system */
    ios->adiosH = adios2_init(ios->union_comm, adios2_debug_mode_on);
    if (ios->adiosH == NULL)
    {
        return pio_err(ios, NULL, PIO_EADIOS2ERR, __FILE__, __LINE__, "Initializing ADIOS failed");
    }
#endif

    /* Copy the computation communicator into comp_comm. */
    if ((mpierr = MPI_Comm_dup(comp_comm, &ios->comp_comm)))
        return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
    LOG((2, "union_comm = %d comp_comm = %d", ios->union_comm, ios->comp_comm));

    ios->my_comm = ios->comp_comm;
    ustride = stride;

    /* Find MPI rank in comp_comm communicator. */
    if ((mpierr = MPI_Comm_rank(ios->comp_comm, &ios->comp_rank)))
        return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);

    /* With non-async, all tasks are part of computation component. */
    ios->compproc = true;

    /* Create an array that holds the ranks of the tasks to be used
     * for computation. */
    if (!(ios->compranks = calloc(ios->num_comptasks, sizeof(int))))
    {
        return pio_err(ios, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                        "PIO Init failed. Out of memory allocating %lld bytes for array of compute process ranks in the I/O descriptor", (unsigned long long) (ios->num_comptasks * sizeof(int)));
    }
    for (int i = 0; i < ios->num_comptasks; i++)
        ios->compranks[i] = i;

    /* Is this the comp master? */
    if (ios->comp_rank == 0)
        ios->compmaster = MPI_ROOT;
    LOG((2, "comp_rank = %d num_comptasks = %d", ios->comp_rank, ios->num_comptasks));

    /* Create an array that holds the ranks of the tasks to be used
     * for IO. */
    if (!(ios->ioranks = calloc(ios->num_iotasks, sizeof(int))))
    {
        return pio_err(ios, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                        "PIO Init failed. Out of memory allocating %lld bytes for array of I/O process ranks in the I/O descriptor", (unsigned long long) (ios->num_iotasks * sizeof(int)));
    }
    for (int i = 0; i < ios->num_iotasks; i++)
    {
        ios->ioranks[i] = (base + i * ustride) % ios->num_comptasks;
        if (ios->ioranks[i] == ios->comp_rank)
            ios->ioproc = true;
        LOG((3, "ios->ioranks[%d] = %d", i, ios->ioranks[i]));
    }
    ios->ioroot = ios->ioranks[0];

    /* We are not providing an info object. */
    ios->info = MPI_INFO_NULL;

    /* Identify the task that will be the root of the IO communicator. */
    if (ios->comp_rank == ios->ioranks[0])
        ios->iomaster = MPI_ROOT;

    /* Create a group for the computation tasks. */
    if ((mpierr = MPI_Comm_group(ios->comp_comm, &ios->compgroup)))
        return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);

    /* Create a group for the IO tasks. */
    if ((mpierr = MPI_Group_incl(ios->compgroup, ios->num_iotasks, ios->ioranks,
                                 &ios->iogroup)))
        return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);

    /* Create an MPI communicator for the IO tasks. */
    if ((mpierr = MPI_Comm_create(ios->comp_comm, ios->iogroup, &ios->io_comm)))
        return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);

    /* For the tasks that are doing IO, get their rank within the IO
     * communicator. If they are not doing IO, set their io_rank to
     * -1. */
    if (ios->ioproc)
    {
        if ((mpierr = MPI_Comm_rank(ios->io_comm, &ios->io_rank)))
            return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
    }
    else
        ios->io_rank = -1;
    LOG((3, "ios->io_comm = %d ios->io_rank = %d", ios->io_comm, ios->io_rank));

    /* Rank in the union comm is the same as rank in the comp comm. */
    ios->union_rank = ios->comp_rank;

    /* Async I/O service message info - not used here */
    ios->async_ios_msg_info.seq_num = PIO_MSG_START_SEQ_NUM;
    ios->async_ios_msg_info.prev_msg = PIO_MSG_INVALID;

    /* Add this ios struct to the list in the PIO library. */
    *iosysidp = pio_add_to_iosystem_list(ios, MPI_COMM_NULL);

    /* Allocate buffer space for compute nodes. */
    if ((ret = compute_buffer_init(ios)))
    {
        return pio_err(ios, NULL, ret, __FILE__, __LINE__,
                        "PIO Init failed. Internal error allocating buffer space on compute processes to cache user data");
    }

    LOG((2, "Init_Intracomm complete iosysid = %d", *iosysidp));

#ifdef TIMING
    GPTLstop("PIO:PIOc_Init_Intracomm");
#endif
    return PIO_NOERR;
}

/**
 * Interface to call from pio_init from fortran.
 *
 * @param f90_comp_comm
 * @param num_iotasks the number of IO tasks
 * @param stride the stride to use assigning tasks
 * @param base the starting point when assigning tasks
 * @param rearr the rearranger
 * @param rearr_opts the rearranger options
 * @param iosysidp a pointer that gets the IO system ID
 * @returns 0 for success, error code otherwise
 * @author Jim Edwards
 */
int PIOc_Init_Intracomm_from_F90(int f90_comp_comm,
                                 const int num_iotasks, const int stride,
                                 const int base, const int rearr,
                                 rearr_opt_t *rearr_opts, int *iosysidp)
{
    int ret = PIO_NOERR;
    fortran_order = true;
    ret = PIOc_Init_Intracomm(MPI_Comm_f2c(f90_comp_comm), num_iotasks,
                              stride, base, rearr,
                              iosysidp);
    if (ret != PIO_NOERR)
    {
        return pio_err(NULL, NULL, ret, __FILE__, __LINE__,
                        "PIO Init (F2C) failed");
    }

    if (rearr_opts)
    {
        LOG((1, "Setting rearranger options, iosys=%d", *iosysidp));
        ret = PIOc_set_rearr_opts(*iosysidp, rearr_opts->comm_type,
                                   rearr_opts->fcd,
                                   rearr_opts->comp2io.hs,
                                   rearr_opts->comp2io.isend,
                                   rearr_opts->comp2io.max_pend_req,
                                   rearr_opts->io2comp.hs,
                                   rearr_opts->io2comp.isend,
                                   rearr_opts->io2comp.max_pend_req);
        if (ret != PIO_NOERR)
        {
            return pio_err(NULL, NULL, ret, __FILE__, __LINE__,
                            "PIO Init (F2C) failed. Setting the rearranger options failed");
        }
    }
    return ret;
}

/**
 * Send a hint to the MPI-IO library.
 *
 * @param iosysid the IO system ID
 * @param hint the hint for MPI
 * @param hintval the value of the hint
 * @returns 0 for success, or PIO_BADID if iosysid can't be found.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_set_hint(int iosysid, const char *hint, const char *hintval)
{
    iosystem_desc_t *ios;
    int mpierr; /* Return value for MPI calls. */

    /* Get the iosysid. */
    if (!(ios = pio_get_iosystem_from_id(iosysid)))
    {
        return pio_err(NULL, NULL, PIO_EBADID, __FILE__, __LINE__,
                        "Setting PIO hints failed. Invalid io system id (%d) provided", iosysid);
    }

    /* User must provide these. */
    if (!hint || !hintval)
    {
        return pio_err(ios, NULL, PIO_EINVAL, __FILE__, __LINE__,
                        "Setting PIO hints failed. Invalid pointers (NULL) to hint (%s) or hintval (%s) provided", (hint) ? "not NULL" : "NULL", (hintval) ? "not NULL" : "NULL");
    }

    LOG((1, "PIOc_set_hint hint = %s hintval = %s", hint, hintval));

    /* Make sure we have an info object. */
    if (ios->info == MPI_INFO_NULL)
        if ((mpierr = MPI_Info_create(&ios->info)))
        {
            LOG((1, "ERROR: Setting PIO hints failed. Creating MPI Info object failed (mpierr = %d)", mpierr));
            return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
        }

    /* Set the MPI hint. */
    if (ios->ioproc)
        if ((mpierr = MPI_Info_set(ios->info, hint, hintval)))
        {
            LOG((1, "ERROR: Setting PIO hints failed. Settnig MPI hints using info object failed (mpierr = %d)", mpierr));
            return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
        }

    return PIO_NOERR;
}

/**
 * Clean up internal data structures, free MPI resources, and exit the
 * pio library.
 *
 * @param iosysid: the io system ID provided by PIOc_Init_Intracomm().
 * @returns 0 for success or non-zero for error.
 * @ingroup PIO_finalize
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_finalize(int iosysid)
{
    iosystem_desc_t *ios;
    int niosysid;          /* The number of currently open IO systems. */
    int ierr = PIO_NOERR;
#ifdef TIMING_INTERNAL
    char gptl_iolog_fname[PIO_MAX_NAME];
    char gptl_log_fname[PIO_MAX_NAME];
#endif

#ifdef TIMING
    GPTLstart("PIO:PIOc_finalize");
#endif
    LOG((1, "PIOc_finalize iosysid = %d MPI_COMM_NULL = %d", iosysid,
         MPI_COMM_NULL));

    /* Find the IO system information. */
    if (!(ios = pio_get_iosystem_from_id(iosysid)))
    {
        return pio_err(NULL, NULL, PIO_EBADID, __FILE__, __LINE__,
                        "PIO Finalize failed. Invalid iosystem id (%d) provided", iosysid);
    }

    /* If asynch IO is in use, send the PIO_MSG_FINALIZE message from the
     * comp master to the IO processes. This may be called by
     * componets for other components iosysid. So don't send unless
     * there is a valid union_comm. */
    if (ios->async && ios->union_comm != MPI_COMM_NULL)
    {
        int msg = PIO_MSG_FINALIZE;

        LOG((3, "found iosystem info comproot = %d union_comm = %d comp_idx = %d",
             ios->comproot, ios->union_comm, ios->comp_idx));

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, iosysid);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "PIO Finalize failed on iosytem (%d). Error sending async msg for PIO_MSG_FINALIZE", iosysid);
        }
    }

    /* Free this memory that was allocated in init_intracomm. */
    if (ios->ioranks)
        free(ios->ioranks);
    LOG((3, "Freed ioranks."));
    if (ios->compranks)
        free(ios->compranks);
    LOG((3, "Freed compranks."));

    /* Learn the number of open IO systems. */
    if ((ierr = pio_num_iosystem(&niosysid)))
    {
        return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                        "PIO Finalize failed on iosystem (%d). Unable to get the number of open I/O systems", iosysid);
    }

    LOG((2, "%d iosystems are still open.", niosysid));

    /* Free the MPI groups. */
    if (ios->compgroup != MPI_GROUP_NULL)
        MPI_Group_free(&ios->compgroup);

    if (ios->iogroup != MPI_GROUP_NULL)
        MPI_Group_free(&(ios->iogroup));

    /* Free the MPI communicators. my_comm is just a copy (but not an
     * MPI copy), so does not have to have an MPI_Comm_free()
     * call. comp_comm and io_comm are MPI duplicates of the comms
     * handed into init_intercomm. So they need to be freed by MPI. */
    if (ios->intercomm != MPI_COMM_NULL)
        MPI_Comm_free(&ios->intercomm);
    if (ios->comp_comm != MPI_COMM_NULL)
        MPI_Comm_free(&ios->comp_comm);
    if (ios->my_comm != MPI_COMM_NULL)
        ios->my_comm = MPI_COMM_NULL;

    /* Free the MPI Info object. */
    if (ios->info != MPI_INFO_NULL)
        MPI_Info_free(&ios->info);

#ifdef PIO_MICRO_TIMING
    ierr = mtimer_finalize();
    if(ierr != PIO_NOERR)
    {
        /* log and continue */
        LOG((1, "Finalizing micro timers failed"));
    }
#endif

#ifdef _ADIOS2
    if (ios->adiosH != NULL)
    {
        adios2_error adiosErr = adios2_finalize(ios->adiosH);
        if (adiosErr != adios2_error_none)
        {
            return pio_err(ios, NULL, PIO_EADIOS2ERR, __FILE__, __LINE__, "Finalizing ADIOS failed (adios2_error=%s) on iosystem (%d)", adios2_error_to_string(adiosErr), iosysid);
        }

        ios->adiosH = NULL;
    }
#endif

    LOG((1, "about to finalize logging"));
    pio_finalize_logging();

    LOG((2, "PIOc_finalize completed successfully"));
#ifdef TIMING
    GPTLstop("PIO:PIOc_finalize");
#ifdef TIMING_INTERNAL
    if(ios->io_comm != MPI_COMM_NULL)
    {
        snprintf(gptl_iolog_fname, PIO_MAX_NAME, "piorwgptlioinfo%010dwrank.dat", ios->ioroot);
        GPTLpr_summary_file(ios->io_comm, gptl_iolog_fname);
        LOG((2, "Finished writing gptl io proc summary"));
    }
    snprintf(gptl_log_fname, PIO_MAX_NAME, "piorwgptlinfo%010dwrank.dat", ios->ioroot);
    if(ios->io_rank == 0)
    {
        GPTLpr_file(gptl_log_fname);
        LOG((2, "Finished writing gptl summary"));
    }
    pio_finalize_gptl();
#endif
#endif
    if (ios->union_comm != MPI_COMM_NULL)
        MPI_Comm_free(&ios->union_comm);
    if (ios->io_comm != MPI_COMM_NULL)
        MPI_Comm_free(&ios->io_comm);

    /* Delete the iosystem_desc_t data associated with this id. */
    LOG((2, "About to delete iosysid %d.", iosysid));
    if ((ierr = pio_delete_iosystem_from_list(iosysid)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "PIO Finalize failed on iosytem (%d). Unable to delete iosystem from internal list", iosysid);
    }

    return PIO_NOERR;
}

/**
 * Return a logical indicating whether this task is an IO task.
 *
 * @param iosysid the io system ID
 * @param ioproc a pointer that gets 1 if task is an IO task, 0
 * otherwise. Ignored if NULL.
 * @returns 0 for success, or PIO_BADID if iosysid can't be found.
 * @author Jim Edwards
 */
int PIOc_iam_iotask(int iosysid, bool *ioproc)
{
    iosystem_desc_t *ios;

    if (!(ios = pio_get_iosystem_from_id(iosysid)))
    {
        return pio_err(NULL, NULL, PIO_EBADID, __FILE__, __LINE__,
                        "Checking whether the local task is an IO task failed. Invalid iosytem id (%d) provided", iosysid);
    }

    if (ioproc)
        *ioproc = ios->ioproc;

    return PIO_NOERR;
}

/**
 * Return the rank of this task in the IO communicator or -1 if this
 * task is not in the communicator.
 *
 * @param iosysid the io system ID
 * @param iorank a pointer that gets the io rank, or -1 if task is not
 * in the IO communicator. Ignored if NULL.
 * @returns 0 for success, or PIO_BADID if iosysid can't be found.
 * @author Jim Edwards
 */
int PIOc_iotask_rank(int iosysid, int *iorank)
{
    iosystem_desc_t *ios;

    if (!(ios = pio_get_iosystem_from_id(iosysid)))
    {
        return pio_err(NULL, NULL, PIO_EBADID, __FILE__, __LINE__,
                        "Getting rank of IO task failed. Invalid iosystem id (%d) provided", iosysid);
    }

    if (iorank)
        *iorank = ios->io_rank;

    return PIO_NOERR;
}

/**
 * Return true if this iotype is supported in the build, 0 otherwise.
 *
 * @param iotype the io type to check
 * @returns 1 if iotype is in build, 0 if not.
 * @author Jim Edwards
 */
int PIOc_iotype_available(int iotype)
{
    switch(iotype)
    {
#ifdef _NETCDF4
    case PIO_IOTYPE_NETCDF4P:
    case PIO_IOTYPE_NETCDF4C:
        return 1;
#endif
#ifdef _NETCDF
    case PIO_IOTYPE_NETCDF:
        return 1;
#endif
#ifdef _PNETCDF
    case PIO_IOTYPE_PNETCDF:
        return 1;
#endif
    default:
        return 0;
    }
}

/**
 * Library initialization used when IO tasks are distinct from compute
 * tasks.
 *
 * This is a collective call.  Input parameters are read on
 * comp_rank=0 values on other tasks are ignored.  This variation of
 * PIO_init sets up a distinct set of tasks to handle IO, these tasks
 * do not return from this call.  Instead they go to an internal loop
 * and wait to receive further instructions from the computational
 * tasks.
 *
 * Sequence of Events to do Asynch I/O
 * -----------------------------------
 *
 * Here is the sequence of events that needs to occur when an IO
 * operation is called from the collection of compute tasks.  I'm
 * going to use pio_put_var because write_darray has some special
 * characteristics that make it a bit more complicated...
 *
 * Compute tasks call pio_put_var with an integer argument
 *
 * The MPI_Send sends a message from comp_rank=0 to io_rank=0 on
 * union_comm (a comm defined as the union of io and compute tasks)
 * msg is an integer which indicates the function being called, in
 * this case the msg is PIO_MSG_PUT_VAR_INT
 *
 * The iotasks now know what additional arguments they should expect
 * to receive from the compute tasks, in this case a file handle, a
 * variable id, the length of the array and the array itself.
 *
 * The iotasks now have the information they need to complete the
 * operation and they call the pio_put_var routine.  (In pio1 this bit
 * of code is in pio_get_put_callbacks.F90.in)
 *
 * After the netcdf operation is completed (in the case of an inq or
 * get operation) the result is communicated back to the compute
 * tasks.
 *
 * @param world the communicator containing all the available tasks.
 *
 * @param num_io_procs the number of processes for the IO component.
 *
 * @param io_proc_list an array of lenth num_io_procs with the
 * processor number for each IO processor. If NULL then the IO
 * processes are assigned starting at processes 0.
 *
 * @param component_count number of computational components
 *
 * @param num_procs_per_comp an array of int, of length
 * component_count, with the number of processors in each computation
 * component.
 *
 * @param proc_list an array of arrays containing the processor
 * numbers for each computation component. If NULL then the
 * computation components are assigned processors sequentially
 * starting with processor num_io_procs.
 *
 * @param user_io_comm pointer to an MPI_Comm. If not NULL, it will
 * get an MPI duplicate of the IO communicator. (It is a full
 * duplicate and later must be freed with MPI_Free() by the caller.)
 *
 * @param user_comp_comm pointer to an array of pointers to MPI_Comm;
 * the array is of length component_count. If not NULL, it will get an
 * MPI duplicate of each computation communicator. (These are full
 * duplicates and each must later be freed with MPI_Free() by the
 * caller.)
 *
 * @param rearranger the default rearranger to use for decompositions
 * in this IO system. Must be either PIO_REARR_BOX or
 * PIO_REARR_SUBSET.
 *
 * @param iosysidp pointer to array of length component_count that
 * gets the iosysid for each component.
 *
 * @return PIO_NOERR on success, error code otherwise.
 * @ingroup PIO_init
 * @author Ed Hartnett
 */
int PIOc_init_async(MPI_Comm world, int num_io_procs, int *io_proc_list,
                    int component_count, int *num_procs_per_comp, int **proc_list,
                    MPI_Comm *user_io_comm, MPI_Comm *user_comp_comm, int rearranger,
                    int *iosysidp)
{
    int my_rank;          /* Rank of this task. */
    int **my_proc_list;   /* Array of arrays of procs for comp components. */
    int *my_io_proc_list; /* List of processors in IO component. */
    int mpierr;           /* Return code from MPI functions. */
    int ret;              /* Return code. */

/* TIMING_INTERNAL implies that the timing statistics are gathered/
 * displayed by pio
 */
#ifdef TIMING
#ifdef TIMING_INTERNAL
    pio_init_gptl();
#endif
    GPTLstart("PIO:PIOc_init_async");
#endif
    /* Check input parameters. */
    if (num_io_procs < 1 || component_count < 1 || !num_procs_per_comp || !iosysidp ||
        (rearranger != PIO_REARR_BOX && rearranger != PIO_REARR_SUBSET))
    {
        return pio_err(NULL, NULL, PIO_EINVAL, __FILE__, __LINE__,
                        "PIO Init (async) failed. Invalid arguments provided, num_io_procs=%d (expected >= 1), component_count=%d (expected >= 1), num_procs_per_comp is %s (expected not NULL), iosysidp is %s (expected not NULL), rearranger=%s (expected PIO_REARR_BOX or PIO_REARR_SUBSET)", num_io_procs, component_count, (num_procs_per_comp) ? "not NULL" : "NULL", (iosysidp) ? "not NULL" : "NULL", (rearranger == PIO_REARR_BOX) ? "PIO_REARR_BOX" : ((rearranger == PIO_REARR_SUBSET) ? "PIO_REARR_SUBSET" : "UNKNOWN REARRANGER"));
    }

    /* Temporarily limit to one computational component. */
    if (component_count > 1)
    {
        return pio_err(NULL, NULL, PIO_EINVAL, __FILE__, __LINE__,
                        "PIO Init (async) failed. Currently only one computational component is supported, and %d computation components were specified", component_count);
    }

    /* Turn on the logging system for PIO. */
    pio_init_logging();
    LOG((1, "PIOc_Init_Async num_io_procs = %d component_count = %d", num_io_procs,
         component_count));

#ifdef PIO_MICRO_TIMING
    /* Initialize the timer framework - MPI_Wtime() + output from root proc */
    ret = mtimer_init(PIO_MICRO_MPI_WTIME_ROOT);
    if(ret != PIO_NOERR)
    {
        return pio_err(NULL, NULL, PIO_EINTERNAL, __FILE__, __LINE__,
                        "PIO Init (async) failed. Initializing micro timers failed");
    }
#endif
    /* If the user did not supply a list of process numbers to use for
     * IO, create it. */
    if (!io_proc_list)
    {
        LOG((3, "calculating processors for IO component"));
        if (!(my_io_proc_list = malloc(num_io_procs * sizeof(int))))
        {
            return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                            "PIO Init (async) failed. Out of memory.");
        }
        for (int p = 0; p < num_io_procs; p++)
        {
            my_io_proc_list[p] = p;
            LOG((3, "my_io_proc_list[%d] = %d", p, my_io_proc_list[p]));
        }
    }
    else
        my_io_proc_list = io_proc_list;

    /* If the user did not provide a list of processes for each
     * component, create one. */
    if (!proc_list)
    {
        int last_proc = num_io_procs;

        /* Allocate space for array of arrays. */
        if (!(my_proc_list = malloc((component_count) * sizeof(int *))))
        {
            return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                            "PIO Init (async) failed. Out of memory");
        }

        /* Fill the array of arrays. */
        for (int cmp = 0; cmp < component_count; cmp++)
        {
            LOG((3, "calculating processors for component %d num_procs_per_comp[cmp] = %d", cmp, num_procs_per_comp[cmp]));

            /* Allocate space for each array. */
            if (!(my_proc_list[cmp] = malloc(num_procs_per_comp[cmp] * sizeof(int))))
            {
                return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                                "PIO Init (async) failed. Out of memory");
            }

            int proc;
            for (proc = last_proc; proc < num_procs_per_comp[cmp] + last_proc; proc++)
            {
                my_proc_list[cmp][proc - last_proc] = proc;
                LOG((3, "my_proc_list[%d][%d] = %d", cmp, proc - last_proc, proc));
            }
            last_proc = proc;
        }
    }
    else
        my_proc_list = proc_list;

    /* Get rank of this task in world. */
    if ((ret = MPI_Comm_rank(world, &my_rank)))
        return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);

    /* Is this process in the IO component? */
    int pidx;
    for (pidx = 0; pidx < num_io_procs; pidx++)
        if (my_rank == my_io_proc_list[pidx])
            break;
    int in_io = (pidx == num_io_procs) ? 0 : 1;
    LOG((3, "in_io = %d", in_io));

    /* Allocate struct to hold io system info for each computation component. */
    iosystem_desc_t *iosys[component_count], *my_iosys;
    for (int cmp1 = 0; cmp1 < component_count; cmp1++)
        if (!(iosys[cmp1] = (iosystem_desc_t *)calloc(1, sizeof(iosystem_desc_t))))
        {
            return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                            "PIO Init (async) failed. Out of memory");
        }

    /* Create group for world. */
    MPI_Group world_group;
    if ((ret = MPI_Comm_group(world, &world_group)))
    {
        LOG((1, "ERROR: PIO Init (async failed). Getting MPI group associated with world failed"));
        return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
    }
    LOG((3, "world group created\n"));

    /* We will create a group for the IO component. */
    MPI_Group io_group;

    /* The shared IO communicator. */
    MPI_Comm io_comm;

    /* Rank of current process in IO communicator. */
    int io_rank = -1;

    /* Set to MPI_ROOT on master process, MPI_PROC_NULL on other
     * processes. */
    int iomaster;

    /* Create a group for the IO component. */
    if ((ret = MPI_Group_incl(world_group, num_io_procs, my_io_proc_list, &io_group)))
    {
        LOG((1, "ERROR: PIO Init (async) failed. Creating MPI group for IO component failed"));
        return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
    }
    LOG((3, "created IO group - io_group = %d MPI_GROUP_EMPTY = %d", io_group, MPI_GROUP_EMPTY));

    /* There is one shared IO comm. Create it. */
    if ((ret = MPI_Comm_create(world, io_group, &io_comm)))
    {
        LOG((1, "ERROR: PIO Init (async) failed. Creating shared MPI Comm for IO component failed"));
        return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
    }
    LOG((3, "created io comm io_comm = %d", io_comm));

    /* Does the user want a copy of the IO communicator? */
    if (user_io_comm)
    {
        *user_io_comm = MPI_COMM_NULL;
        if (in_io)
            if ((mpierr = MPI_Comm_dup(io_comm, user_io_comm)))
                return check_mpi(NULL, NULL, mpierr, __FILE__, __LINE__);
    }

    /* For processes in the IO component, get their rank within the IO
     * communicator. */
    if (in_io)
    {
        LOG((3, "about to get io rank"));
        if ((ret = MPI_Comm_rank(io_comm, &io_rank)))
            return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
        iomaster = !io_rank ? MPI_ROOT : MPI_PROC_NULL;
        LOG((3, "intracomm created for io_comm = %d io_rank = %d IO %s",
             io_comm, io_rank, iomaster == MPI_ROOT ? "MASTER" : "SERVANT"));
    }

    /* We will create a group for each computational component. */
    MPI_Group group[component_count];

    /* We will also create a group for each component and the IO
     * component processes (i.e. a union of computation and IO
     * processes. */
    MPI_Group union_group[component_count];

    /* For each computation component. */
    for (int cmp = 0; cmp < component_count; cmp++)
    {
        LOG((3, "processing component %d", cmp));

        /* Get pointer to current iosys. */
        my_iosys = iosys[cmp];

        /* Initialize some values. */
        my_iosys->io_comm = MPI_COMM_NULL;
        my_iosys->comp_comm = MPI_COMM_NULL;
        my_iosys->union_comm = MPI_COMM_NULL;
        my_iosys->intercomm = MPI_COMM_NULL;
        my_iosys->my_comm = MPI_COMM_NULL;
        my_iosys->async = 1;
        my_iosys->error_handler = default_error_handler;
        my_iosys->num_comptasks = num_procs_per_comp[cmp];
        my_iosys->num_iotasks = num_io_procs;
        my_iosys->num_uniontasks = my_iosys->num_comptasks + my_iosys->num_iotasks;
        my_iosys->compgroup = MPI_GROUP_NULL;
        my_iosys->iogroup = MPI_GROUP_NULL;
        my_iosys->default_rearranger = rearranger;

        /* Initialize the rearranger options. */
        init_rearr_opts(my_iosys);
        
        /* The rank of the computation leader in the union comm. */
        my_iosys->comproot = num_io_procs;
        LOG((3, "my_iosys->comproot = %d", my_iosys->comproot));

        /* We are not providing an info object. */
        my_iosys->info = MPI_INFO_NULL;

        /* Create a group for this component. */
        if ((ret = MPI_Group_incl(world_group, num_procs_per_comp[cmp], my_proc_list[cmp],
                                  &group[cmp])))
            return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
        LOG((3, "created component MPI group - group[%d] = %d", cmp, group[cmp]));

        /* For all the computation components create a union group
         * with their processors and the processors of the (shared) IO
         * component. */

        /* How many processors in the union comm? */
        int nprocs_union = num_io_procs + num_procs_per_comp[cmp];

        /* This will hold proc numbers from both computation and IO
         * components. */
        int proc_list_union[nprocs_union];

        /* Add proc numbers from IO. */
        for (int p = 0; p < num_io_procs; p++)
            proc_list_union[p] = my_io_proc_list[p];

        /* Add proc numbers from computation component. */
        for (int p = 0; p < num_procs_per_comp[cmp]; p++)
            proc_list_union[p + num_io_procs] = my_proc_list[cmp][p];

        /* Allocate space for computation task ranks. */
        if (!(my_iosys->compranks = calloc(my_iosys->num_comptasks, sizeof(int))))
        {
            return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                            "PIO Init (async) failed. Out of memory");
        }
        
        /* Remember computation task ranks. */
        for (int p = 0; p < num_procs_per_comp[cmp]; p++)
            my_iosys->compranks[p] = my_proc_list[cmp][p];

        /* Create the union group. */
        if ((ret = MPI_Group_incl(world_group, nprocs_union, proc_list_union, &union_group[cmp])))
        {
            LOG((1, "ERROR: PIO Init (async) failed. Creating union group failed"));
            return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
        }
        LOG((3, "created union MPI_group - union_group[%d] = %d with %d procs", cmp,
             union_group[cmp], nprocs_union));

        /* Remember whether this process is in the IO component. */
        my_iosys->ioproc = in_io;

        /* With async, tasks are either in a computation component or
         * the IO component. */
        my_iosys->compproc = !in_io;

        /* Is this process in this computation component? */
        int in_cmp = 0;
        for (pidx = 0; pidx < num_procs_per_comp[cmp]; pidx++)
            if (my_rank == my_proc_list[cmp][pidx])
                break;
        in_cmp = (pidx == num_procs_per_comp[cmp]) ? 0 : 1;
        LOG((3, "pidx = %d num_procs_per_comp[%d] = %d in_cmp = %d",
             pidx, cmp, num_procs_per_comp[cmp], in_cmp));

        /* Create an intracomm for this component. Only processes in
         * the component need to participate in the intracomm create
         * call. */
        LOG((3, "creating intracomm cmp = %d from group[%d] = %d", cmp, cmp, group[cmp]));
        if ((ret = MPI_Comm_create(world, group[cmp], &my_iosys->comp_comm)))
            return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);

        if (in_cmp)
        {
            /* Does the user want a copy? */
            if (user_comp_comm)
                if ((mpierr = MPI_Comm_dup(my_iosys->comp_comm, &user_comp_comm[cmp])))
                    return check_mpi(NULL, NULL, mpierr, __FILE__, __LINE__);

            /* Get the rank in this comp comm. */
            if ((ret = MPI_Comm_rank(my_iosys->comp_comm, &my_iosys->comp_rank)))
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);

            /* Set comp_rank 0 to be the compmaster. It will have a
             * setting of MPI_ROOT, all other tasks will have a
             * setting of MPI_PROC_NULL. */
            my_iosys->compmaster = my_iosys->comp_rank ? MPI_PROC_NULL : MPI_ROOT;

            LOG((3, "intracomm created for cmp = %d comp_comm = %d comp_rank = %d comp %s",
                 cmp, my_iosys->comp_comm, my_iosys->comp_rank,
                 my_iosys->compmaster == MPI_ROOT ? "MASTER" : "SERVANT"));
        }

        /* If this is the IO component, make a copy of the IO comm for
         * each computational component. */
        if (in_io)
        {
            LOG((3, "making a dup of io_comm = %d io_rank = %d", io_comm, io_rank));
            if ((ret = MPI_Comm_dup(io_comm, &my_iosys->io_comm)))
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            LOG((3, "dup of io_comm = %d io_rank = %d", my_iosys->io_comm, io_rank));
            my_iosys->iomaster = iomaster;
            my_iosys->io_rank = io_rank;
            my_iosys->ioroot = 0;
            my_iosys->comp_idx = cmp;
        }

        /* Create an array that holds the ranks of the tasks to be used
         * for IO. */
        if (!(my_iosys->ioranks = calloc(my_iosys->num_iotasks, sizeof(int))))
        {
            return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                            "PIO Init (async) failed. Out of memory");
        }
        for (int i = 0; i < my_iosys->num_iotasks; i++)
            my_iosys->ioranks[i] = my_io_proc_list[i];
        my_iosys->ioroot = my_iosys->ioranks[0];

        /* All the processes in this component, and the IO component,
         * are part of the union_comm. */
        if (in_io || in_cmp)
        {
            LOG((3, "my_iosys->io_comm = %d group = %d", my_iosys->io_comm, union_group[cmp]));
            /* Create a group for the union of the IO component
             * and one of the computation components. */
            if ((ret = MPI_Comm_create(world, union_group[cmp], &my_iosys->union_comm)))
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);

            if ((ret = MPI_Comm_rank(my_iosys->union_comm, &my_iosys->union_rank)))
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);

            /* Set my_comm to union_comm for async. */
            my_iosys->my_comm = my_iosys->union_comm;
            LOG((3, "intracomm created for union cmp = %d union_rank = %d union_comm = %d",
                 cmp, my_iosys->union_rank, my_iosys->union_comm));

            if (in_io)
            {
                LOG((3, "my_iosys->io_comm = %d", my_iosys->io_comm));
                /* Create the intercomm from IO to computation component. */
                LOG((3, "about to create intercomm for IO component to cmp = %d "
                     "my_iosys->io_comm = %d", cmp, my_iosys->io_comm));
                if ((ret = MPI_Intercomm_create(my_iosys->io_comm, 0, my_iosys->union_comm,
                                                my_proc_list[cmp][0], 0, &my_iosys->intercomm)))
                    return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }
            else
            {
                /* Create the intercomm from computation component to IO component. */
                LOG((3, "about to create intercomm for cmp = %d my_iosys->comp_comm = %d", cmp,
                     my_iosys->comp_comm));
                if ((ret = MPI_Intercomm_create(my_iosys->comp_comm, 0, my_iosys->union_comm,
                                                my_io_proc_list[0], 0, &my_iosys->intercomm)))
                    return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }
            LOG((3, "intercomm created for cmp = %d", cmp));
        }

        /* Async I/O service message info */
        my_iosys->async_ios_msg_info.seq_num = PIO_MSG_START_SEQ_NUM;
        my_iosys->async_ios_msg_info.prev_msg = PIO_MSG_INVALID;

        /* Add this id to the list of PIO iosystem ids. */
        iosysidp[cmp] = pio_add_to_iosystem_list(my_iosys, MPI_COMM_NULL);
        LOG((2, "new iosys ID added to iosystem_list iosysid = %d", iosysidp[cmp]));
    } /* next computational component */

    /* Initialize async message signatures */
    ret = init_async_msgs_sign();
    if(ret != PIO_NOERR)
    {
        return pio_err(NULL, NULL, ret, __FILE__, __LINE__,
                        "PIO Init (async) failed. Initializing async message signatures failed");
    }

    /* Now call the function from which the IO tasks will not return
     * until the PIO_MSG_FINALIZE message is sent. This will handle all
     * components. */
    if (in_io)
    {
        LOG((2, "Starting message handler io_rank = %d component_count = %d",
             io_rank, component_count));
        if ((ret = pio_msg_handler2(io_rank, component_count, iosys, io_comm)))
        {
            return pio_err(NULL, NULL, ret, __FILE__, __LINE__,
                            "Error processing I/O message");
        }
        LOG((2, "Returned from pio_msg_handler2() ret = %d", ret));
    }

    /* Free resources if needed. */
    if (!io_proc_list)
        free(my_io_proc_list);

    if (in_io)
        if ((mpierr = MPI_Comm_free(&io_comm)))
            return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);

    if (!proc_list)
    {
        for (int cmp = 0; cmp < component_count; cmp++)
            free(my_proc_list[cmp]);
        free(my_proc_list);
    }

    /* Free MPI groups. */
    if ((ret = MPI_Group_free(&io_group)))
        return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);

    for (int cmp = 0; cmp < component_count; cmp++)
    {
        if ((ret = MPI_Group_free(&group[cmp])))
            return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
        if ((ret = MPI_Group_free(&union_group[cmp])))
            return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
    }

    if ((ret = MPI_Group_free(&world_group)))
        return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);

    LOG((2, "successfully done with PIO_Init_Async"));
#ifdef TIMING
    GPTLstop("PIO:PIOc_init_async");
#endif
    return PIO_NOERR;
}

/**
 * This variation of PIO_init supports I/O as an asynchronous service.
 *
 * A set of processes, I/O processes, are used to provide this
 * asynchronous service. A set (or multiple sets) of processes, compute
 * processes, that is disjoint from the set of I/O processes use this
 * service (provided by the I/O processes) by internally passing
 * messages.
 *
 * From the user/application side, all I/O processes will wait (until
 * finalize) in PIO_init(). So this function call does not return
 * until finalize for I/O processes.
 *
 * Meanwhile PIO_init() will return on all compute processes and the
 * application can perform I/O using the regular PIO interfaces.
 * The user (application) needs to provide:
 * io_comm => The communicator for all I/O procs (Only one io comm
 * is supported)
 * comp_comms => One or more communicators for the compute processes
 * (All compute processes in a computational component can be part
 * of one comp_comm). All compute processes in the comp_comms use
 * I/O processes (via the async I/O service) in io_comm for I/O.
 * peer_comm => Parent communicator to all compute and I/O comms. The
 * compute and I/O communicators are derived from this comm.
 *
 * @param component_count Number of components (determines the number
 * of comp_comms and iosysidps)
 * @param peer_comm The parent communicator used to create comp_comms
 * and io_comm, this comm is valid on all procs
 * @param ucomp_comms An array of communicators that represent sets of
 * compute processes (that use I/O processes that are part of io_comm
 * to perform I/O), comp_comms[i] is valid only on procs that are
 * part of comp_comms[i]
 * @param uio_comm The communicator representing all I/O processes.
 * This communicator is valid (!= MPI_COMM_NULL) only on the I/O
 * procs
 * @param rearranger The rearranger to use for I/O
 * @param iosysidps An array to store the iosystem ids returned (each
 * iosystem id in the array is for the corresponding comp_comm in the
 * comp_comms array)
 */

int PIOc_init_intercomm(int component_count, const MPI_Comm peer_comm,
                        const MPI_Comm *ucomp_comms, const MPI_Comm uio_comm,
                        int rearranger, int *iosysidps)
{
    int ret;
/* TIMING_INTERNAL implies that the timing statistics are gathered/
 * displayed by pio
 */
#ifdef TIMING
#ifdef TIMING_INTERNAL
    pio_init_gptl();
#endif
    GPTLstart("PIO:PIOc_init_intercomm");
#endif
    assert((component_count > 0) && ucomp_comms && iosysidps);
    if((component_count <= 0) || (ucomp_comms == NULL) ||
        ((rearranger != PIO_REARR_BOX) && (rearranger != PIO_REARR_SUBSET)) ||
        (iosysidps == NULL))
    {
        return pio_err(NULL, NULL, PIO_EINVAL, __FILE__, __LINE__,
                        "PIO Init (async) failed. Invalid arguments provided, component_count=%d (expected > 0), ucomp_comms is %s (expected not NULL), rearranger=%s (expected PIO_REARR_BOX or PIO_REARR_SUBSET), iosysidps is %s (expected not NULL)", component_count, (ucomp_comms) ? "not NULL" : "NULL", (rearranger == PIO_REARR_BOX) ? "PIO_REARR_BOX" : ((rearranger == PIO_REARR_SUBSET) ? "PIO_REARR_SUBSET" : "UNKNOWN REARRANGER"), (iosysidps) ? "not NULL" : "NULL");
    }

    /* Turn on the logging system for PIO. */
    pio_init_logging();
    LOG((1, "PIOc_init_intercomm component_count = %d", component_count));

#ifdef PIO_MICRO_TIMING
    /* Initialize the timer framework - MPI_Wtime() + output from root proc */
    ret = mtimer_init(PIO_MICRO_MPI_WTIME_ROOT);
    if(ret != PIO_NOERR)
    {
        return pio_err(NULL, NULL, PIO_EINTERNAL, __FILE__, __LINE__,
                        "PIO Init (async) failed. Initializing micro timers failed");
    }
#endif
    /* Dup the comp comms from the user, since we cache
     * these communicators inside PIO
     */
    MPI_Comm *comp_comms = calloc(component_count, sizeof(MPI_Comm));
    if(comp_comms)
    {
        for(int i=0; i<component_count; i++)
        {
            comp_comms[i] = MPI_COMM_NULL;
            if(ucomp_comms[i] != MPI_COMM_NULL)
            {
                ret = MPI_Comm_dup(ucomp_comms[i], &(comp_comms[i]));
                if(ret != MPI_SUCCESS)
                {
                    return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
                }
            }
        }
    }
    else
    {
        return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                        "PIO Init (async) failed. Out of memory allocating %lld bytes for storing MPI communicators for the different asynchronous components", (unsigned long long) (component_count * sizeof(MPI_Comm)));
    }

    /* Allocate iosystems for all comp comms
     * Each iosystem here includes comp_comms[i] and io_comm
     */
    iosystem_desc_t *iosys[component_count];
    for(int i=0; i<component_count; i++)
    {
        iosys[i] = (iosystem_desc_t *) calloc(1, sizeof(iosystem_desc_t));
        if(!iosys[i])
        {
            return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                            "PIO init (async) failed. Out of memory allocating %lld bytes for storing I/O system descriptor for component %d", (unsigned long long) sizeof(iosystem_desc_t), i);
        }

        /* Initialize the iosystem */
        iosys[i]->iosysid = -1;
        iosys[i]->union_comm = MPI_COMM_NULL;
        iosys[i]->io_comm = MPI_COMM_NULL;
        iosys[i]->comp_comm = MPI_COMM_NULL;
        iosys[i]->intercomm = MPI_COMM_NULL;
        iosys[i]->my_comm = MPI_COMM_NULL;

        iosys[i]->compgroup = MPI_GROUP_NULL;
        iosys[i]->iogroup = MPI_GROUP_NULL;

        iosys[i]->iomaster = MPI_PROC_NULL;
        iosys[i]->compmaster = MPI_PROC_NULL;

        iosys[i]->error_handler = default_error_handler;
        iosys[i]->default_rearranger = rearranger;

        iosys[i]->info = MPI_INFO_NULL;

        iosys[i]->rearr_opts.comm_type = PIO_REARR_COMM_COLL;
        iosys[i]->rearr_opts.fcd = PIO_REARR_COMM_FC_2D_DISABLE;

        iosys[i]->comp_idx = -1;
    }
    /* For each component in comp_comms create the necessary comms
     * with the io_comm, and initialize the iosystem including
     * comp_comms[i] and io_comm
     */
    for(int i=0; i<component_count; i++)
    {
        /* Ranks of io and comp leaders in io_comm and comp_comms[i]
         * respectively
         */
        const int IO_LEADER_LRANK = 0;
        const int COMP_LEADER_LRANK = 0;

        /* I/O and comp roots in the union comm (union of io_comm and
         * comp_comms[i]
         */
        const int IO_ROOT_URANK = 0;
        const int COMP_ROOT_URANK = 0;

        /* MPI tag used during intercomm merge */
        int tag_intercomm_comm = i;

        /* Ranks of io and comp leaders in peer_comm */
        int io_leader_grank = -1;
        int comp_leader_grank = -1;
        LOG((3, "Async I/O Service : processing compute component %d", i));

        iosys[i]->async = true;

        /* Dup the io comm since its cached in the iosystem */
        MPI_Comm io_comm = uio_comm;
        if(uio_comm != MPI_COMM_NULL)
        {
            ret = MPI_Comm_dup(uio_comm, &io_comm);
            if(ret != MPI_SUCCESS)
            {
                LOG((1, "PIO Init (async) failed. Duping user I/O comm failed"));
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }
        }
        /* The compute comm is comp_comms[i] and the io comm is io_comm
         * In the compute procs io_comm is NULL and on the io procs the
         * comp_comms are NULL
         * In the compute procs comp_comms[i] is only valid (!= MPI_COMM_NULL)
         * for comms that the compute proc belongs.
         * Also note that this implies that all compute procs agree on
         * the indices in comp_comms, but can have different values in
         * comp_comms depending on whether the current compute proc belongs
         * to the comp_comms[i]
         * Since peer_comm is the parent communicator for io_comm and comp_comm
         * it is used for all global communication across all comms
         * */
        iosys[i]->io_comm = io_comm;
        iosys[i]->comp_comm = comp_comms[i];
        iosys[i]->comp_idx = i;
        /* Rank of io process and comp process in peer_comm */
        int io_grank = -1, comp_grank = -1;
        if(io_comm != MPI_COMM_NULL)
        {
            /* I/O process */
            iosys[i]->ioproc = true;
            iosys[i]->comp_rank = -1;
            iosys[i]->compproc = false;
            iosys[i]->num_comptasks = 0;

            ret = MPI_Comm_rank(io_comm, &(iosys[i]->io_rank));
            if(ret != MPI_SUCCESS)
            {
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }

            ret = MPI_Comm_size(io_comm, &(iosys[i]->num_iotasks));
            if(ret != MPI_SUCCESS)
            {
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }

            io_grank = -1;
            comp_grank = -1;
            io_leader_grank = -1;
            comp_leader_grank = -1;
            ret = MPI_Comm_rank(peer_comm, &io_grank);
            if(ret != MPI_SUCCESS)
            {
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }
            /* Find the io leader for intercomm */
            if(iosys[i]->io_rank == IO_LEADER_LRANK)
            {
                io_leader_grank = io_grank;
                iosys[i]->iomaster = MPI_ROOT;
            }
            else
            {
                iosys[i]->iomaster = MPI_PROC_NULL;
            }
            iosys[i]->compmaster = COMP_LEADER_LRANK;

            int tmp_io_leader_grank = io_leader_grank;
            ret = MPI_Allreduce(&tmp_io_leader_grank, &io_leader_grank, 1, MPI_INT, MPI_MAX, peer_comm);
            if(ret != MPI_SUCCESS)
            {
                LOG((1, "PIO Init (async) failed. Finding I/O leader failed"));
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }

            /* Find the comp leader for intercomm */
            int tmp_comp_leader_grank = comp_leader_grank;
            ret = MPI_Allreduce(&tmp_comp_leader_grank, &comp_leader_grank, 1, MPI_INT, MPI_MAX, peer_comm);
            if(ret != MPI_SUCCESS)
            {
                LOG((1, "PIO Init (async) failed. Finding Comp leader failed"));
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }

            /* Create the intercomm between io_comm and comp_comms[i] */
            ret = MPI_Intercomm_create(io_comm, IO_LEADER_LRANK, peer_comm, comp_leader_grank, tag_intercomm_comm, &(iosys[i]->intercomm));
            if(ret != MPI_SUCCESS)
            {
                LOG((1, "PIO Init (async) failed. Creating an intercomm between I/O comm and Comp comms failed"));
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }

            /* Create the union comm between io_comm and comp_comms[i] */
            /* Make sure that the io procs are in the "high group" in the union comm,
             * This ensures that io procs are placed after compute procs in the
             * union comm
             */
            int is_high_group = 1;
            ret = MPI_Intercomm_merge(iosys[i]->intercomm, is_high_group, &(iosys[i]->union_comm));
            if(ret != MPI_SUCCESS)
            {
                LOG((1, "PIO Init (async) failed. Merging intercomm between I/O comm and Comp comms failed"));
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }

            ret = MPI_Comm_size(iosys[i]->union_comm, &(iosys[i]->num_uniontasks));
            if(ret != MPI_SUCCESS)
            {
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }

            assert(iosys[i]->num_uniontasks > 0);
            ret = MPI_Comm_rank(iosys[i]->union_comm, &(iosys[i]->union_rank));
            if(ret != MPI_SUCCESS)
            {
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }

            iosys[i]->num_comptasks = iosys[i]->num_uniontasks - iosys[i]->num_iotasks;
            /* Since in the intercomm io procs are always in the "high group" the
             * ranks for the I/O processes start at iosys[i]->num_comptasks
             * The compute procs start at rank == 0 ("low group")
             */
            iosys[i]->comproot = COMP_ROOT_URANK;
            iosys[i]->ioroot = iosys[i]->num_comptasks + IO_ROOT_URANK;

            iosys[i]->ioranks = malloc(iosys[i]->num_iotasks * sizeof(int));
            if(!(iosys[i]->ioranks))
            {
                return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                                "PIO Init (async) failed. Out of memory allocating %lld bytes to store ranks of I/O processes for component %d", (unsigned long long) (iosys[i]->num_iotasks * sizeof(int)), i);
            }
            for(int j=0; j<iosys[i]->num_iotasks; j++)
            {
                iosys[i]->ioranks[j] = iosys[i]->num_comptasks + j;
            }

            iosys[i]->compranks = malloc(iosys[i]->num_comptasks * sizeof(int));
            if(!(iosys[i]->compranks))
            {
                return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                                "PIO Init (async) failed. Out of memory allocating %lld bytes to store ranks of compute processes for component %d", (unsigned long long) (iosys[i]->num_comptasks * sizeof(int)), i);
            }
            for(int j=0; j<iosys[i]->num_comptasks; j++)
            {
                iosys[i]->compranks[j] = j;
            }

            MPI_Group union_comm_group;
            ret = MPI_Comm_group(iosys[i]->union_comm, &union_comm_group);
            if(ret != MPI_SUCCESS)
            {
                LOG((1, "PIO Init (async) failed. Unable to get process group for the union comm"));
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }

            ret = MPI_Group_incl(union_comm_group, iosys[i]->num_comptasks, iosys[i]->compranks, &(iosys[i]->compgroup));
            if(ret != MPI_SUCCESS)
            {
                LOG((1, "PIO Init (async) failed. Unable to find procs in comp group"));
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }

            ret = MPI_Group_incl(union_comm_group, iosys[i]->num_iotasks, iosys[i]->ioranks, &(iosys[i]->iogroup));
            if(ret != MPI_SUCCESS)
            {
                LOG((1, "PIO Init (async) failed. Unable to find procs in I/O group"));
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }

            MPI_Group_free(&union_comm_group);
        }
        else
        {
            /* Compute process - belonging to any of comp_comms[i] */
            iosys[i]->comp_rank = -1;
            iosys[i]->io_rank = -1;
            iosys[i]->num_comptasks = 0;
            iosys[i]->ioproc = false;
            iosys[i]->compproc = false;

            io_grank = -1;
            comp_grank = -1;
            io_leader_grank = -1;
            comp_leader_grank = -1;

            if(comp_comms[i] != MPI_COMM_NULL)
            {
                iosys[i]->compproc = true;

                ret = MPI_Comm_rank(comp_comms[i], &(iosys[i]->comp_rank));
                if(ret != MPI_SUCCESS)
                {
                    return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
                }
                ret = MPI_Comm_size(comp_comms[i], &(iosys[i]->num_comptasks));
                if(ret != MPI_SUCCESS)
                {
                    return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
                }

                ret = MPI_Comm_rank(peer_comm, &comp_grank);
                if(ret != MPI_SUCCESS)
                {
                    return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
                }

                if(iosys[i]->comp_rank == COMP_LEADER_LRANK)
                {
                    comp_leader_grank = comp_grank;
                    iosys[i]->compmaster = MPI_ROOT;
                }
                else
                {
                    iosys[i]->compmaster = MPI_PROC_NULL;
                }
                iosys[i]->iomaster = IO_LEADER_LRANK;
            }
            /* Find the io leader for intercomm */
            int tmp_io_leader_grank = io_leader_grank;
            ret = MPI_Allreduce(&tmp_io_leader_grank, &io_leader_grank, 1, MPI_INT, MPI_MAX, peer_comm);
            if(ret != MPI_SUCCESS)
            {
                LOG((1, "PIO Init (async) failed. Finding I/O leader failed"));
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }

            /* Find the comp leader for intercomm */
            int tmp_comp_leader_grank = comp_leader_grank;
            ret = MPI_Allreduce(&tmp_comp_leader_grank, &comp_leader_grank, 1, MPI_INT, MPI_MAX, peer_comm);
            if(ret != MPI_SUCCESS)
            {
                LOG((1, "PIO Init (async) failed. Finding Comp leader failed"));
                return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
            }

            if(comp_comms[i] != MPI_COMM_NULL)
            {
                /* Create the intercomm between io_comm and comp_comms[i] */
                ret = MPI_Intercomm_create(comp_comms[i], COMP_LEADER_LRANK, peer_comm, io_leader_grank, tag_intercomm_comm, &(iosys[i]->intercomm));
                if(ret != MPI_SUCCESS)
                {
                    LOG((1, "PIO Init (async) failed. Creating intercomm between I/O comm and Comp comms failed"));
                    return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
                }

                /* Create the union comm between io_comm and comp_comms[i] */
                /* Make sure that the comp procs are in the "low group" in the union comm,
                 * This ensures that comp procs are placed before io procs in the
                 * union comm
                 */
                int is_high_group = 0;
                ret = MPI_Intercomm_merge(iosys[i]->intercomm, is_high_group, &(iosys[i]->union_comm));
                if(ret != MPI_SUCCESS)
                {
                    LOG((1, "PIO Init (async) failed. Merging intercomm between I/O comm and Comp comms failed"));
                    return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
                }

                ret = MPI_Comm_size(iosys[i]->union_comm, &(iosys[i]->num_uniontasks));
                if(ret != MPI_SUCCESS)
                {
                    return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
                }

                ret = MPI_Comm_rank(iosys[i]->union_comm, &(iosys[i]->union_rank));
                if(ret != MPI_SUCCESS)
                {
                    return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
                }

                iosys[i]->num_iotasks = iosys[i]->num_uniontasks - iosys[i]->num_comptasks;
                /* Since in the intercomm io procs are always in the "high group" the
                 * ranks for the I/O processes start at iosys[i]->num_comptasks
                 * The compute procs start at rank == 0 ("low group")
                 */
                iosys[i]->comproot = COMP_ROOT_URANK;
                iosys[i]->ioroot = iosys[i]->num_comptasks + IO_ROOT_URANK;

                iosys[i]->ioranks = malloc(iosys[i]->num_iotasks * sizeof(int));
                if(!(iosys[i]->ioranks))
                {
                    return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                                    "PIO Init (async) failed. Out of memory allocating %lld bytes to store ranks of I/O processes for component %d", (unsigned long long) (iosys[i]->num_iotasks * sizeof(int)), i);
                }
                for(int j=0; j<iosys[i]->num_iotasks; j++)
                {
                    iosys[i]->ioranks[j] = iosys[i]->num_comptasks + j;
                }

                iosys[i]->compranks = malloc(iosys[i]->num_comptasks * sizeof(int));
                if(!(iosys[i]->compranks))
                {
                    return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                                    "PIO Init (async) failed. Out of memory allocating %lld bytes to store ranks of compute processes for component %d", (unsigned long long) (iosys[i]->num_comptasks * sizeof(int)), i);
                }
                for(int j=0; j<iosys[i]->num_comptasks; j++)
                {
                    iosys[i]->compranks[j] = j;
                }

                MPI_Group union_comm_group;
                ret = MPI_Comm_group(iosys[i]->union_comm, &union_comm_group);
                if(ret != MPI_SUCCESS)
                {
                    LOG((1, "PIO Init (async) failed. Finding MPI process group for union comm failed"));
                    return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
                }

                ret = MPI_Group_incl(union_comm_group, iosys[i]->num_comptasks, iosys[i]->compranks, &(iosys[i]->compgroup));
                if(ret != MPI_SUCCESS)
                {
                    LOG((1, "PIO Init (async) failed. Finding MPI processes in comp group failed"));
                    return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
                }

                ret = MPI_Group_incl(union_comm_group, iosys[i]->num_iotasks, iosys[i]->ioranks, &(iosys[i]->iogroup));
                if(ret != MPI_SUCCESS)
                {
                    LOG((1, "PIO Init (async) failed. Finding MPI processes in io group failed"));
                    return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
                }

                MPI_Group_free(&union_comm_group);
            }
        }

        iosys[i]->my_comm = iosys[i]->union_comm;
        /* Async I/O service message info */
        iosys[i]->async_ios_msg_info.seq_num = PIO_MSG_START_SEQ_NUM;
        iosys[i]->async_ios_msg_info.prev_msg = PIO_MSG_INVALID;

        /* Add this id to the list of PIO iosystem ids. */
        iosysidps[i] = pio_add_to_iosystem_list(iosys[i], peer_comm);
        LOG((2, "PIOc_init_intercomm : iosys[%d]->ioid=%d, iosys[%d]->uniontasks = %d, iosys[%d]->union_rank=%d, %s", i, iosys[i]->iosysid, i, iosys[i]->num_uniontasks, i, iosys[i]->union_rank, ((iosys[i]->ioproc) ? ("IS IO PROC"):((iosys[i]->compproc) ? ("IS COMPUTE PROC") : ("NEITHER IO NOR COMPUTE PROC"))) ));
        LOG((2, "New IOsystem added to iosystem_list iosysid = %d", iosysidps[i]));
    }

    /* The comp_comms array is freed. The communicators will be freed internally
     * in PIO - during finalize
     */
    free(comp_comms);
    
    /* Initialize async message signatures */
    ret = init_async_msgs_sign();
    if(ret != PIO_NOERR)
    {
        return pio_err(NULL, NULL, ret, __FILE__, __LINE__,
                        "PIO Init (async) failed. Initializing asynchronous message signatures failed");
    }

    /* Invoke the message handler for I/O procs. The message handler goes
     * into a continuous loop to handle messages from compute procs and
     * only returns after compute procs call PIOc_finalize() for the
     * iosystem
     */
    if(uio_comm != MPI_COMM_NULL)
    {
        /* I/O Process. Create the global communicator required for
         * asynchronous messaging and start handling messages in
         * the message handler. Note that the message handler only
         * returns when compute procs call PIOc_finalize()
         */
        int rank;
        MPI_Comm msg_comm = MPI_COMM_NULL;

        LOG((2, "Creating global comm for async i/o service messages"));
        ret = create_async_service_msg_comm(uio_comm, &msg_comm);
        if(ret != PIO_NOERR)
        {
            return pio_err(NULL, NULL, ret, __FILE__, __LINE__,
                            "PIO Init (async) failed. Creating an MPI comm for asynchronous messages failed");
        }

        ret = MPI_Comm_rank(msg_comm, &rank);
        if(ret != MPI_SUCCESS)
        {
            return check_mpi(NULL, NULL, ret, __FILE__, __LINE__);
        }

        LOG((2, "Starting message handler io_rank = %d component_count = %d",
             rank, component_count));
        ret = pio_msg_handler2(rank, component_count, iosys, msg_comm);
        if(ret != PIO_NOERR)
        {
            return pio_err(NULL, NULL, ret, __FILE__, __LINE__,
                            "PIO Init (async) failed. Error processing asynchronous messages");
            LOG((2, "Returned from pio_msg_handler2(), Msg handler failed, ret = %d", ret));
        }
        LOG((2, "Returned from pio_msg_handler2() ret = %d", ret));
    }

#ifdef TIMING
    GPTLstop("PIO:PIOc_init_intercomm");
#endif
    return PIO_NOERR;
}

/**
 * Interface to call pio_init (from fortran)
 *
 * @param component_count The number of computational components to
 * associate with this IO component
 * @param f90_peer_comm  The communicator from which all other communicator
 * arguments are derived
 * @param f90_comp_comms The computational communicator for each of the
 * computational components
 * @param f90_io_comm The io communicator
 * @param iosysidp a pointer that gets the IO system ID
 * @returns 0 for success, error code otherwise
 * operations (defined in PIO_types).*
 */
int PIOc_Init_Intercomm_from_F90(int component_count, int f90_peer_comm,
                                  const int *f90_comp_comms, int f90_io_comm,
                                  int rearranger, int *iosysidps)
{
    MPI_Comm peer_comm = MPI_Comm_f2c(f90_peer_comm);
    MPI_Comm io_comm = MPI_Comm_f2c(f90_io_comm);
    int ret = PIO_NOERR;

    fortran_order = true;
    if((component_count <= 0) || (!f90_comp_comms) || (!iosysidps))
    {
        return pio_err(NULL, NULL, PIO_EINVAL, __FILE__, __LINE__,
                        "PIO Init (async, F2C wrapper) failed. Invalid arguments provided, component_count=%d (expected > 0), f90_comp_comms is %s (expected not NULL), iosysidps is %s (expected not NULL)", component_count, (f90_comp_comms) ? "not NULL" : "NULL", (iosysidps) ? "not NULL" : "NULL");
    }

    MPI_Comm comp_comms[component_count];
    for(int i=0; i<component_count; i++)
    {
        comp_comms[i] = MPI_Comm_f2c(f90_comp_comms[i]);
        iosysidps[i] = -1;
    }

    ret = PIOc_init_intercomm(component_count, peer_comm, comp_comms,
            io_comm, rearranger, iosysidps);
    if (ret != PIO_NOERR)
    {
        return pio_err(NULL, NULL, ret, __FILE__, __LINE__,
                        "PIO Init (async, F2C wrapper) failed");
    }

    return ret;
}

/**
 * Set the target blocksize for the box rearranger.
 *
 * @param newblocksize the new blocksize.
 * @returns 0 for success.
 * @ingroup PIO_set_blocksize
 * @author Jim Edwards
 */
int PIOc_set_blocksize(int newblocksize)
{
    if (newblocksize <= 0)
    {
        return pio_err(NULL, NULL, PIO_EINVAL, __FILE__, __LINE__,
                        "Setting block size for the BOX rearranger failed. The new block size (%d) needs to be > 0", newblocksize);
    }

    blocksize = newblocksize;
    return PIO_NOERR;
}
