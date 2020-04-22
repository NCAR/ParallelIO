/** @file
 *
 * Public functions that read and write distributed arrays in PIO.
 *
 * When arrays are distributed, each processor holds some of the
 * array. Only by combining the distributed arrays from all processor
 * can the full array be obtained.
 *
 * @author Jim Edwards
 */
#include <pio_config.h>
#include <pio.h>
#include <pio_internal.h>
#ifdef PIO_MICRO_TIMING
#include "pio_timer.h"
#endif
#include "pio_sdecomps_regex.h"

/* uint64_t definition */
#ifdef _ADIOS2
#include <stdint.h>
#endif

/* 10MB default limit. */
PIO_Offset pio_buffer_size_limit = 10485760;

/* Maximum buffer usage. */
PIO_Offset maxusage = 0;

/* For write_darray_multi_serial() and write_darray_multi_par() to
 * indicate whether fill or data are being written. */
#define DARRAY_FILL 1
#define DARRAY_DATA 0

/**
 * Set the PIO IO node data buffer size limit.
 *
 * The pio_buffer_size_limit will only apply to files opened after
 * the setting is changed.
 *
 * @param limit the size of the buffer on the IO nodes
 * @return The previous limit setting.
 */
PIO_Offset PIOc_set_buffer_size_limit(PIO_Offset limit)
{
    PIO_Offset oldsize = pio_buffer_size_limit;

    /* If the user passed a valid size, use it. */
    if (limit > 0)
        pio_buffer_size_limit = limit;

    return oldsize;
}

/**
 * Write one or more arrays with the same IO decomposition to the
 * file.
 *
 * This funciton is similar to PIOc_write_darray(), but allows the
 * caller to use their own data buffering (instead of using the
 * buffering implemented in PIOc_write_darray()).
 *
 * When the user calls PIOc_write_darray() one or more times, then
 * PIO_write_darray_multi() will be called when the buffer is flushed.
 *
 * Internally, this function will:
 * <ul>
 * <li>Find info about file, decomposition, and variable.
 * <li>Do a special flush for pnetcdf if needed.
 * <li>Allocates a buffer big enough to hold all the data in the
 * multi-buffer, for all tasks.
 * <li>Calls rearrange_comp2io() to move data from compute to IO
 * tasks.
 * <li>For parallel iotypes (pnetcdf and netCDF-4 parallel) call
 * pio_write_darray_multi_nc().
 * <li>For serial iotypes (netcdf classic and netCDF-4 serial) call
 * write_darray_multi_serial().
 * <li>For subset rearranger, create holegrid to write missing
 * data. Then call pio_write_darray_multi_nc() or
 * write_darray_multi_serial() to write the holegrid.
 * <li>Special buffer flush for pnetcdf.
 * </ul>
 *
 * @param ncid identifies the netCDF file.
 * @param varids an array of length nvars containing the variable ids to
 * be written.
 * @param ioid the I/O description ID as passed back by
 * PIOc_InitDecomp().
 * @param nvars the number of variables to be written with this
 * call.
 * @param arraylen the length of the array to be written. This is the
 * length of the distrubited array. That is, the length of the portion
 * of the data that is on the processor. The same arraylen is used for
 * all variables in the call.
 * @param array pointer to the data to be written. This is a pointer
 * to an array of arrays with the distributed portion of the array
 * that is on this processor. There are nvars arrays of data, and each
 * array of data contains one record worth of data for that variable.
 * @param frame an array of length nvars with the frame or record
 * dimension for each of the nvars variables in IOBUF. NULL if this
 * iodesc contains non-record vars.
 * @param fillvalue pointer an array (of length nvars) of pointers to
 * the fill value to be used for missing data.
 * @param flushtodisk non-zero to cause buffers to be flushed to disk.
 * @return 0 for success, error code otherwise.
 * @ingroup PIO_write_darray
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_write_darray_multi(int ncid, const int *varids, int ioid, int nvars,
                            PIO_Offset arraylen, void *array, const int *frame,
                            void **fillvalue, bool flushtodisk)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    io_desc_t *iodesc;     /* Pointer to IO description information. */
    size_t rlen;           /* Total data buffer size. */
    var_desc_t *vdesc0;    /* Array of var_desc structure for each var. */
    int fndims = 0;        /* Number of dims in the var in the file. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function calls. */
    int ierr = PIO_NOERR;              /* Return code. */

#ifdef TIMING
    GPTLstart("PIO:PIOc_write_darray_multi");
#endif
    /* Get the file info. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, PIO_EBADID, __FILE__, __LINE__,
                        "Writing multiple variables to file (ncid=%d) failed. Unable to query the internal file structure associated with the file. Invalid file id", ncid);
    }
    ios = file->iosystem;

    /* Check inputs. */
    if (nvars <= 0 || !varids)
    {
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Writing multiple variables to file (%s, ncid=%d) failed. Internal error, invalid arguments, nvars = %d (expected > 0), varids is %s (expected not NULL)", pio_get_fname_from_file(file), ncid, nvars, PIO_IS_NULL(varids));
    }
    for (int v = 0; v < nvars; v++)
        if (varids[v] < 0 || varids[v] > PIO_MAX_VARS)
        {
            return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Internal error, invalid arguments, nvars = %d, varids[%d] = %d (expected >= 0 && <= PIO_MAX_VARS=%d)", pio_get_fname_from_file(file), ncid, nvars, v, varids[v], PIO_MAX_VARS);
        }

    LOG((1, "PIOc_write_darray_multi ncid = %d ioid = %d nvars = %d arraylen = %ld "
         "flushtodisk = %d",
         ncid, ioid, nvars, arraylen, flushtodisk));

    /* Check that we can write to this file. */
    if (!(file->mode & PIO_WRITE))
    {
        return pio_err(ios, file, PIO_EPERM, __FILE__, __LINE__,
                        "Writing multiple variables to file (%s, ncid=%d) failed. Trying to write to a read only file, try reopening the file in write mode (use the PIO_WRITE flag)", pio_get_fname_from_file(file), ncid);
    }

    /* Get iodesc. */
    if (!(iodesc = pio_get_iodesc_from_id(ioid)))
    {
        return pio_err(ios, file, PIO_EBADID, __FILE__, __LINE__,
                        "Writing multiple variables to file (%s, ncid=%d) failed. Invalid arguments, invalid PIO decomposition id (%d) provided", pio_get_fname_from_file(file), ncid, ioid);
    }
    pioassert(iodesc->rearranger == PIO_REARR_BOX || iodesc->rearranger == PIO_REARR_SUBSET,
              "unknown rearranger", __FILE__, __LINE__);

    /* Get a pointer to the variable info for the first variable. */
    vdesc0 = &file->varlist[varids[0]];

    /* Run these on all tasks if async is not in use, but only on
     * non-IO tasks if async is in use. */
    if (!ios->async || !ios->ioproc)
    {
        /* Get the number of dims for this var. */
        LOG((3, "about to call PIOc_inq_varndims varids[0] = %d", varids[0]));
        ierr = PIOc_inq_varndims(file->pio_ncid, varids[0], &fndims);
        if(ierr != PIO_NOERR){
          return pio_err(ios, file, ierr, __FILE__, __LINE__,
                          "Writing multiple variables to file (%s, ncid=%d) failed. Inquiring number of dimensions in the first variable (%s, varid=%d) in the list failed", pio_get_fname_from_file(file), ncid, pio_get_vname_from_file(file, varids[0]), varids[0]);
        }
        LOG((3, "called PIOc_inq_varndims varids[0] = %d fndims = %d", varids[0], fndims));
    }

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_WRITEDARRAYMULTI;
        char frame_present = frame ? true : false;         /* Is frame non-NULL? */
        char fillvalue_present = fillvalue ? true : false; /* Is fillvalue non-NULL? */
        int flushtodisk_int = flushtodisk; /* Need this to be int not boolean. */

        int *amsg_frame = NULL;
        void *amsg_fillvalue = fillvalue;

        if(!frame_present)
        {
            amsg_frame = (int *)calloc(nvars, sizeof(int));
        }
        if(!fillvalue_present)
        {
            amsg_fillvalue = calloc(nvars * iodesc->piotype_size, sizeof(char ));
        }

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr,
            ncid, nvars, nvars, varids, ioid, arraylen,
            arraylen * iodesc->piotype_size, array, frame_present,
            nvars,
            (frame_present) ? frame : amsg_frame, fillvalue_present,
            nvars * iodesc->piotype_size, amsg_fillvalue, flushtodisk_int);

        if(!frame_present)
        {
            free(amsg_frame);
        }
        if(!fillvalue_present)
        {
            free(amsg_fillvalue);
        }

        /* Share results known only on computation tasks with IO tasks. */
        if ((mpierr = MPI_Bcast(&fndims, 1, MPI_INT, ios->comproot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
        LOG((3, "shared fndims = %d", fndims));
    }

    /* if the buffer is already in use in pnetcdf we need to flush first */
    if (file->iotype == PIO_IOTYPE_PNETCDF && file->iobuf[ioid - PIO_IODESC_START_ID])
    {
        ierr = flush_output_buffer(file, true, 0);
        if (ierr != PIO_NOERR)
        {
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Flushing data to disk (PIO_IOTYPE_PNETCDF) failed", pio_get_fname_from_file(file), ncid);
        }
    }

    pioassert(!file->iobuf[ioid - PIO_IODESC_START_ID], "buffer overwrite",__FILE__, __LINE__);

    /* Determine total size of aggregated data (all vars/records).
     * For netcdf serial writes we collect the data on io nodes and
     * then move that data one node at a time to the io master node
     * and write (or read). The buffer size on io task 0 must be as
     * large as the largest used to accommodate this serial io
     * method.  */
    rlen = iodesc->maxiobuflen * nvars;

#ifdef PIO_MICRO_TIMING
    bool var_mtimer_was_running[nvars];
    /* Use the timer on the first variable to capture the total
      *time to rearrange data for all variables
      */
    ierr = mtimer_start(file->varlist[varids[0]].wr_rearr_mtimer);
    if(ierr != PIO_NOERR)
    {
        return pio_err(ios, file, ierr, __FILE__, __LINE__,
                        "Writing multiple variables to file (%s, ncid=%d) failed. Starting a micro timer failed", pio_get_fname_from_file(file), ncid);
    }
    /* Stop any write timers that are running, these timers will
      *be updated later with the avg rearrange time 
      * (wr_rearr_mtimer)
      */
    for(int i=0; i<nvars; i++)
    {
        var_mtimer_was_running[i] = false;
        assert(mtimer_is_valid(file->varlist[varids[i]].wr_mtimer));
        ierr = mtimer_pause(file->varlist[varids[i]].wr_mtimer,
                &(var_mtimer_was_running[i]));
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Pausing a micro timer failed", pio_get_fname_from_file(file), ncid);
        }
    }
#endif

    /* Allocate iobuf. */
    if (rlen > 0)
    {
        /* Allocate memory for the buffer for all vars/records. */
        if (!(file->iobuf[ioid - PIO_IODESC_START_ID] = bget(iodesc->mpitype_size * rlen)))
        {
            return pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Out of memory (Trying to allocate %lld bytes for rearranged data for multiple variables with the same decomposition)", pio_get_fname_from_file(file), ncid, (unsigned long long)(iodesc->mpitype_size * rlen));
        }
        LOG((3, "allocated %lld bytes for variable buffer", rlen * iodesc->mpitype_size));

        /* If fill values are desired, and we're using the BOX
         * rearranger, insert fill values. */
        if (iodesc->needsfill && iodesc->rearranger == PIO_REARR_BOX)
        {
            LOG((3, "inerting fill values iodesc->maxiobuflen = %d", iodesc->maxiobuflen));
            for (int nv = 0; nv < nvars; nv++)
                for (PIO_Offset i = 0; i < iodesc->maxiobuflen; i++)
                    memcpy(&((char *)file->iobuf[ioid - PIO_IODESC_START_ID])[iodesc->mpitype_size * (i + nv * iodesc->maxiobuflen)],
                           &((char *)fillvalue)[nv * iodesc->mpitype_size], iodesc->mpitype_size);
        }
    }
    else if (file->iotype == PIO_IOTYPE_PNETCDF && ios->ioproc)
    {
	/* this assures that iobuf is allocated on all iotasks thus
	 assuring that the flush_output_buffer call above is called
	 collectively (from all iotasks) */
        if (!(file->iobuf[ioid - PIO_IODESC_START_ID] = bget(1)))
        {
            return pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Out of memory (Trying to allocate 1 byte)", pio_get_fname_from_file(file), ncid);
        }
        LOG((3, "allocated token for variable buffer"));
    }

    /* Move data from compute to IO tasks. */
    if ((ierr = rearrange_comp2io(ios, iodesc, array, file->iobuf[ioid - PIO_IODESC_START_ID], nvars)))
    {
        return pio_err(ios, file, ierr, __FILE__, __LINE__,
                        "Writing multiple variables to file (%s, ncid=%d) failed. Error rearranging and moving data from compute tasks to I/O tasks", pio_get_fname_from_file(file), ncid);
    }

#ifdef PIO_MICRO_TIMING
    double rearr_time = 0;
    /* Use the timer on the first variable to capture the total
      *time to rearrange data for all variables
      */
    ierr = mtimer_pause(file->varlist[varids[0]].wr_rearr_mtimer, NULL);
    if(ierr != PIO_NOERR)
    {
        return pio_err(ios, file, ierr, __FILE__, __LINE__,
                        "Writing multiple variables to file (%s, ncid=%d) failed. Pausing a micro timer (to measure rearrange time) failed", pio_get_fname_from_file(file), ncid);
    }

    ierr = mtimer_get_wtime(file->varlist[varids[0]].wr_rearr_mtimer,
            &rearr_time);
    if(ierr != PIO_NOERR)
    {
        return pio_err(ios, file, ierr, __FILE__, __LINE__,
                        "Writing multiple variables to file (%s, ncid=%d) failed. Retrieving wallclock time from a micro timer (rearrange time) failed", pio_get_fname_from_file(file), ncid);
    }

    /* Calculate the average rearrange time for a variable */
    rearr_time /= nvars;
    for(int i=0; i<nvars; i++)
    {
        /* Reset, update and flush each timer */
        ierr = mtimer_reset(file->varlist[varids[i]].wr_rearr_mtimer);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Resetting micro timer (to measure rearrange time) for variable %d failed", pio_get_fname_from_file(file), ncid, i);
        }

        /* Update the rearrange timer with avg rearrange time for a var */
        ierr = mtimer_update(file->varlist[varids[i]].wr_rearr_mtimer,
                rearr_time);
        if(ierr != PIO_NOERR)
        {
            LOG((1, "ERROR: Unable to update wr rearr timer"));
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Updating micro timer (to measure rearrange time) for variable %d failed", pio_get_fname_from_file(file), ncid, i);
        }
        ierr = mtimer_flush(file->varlist[varids[i]].wr_rearr_mtimer,
                get_var_desc_str(file->pio_ncid, varids[i], NULL));
        if(ierr != PIO_NOERR)
        {
            LOG((1, "ERROR: Unable to flush wr rearr timer"));
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Flushing micro timer (to measure rearrange time) for variable %d failed", pio_get_fname_from_file(file), ncid, i);
        }
        /* Update the write timer with avg rearrange time for a var
         * i.e, the write timer includes the rearrange time
         */
        ierr = mtimer_update(file->varlist[varids[i]].wr_mtimer,
                rearr_time);
        if(ierr != PIO_NOERR)
        {
            LOG((1, "ERROR: Unable to update wr timer"));
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Updating micro timer (to measure write time) for variable %d failed", pio_get_fname_from_file(file), ncid, i);
        }

        /* If the write timer was already running, resume it */
        if(var_mtimer_was_running[i])
        {
            ierr = mtimer_resume(file->varlist[varids[i]].wr_mtimer);
            if(ierr != PIO_NOERR)
            {
                LOG((1, "ERROR: Unable to resume wr timer"));
                return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Updating micro timer (to measure write time) for variable %d failed", pio_get_fname_from_file(file), ncid, i);
            }
        }
    }
#endif
    /* Write the darray based on the iotype. */
    LOG((2, "about to write darray for iotype = %d", file->iotype));
    switch (file->iotype)
    {
    case PIO_IOTYPE_NETCDF4P:
    case PIO_IOTYPE_PNETCDF:
        if ((ierr = write_darray_multi_par(file, nvars, fndims, varids, iodesc,
                                           DARRAY_DATA, frame)))
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Internal error writing variable data in parallel (iotype = %s)", pio_get_fname_from_file(file), ncid, pio_iotype_to_string(file->iotype));
        break;
    case PIO_IOTYPE_NETCDF4C:
    case PIO_IOTYPE_NETCDF:
        if ((ierr = write_darray_multi_serial(file, nvars, fndims, varids, iodesc,
                                              DARRAY_DATA, frame)))
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Internal error writing variable data serially (iotype = %s)", pio_get_fname_from_file(file), ncid, pio_iotype_to_string(file->iotype));

        break;
    default:
        return pio_err(NULL, NULL, PIO_EBADIOTYPE, __FILE__, __LINE__,
                        "Writing multiple variables to file (%s, ncid=%d) failed. Invalid iotype (%d) provided", pio_get_fname_from_file(file), ncid, file->iotype);
    }

    /* For PNETCDF the iobuf is freed in flush_output_buffer() */
    if (file->iotype != PIO_IOTYPE_PNETCDF)
    {
        /* Release resources. */
        if (file->iobuf[ioid - PIO_IODESC_START_ID])
        {
	    LOG((3,"freeing variable buffer in pio_darray"));
            brel(file->iobuf[ioid - PIO_IODESC_START_ID]);
            file->iobuf[ioid - PIO_IODESC_START_ID] = NULL;
        }
    }

    /* The box rearranger will always have data (it could be fill
     * data) to fill the entire array - that is the aggregate start
     * and count values will completely describe one unlimited
     * dimension unit of the array. For the subset method this is not
     * necessarily the case, areas of missing data may never be
     * written. In order to make sure that these areas are given the
     * missing value a 'holegrid' is used to describe the missing
     * points. This is generally faster than the netcdf method of
     * filling the entire array with missing values before overwriting
     * those values later. */
    if (iodesc->rearranger == PIO_REARR_SUBSET && iodesc->needsfill)
    {
        LOG((2, "nvars = %d holegridsize = %ld iodesc->needsfill = %d\n", nvars,
             iodesc->holegridsize, iodesc->needsfill));

	pioassert(!vdesc0->fillbuf, "buffer overwrite",__FILE__, __LINE__);

        /* Get a buffer. */
	if (ios->io_rank == 0)
	    vdesc0->fillbuf = bget(iodesc->maxholegridsize * iodesc->mpitype_size * nvars);
	else if (iodesc->holegridsize > 0)
	    vdesc0->fillbuf = bget(iodesc->holegridsize * iodesc->mpitype_size * nvars);

        /* copying the fill value into the data buffer for the box
         * rearranger. This will be overwritten with data where
         * provided. */
        for (int nv = 0; nv < nvars; nv++)
            for (int i = 0; i < iodesc->holegridsize; i++)
                memcpy(&((char *)vdesc0->fillbuf)[iodesc->mpitype_size * (i + nv * iodesc->holegridsize)],
                       &((char *)fillvalue)[iodesc->mpitype_size * nv], iodesc->mpitype_size);

        /* Write the darray based on the iotype. */
        switch (file->iotype)
        {
        case PIO_IOTYPE_PNETCDF:
        case PIO_IOTYPE_NETCDF4P:
            if ((ierr = write_darray_multi_par(file, nvars, fndims, varids, iodesc,
                                               DARRAY_FILL, frame)))
                return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Internal error writing variable fillvalues in parallel (iotype = %s)", pio_get_fname_from_file(file), ncid, pio_iotype_to_string(file->iotype));
            break;
        case PIO_IOTYPE_NETCDF4C:
        case PIO_IOTYPE_NETCDF:
            if ((ierr = write_darray_multi_serial(file, nvars, fndims, varids, iodesc,
                                                  DARRAY_FILL, frame)))
                return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Internal error writing variable fillvalues serially (iotype = %s)", pio_get_fname_from_file(file), ncid, pio_iotype_to_string(file->iotype));
            break;
        default:
            return pio_err(ios, file, PIO_EBADIOTYPE, __FILE__, __LINE__,
                        "Writing fillvalues for multiple variables to file (%s, ncid=%d) failed. Unsupported iotype (%s) provided", pio_get_fname_from_file(file), ncid, pio_iotype_to_string(file->iotype));
        }

        /* For PNETCDF fillbuf is freed in flush_output_buffer() */
        if (file->iotype != PIO_IOTYPE_PNETCDF)
        {
            /* Free resources. */
            if (vdesc0->fillbuf)
            {
                brel(vdesc0->fillbuf);
                vdesc0->fillbuf = NULL;
            }
        }
    }

    /* Only PNETCDF does non-blocking buffered writes, and hence
     * needs an explicit flush/wait to make sure data is written
     * to disk (if the buffer is full)
     */
    if (ios->ioproc && file->iotype == PIO_IOTYPE_PNETCDF)
    {
        /* Flush data to disk for pnetcdf. */
        if ((ierr = flush_output_buffer(file, flushtodisk, 0)))
        {
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Writing multiple variables to file (%s, ncid=%d) failed. Flushing data to disk (PIO_IOTYPE_PNETCDF) failed", pio_get_fname_from_file(file), ncid);
        }
    }
    else
    {
        for(int i=0; i<nvars; i++)
        {
            file->varlist[varids[i]].wb_pend = 0;
#ifdef PIO_MICRO_TIMING
            /* No more async events pending (all buffered data is written out) */
            mtimer_async_event_in_progress(file->varlist[varids[i]].wr_mtimer, false);
            mtimer_flush(file->varlist[varids[i]].wr_mtimer, get_var_desc_str(file->pio_ncid, varids[i], NULL));
#endif
        }
        file->wb_pend = 0;
    }

#ifdef TIMING
    GPTLstop("PIO:PIOc_write_darray_multi");
#endif
    return PIO_NOERR;
}

/**
 * Find the fillvalue that should be used for a variable.
 *
 * @param file Info about file we are writing to.
 * @param varid the variable ID.
 * @param vdesc pointer to var_desc_t info for this var.
 * @returns 0 for success, non-zero error code for failure.
 * @ingroup PIO_write_darray
 * @author Ed Hartnett
*/
int find_var_fillvalue(file_desc_t *file, int varid, var_desc_t *vdesc)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    int no_fill;
    int ierr = PIO_NOERR;

    /* Check inputs. */
    pioassert(file && file->iosystem && vdesc, "invalid input", __FILE__, __LINE__);
    ios = file->iosystem;

    LOG((3, "find_var_fillvalue file->pio_ncid = %d varid = %d", file->pio_ncid, varid));

    /* Find out PIO data type of var. */
    if ((ierr = PIOc_inq_vartype(file->pio_ncid, varid, &vdesc->pio_type)))
    {
        return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                        "Finding fillvalue for variable (%s, varid=%d) in file (%s, ncid=%d), failed. Inquiring variable data type failed", vdesc->vname, varid, file->fname, file->pio_ncid); 
    }

    /* Find out length of type. */
    if ((ierr = PIOc_inq_type(file->pio_ncid, vdesc->pio_type, NULL, &vdesc->type_size)))
    {
        return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                        "Finding fillvalue for variable (%s, varid=%d) in file (%s, ncid=%d), failed. Inquiring variable data type length failed", vdesc->vname, varid, file->fname, file->pio_ncid); 
    }
    LOG((3, "getting fill value for varid = %d pio_type = %d type_size = %d",
         varid, vdesc->pio_type, vdesc->type_size));

    /* Allocate storage for the fill value. */
    if (!(vdesc->fillvalue = malloc(vdesc->type_size)))
    {
        return pio_err(ios, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                        "Finding fillvalue for variable (%s, varid=%d) in file (%s, ncid=%d), failed. Out of memory allocating %lld bytes for fill value", vdesc->vname, varid, file->fname, file->pio_ncid, (unsigned long long) (vdesc->type_size)); 
    }

    /* Get the fill value. */
    if ((ierr = PIOc_inq_var_fill(file->pio_ncid, varid, &no_fill, vdesc->fillvalue)))
    {
        return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                        "Finding fillvalue for variable (%s, varid=%d) in file (%s, ncid=%d), failed. Inquiring variable fillvalue failed", vdesc->vname, varid, file->fname, file->pio_ncid); 
    }
    vdesc->use_fill = no_fill ? 0 : 1;
    LOG((3, "vdesc->use_fill = %d", vdesc->use_fill));

    return PIO_NOERR;
}

/* Check if the write multi buffer requires a flush
 * wmb : A write multi buffer that might already contain data
 * arraylen : The length of the new array that needs to be cached in this wmb
 *            (The array is not cached yet)
 * iodesc : io descriptor for the data cached in the write multi buffer
 * A disk flush implies that data needs to be rearranged and write needs to be
 * completed. Rearranging and writing data frees up cache is compute and I/O
 * processes
 * An I/O flush implies that data needs to be rearranged and write needs to be
 * started (for iotypes other than PnetCDF write also completes). This would
 * free up cache in compute processes (I/O processes still need to cache the
 * rearranged data until the write completes)
 * Returns 2 if a disk flush is required, 1 if an I/O flush is required, 0 otherwise
 */
static int PIO_wmb_needs_flush(wmulti_buffer *wmb, int arraylen, io_desc_t *iodesc)
{
    bufsize curalloc, totfree, maxfree;
    long nget, nrel;
    const int NEEDS_DISK_FLUSH=2, NEEDS_IO_FLUSH=1, NO_FLUSH=0;

    assert(wmb && iodesc);
    /* Find out how much free, contiguous space is available. */
    bstats(&curalloc, &totfree, &maxfree, &nget, &nrel);

    LOG((2, "maxfree = %ld wmb->num_arrays = %d (1 + wmb->num_arrays) *"
         " arraylen * iodesc->mpitype_size = %ld totfree = %ld\n",
          maxfree, wmb->num_arrays,
         (1 + wmb->num_arrays) * arraylen * iodesc->mpitype_size, totfree));

    /* We have exceeded the set buffer write cache limit, write data to
     * disk
     */
    if(curalloc >= pio_buffer_size_limit)
    {
        return NEEDS_DISK_FLUSH;
    }

    PIO_Offset array_sz_bytes = arraylen * iodesc->mpitype_size;
    /* Total cache size required to cache this array
     * - including existing data cached in wmb
     * Note that all the arrays are cached in an wmb in a single
     * contiguous block of memory.
     */
    PIO_Offset wmb_req_cache_sz = (1 + wmb->num_arrays) * array_sz_bytes;
    /* maxfree is the maximum amount of contiguous memory available.
     * if maxfree <= 110% of the current size of wmb cache, it is close
     * to being exhausted/filled, flush so that we have enough space
     * to satisfy future requests
     * FIXME: What is the logic for using 110% here?
     */ 
    if(maxfree <= 1.1 * wmb_req_cache_sz)
    {
        return NEEDS_IO_FLUSH;
    }

    return NO_FLUSH;
}

#ifdef _ADIOS2
static int needs_to_write_decomp(file_desc_t *file, int ioid)
{
    assert(file != NULL);
    int ret = 1; // Yes
    for (int i = 0; i < file->n_written_ioids; i++)
    {
        if (file->written_ioids[i] == ioid)
        {
            ret = 0; // No
            break;
        }
    }

    return ret;
}

static int register_decomp(file_desc_t *file, int ioid)
{
    assert(file != NULL);
    if (file->n_written_ioids >= ADIOS_PIO_MAX_DECOMPS)
    {
        return pio_err(NULL, NULL, PIO_EINVAL, __FILE__, __LINE__,
                        "Registering (ADIOS) I/O decomposition (id = %d) failed for file (%s, ncid=%d). The number of I/O decompositions registered (%d) equals the maximum allowed for the file (%d)", ioid, pio_get_fname_from_file(file), file->pio_ncid, file->n_written_ioids, ADIOS_PIO_MAX_DECOMPS);
    }

    file->written_ioids[file->n_written_ioids] = ioid;
    ++file->n_written_ioids;

    return PIO_NOERR;
}

static int PIOc_write_decomp_adios(file_desc_t *file, int ioid)
{
    assert(file != NULL);
    adios2_error adiosErr = adios2_error_none;
    io_desc_t *iodesc = pio_get_iodesc_from_id(ioid);
    char name[PIO_MAX_NAME];
    snprintf(name, PIO_MAX_NAME, "/__pio__/decomp/%d", ioid);

    adios2_type type = adios2_type_int32_t;
    if (sizeof(PIO_Offset) == 8)
        type = adios2_type_int64_t;

    size_t av_count[1];
    if (iodesc->maplen > 1)
    {
        av_count[0] = (size_t)iodesc->maplen;

        adios2_variable *variableH = adios2_inquire_variable(file->ioH, name);
        if (variableH == NULL)
        {
            variableH = adios2_define_variable(file->ioH, name, type,
                                               1, NULL, NULL, av_count,
                                               adios2_constant_dims_true);
            if (variableH == NULL)
            {
                return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) variable (name=%s) failed for file (%s, ncid=%d)", name, pio_get_fname_from_file(file), file->pio_ncid);
            }
        }

        adiosErr = adios2_put(file->engineH, variableH, iodesc->map, adios2_mode_sync);
        if (adiosErr != adios2_error_none)
        {
            return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Putting (ADIOS) variable (name=%s) failed (adios2_error=%s) for file (%s, ncid=%d)", name, adios2_error_to_string(adiosErr), pio_get_fname_from_file(file), file->pio_ncid);
        }
    }
    else if (iodesc->maplen == 1) /* Handle the case where maplen is 1 */
    {
        int maplen = iodesc->maplen + 1;
        void *mapbuf = NULL;
        if (type == adios2_type_int32_t)
        {
            mapbuf = (int*)calloc(maplen, sizeof(int));
            if (mapbuf == NULL)
            {
                return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                                "Writing (ADIOS) I/O decomposition (id = %d) failed for file (%s, ncid=%d). Out of memory allocating %lld bytes for map buffer", ioid, pio_get_fname_from_file(file), file->pio_ncid, (long long)(maplen * sizeof(int)));
            }
            ((int*)mapbuf)[0] = iodesc->map[0];
            ((int*)mapbuf)[1] = 0;
        }
        else
        {
            mapbuf = (long*)calloc(maplen, sizeof(long));
            if (mapbuf == NULL)
            {
                return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                                "Writing (ADIOS) I/O decomposition (id = %d) failed for file (%s, ncid=%d). Out of memory allocating %lld bytes for map buffer", ioid, pio_get_fname_from_file(file), file->pio_ncid, (long long)(maplen * sizeof(long)));
            }
            ((long*)mapbuf)[0] = iodesc->map[0];
            ((long*)mapbuf)[1] = 0;
        }

        av_count[0] = (size_t)maplen;
        adios2_variable *variableH = adios2_inquire_variable(file->ioH, name);
        if (variableH == NULL)
        {
            variableH = adios2_define_variable(file->ioH, name, type,
                                               1, NULL, NULL, av_count,
                                               adios2_constant_dims_true);
            if (variableH == NULL)
            {
                return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) variable (name=%s) failed for file (%s, ncid=%d)", name, pio_get_fname_from_file(file), file->pio_ncid);
            }
        }

        adiosErr = adios2_put(file->engineH, variableH, mapbuf, adios2_mode_sync);
        if (adiosErr != adios2_error_none)
        {
            return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Putting (ADIOS) variable (name=%s) failed (adios2_error=%s) for file (%s, ncid=%d)", name, adios2_error_to_string(adiosErr), pio_get_fname_from_file(file), file->pio_ncid);
        }

        if (mapbuf != NULL)
            free(mapbuf);
    }
    else /* Handle the case where maplen is less than 1 */
    {
        long mapbuf[2];
        mapbuf[0] = 0;
        mapbuf[1] = 0;
        av_count[0] = (size_t)2;

        adios2_variable *variableH = adios2_inquire_variable(file->ioH, name);
        if (variableH == NULL)
        {
            variableH = adios2_define_variable(file->ioH, name, type,
                                               1, NULL, NULL, av_count,
                                               adios2_constant_dims_true);
            if (variableH == NULL)
            {
                return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) variable (name=%s) failed for file (%s, ncid=%d)", name, pio_get_fname_from_file(file), file->pio_ncid);
            }
        }

        adiosErr = adios2_put(file->engineH, variableH, mapbuf, adios2_mode_sync);
        if (adiosErr != adios2_error_none)
        {
            return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Putting (ADIOS) variable (name=%s) failed (adios2_error=%s) for file (%s, ncid=%d)", name, adios2_error_to_string(adiosErr), pio_get_fname_from_file(file), file->pio_ncid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
    if (file->adios_iomaster == MPI_ROOT)
    {
        char att_name[PIO_MAX_NAME];

        snprintf(att_name, PIO_MAX_NAME, "%s/piotype", name);
        adios2_attribute *attributeH = adios2_inquire_attribute(file->ioH, att_name);
        if (attributeH == NULL)
        {
            attributeH = adios2_define_attribute(file->ioH, att_name, adios2_type_int32_t, &iodesc->piotype);
            if (attributeH == NULL)
            {
                return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) attribute (name=%s) failed for file (%s, ncid=%d)", att_name, pio_get_fname_from_file(file), file->pio_ncid);
            }
        }

        snprintf(att_name, PIO_MAX_NAME, "%s/ndims", name);
        attributeH = adios2_inquire_attribute(file->ioH, att_name);
        if (attributeH == NULL)
        {
            attributeH = adios2_define_attribute(file->ioH, att_name, adios2_type_int32_t, &iodesc->ndims);
            if (attributeH == NULL)
            {
                return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) attribute (name=%s) failed for file (%s, ncid=%d)", att_name, pio_get_fname_from_file(file), file->pio_ncid);
            }
        }

        snprintf(att_name, PIO_MAX_NAME, "%s/dimlen", name);
        attributeH = adios2_inquire_attribute(file->ioH, att_name);
        if (attributeH == NULL)
        {
            attributeH = adios2_define_attribute_array(file->ioH, att_name, adios2_type_int32_t, iodesc->dimlen, iodesc->ndims);
            if (attributeH == NULL)
            {
                return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) attribute array (name=%s, size=%d) failed for file (%s, ncid=%d)", att_name, iodesc->ndims, pio_get_fname_from_file(file), file->pio_ncid);
            }
        }
    }

    return PIO_NOERR;
}

#define ADIOS_CONVERT_ARRAY(array, arraylen, from_type, to_type, ierr, buf) \
{ \
    from_type *d = (from_type*)array; \
    to_type *f = (to_type*)malloc(arraylen * sizeof(to_type)); \
    if (f) { \
        for (int i = 0; i < arraylen; ++i) \
            f[i] = (to_type)d[i]; \
        buf = f; \
    } \
    else \
    { \
        ierr = PIO_ENOMEM; \
    } \
}

#define ADIOS_CONVERT_FROM(FROM_TYPE_ID, from_type) \
{ \
    if (iodesc->piotype == FROM_TYPE_ID && av->nc_type == PIO_DOUBLE) \
    { \
        ADIOS_CONVERT_ARRAY(array, arraylen, from_type, double, *ierr, buf); \
    } \
    else if (iodesc->piotype == FROM_TYPE_ID && av->nc_type == PIO_FLOAT) \
    { \
        ADIOS_CONVERT_ARRAY(array, arraylen, from_type, float, *ierr, buf); \
    } \
    else if (iodesc->piotype == FROM_TYPE_ID && av->nc_type == PIO_REAL) \
    { \
        ADIOS_CONVERT_ARRAY(array, arraylen, from_type, float, *ierr, buf); \
    } \
    else if (iodesc->piotype == FROM_TYPE_ID && av->nc_type == PIO_INT) \
    { \
        ADIOS_CONVERT_ARRAY(array, arraylen, from_type, int, *ierr, buf); \
    } \
    else if (iodesc->piotype == FROM_TYPE_ID && av->nc_type == PIO_UINT) \
    { \
        ADIOS_CONVERT_ARRAY(array, arraylen, from_type, unsigned int, *ierr, buf); \
    } \
    else if (iodesc->piotype == FROM_TYPE_ID && av->nc_type == PIO_SHORT) \
    { \
        ADIOS_CONVERT_ARRAY(array, arraylen, from_type, short int, *ierr, buf); \
    } \
    else if (iodesc->piotype == FROM_TYPE_ID && av->nc_type == PIO_USHORT) \
    { \
        ADIOS_CONVERT_ARRAY(array, arraylen, from_type, unsigned short int, *ierr, buf); \
    } \
    else if (iodesc->piotype == FROM_TYPE_ID && av->nc_type == PIO_INT64) \
    { \
        ADIOS_CONVERT_ARRAY(array, arraylen, from_type, int64_t, *ierr, buf); \
    } \
    else if (iodesc->piotype == FROM_TYPE_ID && av->nc_type == PIO_UINT64) \
    { \
        ADIOS_CONVERT_ARRAY(array, arraylen, from_type, uint64_t, *ierr, buf); \
    } \
    else if (iodesc->piotype == FROM_TYPE_ID && av->nc_type == PIO_CHAR) \
    { \
        ADIOS_CONVERT_ARRAY(array, arraylen, from_type, char, *ierr, buf); \
    } \
    else if (iodesc->piotype == FROM_TYPE_ID && av->nc_type == PIO_BYTE) \
    { \
        ADIOS_CONVERT_ARRAY(array, arraylen, from_type, char, *ierr, buf); \
    } \
    else if (iodesc->piotype == FROM_TYPE_ID && av->nc_type == PIO_UBYTE) \
    { \
        ADIOS_CONVERT_ARRAY(array, arraylen, from_type, unsigned char, *ierr, buf); \
    } \
}

static void *PIOc_convert_buffer_adios(file_desc_t *file, io_desc_t *iodesc,
                                       adios_var_desc_t *av, void *array, int arraylen,
                                       int *ierr)
{
    assert(file != NULL && iodesc != NULL && av != NULL && array != NULL && ierr != NULL);
    void *buf = array;
    *ierr = 0;

    ADIOS_CONVERT_FROM(PIO_DOUBLE, double);
    ADIOS_CONVERT_FROM(PIO_FLOAT, float);
    ADIOS_CONVERT_FROM(PIO_INT, int);
    ADIOS_CONVERT_FROM(PIO_UINT, unsigned int);
    ADIOS_CONVERT_FROM(PIO_SHORT, short int);
    ADIOS_CONVERT_FROM(PIO_USHORT, unsigned short int);
    ADIOS_CONVERT_FROM(PIO_INT64, int64_t);
    ADIOS_CONVERT_FROM(PIO_UINT64, uint64_t);
    ADIOS_CONVERT_FROM(PIO_CHAR, char);
    ADIOS_CONVERT_FROM(PIO_BYTE, char);
    ADIOS_CONVERT_FROM(PIO_UBYTE, unsigned char);

    return buf;
}

#define ADIOS_COPY_ONE(temp_buf, array, var_type) \
{ \
    temp_buf = (var_type*)malloc(2 * sizeof(var_type)); \
    if (temp_buf != NULL) \
        memcpy(temp_buf, array, sizeof(var_type)); \
}

void *PIOc_copy_one_element_adios(void *array, io_desc_t *iodesc)
{
    assert(array != NULL && iodesc != NULL);
    void *temp_buf = NULL;
    if (iodesc->piotype == PIO_DOUBLE)
    {
        ADIOS_COPY_ONE(temp_buf, array, double);
    }
    else if (iodesc->piotype == PIO_FLOAT || iodesc->piotype == PIO_REAL)
    {
        ADIOS_COPY_ONE(temp_buf, array, float);
    }
    else if (iodesc->piotype == PIO_INT || iodesc->piotype == PIO_UINT)
    {
        ADIOS_COPY_ONE(temp_buf, array, int);
    }
    else if (iodesc->piotype == PIO_SHORT || iodesc->piotype == PIO_USHORT)
    {
        ADIOS_COPY_ONE(temp_buf, array, short int);
    }
    else if (iodesc->piotype == PIO_INT64 || iodesc->piotype == PIO_UINT64)
    {
        ADIOS_COPY_ONE(temp_buf, array, int64_t);
    }
    else if (iodesc->piotype == PIO_CHAR || iodesc->piotype == PIO_BYTE || iodesc->piotype == PIO_UBYTE)
    {
        ADIOS_COPY_ONE(temp_buf, array, char);
    }

    return temp_buf;
}

static int PIOc_write_darray_adios(file_desc_t *file, int varid, int ioid,
                                   io_desc_t *iodesc, PIO_Offset arraylen,
                                   void *array, void *fillvalue)
{
    assert(file != NULL && iodesc != NULL && array != NULL);
    int ierr = PIO_NOERR;
    adios2_error adiosErr = adios2_error_none;
    if (varid < 0 || varid >= file->num_vars)
    {
        return pio_err(NULL, file, PIO_EBADID, __FILE__, __LINE__,
                        "Writing (ADIOS) variable (varid=%d) to file (%s, ncid=%d) failed. Invalid variable id, %d (expected >=0 && < number of variables in file, %d), provided", varid, pio_get_fname_from_file(file), file->pio_ncid, varid, file->num_vars);
    }

    adios_var_desc_t *av = &(file->adios_vars[varid]);

    void *temp_buf = NULL;
    if (arraylen == 1) /* Handle the case where there is one array element */
    {
        arraylen = 2;
        temp_buf = PIOc_copy_one_element_adios(array, iodesc);
        if (temp_buf == NULL)
        {
            return pio_err(NULL, file, PIO_ENOMEM, __FILE__, __LINE__,
                            "Writing (ADIOS) variable (varid=%d) to file (%s, ncid=%d) failed. Out of memory allocating %lld bytes for a temporary buffer", varid, pio_get_fname_from_file(file), file->pio_ncid, (long long) (arraylen * iodesc->piotype_size));
        }
        array = temp_buf;
    }
    else if (arraylen == 0) /* Handle the case where there is zero array element */
    {
        arraylen = 2;
        temp_buf = (int64_t*)calloc(arraylen, sizeof(int64_t));
        if (temp_buf == NULL)
        {
            return pio_err(NULL, file, PIO_ENOMEM, __FILE__, __LINE__,
                            "Writing (ADIOS) variable (varid=%d) to file (%s, ncid=%d) failed. Out of memory allocating %lld bytes for a temporary buffer", varid, pio_get_fname_from_file(file), file->pio_ncid, (long long) (arraylen * sizeof(int64_t)));
        }
        array = temp_buf;
    }

    if (av->adios_varid == NULL)
    {
        /* First we need to define the variable now that we know it's decomposition */
        adios2_type atype = av->adios_type;
        size_t av_count[1];
        av_count[0] = (size_t)arraylen;
        av->adios_varid = adios2_define_variable(file->ioH, av->name, atype,
                                                 1, NULL, NULL, av_count,
                                                 adios2_constant_dims_true);
        if (av->adios_varid == NULL)
        {
            return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) variable (name=%s) failed for file (%s, ncid=%d)", av->name, pio_get_fname_from_file(file), file->pio_ncid);
        }

        /* Different decompositions at different frames */
        char name_varid[PIO_MAX_NAME];
        snprintf(name_varid, PIO_MAX_NAME, "decomp_id/%s", av->name);
        av_count[0] = 1;
        av->decomp_varid = adios2_inquire_variable(file->ioH, name_varid);
        if (av->decomp_varid == NULL)
        {
            av->decomp_varid = adios2_define_variable(file->ioH, name_varid, adios2_type_int32_t,
                                                      1, NULL, NULL, av_count,
                                                      adios2_constant_dims_true);
            if (av->decomp_varid == NULL)
            {
                return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) variable (name=%s) failed for file (%s, ncid=%d)", name_varid, pio_get_fname_from_file(file), file->pio_ncid);
            }
        }

        snprintf(name_varid, PIO_MAX_NAME, "frame_id/%s", av->name);
        av_count[0] = 1;
        av->frame_varid = adios2_inquire_variable(file->ioH, name_varid);
        if (av->frame_varid == NULL)
        {
            av->frame_varid = adios2_define_variable(file->ioH, name_varid, adios2_type_int32_t,
                                                     1, NULL, NULL, av_count,
                                                     adios2_constant_dims_true);
            if (av->frame_varid == NULL)
            {
                return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) variable (name=%s) failed for file (%s, ncid=%d)", name_varid, pio_get_fname_from_file(file), file->pio_ncid);
            }
        }

        snprintf(name_varid, PIO_MAX_NAME, "fillval_id/%s", av->name);
        av_count[0] = 1;
        av->fillval_varid = adios2_inquire_variable(file->ioH, name_varid);
        if (av->fillval_varid == NULL)
        {
            av->fillval_varid = adios2_define_variable(file->ioH, name_varid, atype,
                                                       1, NULL, NULL, av_count,
                                                       adios2_constant_dims_true);
            if (av->fillval_varid == NULL)
            {
                return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) variable (name=%s) failed for file (%s, ncid=%d)", name_varid, pio_get_fname_from_file(file), file->pio_ncid);
            }
        }

        if (file->adios_iomaster == MPI_ROOT)
        {
            /* Some of the codes were moved to pio_nc.c */
            char decompname[PIO_MAX_NAME];
            char att_name[PIO_MAX_NAME];

            snprintf(decompname, PIO_MAX_NAME, "%d", ioid);
            snprintf(att_name, PIO_MAX_NAME, "%s/__pio__/decomp", av->name);
            adios2_attribute *attributeH = adios2_inquire_attribute(file->ioH, att_name);
            if (attributeH == NULL)
            {
                attributeH = adios2_define_attribute(file->ioH, att_name, adios2_type_string, decompname);
                if (attributeH == NULL)
                {
                    return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) attribute (name=%s) failed for file (%s, ncid=%d)", att_name, pio_get_fname_from_file(file), file->pio_ncid);
                }
            }

            snprintf(att_name, PIO_MAX_NAME, "%s/__pio__/ncop", av->name);
            attributeH = adios2_inquire_attribute(file->ioH, att_name);
            if (attributeH == NULL)
            {
                attributeH = adios2_define_attribute(file->ioH, att_name, adios2_type_string, "darray");
                if (attributeH == NULL)
                {
                    return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) attribute (name=%s) failed for file (%s, ncid=%d)", att_name, pio_get_fname_from_file(file), file->pio_ncid);
                }
            }
        }
    }

    /* Check if we need to write the decomposition. Write it */
    if (needs_to_write_decomp(file, ioid))
    {
        ierr = PIOc_write_decomp_adios(file, ioid);
        if (ierr != PIO_NOERR)
        {
            return pio_err(NULL, file, ierr, __FILE__, __LINE__,
                            "Writing (ADIOS) variable (varid=%d) to file (%s, ncid=%d) failed. Writing the I/O decomposition (ioid=%d) associated with the variable failed", varid, pio_get_fname_from_file(file), file->pio_ncid, ioid);
        }
        ierr = register_decomp(file, ioid);
        if (ierr != PIO_NOERR)
        {
            return pio_err(NULL, file, ierr, __FILE__, __LINE__,
                            "Writing (ADIOS) variable (varid=%d) to file (%s, ncid=%d) failed. Registering the I/O decomposition (ioid=%d) associated with the variable failed", varid, pio_get_fname_from_file(file), file->pio_ncid, ioid);
        }
    }

    /* E3SM history data special handling: down-conversion from double to float */
    void *databuf = array;
    void *fillbuf = fillvalue;
    int buf_needs_free = 0;
    if (iodesc->piotype != av->nc_type)
    {
        databuf = PIOc_convert_buffer_adios(file, iodesc, av, array, arraylen, &ierr);
        if (ierr != PIO_NOERR)
        {
            return pio_err(NULL, file, ierr, __FILE__, __LINE__,
                            "Writing (ADIOS) variable (varid=%d) to file (%s, ncid=%d) failed. Type conversion for data buffer failed", varid, pio_get_fname_from_file(file), file->pio_ncid);
        }

        if (fillvalue != NULL)
        {
            fillbuf = PIOc_convert_buffer_adios(file, iodesc, av, fillvalue, 1, &ierr);
            if (ierr != PIO_NOERR)
            {
                return pio_err(NULL, file, ierr, __FILE__, __LINE__,
                                "Writing (ADIOS) variable (varid=%d) to file (%s, ncid=%d) failed. Type conversion for fill buffer failed", varid, pio_get_fname_from_file(file), file->pio_ncid);
            }
        }

        buf_needs_free = 1;
    }

    adiosErr = adios2_put(file->engineH, av->adios_varid, databuf, adios2_mode_sync);
    if (adiosErr != adios2_error_none)
    {
        return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Putting (ADIOS) variable (name=%s) failed (adios2_error=%s) for file (%s, ncid=%d)", av->name, adios2_error_to_string(adiosErr), pio_get_fname_from_file(file), file->pio_ncid);
    }

    /* NOTE: PIOc_setframe with different decompositions */
    /* Different decompositions at different frames and fillvalue */
    if (fillbuf != NULL) /* Write out user provided fillvalue */
    {
        adiosErr = adios2_put(file->engineH, av->fillval_varid, fillbuf, adios2_mode_sync);
        if (adiosErr != adios2_error_none)
        {
            return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Putting (ADIOS) variable (name=fillval_id/%s) failed (adios2_error=%s) for file (%s, ncid=%d)", av->name, adios2_error_to_string(adiosErr), pio_get_fname_from_file(file), file->pio_ncid);
        }
    }
    else
    {
        ioid = -ioid;
    }

    adiosErr = adios2_put(file->engineH, av->decomp_varid, &ioid, adios2_mode_sync);
    if (adiosErr != adios2_error_none)
    {
        return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Putting (ADIOS) variable (name=decomp_id/%s) failed (adios2_error=%s) for file (%s, ncid=%d)", av->name, adios2_error_to_string(adiosErr), pio_get_fname_from_file(file), file->pio_ncid);
    }

    adiosErr = adios2_put(file->engineH, av->frame_varid, &(file->varlist[varid].record), adios2_mode_sync);
    if (adiosErr != adios2_error_none)
    {
        return pio_err(NULL, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Putting (ADIOS) variable (name=frame_id/%s) failed (adios2_error=%s) for file (%s, ncid=%d)", av->name, adios2_error_to_string(adiosErr), pio_get_fname_from_file(file), file->pio_ncid);
    }

    if (buf_needs_free)
    {
        if (databuf != NULL)
            free(databuf);

        if (fillbuf != NULL)
            free(fillbuf);
    }

    if (temp_buf != NULL)
        free(temp_buf);

    return PIO_NOERR;
}

#endif

/**
 * Write a distributed array to the output file.
 *
 * This routine aggregates output on the compute nodes and only sends
 * it to the IO nodes when the compute buffer is full or when a flush
 * is triggered.
 *
 * Internally, this function will:
 * <ul>
 * <li>Locate info about this file, decomposition, and variable.
 * <li>If we don't have a fillvalue for this variable, determine one
 * and remember it for future calls.
 * <li>Initialize or find the multi_buffer for this record/var.
 * <li>Find out how much free space is available in the multi buffer
 * and flush if needed.
 * <li>Store the new user data in the mutli buffer.
 * <li>If needed (only for subset rearranger), fill in gaps in data
 * with fillvalue.
 * <li>Remember the frame value (i.e. record number) of this data if
 * there is one.
 * </ul>
 *
 * NOTE: The write multi buffer wmulti_buffer is the cache on compute
 * nodes that will collect and store multiple variables before sending
 * them to the io nodes. Aggregating variables in this way leads to a
 * considerable savings in communication expense. Variables in the wmb
 * array must have the same decomposition and base data size and we
 * also need to keep track of whether each is a recordvar (has an
 * unlimited dimension) or not.
 *
 * @param ncid the ncid of the open netCDF file.
 * @param varid the ID of the variable that these data will be written
 * to.
 * @param ioid the I/O description ID as passed back by
 * PIOc_InitDecomp().
 * @param arraylen the length of the array to be written. This should
 * be at least the length of the local component of the distrubited
 * array. (Any values beyond length of the local component will be
 * ignored.)
 * @param array pointer to an array of length arraylen with the data
 * to be written. This is a pointer to the distributed portion of the
 * array that is on this task.
 * @param fillvalue pointer to the fill value to be used for missing
 * data.
 * @returns 0 for success, non-zero error code for failure.
 * @ingroup PIO_write_darray
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_write_darray(int ncid, int varid, int ioid, PIO_Offset arraylen, void *array,
                      void *fillvalue)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Info about file we are writing to. */
    io_desc_t *iodesc;     /* The IO description. */
    var_desc_t *vdesc;     /* Info about the var being written. */
    void *bufptr;          /* A data buffer. */
    MPI_Datatype vtype;    /* The MPI type of the variable. */
    wmulti_buffer *wmb;    /* The write multi buffer for one or more vars. */
    int recordvar;         /* Non-zero if this is a record variable. */
    int needsflush = 0;    /* True if we need to flush buffer. */
    PIO_Offset decomp_max_regions; /* Max non-contiguous regions in the IO decomposition */
    PIO_Offset io_max_regions; /* Max non-contiguous regions cached in a single IO process */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI functions. */
    int ierr = PIO_NOERR;  /* Return code. */

#ifdef TIMING
    GPTLstart("PIO:PIOc_write_darray");
#endif
    LOG((1, "PIOc_write_darray ncid = %d varid = %d ioid = %d arraylen = %d",
         ncid, varid, ioid, arraylen));

    /* Get the file info. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, PIO_EBADID, __FILE__, __LINE__,
                        "Writing variable (varid=%d) failed on file. Invalid file id (ncid=%d) provided", varid, ncid);
    }
    ios = file->iosystem;

#ifdef TIMING
#ifdef _ADIOS2 /* TAHSIN: timing */
    if (file->iotype == PIO_IOTYPE_ADIOS)
        GPTLstart("PIO:PIOc_write_darray_adios"); /* TAHSIN: start */
#endif
#endif

    LOG((1, "PIOc_write_darray ncid=%d varid=%d wb_pend=%llu file_wb_pend=%llu",
          ncid, varid,
          (unsigned long long int) file->varlist[varid].wb_pend,
          (unsigned long long int) file->wb_pend
    ));

    /* Can we write to this file? */
    if (!(file->mode & PIO_WRITE))
    {
        return pio_err(ios, file, PIO_EPERM, __FILE__, __LINE__,
                        "Writing variable (%s, varid=%d) to file (%s, ncid=%d) failed. The file was not opened for writing, try reopening the file in write mode (use the PIO_WRITE flag)", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid);
    }

    /* Get decomposition information. */
    if (!(iodesc = pio_get_iodesc_from_id(ioid)))
    {
        return pio_err(ios, file, PIO_EBADID, __FILE__, __LINE__,
                        "Writing variable (%s, varid=%d) to file (%s, ncid=%d) failed. Invalid I/O descriptor id (ioid=%d) provided", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, ioid);
    }

    /* Check that the local size of the variable passed in matches the
     * size expected by the io descriptor. Fail if arraylen is too
     * small, just put a warning in the log and truncate arraylen
     * if it is too big (the excess values will be ignored.) */
    if (arraylen < iodesc->ndof)
    {
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Writing variable (%s, varid=%d) to file (%s, ncid=%d) failed. The local array size (arraylen=%lld) is smaller than expected, the I/O decomposition (ioid=%d) requires a local array of size = %lld", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, (long long int) arraylen, ioid, (long long int) iodesc->ndof);
    }
    LOG((2, "%s arraylen = %d iodesc->ndof = %d",
         (arraylen > iodesc->ndof) ? "WARNING: arraylen > iodesc->ndof" : "",
         arraylen, iodesc->ndof));
    if (arraylen > iodesc->ndof)
        arraylen = iodesc->ndof;

#ifdef PIO_MICRO_TIMING
    mtimer_start(file->varlist[varid].wr_mtimer);
#endif

#if PIO_SAVE_DECOMPS
    if(!(iodesc->is_saved) &&
        pio_save_decomps_regex_match(ioid, file->fname, file->varlist[varid].vname))
    {
        char filename[PIO_MAX_NAME];
        ierr = pio_create_uniq_str(ios, iodesc, filename, PIO_MAX_NAME, "piodecomp", ".dat");
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Writing variable (%s, varid=%d) to file (%s, ncid=%d) failed. Saving I/O decomposition (ioid=%d) failed. Unable to create a unique file name for saving the I/O decomposition", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, ioid);
        }
        LOG((2, "Saving decomp map (write) to %s", filename));
        PIOc_writemap(filename, ioid, iodesc->ndims, iodesc->dimlen, iodesc->maplen, iodesc->map, ios->my_comm);
        iodesc->is_saved = true;
    }
#endif
    /* Get var description. */
    vdesc = &(file->varlist[varid]);
    LOG((2, "vdesc record %d nreqs %d", vdesc->record, vdesc->nreqs));

    /* If we don't know the fill value for this var, get it. */
    if (!vdesc->fillvalue)
        if ((ierr = find_var_fillvalue(file, varid, vdesc)))
        {
            return pio_err(ios, file, PIO_EBADID, __FILE__, __LINE__,
                            "Writing variable (%s, varid=%d) to file (%s, ncid=%d) failed. Finding fillvalue associated with the variable failed", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid);
        }

    /* Is this a record variable? The user must set the vdesc->record
     * value by calling PIOc_setframe() before calling this
     * function. */
    recordvar = vdesc->record >= 0 ? 1 : 0;
    LOG((3, "recordvar = %d looking for multibuffer", recordvar));

    /* Move to end of list or the entry that matches this ioid. */
    for (wmb = &file->buffer; wmb->next; wmb = wmb->next)
        if (wmb->ioid == ioid && wmb->recordvar == recordvar)
            break;
    LOG((3, "wmb->ioid = %d wmb->recordvar = %d", wmb->ioid, wmb->recordvar));

#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        ierr = PIOc_write_darray_adios(file, varid, ioid, iodesc, arraylen, array, fillvalue);
#ifdef TIMING
        GPTLstop("PIO:PIOc_write_darray_adios"); /* TAHSIN: stop */
        GPTLstop("PIO:PIOc_write_darray");
#endif
        return ierr;
    }
#endif

    /* If we did not find an existing wmb entry, create a new wmb. */
    if (wmb->ioid != ioid || wmb->recordvar != recordvar)
    {
        /* Allocate a buffer. */
        LOG((3, "allocating multi-buffer"));
        if (!(wmb->next = calloc(1, sizeof(wmulti_buffer))))
        {
            return pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                            "Writing variable (%s, varid=%d) to file (%s, ncid=%d) failed. Out of memory allocating %lld bytes for a write multi buffer to cache user data", pio_get_fname_from_file(file), varid, pio_get_fname_from_file(file), file->pio_ncid, (unsigned long long) sizeof(wmulti_buffer));
        }
        LOG((3, "allocated multi-buffer"));

        /* Set pointer to newly allocated buffer and initialize.*/
        wmb = wmb->next;
        wmb->recordvar = recordvar;
        wmb->next = NULL;
        wmb->ioid = ioid;
        wmb->num_arrays = 0;
        wmb->arraylen = arraylen;
        wmb->vid = NULL;
        wmb->data = NULL;
        wmb->frame = NULL;
        wmb->fillvalue = NULL;
    }
    LOG((2, "wmb->num_arrays = %d arraylen = %d iodesc->mpitype_size = %d\n",
         wmb->num_arrays, arraylen, iodesc->mpitype_size));

    needsflush = PIO_wmb_needs_flush(wmb, arraylen, iodesc);
    assert(needsflush >= 0);

    /* When using PIO with PnetCDF + SUBSET rearranger the number
       of non-contiguous regions cached in a single IO process can
       grow to a large number. PnetCDF is not efficient at handling
       very large number of regions (sub-array requests) in the
       data written out. We typically run out of memory or the
       write is very slow.

       We need to set a limit on the potential (after rearrangement)
       maximum number of non-contiguous regions in an IO process and
       forcefully flush out user data cached by a compute process
       when that limit has been reached. */
    decomp_max_regions = (iodesc->maxregions >= iodesc->maxfillregions)? iodesc->maxregions : iodesc->maxfillregions;
    io_max_regions = (1 + wmb->num_arrays) * decomp_max_regions;
    if (io_max_regions > PIO_MAX_CACHED_IO_REGIONS)
        needsflush = 2;

    /* Tell all tasks on the computation communicator whether we need
     * to flush data. */
    if ((mpierr = MPI_Allreduce(MPI_IN_PLACE, &needsflush, 1,  MPI_INT,  MPI_MAX,
                                ios->comp_comm)))
        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    LOG((2, "needsflush = %d", needsflush));

    if(!ios->async || !ios->ioproc)
    {
        if(file->varlist[varid].vrsize == 0)
        {
            ierr = calc_var_rec_sz(ncid, varid);
            if(ierr != PIO_NOERR)
            {
                LOG((1, "Unable to calculate the variable record size"));
            }
        }
    }
    /* Flush data if needed. */
    if (needsflush > 0)
    {
#if !PIO_USE_MALLOC
#ifdef PIO_ENABLE_LOGGING
        /* Collect a debug report about buffer. */
        cn_buffer_report(ios, true);
#endif /* PIO_ENABLE_LOGGING */
#endif /* !PIO_USE_MALLOC */

        /* Flush buffer to I/O processes - rearrange data and
         * start writing data from the I/O processes
         * Note : Setting the last flag in flush_buffer to
         * true will force flush the buffer to disk for all
         * iotypes (wait for write to complete for PnetCDF)
         */
        if ((ierr = flush_buffer(ncid, wmb, (needsflush == 2))))
        {
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Writing variable (%s, varid=%d) to file (%s, ncid=%d) failed. Flushing data (multiple cached variables with the same decomposition) from compute processes to I/O processes %s failed", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, (needsflush == 2) ? "and to disk" : "");
        }
    }

    /* One record size (sum across all procs) of data is buffered */
    file->varlist[varid].wb_pend += file->varlist[varid].vrsize;
    file->wb_pend += file->varlist[varid].vrsize;
    LOG((1, "Current pending bytes for ncid=%d, varid=%d var_wb_pend= %llu, file_wb_pend=%llu",
          ncid, varid,
          (unsigned long long int) file->varlist[varid].wb_pend,
          (unsigned long long int) file->wb_pend
    ));
    /* Buffering data is considered an async event (to indicate
      *that the event is not yet complete)
      */
#ifdef PIO_MICRO_TIMING
    mtimer_async_event_in_progress(file->varlist[varid].wr_mtimer, true);
#endif

    /* Get memory for data. */
    if (arraylen > 0)
    {
        if (!(wmb->data = bgetr(wmb->data, (1 + wmb->num_arrays) * arraylen * iodesc->mpitype_size)))
        {
            return pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                            "Writing variable (%s, varid=%d) to file (%s, ncid=%d) failed. Out of memory allocating space (realloc %lld bytes) to cache user data", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, (long long int )((1 + wmb->num_arrays) * arraylen * iodesc->mpitype_size));
        }
        LOG((2, "got %ld bytes for data", (1 + wmb->num_arrays) * arraylen * iodesc->mpitype_size));
    }

    /* vid is an array of variable ids in the wmb list, grow the list
     * and add the new entry. */
    if (!(wmb->vid = realloc(wmb->vid, sizeof(int) * (1 + wmb->num_arrays))))
    {
        return pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                        "Writing variable (%s, varid=%d) to file (%s, ncid=%d) failed. Out of memory allocating space (realloc %lld bytes) for array of variable ids in write multi buffer to cache user data", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, (unsigned long long)(sizeof(int) * (1 + wmb->num_arrays)));
    }

    /* wmb->frame is the record number, we assume that the variables
     * in the wmb list may not all have the same unlimited dimension
     * value although they usually do. */
    if (vdesc->record >= 0)
        if (!(wmb->frame = realloc(wmb->frame, sizeof(int) * (1 + wmb->num_arrays))))
        {
            return pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                            "Writing variable (%s, varid=%d) to file (%s, ncid=%d) failed. Out of memory allocating space (realloc %lld bytes) for array of frame numbers in write multi buffer to cache user data", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, (unsigned long long)(sizeof(int) * (1 + wmb->num_arrays)));
        }

    /* If we need a fill value, get it. If we are using the subset
     * rearranger and not using the netcdf fill mode then we need to
     * do an extra write to fill in the holes with the fill value. */
    if (iodesc->needsfill)
    {
        /* Get memory to hold fill value. */
        if (!(wmb->fillvalue = bgetr(wmb->fillvalue, iodesc->mpitype_size * (1 + wmb->num_arrays))))
        {
            return pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                            "Writing variable (%s, varid=%d) to file (%s, ncid=%d) failed. Out of memory allocating space (realloc %lld bytes) for variable fillvalues in write multi buffer to cache user data", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, (unsigned long long)(iodesc->mpitype_size * (1 + wmb->num_arrays)));
        }

        /* If the user passed a fill value, use that, otherwise use
         * the default fill value of the netCDF type. Copy the fill
         * value to the buffer. */
        if (fillvalue)
        {
            memcpy((char *)wmb->fillvalue + iodesc->mpitype_size * wmb->num_arrays,
                   fillvalue, iodesc->mpitype_size);
            LOG((3, "copied user-provided fill value iodesc->mpitype_size = %d",
                 iodesc->mpitype_size));
        }
        else
        {
            void *fill;
            signed char byte_fill = PIO_FILL_BYTE;
            char char_fill = PIO_FILL_CHAR;
            short short_fill = PIO_FILL_SHORT;
            int int_fill = PIO_FILL_INT;
            float float_fill = PIO_FILL_FLOAT;
            double double_fill = PIO_FILL_DOUBLE;
#ifdef _NETCDF4
            unsigned char ubyte_fill = PIO_FILL_UBYTE;
            unsigned short ushort_fill = PIO_FILL_USHORT;
            unsigned int uint_fill = PIO_FILL_UINT;
            long long int64_fill = PIO_FILL_INT64;
            long long uint64_fill = PIO_FILL_UINT64;
#endif /* _NETCDF4 */
            vtype = (MPI_Datatype)iodesc->mpitype;
            LOG((3, "caller did not provide fill value vtype = %d", vtype));

            /* This must be done with an if statement, not a case, or
             * openmpi will not build. */
            if (vtype == MPI_BYTE)
                fill = &byte_fill;
            else if (vtype == MPI_CHAR)
                fill = &char_fill;
            else if (vtype == MPI_SHORT)
                fill = &short_fill;
            else if (vtype == MPI_INT)
                fill = &int_fill;
            else if (vtype == MPI_FLOAT)
                fill = &float_fill;
            else if (vtype == MPI_DOUBLE)
                fill = &double_fill;
#ifdef _NETCDF4
            else if (vtype == MPI_UNSIGNED_CHAR)
                fill = &ubyte_fill;
            else if (vtype == MPI_UNSIGNED_SHORT)
                fill = &ushort_fill;
            else if (vtype == MPI_UNSIGNED)
                fill = &uint_fill;
            else if (vtype == MPI_LONG_LONG)
                fill = &int64_fill;
            else if (vtype == MPI_UNSIGNED_LONG_LONG)
                fill = &uint64_fill;
#endif /* _NETCDF4 */
            else
                return pio_err(ios, file, PIO_EBADTYPE, __FILE__, __LINE__,
                                "Writing variable (%s, varid=%d) to file (%s, ncid=%d) failed. Unable to find a default fillvalue for variable, unsupported variable type (MPI type = %x)", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, vtype);

            memcpy((char *)wmb->fillvalue + iodesc->mpitype_size * wmb->num_arrays,
                   fill, iodesc->mpitype_size);
            LOG((3, "copied fill value"));
        }
    }

    /* Tell the buffer about the data it is getting. */
    wmb->arraylen = arraylen;
    wmb->vid[wmb->num_arrays] = varid;
    LOG((3, "wmb->num_arrays = %d wmb->vid[wmb->num_arrays] = %d", wmb->num_arrays,
         wmb->vid[wmb->num_arrays]));

    /* Copy the user-provided data to the buffer. */
    bufptr = (void *)((char *)wmb->data + arraylen * iodesc->mpitype_size * wmb->num_arrays);
    if (arraylen > 0)
    {
        memcpy(bufptr, array, arraylen * iodesc->mpitype_size);
        LOG((3, "copied %ld bytes of user data", arraylen * iodesc->mpitype_size));
    }

    /* Add the unlimited dimension value of this variable to the frame
     * array in wmb. */
    if (wmb->frame)
        wmb->frame[wmb->num_arrays] = vdesc->record;
    wmb->num_arrays++;

    LOG((2, "wmb->num_arrays = %d iodesc->maxbytes / iodesc->mpitype_size = %d "
         "iodesc->ndof = %d iodesc->llen = %d", wmb->num_arrays,
         iodesc->maxbytes / iodesc->mpitype_size, iodesc->ndof, iodesc->llen));

        LOG((1, "Write darray end : pending bytes for ncid=%d, varid=%d var_wb_pend=%llu file_wb_pend=%llu",
              ncid, varid,
              (unsigned long long int) file->varlist[varid].wb_pend,
              (unsigned long long int) file->wb_pend
        ));
#ifdef PIO_MICRO_TIMING
    mtimer_stop(file->varlist[varid].wr_mtimer, get_var_desc_str(ncid, varid, NULL));
#endif
#ifdef TIMING
    GPTLstop("PIO:PIOc_write_darray");
#endif
    return PIO_NOERR;
}

/**
 * Read a field from a file to the IO library.
 *
 * @param ncid identifies the netCDF file
 * @param varid the variable ID to be read
 * @param ioid: the I/O description ID as passed back by
 * PIOc_InitDecomp().
 * @param arraylen: the length of the array to be read. This
 * is the length of the distrubited array. That is, the length of
 * the portion of the data that is on the processor.
 * @param array: pointer to the data to be read. This is a
 * pointer to the distributed portion of the array that is on this
 * processor.
 * @return 0 for success, error code otherwise.
 * @ingroup PIO_read_darray
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_read_darray(int ncid, int varid, int ioid, PIO_Offset arraylen,
                     void *array)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    io_desc_t *iodesc;     /* Pointer to IO description information. */
    void *iobuf = NULL;    /* holds the data as read on the io node. */
    size_t rlen = 0;       /* the length of data in iobuf. */
    int ierr = PIO_NOERR, mpierr = MPI_SUCCESS;           /* Return code. */
    int fndims = 0;

#ifdef TIMING
    GPTLstart("PIO:PIOc_read_darray");
#endif
    /* Get the file info. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, PIO_EBADID, __FILE__, __LINE__,
                        "Reading variable (varid=%d) failed. Invalid arguments provided, file id (ncid=%d) is invalid", varid, ncid);
    }
    ios = file->iosystem;

    LOG((1, "PIOc_read_darray (ncid=%d (%s), varid=%d (%s)", ncid, pio_get_fname_from_file(file), varid, pio_get_vname_from_file(file, varid)));

    /* Get the iodesc. */
    if (!(iodesc = pio_get_iodesc_from_id(ioid)))
    {
        return pio_err(ios, file, PIO_EBADID, __FILE__, __LINE__,
                        "Reading variable (%s, varid=%d) from file (%s, ncid=%d)failed. Invalid arguments provided, I/O descriptor id (ioid=%d) is invalid", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, ioid);
    }
    pioassert(iodesc->rearranger == PIO_REARR_BOX || iodesc->rearranger == PIO_REARR_SUBSET,
              "unknown rearranger", __FILE__, __LINE__);

#ifdef PIO_MICRO_TIMING
    mtimer_start(file->varlist[varid].rd_mtimer);
#endif

#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        return pio_err(ios, file, PIO_EADIOSREAD, __FILE__, __LINE__,
                        "Reading variable (%s, varid=%d) from file (%s, ncid=%d)failed . ADIOS currently does not support reading variables", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid);
    }
#endif

    /* Run these on all tasks if async is not in use, but only on
     * non-IO tasks if async is in use. */
    if (!ios->async || !ios->ioproc)
    {
        /* Get the number of dims for this var. */
        LOG((3, "about to call PIOc_inq_varndims varid = %d", varid));
        ierr = PIOc_inq_varndims(file->pio_ncid, varid, &fndims);
        if(ierr != PIO_NOERR){
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Reading variable (%s, varid=%d) from file (%s, ncid=%d) failed . Inquiring number of variable dimensions failed", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid);
        }
        LOG((3, "called PIOc_inq_varndims varid = %d fndims = %d", varid, fndims));
    }
    /* ??? */
    if (ios->iomaster == MPI_ROOT)
        rlen = iodesc->maxiobuflen;
    else
        rlen = iodesc->llen;

    if(!ios->async || !ios->ioproc)
    {
        if(file->varlist[varid].vrsize == 0)
        {
            ierr = calc_var_rec_sz(ncid, varid);
            if(ierr != PIO_NOERR)
            {
                LOG((1, "Unable to calculate the variable record size"));
            }
        }
    }

    file->varlist[varid].rb_pend += file->varlist[varid].vrsize;
    file->rb_pend += file->varlist[varid].vrsize;

    /* Allocate a buffer for one record. */
    if (ios->ioproc && rlen > 0)
        if (!(iobuf = bget(iodesc->mpitype_size * rlen)))
        {
            return pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                            "Reading variable (%s, varid=%d) from file (%s, ncid=%d) failed . Out of memory allocating space (%lld bytes) in I/O processes to read data from file (before rearrangement)", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, (long long int) (iodesc->mpitype_size * rlen));
        }

    if(ios->async)
    {
        /* Send relevant args from compute procs to I/O procs */
        int msg = PIO_MSG_READDARRAY;
        
        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, ioid);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Reading variable (%s, varid=%d) from file (%s, ncid=%d) failed . Sending async message, PIO_MSG_READDARRAY, failed", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid);
        }

        /* Share results known only on computation tasks with IO tasks. */
        mpierr = MPI_Bcast(&fndims, 1, MPI_INT, ios->comproot, ios->my_comm);
        if(mpierr != MPI_SUCCESS)
        {
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
        }
        LOG((3, "shared fndims = %d", fndims));
    }

#if PIO_SAVE_DECOMPS
    if(!(iodesc->is_saved) &&
        pio_save_decomps_regex_match(ioid, file->fname, file->varlist[varid].vname))
    {
        char filename[PIO_MAX_NAME];
        ierr = pio_create_uniq_str(ios, iodesc, filename, PIO_MAX_NAME, "piodecomp", ".dat");
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Reading variable (%s, varid=%d) from file (%s, ncid=%d) failed . Saving the I/O decomposition (ioid=%d) failed, unable to create a unique file name for saving the decomposition", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, ioid);
        }
        LOG((2, "Saving decomp map (read) to %s", filename));
        PIOc_writemap(filename, ioid, iodesc->ndims, iodesc->dimlen, iodesc->maplen, iodesc->map, ios->my_comm);
        iodesc->is_saved = true;
    }
#endif
    /* Call the correct darray read function based on iotype. */
    if(!ios->async || ios->ioproc)
    {
        switch (file->iotype)
        {
        case PIO_IOTYPE_NETCDF:
        case PIO_IOTYPE_NETCDF4C:
            if ((ierr = pio_read_darray_nc_serial(file, fndims, iodesc, varid, iobuf)))
            {
                return pio_err(ios, file, ierr, __FILE__, __LINE__,
                                "Reading variable (%s, varid=%d) from file (%s, ncid=%d) failed . Reading variable in serial (iotype=%s) failed", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, pio_iotype_to_string(file->iotype));
            }
            break;
        case PIO_IOTYPE_PNETCDF:
        case PIO_IOTYPE_NETCDF4P:
            if ((ierr = pio_read_darray_nc(file, fndims, iodesc, varid, iobuf)))
            {
                return pio_err(ios, file, ierr, __FILE__, __LINE__,
                                "Reading variable (%s, varid=%d) from file (%s, ncid=%d) failed . Reading variable in parallel (iotype=%s) failed", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, pio_iotype_to_string(file->iotype));
            }
            break;
        default:
            return pio_err(NULL, NULL, PIO_EBADIOTYPE, __FILE__, __LINE__,
                             "Reading variable (%s, varid=%d) from file (%s, ncid=%d) failed . Invalid iotype (%d) provided", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid, file->iotype);
        }
    }

#ifdef PIO_MICRO_TIMING
    mtimer_start(file->varlist[varid].rd_rearr_mtimer);
#endif
    /* Rearrange the data. */
    if ((ierr = rearrange_io2comp(ios, iodesc, iobuf, array)))
    {
        return pio_err(ios, file, ierr, __FILE__, __LINE__,
                         "Reading variable (%s, varid=%d) from file (%s, ncid=%d) failed . Rearranging data read in the I/O processes to compute processes failed", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), file->pio_ncid);
    }

#ifdef PIO_MICRO_TIMING
    mtimer_stop(file->varlist[varid].rd_rearr_mtimer, get_var_desc_str(ncid, varid, NULL));
#endif
    /* We don't use non-blocking reads */
    file->varlist[varid].rb_pend = 0;
    file->rb_pend = 0;

    /* Free the buffer. */
    if (rlen > 0)
        brel(iobuf);

#ifdef PIO_MICRO_TIMING
    mtimer_stop(file->varlist[varid].rd_mtimer, get_var_desc_str(ncid, varid, NULL));
#endif
#ifdef TIMING
    GPTLstop("PIO:PIOc_read_darray");
#endif
    return PIO_NOERR;
}
