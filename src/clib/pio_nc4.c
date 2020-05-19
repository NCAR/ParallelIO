/** @file
 *
 * Functions to wrap netCDF-4 functions for PIO.
 *
 * @author Ed Hartnett
 */
#include <pio_config.h>
#include <pio.h>
#include <pio_internal.h>

/**
 * Set deflate (zlib) settings for a variable.
 *
 * This function only applies to netCDF-4 files. When used with netCDF
 * classic files, the error PIO_ENOTNC4 will be returned.
 *
 * See the <a
 * href="http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html">netCDF
 * variable documentation</a> for details about the operation of this
 * function.
 *
 * @param ncid the ncid of the open file.
 * @param varid the ID of the variable.
 * @param shuffle non-zero to turn on shuffle filter (can be good for
 * integer data).
 * @param deflate non-zero to turn on zlib compression for this
 * variable.
 * @param deflate_level 1 to 9, with 1 being faster and 9 being more
 * compressed.
 * @return PIO_NOERR for success, otherwise an error code.
 * @ingroup PIO_def_var
 * @author Ed Hartnett
 */
int PIOc_def_var_deflate(int ncid, int varid, int shuffle, int deflate,
                         int deflate_level)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;  /* Return code from function calls. */

    /* Get the file info. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Defining variable compression/deflate parameters for variable (varid=%d) failed on file (ncid=%d). Unable to query the internal file structure associated with the file", varid, ncid);
    }
    ios = file->iosystem;

    /* Only netCDF-4 files can use this feature. */
    if (file->iotype != PIO_IOTYPE_NETCDF4P && file->iotype != PIO_IOTYPE_NETCDF4C)
    {
        return pio_err(ios, file, PIO_ENOTNC4, __FILE__, __LINE__,
                        "Defining variable compression/deflate parameters for variable %s (varid=%d) failed on file %s (ncid=%d). Unable to define parameters on a non-NetCDF file. Variable compression is only available for NetCDF4 files", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid);
    }

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_DEF_VAR_DEFLATE;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, shuffle, deflate, deflate_level);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Defining variable compression/deflate parameters for variable %s (varid=%d) failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_DEF_VAR_DEFLATE, on iosystem (iosysid=%d)", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    if (ios->ioproc)
    {
#ifdef _NETCDF4
        if (file->iotype == PIO_IOTYPE_NETCDF4P)
            ierr = NC_EINVAL;
        else
        {
            if (file->do_io)
            {
                /* In NetCDF 4.7.4 and later releases, to set a new deflate level, deflation
                   needs to be turned off first to unset existing deflate level */
                ierr = nc_def_var_deflate(file->fh, varid, 0, 0, 1);
                if (ierr == PIO_NOERR)
                    ierr = nc_def_var_deflate(file->fh, varid, shuffle, deflate, deflate_level);
            }
        }
#endif
    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc_def_var_deflate failed, ierr = %d", ierr));
        return ierr;
    }

    return PIO_NOERR;
}

/**
 * This function only applies to netCDF-4 files. When used with netCDF
 * classic files, the error PIO_ENOTNC4 will be returned.
 *
 * Inquire about deflate (zlib compression) settings for a variable.
 *
 * See the <a
 * href="http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html">netCDF
 * variable documentation</a> for details about the operation of this
 * function.
 *
 * @param ncid the ncid of the open file.
 * @param varid the ID of the variable to set chunksizes for.
 * @param shufflep pointer to an int that will get the status of the
 * shuffle filter. Ignored if NULL.
 * @param deflatep pointer to an int that will be set to non-zero if
 * deflation is in use for this variable. Ignored if NULL.
 * @param deflate_levelp pointer to an int that will get the deflation
 * level (from 1-9) if deflation is in use for this variable.  Ignored
 * if NULL.
 * @return PIO_NOERR for success, otherwise an error code.
 * @ingroup PIO_inq_var
 * @author Ed Hartnett
 */
int PIOc_inq_var_deflate(int ncid, int varid, int *shufflep, int *deflatep,
                         int *deflate_levelp)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */

    /* Get the file info. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring variable compression/deflate parameters for variable (varid=%d) failed on file (ncid=%d). Unable to query the internal file structure associated with the file", varid, ncid);
    }
    ios = file->iosystem;

    /* Only netCDF-4 files can use this feature. */
    if (file->iotype != PIO_IOTYPE_NETCDF4P && file->iotype != PIO_IOTYPE_NETCDF4C)
    {
        return pio_err(ios, file, PIO_ENOTNC4, __FILE__, __LINE__,
                        "Inquiring variable compression/deflate parameters for variable %s (varid=%d) failed on file %s (ncid=%d). Unable to query parameters on a non-NetCDF file. Variable compression is only available for NetCDF4 files", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid);
    }

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_INQ_VAR_DEFLATE;
        char shuffle_present = shufflep ? true : false;
        char deflate_present = deflatep ? true : false;
        char deflate_level_present = deflate_levelp ? true : false;

        int amsg_shuffle = (shuffle_present) ? (*shufflep) : 0;
        int amsg_deflate = (deflate_present) ? (*deflatep) : 0;
        int amsg_deflate_level = (deflate_level_present) ? (*deflate_levelp) : 0;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid,
                            shuffle_present, amsg_shuffle,
                            deflate_present, amsg_deflate,
                            deflate_level_present, amsg_deflate_level);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Inquiring variable compression/deflate parameters for variable %s (varid=%d) failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_INQ_VAR_DEFLATE, on iosystem (iosysid=%d)", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    if (ios->ioproc)
    {
#ifdef _NETCDF4
        if (file->do_io)
            ierr = nc_inq_var_deflate(file->fh, varid, shufflep, deflatep, deflate_levelp);
#endif
    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc_inq_var_deflate failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results to all tasks. */
    if (shufflep)
        if ((mpierr = MPI_Bcast(shufflep, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    if (deflatep)
        if ((mpierr = MPI_Bcast(deflatep, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    if (deflate_levelp)
        if ((mpierr = MPI_Bcast(deflate_levelp, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    return PIO_NOERR;
}

/**
 * @ingroup PIO_def_var
 * Set chunksizes for a variable.
 *
 * This function only applies to netCDF-4 files. When used with netCDF
 * classic files, the error PIO_ENOTNC4 will be returned.
 *
 * Chunksizes have important performance repercussions. NetCDF
 * attempts to choose sensible chunk sizes by default, but for best
 * performance check chunking against access patterns.
 *
 * See the <a
 * href="http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html">netCDF
 * variable documentation</a> for details about the operation of this
 * function.
 *
 * @param ncid the ncid of the open file.
 * @param varid the ID of the variable to set chunksizes for.
 * @param storage NC_CONTIGUOUS or NC_CHUNKED.
 * @param chunksizep an array of chunksizes. Must have a chunksize for
 * every variable dimension.
 * @return PIO_NOERR for success, otherwise an error code.
 * @author Ed Hartnett
 */
int PIOc_def_var_chunking(int ncid, int varid, int storage, const PIO_Offset *chunksizesp)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ndims;             /* The number of dimensions for this var. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */

    LOG((1, "PIOc_def_var_chunking ncid = %d varid = %d storage = %d", ncid,
         varid, storage));

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Defining variable chunking parameters for variable (varid=%d) failed on file (ncid=%d). Unable to query the internal file structure associated with the file", varid, ncid);
    }
    ios = file->iosystem;

    /* Only netCDF-4 files can use this feature. */
    if (file->iotype != PIO_IOTYPE_NETCDF4P && file->iotype != PIO_IOTYPE_NETCDF4C)
    {
        return pio_err(ios, file, PIO_ENOTNC4, __FILE__, __LINE__,
                        "Defining variable chunking parameters for variable %s (varid=%d) failed on file %s (ncid=%d). Unable to define variable chunking parameters on a non-NetCDF file. This option is only available for NetCDF4 files", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid);
    }

    /* Run this on all tasks if async is not in use, but only on
     * non-IO tasks if async is in use. Get the number of
     * dimensions. */
    if(!ios->async || !ios->ioproc){
        ierr = PIOc_inq_varndims(ncid, varid, &ndims);
        if(ierr != PIO_NOERR){
            LOG((1, "PIOc_inq_varndims failed, ierr = %d", ierr));
            return ierr;
        }
    }
    LOG((2, "PIOc_def_var_chunking first ndims = %d", ndims));

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_DEF_VAR_CHUNKING;
        char chunksizes_present = chunksizesp ? true : false;
        PIO_Offset *amsg_chunksizesp = NULL;
        if(!chunksizes_present)
        {
            amsg_chunksizesp = calloc(ndims, sizeof(PIO_Offset));
        }

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, storage, ndims,
            chunksizes_present, ndims,
            (chunksizes_present) ? (chunksizesp) : amsg_chunksizesp);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Defining variable chunking parameters for variable %s (varid=%d) failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_DEF_VAR_CHUNKING, on iosystem (iosysid=%d)", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }

        if(!chunksizes_present)
        {
            free(amsg_chunksizesp);
        }

        /* Broadcast values currently only known on computation tasks to IO tasks. */
        if ((mpierr = MPI_Bcast(&ndims, 1, MPI_INT, ios->comproot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    LOG((2, "PIOc_def_var_chunking ndims = %d", ndims));

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _NETCDF4
        if (file->do_io)
        {
            size_t chunksizes_sizet[ndims];
            for (int d = 0; d < ndims; d++)
            {
                if (chunksizesp[d] < 0)
                {
                    ierr = PIO_ERANGE;
                    break;
                }
                chunksizes_sizet[d] = chunksizesp[d];
            }
            if (!ierr)
                ierr = nc_def_var_chunking(file->fh, varid, storage, chunksizes_sizet);
        }
#endif
    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc_def_var_chunking failed, ierr = %d", ierr));
        return ierr;
    }

    return PIO_NOERR;
}

/**
 * Inquire about chunksizes for a variable.
 *
 * This function only applies to netCDF-4 files. When used with netCDF
 * classic files, the error PIO_ENOTNC4 will be returned.
 *
 * See the <a
 * href="http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html">netCDF
 * variable documentation</a> for details about the operation of this
 * function.
 *
 * @param ncid the ncid of the open file.
 * @param varid the ID of the variable to set chunksizes for.
 * @param storagep pointer to int which will be set to either
 * NC_CONTIGUOUS or NC_CHUNKED.
 * @param chunksizep pointer to memory where chunksizes will be
 * set. There are the same number of chunksizes as there are
 * dimensions.
 * @return PIO_NOERR for success, otherwise an error code.
 * @ingroup PIO_inq_var
 * @author Ed Hartnett
 */
int PIOc_inq_var_chunking(int ncid, int varid, int *storagep, PIO_Offset *chunksizesp)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */
    int ndims = 0; /* The number of dimensions in the variable. */

    LOG((1, "PIOc_inq_var_chunking ncid = %d varid = %d"));

    /* Get the file info. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring variable chunking parameters for variable (varid=%d) failed on file (ncid=%d). Unable to query the internal file structure associated with the file", varid, ncid);
    }

    ios = file->iosystem;

    /* Only netCDF-4 files can use this feature. */
    if (file->iotype != PIO_IOTYPE_NETCDF4P && file->iotype != PIO_IOTYPE_NETCDF4C)
    {
        return pio_err(ios, file, PIO_ENOTNC4, __FILE__, __LINE__,
                        "Inquiring variable chunking parameters for variable %s (varid=%d) failed on file %s (ncid=%d). Unable to query variable chunking parameters on a non-NetCDF file. This option is only available for NetCDF4 files", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid);
    }

    /* Run these on all tasks if async is not in use, but only on
     * non-IO tasks if async is in use. */
    if (!ios->async || !ios->ioproc)
    {
        /* Find the number of dimensions of this variable. */
        if ((ierr = PIOc_inq_varndims(ncid, varid, &ndims)))
        {
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Inquiring variable chunking parameters for variable %s (varid=%d) failed on file %s (ncid=%d). Inquiring number of dimensions of the variable failed", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid);
        }
        LOG((2, "ndims = %d", ndims));
    }

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_INQ_VAR_CHUNKING;
        char storage_present = storagep ? true : false;
        char chunksizes_present = chunksizesp ? true : false;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid,
            storage_present, chunksizes_present);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Inquiring variable chunking parameters for variable %s (varid=%d) failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_INQ_VAR_CHUNKING, on iosystem (iosysid=%d)", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }

        /* Broadcast values currently only known on computation tasks to IO tasks. */
        if ((mpierr = MPI_Bcast(&ndims, 1, MPI_INT, ios->comproot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _NETCDF4
        if (file->do_io)
        {
            size_t chunksizes_sizet[ndims];
            ierr = nc_inq_var_chunking(file->fh, varid, storagep, chunksizes_sizet);
            if (!ierr && chunksizesp)
                for (int d = 0; d < ndims; d++)
                {
                    if (chunksizes_sizet[d] > NC_MAX_INT64)
                    {
                        ierr = PIO_ERANGE;
                        break;
                    }
                    chunksizesp[d] = chunksizes_sizet[d];
                }
        }
#endif
        LOG((2, "ierr = %d", ierr));
    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc_inq_var_chunking failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results to all tasks. */
    if ((mpierr = MPI_Bcast(&ndims, 1, MPI_INT, ios->ioroot, ios->my_comm)))
        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    if (storagep)
        if ((mpierr = MPI_Bcast(storagep, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    if (chunksizesp)
        if ((mpierr = MPI_Bcast(chunksizesp, ndims, MPI_OFFSET, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    return PIO_NOERR;
}

/**
 * Set chunksizes for a variable.
 *
 * This function only applies to netCDF-4 files. When used with netCDF
 * classic files, the error PIO_ENOTNC4 will be returned.
 *
 * See the <a
 * href="http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html">netCDF
 * variable documentation</a> for details about the operation of this
 * function.
 *
 * Chunksizes have important performance repercussions. NetCDF
 * attempts to choose sensible chunk sizes by default, but for best
 * performance check chunking against access patterns.
 *
 * @param ncid the ncid of the open file.
 * @param varid the ID of the variable to set chunksizes for.
 * @param storage NC_CONTIGUOUS or NC_CHUNKED.
 * @param chunksizep an array of chunksizes. Must have a chunksize for
 * every variable dimension.
 * @return PIO_NOERR for success, otherwise an error code.
 * @ingroup PIO_def_var
 * @author Ed Hartnett
 */
int PIOc_def_var_endian(int ncid, int varid, int endian)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */

    /* Get the file info. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Defining endianness for variable (varid=%d) failed on file (ncid=%d). Unable to query the internal file structure associated with the file", varid, ncid);
    }
    ios = file->iosystem;

    /* Only netCDF-4 files can use this feature. */
    if (file->iotype != PIO_IOTYPE_NETCDF4P && file->iotype != PIO_IOTYPE_NETCDF4C)
    {
        return pio_err(ios, file, PIO_ENOTNC4, __FILE__, __LINE__,
                        "Defining endianness for variable %s (varid=%d) failed on file %s (ncid=%d). Unable to define variable endianness on a non-NetCDF file. This option is only available for NetCDF4 files", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid);
    }

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_DEF_VAR_ENDIAN;
        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, endian);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Defining endianness variable %s (varid=%d) failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_DEF_VAR_ENDIAN, on iosystem (iosysid=%d)", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    if (ios->ioproc)
    {
#ifdef _NETCDF4
        if (file->do_io)
            ierr = nc_def_var_endian(file->fh, varid, endian);
#endif
    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc_def_var_endian failed, ierr = %d", ierr));
        return ierr;
    }

    return PIO_NOERR;
}

/**
 * Inquire about chunksizes for a variable.
 *
 * This function only applies to netCDF-4 files. When used with netCDF
 * classic files, the error PIO_ENOTNC4 will be returned.
 *
 * See the <a
 * href="http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html">netCDF
 * variable documentation</a> for details about the operation of this
 * function.
 *
 * @param ncid the ncid of the open file.
 * @param varid the ID of the variable to set chunksizes for.
 * @param endianp pointer to int which will be set to
 * endianness. Ignored if NULL.
 * @return PIO_NOERR for success, otherwise an error code.
 * @ingroup PIO_inq_var
 * @author Ed Hartnett
 */
int PIOc_inq_var_endian(int ncid, int varid, int *endianp)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */

    LOG((1, "PIOc_inq_var_endian ncid = %d varid = %d", ncid, varid));

    /* Get the file info. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring endianness for variable (varid=%d) failed on file (ncid=%d). Unable to query the internal file structure associated with the file", varid, ncid);
    }
    ios = file->iosystem;

    /* Only netCDF-4 files can use this feature. */
    if (file->iotype != PIO_IOTYPE_NETCDF4P && file->iotype != PIO_IOTYPE_NETCDF4C)
    {
        return pio_err(ios, file, PIO_ENOTNC4, __FILE__, __LINE__,
                        "Inquiring endianness for variable %s (varid=%d) failed on file %s (ncid=%d). Unable to inquire variable endianness on a non-NetCDF file. This option is only available for NetCDF4 files", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid);
    }

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_INQ_VAR_ENDIAN;
        char endian_present = endianp ? true : false;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, endian_present);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Inquiring endianness of variable %s (varid=%d) failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_INQ_VAR_ENDIAN, on iosystem (iosysid=%d)", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _NETCDF4
        if (file->do_io)
            ierr = nc_inq_var_endian(file->fh, varid, endianp);
#endif
    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc_inq_var_endian failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results to all tasks. */
    if (endianp)
        if ((mpierr = MPI_Bcast(endianp, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    return PIO_NOERR;
}

/**
 * Set chunk cache netCDF files to be opened/created.
 *
 * This function only applies to netCDF-4 files. When used with netCDF
 * classic files, the error PIO_ENOTNC4 will be returned.
 *
 * The file chunk cache for HDF5 can be set, and will apply for any
 * files opened or created until the program ends, or the settings are
 * changed again. The cache settings apply only to the open file. They
 * do not persist with the file, and must be set each time the file is
 * opened, before it is opened, if they are to have effect.
 *
 * See the <a
 * href="http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html">netCDF
 * variable documentation</a> for details about the operation of this
 * function.
 *
 * @param iotype the iotype of files to be created or opened.
 * @param size size of file cache.
 * @param nelems number of elements in file cache.
 * @param preemption preemption setting for file cache.
 * @return PIO_NOERR for success, otherwise an error code.
 * @ingroup PIO_def_var
 * @author Ed Hartnett
 */
int PIOc_set_chunk_cache(int iosysid, int iotype, PIO_Offset size, PIO_Offset nelems,
                         float preemption)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */

    LOG((1, "PIOc_set_chunk_cache iosysid = %d iotype = %d size = %d nelems = %d preemption = %g",
         iosysid, iotype, size, nelems, preemption));

    /* Get the IO system info. */
    if (!(ios = pio_get_iosystem_from_id(iosysid)))
    {
        return pio_err(NULL, NULL, PIO_EBADID, __FILE__, __LINE__,
                        "Setting cache chunk parameters failed. Invalid iosystem (iosysid=%d) provided", iosysid);
    }

    /* Only netCDF-4 files can use this feature. */
    if (iotype != PIO_IOTYPE_NETCDF4P && iotype != PIO_IOTYPE_NETCDF4C)
    {
        return pio_err(ios, NULL, PIO_ENOTNC4, __FILE__, __LINE__,
                        "Setting cache chunk parameters failed. Unable to set cache chunk parameters on a non-NetCDF4 iotype. The usage is only supported for NetCDF4 iotypes");
    }

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_SET_CHUNK_CACHE; /* Message for async notification. */

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, iosysid, iotype, size,
            nelems, preemption);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Setting cache chunk parameters failed. Unable to send asynchronous message, PIO_MSG_SET_CHUNK_CACHE, on iosystem (iosysid=%d)", iosysid);
        }
    }

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _NETCDF4
        LOG((2, "calling nc_chunk_cache"));
        if (iotype == PIO_IOTYPE_NETCDF4P)
            ierr = nc_set_chunk_cache(size, nelems, preemption);
        else
            if (!ios->io_rank)
                ierr = nc_set_chunk_cache(size, nelems, preemption);
#endif
    }

    ierr = check_netcdf(ios, NULL, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc_set_chunk_cache failed, ierr = %d", ierr));
        return ierr;
    }

    LOG((2, "PIOc_set_chunk_cache complete!"));
    return PIO_NOERR;
}

/**
 * Get current file chunk cache settings from HDF5.
 *
 * This function has no effect on netCDF classic files. Calling this
 * function with iotype of PIO_IOTYPE_PNETCDF or PIO_IOTYPE_NETCDF
 * returns an error.
 *
 * The file chunk cache for HDF5 can be set, and will apply for any
 * files opened or created until the program ends, or the settings are
 * changed again. The cache settings apply only to the open file. They
 * do not persist with the file, and must be set each time the file is
 * opened, before it is opened, if they are to have effect.
 *
 * See the <a
 * href="http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html">netCDF
 * variable documentation</a> for details about the operation of this
 * function.
 *
 * Chunksizes have important performance repercussions. NetCDF
 * attempts to choose sensible chunk sizes by default, but for best
 * performance check chunking against access patterns.
 *
 * @param iotype the iotype of files to be created or opened.
 * @param sizep gets the size of file cache.
 * @param nelemsp gets the number of elements in file cache.
 * @param preemptionp gets the preemption setting for file cache.
 * @return PIO_NOERR for success, otherwise an error code.
 * @ingroup PIO_def_var
 * @author Ed Hartnett
 */
int PIOc_get_chunk_cache(int iosysid, int iotype, PIO_Offset *sizep, PIO_Offset *nelemsp,
                         float *preemptionp)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */

    LOG((1, "PIOc_get_chunk_cache iosysid = %d iotype = %d", iosysid, iotype));

    /* Get the io system info. */
    if (!(ios = pio_get_iosystem_from_id(iosysid)))
    {
        return pio_err(NULL, NULL, PIO_EBADID, __FILE__, __LINE__,
                        "Getting cache chunk parameters failed. Invalid iosystem (iosysid=%d) provided", iosysid);
    }

    /* Only netCDF-4 files can use this feature. */
    if (iotype != PIO_IOTYPE_NETCDF4P && iotype != PIO_IOTYPE_NETCDF4C)
    {
        return pio_err(ios, NULL, PIO_ENOTNC4, __FILE__, __LINE__,
                        "Getting cache chunk parameters failed. Unable to set cache chunk parameters on a non-NetCDF4 iotype. The usage is only supported for NetCDF4 iotypes");
    }

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_GET_CHUNK_CACHE; /* Message for async notification. */
        char size_present = sizep ? true : false;
        char nelems_present = nelemsp ? true : false;
        char preemption_present = preemptionp ? true : false;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, iosysid, iotype, size_present,
            nelems_present, preemption_present);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Getting cache chunk parameters failed. Unable to send asynchronous message, PIO_MSG_GET_CHUNK_CACHE, on iosystem (iosysid=%d)", iosysid);
        }
    }

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _NETCDF4
        if (iotype == PIO_IOTYPE_NETCDF4P)
            ierr = nc_get_chunk_cache((size_t *)sizep, (size_t *)nelemsp, preemptionp);
        else
            if (!ios->io_rank)
                ierr = nc_get_chunk_cache((size_t *)sizep, (size_t *)nelemsp, preemptionp);
#endif
        LOG((2, "nc_get_chunk_cache called ierr = %d", ierr));
    }

    ierr = check_netcdf(ios, NULL, ierr, __FILE__, __LINE__);
    LOG((2, "bcast complete ierr = %d sizep = %d", ierr, sizep));
    if(ierr != PIO_NOERR){
        LOG((1, "nc_get_chunk_cache failed, ierr = %d", ierr));
        return ierr;
    }

    if (sizep)
    {
        LOG((2, "bcasting size = %d ios->ioroot = %d", *sizep, ios->ioroot));
        if ((mpierr = MPI_Bcast(sizep, 1, MPI_OFFSET, ios->ioroot, ios->my_comm)))
            return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
        LOG((2, "bcast size = %d", *sizep));
    }
    if (nelemsp)
    {
        if ((mpierr = MPI_Bcast(nelemsp, 1, MPI_OFFSET, ios->ioroot, ios->my_comm)))
            return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
        LOG((2, "bcast complete nelems = %d", *nelemsp));
    }
    if (preemptionp)
    {
        if ((mpierr = MPI_Bcast(preemptionp, 1, MPI_FLOAT, ios->ioroot, ios->my_comm)))
            return check_mpi(ios, NULL, mpierr, __FILE__, __LINE__);
        LOG((2, "bcast complete preemption = %d", *preemptionp));
    }

    return PIO_NOERR;
}

/**
 * Set chunksizes for a variable.
 *
 * This function only applies to netCDF-4 files. When used with netCDF
 * classic files, the error PIO_ENOTNC4 will be returned.
 *
 * See the <a
 * href="http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html">netCDF
 * variable documentation</a> for details about the operation of this
 * function.
 *
 * Chunksizes have important performance repercussions. NetCDF
 * attempts to choose sensible chunk sizes by default, but for best
 * performance check chunking against access patterns.
 *
 * @param ncid the ncid of the open file.
 * @param varid the ID of the variable to set chunksizes for.
 * @param storage NC_CONTIGUOUS or NC_CHUNKED.
 * @param chunksizep an array of chunksizes. Must have a chunksize for
 * every variable dimension.
 * @return PIO_NOERR for success, otherwise an error code.
 * @ingroup PIO_def_var
 * @author Ed Hartnett
 */
int PIOc_set_var_chunk_cache(int ncid, int varid, PIO_Offset size, PIO_Offset nelems,
                             float preemption)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */

    /* Get the file info. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Setting chunk cache for variable (varid=%d) failed on file (ncid=%d). Unable to query the internal file structure associated with the file", varid, ncid);
    }
    ios = file->iosystem;

    /* Only netCDF-4 files can use this feature. */
    if (file->iotype != PIO_IOTYPE_NETCDF4P && file->iotype != PIO_IOTYPE_NETCDF4C)
    {
        return pio_err(ios, file, PIO_ENOTNC4, __FILE__, __LINE__,
                        "Setting chunk cache for variable %s (varid=%d) failed on file %s (ncid=%d). This option is only available for variables in NetCDF4 files", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid);
    }

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_SET_VAR_CHUNK_CACHE;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, size, nelems, preemption);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, file, PIO_ENOTNC4, __FILE__, __LINE__,
                            "Setting chunk cache for variable %s (varid=%d) failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_SET_VAR_CHUNK_CACHE on iosystem (iosysid=%d)", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    if (ios->ioproc)
    {
#ifdef _NETCDF4
        if (file->do_io)
            ierr = nc_set_var_chunk_cache(file->fh, varid, size, nelems, preemption);
#endif
    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc_set_var_chunk_cache failed, ierr = %d", ierr));
        return ierr;
    }

    return PIO_NOERR;
}

/**
 * Get the variable chunk cache settings.
 *
 * This function only applies to netCDF-4 files. When used with netCDF
 * classic files, the error PIO_ENOTNC4 will be returned.
 *
 * Note that these settings are not part of the data file - they apply
 * only to the open file as long as it is open.
 *
 *  See the <a
 * href="http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html">netCDF
 * variable documentation</a> for details about the operation of this
 * function.
 *
 * @param ncid the ncid of the open file.
 * @param varid the ID of the variable to set chunksizes for.
 * @param sizep will get the size of the cache in bytes. Ignored if NULL.
 * @param nelemsp will get the number of elements in the cache. Ignored if NULL.
 * @param preemptionp will get the cache preemption value. Ignored if NULL.
 * @return PIO_NOERR for success, otherwise an error code.
 * @ingroup PIO_inq_var
 * @author Ed Hartnett
 */
int PIOc_get_var_chunk_cache(int ncid, int varid, PIO_Offset *sizep, PIO_Offset *nelemsp,
                             float *preemptionp)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */

    LOG((1, "PIOc_get_var_chunk_cache ncid = %d varid = %d"));

    /* Get the file info. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Getting chunk cache for variable (varid=%d) failed on file (ncid=%d). Unable to query the internal file structure associated with the file", varid, ncid);
    }
    ios = file->iosystem;

    /* Only netCDF-4 files can use this feature. */
    if (file->iotype != PIO_IOTYPE_NETCDF4P && file->iotype != PIO_IOTYPE_NETCDF4C)
    {
        return pio_err(ios, file, PIO_ENOTNC4, __FILE__, __LINE__,
                        "Getting chunk cache for variable %s (varid=%d) failed on file %s (ncid=%d). This option is only available for variables in NetCDF4 files", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid);
    }

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_GET_VAR_CHUNK_CACHE; /* Message for async notification. */
        char size_present = sizep ? true : false;
        char nelems_present = nelemsp ? true : false;
        char preemption_present = preemptionp ? true : false;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, size_present,
            nelems_present, preemption_present);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, file, PIO_ENOTNC4, __FILE__, __LINE__,
                            "Getting chunk cache for variable %s (varid=%d) failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_GET_VAR_CHUNK_CACHE on iosystem (iosysid=%d)", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _NETCDF4
        if (file->do_io)
            ierr = nc_get_var_chunk_cache(file->fh, varid, (size_t *)sizep, (size_t *)nelemsp,
                                          preemptionp);
#endif
    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc_get_var_chunk_cache failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results to all tasks. */
    if (sizep && !ierr)
        if ((mpierr = MPI_Bcast(sizep, 1, MPI_OFFSET, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    if (nelemsp && !ierr)
        if ((mpierr = MPI_Bcast(nelemsp, 1, MPI_OFFSET, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    if (preemptionp && !ierr)
        if ((mpierr = MPI_Bcast(preemptionp, 1, MPI_FLOAT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    return PIO_NOERR;
}
