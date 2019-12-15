/** @file
 *
 * Private functions to help read and write distributed arrays in PIO.
 *
 * When arrays are distributed, each processor holds some of the
 * array. Only by combining the distributed arrays from all processor
 * can the full array be obtained.
 *
 * @author Jim Edwards
 */

#include <limits.h>
#include <pio_config.h>
#include <pio.h>
#include <pio_internal.h>
#ifdef PIO_MICRO_TIMING
#include "pio_timer.h"
#endif

/* 10MB default limit. */
extern PIO_Offset pio_buffer_size_limit;

/* Initial size of compute buffer. */
bufsize pio_cnbuffer_limit = 0;

/* Maximum buffer usage. */
extern PIO_Offset maxusage;

/* handler for freeing the memory buffer pool */
void bpool_free(void *p)
{
  free(p);
}

/* Handler for allocating more memory for the bget buffer pool */
void *bpool_alloc(bufsize sz)
{
  void *p = malloc((size_t ) sz);
  return p;
}

/**
 * Initialize the compute buffer to size pio_cnbuffer_limit.
 *
 * This routine initializes the compute buffer pool if the bget memory
 * management is used. If malloc is used (that is, PIO_USE_MALLOC is
 * non zero), this function does nothing.
 *
 * @param ios pointer to the iosystem descriptor which will use the
 * new buffer.
 * @returns 0 for success, error code otherwise.
 * @author Jim Edwards
 */
int compute_buffer_init(iosystem_desc_t *ios)
{
    /* Default block size increment = 32 MB */
    const bufsize DEFAULT_BUF_INC_SZ = (bufsize ) (32L * 1024L * 1024L);
    bufsize bpool_block_inc_sz = (bufsize )((pio_buffer_size_limit > 0) ? pio_buffer_size_limit : DEFAULT_BUF_INC_SZ);
    LOG((2, "Initializing buffer pool with block increment = %lld bytes", (long long int) bpool_block_inc_sz));
    pio_cnbuffer_limit = bpool_block_inc_sz;
#if PIO_USE_MALLOC
    bpool(NULL, pio_cnbuffer_limit);
#else
    bectl(NULL, bpool_alloc, bpool_free, pio_cnbuffer_limit);
#endif /* PIO_USE_MALLOC */
    LOG((2, "compute_buffer_init complete"));

    return PIO_NOERR;
}

/** 
 * Fill start/count arrays for write_darray_multi_par(). This is an
 * internal function.
 * 
 * @param ndims the number of dims in the decomposition.
 * @param dimlen the lengths of dims in the decomposition.
 * @param fndims the number of dims in the file.
 * @param vdesc pointer to the var_desc_t info.
 * @param region pointer to a region.
 * @param start an already-allocated array which gets the start
 * values.
 * @param count an already-allocated array which gets the count
 * values.
 * @return 0 for success, error code otherwise.
 * @ingroup PIO_write_darray
 * @author Ed Hartnett
 */
int find_start_count(int ndims, const int *dimlen, int fndims, var_desc_t *vdesc,
                     io_region *region, size_t *start, size_t *count)
{
    /* Init start/count arrays to zero. */
    for (int i = 0; i < fndims; i++)
    {
        start[i] = 0;
        count[i] = 0;
    }

    if (region)
    {
        /* Allow extra outermost dimensions in the decomposition */
        int num_extra_dims = (vdesc->record >= 0 && fndims > 1)? (ndims - (fndims - 1)) : (ndims - fndims);
        pioassert(num_extra_dims >= 0, "Unexpected num_extra_dims", __FILE__, __LINE__);
        if (num_extra_dims > 0)
        {
            for (int d = 0; d < num_extra_dims; d++)
                pioassert(dimlen[d] == 1, "Extra outermost dimensions must have lengths of 1",
                          __FILE__, __LINE__);
        }

        if (vdesc->record >= 0 && fndims > 1)
        {
            /* This is a record based multidimensional
             * array. Figure out start/count for all but the
             * record dimension (dimid 0). */
            for (int i = 1; i < fndims; i++)
            {
                start[i] = region->start[num_extra_dims + (i - 1)];
                count[i] = region->count[num_extra_dims + (i - 1)];
            }

            /* Set count for record dimension (start cannot be determined so far). */
            if (count[1] > 0)
                count[0] = 1;
        }
        else
        {
            /* This is a non record variable. */
            for (int i = 0; i < fndims; i++)
            {
                start[i] = region->start[num_extra_dims + i];
                count[i] = region->count[num_extra_dims + i];
            }
        }

#if PIO_ENABLE_LOGGING
        /* Log arrays for debug purposes. */
        for (int i = 0; i < fndims; i++)
            LOG((3, "start[%d] = %d count[%d] = %d", i, start[i], i, count[i]));
#endif /* PIO_ENABLE_LOGGING */
    }
    
    return PIO_NOERR;
}

/**
 * Write a set of one or more aggregated arrays to output file. This
 * function is only used with parallel-netcdf and netcdf-4 parallel
 * iotypes. Serial io types use write_darray_multi_serial().
 *
 * @param file a pointer to the open file descriptor for the file
 * that will be written to
 * @param nvars the number of variables to be written with this
 * decomposition.
 * @param vid: an array of the variable ids to be written.
 * @param iodesc pointer to the io_desc_t info.
 * @param fill Non-zero if this write is fill data.
 * @param frame the record dimension for each of the nvars variables
 * in iobuf. NULL if this iodesc contains non-record vars.
 * @return 0 for success, error code otherwise.
 * @ingroup PIO_write_darray
 * @author Jim Edwards, Ed Hartnett
 */
int write_darray_multi_par(file_desc_t *file, int nvars, int fndims, const int *varids,
                           io_desc_t *iodesc, int fill, const int *frame)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    var_desc_t *vdesc;     /* Pointer to var info struct. */
    int dsize;             /* Data size (for one region). */
    int ierr = PIO_NOERR;

    /* Check inputs. */
    pioassert(file && file->iosystem && varids && varids[0] >= 0 && varids[0] <= PIO_MAX_VARS &&
              iodesc, "invalid input", __FILE__, __LINE__);

    LOG((1, "write_darray_multi_par nvars = %d iodesc->ndims = %d iodesc->mpitype = %d "
         "iodesc->maxregions = %d iodesc->llen = %d", nvars, iodesc->ndims,
         iodesc->mpitype, iodesc->maxregions, iodesc->llen));

#ifdef TIMING
    /* Start timing this function. */
    GPTLstart("PIO:write_darray_multi_par");
#endif

    /* Get pointer to iosystem. */
    ios = file->iosystem;

    /* Point to var description scruct for first var. */
    vdesc = file->varlist + varids[0];

    /* Set these differently for data and fill writing. */
    int num_regions = fill ? iodesc->maxfillregions: iodesc->maxregions;
    io_region *region = fill ? iodesc->fillregion : iodesc->firstregion;
    PIO_Offset llen = fill ? iodesc->holegridsize : iodesc->llen;
    void *iobuf = fill ? vdesc->fillbuf : file->iobuf[iodesc->ioid - PIO_IODESC_START_ID];

    /* If this is an IO task write the data. */
    if (ios->ioproc)
    {
        int rrcnt = 0; /* Number of subarray requests (pnetcdf only). */
        void *bufptr = NULL;
        size_t start[fndims];
        size_t count[fndims];
        PIO_Offset *startlist[num_regions]; /* Array of start arrays for ncmpi_iput_varn(). */
        PIO_Offset *countlist[num_regions]; /* Array of count  arrays for ncmpi_iput_varn(). */

        LOG((3, "num_regions = %d", num_regions));

        /* Process each region of data to be written. */
        for (int regioncnt = 0; regioncnt < num_regions; regioncnt++)
        {
            /* Fill the start/count arrays. */
            if ((ierr = find_start_count(iodesc->ndims, iodesc->dimlen, fndims, vdesc, region, start, count)))
            {
                ierr = pio_err(ios, file, ierr, __FILE__, __LINE__,
                                "Writing variables (number of variables = %d) to file (%s, ncid=%d) failed. Internal error, finding start/count for the I/O regions written out from the I/O process failed", nvars, pio_get_fname_from_file(file), file->pio_ncid);
                break;
            }

            /* IO tasks will run the netCDF/pnetcdf functions to write the data. */
            switch (file->iotype)
            {
#ifdef _NETCDF4
            case PIO_IOTYPE_NETCDF4P:
                /* For each variable to be written. */
                for (int nv = 0; nv < nvars; nv++)
                {
                    /* Set the start of the record dimension. */
                    if (vdesc->record >= 0 && fndims > 1)
                        start[0] = frame[nv];

                    /* If there is data for this region, get a pointer to it. */
                    if (region)
                        bufptr = (void *)((char *)iobuf + iodesc->mpitype_size * (nv * llen + region->loffset));

                    /* Ensure collective access. */
                    ierr = nc_var_par_access(file->fh, varids[nv], NC_COLLECTIVE);
                    if(ierr != NC_NOERR)
                    {
                        ierr = pio_err(ios, file, ierr, __FILE__, __LINE__,
                                        "Writing variables (number of variables = %d) to file (%s, ncid=%d) using PIO_IOTYPE_NETCDF4P iotype failed. Changing parallel access for variable (%s, varid=%d) to collective failed", nvars, pio_get_fname_from_file(file), file->pio_ncid, pio_get_vname_from_file(file, varids[nv]), varids[nv]);
                        break;
                    }

                    switch (iodesc->piotype)
                    {
                    case PIO_BYTE:
                        ierr = nc_put_vara_schar(file->fh, varids[nv], start, count, (signed char*)bufptr);
                        break;
                    case PIO_CHAR:
                        ierr = nc_put_vara_text(file->fh, varids[nv], start, count, (char*)bufptr);
                        break;
                    case PIO_SHORT:
                        ierr = nc_put_vara_short(file->fh, varids[nv], start, count, (short*)bufptr);
                        break;
                    case PIO_INT:
                        ierr = nc_put_vara_int(file->fh, varids[nv], start, count, (int*)bufptr);
                        break;
                    case PIO_FLOAT:
                        ierr = nc_put_vara_float(file->fh, varids[nv], start, count, (float*)bufptr);
                        break;
                    case PIO_DOUBLE:
                        ierr = nc_put_vara_double(file->fh, varids[nv], start, count, (double*)bufptr);
                        break;
                    case PIO_UBYTE:
                        ierr = nc_put_vara_uchar(file->fh, varids[nv], start, count, (unsigned char*)bufptr);
                        break;
                    case PIO_USHORT:
                        ierr = nc_put_vara_ushort(file->fh, varids[nv], start, count, (unsigned short*)bufptr);
                        break;
                    case PIO_UINT:
                        ierr = nc_put_vara_uint(file->fh, varids[nv], start, count, (unsigned int*)bufptr);
                        break;
                    case PIO_INT64:
                        ierr = nc_put_vara_longlong(file->fh, varids[nv], start, count, (long long*)bufptr);
                        break;
                    case PIO_UINT64:
                        ierr = nc_put_vara_ulonglong(file->fh, varids[nv], start, count, (unsigned long long*)bufptr);
                        break;
                    case PIO_STRING:
                        ierr = nc_put_vara_string(file->fh, varids[nv], start, count, (const char**)bufptr);
                        break;
                    default:
                        ierr = pio_err(ios, file, PIO_EBADTYPE, __FILE__, __LINE__,
                                        "Writing variables (number of variables = %d) to file (%s, ncid=%d) using PIO_IOTYPE_NETCDF4P iotype failed. Unsupported variable data type (type=%d)", nvars, pio_get_fname_from_file(file), file->pio_ncid, iodesc->piotype);
                        break;
                    }
                    if(ierr != NC_NOERR)
                    {
                        ierr = pio_err(ios, file, ierr, __FILE__, __LINE__,
                                        "Writing variables (number of variables = %d) to file (%s, ncid=%d) using PIO_IOTYPE_NETCDF4P iotype failed. Writing variable (%s, varid=%d) failed", nvars, pio_get_fname_from_file(file), file->pio_ncid, pio_get_vname_from_file(file, varids[nv]), varids[nv]);
                        break;
                    }
                }
                break;
#endif
#ifdef _PNETCDF
            case PIO_IOTYPE_PNETCDF:
                /* Get the total number of data elements we are
                 * writing for this region. */
                dsize = 1;
                for (int i = 0; i < fndims; i++)
                    dsize *= count[i];
                LOG((3, "dsize = %d", dsize));

                /* For pnetcdf's ncmpi_iput_varn() function, we need
                 * to provide arrays of arrays for start/count. */
                if (dsize > 0)
                {
                    /* Allocate storage for start/count arrays for
                     * this region. */
                    if (!(startlist[rrcnt] = calloc(fndims, sizeof(PIO_Offset))))
                    {
                        ierr = pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                                          "Writing variables (number of variables = %d) to file (%s, ncid=%d) using PIO_IOTYPE_PNETCDF iotype failed. Out of memory allocating buffer (%lld bytes) for array to store starts of I/O regions written out to file", nvars, pio_get_fname_from_file(file), file->pio_ncid, (long long int) (fndims * sizeof(PIO_Offset)));
                        break;
                    }
                    if (!(countlist[rrcnt] = calloc(fndims, sizeof(PIO_Offset))))
                    {
                        ierr = pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                                          "Writing variables (number of variables = %d) to file (%s, ncid=%d) using PIO_IOTYPE_PNETCDF iotype failed. Out of memory allocating buffer (%lld bytes) for array to store counts of I/O regions written out to file", nvars, pio_get_fname_from_file(file), file->pio_ncid, (long long int) (fndims * sizeof(PIO_Offset)));
                        break;
                    }

                    /* Copy the start/count arrays for this region. */
                    for (int i = 0; i < fndims; i++)
                    {
                        startlist[rrcnt][i] = start[i];
                        countlist[rrcnt][i] = count[i];
                        LOG((3, "startlist[%d][%d] = %d countlist[%d][%d] = %d", rrcnt, i,
                             startlist[rrcnt][i], rrcnt, i, countlist[rrcnt][i]));
                    }
                    rrcnt++;
                }

                /* Do this when we reach the last region. */
                if (regioncnt == num_regions - 1)
                {
                    /* For each variable to be written. */
                    for (int nv = 0; nv < nvars; nv++)
                    {
                        /* Get the var info. */
                        vdesc = file->varlist + varids[nv];

                        /* If this is a record (or quasi-record) var, set the start for
                         * the record dimension. */
                        if (vdesc->record >= 0 && fndims > 1)
                            for (int rc = 0; rc < rrcnt; rc++)
                                startlist[rc][0] = frame[nv];

                        /* Get a pointer to the data. */
                        bufptr = (void *)((char *)iobuf + nv * iodesc->mpitype_size * llen);

                        if (vdesc->nreqs % PIO_REQUEST_ALLOC_CHUNK == 0)
                        {
                            if (!(vdesc->request = realloc(vdesc->request, sizeof(int) *
                                                           (vdesc->nreqs + PIO_REQUEST_ALLOC_CHUNK))))
                            {
                                ierr = pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                                          "Writing variables (number of variables = %d) to file (%s, ncid=%d) using PIO_IOTYPE_PNETCDF iotype failed. Out of memory reallocing buffer (%lld bytes) for array to store pnetcdf request handles", nvars, pio_get_fname_from_file(file), file->pio_ncid, (long long int) (sizeof(int) * (vdesc->nreqs + PIO_REQUEST_ALLOC_CHUNK)));
                                break;
                            }

                            vdesc->request_sz = realloc(vdesc->request_sz,
                                                  sizeof(PIO_Offset) *
                                                  (vdesc->nreqs +
                                                    PIO_REQUEST_ALLOC_CHUNK));
                            if(vdesc->request_sz == NULL)
                            {
                                ierr = pio_err(ios, file, PIO_ENOMEM,
                                          __FILE__, __LINE__,
                                          "Writing variables (number of variables = %d) to file (%s, ncid=%d) using PIO_IOTYPE_PNETCDF iotype failed. Out of memory reallocing buffer (%lld bytes) for array to store pending pnetcdf request sizes", nvars, pio_get_fname_from_file(file), file->pio_ncid, (long long int) (sizeof(PIO_Offset) * (vdesc->nreqs + PIO_REQUEST_ALLOC_CHUNK)));
                                break;
                            }

                            for (int i = vdesc->nreqs; i < vdesc->nreqs + PIO_REQUEST_ALLOC_CHUNK; i++)
                            {
                                vdesc->request[i] = PIO_REQ_NULL;
                                vdesc->request_sz[i] = 0;
                            }
                        }

                        /* Write, in non-blocking fashion, a list of subarrays. */
                        LOG((3, "about to call ncmpi_iput_varn() varids[%d] = %d rrcnt = %d, llen = %d",
                             nv, varids[nv], rrcnt, llen));
                        ierr = ncmpi_iput_varn(file->fh, varids[nv], rrcnt, startlist, countlist,
                                               bufptr, llen, iodesc->mpitype, vdesc->request + vdesc->nreqs);
                        if (ierr != PIO_NOERR)
                        {
                            ierr = pio_err(ios, file, ierr, __FILE__, __LINE__,
                                      "Writing variables (number of variables = %d) to file (%s, ncid=%d) using PIO_IOTYPE_PNETCDF iotype failed. Non blocking write for variable (%s, varid=%d) failed (Number of subarray requests/regions=%d, Size of data local to this process = %lld)", nvars, pio_get_fname_from_file(file), file->pio_ncid, pio_get_vname_from_file(file, varids[nv]), varids[nv], rrcnt, (long long int )llen);
                            break;
                        }

                        /* PIO_REQ_NULL == NC_REQ_NULL */
                        if (vdesc->request[vdesc->nreqs] != PIO_REQ_NULL)
                        {
                            vdesc->request_sz[vdesc->nreqs] = llen * iodesc->mpitype_size;
                        }

                        /* Ensure that we increment the number of requests
                         * even if vdesc->request[vdesc->nreqs] == PIO_REQ_NULL
                         * for this process. This ensures that wait calls are
                         * in sync across multiple processes.
                         * Note: PnetCDF returns a NC_REQ_NULL for requests
                         * that are complete, e.g. 0 bytes written from the
                         * current process, on the current process
                         */
                        vdesc->nreqs++;
                    }

                    /* Free resources. */
                    for (int i = 0; i < rrcnt; i++)
                    {
                        free(startlist[i]);
                        free(countlist[i]);
                    }
                }
                break;
#endif
            default:
                ierr = pio_err(ios, file, PIO_EBADIOTYPE, __FILE__, __LINE__,
                                "Writing variables (number of variables = %d) to file (%s, ncid=%d) failed. Invalid iotype (%d) specified", nvars, pio_get_fname_from_file(file), file->pio_ncid, file->iotype);
                break;
            }

            if (ierr != PIO_NOERR)
            {
                ierr = pio_err(ios, file, ierr, __FILE__, __LINE__,
                                "Writing variables (number of variables = %d) to file (%s, ncid=%d) failed. Writing region of data at offset = %d failed", nvars, pio_get_fname_from_file(file), file->pio_ncid, region->loffset);
                break;
            }
            /* Go to next region. */
            if (region)
                region = region->next;
        } /* next regioncnt */
    } /* endif (ios->ioproc) */

    /* Check the return code from the netCDF/pnetcdf call. */
    ierr = check_netcdf(NULL, file, ierr, __FILE__,__LINE__);

#ifdef TIMING
    /* Stop timing this function. */
    GPTLstop("PIO:write_darray_multi_par");
#endif

    return ierr;
}

/**
 * Fill the tmp_start and tmp_count arrays, which contain the start
 * and count arrays for all regions.
 *
 * This is an internal function which is only called on io tasks. It
 * is called by write_darray_multi_serial().
 *
 * @param region pointer to the first in a linked list of regions.
 * @param maxregions the number of regions in the list.
 * @param fndims the number of dimensions in the file.
 * @param iodesc_ndims the number of dimensions in the decomposition.
 * @param dimlen the lengths of dimensions in the decomposition.
 * @param vdesc pointer to an array of var_desc_t for the vars being
 * written.
 * @param tmp_start pointer to an already allocaed array of length
 * fndims * maxregions. This array will get the start values for all
 * regions.
 * @param tmp_count pointer to an already allocaed array of length
 * fndims * maxregions. This array will get the count values for all
 * regions.
 * @returns 0 for success, error code otherwise.
 * @ingroup PIO_read_darray
 * @author Jim Edwards, Ed Hartnett
 **/
int find_all_start_count(io_region *region, int maxregions, int fndims,
                         int iodesc_ndims, const int *dimlen, var_desc_t *vdesc,
                         size_t *tmp_start, size_t *tmp_count)
{
    /* Check inputs. */
    pioassert(maxregions >= 0 && fndims > 0 && iodesc_ndims >= 0 && vdesc &&
              tmp_start && tmp_count, "invalid input", __FILE__, __LINE__);

    /* Find the start/count arrays for each region in the list. */
    for (int r = 0; r < maxregions; r++)
    {
        /* Initialize the start/count arrays for this region to 0. */
        for (int d = 0; d < fndims; d++)
        {
            tmp_start[d + r * fndims] = 0;
            tmp_count[d + r * fndims] = 0;
        }

        if (region)
        {
            /* Allow extra outermost dimensions in the decomposition */
            int num_extra_dims = (vdesc->record >= 0 && fndims > 1)? (iodesc_ndims - (fndims - 1)) : (iodesc_ndims - fndims);
            pioassert(num_extra_dims >= 0, "Unexpected num_extra_dims", __FILE__, __LINE__);
            if (num_extra_dims > 0)
            {
                for (int d = 0; d < num_extra_dims; d++)
                    pioassert(dimlen[d] == 1, "Extra outermost dimensions must have lengths of 1",
                              __FILE__, __LINE__);
            }

            if (vdesc->record >= 0 && fndims > 1)
            {
                /* This is a record based multidimensional
                 * array. Copy start/count for non-record
                 * dimensions. */
                for (int i = 1; i < fndims; i++)
                {
                    tmp_start[i + r * fndims] = region->start[num_extra_dims + (i - 1)];
                    tmp_count[i + r * fndims] = region->count[num_extra_dims + (i - 1)];
                    LOG((3, "tmp_start[%d] = %d tmp_count[%d] = %d", i + r * fndims,
                         tmp_start[i + r * fndims], i + r * fndims,
                         tmp_count[i + r * fndims]));
                }
            }
            else
            {
                /* This is not a record based multidimensional array. */
                for (int i = 0; i < fndims; i++)
                {
                    tmp_start[i + r * fndims] = region->start[num_extra_dims + i];
                    tmp_count[i + r * fndims] = region->count[num_extra_dims + i];
                    LOG((3, "tmp_start[%d] = %d tmp_count[%d] = %d", i + r * fndims,
                         tmp_start[i + r * fndims], i + r * fndims,
                         tmp_count[i + r * fndims]));
                }
            }

            /* Move to next region. */
            region = region->next;

        } /* endif region */
    } /* next r */

    return PIO_NOERR;
}

/**
 * Internal function called by IO tasks other than IO task 0 to send
 * their tmp_start/tmp_count arrays to IO task 0.
 *
 * This is an internal function which is only called on io tasks other
 * than IO task 0. It is called by write_darray_multi_serial().
 *
 * @return 0 for success, error code otherwise.
 * @ingroup PIO_write_darray
 * @author Jim Edwards, Ed Hartnett
 */
int send_all_start_count(iosystem_desc_t *ios, io_desc_t *iodesc, PIO_Offset llen,
                         int maxregions, int nvars, int fndims, size_t *tmp_start,
                         size_t *tmp_count, void *iobuf)
{
    MPI_Status status;     /* Recv status for MPI. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */
    int ierr = PIO_NOERR;    /* Return code. */

    /* Check inputs. */
    pioassert(ios && ios->ioproc && ios->io_rank > 0 && maxregions >= 0,
              "invalid inputs", __FILE__, __LINE__);

    /* Do a handshake. */
    if ((mpierr = MPI_Recv(&ierr, 1, MPI_INT, 0, 0, ios->io_comm, &status)))
        return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);

    /* Send local length of iobuffer for each field (all
     * fields are the same length). */
    if ((mpierr = MPI_Send((void *)&llen, 1, MPI_OFFSET, 0, ios->io_rank, ios->io_comm)))
        return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
    LOG((3, "sent llen = %d", llen));

    /* Send the number of data regions, the start/count for
     * all regions, and the data buffer with all the data. */
    if (llen > 0)
    {
        if ((mpierr = MPI_Send((void *)&maxregions, 1, MPI_INT, 0, ios->io_rank + ios->num_iotasks,
                               ios->io_comm)))
            return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
        if ((mpierr = MPI_Send(tmp_start, maxregions * fndims, MPI_OFFSET, 0,
                               ios->io_rank + 2 * ios->num_iotasks, ios->io_comm)))
            return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
        if ((mpierr = MPI_Send(tmp_count, maxregions * fndims, MPI_OFFSET, 0,
                               ios->io_rank + 3 * ios->num_iotasks, ios->io_comm)))
            return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
        if ((mpierr = MPI_Send(iobuf, nvars * llen, iodesc->mpitype, 0,
                               ios->io_rank + 4 * ios->num_iotasks, ios->io_comm)))
            return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
        LOG((3, "sent data for maxregions = %d", maxregions));
    }

    return PIO_NOERR;
}

/**
 * This is an internal function that is run only on IO proc 0. It
 * receives data from all the other IO tasks, and write that data to
 * disk. This is called from write_darray_multi_serial().
 *
 * @param file a pointer to the open file descriptor for the file
 * that will be written to.
 * @param varids an array of the variable ids to be written
 * @param frame the record dimension for each of the nvars variables
 * in iobuf.  NULL if this iodesc contains non-record vars.
 * @param iodesc pointer to the decomposition info.
 * @param llen length of the iobuffer on this task for a single
 * field.
 * @param maxregions max number of blocks to be written from this
 * iotask.
 * @param nvars the number of variables to be written with this
 * decomposition.
 * @param fndims the number of dimensions in the file.
 * @param tmp_start pointer to an already allocaed array of length
 * fndims * maxregions. This array will get the start values for all
 * regions.
 * @param tmp_count pointer to an already allocaed array of length
 * fndims * maxregions. This array will get the count values for all
 * regions.
 * @param iobuf the buffer to be written from this mpi task. May be
 * null. for example we have 8 ionodes and a distributed array with
 * global size 4, then at least 4 nodes will have a null iobuf. In
 * practice the box rearranger trys to have at least blocksize bytes
 * on each io task and so if the total number of bytes to write is
 * less than blocksize*numiotasks then some iotasks will have a NULL
 * iobuf.
 * @return 0 for success, error code otherwise.
 * @ingroup PIO_write_darray
 * @author Jim Edwards, Ed Hartnett
 */
int recv_and_write_data(file_desc_t *file, const int *varids, const int *frame,
                        io_desc_t *iodesc, PIO_Offset llen, int maxregions, int nvars,
                        int fndims, size_t *tmp_start, size_t *tmp_count, void *iobuf)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    size_t rlen;    /* Length of IO buffer on this task. */
    int rregions;   /* Number of regions in buffer for this task. */
    size_t start[fndims], count[fndims];
    size_t loffset;
    void *bufptr;
    var_desc_t *vdesc;     /* Contains info about the variable. */
    MPI_Status status;     /* Recv status for MPI. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */
    int ierr = PIO_NOERR;    /* Return code. */

    /* Check inputs. */
    pioassert(file && varids && iodesc && tmp_start && tmp_count, "invalid input",
              __FILE__, __LINE__);

    LOG((2, "recv_and_write_data llen = %d maxregions = %d nvars = %d fndims = %d",
         llen, maxregions, nvars, fndims));

    /* Get pointer to IO system. */
    ios = file->iosystem;

    /* For each of the other tasks that are using this task
     * for IO. */
    for (int rtask = 0; rtask < ios->num_iotasks; rtask++)
    {
        /* From the remote tasks, we send information about
         * the data regions. and also the data. */
        if (rtask)
        {
            /* handshake - tell the sending task I'm ready */
            if ((mpierr = MPI_Send(&ierr, 1, MPI_INT, rtask, 0, ios->io_comm)))
                return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);

            /* Get length of iobuffer for each field on this
             * task (all fields are the same length). */
            if ((mpierr = MPI_Recv(&rlen, 1, MPI_OFFSET, rtask, rtask, ios->io_comm,
                                   &status)))
                return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
            LOG((3, "received rlen = %d", rlen));

            /* Get the number of regions, the start/count
             * values for all regions, and the data buffer. */
            if (rlen > 0)
            {
                if ((mpierr = MPI_Recv(&rregions, 1, MPI_INT, rtask, rtask + ios->num_iotasks,
                                       ios->io_comm, &status)))
                    return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
                if ((mpierr = MPI_Recv(tmp_start, rregions * fndims, MPI_OFFSET, rtask,
                                       rtask + 2 * ios->num_iotasks, ios->io_comm, &status)))
                    return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
                if ((mpierr = MPI_Recv(tmp_count, rregions * fndims, MPI_OFFSET, rtask,
                                       rtask + 3 * ios->num_iotasks, ios->io_comm, &status)))
                    return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
                if ((mpierr = MPI_Recv(iobuf, nvars * rlen, iodesc->mpitype, rtask,
                                       rtask + 4 * ios->num_iotasks, ios->io_comm, &status)))
                    return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
                LOG((3, "received data rregions = %d fndims = %d", rregions, fndims));
            }
        }
        else /* task 0 */
        {
            rlen = llen;
            rregions = maxregions;
        }
        LOG((3, "rtask = %d rlen = %d rregions = %d", rtask, rlen, rregions));

        /* If there is data from this task, write it. */
        if (rlen > 0)
        {
            loffset = 0;
            for (int regioncnt = 0; regioncnt < rregions; regioncnt++)
            {
                LOG((3, "writing data for region with regioncnt = %d", regioncnt));

                /* Get the start/count arrays for this region. */
                for (int i = 0; i < fndims; i++)
                {
                    start[i] = tmp_start[i + regioncnt * fndims];
                    count[i] = tmp_count[i + regioncnt * fndims];
                    LOG((3, "start[%d] = %d count[%d] = %d", i, start[i], i, count[i]));
                }

                /* Process each variable in the buffer. */
                for (int nv = 0; nv < nvars; nv++)
                {
                    LOG((3, "writing buffer var %d", nv));
                    vdesc = file->varlist + varids[nv];

                    /* Get a pointer to the correct part of the buffer. */
                    bufptr = (void *)((char *)iobuf + iodesc->mpitype_size * (nv * rlen + loffset));

                    /* If this var has a record dim, set
                     * the start on that dim to the frame
                     * value for this variable. */
                    if (vdesc->record >= 0 && fndims > 1)
                    {
                        if (count[1] > 0)
                        {
                            count[0] = 1;
                            start[0] = frame[nv];
                        }
                    }

                    /* Call the netCDF functions to write the data. */
                    /*
                    if ((ierr = nc_put_vara(file->fh, varids[nv], start, count, bufptr)))
                        return check_netcdf(ios, NULL, ierr, __FILE__, __LINE__);
                    */
                    switch (iodesc->piotype)
                    {
#ifdef _NETCDF
                    case PIO_BYTE:
                        ierr = nc_put_vara_schar(file->fh, varids[nv], start, count, (signed char*)bufptr);
                        break;
                    case PIO_CHAR:
                        ierr = nc_put_vara_text(file->fh, varids[nv], start, count, (char*)bufptr);
                        break;
                    case PIO_SHORT:
                        ierr = nc_put_vara_short(file->fh, varids[nv], start, count, (short*)bufptr);
                        break;
                    case PIO_INT:
                        ierr = nc_put_vara_int(file->fh, varids[nv], start, count, (int*)bufptr);
                        break;
                    case PIO_FLOAT:
                        ierr = nc_put_vara_float(file->fh, varids[nv], start, count, (float*)bufptr);
                        break;
                    case PIO_DOUBLE:
                        ierr = nc_put_vara_double(file->fh, varids[nv], start, count, (double*)bufptr);
                        break;
#endif /* _NETCDF */
#ifdef _NETCDF4
                    case PIO_UBYTE:
                        ierr = nc_put_vara_uchar(file->fh, varids[nv], start, count, (unsigned char*)bufptr);
                        break;
                    case PIO_USHORT:
                        ierr = nc_put_vara_ushort(file->fh, varids[nv], start, count, (unsigned short*)bufptr);
                        break;
                    case PIO_UINT:
                        ierr = nc_put_vara_uint(file->fh, varids[nv], start, count, (unsigned int*)bufptr);
                        break;
                    case PIO_INT64:
                        ierr = nc_put_vara_longlong(file->fh, varids[nv], start, count, (long long*)bufptr);
                        break;
                    case PIO_UINT64:
                        ierr = nc_put_vara_ulonglong(file->fh, varids[nv], start, count, (unsigned long long*)bufptr);
                        break;
                    case PIO_STRING:
                        ierr = nc_put_vara_string(file->fh, varids[nv], start, count, (const char**)bufptr);
                        break;
#endif /* _NETCDF4 */
                    default:
                        ierr = pio_err(ios, file, PIO_EBADTYPE,
                                        __FILE__, __LINE__,
                                        "Writing multiple variables (number of variables = %d) to file (%s, ncid=%d) using serial I/O failed. Unsupported variable type (type = %d)", nvars, pio_get_fname_from_file(file), file->pio_ncid, iodesc->piotype);
                        break;
                    }
                    if(ierr != PIO_NOERR){
                        ierr = pio_err(ios, file, ierr, __FILE__, __LINE__,
                                        "Writing variable %s, varid=%d, (total number of variables = %d) to file %s (ncid=%d) using serial I/O failed.", pio_get_vname_from_file(file, varids[nv]), varids[nv], nvars, pio_get_fname_from_file(file), file->pio_ncid);
                        return ierr;
                    }
                } /* next var */

                /* Calculate the total size. */
                size_t tsize = 1;
                for (int i = 0; i < fndims; i++)
                    tsize *= count[i];

                /* Keep track of where we are in the buffer. */
                loffset += tsize;

                LOG((3, " at bottom of loop regioncnt = %d tsize = %d loffset = %d", regioncnt,
                     tsize, loffset));
            } /* next regioncnt */
        } /* endif (rlen > 0) */
    } /* next rtask */

    return PIO_NOERR;
}

/**
 * Write a set of one or more aggregated arrays to output file in
 * serial mode. This function is called for netCDF classic and
 * netCDF-4 serial iotypes. Parallel iotypes use
 * write_darray_multi_par().
 *
 * @param file a pointer to the open file descriptor for the file
 * that will be written to.
 * @param nvars the number of variables to be written with this
 * decomposition.
 * @param varids an array of the variable ids to be written
 * @param iodesc pointer to the decomposition info.
 * @param fill Non-zero if this write is fill data.
 * @param frame the record dimension for each of the nvars variables
 * in iobuf. NULL if this iodesc contains non-record vars.
 * @return 0 for success, error code otherwise.
 * @ingroup PIO_write_darray
 * @author Jim Edwards, Ed Hartnett
 */
int write_darray_multi_serial(file_desc_t *file, int nvars, int fndims, const int *varids,
                              io_desc_t *iodesc, int fill, const int *frame)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    var_desc_t *vdesc;     /* Contains info about the variable. */
    int ierr = PIO_NOERR;              /* Return code. */

    /* Check inputs. */
    pioassert(file && file->iosystem && file->varlist && varids && varids[0] >= 0 &&
              varids[0] <= PIO_MAX_VARS && iodesc, "invalid input", __FILE__, __LINE__);

    LOG((1, "write_darray_multi_serial nvars = %d fndims = %d iodesc->ndims = %d "
         "iodesc->mpitype = %d", nvars, iodesc->ndims, fndims, iodesc->mpitype));

    /* Get the iosystem info. */
    ios = file->iosystem;

    /* Get the var info. */
    vdesc = file->varlist + varids[0];
    LOG((2, "vdesc record %d nreqs %d ios->async = %d", vdesc->record, vdesc->nreqs,
         ios->async));

    /* Set these differently for data and fill writing. iobuf may be
     * null if array size < number of nodes. */
    int num_regions = fill ? iodesc->maxfillregions: iodesc->maxregions;
    io_region *region = fill ? iodesc->fillregion : iodesc->firstregion;
    PIO_Offset llen = fill ? iodesc->holegridsize : iodesc->llen;
    void *iobuf = fill ? vdesc->fillbuf : file->iobuf[iodesc->ioid - PIO_IODESC_START_ID];

#ifdef TIMING
    /* Start timing this function. */
    GPTLstart("PIO:write_darray_multi_serial");
#endif

    /* Only IO tasks participate in this code. */
    if (ios->ioproc)
    {
        size_t tmp_start[fndims * num_regions]; /* A start array for each region. */
        size_t tmp_count[fndims * num_regions]; /* A count array for each region. */

        LOG((3, "num_regions = %d", num_regions));

        /* Fill the tmp_start and tmp_count arrays, which contain the
         * start and count arrays for all regions. */
        if ((ierr = find_all_start_count(region, num_regions, fndims, iodesc->ndims, iodesc->dimlen, vdesc,
                                         tmp_start, tmp_count)))
        {
            ierr = pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Writing multiple variables (number of variables = %d) to file (%s, ncid=%d) using serial I/O failed. Internal error finding start/count of I/O regions to write to file.", nvars, pio_get_fname_from_file(file), file->pio_ncid);
        }

        if (ierr == PIO_NOERR)
        {
            /* Tasks other than 0 will send their data to task 0. */
            if (ios->io_rank > 0)
            {
                /* Send the tmp_start and tmp_count arrays from this IO task
                 * to task 0. */
                if ((ierr = send_all_start_count(ios, iodesc, llen, num_regions, nvars, fndims,
                                                 tmp_start, tmp_count, iobuf)))
                {
                    ierr = pio_err(ios, file, ierr, __FILE__, __LINE__,
                                    "Writing multiple variables (number of variables = %d) to file (%s, ncid=%d) using serial I/O failed. Internal error sending start/count of I/O regions to write to file from root process.", nvars, pio_get_fname_from_file(file), file->pio_ncid);
                }
            }
            else
            {
                /* Task 0 will receive data from all other IO tasks. */

                if ((ierr = recv_and_write_data(file, varids, frame, iodesc, llen, num_regions, nvars, fndims,
                                                tmp_start, tmp_count, iobuf)))
                {
                    ierr = pio_err(ios, file, ierr, __FILE__, __LINE__,
                                    "Writing multiple variables (number of variables = %d) to file (%s, ncid=%d) using serial I/O failed. Internal error receiving start/count of I/O regions to write to file from non-root processes.", nvars, pio_get_fname_from_file(file), file->pio_ncid);
                }
            } /* if (ios->io_rank > 0) */
        } /* if (ierr == PIO_NOERR) */
    } /* if (ios->ioproc) */

    ierr = check_netcdf(ios, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc_put_vara* or sending data to root failed, ierr = %d", ierr));
        return pio_err(ios, file, ierr, __FILE__, __LINE__,
                        "Writing multiple variables (number of variables = %d) to file (%s, ncid=%d) using serial I/O failed. Internal error in I/O processes finding/sending/receiving start/count of I/O regions to write to file", nvars, pio_get_fname_from_file(file), file->pio_ncid);
    }

#ifdef TIMING
    /* Stop timing this function. */
    GPTLstop("PIO:write_darray_multi_serial");
#endif

    return PIO_NOERR;
}

/**
 * Read an array of data from a file to the (parallel) IO library.
 *
 * @param file a pointer to the open file descriptor for the file
 * that will be written to
 * @param fndims The number of dims in the file
 * @param iodesc a pointer to the defined iodescriptor for the buffer
 * @param vid the variable id to be read
 * @param iobuf the buffer to be read into from this mpi task. May be
 * null. for example we have 8 ionodes and a distributed array with
 * global size 4, then at least 4 nodes will have a null iobuf. In
 * practice the box rearranger trys to have at least blocksize bytes
 * on each io task and so if the total number of bytes to write is
 * less than blocksize*numiotasks then some iotasks will have a NULL
 * iobuf.
 * @return 0 on success, error code otherwise.
 * @ingroup PIO_read_darray
 * @author Jim Edwards, Ed Hartnett
 */
int pio_read_darray_nc(file_desc_t *file, int fndims, io_desc_t *iodesc, int vid, void *iobuf)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    var_desc_t *vdesc;     /* Information about the variable. */
    int ndims;             /* Number of dims in decomposition. */
    int ierr = PIO_NOERR;  /* Return code from netCDF functions. */

    /* Check inputs. */
    pioassert(file && (fndims > 0) && file->iosystem && iodesc && vid <= PIO_MAX_VARS, "invalid input",
              __FILE__, __LINE__);

#ifdef TIMING
    /* Start timing this function. */
    GPTLstart("PIO:read_darray_nc");
#endif

    /* Get the IO system info. */
    ios = file->iosystem;

    /* Get the variable info. */
    vdesc = file->varlist + vid;

    /* Get the number of dimensions in the decomposition. */
    ndims = iodesc->ndims;

    /* IO procs will actially read the data. */
    if (ios->ioproc)
    {
        io_region *region;
        size_t start[fndims];
        size_t count[fndims];
        size_t tmp_bufsize = 1;
        void *bufptr;
        int rrlen = 0;
        PIO_Offset *startlist[iodesc->maxregions];
        PIO_Offset *countlist[iodesc->maxregions];

        /* buffer is incremented by byte and loffset is in terms of
           the iodessc->mpitype so we need to multiply by the size of
           the mpitype. */
        region = iodesc->firstregion;

        /* This is a record (or quasi-record) var. If the record
           number has not been set yet, set it to 0 by default */
        if (fndims > ndims)
        {
            if (vdesc->record < 0)
                vdesc->record = 0;
        }

        /* For each regions, read the data. */
        for (int regioncnt = 0; regioncnt < iodesc->maxregions; regioncnt++)
        {
            tmp_bufsize = 1;
            if (region == NULL || iodesc->llen == 0)
            {
                /* No data for this region. */
                for (int i = 0; i < fndims; i++)
                {
                    start[i] = 0;
                    count[i] = 0;
                }
                bufptr = NULL;
            }
            else
            {
                /* Get a pointer where we should put the data we read. */
                if (regioncnt == 0 || region == NULL)
                    bufptr = iobuf;
                else
                    bufptr=(void *)((char *)iobuf + iodesc->mpitype_size * region->loffset);

                LOG((2, "%d %d %d", iodesc->llen - region->loffset, iodesc->llen, region->loffset));

                /* Allow extra outermost dimensions in the decomposition */
                int num_extra_dims = (vdesc->record >= 0 && fndims > 1)? (ndims - (fndims - 1)) : (ndims - fndims);
                pioassert(num_extra_dims >= 0, "Unexpected num_extra_dims", __FILE__, __LINE__);
                if (num_extra_dims > 0)
                {
                    for (int d = 0; d < num_extra_dims; d++)
                        pioassert(iodesc->dimlen[d] == 1, "Extra outermost dimensions must have lengths of 1",
                                  __FILE__, __LINE__);
                }

                /* Get the start/count arrays. */
                if (vdesc->record >= 0 && fndims > 1)
                {
                    /* This is a record (or quasi-record) var. The record dimension
                     * (0) is handled specially. */
                    start[0] = vdesc->record;
                    for (int i = 1; i < fndims; i++)
                    {
                        start[i] = region->start[num_extra_dims + (i - 1)];
                        count[i] = region->count[num_extra_dims + (i - 1)];
                    }

                    /* Read one record. */
                    if (count[1] > 0)
                        count[0] = 1;
                }
                else
                {
                    /* Non-time dependent array */
                    for (int i = 0; i < fndims; i++)
                    {
                        start[i] = region->start[num_extra_dims + i];
                        count[i] = region->count[num_extra_dims + i];
                    }
                }
            }

            /* Do the read. */
            switch (file->iotype)
            {
#ifdef _NETCDF4
            case PIO_IOTYPE_NETCDF4P:
                /* ierr = nc_get_vara(file->fh, vid, start, count, bufptr); */
                switch (iodesc->piotype)
                {
                case PIO_BYTE:
                    ierr = nc_get_vara_schar(file->fh, vid, start, count, (signed char*)bufptr);
                    break;
                case PIO_CHAR:
                    ierr = nc_get_vara_text(file->fh, vid, start, count, (char*)bufptr);
                    break;
                case PIO_SHORT:
                    ierr = nc_get_vara_short(file->fh, vid, start, count, (short*)bufptr);
                    break;
                case PIO_INT:
                    ierr = nc_get_vara_int(file->fh, vid, start, count, (int*)bufptr);
                    break;
                case PIO_FLOAT:
                    ierr = nc_get_vara_float(file->fh, vid, start, count, (float*)bufptr);
                    break;
                case PIO_DOUBLE:
                    ierr = nc_get_vara_double(file->fh, vid, start, count, (double*)bufptr);
                    break;
                case PIO_UBYTE:
                    ierr = nc_get_vara_uchar(file->fh, vid, start, count, (unsigned char*)bufptr);
                    break;
                case PIO_USHORT:
                    ierr = nc_get_vara_ushort(file->fh, vid, start, count, (unsigned short*)bufptr);
                    break;
                case PIO_UINT:
                    ierr = nc_get_vara_uint(file->fh, vid, start, count, (unsigned int*)bufptr);
                    break;
                case PIO_INT64:
                    ierr = nc_get_vara_longlong(file->fh, vid, start, count, (long long*)bufptr);
                    break;
                case PIO_UINT64:
                    ierr = nc_get_vara_ulonglong(file->fh, vid, start, count, (unsigned long long*)bufptr);
                    break;
                case PIO_STRING:
                    ierr = nc_get_vara_string(file->fh, vid, start, count, (char**)bufptr);
                    break;
                default:
                    ierr = pio_err(ios, file, PIO_EBADTYPE, __FILE__, __LINE__,
                                    "Reading variable (%s, varid=%d) from file (%s, ncid=%d) failed with iotype=PIO_IOTYPE_NETCDF4P. Unsupported variable type (type=%d)", pio_get_vname_from_file(file, vid), vid, pio_get_fname_from_file(file), file->pio_ncid, file->iotype);
                    break;
                }
                break;
#endif
#ifdef _PNETCDF
            case PIO_IOTYPE_PNETCDF:
            {
                tmp_bufsize = 1;
                for (int j = 0; j < fndims; j++)
                    tmp_bufsize *= count[j];

                if (tmp_bufsize > 0)
                {
                    startlist[rrlen] = bget(fndims * sizeof(PIO_Offset));
                    countlist[rrlen] = bget(fndims * sizeof(PIO_Offset));

                    for (int j = 0; j < fndims; j++)
                    {
                        startlist[rrlen][j] = start[j];
                        countlist[rrlen][j] = count[j];
                    }
                    rrlen++;
                }

                /* Is this is the last region to process? */
                if (regioncnt == iodesc->maxregions - 1)
                {
                    /* Read a list of subarrays. */
                    ierr = ncmpi_get_varn_all(file->fh, vid, rrlen, startlist,
                                              countlist, iobuf, iodesc->llen, iodesc->mpitype);
                    if(ierr != PIO_NOERR)
                    {
                        ierr = pio_err(ios, file, ierr, __FILE__, __LINE__,
                                "Reading variable (%s, varid=%d) from file (%s, ncid=%d) failed with PIO_IOTYPE_PNETCDF iotype. The low level (PnetCDF) I/O library call failed to read the variable (Number of regions = %d, iodesc id = %d, Bytes to read on this process = %llu)", pio_get_vname_from_file(file, vid), vid, pio_get_fname_from_file(file), file->pio_ncid, rrlen, iodesc->ioid, (unsigned long long int) iodesc->llen);
                        break;
                    }

                    /* Release the start and count arrays. */
                    for (int i = 0; i < rrlen; i++)
                    {
                        brel(startlist[i]);
                        brel(countlist[i]);
                    }
                }
            }
            break;
#endif
            default:
                ierr = pio_err(ios, file, PIO_EBADIOTYPE, __FILE__, __LINE__,
                                "Reading variable (%s, varid=%d) from file (%s, ncid=%d) failed with iotype=PIO_IOTYPE_PNETCDF. Unsupported variable type (type=%d)", pio_get_vname_from_file(file, vid), vid, pio_get_fname_from_file(file), file->pio_ncid, file->iotype);
                break;
            }

            /* Check return code. */
            if(ierr != PIO_NOERR){
              break;
            }

            /* Move to next region. */
            if (region)
                region = region->next;
        } /* next regioncnt */
    }
    ierr = check_netcdf(NULL, file, ierr, __FILE__,__LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc*_get_var* failed, ierr = %d", ierr));
        return pio_err(NULL, file, ierr, __FILE__, __LINE__,
                        "Reading variable (%s, varid=%d) from file (%s, ncid=%d) failed with iotype=%s. The underlying I/O library (%s) call, nc*_get_var*, failed.", pio_get_vname_from_file(file, vid), vid, pio_get_fname_from_file(file), file->pio_ncid, pio_iotype_to_string(file->iotype), (file->iotype == PIO_IOTYPE_NETCDF4P) ? "NetCDF" : "PnetCDF");
    }

#ifdef TIMING
    /* Stop timing this function. */
    GPTLstop("PIO:read_darray_nc");
#endif

    return PIO_NOERR;
}

/**
 * Read an array of data from a file to the (serial) IO library. This
 * function is only used with netCDF classic and netCDF-4 serial
 * iotypes.
 *
 * @param file a pointer to the open file descriptor for the file
 * that will be written to
 * @param fndims The number of dims in the file
 * @param iodesc a pointer to the defined iodescriptor for the buffer
 * @param vid the variable id to be read.
 * @param iobuf the buffer to be written from this mpi task. May be
 * null. for example we have 8 ionodes and a distributed array with
 * global size 4, then at least 4 nodes will have a null iobuf. In
 * practice the box rearranger trys to have at least blocksize bytes
 * on each io task and so if the total number of bytes to write is
 * less than blocksize * numiotasks then some iotasks will have a NULL
 * iobuf.
 * @returns 0 for success, error code otherwise.
 * @ingroup PIO_read_darray
 * @author Jim Edwards, Ed Hartnett
 */
int pio_read_darray_nc_serial(file_desc_t *file, int fndims, io_desc_t *iodesc, int vid,
                              void *iobuf)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    var_desc_t *vdesc;     /* Information about the variable. */
    int ndims;             /* Number of dims in decomposition. */
    MPI_Status status;
    int mpierr = MPI_SUCCESS;  /* Return code from MPI functions. */
    int ierr = PIO_NOERR;

    /* Check inputs. */
    pioassert(file && (fndims > 0) && file->iosystem && iodesc && vid >= 0 && vid <= PIO_MAX_VARS,
              "invalid input", __FILE__, __LINE__);

#ifdef TIMING
    /* Start timing this function. */
    GPTLstart("PIO:read_darray_nc_serial");
#endif
    ios = file->iosystem;

    /* Get var info for this var. */
    vdesc = file->varlist + vid;

    /* Get the number of dims in our decomposition. */
    ndims = iodesc->ndims;

    if (ios->ioproc)
    {
        io_region *region;
        size_t start[fndims];
        size_t count[fndims];
        size_t tmp_start[fndims * iodesc->maxregions];
        size_t tmp_count[fndims * iodesc->maxregions];
        size_t tmp_bufsize;
        void *bufptr;

        /* buffer is incremented by byte and loffset is in terms of
           the iodessc->mpitype so we need to multiply by the size of
           the mpitype. */
        region = iodesc->firstregion;

        /* This is a record (or quasi-record) var. If the record
           number has not been set yet, set it to 0 by default */
        if (fndims > ndims)
        {
            if (vdesc->record < 0)
                vdesc->record = 0;
        }

        /* Put together start/count arrays for all regions. */
        for (int regioncnt = 0; regioncnt < iodesc->maxregions; regioncnt++)
        {
            if (!region || iodesc->llen == 0)
            {
                /* Nothing to write for this region. */
                for (int i = 0; i < fndims; i++)
                {
                    tmp_start[i + regioncnt * fndims] = 0;
                    tmp_count[i + regioncnt * fndims] = 0;
                }
                bufptr = NULL;
            }
            else
            {
                /* Allow extra outermost dimensions in the decomposition */
                int num_extra_dims = (vdesc->record >= 0 && fndims > 1)? (ndims - (fndims - 1)) : (ndims - fndims);
                pioassert(num_extra_dims >= 0, "Unexpected num_extra_dims", __FILE__, __LINE__);
                if (num_extra_dims > 0)
                {
                    for (int d = 0; d < num_extra_dims; d++)
                        pioassert(iodesc->dimlen[d] == 1, "Extra outermost dimensions must have lengths of 1",
                                  __FILE__, __LINE__);
                }

                if (vdesc->record >= 0 && fndims > 1)
                {
                    /* This is a record (or quasi-record) var. Find start for record dims. */
                    tmp_start[regioncnt * fndims] = vdesc->record;

                    /* Find start/count for all non-record dims. */
                    for (int i = 1; i < fndims; i++)
                    {
                        tmp_start[i + regioncnt * fndims] = region->start[num_extra_dims + (i - 1)];
                        tmp_count[i + regioncnt * fndims] = region->count[num_extra_dims + (i - 1)];
                    }

                    /* Set count for record dimension. */
                    if (tmp_count[1 + regioncnt * fndims] > 0)
                        tmp_count[regioncnt * fndims] = 1;
                }
                else
                {
                    /* Non-time dependent array */
                    for (int i = 0; i < fndims; i++)
                    {
                        tmp_start[i + regioncnt * fndims] = region->start[num_extra_dims + i];
                        tmp_count[i + regioncnt * fndims] = region->count[num_extra_dims + i];
                    }
                }
            }

#if PIO_ENABLE_LOGGING
            /* Log arrays for debug purposes. */
            LOG((3, "region = %d", region));
            for (int i = 0; i < fndims; i++)
                LOG((3, "tmp_start[%d] = %d tmp_count[%d] = %d", i + regioncnt * fndims, tmp_start[i + regioncnt * fndims],
                     i + regioncnt * fndims, tmp_count[i + regioncnt * fndims]));
#endif /* PIO_ENABLE_LOGGING */

            /* Move to next region. */
            if (region)
                region = region->next;
        } /* next regioncnt */

        /* IO tasks other than 0 send their starts/counts and data to
         * IO task 0. */
        if (ios->io_rank > 0)
        {
            if ((mpierr = MPI_Send(&iodesc->llen, 1, MPI_OFFSET, 0, ios->io_rank, ios->io_comm)))
                return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
            LOG((3, "sent iodesc->llen = %d", iodesc->llen));

            if (iodesc->llen > 0)
            {
                if ((mpierr = MPI_Send(&(iodesc->maxregions), 1, MPI_INT, 0,
                                       ios->num_iotasks + ios->io_rank, ios->io_comm)))
                    return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
                if ((mpierr = MPI_Send(tmp_count, iodesc->maxregions * fndims, MPI_OFFSET, 0,
                                       2 * ios->num_iotasks + ios->io_rank, ios->io_comm)))
                    return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
                if ((mpierr = MPI_Send(tmp_start, iodesc->maxregions * fndims, MPI_OFFSET, 0,
                                       3 * ios->num_iotasks + ios->io_rank, ios->io_comm)))
                    return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
                LOG((3, "sent iodesc->maxregions = %d tmp_count and tmp_start arrays", iodesc->maxregions));

                if ((mpierr = MPI_Recv(iobuf, iodesc->llen, iodesc->mpitype, 0,
                                       4 * ios->num_iotasks + ios->io_rank, ios->io_comm, &status)))
                    return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
                LOG((3, "received %d elements of data", iodesc->llen));
            }
        }
        else if (ios->io_rank == 0)
        {
            /* This is IO task 0. Get starts/counts and data from
             * other IO tasks. */
            int maxregions = 0;
            size_t loffset, regionsize;
            size_t this_start[fndims * iodesc->maxregions];
            size_t this_count[fndims * iodesc->maxregions];

            for (int rtask = 1; rtask <= ios->num_iotasks; rtask++)
            {
                if (rtask < ios->num_iotasks)
                {
                    if ((mpierr = MPI_Recv(&tmp_bufsize, 1, MPI_OFFSET, rtask, rtask, ios->io_comm, &status)))
                        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
                    LOG((3, "received tmp_bufsize = %d", tmp_bufsize));

                    if (tmp_bufsize > 0)
                    {
                        if ((mpierr = MPI_Recv(&maxregions, 1, MPI_INT, rtask, ios->num_iotasks + rtask,
                                               ios->io_comm, &status)))
                            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
                        if ((mpierr = MPI_Recv(this_count, maxregions * fndims, MPI_OFFSET, rtask,
                                               2 * ios->num_iotasks + rtask, ios->io_comm, &status)))
                            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
                        if ((mpierr = MPI_Recv(this_start, maxregions * fndims, MPI_OFFSET, rtask,
                                               3 * ios->num_iotasks + rtask, ios->io_comm, &status)))
                            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
                        LOG((3, "received maxregions = %d this_count, this_start arrays ", maxregions));
                    }
                }
                else
                {
                    maxregions = iodesc->maxregions;
                    tmp_bufsize = iodesc->llen;
                }
                LOG((3, "maxregions = %d tmp_bufsize = %d", maxregions, tmp_bufsize));

                /* Now get each region of data. */
                loffset = 0;
                for (int regioncnt = 0; regioncnt < maxregions; regioncnt++)
                {
                    /* Get pointer where data should go. */
                    bufptr = (void *)((char *)iobuf + iodesc->mpitype_size * loffset);
                    regionsize = 1;

                    /* ??? */
                    if (rtask < ios->num_iotasks)
                    {
                        for (int m = 0; m < fndims; m++)
                        {
                            start[m] = this_start[m + regioncnt * fndims];
                            count[m] = this_count[m + regioncnt * fndims];
                            regionsize *= count[m];
                        }
                    }
                    else
                    {
                        for (int m = 0; m < fndims; m++)
                        {
                            start[m] = tmp_start[m + regioncnt * fndims];
                            count[m] = tmp_count[m + regioncnt * fndims];
                            regionsize *= count[m];
                        }
                    }
                    loffset += regionsize;

                    /* Read the data. */
                    /* ierr = nc_get_vara(file->fh, vid, start, count, bufptr); */
                    switch (iodesc->piotype)
                    {
#ifdef _NETCDF
                    case PIO_BYTE:
                        ierr = nc_get_vara_schar(file->fh, vid, start, count, (signed char*)bufptr);
                        break;
                    case PIO_CHAR:
                        ierr = nc_get_vara_text(file->fh, vid, start, count, (char*)bufptr);
                        break;
                    case PIO_SHORT:
                        ierr = nc_get_vara_short(file->fh, vid, start, count, (short*)bufptr);
                        break;
                    case PIO_INT:
                        ierr = nc_get_vara_int(file->fh, vid, start, count, (int*)bufptr);
                        break;
                    case PIO_FLOAT:
                        ierr = nc_get_vara_float(file->fh, vid, start, count, (float*)bufptr);
                        break;
                    case PIO_DOUBLE:
                        ierr = nc_get_vara_double(file->fh, vid, start, count, (double*)bufptr);
                        break;
#endif /* _NETCDF */
#ifdef _NETCDF4
                    case PIO_UBYTE:
                        ierr = nc_get_vara_uchar(file->fh, vid, start, count, (unsigned char*)bufptr);
                        break;
                    case PIO_USHORT:
                        ierr = nc_get_vara_ushort(file->fh, vid, start, count, (unsigned short*)bufptr);
                        break;
                    case PIO_UINT:
                        ierr = nc_get_vara_uint(file->fh, vid, start, count, (unsigned int*)bufptr);
                        break;
                    case PIO_INT64:
                        ierr = nc_get_vara_longlong(file->fh, vid, start, count, (long long*)bufptr);
                        break;
                    case PIO_UINT64:
                        ierr = nc_get_vara_ulonglong(file->fh, vid, start, count, (unsigned long long*)bufptr);
                        break;
                    case PIO_STRING:
                        ierr = nc_get_vara_string(file->fh, vid, start, count, (char**)bufptr);
                        break;
#endif /* _NETCDF4 */
                    default:
                        ierr = pio_err(ios, file, PIO_EBADTYPE, __FILE__, __LINE__,
                                        "Reading variable (%s, varid=%d) from file (%s, ncid=%d) with serial I/O failed. Unsupported variable type (iotype=%d)", pio_get_vname_from_file(file, vid), vid, pio_get_fname_from_file(file), file->pio_ncid, iodesc->piotype);
                        break;
                    }

                    /* Check error code of netCDF call. */
                    if(ierr != PIO_NOERR){
                        break;
                    }
                }
                if(ierr != PIO_NOERR){
                    break;
                }

                /* The decomposition may not use all of the active io
                 * tasks. rtask here is the io task rank and
                 * ios->num_iotasks is the number of iotasks actually
                 * used in this decomposition. */
                if (rtask < ios->num_iotasks && tmp_bufsize > 0)
                    if ((mpierr = MPI_Send(iobuf, tmp_bufsize, iodesc->mpitype, rtask,
                                           4 * ios->num_iotasks + rtask, ios->io_comm)))
                        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
            }
        }
    }
    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc*_get_var* failed, ierr = %d", ierr));
        return pio_err(ios, file, ierr, __FILE__, __LINE__,
                        "Reading variable (%s, varid=%d) from file (%s, ncid=%d) with serial I/O failed. The underlying I/O library call to write data failed on root I/O process", pio_get_vname_from_file(file, vid), vid, pio_get_fname_from_file(file), file->pio_ncid);
    }

#ifdef TIMING
    /* Stop timing this function. */
    GPTLstop("PIO:read_darray_nc_serial");
#endif

    return PIO_NOERR;
}

/* Max size of cached requests (in bytes) that we wait on
 * in a single wait call.
 * This is only valid for PIO_IOTYPE_PNETCDF iotype
 * PnetCDF has a limitation on the aggregate size of requests
 * from a single rank (due to inherent limitations in the MPI
 * I/O libraries) that it cannot exceed INT_MAX
 */
PIO_Offset file_req_block_sz_limit = INT_MAX;

/* Set the aggregate size of requests used in a single blocking
 * wait call
 * This setting is only used for PIO_IOTYPE_PNETCDF iotype
 */
int set_file_req_block_size_limit(file_desc_t *file, PIO_Offset sz)
{
  assert(file && (sz > 0));
  file_req_block_sz_limit = sz;

  return PIO_NOERR;
}

/**
 * Convert the pending requests on a file to blocks of size
 * < file_req_block_sz_limit . The function returns multiple blocks
 * {[req_block_starts[0], req_block_ends[0]], ...} such
 * that each block size <= file_req_block_sz_limit. If any single
 * request is > file_req_block_sz_limit it is allocated an
 * individual block and a warning is printed out
 * 
 * The array *preq_block_ranges contains start and end indices
 * to the *preqs array indicating multiple blocks of requests.
 * The first half ( [0, nreq_blocks) ) of the array includes
 * the start indices and the latter half
 * ( [nreq_blocks, 2 * nreq_blocks) ) of the array includes
 * the end indices of each block.
 * The request block i in the file->varlist array is
 * determined by the range,
 * [(*preq_block_ranges)[i],
 *    (*preq_block_ranges)[i + nreq_blocks]]
 * (the end index is also part of the block)
 *
 * Collective on all I/O processes (in the I/O system associated
 * with the file)
 * This function is only used by the PIO_IOTYPE_PNETCDF
 *
 * @param file Pointer to file descriptor of the file
 * @param preqs Pointer to a buffer that will contain
 *          pending requests on this file
 * @param nreqs Pointer to integer that will contain the number
 *          of pending requests on this file
 * @param nvars_with_reqs Pointer to integer that will contain
 *          the number of variables that have valid pending
            requests
 * @param last_var_with_req Pointer to integer that will contain
 *          the index of the last variable in file->varlist that
 *          contains pending requests
 * @param preq_block_ranges Pointer to buffer that will contain
 *          the block ranges
 *          The initial half of the buffer contains start
 *          indices and the latter half the end indices of
 *          the block ranges
 *          i.e., (*preq_block_ranges)[2 * nreq_blocks]
 * @param nreq_blocks Pointer to integer that will contain the
 *          number of block ranges
 *
 * @author Jayesh Krishna
 */

int get_file_req_blocks(file_desc_t *file,
      int **preqs,
      int *nreqs,
      int *nvars_with_reqs,
      int *last_var_with_req,
      int **preq_block_ranges,
      int *nreq_blocks)
{
    int mpierr = MPI_SUCCESS;

    assert(file &&
            preqs && nreqs && last_var_with_req &&
            preq_block_ranges && nreq_blocks);
    assert(file->iotype == PIO_IOTYPE_PNETCDF);
    assert(file->iosystem && (file->iosystem->num_iotasks > 0));

    *nreqs = 0;
    *nvars_with_reqs = 0;
    *last_var_with_req = 0;
    *nreq_blocks = 0;

    int file_nreqs = 0;
    int vdesc_with_reqs_start = 0;
    int vdesc_with_reqs_end = 0;
    /* FIXME: Update file->nreqs with vdesc->nreqs to avoid computing
     * everytime
     */
    for(int i = 0; i < PIO_MAX_VARS; i++){
      var_desc_t *vdesc = file->varlist + i;
      if(vdesc->nreqs > 0){
        file_nreqs += vdesc->nreqs;
        /* Once the range starts, vdesc_with_reqs_end >= 1 */
        if(vdesc_with_reqs_end == 0){
          vdesc_with_reqs_start = i;
        }
        vdesc_with_reqs_end = i + 1;
        (*nvars_with_reqs)++;
        *last_var_with_req = i;
      }
    }

#ifdef PIO_ENABLE_SANITY_CHECKS
    /* Sanity check : All io ranks have the same number of reqs per file */
    int file_nreqs_root = file_nreqs;
    mpierr = MPI_Bcast(&file_nreqs_root, 1, MPI_INT,
              file->iosystem->ioroot, file->iosystem->io_comm);
    assert(mpierr == MPI_SUCCESS);
    assert(file_nreqs_root == file_nreqs);
#endif

    /* No requests pending on this file */
    if(file_nreqs == 0){
      *nreqs = 0;
      *nreq_blocks = 0;

      return PIO_NOERR;
    }

    /* preqs : pointer to consolidated list of pending requests on this file */
    *preqs = (int *) calloc(file_nreqs, sizeof(int));
    if(!(*preqs)){
      return pio_err(file->iosystem, file, PIO_ENOMEM, __FILE__, __LINE__,
                      "Unable to allocate memory (%llu bytes) for consolidating pending requests on a file (%s, ncid=%d, num pending requests = %d)", (unsigned long long) (file_nreqs * sizeof(int)), pio_get_fname_from_file(file), file->pio_ncid, file_nreqs);
    }
    *nreqs = file_nreqs;

    /* preq_block_ranges : Contains the ranges for the blocks,
     * each block of size <= file_req_block_sz_limit, which includes
     * start indices in the first half and end indices in the second half.
     * The number of block ranges should be less than the number of
     * pending requests (max number of blocks => 1 request per block)
     * => 2 * file_nreqs
     * This buffer is also used to store the size of block ranges at
     * index 2 * file_nreqs => 1 int (for communicating with non-root procs)
     * => size of 2 * file_nreqs + 1
     */
    *preq_block_ranges = (int *) calloc(2 * file_nreqs + 1, sizeof(int));
    if(!(*preq_block_ranges)){
      return pio_err(file->iosystem, file, PIO_ENOMEM, __FILE__, __LINE__,
                      "Unable to allocate memory (%llu bytes) for storing pending request ranges in a file (%s, ncid=%d, num pending requests = %d)", (unsigned long long) ((2 * file_nreqs + 1)* sizeof(int)), pio_get_fname_from_file(file), file->pio_ncid, file_nreqs);
    }
    int *preq_block_ranges_sz = (*preq_block_ranges) + 2 * file_nreqs;
    *nreq_blocks = 0;

    int *req_block_starts = *preq_block_ranges;
    int *req_block_ends = *preq_block_ranges + file_nreqs;

    /* One pending request on this file */
    if(file_nreqs == 1){
      int req = file->varlist[vdesc_with_reqs_start].request[0];
      (*preqs)[0] = req;
      req_block_starts[0] = 0;
      req_block_ends[0] = 0;
      *nreq_blocks = 1;

      return PIO_NOERR;
    }

    /* local file pending request sizes */
    int *file_lrequest = *preqs;
    PIO_Offset file_lrequest_sz[file_nreqs];
    PIO_Offset file_grequest_sz[file_nreqs * file->iosystem->num_iotasks];

    for(int i = vdesc_with_reqs_start, j = 0;
          (i < vdesc_with_reqs_end) && (j < file_nreqs); i++){
      var_desc_t *vdesc = file->varlist + i;
      for(int k = 0; k < vdesc->nreqs; k++, j++){
        file_lrequest[j] = vdesc->request[k];
        file_lrequest_sz[j] = vdesc->request_sz[k];
      }
    }

#ifdef FLUSH_EVERY_VAR
    /* Its easier to handle this corner case, used infrequently, separately */
    /* Each request block consists of requests pending on a single variable */
    *nreq_blocks = 0;
    for(int i = vdesc_with_reqs_start, j = 0;
        (i < vdesc_with_reqs_end) && (j < file_nreqs); i++){
      var_desc_t *vdesc = file->varlist + i;
      if(vdesc->nreqs > 0){
        req_block_starts[*nreq_blocks] = j;
        req_block_ends[*nreq_blocks] = j + vdesc->nreqs - 1;
        (*nreq_blocks)++;
        j += vdesc->nreqs;
      }
    }

    /* Copy the starts and ends to a contiguous section */
    if(file_nreqs != *nreq_blocks){
      for(int i = *nreq_blocks, j = 0;
          (i < 2 * file_nreqs) && (j < *nreq_blocks); i++, j++){
        req_block_starts[i] = req_block_ends[j];
      }
    }

    return PIO_NOERR;
#endif /* #ifdef FLUSH_EVERY_VAR */

    /* Gather file pending request sizes from all I/O processes */
    mpierr = MPI_Gather(file_lrequest_sz, file_nreqs, MPI_OFFSET,
                        file_grequest_sz, file_nreqs, MPI_OFFSET,
                        file->iosystem->ioroot, file->iosystem->io_comm);
    if(mpierr != MPI_SUCCESS){
      return check_mpi(file->iosystem, file, mpierr, __FILE__, __LINE__);
    }

    /* Find the request blocks in the root I/O process */
    bool is_ioroot =
      (file->iosystem->io_rank == file->iosystem->ioroot) ? true : false;
    if(is_ioroot){
      PIO_Offset file_cur_block_grequest_sz[file->iosystem->num_iotasks];

      /* file_grequest_sz[] = {
       * rank_0_req0_sz, rank_0_req1_sz, ..., rank_0_reqi_sz, ...
       * rank_1_req0_sz, rank_1_req1_sz, ..., rank_1_reqi_sz, ...
       * rank_j_req0_sz, rank_j_req1_sz, ..., rank_j_reqi_sz,...}
       * In the loop below we calculate block size by calculating
       * partial sums of requests, one request at a time
       */

      /* Initialize cur block grequest size with size of the first
       * request for each I/O rank
       * Note:
       *  Number of pending requests on this file, file_nreqs > 1
       */
      for(int j = 0; j < file->iosystem->num_iotasks; j++){
        PIO_Offset cur_idx = j * file_nreqs;
        file_cur_block_grequest_sz[j] = file_grequest_sz[cur_idx];
      }
      int k = 0;
      for(int i = 1; i < file_nreqs; i++){
        req_block_ends[k] = i;
        for(int j = 0; j < file->iosystem->num_iotasks; j++){
          /* Get idx for reqi on rank j */
          PIO_Offset cur_idx = i + j * file_nreqs;
          file_cur_block_grequest_sz[j] += file_grequest_sz[cur_idx];
          if(file_cur_block_grequest_sz[j] > file_req_block_sz_limit){
            PIO_Offset nreqs_in_cur_block =
                        req_block_ends[k] - req_block_starts[k] + 1;
            assert(nreqs_in_cur_block >= 1);
            if(nreqs_in_cur_block == 1){
              /* We cannot have 0 requests in a block but the size
               * of ith request is > file_req_block_sz_limit.
               * So include this ith request in a single block with
               * a warning to the user
               */
              printf("PIO: WARNING: Found a single user request (size=%lld bytes) that exceeds the maximum limit (%lld bytes) for user request %d on I/O process %d when processing pending requests on file (%s, ncid=%d, number of pending requests=%d). Waiting on this request might fail during a future wait, consider writing out data < %lld bytes from a single process\n", file_cur_block_grequest_sz[j], file_req_block_sz_limit, i, j, pio_get_fname_from_file(file), file->pio_ncid, file_nreqs, file_req_block_sz_limit);
            }
            /* Finish the prev block - indicate that the prev
             * block cannot include this request
             */
            req_block_ends[k] = i - 1;
            /* We need to start a new block */
            k++;
            req_block_starts[k] = i;
            req_block_ends[k] = i;
            /* Reset the current size of block for all iotasks */
            for(int l = 0; l < file->iosystem->num_iotasks; l++){
              /* Get idx for reqi on rank l */
              PIO_Offset idx = i + l * file_nreqs;
              file_cur_block_grequest_sz[l] = file_grequest_sz[idx];
            }
            break;
          }
        }
      }

      /* Note: We are guarantreed to have at least 1 block here */
      *nreq_blocks = ++k;

      /* Copy the starts and ends to a contiguous section */
      if(file_nreqs != *nreq_blocks){
        for(int i = *nreq_blocks, j = 0;
            (i < 2 * file_nreqs) && (j < *nreq_blocks); i++, j++){
          req_block_starts[i] = req_block_ends[j];
        }
      }
    } /* if(is_ioroot) */

    /* Bcast the request blocks
     * Note that the last int in the buffer is the number of the blocks
     */
    *preq_block_ranges_sz = *nreq_blocks;
    mpierr = MPI_Bcast(*preq_block_ranges, 2 * file_nreqs + 1, MPI_INT,
              file->iosystem->ioroot, file->iosystem->io_comm);
    if(mpierr != MPI_SUCCESS){
      return check_mpi(file->iosystem, file, mpierr, __FILE__, __LINE__);
    }
    *nreq_blocks = *preq_block_ranges_sz;
    assert(*nreq_blocks > 0);

    return PIO_NOERR;
}

/**
 * Flush the output buffer. This is only relevant for files opened
 * with pnetcdf.
 *
 * @param file a pointer to the open file descriptor for the file
 * that will be written to
 * @param force true to force the flushing of the buffer
 * @param addsize additional size to add to buffer (in bytes)
 * @return 0 for success, error code otherwise.
 * @ingroup PIO_write_darray
 * @author Jim Edwards, Jayesh Krishna, Ed Hartnett
 */
int flush_output_buffer(file_desc_t *file, bool force, PIO_Offset addsize)
{
    int mpierr = MPI_SUCCESS;  /* Return code from MPI functions. */
    int ierr = PIO_NOERR;

#ifdef TIMING
    GPTLstart("PIO:flush_output_buffer");
#endif
#ifdef _PNETCDF
    var_desc_t *vdesc;
    PIO_Offset usage = 0;

    /* Check inputs. */
    pioassert(file, "invalid input", __FILE__, __LINE__);

    /* Find out the buffer usage. */
    if ((ierr = ncmpi_inq_buffer_usage(file->fh, &usage)))
	/* allow the buffer to be undefined */
	if (ierr != NC_ENULLABUF)
        {
            return pio_err(NULL, file, PIO_EBADID, __FILE__, __LINE__,
                              "Internal error flushing data written (ensuring/waiting_for all pending data is written to disk) to file (%s, ncid=%d). Unable to query the PnetCDF library buffer usage", file->fname, file->pio_ncid);
        }

    /* If we are not forcing a flush, spread the usage to all IO
     * tasks. */
    if (!force && file->iosystem->io_comm != MPI_COMM_NULL)
    {
        usage += addsize;
        if ((mpierr = MPI_Allreduce(MPI_IN_PLACE, &usage, 1,  MPI_OFFSET,  MPI_MAX,
                                    file->iosystem->io_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    /* Keep track of the maximum usage. */
    if (usage > maxusage)
        maxusage = usage;

    /* If the user forces it, or the buffer has exceeded the size
     * limit, then flush to disk. */
    if (force || usage >= pio_buffer_size_limit)
    {
        int rcnt;
        int  maxreq; /* Index of the last vdesc with pending requests */
        int nvars_with_reqs = 0;
        maxreq = -1;
        rcnt = 0;

        int *reqs = NULL;
        int nreqs = 0;
        int *req_block_ranges = NULL;
        int nreq_blocks = 0;

        ierr = get_file_req_blocks(file, 
                &reqs, &nreqs, &nvars_with_reqs, &maxreq,
                &req_block_ranges, &nreq_blocks);
        if(ierr != PIO_NOERR)
        {
            return pio_err(file->iosystem, file, ierr, __FILE__, __LINE__,
                            "Unable to consolidate pending requests on file (%s, ncid=%d) to blocks (The function returned : Number of pending requests on file = %d, Number of variables with pending requests = %d, Number of request blocks = %d).", pio_get_fname_from_file(file), file->pio_ncid, nreqs, nvars_with_reqs, nreq_blocks); 
        }

#ifdef PIO_MICRO_TIMING
        bool var_has_pend_reqs[maxreq + 1];
        bool var_timer_was_running[maxreq + 1];
        mtimer_t tmp_mt;

        /* Temp timer to keep track of wait time */
        tmp_mt = mtimer_create("Temp_wait_timer", file->iosystem->my_comm, "piowaitlog");
        if(!mtimer_is_valid(tmp_mt))
        {
            LOG((1, "Unable to create a temp timer"));
            return pio_err(file->iosystem, file, PIO_EINTERNAL, __FILE__, __LINE__,
                              "Internal error flushing data written (ensuring/waiting_for all pending data is written to disk) to file (%s, ncid=%d). Unable to create a micro timer to measure wait/flush time", pio_get_fname_from_file(file), file->pio_ncid);
        }

        ierr = mtimer_start(tmp_mt);
        if(ierr != PIO_NOERR)
        {
            LOG((1, "Unable to start the temp wait timer"));
            return ierr;
        }

        for (int i = 0; i <= maxreq; i++)
        {
            vdesc = file->varlist + i;
            /* Pause all timers, the temp wait timer is used to keep
             * track of wait time
             */
            var_timer_was_running[i] = false;
            var_has_pend_reqs[i] = (vdesc->nreqs > 0) ? true : false;
            if(mtimer_is_valid(vdesc->wr_mtimer))
            {
                ierr = mtimer_pause(vdesc->wr_mtimer, &(var_timer_was_running[i]));
                if(ierr != PIO_NOERR)
                {
                    LOG((1, "Unable to pause the timer"));
                    return ierr;
                }
            }
        }
#endif

#ifdef MPIO_ONESIDED
        int *request = reqs;
        int status[nreqs];
        rcnt = 0;
        for (int i = 0; i <= maxreq; i++)
        {
            vdesc = file->varlist + i;
            /* Onesided optimization requires that all of the requests
             * in a wait_all call represent a contiguous block of data
             * in the file
             */
            if (rcnt > 0 && (prev_record != vdesc->record || vdesc->nreqs==0))
            {
                ierr = ncmpi_wait_all(file->fh, rcnt, request, status);
                if(ierr != PIO_NOERR)
                {
                    return pio_err(file->iosystem, file, ierr,
                                    __FILE__, __LINE__,
                                    "Waiting on pending requests on file (%s, ncid=%d) failed (Number of pending requests on file = %d, Number of variables with pending requests = %d, Number of requests currently being waited on = %d).", pio_get_fname_from_file(file), file->pio_ncid, nreqs, nvars_with_reqs, rcnt); 
                }

                request += rcnt;
                rcnt = 0;
            }
            rcnt += vdesc->nreqs;
            prev_record = vdesc->record;
        }
        if (rcnt > 0)
        {
            ierr = ncmpi_wait_all(file->fh, rcnt, request, status);
            if(ierr != PIO_NOERR)
            {
                return pio_err(file->iosystem, file, ierr,
                                __FILE__, __LINE__,
                                "Waiting on pending requests on file (%s, ncid=%d) failed (Number of pending requests on file = %d, Number of variables with pending requests = %d, Number of requests currently being waited on = %d).", pio_get_fname_from_file(file), file->pio_ncid, nreqs, nvars_with_reqs, rcnt); 
            }
        }
#else /* MPIO_ONESIDED */
        int *request = reqs;
        int status[nreqs];
        rcnt = 0;
        int *req_block_starts = req_block_ranges;
        int *req_block_ends = req_block_ranges + nreq_blocks;
        for(int k = 0; k < nreq_blocks; k++)
        {
            assert(req_block_ends[k] >= req_block_starts[k]);
            rcnt = req_block_ends[k] - req_block_starts[k] + 1;

            LOG((1, "ncmpi_wait_all(file=%s, ncid=%d, request range = [%d, %d], num pending requests = %d)", pio_get_fname_from_file(file), file->pio_ncid, req_block_starts[k], req_block_ends[k], nreqs));
            ierr = ncmpi_wait_all(file->fh, rcnt, request, status);
            if(ierr != PIO_NOERR)
            {
                return pio_err(file->iosystem, file, ierr, __FILE__, __LINE__,
                                "Waiting on pending requests on file (%s, ncid=%d) failed (Number of pending requests on file = %d, Number of variables with pending requests = %d, Number of request blocks = %d, Current block being waited on = %d, Number of requests in current block = %d).", pio_get_fname_from_file(file), file->pio_ncid, nreqs, nvars_with_reqs, nreq_blocks, k, rcnt); 
            }
            request += rcnt;
        }
#endif /* MPIO_ONESIDED */
        free(reqs);
        free(req_block_ranges);

#ifdef PIO_MICRO_TIMING
        ierr = mtimer_pause(tmp_mt, NULL);
        if(ierr != PIO_NOERR)
        {
            LOG((1, "Unable to pause temp wait timer"));
            return ierr;
        }

        /* Get the total wait time */
        double wait_time = 0;
        ierr = mtimer_get_wtime(tmp_mt, &wait_time);
        if(ierr != PIO_NOERR)
        {
            LOG((1, "Error trying to get wallclock time (temp wait timer)"));
            return ierr;
        }

        ierr = mtimer_destroy(&tmp_mt);
        if(ierr != PIO_NOERR)
        {
            LOG((1, "Destroying temp wait timer failed"));
            /* Continue */
        }

        /* Find avg wait time per variable */
        wait_time /= (nvars_with_reqs > 0) ? nvars_with_reqs : 1;

        /* Update timers for vars with pending ops (with the avg
         * wait time)
         */
        for (int i = 0; i <= maxreq; i++)
        {
            vdesc = file->varlist + i;
            if(var_has_pend_reqs[i] && mtimer_is_valid(vdesc->wr_mtimer))
            {
                ierr = mtimer_update(vdesc->wr_mtimer, wait_time);
                if(ierr != PIO_NOERR)
                {
                    LOG((1, "Unable to update variable write timer"));
                    return ierr;
                }

                /* Wait complete - no more async events in progress */
                ierr = mtimer_async_event_in_progress(vdesc->wr_mtimer, false);
                if(ierr != PIO_NOERR)
                {
                    LOG((1, "Unable to disable async events for var"));
                    return ierr;
                }
                /* If timer was already running, restart it or else flush it */
                if(var_timer_was_running[i])
                {
                    ierr = mtimer_resume(vdesc->wr_mtimer);
                    if(ierr != PIO_NOERR)
                    {
                        LOG((1, "Unable to resume variable write timer"));
                        return ierr;
                    }
                }
                else
                {
                    ierr = mtimer_flush(vdesc->wr_mtimer,
                            get_var_desc_str(file->pio_ncid, vdesc->varid, NULL));
                    if(ierr != PIO_NOERR)
                    {
                        LOG((1, "Unable to flush timer"));
                        return ierr;
                    }
                }
            }
        }
#endif

        /* Release resources. */
        for (int i = 0; i < PIO_IODESC_MAX_IDS; i++)
        {
            if (file->iobuf[i])
            {
                LOG((3,"freeing variable buffer in flush_output_buffer"));
                brel(file->iobuf[i]);
                file->iobuf[i] = NULL;
            }
        }
        for (int i = 0; i < PIO_MAX_VARS; i++)
        {
            vdesc = file->varlist + i;
            vdesc->wb_pend = 0;
            if (vdesc->nreqs > 0)
            {
                assert(vdesc->request && vdesc->request_sz);
                free(vdesc->request);
                free(vdesc->request_sz);

                vdesc->request = NULL;
                vdesc->request_sz = NULL;
                vdesc->nreqs = 0;
            }
  
            if (vdesc->fillbuf)
            {
                brel(vdesc->fillbuf);
                vdesc->fillbuf = NULL;
            }
        }
        file->wb_pend = 0;
    }

#endif /* _PNETCDF */
#ifdef TIMING
    GPTLstop("PIO:flush_output_buffer");
#endif
    return ierr;
}

/**
 * Print out info about the buffer for debug purposes. This should
 * only be called when logging is enabled.
 *
 * @param ios pointer to the IO system structure
 * @param collective true if collective report is desired
 * @ingroup PIO_write_darray
 * @author Jim Edwards
 */
void cn_buffer_report(iosystem_desc_t *ios, bool collective)
{
    int mpierr = MPI_SUCCESS;  /* Return code from MPI functions. */

    LOG((2, "cn_buffer_report ios->iossysid = %d collective = %d",
         ios->iosysid, collective));
    long bget_stats[5];
    long bget_mins[5];
    long bget_maxs[5];

    bstats(bget_stats, bget_stats+1,bget_stats+2,bget_stats+3,bget_stats+4);
    if (collective)
    {
        LOG((3, "cn_buffer_report calling MPI_Reduce ios->comp_comm = %d", ios->comp_comm));
        if ((mpierr = MPI_Reduce(bget_stats, bget_maxs, 5, MPI_LONG, MPI_MAX, 0, ios->comp_comm)))
            check_mpi(NULL, NULL, mpierr, __FILE__, __LINE__);
        LOG((3, "cn_buffer_report calling MPI_Reduce"));
        if ((mpierr = MPI_Reduce(bget_stats, bget_mins, 5, MPI_LONG, MPI_MIN, 0, ios->comp_comm)))
            check_mpi(NULL, NULL, mpierr, __FILE__, __LINE__);
        if (ios->compmaster == MPI_ROOT)
        {
            LOG((1, "Currently allocated buffer space %ld %ld", bget_mins[0], bget_maxs[0]));
            LOG((1, "Currently available buffer space %ld %ld", bget_mins[1], bget_maxs[1]));
            LOG((1, "Current largest free block %ld %ld", bget_mins[2], bget_maxs[2]));
            LOG((1, "Number of successful bget calls %ld %ld", bget_mins[3], bget_maxs[3]));
            LOG((1, "Number of successful brel calls  %ld %ld", bget_mins[4], bget_maxs[4]));
        }
    }
    else
    {
        LOG((1, "Currently allocated buffer space %ld", bget_stats[0]));
        LOG((1, "Currently available buffer space %ld", bget_stats[1]));
        LOG((1, "Current largest free block %ld", bget_stats[2]));
        LOG((1, "Number of successful bget calls %ld", bget_stats[3]));
        LOG((1, "Number of successful brel calls  %ld", bget_stats[4]));
    }
}

/**
 * Flush the buffer.
 *
 * @param ncid identifies the netCDF file.
 * @param wmb pointer to the wmulti_buffer structure.
 * @param flushtodisk if true, then flush data to disk.
 * @returns 0 for success, error code otherwise.
 * @ingroup PIO_write_darray
 * @author Jim Edwards, Ed Hartnett
 */
int flush_buffer(int ncid, wmulti_buffer *wmb, bool flushtodisk)
{
    file_desc_t *file;
    int ret;

#ifdef TIMING
    GPTLstart("PIO:flush_buffer");
#endif
    /* Check input. */
    pioassert(wmb, "invalid input", __FILE__, __LINE__);

    /* Get the file info (to get error handler). */
    if ((ret = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ret, __FILE__, __LINE__,
                        "Internal error flushing data cached in a write multi buffer to %s. Invalid file id (ncid=%d) provided", (flushtodisk) ? "disk" : "I/O processes", ncid);
    }

    LOG((1, "flush_buffer ncid = %d flushtodisk = %d", ncid, flushtodisk));

    /* If there are any variables in this buffer... */
    if (wmb->num_arrays > 0)
    {
        /* Write any data in the buffer. */
        ret = PIOc_write_darray_multi(ncid, wmb->vid,  wmb->ioid, wmb->num_arrays,
                                      wmb->arraylen, wmb->data, wmb->frame,
                                      wmb->fillvalue, flushtodisk);
        LOG((2, "return from PIOc_write_darray_multi ret = %d", ret));

        wmb->num_arrays = 0;

        /* Release the list of variable IDs. */
        free(wmb->vid);
        wmb->vid = NULL;

        /* Release the data memory. */
        brel(wmb->data);
        wmb->data = NULL;

        /* If there is a fill value, release it. */
        if (wmb->fillvalue)
            brel(wmb->fillvalue);
        wmb->fillvalue = NULL;

        /* Release the record number. */
        if (wmb->frame)
            free(wmb->frame);
        wmb->frame = NULL;

        if (ret)
        {
            return pio_err(NULL, file, ret, __FILE__, __LINE__,
                        "Internal error flushing data cached in a write multi buffer to file (%s, ncid=%d). Error while flushing data to %s. Internal error flushing arrays (%d) in the write multi buffer", pio_get_fname_from_file(file), file->pio_ncid, (flushtodisk) ? "disk" : "I/O processes", wmb->num_arrays);
        }
    }

#ifdef TIMING
    GPTLstop("PIO:flush_buffer");
#endif
    return PIO_NOERR;
}

/**
 * Compute the maximum aggregate number of bytes. This is called by
 * subset_rearrange_create() and box_rearrange_create().
 *
 * @param ios pointer to the IO system structure.
 * @param iodesc a pointer to decomposition description.
 * @returns 0 for success, error code otherwise.
 * @author Jim Edwards
 */
int compute_maxaggregate_bytes(iosystem_desc_t *ios, io_desc_t *iodesc)
{
    int maxbytesoniotask = INT_MAX;
    int maxbytesoncomputetask = INT_MAX;
    int maxbytes;
    int mpierr;  /* Return code from MPI functions. */

    /* Check inputs. */
    pioassert(iodesc, "invalid input", __FILE__, __LINE__);

    LOG((2, "compute_maxaggregate_bytes iodesc->maxiobuflen = %d iodesc->ndof = %d",
         iodesc->maxiobuflen, iodesc->ndof));

    /* Determine the max bytes that can be held on IO task. */
    if (ios->ioproc && iodesc->maxiobuflen > 0)
        maxbytesoniotask = pio_buffer_size_limit / iodesc->maxiobuflen;

    /* Determine the max bytes that can be held on computation task. */
    if (ios->comp_rank >= 0 && iodesc->ndof > 0)
        maxbytesoncomputetask = pio_cnbuffer_limit / iodesc->ndof;

    /* Take the min of the max IO and max comp bytes. */
    maxbytes = min(maxbytesoniotask, maxbytesoncomputetask);
    LOG((2, "compute_maxaggregate_bytes maxbytesoniotask = %d maxbytesoncomputetask = %d",
         maxbytesoniotask, maxbytesoncomputetask));

    /* Get the min value of this on all tasks. */
    LOG((3, "before allreaduce maxbytes = %d", maxbytes));
    if ((mpierr = MPI_Allreduce(MPI_IN_PLACE, &maxbytes, 1, MPI_INT, MPI_MIN,
                                ios->union_comm)))
        return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
    LOG((3, "after allreaduce maxbytes = %d", maxbytes));

    /* Remember the result. */
    iodesc->maxbytes = maxbytes;

    return PIO_NOERR;
}
