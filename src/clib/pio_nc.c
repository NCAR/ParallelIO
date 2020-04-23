/**
 * @file
 * PIO interfaces to
 * [NetCDF](http://www.unidata.ucar.edu/software/netcdf/docs/modules.html)
 * support functions
 *
 *  This file provides an interface to the
 *  [NetCDF](http://www.unidata.ucar.edu/software/netcdf/docs/modules.html)
 *  support functions.  Each subroutine calls the underlying netcdf or
 *  pnetcdf or netcdf4 functions from the appropriate subset of mpi
 *  tasks (io_comm). Each routine must be called collectively from
 *  union_comm.
 *
 * @author Jim Edwards (jedwards@ucar.edu), Ed Hartnett
 * @date     Feburary 2014, April 2016
 */
#include <pio_config.h>
#include <pio.h>
#include <pio_internal.h>
#ifdef PIO_MICRO_TIMING
#include "pio_timer.h"
#endif

#ifdef _ADIOS2
int adios2_type_size(adios2_type type, const void *var)
{
    switch (type)
    {
        case adios2_type_int8_t:
        case adios2_type_uint8_t:
            return 1;

        case adios2_type_string:
            if (!var)
                return 1;
            else
                return strlen((const char *)var) + 1;

        case adios2_type_int16_t:
        case adios2_type_uint16_t:
            return 2;

        case adios2_type_int32_t:
        case adios2_type_uint32_t:
            return 4;

        case adios2_type_int64_t:
        case adios2_type_uint64_t:
            return 8;

        case adios2_type_float:
            return 4;

        case adios2_type_double:
            return 8;

        case adios2_type_float_complex:
            return 2 * 4;

        case adios2_type_double_complex:
            return 2 * 8;

        default:
            return -1;
    }
}
#endif

/**
 * @ingroup PIO_inq
 * The PIO-C interface for the NetCDF function nc_inq.
 *
 * This routine is called collectively by all tasks in the
 * communicator ios.union_comm. For more information on the underlying
 * NetCDF commmand please read about this function in the NetCDF
 * documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__datasets.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 *
 * @return PIO_NOERR for success, error code otherwise. See
 * PIOc_Set_File_Error_Handling
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq(int ncid, int *ndimsp, int *nvarsp, int *ngattsp, int *unlimdimidp)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function calls. */

    LOG((1, "PIOc_inq ncid = %d", ncid));

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring information about file (ncid=%d) failed. Invalid file id. Unable to find internal structure assocaited with the file id", ncid);
    }
    ios = file->iosystem;

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_INQ; /* Message for async notification. */
        char ndims_present = ndimsp ? true : false;
        char nvars_present = nvarsp ? true : false;
        char ngatts_present = ngattsp ? true : false;
        char unlimdimid_present = unlimdimidp ? true : false;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, ndims_present, nvars_present,
            ngatts_present, unlimdimid_present);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Inquiring information about file %s (ncid=%d) failed. Unable to send asynchronous message, PIO_MSG_INQ on iosystem (iosysid=%d)", pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        if (ndimsp)
            *ndimsp = file->num_dim_vars;

        if (nvarsp)
            *nvarsp = file->num_vars;

        if (ngattsp)
            *ngattsp = file->num_gattrs;

        if (unlimdimidp)
        {
            *unlimdimidp = -1;
            for (int i = 0; i < file->num_dim_vars; ++i)
            {
                if (file->dim_values[i] == PIO_UNLIMITED)
                    *unlimdimidp = i;
            }
        }

        return PIO_NOERR;
     }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
        {
            ierr = ncmpi_inq(file->fh, ndimsp, nvarsp, ngattsp, unlimdimidp);
            if (unlimdimidp)
                LOG((2, "PIOc_inq returned from ncmpi_inq unlimdimid = %d", *unlimdimidp));
        }
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype == PIO_IOTYPE_NETCDF && file->do_io)
        {
            LOG((2, "PIOc_inq calling classic nc_inq"));
            /* Should not be necessary to do this - nc_inq should
             * handle null pointers. This has been reported as a bug
             * to netCDF developers. */
            int tmp_ndims, tmp_nvars, tmp_ngatts, tmp_unlimdimid;
            LOG((2, "PIOc_inq calling classic nc_inq"));
            ierr = nc_inq(file->fh, &tmp_ndims, &tmp_nvars, &tmp_ngatts, &tmp_unlimdimid);
            LOG((2, "PIOc_inq calling classic nc_inq"));
            if (unlimdimidp)
                LOG((2, "classic tmp_unlimdimid = %d", tmp_unlimdimid));
            if (ndimsp)
                *ndimsp = tmp_ndims;
            if (nvarsp)
                *nvarsp = tmp_nvars;
            if (ngattsp)
                *ngattsp = tmp_ngatts;
            if (unlimdimidp)
                *unlimdimidp = tmp_unlimdimid;
            if (unlimdimidp)
                LOG((2, "classic unlimdimid = %d", *unlimdimidp));
        }
#ifdef _NETCDF4
        else if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
        {
            LOG((2, "PIOc_inq calling netcdf-4 nc_inq"));
            ierr = nc_inq(file->fh, ndimsp, nvarsp, ngattsp, unlimdimidp);
        }
#endif /* _NETCDF4 */
#endif /* _NETCDF */

        LOG((2, "PIOc_inq netcdf call returned %d", ierr));
    }

    /* A failure to inquire is not fatal */
    mpierr = MPI_Bcast(&ierr, 1, MPI_INT, ios->ioroot, ios->my_comm);
    if(mpierr != MPI_SUCCESS){
        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    if(ierr != PIO_NOERR){
        LOG((1, "nc*_inq failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results to all tasks. Ignore NULL parameters. */
    if (ndimsp)
        if ((mpierr = MPI_Bcast(ndimsp, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    if (nvarsp)
        if ((mpierr = MPI_Bcast(nvarsp, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    if (ngattsp)
        if ((mpierr = MPI_Bcast(ngattsp, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    if (unlimdimidp)
        if ((mpierr = MPI_Bcast(unlimdimidp, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    return PIO_NOERR;
}

/**
 * @ingroup PIO_inq_ndims
 * Find out how many dimensions are defined in the file.
 *
 * @param ncid the ncid of the open file.
 * @param ndimsp a pointer that will get the number of dimensions.
 * @returns 0 for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_ndims(int ncid, int *ndimsp)
{
    LOG((1, "PIOc_inq_ndims"));
    return PIOc_inq(ncid, ndimsp, NULL, NULL, NULL);
}

/**
 * @ingroup PIO_inq_nvars
 * Find out how many variables are defined in a file.
 *
 * @param ncid the ncid of the open file.
 * @param nvarsp a pointer that will get the number of variables.
 * @returns 0 for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_nvars(int ncid, int *nvarsp)
{
    return PIOc_inq(ncid, NULL, nvarsp, NULL, NULL);
}

/**
 * @ingroup PIO_inq_natts
 * Find out how many global attributes are defined in a file.
 *
 * @param ncid the ncid of the open file.
 * @param nattsp a pointer that will get the number of attributes.
 * @returns 0 for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_natts(int ncid, int *ngattsp)
{
    return PIOc_inq(ncid, NULL, NULL, ngattsp, NULL);
}

/**
 * @ingroup PIO_inq_unlimdim
 * Find out the dimension ids of the unlimited dimension.
 *
 * @param ncid the ncid of the open file.
 * @param unlimdimidp a pointer that will the ID of the unlimited
 * dimension, or -1 if there is no unlimited dimension.
 * @returns 0 for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_unlimdim(int ncid, int *unlimdimidp)
{
    LOG((1, "PIOc_inq_unlimdim ncid = %d", ncid));
    return PIOc_inq(ncid, NULL, NULL, NULL, unlimdimidp);
}

/**
 * Find out the dimension ids of all unlimited dimensions. Note that
 * only netCDF-4 files can have more than 1 unlimited dimension.
 *
 * @param ncid the ncid of the open file.
 * @param nunlimdimsp a pointer that gets the number of unlimited
 * dimensions. Ignored if NULL.
 * @param unlimdimidsp a pointer that will get an array of unlimited
 * dimension IDs.
 * @returns 0 for success, error code otherwise.
 * @ingroup PIO_inq_unlimdim
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_unlimdims(int ncid, int *nunlimdimsp, int *unlimdimidsp)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int tmp_nunlimdims;    /* The number of unlimited dims. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function calls. */

    LOG((1, "PIOc_inq_unlimdims ncid = %d", ncid));

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring unlimited dimension information failed on file (ncid=%d). Invalid file id. Unable to find internal structure associated with the file id", ncid);
    }
    ios = file->iosystem;

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_INQ_UNLIMDIMS; /* Message for async notification. */
        char nunlimdimsp_present = nunlimdimsp ? true : false;
        char unlimdimidsp_present = unlimdimidsp ? true : false;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid,
            nunlimdimsp_present, unlimdimidsp_present);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Inquiring unlimited dimension information on file %s (ncid=%d) failed. Unable to send asynchronous message, PIO_MSG_INQ_UNLIMDIMS on iosystem (iosysid=%d)", pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    LOG((2, "file->iotype = %d", file->iotype));
    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
        if (file->iotype == PIO_IOTYPE_NETCDF && file->do_io)
        {
#ifdef _NETCDF
            LOG((2, "netcdf"));
            int tmp_unlimdimid;
            ierr = nc_inq_unlimdim(file->fh, &tmp_unlimdimid);
            LOG((2, "classic tmp_unlimdimid = %d", tmp_unlimdimid));
            tmp_nunlimdims = tmp_unlimdimid >= 0 ? 1 : 0;
            if (nunlimdimsp)
                *nunlimdimsp = tmp_unlimdimid >= 0 ? 1 : 0;
            if (unlimdimidsp)
                *unlimdimidsp = tmp_unlimdimid;
#endif /* _NETCDF */
        }
#ifdef _PNETCDF
        else if (file->iotype == PIO_IOTYPE_PNETCDF)
        {
            LOG((2, "pnetcdf"));
            int tmp_unlimdimid;            
            ierr = ncmpi_inq_unlimdim(file->fh, &tmp_unlimdimid);
            LOG((2, "pnetcdf tmp_unlimdimid = %d", tmp_unlimdimid));
            tmp_nunlimdims = tmp_unlimdimid >= 0 ? 1 : 0;            
            if (nunlimdimsp)
                *nunlimdimsp = tmp_nunlimdims;
            if (unlimdimidsp)
                *unlimdimidsp = tmp_unlimdimid;
        }
#endif /* _PNETCDF */
#ifdef _NETCDF4
        else if ((file->iotype == PIO_IOTYPE_NETCDF4C || file->iotype == PIO_IOTYPE_NETCDF4P) &&
                 file->do_io)
        {
            LOG((2, "PIOc_inq calling netcdf-4 nc_inq_unlimdims"));
            int *tmp_unlimdimids;
            ierr = nc_inq_unlimdims(file->fh, &tmp_nunlimdims, NULL);
            if (!ierr)
            {
                if (nunlimdimsp)
                    *nunlimdimsp = tmp_nunlimdims;
                LOG((3, "tmp_nunlimdims = %d", tmp_nunlimdims));
                if (!(tmp_unlimdimids = malloc(tmp_nunlimdims * sizeof(int))))
                    ierr = PIO_ENOMEM;
                if (!ierr)
                    ierr = nc_inq_unlimdims(file->fh, &tmp_nunlimdims, tmp_unlimdimids);
                if (unlimdimidsp)
                    for (int d = 0; d < tmp_nunlimdims; d++)
                    {
                        LOG((3, "tmp_unlimdimids[%d] = %d", d, tmp_unlimdimids[d]));
                        unlimdimidsp[d] = tmp_unlimdimids[d];
                    }
                free(tmp_unlimdimids);
            }
        }
#endif /* _NETCDF4 */

        LOG((2, "PIOc_inq_unlimdims netcdf call returned %d", ierr));
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        LOG((2, "ADIOS missing %s:%s", __FILE__, __func__));
        ierr = PIO_NOERR;
    }
#endif

    /* A failure to inquire is not fatal */
    mpierr = MPI_Bcast(&ierr, 1, MPI_INT, ios->ioroot, ios->my_comm);
    if(mpierr != MPI_SUCCESS){
        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    if(ierr != PIO_NOERR){
        LOG((1, "nc*_inq_unlimdims failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results to all tasks. Ignore NULL parameters. */
    if ((mpierr = MPI_Bcast(&tmp_nunlimdims, 1, MPI_INT, ios->ioroot, ios->my_comm)))
        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    
    if (nunlimdimsp)
        if ((mpierr = MPI_Bcast(nunlimdimsp, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    if (unlimdimidsp)
        if ((mpierr = MPI_Bcast(unlimdimidsp, tmp_nunlimdims, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    return PIO_NOERR;
}

/**
 * @ingroup PIO_typelen
 * Learn the name and size of a type.
 *
 * @param ncid the ncid of the open file.
 * @param xtype the type to learn about
 * @param name pointer that will get the name of the type.
 * @param sizep pointer that will get the size of the type in bytes.
 * @returns 0 for success, error code otherwise.
 * @author Ed Hartnett
 */
int PIOc_inq_type(int ncid, nc_type xtype, char *name, PIO_Offset *sizep)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */

    LOG((1, "PIOc_inq_type ncid = %d xtype = %d", ncid, xtype));

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring type information failed on file (ncid=%d). Invalid file id. Unable to find internal structure associated with the file id", ncid);
    }
    ios = file->iosystem;

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_INQ_TYPE; /* Message for async notification. */
        char name_present = name ? true : false;
        char size_present = sizep ? true : false;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, xtype, name_present, size_present);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Inquiring type information on file %s (ncid=%d) failed. Unable to send asynchronous message, PIO_MSG_INQ_TYPE on iosystem (iosysid=%d)", pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        if (sizep)
        {
            adios2_type atype = PIOc_get_adios_type(xtype);
            int asize = adios2_type_size(atype, NULL);
            *sizep = (PIO_Offset) asize;
        }

        return PIO_NOERR;
    }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
            ierr = pioc_pnetcdf_inq_type(ncid, xtype, name, sizep);
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
            ierr = nc_inq_type(file->fh, xtype, name, (size_t *)sizep);
#endif /* _NETCDF */
        LOG((2, "PIOc_inq_type netcdf call returned %d", ierr));
    }

    /* Failure to inquire is fatal */
    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc*_inq_type failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results to all tasks. Ignore NULL parameters. */
    if (name)
    {
        int slen;
        if (ios->iomaster == MPI_ROOT)
            slen = strlen(name);
        if ((mpierr = MPI_Bcast(&slen, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
        if (!mpierr)
            if ((mpierr = MPI_Bcast((void *)name, slen + 1, MPI_CHAR, ios->ioroot, ios->my_comm)))
                return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }
    if (sizep)
        if ((mpierr = MPI_Bcast(sizep , 1, MPI_OFFSET, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    return PIO_NOERR;
}

/**
 * @ingroup PIO_inq_format
 * Learn the netCDF format of an open file.
 *
 * @param ncid the ncid of an open file.
 * @param formatp a pointer that will get the format.
 * @returns 0 for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_format(int ncid, int *formatp)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */

    LOG((1, "PIOc_inq ncid = %d", ncid));

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring format failed on file (ncid=%d). Invalid fild id. Unable to find internal structure associated with the file id", ncid);
    }
    ios = file->iosystem;

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_INQ_FORMAT;
        char format_present = formatp ? true : false;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, format_present);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Inquiring format of file %s (ncid=%d) failed. Unable to send asynchronous message, PIO_MSG_INQ_FORMAT, on iosystem (iosysid=%d)", pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        *formatp = 1;
        return PIO_NOERR;
    }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
            ierr = ncmpi_inq_format(file->fh, formatp);
#endif /* _PNETCDF */

#ifdef _ADIOS2
        if (file->iotype == PIO_IOTYPE_ADIOS)
        {
            LOG((2, "ADIOS missing %s:%s", __FILE__, __func__));
            ierr = PIO_NOERR;
        }
#endif

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
            ierr = nc_inq_format(file->fh, formatp);
#endif /* _NETCDF */
        LOG((2, "PIOc_inq netcdf call returned %d", ierr));
    }

    /* Failure to inquire is fatal */
    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc*_inq_format failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results to all tasks. Ignore NULL parameters. */
    if (formatp)
        if ((mpierr = MPI_Bcast(formatp , 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    return PIO_NOERR;
}

/**
 * @ingroup PIO_inq_dim
 * The PIO-C interface for the NetCDF function nc_inq_dim.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__dimensions.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param lenp a pointer that will get the number of values
 * @return PIO_NOERR for success, error code otherwise.  See PIOc_Set_File_Error_Handling
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_dim(int ncid, int dimid, char *name, PIO_Offset *lenp)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */

    LOG((1, "PIOc_inq_dim ncid = %d dimid = %d", ncid, dimid));

    /* Get the file info, based on the ncid. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring dimension (dimid=%d) information failed on file (ncid=%d). Invalid file id. Unable to find internal structure associated with the file id", dimid, ncid);
    }
    ios = file->iosystem;

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_INQ_DIM;
        char name_present = name ? true : false;
        char len_present = lenp ? true : false;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, dimid, name_present, len_present);
        if(ierr != PIO_NOERR)
        {
            const char *dname = (name) ? name : "UNKNOWN";
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Inquiring information about dimension %s (dimid=%d) failed on file %s (ncid=%d) failed. Unable to send asynchronous message, PIO_MSG_INQ_DIM, on iosystem (iosysid=%d)", dname, dimid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        if (0 <= dimid && dimid < file->num_dim_vars)
        {
            if (name)
                strcpy(name, file->dim_names[dimid]);

            if (lenp)
                *lenp = file->dim_values[dimid];

            ierr = PIO_NOERR;
        }
        else
        {
            for (int i = 0; i < file->num_dim_vars; i++)
            {
                printf("%s", file->dim_names[i]);
                if (i < file->num_dim_vars - 1)
                    printf(", ");
            }
            printf("\n");

            if (name)
                name[0] = '\0';

            if (lenp)
                *lenp = 0;

            ierr = PIO_EBADDIM;
        }

        return ierr;
    }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
        {
            LOG((2, "calling ncmpi_inq_dim"));
            ierr = ncmpi_inq_dim(file->fh, dimid, name, lenp);;
        }
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
        {
            LOG((2, "calling nc_inq_dim"));
            ierr = nc_inq_dim(file->fh, dimid, name, (size_t *)lenp);;
        }
#endif /* _NETCDF */
        LOG((2, "ierr = %d", ierr));
    }

    /* A failure to inquire is not fatal */
    mpierr = MPI_Bcast(&ierr, 1, MPI_INT, ios->ioroot, ios->my_comm);
    if(mpierr != MPI_SUCCESS){
        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    if(ierr != PIO_NOERR){
        LOG((1, "nc*_inq_dim failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results to all tasks. Ignore NULL parameters. */
    if (name)
    {
        int slen;
        LOG((2, "bcasting results my_comm = %d", ios->my_comm));
        if (ios->iomaster == MPI_ROOT)
            slen = strlen(name);
        if ((mpierr = MPI_Bcast(&slen, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
        if ((mpierr = MPI_Bcast((void *)name, slen + 1, MPI_CHAR, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    if (lenp)
        if ((mpierr = MPI_Bcast(lenp , 1, MPI_OFFSET, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    LOG((2, "done with PIOc_inq_dim"));
    return PIO_NOERR;
}

/**
 * @ingroup PIO_inq_dimname
 * Find the name of a dimension.
 *
 * @param ncid the ncid of an open file.
 * @param dimid the dimension ID.
 * @param name a pointer that gets the name of the dimension. Igorned
 * if NULL.
 * @returns 0 for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_dimname(int ncid, int dimid, char *name)
{
    LOG((1, "PIOc_inq_dimname ncid = %d dimid = %d", ncid, dimid));
    return PIOc_inq_dim(ncid, dimid, name, NULL);
}

/**
 * @ingroup PIO_inq_dimlen
 * Find the length of a dimension.
 *
 * @param ncid the ncid of an open file.
 * @param dimid the dimension ID.
 * @param lenp a pointer that gets the length of the dimension. Igorned
 * if NULL.
 * @returns 0 for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_dimlen(int ncid, int dimid, PIO_Offset *lenp)
{
    return PIOc_inq_dim(ncid, dimid, NULL, lenp);
}

/**
 * @ingroup PIO_inq_dimid
 * The PIO-C interface for the NetCDF function nc_inq_dimid.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__dimensions.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param idp a pointer that will get the id of the variable or attribute.
 * @return PIO_NOERR for success, error code otherwise.  See PIOc_Set_File_Error_Handling
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_dimid(int ncid, const char *name, int *idp)
{
    iosystem_desc_t *ios;
    file_desc_t *file;
    int ierr = PIO_NOERR;
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */

    /* Get the file info, based on the ncid. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        const char *dname = (name) ? name : "UNKNOWN";
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring id of dimension %s failed on file (ncid=%d). Invalid file id. Unable to find internal structure associated with the file id", dname, ncid);
    }
    ios = file->iosystem;
    LOG((2, "iosysid = %d", ios->iosysid));

    /* User must provide name shorter than PIO_MAX_NAME +1. */
    if (!name || strlen(name) > PIO_MAX_NAME)
    {
        const char *str_null_msg = "The specified dimension name pointer is NULL";
        const char *str_too_long_msg = "The specified dimension name is too long (> PIO_MAX_NAME chars)";
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Inquiring id of dimension failed on file %s (ncid=%d). %s", pio_get_fname_from_file(file), ncid, (!name) ? str_null_msg : str_too_long_msg);
    }

    LOG((1, "PIOc_inq_dimid ncid = %d name = %s", ncid, name));

    /* If using async, and not an IO task, then send parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_INQ_DIMID;
        char id_present = idp ? true : false;
        int namelen = strlen(name) + 1;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, namelen, name, id_present);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                            "Inquiring id of dimension %s failed on file %s (ncid=%d) failed. Unable to send asynchronous message, PIO_MSG_INQ_DIMID, on iosystem (iosysid=%d)", name, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        ierr = PIO_EBADDIM;
        for (int i = 0; i < file->num_dim_vars; i++)
        {
            if (!strcmp(name, file->dim_names[i]))
            {
                *idp = i;
                ierr = PIO_NOERR;
                break;
            }
        }

        if (ierr == PIO_EBADDIM)
        {
            for (int i = 0; i < file->num_dim_vars; i++)
            {
                printf("%s", file->dim_names[i]);
                if (i < file->num_dim_vars - 1)
                    printf(", ");
            }
            printf("\n");
        }

        return ierr;
    }
#endif
 
    /* IO tasks call the netCDF functions. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
            ierr = ncmpi_inq_dimid(file->fh, name, idp);
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
            ierr = nc_inq_dimid(file->fh, name, idp);
#endif /* _NETCDF */
    }
    LOG((3, "nc_inq_dimid call complete ierr = %d", ierr));

    /* A failure to inquire is not fatal */
    mpierr = MPI_Bcast(&ierr, 1, MPI_INT, ios->ioroot, ios->my_comm);
    if(mpierr != MPI_SUCCESS){
        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    if(ierr != PIO_NOERR){
        LOG((1, "nc*_inq_dimid failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results. */
    if (idp)
        if ((mpierr = MPI_Bcast(idp, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    return PIO_NOERR;
}

/**
 * @ingroup PIO_inq_var
 * The PIO-C interface for the NetCDF function nc_inq_var.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param xtypep a pointer that will get the type of the attribute.
 * @param nattsp a pointer that will get the number of attributes
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_var(int ncid, int varid, char *name, int namelen, nc_type *xtypep, int *ndimsp,
                 int *dimidsp, int *nattsp)
{
    iosystem_desc_t *ios;
    file_desc_t *file;
    int ndims = 0;    /* The number of dimensions for this variable. */
    char my_name[PIO_MAX_NAME + 1];
    int slen;
    int ierr = PIO_NOERR;
#ifdef PIO_MICRO_TIMING
    char timer_log_fname[PIO_MAX_NAME];
#endif
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */

    LOG((1, "PIOc_inq_var ncid = %d varid = %d", ncid, varid));

    /* Get the file info, based on the ncid. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring information on variable (varid=%d) failed on file (ncid=%d). Invalid file id. Unable to find internal structure associated with the file id", varid, ncid);
    }
    ios = file->iosystem;

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_INQ_VAR;
        char name_present = name ? true : false;
        char xtype_present = xtypep ? true : false;
        char ndims_present = ndimsp ? true : false;
        char dimids_present = dimidsp ? true : false;
        char natts_present = nattsp ? true : false;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, name_present,
            xtype_present, ndims_present, dimids_present, natts_present);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Inquiring information of variable %s (varid=%d) failed on file %s (ncid=%d) failed. Unable to send asynchronous message, PIO_MSG_INQ_VAR, on iosystem (iosysid=%d)", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        if (varid < file->num_vars)
        {
            if (name)
                strcpy(name, file->adios_vars[varid].name);

            if (xtypep)
                *xtypep = file->adios_vars[varid].nc_type;

            if (ndimsp)
                *ndimsp = file->adios_vars[varid].ndims;

            if (dimidsp)
                memcpy(dimidsp, file->adios_vars[varid].gdimids,
                       file->adios_vars[varid].ndims * sizeof(int));

            if (nattsp)
                *nattsp = file->adios_vars[varid].nattrs;

            ierr = PIO_NOERR;
        }
        else
            ierr = PIO_EBADID;

        /* Copied and modified from MPI_Bcast lines */
        if (name && namelen > 0)
        {
            assert(namelen <= PIO_MAX_NAME + 1);
            strncpy(name, my_name, namelen);
        }

        strncpy(file->varlist[varid].vname, my_name, PIO_MAX_NAME);

        return ierr;
    }
#endif

    /* Call the netCDF layer. */
    if (ios->ioproc)
    {
        LOG((2, "Calling the netCDF layer"));
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
        {
            int *tmp_dimidsp = NULL;
            ierr = ncmpi_inq_varndims(file->fh, varid, &ndims);
            LOG((2, "from pnetcdf ndims = %d", ndims));
            if(!ierr && (!dimidsp) && (file->num_unlim_dimids > 0))
            {
                tmp_dimidsp = (int *)malloc(ndims * sizeof(int));
                if(!tmp_dimidsp)
                {
                    return pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                                    "Inquiring information of variable %s (varid=%d) failed on file %s (ncid=%d) failed. Out of memory allocating %lld bytes for storing dimension ids", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, (unsigned long long) (ndims * sizeof(int)));
                }
            }
            if (!ierr)
                ierr = ncmpi_inq_var(file->fh, varid, my_name, xtypep, ndimsp, (dimidsp)?dimidsp:tmp_dimidsp, nattsp);
            if (!ierr && name && namelen > 0)
            {
                assert(namelen <= PIO_MAX_NAME + 1);
                strncpy(name, my_name, namelen);
            }
            if(!ierr && (file->num_unlim_dimids > 0))
            {
                int *p = (dimidsp) ? dimidsp : tmp_dimidsp;
                int is_rec_var = file->varlist[varid].rec_var;
                for(int i=0; (i<ndims) && (!is_rec_var); i++)
                {
                    for(int j=0; (j<file->num_unlim_dimids) && (!is_rec_var); j++)
                    {
                        if(p[i] == file->unlim_dimids[j])
                        {
                            is_rec_var = 1;
                        }
                    }
                }
                file->varlist[varid].rec_var = is_rec_var;
            }
            free(tmp_dimidsp);
        }
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
        {
            ierr = nc_inq_varndims(file->fh, varid, &ndims);
            LOG((3, "nc_inq_varndims called ndims = %d", ndims));
            if (!ierr)
            {
                nc_type my_xtype;
                int my_ndims = 0, my_dimids[ndims], my_natts = 0;
                ierr = nc_inq_var(file->fh, varid, my_name, &my_xtype, &my_ndims, my_dimids, &my_natts);
                LOG((3, "my_name = %s my_xtype = %d my_ndims = %d my_natts = %d",  my_name, my_xtype, my_ndims, my_natts));
                if (!ierr)
                {
                    if (name)
                        strcpy(name, my_name);
                    if (xtypep)
                        *xtypep = my_xtype;
                    if (ndimsp)
                        *ndimsp = my_ndims;
                    if (dimidsp)
                    {
                        for (int d = 0; d < ndims; d++)
                            dimidsp[d] = my_dimids[d];
                    }
                    if (nattsp)
                        *nattsp = my_natts;

                    if(file->num_unlim_dimids > 0)
                    {
                        int is_rec_var = file->varlist[varid].rec_var;
                        for(int i=0; (i<ndims) && (!is_rec_var); i++)
                        {
                            for(int j=0; (j<file->num_unlim_dimids) && (!is_rec_var); j++)
                            {
                                if(my_dimids[i] == file->unlim_dimids[j])
                                {
                                    is_rec_var = 1;
                                }
                            }
                        }
                        file->varlist[varid].rec_var = is_rec_var;
                    }
                }
            }
        }
#endif /* _NETCDF */
        if (ndimsp)
            LOG((2, "PIOc_inq_var ndims = %d ierr = %d", *ndimsp, ierr));
    }

    /* A failure to inquire is not fatal */
    mpierr = MPI_Bcast(&ierr, 1, MPI_INT, ios->ioroot, ios->my_comm);
    if(mpierr != MPI_SUCCESS){
        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    if(ierr != PIO_NOERR){
        LOG((1, "nc*_inq_var failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast the results for non-null pointers. */
    if (ios->iomaster == MPI_ROOT)
        slen = strlen(my_name);
    if ((mpierr = MPI_Bcast(&slen, 1, MPI_INT, ios->ioroot, ios->my_comm)))
        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    if ((mpierr = MPI_Bcast((void *)my_name, slen + 1, MPI_CHAR, ios->ioroot, ios->my_comm)))
        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    if (name && namelen > 0)
    {
        assert(namelen <= PIO_MAX_NAME + 1);
        strncpy(name, my_name, namelen);
    }
    strncpy(file->varlist[varid].vname, my_name, PIO_MAX_NAME);

#ifdef PIO_MICRO_TIMING
    /* Create timers for the variable
      * - Assuming that we don't reuse varids 
      * - Also assuming that a timer is needed if we query about a var
      * */
    snprintf(timer_log_fname, PIO_MAX_NAME, "piorwinfo%010dwrank.dat", ios->ioroot);
    if(!mtimer_is_valid(file->varlist[varid].rd_mtimer))
    {
        char tmp_timer_name[PIO_MAX_NAME];
        snprintf(tmp_timer_name, PIO_MAX_NAME, "%s_%s", "rd", file->varlist[varid].vname);
        file->varlist[varid].rd_mtimer = mtimer_create(tmp_timer_name, ios->my_comm, timer_log_fname);
        if(!mtimer_is_valid(file->varlist[varid].rd_mtimer))
        {
            return pio_err(ios, file, PIO_EINTERNAL, __FILE__, __LINE__,
                            "Inquiring information of variable %s (varid=%d) failed on file %s (ncid=%d) failed. Error creating micro timer (read) for variable", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid);
        }
        assert(!mtimer_is_valid(file->varlist[varid].rd_rearr_mtimer));
        snprintf(tmp_timer_name, PIO_MAX_NAME, "%s_%s", "rd_rearr", file->varlist[varid].vname);
        file->varlist[varid].rd_rearr_mtimer = mtimer_create(tmp_timer_name, ios->my_comm, timer_log_fname);
        if(!mtimer_is_valid(file->varlist[varid].rd_rearr_mtimer))
        {
            return pio_err(ios, file, PIO_EINTERNAL, __FILE__, __LINE__,
                            "Inquiring information of variable %s (varid=%d) failed on file %s (ncid=%d) failed. Error creating micro timer (read rearrange) for variable", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid);
        }
        snprintf(tmp_timer_name, PIO_MAX_NAME, "%s_%s", "wr", file->varlist[varid].vname);
        file->varlist[varid].wr_mtimer = mtimer_create(tmp_timer_name, ios->my_comm, timer_log_fname);
        if(!mtimer_is_valid(file->varlist[varid].wr_mtimer))
        {
            return pio_err(ios, file, PIO_EINTERNAL, __FILE__, __LINE__,
                            "Inquiring information of variable %s (varid=%d) failed on file %s (ncid=%d) failed. Error creating micro timer (write) for variable", pio_get_fname_from_file(file), varid, pio_get_fname_from_file(file), ncid);
        }
        assert(!mtimer_is_valid(file->varlist[varid].wr_rearr_mtimer));
        snprintf(tmp_timer_name, PIO_MAX_NAME, "%s_%s", "wr_rearr", file->varlist[varid].vname);
        file->varlist[varid].wr_rearr_mtimer = mtimer_create(tmp_timer_name, ios->my_comm, timer_log_fname);
        if(!mtimer_is_valid(file->varlist[varid].wr_rearr_mtimer))
        {
            return pio_err(ios, file, PIO_EINTERNAL, __FILE__, __LINE__,
                            "Inquiring information of variable %s (varid=%d) failed on file %s (ncid=%d) failed. Error creating micro timer (write rearrange) for variable", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid);
        }
    }
#endif

    if ((mpierr = MPI_Bcast(&(file->varlist[varid].rec_var), 1, MPI_INT, ios->ioroot, ios->my_comm)))
        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    if (xtypep)
        if ((mpierr = MPI_Bcast(xtypep, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    if (ndimsp)
    {
        LOG((2, "PIOc_inq_var about to Bcast ndims = %d ios->ioroot = %d ios->my_comm = %d",
             *ndimsp, ios->ioroot, ios->my_comm));
        if ((mpierr = MPI_Bcast(ndimsp, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
        LOG((2, "PIOc_inq_var Bcast ndims = %d", *ndimsp));
    }
    if (dimidsp)
    {
        if ((mpierr = MPI_Bcast(&ndims, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
        if ((mpierr = MPI_Bcast(dimidsp, ndims, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }
    if (nattsp)
        if ((mpierr = MPI_Bcast(nattsp, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    return PIO_NOERR;
}

/**
 * @ingroup PIO_inq_varname
 * Get the name of a variable.
 *
 * @param ncid the ncid of the open file.
 * @param varid the variable ID.
 * @param name a pointer that will get the variable name.
 * @param namelen the size of the user buffer pointed by name.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_varname(int ncid, int varid, char *name, int namelen)
{
    return PIOc_inq_var(ncid, varid, name, namelen, NULL, NULL, NULL, NULL);
}

/**
 * @ingroup PIO_inq_vartype
 * Find the type of a variable.
 *
 * @param ncid the ncid of the open file.
 * @param varid the variable ID.
 * @param xtypep a pointer that will get the type of the
 * attribute. Ignored if NULL.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_vartype(int ncid, int varid, nc_type *xtypep)
{
    return PIOc_inq_var(ncid, varid, NULL, 0, xtypep, NULL, NULL, NULL);
}

/**
 * @ingroup PIO_inq_varndims
 * Find the number of dimensions of a variable.
 *
 * @param ncid the ncid of the open file.
 * @param varid the variable ID.
 * @param ndimsp a pointer that will get the number of
 * dimensions. Ignored if NULL.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_varndims(int ncid, int varid, int *ndimsp)
{
    return PIOc_inq_var(ncid, varid, NULL, 0, NULL, ndimsp, NULL, NULL);
}

/**
 * @ingroup PIO_inq_vardimid
 * Find the dimension IDs associated with a variable.
 *
 * @param ncid the ncid of the open file.
 * @param varid the variable ID.
 * @param dimidsp a pointer that will get an array of dimids. Ignored
 * if NULL.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_vardimid(int ncid, int varid, int *dimidsp)
{
    return PIOc_inq_var(ncid, varid, NULL, 0, NULL, NULL, dimidsp, NULL);
}

/**
 * @ingroup PIO_inq_varnatts
 * Find the number of attributes associated with a variable.
 *
 * @param ncid the ncid of the open file.
 * @param varid the variable ID.
 * @param nattsp a pointer that will get the number of attriburtes. Ignored
 * if NULL.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_varnatts(int ncid, int varid, int *nattsp)
{
    return PIOc_inq_var(ncid, varid, NULL, 0, NULL, NULL, NULL, nattsp);
}

/**
 * @ingroup PIO_inq_varid
 * The PIO-C interface for the NetCDF function nc_inq_varid.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param varidp a pointer that will get the variable id
 * @return PIO_NOERR for success, error code otherwise.  See PIOc_Set_File_Error_Handling
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_varid(int ncid, const char *name, int *varidp)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
#ifdef PIO_MICRO_TIMING
    char timer_log_fname[PIO_MAX_NAME];
#endif
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */

    /* Get file info based on ncid. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        const char *vname = (name) ? name : "UNKNOWN";
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring id for variable %s failed on file (ncid=%d). Invalid file id. Unable to find internal structure associated with the file id", vname, ncid);
    }
    ios = file->iosystem;

    /* Caller must provide name. */
    if (!name || strlen(name) > PIO_MAX_NAME)
    {
        const char *vname = (name) ? name : "UNKNOWN";
        const char *err_msg = (!name) ? "The pointer to variable name is NULL" : "The length of variable name exceeds PIO_MAX_NAME";
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Inquiring id for variable %s failed on file %s (ncid=%d). %s", vname, pio_get_fname_from_file(file), ncid, err_msg);
    }

    LOG((1, "PIOc_inq_varid ncid = %d name = %s", ncid, name));

    if (ios->async)
    {
        int msg = PIO_MSG_INQ_VARID;
        int namelen = strlen(name) + 1;
        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, namelen, name);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring id for variable %s failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_INQ_VARID, on iosystem (iosysid=%d)", name, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        ierr = PIO_ENOTVAR;
        for (int i = 0; i < file->num_vars; i++)
        {
            if (!strcmp(name, file->adios_vars[i].name))
            {
                *varidp = i;
                ierr = PIO_NOERR;
                break;
            }
        }

        return ierr;
    }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
            ierr = ncmpi_inq_varid(file->fh, name, varidp);
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
            ierr = nc_inq_varid(file->fh, name, varidp);
#endif /* _NETCDF */
    }

    /* A failure to inquire is not fatal */
    mpierr = MPI_Bcast(&ierr, 1, MPI_INT, ios->ioroot, ios->my_comm);
    if(mpierr != MPI_SUCCESS){
        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    if(ierr != PIO_NOERR){
        LOG((1, "nc*_inq_varid failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results to all tasks. Ignore NULL parameters. */
    if (varidp)
        if ((mpierr = MPI_Bcast(varidp, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

#ifdef PIO_MICRO_TIMING
    /* Create timers for the variable
      * - Assuming that we don't reuse varids
      * - Also assuming that a timer is needed if we query about a var
      * */
    snprintf(timer_log_fname, PIO_MAX_NAME, "piorwinfo%010dwrank.dat", ios->ioroot);
    if(!mtimer_is_valid(file->varlist[*varidp].rd_mtimer))
    {
        char tmp_timer_name[PIO_MAX_NAME];
        snprintf(tmp_timer_name, PIO_MAX_NAME, "%s_%s", "rd", name);
        file->varlist[*varidp].rd_mtimer = mtimer_create(tmp_timer_name, ios->my_comm, timer_log_fname);
        if(!mtimer_is_valid(file->varlist[*varidp].rd_mtimer))
        {
            return pio_err(ios, file, PIO_EINTERNAL, __FILE__, __LINE__,
                            "Inquiring id for variable %s failed on file %s (ncid=%d). Unable to create micro timers (read) for variable", name, pio_get_fname_from_file(file), ncid);
        }
        assert(!mtimer_is_valid(file->varlist[*varidp].rd_rearr_mtimer));
        snprintf(tmp_timer_name, PIO_MAX_NAME, "%s_%s", "rd_rearr", name);
        file->varlist[*varidp].rd_rearr_mtimer = mtimer_create(tmp_timer_name, ios->my_comm, timer_log_fname);
        if(!mtimer_is_valid(file->varlist[*varidp].rd_rearr_mtimer))
        {
            return pio_err(ios, file, PIO_EINTERNAL, __FILE__, __LINE__,
                            "Inquiring id for variable %s failed on file %s (ncid=%d). Unable to create micro timers (read rearrange) for variable", name, pio_get_fname_from_file(file), ncid);
        }
        snprintf(tmp_timer_name, PIO_MAX_NAME, "%s_%s", "wr", name);
        file->varlist[*varidp].wr_mtimer = mtimer_create(tmp_timer_name, ios->my_comm, timer_log_fname);
        if(!mtimer_is_valid(file->varlist[*varidp].wr_mtimer))
        {
            return pio_err(ios, file, PIO_EINTERNAL, __FILE__, __LINE__,
                            "Inquiring id for variable %s failed on file %s (ncid=%d). Unable to create micro timers (write) for variable", name, pio_get_fname_from_file(file), ncid);
        }
        assert(!mtimer_is_valid(file->varlist[*varidp].wr_rearr_mtimer));
        snprintf(tmp_timer_name, PIO_MAX_NAME, "%s_%s", "wr_rearr", name);
        file->varlist[*varidp].wr_rearr_mtimer = mtimer_create(tmp_timer_name, ios->my_comm, timer_log_fname);
        if(!mtimer_is_valid(file->varlist[*varidp].wr_rearr_mtimer))
        {
            return pio_err(ios, file, PIO_EINTERNAL, __FILE__, __LINE__,
                            "Inquiring id for variable %s failed on file %s (ncid=%d). Unable to create micro timers (write rearrange) for variable", name, pio_get_fname_from_file(file), ncid);
        }
    }
#endif
    return PIO_NOERR;
}

/**
 * @ingroup PIO_inq_att
 * The PIO-C interface for the NetCDF function nc_inq_att.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__attributes.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param xtypep a pointer that will get the type of the attribute.
 * @param lenp a pointer that will get the number of values
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_att(int ncid, int varid, const char *name, nc_type *xtypep,
                 PIO_Offset *lenp)
{
    int msg = PIO_MSG_INQ_ATT;
    iosystem_desc_t *ios;
    file_desc_t *file;
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */
    int ierr = PIO_NOERR;

    /* Find file based on ncid. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        const char *aname = (name) ? name : "UNKNOWN";
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring attribute (%s) associated with variable (varid=%d) failed on file (ncid=%d). Unable to query internal structure associated with the file id", aname, varid, ncid);
    }
    ios = file->iosystem;

    /* User must provide name shorter than PIO_MAX_NAME +1. */
    if (!name || strlen(name) > PIO_MAX_NAME)
    {
        const char *aname = (name) ? name : "UNKNOWN";
        const char *err_msg = (!name) ? "The pointer to attribute name is NULL" : "The length of attribute name exceeds PIO_MAX_NAME";
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Inquiring info for attribute, %s, associated with variable %s (varid=%d) failed on file %s (ncid=%d). %s", aname, pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, err_msg);
    }

    LOG((1, "PIOc_inq_att ncid = %d varid = %d", ncid, varid));

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        bool xtype_present = (xtypep) ? true : false;
        bool len_present = (lenp) ? true : false;
        int namelen = strlen(name) + 1;
        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, namelen, name, xtype_present, len_present);
        if(ierr != PIO_NOERR)
        {
            const char *aname = (name) ? name : "UNKNOWN";
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Inquiring info for attribute, %s, associated with variable %s (varid=%d) failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_INQ_ATT, on iosystem (iosysid=%d)", aname, pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        } 
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        /* LOG((2, "ADIOS missing %s:%s", __FILE__, __func__)); */
        /* Track attributes */
        ierr = PIO_ENOTATT;
        for (int i = 0; i < file->num_attrs; i++)
        {
            if (!strcmp(name, file->adios_attrs[i].att_name) &&
                file->adios_attrs[i].att_varid == varid &&
                file->adios_attrs[i].att_ncid == ncid)
            {
                ierr = PIO_NOERR;
                *xtypep = (nc_type) (file->adios_attrs[i].att_type);
                *lenp = (PIO_Offset) (file->adios_attrs[i].att_len);
                i = file->num_attrs + 1;
            }
        }

        return ierr;
    }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
            ierr = ncmpi_inq_att(file->fh, varid, name, xtypep, lenp);
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
            ierr = nc_inq_att(file->fh, varid, name, xtypep, (size_t *)lenp);
#endif /* _NETCDF */
        LOG((2, "PIOc_inq netcdf call returned %d", ierr));
    }

    /* A failure to inquire is not fatal */
    mpierr = MPI_Bcast(&ierr, 1, MPI_INT, ios->ioroot, ios->my_comm);
    if(mpierr != MPI_SUCCESS){
        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    if(ierr != PIO_NOERR){
        LOG((1, "nc*_inq_att failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results. */
    if (xtypep)
        if ((mpierr = MPI_Bcast(xtypep, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    if (lenp)
        if ((mpierr = MPI_Bcast(lenp, 1, MPI_OFFSET, ios->ioroot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    return PIO_NOERR;
}

/**
 * @ingroup PIO_inq_attlen
 * Get the length of an attribute.
 *
 * @param ncid the ID of an open file.
 * @param varid the variable ID, or NC_GLOBAL for global attributes.
 * @param name the name of the attribute.
 * @param lenp a pointer that gets the lenght of the attribute
 * array. Ignored if NULL.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_attlen(int ncid, int varid, const char *name, PIO_Offset *lenp)
{
    return PIOc_inq_att(ncid, varid, name, NULL, lenp);
}

/**
 * @ingroup PIO_inq_atttype
 * Get the type of an attribute.
 *
 * @param ncid the ID of an open file.
 * @param varid the variable ID, or NC_GLOBAL for global attributes.
 * @param name the name of the attribute.
 * @param xtypep a pointer that gets the type of the
 * attribute. Ignored if NULL.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_atttype(int ncid, int varid, const char *name, nc_type *xtypep)
{
    return PIOc_inq_att(ncid, varid, name, xtypep, NULL);
}

/**
 * @ingroup PIO_inq_attname
 * The PIO-C interface for the NetCDF function nc_inq_attname.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__attributes.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param attnum the attribute ID.
 * @return PIO_NOERR for success, error code otherwise.  See PIOc_Set_File_Error_Handling
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_attname(int ncid, int varid, int attnum, char *name)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */

    LOG((1, "PIOc_inq_attname ncid = %d varid = %d attnum = %d", ncid, varid,
         attnum));

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring name of attribute with id=%d associated with variable (varid=%d) on file (ncid%d) failed. Unable to inquire internal structure associated with the file id", attnum, varid, ncid);
    }
    ios = file->iosystem;

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_INQ_ATTNAME;
        char name_present = name ? true : false;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, attnum, name_present);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Inquiring name of attribute with id=%d associated with variable %s (varid=%d) on file %s (ncid%d) failed. Unable to send asynchronous message, PIO_MSG_INQ_ATTNAME, on iosystem (iosysid=%d)", attnum, pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        LOG((2, "ADIOS missing %s:%s", __FILE__, __func__));
        ierr = PIO_NOERR;
    }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
            ierr = ncmpi_inq_attname(file->fh, varid, attnum, name);
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
            ierr = nc_inq_attname(file->fh, varid, attnum, name);
#endif /* _NETCDF */
        LOG((2, "PIOc_inq_attname netcdf call returned %d", ierr));
    }

    /* Failure to inquire is fatal */
    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc*_inq_attname failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results to all tasks. Ignore NULL parameters. */
    if (name)
    {
        int namelen = strlen(name);
        if ((mpierr = MPI_Bcast(&namelen, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
        /* Casting to void to avoid warnings on some compilers. */
        if ((mpierr = MPI_Bcast((void *)name, namelen + 1, MPI_CHAR, ios->ioroot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    return PIO_NOERR;
}

/**
 * @ingroup PIO_inq_attid
 * The PIO-C interface for the NetCDF function nc_inq_attid.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__attributes.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param idp a pointer that will get the id of the variable or attribute.
 * @return PIO_NOERR for success, error code otherwise.  See PIOc_Set_File_Error_Handling
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_attid(int ncid, int varid, const char *name, int *idp)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        const char *aname = (name) ? name : "UNKNOWN";
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring id of attribute %s associated with variable (varid=%d) on file (ncid%d) failed. Unable to inquire internal structure associated with the file id", aname, varid, ncid);
    }
    ios = file->iosystem;

    /* User must provide name shorter than PIO_MAX_NAME +1. */
    if (!name || strlen(name) > PIO_MAX_NAME)
    {
        const char *aname = (name) ? name : "UNKNOWN";
        const char *err_msg = (!name) ? "The pointer to attribute name is NULL" : "The length of attribute name exceeds PIO_MAX_NAME";
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Inquiring id for attribute, %s, associated with variable %s (varid=%d) failed on file %s (ncid=%d). %s", aname, pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, err_msg);
    }

    LOG((1, "PIOc_inq_attid ncid = %d varid = %d name = %s", ncid, varid, name));

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_INQ_ATTID;
        int namelen = strlen(name) + 1;
        char id_present = idp ? true : false;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, namelen, name, id_present);
        if(ierr != PIO_NOERR)
        {
            const char *aname = (name) ? name : "UNKNOWN";
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Inquiring id for attribute, %s, associated with variable %s (varid=%d) failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_INQ_ATTID, on iosystem (iosysid=%d)", aname, pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        LOG((2, "ADIOS missing %s:%s", __FILE__, __func__));
        ierr = PIO_NOERR;
    }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
            ierr = ncmpi_inq_attid(file->fh, varid, name, idp);
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
            ierr = nc_inq_attid(file->fh, varid, name, idp);
#endif /* _NETCDF */
        LOG((2, "PIOc_inq_attname netcdf call returned %d", ierr));
    }

    /* A failure to inquire is not fatal */
    mpierr = MPI_Bcast(&ierr, 1, MPI_INT, ios->ioroot, ios->my_comm);
    if(mpierr != MPI_SUCCESS){
        return check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    if(ierr != PIO_NOERR){
        LOG((1, "nc*_inq_attid failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results. */
    if (idp)
        if ((mpierr = MPI_Bcast(idp, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    return PIO_NOERR;
}

/**
 * @ingroup PIO_rename_dim
 * The PIO-C interface for the NetCDF function nc_rename_dim.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__dimensions.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @return PIO_NOERR for success, error code otherwise.  See PIOc_Set_File_Error_Handling
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_rename_dim(int ncid, int dimid, const char *name)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        const char *dname = (name) ? name : "UNKNOWN";
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Renaming dimension (dimid=%d) to %s failed on file (ncid=%d). Unable to inquire internal structure associated with the file id", dimid, dname, ncid);
    }
    ios = file->iosystem;

    /* User must provide name shorter than PIO_MAX_NAME +1. */
    if (!name || strlen(name) > PIO_MAX_NAME)
    {
        const char *dname = (name) ? name : "UNKNOWN";
        const char *err_msg = (!name) ? "The pointer to dimension name is NULL" : "The length of dimension name exceeds PIO_MAX_NAME";
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Renaming dimension (dimid=%d) to %s failed on file %s (ncid=%d). %s", dimid, dname, pio_get_fname_from_file(file), ncid, err_msg);
    }

    LOG((1, "PIOc_rename_dim ncid = %d dimid = %d name = %s", ncid, dimid, name));

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_RENAME_DIM; /* Message for async notification. */
        int namelen = strlen(name) + 1;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, dimid, namelen, name);
        if(ierr != PIO_NOERR)
        {
            const char *dname = (name) ? name : "UNKNOWN";
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Renaming dimension (dimid=%d) to %s failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_RENAME_DIM, on iosystem (iosysid=%d)", dimid, dname, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        LOG((2, "ADIOS missing %s:%s", __FILE__, __func__));
        ierr = PIO_NOERR;
    }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
            ierr = ncmpi_rename_dim(file->fh, dimid, name);
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
            ierr = nc_rename_dim(file->fh, dimid, name);
#endif /* _NETCDF */
        LOG((2, "PIOc_inq netcdf call returned %d", ierr));
    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc*_rename_dim failed, ierr = %d", ierr));
        return ierr;
    }

    return PIO_NOERR;
}

/**
 * @ingroup PIO_rename_var
 * The PIO-C interface for the NetCDF function nc_rename_var.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @return PIO_NOERR for success, error code otherwise.  See PIOc_Set_File_Error_Handling
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_rename_var(int ncid, int varid, const char *name)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        const char *vname = (name) ? name : "UNKNOWN";
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Renaming variable (varid=%d) to %s failed on file (ncid=%d). Unable to inquire internal structure associated with the file id", varid, vname, ncid);
    }
    ios = file->iosystem;

    /* User must provide name shorter than PIO_MAX_NAME +1. */
    if (!name || strlen(name) > PIO_MAX_NAME)
    {
        const char *vname = (name) ? name : "UNKNOWN";
        const char *err_msg = (!name) ? "The pointer to variable name is NULL" : "The length of variable name exceeds PIO_MAX_NAME";
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Renaming variable (varid=%d) to %s failed on file %s (ncid=%d). %s", varid, vname, pio_get_fname_from_file(file), ncid, err_msg);
    }

    LOG((1, "PIOc_rename_var ncid = %d varid = %d name = %s", ncid, varid, name));

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_RENAME_VAR; /* Message for async notification. */
        int namelen = strlen(name) + 1;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, namelen, name);
        if(ierr != PIO_NOERR)
        {
            const char *vname = (name) ? name : "UNKNOWN";
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Renaming variable (varid=%d) to %s failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_RENAME_VAR, on iosystem (iosysid=%d)", varid, vname, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        LOG((2, "ADIOS missing %s:%s", __FILE__, __func__));
        ierr = PIO_NOERR;
    }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
            ierr = ncmpi_rename_var(file->fh, varid, name);
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
            ierr = nc_rename_var(file->fh, varid, name);
#endif /* _NETCDF */
        LOG((2, "PIOc_inq netcdf call returned %d", ierr));
    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc*_rename_var failed, ierr = %d", ierr));
        return ierr;
    }

    return PIO_NOERR;
}

/**
 * @ingroup PIO_rename_att
 * The PIO-C interface for the NetCDF function nc_rename_att.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__attributes.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @return PIO_NOERR for success, error code otherwise.  See
 * PIOc_Set_File_Error_Handling
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_rename_att(int ncid, int varid, const char *name,
                    const char *newname)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        const char *aname = (name) ? name : "UNKNOWN";
        const char *anewname = (newname) ? newname : "UNKNOWN";
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Renaming attribute %s associated with variable (varid=%d) to %s failed on file (ncid=%d). Unable to inquire internal structure associated with the file id", aname, varid, anewname, ncid);
    }
    ios = file->iosystem;

    /* User must provide names of correct length. */
    if (!name || strlen(name) > PIO_MAX_NAME ||
        !newname || strlen(newname) > PIO_MAX_NAME)
    {
        const char *aname = (name) ? name : "UNKNOWN";
        const char *anewname = (newname) ? newname : "UNKNOWN";
        const char *err_msg_name = (!name) ? "The pointer to attribute name is NULL" : "The length of attribute name exceeds PIO_MAX_NAME";
        const char *err_msg_newname = (!newname) ? "The pointer to the new attribute name is NULL" : "The length of the new attribute name exceeds PIO_MAX_NAME";
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Renaming attribute %s associated with variable %s (varid=%d) to %s failed on file %s (ncid=%d). %s", aname, pio_get_vname_from_file(file, varid), varid, anewname, pio_get_fname_from_file(file), ncid, (!name || (strlen(name) > PIO_MAX_NAME) ? err_msg_name : err_msg_newname));
    }

    LOG((1, "PIOc_rename_att ncid = %d varid = %d name = %s newname = %s",
         ncid, varid, name, newname));

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_RENAME_ATT; /* Message for async notification. */
        int namelen = strlen(name) + 1;
        int newnamelen = strlen(newname) + 1;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, namelen, name,
          newnamelen, newname);
        if(ierr != PIO_NOERR)
        {
            const char *aname = (name) ? name : "UNKNOWN";
            const char *anewname = (newname) ? newname : "UNKNOWN";
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                        "Renaming attribute %s associated with variable %s (varid=%d) to %s failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_RENAME_ATT, on iosystem (iosysid=%d)", aname, pio_get_vname_from_file(file, varid), varid, anewname, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        LOG((2, "ADIOS missing %s:%s", __FILE__, __func__));
        ierr = PIO_NOERR;
    }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
            ierr = ncmpi_rename_att(file->fh, varid, name, newname);
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
            ierr = nc_rename_att(file->fh, varid, name, newname);
#endif /* _NETCDF */
    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc*_rename_att failed, ierr = %d", ierr));
        return ierr;
    }

    LOG((2, "PIOc_rename_att succeeded"));
    return PIO_NOERR;
}

/**
 * @ingroup PIO_del_att
 * The PIO-C interface for the NetCDF function nc_del_att.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__attributes.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name of the attribute to delete.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_del_att(int ncid, int varid, const char *name)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        const char *aname = (name) ? name : "UNKNOWN";
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Deleting attribute %s associated with variable (varid=%d) failed on file (ncid=%d). Unable to inquire internal structure associated with the file id", aname, varid, ncid);
    }
    ios = file->iosystem;

    /* User must provide name shorter than PIO_MAX_NAME +1. */
    if (!name || strlen(name) > PIO_MAX_NAME)
    {
        const char *aname = (name) ? name : "UNKNOWN";
        const char *err_msg = (!name) ? "The pointer to attribute name is NULL" : "The length of attribute name exceeds PIO_MAX_NAME";
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Deleting attribute %s associated with variable %s (varid=%d) failed on file %s (ncid=%d). %s", aname, pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, err_msg);
    }

    LOG((1, "PIOc_del_att ncid = %d varid = %d name = %s", ncid, varid, name));

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_DEL_ATT;
        int namelen = strlen(name) + 1; /* Length of name string. */

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, namelen, name);
        if(ierr != PIO_NOERR)
        {
            const char *aname = (name) ? name : "UNKNOWN";
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Deleting attribute %s associated with variable %s (varid=%d) failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_DEL_ATT, on iosystem (iosysid=%d)", aname, pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        LOG((2, "ADIOS missing %s:%s", __FILE__, __func__));
        ierr = PIO_NOERR;
    }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
            ierr = ncmpi_del_att(file->fh, varid, name);
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
            ierr = nc_del_att(file->fh, varid, name);
#endif /* _NETCDF */
    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc*_del_att failed, ierr = %d", ierr));
        return ierr;
    }

    return PIO_NOERR;
}

/**
 * The PIO-C interface for the NetCDF function nc_set_fill.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__datasets.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param fillmode either NC_FILL or NC_NOFILL.
 * @param old_modep a pointer to an int that gets the old setting.
 * @return PIO_NOERR for success, error code otherwise.
 * @ingroup PIO_set_fill
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_set_fill(int ncid, int fillmode, int *old_modep)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI functions. */

    LOG((1, "PIOc_set_fill ncid = %d fillmode = %d", ncid, fillmode));

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Setting fill mode failed on file (ncid=%d). Unable to query internal structure associated with the file id", ncid);
    }
    ios = file->iosystem;

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_SET_FILL;
        int old_modep_present = old_modep ? 1 : 0;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, fillmode, old_modep_present);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Setting fill mode failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_SET_FILL, on iosystem (iosysid=%d)", pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        if (old_modep)
            *old_modep = file->fillmode;

        file->fillmode = fillmode;

        return PIO_NOERR;
    }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
        {
            LOG((3, "about to call ncmpi_set_fill() fillmode = %d", fillmode));
            ierr = ncmpi_set_fill(file->fh, fillmode, old_modep);
        }
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
            ierr = nc_set_fill(file->fh, fillmode, old_modep);
#endif /* _NETCDF */
    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc*_set_fill failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results. */
    if (old_modep)
    {
        LOG((2, "old_mode = %d", *old_modep));
        if ((mpierr = MPI_Bcast(old_modep, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    LOG((2, "PIOc_set_fill succeeded"));
    return PIO_NOERR;
}

/**
 * @ingroup PIO_enddef
 * The PIO-C interface for the NetCDF function nc_enddef.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__datasets.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_enddef(int ncid)
{
    return pioc_change_def(ncid, 1);
}

/**
 * @ingroup PIO_redef
 * The PIO-C interface for the NetCDF function nc_redef.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__datasets.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_redef(int ncid)
{
    return pioc_change_def(ncid, 0);
}

/**
 * @ingroup PIO_def_dim
 * The PIO-C interface for the NetCDF function nc_def_dim.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__dimensions.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param idp a pointer that will get the id of the variable or attribute.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_def_dim(int ncid, const char *name, PIO_Offset len, int *idp)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */
    int tmp_id = -1;

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        const char *dname = (name) ? name : "UNKNOWN";
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Defining dimension %s in file (ncid=%d) failed. Unable to inquire internal structure associated with the file id", dname, ncid);
    }
    ios = file->iosystem;

    /* User must provide name shorter than PIO_MAX_NAME +1. */
    if (!name || strlen(name) > PIO_MAX_NAME)
    {
        const char *dname = (name) ? name : "UNKNOWN";
        const char *err_msg = (!name) ? "The pointer to dimension name is NULL" : "The length of dimension name exceeds PIO_MAX_NAME";
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Defining dimension %s in file %s (ncid=%d) failed. %s", dname, pio_get_fname_from_file(file), ncid, err_msg);
    }

    if(!idp)
    {
        idp = &tmp_id;
    }

    LOG((1, "PIOc_def_dim ncid = %d name = %s len = %d", ncid, name, len));

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_DEF_DIM;
        int namelen = strlen(name) + 1;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, namelen, name, len);
        if(ierr != PIO_NOERR)
        {
            const char *dname = (name) ? name : "UNKNOWN";
            return pio_err(ios, file, ierr, __FILE__, __LINE__,
                        "Defining dimension %s in file %s (ncid=%d) failed. Unable to send asynchronous message, PIO_MSG_DEF_DIM, on iosystem (iosysid=%d)", dname, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        LOG((2, "ADIOS define dimension %s with size %llu, id = %d",
                name, (unsigned long long)len, file->num_dim_vars));

        char dimname[PIO_MAX_NAME];
        snprintf(dimname, PIO_MAX_NAME, "/__pio__/dim/%s", name);
        adios2_variable *variableH = adios2_inquire_variable(file->ioH, dimname);
        if (variableH == NULL)
        {
            variableH = adios2_define_variable(file->ioH, dimname, adios2_type_uint64_t,
                                               0, NULL, NULL, NULL,
                                               adios2_constant_dims_false);
            if (variableH == NULL)
            {
                return pio_err(ios, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) variable (name=%s) failed for file (%s, ncid=%d)", dimname, pio_get_fname_from_file(file), file->pio_ncid);
            }
        }

        assert(file->num_dim_vars < PIO_MAX_DIMS);
        file->dim_names[file->num_dim_vars] = strdup(name);
        file->dim_values[file->num_dim_vars] = len;
        *idp = file->num_dim_vars;
        ++file->num_dim_vars;
        adios2_error adiosErr = adios2_put(file->engineH, variableH, &len, adios2_mode_sync);
        if (adiosErr != adios2_error_none)
        {
            return pio_err(ios, file, ierr, __FILE__, __LINE__, "adios2_put failed, error code = %d", adiosErr);
        }

        if (len == PIO_UNLIMITED)
        {
            file->num_unlim_dimids++;
            file->unlim_dimids = (int *)realloc(file->unlim_dimids,
                                                file->num_unlim_dimids * sizeof(int));
            if (!file->unlim_dimids)
            {
                const char *dname = (name) ? name : "UNKNOWN";
                return pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                            "Defining dimension %s in file %s (ncid=%d) failed. Out of memory allocating %lld bytes to cache unlimited dimension ids", dname, pio_get_fname_from_file(file), ncid, (unsigned long long) (file->num_unlim_dimids * sizeof(int)));
            }

            file->unlim_dimids[file->num_unlim_dimids - 1] = *idp;
            LOG((1, "pio_def_dim : %d dim is unlimited", *idp));
        }

        return PIO_NOERR;
    }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
            ierr = ncmpi_def_dim(file->fh, name, len, idp);
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
            ierr = nc_def_dim(file->fh, name, (size_t)len, idp);
#endif /* _NETCDF */
    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc*_def_dim failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results to all tasks. Ignore NULL parameters. */
    if (idp)
        if ((mpierr = MPI_Bcast(idp , 1, MPI_INT, ios->ioroot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    if(len == PIO_UNLIMITED)
    {
        file->num_unlim_dimids++;
        file->unlim_dimids = (int *)realloc(file->unlim_dimids,
                                      file->num_unlim_dimids * sizeof(int));
        if(!file->unlim_dimids)
        {
            const char *dname = (name) ? name : "UNKNOWN";
            return pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                        "Defining dimension %s in file %s (ncid=%d) failed. Out of memory allocating %lld bytes to cache unlimited dimension ids", dname, pio_get_fname_from_file(file), ncid, (unsigned long long) (file->num_unlim_dimids * sizeof(int)));
        }
        file->unlim_dimids[file->num_unlim_dimids-1] = *idp;
        LOG((1, "pio_def_dim : %d dim is unlimited", *idp));
    }

    LOG((2, "def_dim ierr = %d", ierr));
    return PIO_NOERR;
}

/**
 * The PIO-C interface for the NetCDF function nc_def_var.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param varidp a pointer that will get the variable id
 * @return PIO_NOERR for success, error code otherwise.
 * @ingroup PIO_def_var
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_def_var(int ncid, const char *name, nc_type xtype, int ndims,
                 const int *dimidsp, int *varidp)
{
    iosystem_desc_t *ios;      /* Pointer to io system information. */
    file_desc_t *file;         /* Pointer to file information. */
    int invalid_unlim_dim = 0; /* True invalid dims are used. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */
    int ierr = PIO_NOERR;                  /* Return code from function calls. */
    int ierr2 = PIO_NOERR;    /* Return code from function calls. */
#ifdef PIO_MICRO_TIMING
    char timer_log_fname[PIO_MAX_NAME];
#endif

    /* Get the file information. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        const char *vname = (name) ? name : "UNKNOWN";
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Defining variable %s in file (ncid=%d) failed. Unable to inquire internal structure associated with the file id", vname, ncid);
    }
    ios = file->iosystem;

    /* User must provide name and storage for varid. */
    if (!name || !varidp || strlen(name) > PIO_MAX_NAME)
    {
        const char *vname = (name) ? name : "UNKNOWN";
        const char *err_msg_varidp = "Invalid (NULL) pointer to buffer to return variable id";
        const char *err_msg_name = (!name) ? "The pointer to variable name is NULL" : "The length of variable name exceeds PIO_MAX_NAME";
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Defining variable %s in file %s (ncid=%d) failed. %s", vname, pio_get_fname_from_file(file), ncid, ((!varidp) ? err_msg_varidp : err_msg_name));
    }

    LOG((1, "PIOc_def_var ncid = %d name = %s xtype = %d ndims = %d", ncid, name,
         xtype, ndims));

    /* Run this on all tasks if async is not in use, but only on
     * non-IO tasks if async is in use. Learn whether each dimension
     * is unlimited. */
    if (!ios->async || !ios->ioproc)
    {
        for (int d = 1; d < ndims; d++)
        {
            PIO_Offset dimlen;
            
            ierr = PIOc_inq_dimlen(ncid, dimidsp[d], &dimlen);
            if(ierr != PIO_NOERR){
                LOG((1, "PIOc_inq_dimlen failed, ierr = %d", ierr));
                return ierr;
            }
            if (dimlen == PIO_UNLIMITED)
                invalid_unlim_dim++;
        }
    }

    /* If using async, and not an IO task, then send parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_DEF_VAR;
        int namelen = strlen(name) + 1;
        /* A place holder for scalars with ndims == 0 */
        int amsg_dimids = 0;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, namelen, name, xtype,
            ndims, (ndims > 0) ? ndims : 1, (ndims > 0) ? dimidsp : &amsg_dimids);
        if(ierr != PIO_NOERR)
        {
            const char *vname = (name) ? name : "UNKNOWN";
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Defining variable %s in file %s (ncid=%d) failed. Unable to send asynchronous message, PIO_MSG_DEF_VAR, on iosystem (iosysid=%d)", vname, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }

        /* Broadcast values currently only known on computation tasks to IO tasks. */
        if ((mpierr = MPI_Bcast(&invalid_unlim_dim, 1, MPI_INT, ios->comproot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    /* Check that only one unlimited dim is specified, and that it is
     * first. */
    if (invalid_unlim_dim)
            return PIO_EINVAL;

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        LOG((2, "ADIOS pre-define variable %s (%d dimensions, type %d)", name, ndims, xtype));

        assert(file->num_vars < PIO_MAX_VARS);
        file->adios_vars[file->num_vars].name = strdup(name);
        file->adios_vars[file->num_vars].nc_type = xtype;
        file->adios_vars[file->num_vars].adios_type = PIOc_get_adios_type(xtype);
        file->adios_vars[file->num_vars].nattrs = 0;
        file->adios_vars[file->num_vars].ndims = ndims;
        file->adios_vars[file->num_vars].adios_varid = 0;
        file->adios_vars[file->num_vars].decomp_varid = 0;
        file->adios_vars[file->num_vars].frame_varid = 0;
        file->adios_vars[file->num_vars].fillval_varid = 0;
        file->adios_vars[file->num_vars].gdimids = (int*) malloc(ndims * sizeof(int));
        if (file->adios_vars[file->num_vars].gdimids == NULL)
        {
            const char *vname = (name) ? name : "UNKNOWN";
            return pio_err(ios, file, PIO_ENOMEM, __FILE__, __LINE__,
                            "Defining variable %s in file %s (ncid=%d) using ADIOS iotype failed. Out of memory allocating %lld bytes for global dimensions", vname, pio_get_fname_from_file(file), ncid, (unsigned long long) (ndims * sizeof(int)));
        }
        memcpy(file->adios_vars[file->num_vars].gdimids, dimidsp, ndims * sizeof(int));
        *varidp = file->num_vars;
        file->num_vars++;

        /* Some codes moved from pio_darray.c */
        {
            adios_var_desc_t *av = &(file->adios_vars[*varidp]);
            if (file->adios_iomaster == MPI_ROOT)
            {
                char att_name[PIO_MAX_NAME];
                snprintf(att_name, PIO_MAX_NAME, "%s/__pio__/ndims", av->name);
                adios2_attribute *attributeH = adios2_inquire_attribute(file->ioH, att_name);
                if (attributeH == NULL)
                {
                    attributeH = adios2_define_attribute(file->ioH, att_name, adios2_type_int32_t, &av->ndims);
                    if (attributeH == NULL)
                    {
                        return pio_err(ios, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) attribute (name=%s) failed for file (%s, ncid=%d)", att_name, pio_get_fname_from_file(file), file->pio_ncid);
                    }
                }

                snprintf(att_name, PIO_MAX_NAME, "%s/__pio__/nctype", av->name);
                attributeH = adios2_inquire_attribute(file->ioH, att_name);
                if (attributeH == NULL)
                {
                    attributeH = adios2_define_attribute(file->ioH, att_name, adios2_type_int32_t, &av->nc_type);
                    if (attributeH == NULL)
                    {
                        return pio_err(ios, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) attribute (name=%s) failed for file (%s, ncid=%d)", att_name, pio_get_fname_from_file(file), file->pio_ncid);
                    }
                }

                if (av->ndims != 0) /* If zero dimensions, do not write out __pio__/dims */
                {
                    char* dimnames[PIO_MAX_DIMS];
                    assert(av->ndims <= PIO_MAX_DIMS);
                    for (int i = 0; i < av->ndims; i++)
                        dimnames[i] = file->dim_names[av->gdimids[i]];

                    snprintf(att_name, PIO_MAX_NAME, "%s/__pio__/dims", av->name);
                    attributeH = adios2_inquire_attribute(file->ioH, att_name);
                    if (attributeH == NULL)
                    {
                        attributeH = adios2_define_attribute_array(file->ioH, att_name, adios2_type_string, dimnames, av->ndims);
                        if (attributeH == NULL)
                        {
                            return pio_err(ios, file, PIO_EADIOS2ERR, __FILE__, __LINE__, "Defining (ADIOS) attribute array (name=%s, size=%d) failed for file (%s, ncid=%d)", att_name, av->ndims, pio_get_fname_from_file(file), file->pio_ncid);
                        }
                    }
                }
            }
        }

        strncpy(file->varlist[*varidp].vname, name, PIO_MAX_NAME);
        if (file->num_unlim_dimids > 0)
        {
            int is_rec_var = 0;
            for (int i = 0; (i < ndims) && (!is_rec_var); i++)
            {
                for (int j = 0; (j < file->num_unlim_dimids) && (!is_rec_var); j++)
                {
                    if (dimidsp[i] == file->unlim_dimids[j])
                    {
                        is_rec_var = 1;
                    }
                }
            }

            file->varlist[*varidp].rec_var = is_rec_var;
        }

        return PIO_NOERR;
    }
#endif

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
#ifdef _PNETCDF
        if (file->iotype == PIO_IOTYPE_PNETCDF)
        {
            ierr = ncmpi_def_var(file->fh, name, xtype, ndims, dimidsp, varidp);
            if (ierr != PIO_NOERR)
            {
                char errmsg[PIO_MAX_NAME];
                ierr2 = PIOc_strerror(ierr, errmsg);
                ierr = pio_err(ios, file, ierr, __FILE__, __LINE__,
                                "Defining variable %s (ndims = %d) in file %s (ncid=%d, iotype=%s) failed. %s", name, ndims, pio_get_fname_from_file(file), ncid, pio_iotype_to_string(file->iotype), ((ierr2 == PIO_NOERR) ? errmsg : ""));
            }
        }
#endif /* _PNETCDF */

#ifdef _NETCDF
        if (file->iotype != PIO_IOTYPE_PNETCDF && file->iotype != PIO_IOTYPE_ADIOS && file->do_io)
        {
            ierr = nc_def_var(file->fh, name, xtype, ndims, dimidsp, varidp);
            if (ierr != PIO_NOERR)
            {
                char errmsg[PIO_MAX_NAME];
                ierr2 = PIOc_strerror(ierr, errmsg);
                ierr = pio_err(ios, file, ierr, __FILE__, __LINE__,
                                "Defining variable %s (ndims = %d) in file %s (ncid=%d, iotype=%s) failed. %s", name, ndims, pio_get_fname_from_file(file), ncid, pio_iotype_to_string(file->iotype), ((ierr2 == PIO_NOERR) ? errmsg : ""));
            }
        }
#endif /* _NETCDF */

#ifdef _NETCDF4
        /* For netCDF-4 serial files, turn on compression for this
           variable (non-scalar). */
        if (!ierr && file->iotype == PIO_IOTYPE_NETCDF4C
            && (ndims > 0) && file->do_io)
        {
            ierr = nc_def_var_deflate(file->fh, *varidp, 0, 1, 1);
            if (ierr != PIO_NOERR)
            {
                char errmsg[PIO_MAX_NAME];
                ierr2 = PIOc_strerror(ierr, errmsg);
                ierr = pio_err(ios, file, ierr, __FILE__, __LINE__,
                                "Defining variable %s (varid = %d, ndims = %d) in file %s (ncid=%d, iotype=%s) failed. Turning on compression on the variable failed. %s", name, *varidp, ndims, pio_get_fname_from_file(file), ncid, pio_iotype_to_string(file->iotype), ((ierr2 == PIO_NOERR) ? errmsg : ""));
            }
        }

        /* For netCDF-4 parallel files, set parallel access to collective. */
        if (!ierr && file->iotype == PIO_IOTYPE_NETCDF4P && file->do_io)
        {
            ierr = nc_var_par_access(file->fh, *varidp, NC_COLLECTIVE);
            if (ierr != PIO_NOERR)
            {
                char errmsg[PIO_MAX_NAME];
                ierr2 = PIOc_strerror(ierr, errmsg);
                ierr = pio_err(ios, file, ierr, __FILE__, __LINE__,
                                "Defining variable %s (varid = %d, ndims = %d) in file %s (ncid=%d, iotype=%s) failed. Setting parallel access for the variable failed. %s", name, *varidp, ndims, pio_get_fname_from_file(file), ncid, pio_iotype_to_string(file->iotype), ((ierr2 == PIO_NOERR) ? errmsg : ""));
            }
        }
#endif /* _NETCDF4 */

    }

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc*_def_var_* failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results. */
    /* FIXME: varidp should be valid, no need to check it here */
    if (varidp)
        if ((mpierr = MPI_Bcast(varidp, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    strncpy(file->varlist[*varidp].vname, name, PIO_MAX_NAME);
    if(file->num_unlim_dimids > 0)
    {
        int is_rec_var = 0;
        for(int i=0; (i<ndims) && (!is_rec_var); i++)
        {
            for(int j=0; (j<file->num_unlim_dimids) && (!is_rec_var); j++)
            {
                if(dimidsp[i] == file->unlim_dimids[j])
                {
                    is_rec_var = 1;
                }
            }
        }
        file->varlist[*varidp].rec_var = is_rec_var;
    }
#ifdef PIO_MICRO_TIMING
    /* Create timers for the variable
      * - Assuming that we don't reuse varids 
      * - Also assuming that a timer is needed if we query about a var
      * */
    snprintf(timer_log_fname, PIO_MAX_NAME, "piorwinfo%010dwrank.dat", ios->ioroot);
    if(!mtimer_is_valid(file->varlist[*varidp].rd_mtimer))
    {
        char tmp_timer_name[PIO_MAX_NAME];
        snprintf(tmp_timer_name, PIO_MAX_NAME, "%s_%s", "rd", name);
        file->varlist[*varidp].rd_mtimer = mtimer_create(tmp_timer_name, ios->my_comm, timer_log_fname);
        if(!mtimer_is_valid(file->varlist[*varidp].rd_mtimer))
        {
            const char *vname = (name) ? name : "UNKNOWN";
            return pio_err(ios, file, PIO_EINTERNAL, __FILE__, __LINE__,
                            "Defining variable %s in file %s (ncid=%d) failed. Unable to create micro timer (read) for the variable", vname, pio_get_fname_from_file(file), ncid);
        }
        assert(!mtimer_is_valid(file->varlist[*varidp].rd_rearr_mtimer));
        snprintf(tmp_timer_name, PIO_MAX_NAME, "%s_%s", "rd_rearr", name);
        file->varlist[*varidp].rd_rearr_mtimer = mtimer_create(tmp_timer_name, ios->my_comm, timer_log_fname);
        if(!mtimer_is_valid(file->varlist[*varidp].rd_rearr_mtimer))
        {
            const char *vname = (name) ? name : "UNKNOWN";
            return pio_err(ios, file, PIO_EINTERNAL, __FILE__, __LINE__,
                            "Defining variable %s in file %s (ncid=%d) failed. Unable to create micro timer (read rearrange) for the variable", vname, pio_get_fname_from_file(file), ncid);
        }
        snprintf(tmp_timer_name, PIO_MAX_NAME, "%s_%s", "wr", name);
        file->varlist[*varidp].wr_mtimer = mtimer_create(tmp_timer_name, ios->my_comm, timer_log_fname);
        if(!mtimer_is_valid(file->varlist[*varidp].wr_mtimer))
        {
            const char *vname = (name) ? name : "UNKNOWN";
            return pio_err(ios, file, PIO_EINTERNAL, __FILE__, __LINE__,
                            "Defining variable %s in file %s (ncid=%d) failed. Unable to create micro timer (write) for the variable", vname, pio_get_fname_from_file(file), ncid);
        }
        assert(!mtimer_is_valid(file->varlist[*varidp].wr_rearr_mtimer));
        snprintf(tmp_timer_name, PIO_MAX_NAME, "%s_%s", "wr_rearr", name);
        file->varlist[*varidp].wr_rearr_mtimer = mtimer_create(tmp_timer_name, ios->my_comm, timer_log_fname);
        if(!mtimer_is_valid(file->varlist[*varidp].wr_rearr_mtimer))
        {
            const char *vname = (name) ? name : "UNKNOWN";
            return pio_err(ios, file, PIO_EINTERNAL, __FILE__, __LINE__,
                            "Defining variable %s in file %s (ncid=%d) failed. Unable to create micro timer (write rearrange) for the variable", vname, pio_get_fname_from_file(file), ncid);
        }
    }
#endif
    return PIO_NOERR;
}

/**
 * Set the fill value for a variable.
 *
 * See the <a
 * href="http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html">netCDF
 * variable documentation</a> for details about the operation of this
 * function.
 *
 * When the fill mode for the file is NC_FILL, then fill values are
 * used for missing data. This function sets the fill value to be used
 * for a variable. If no specific fill value is set (as a _FillValue
 * attribute), then the default fill values from netcdf.h are used.
 *
 * NetCDF-4 and pnetcdf files allow setting fill_mode (to NC_FILL or
 * NC_NOFILL) on a per-variable basis. NetCDF classic only allows the
 * fill_mode setting to be set for the whole file. For this function,
 * the fill_mode parameter is ignored for classic files. Set the
 * file-level fill mode with PIOc_set_fill().
 *
 * @param ncid the ncid of the open file.
 * @param varid the ID of the variable to set chunksizes for.
 * @param fill_mode fill mode for this variable (NC_FILL or NC_NOFILL)
 * @param fill_value pointer to the fill value to be used if fill_mode is set to NC_FILL.
 * @return PIO_NOERR for success, otherwise an error code.
 * @ingroup PIO_def_var
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_def_var_fill(int ncid, int varid, int fill_mode, const void *fill_valuep)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    nc_type xtype = NC_NAT;    /* The type of the variable (and fill value att). */
    PIO_Offset type_size = 0;  /* Size in bytes of this variable's type. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */

    LOG((1, "PIOc_def_var_fill ncid = %d varid = %d fill_mode = %d\n", ncid, varid,
         fill_mode));

    /* Get the file info. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Defining fillvalue for variable (varid=%d) failed on file (ncid=%d). Unable to inquire internal structure associated with the file id", varid, ncid);
    }
    ios = file->iosystem;

    /* Caller must provide correct values. */
    if ((fill_mode != NC_FILL && fill_mode != NC_NOFILL) ||
        (fill_mode == NC_FILL && !fill_valuep))
    {
        const char *err_msg = (fill_mode != NC_NOFILL) ? "Fill mode specified by the user is not valid" : "The pointer to fill value is invalid (NULL)";
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Defining fillvalue for variable %s (varid=%d) failed on file %s (ncid=%d). %s", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, err_msg);
    }

    /* Run this on all tasks if async is not in use, but only on
     * non-IO tasks if async is in use. Get the size of this vars
     * type. */
    if (!ios->async || !ios->ioproc)
    {
        ierr = PIOc_inq_vartype(ncid, varid, &xtype);
        if(ierr != PIO_NOERR){
            LOG((1, "PIOc_inq_vartype failed, ierr = %d", ierr));
            return ierr;
        }
        ierr = PIOc_inq_type(ncid, xtype, NULL, &type_size);
        if(ierr != PIO_NOERR){
            LOG((1, "PIOc_inq_type failed, ierr = %d", ierr));
            return ierr;
        }
    }
    LOG((2, "PIOc_def_var_fill type_size = %d", type_size));

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_DEF_VAR_FILL;
        char fill_value_present = fill_valuep ? true : false;
        void *amsg_fillvalue_p = NULL;
        if(!fill_value_present)
        {
            amsg_fillvalue_p = calloc(type_size, sizeof(char));
        }

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, fill_mode,
            type_size, fill_value_present, type_size,
            (fill_value_present) ? fill_valuep : amsg_fillvalue_p);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                        "Defining fillvalue for variable %s (varid=%d) failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_DEF_VAR_FILL, on iosystem (iosysid=%d)", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }

        if(!fill_value_present)
        {
            free(amsg_fillvalue_p);
        }

        /* Broadcast values currently only known on computation tasks to IO tasks. */
        if ((mpierr = MPI_Bcast(&xtype, 1, MPI_INT, ios->comproot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
        if ((mpierr = MPI_Bcast(&type_size, 1, MPI_OFFSET, ios->comproot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    if (ios->ioproc)
    {
        if (file->iotype == PIO_IOTYPE_PNETCDF)
        {
#ifdef _PNETCDF
            ierr = ncmpi_def_var_fill(file->fh, varid, fill_mode, (void *)fill_valuep);
#endif /* _PNETCDF */
        }
        else if (file->iotype == PIO_IOTYPE_NETCDF)
        {
#ifdef _NETCDF
            LOG((2, "defining fill value attribute for netCDF classic file"));
            if (file->do_io)            
                ierr = nc_put_att(file->fh, varid, _FillValue, xtype, 1, fill_valuep);
#endif /* _NETCDF */
        }
        else
        {
#ifdef _NETCDF4
            if (file->do_io)
                ierr = nc_def_var_fill(file->fh, varid, fill_mode, fill_valuep);
#endif
        }
        LOG((2, "after def_var_fill ierr = %d", ierr));
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        LOG((2, "ADIOS missing %s:%s", __FILE__, __func__));
        ierr = PIO_NOERR;
    }
#endif

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc*_def_var_fill failed, ierr = %d", ierr));
        return ierr;
    }

    return PIO_NOERR;
}

/**
 * The PIO-C interface for the NetCDF function nc_inq_var_fill.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm. For more information on the underlying NetCDF commmand
 * please read about this function in the NetCDF documentation at:
 * http://www.unidata.ucar.edu/software/netcdf/docs/group__variables.html
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param no_fill a pointer to int that will get the fill
 * mode. Ignored if NULL (except with pnetcdf, which seg-faults with
 * NULL.)
 * @param fill_valuep pointer to space that gets the fill value for
 * this variable. Ignored if NULL.
 * @return PIO_NOERR for success, error code otherwise.
 * @ingroup PIO_inq_var_fill
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_inq_var_fill(int ncid, int varid, int *no_fill, void *fill_valuep)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    nc_type xtype = NC_NAT;  /* Type of variable and its _FillValue attribute. */
    PIO_Offset type_size = 0;  /* Size in bytes of this variable's type. */
    int mpierr = MPI_SUCCESS;  /* Return code from MPI function codes. */
    int ierr = PIO_NOERR;  /* Return code from function calls. */

    LOG((1, "PIOc_inq_var_fill ncid = %d varid = %d", ncid, varid));

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Inquiring fill value settings for the variable (varid=%d) failed on file (ncid=%d). Unable to query internal structure associated with the file id", varid, ncid);
    }
    ios = file->iosystem;
    LOG((2, "found file"));

    /* Run this on all tasks if async is not in use, but only on
     * non-IO tasks if async is in use. Get the size of this vars
     * type. */
    if (!ios->async || !ios->ioproc)
    {
        ierr = PIOc_inq_vartype(ncid, varid, &xtype);
        if(ierr != PIO_NOERR){
            LOG((1, "PIOc_inq_vartype failed, ierr = %d", ierr));
            return ierr;
        }
        ierr = PIOc_inq_type(ncid, xtype, NULL, &type_size);
        if(ierr != PIO_NOERR){
            LOG((1, "PIOc_inq_type failed, ierr = %d", ierr));
            return ierr;
        }
        LOG((2, "PIOc_inq_var_fill type_size = %d", type_size));
    }

    /* If async is in use, and this is not an IO task, bcast the parameters. */
    if (ios->async)
    {
        int msg = PIO_MSG_INQ_VAR_FILL;
        char no_fill_present = no_fill ? true : false;
        char fill_value_present = fill_valuep ? true : false;

        PIO_SEND_ASYNC_MSG(ios, msg, &ierr, ncid, varid, type_size,
            no_fill_present, fill_value_present);
        if(ierr != PIO_NOERR)
        {
            return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                            "Inquiring fill value settings for the variable %s (varid=%d) failed on file %s (ncid=%d). Unable to send asynchronous message, PIO_MSG_INQ_VAR_FILL, on iosystem (iosysid=%d)", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, ios->iosysid);
        }

        /* Broadcast values currently only known on computation tasks to IO tasks. */
        if ((mpierr = MPI_Bcast(&xtype, 1, MPI_INT, ios->comproot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
        if ((mpierr = MPI_Bcast(&type_size, 1, MPI_OFFSET, ios->comproot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    }

    /* If this is an IO task, then call the netCDF function. */
    if (ios->ioproc)
    {
        LOG((2, "calling inq_var_fill file->iotype = %d file->fh = %d varid = %d",
             file->iotype, file->fh, varid));
        if (file->iotype == PIO_IOTYPE_PNETCDF)
        {
#ifdef _PNETCDF
            ierr = ncmpi_inq_var_fill(file->fh, varid, no_fill, fill_valuep);
#endif /* _PNETCDF */
        }
        else if (file->iotype == PIO_IOTYPE_NETCDF && file->do_io)
        {
#ifdef _NETCDF
            /* Get the file-level fill mode. */
            if (no_fill)
            {
                ierr = nc_set_fill(file->fh, NC_NOFILL, no_fill);
                if (!ierr)
                    ierr = nc_set_fill(file->fh, *no_fill, NULL);
            }

            if (!ierr && fill_valuep)
            {
                ierr = nc_get_att(file->fh, varid, _FillValue, fill_valuep);
                if (ierr == NC_ENOTATT)
                {
                    char char_fill_value = NC_FILL_CHAR;
                    signed char byte_fill_value = NC_FILL_BYTE;
                    short short_fill_value = NC_FILL_SHORT;
                    int int_fill_value = NC_FILL_INT;
                    float float_fill_value = NC_FILL_FLOAT;
                    double double_fill_value = NC_FILL_DOUBLE;
                    switch (xtype)
                    {
                    case NC_BYTE:
                        memcpy(fill_valuep, &byte_fill_value, sizeof(signed char));
                        break;
                    case NC_CHAR:
                        memcpy(fill_valuep, &char_fill_value, sizeof(char));
                        break;
                    case NC_SHORT:
                        memcpy(fill_valuep, &short_fill_value, sizeof(short));
                        break;
                    case NC_INT:
                        memcpy(fill_valuep, &int_fill_value, sizeof(int));
                        break;
                    case NC_FLOAT:
                        memcpy(fill_valuep, &float_fill_value, sizeof(float));
                        break;
                    case NC_DOUBLE:
                        memcpy(fill_valuep, &double_fill_value, sizeof(double));
                        break;
                    default:
                        {
                          return pio_err(ios, file, NC_EBADTYPE, __FILE__, __LINE__,
                                          "Inquiring fill value settings for the variable %s (varid=%d) failed on file %s (ncid=%d). Unsupported type (xtype=%x) specified for the fillvalue", pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, xtype);
                        }
                    }
                    ierr = PIO_NOERR;
                }
            }
#endif /* _NETCDF */
        }
        else
        {
#ifdef _NETCDF4
            /* The inq_var_fill is not supported in classic-only builds. */
            if (file->do_io)
                ierr = nc_inq_var_fill(file->fh, varid, no_fill, fill_valuep);
#endif /* _NETCDF */
        }
        LOG((2, "after call to inq_var_fill, ierr = %d", ierr));
    }

    /* ADIOS: assume all procs are also IO tasks */
#ifdef _ADIOS2
    if (file->iotype == PIO_IOTYPE_ADIOS)
    {
        LOG((2, "ADIOS missing %s:%s", __FILE__, __func__));
        ierr = PIO_NOERR;
    }
#endif

    ierr = check_netcdf(NULL, file, ierr, __FILE__, __LINE__);
    if(ierr != PIO_NOERR){
        LOG((1, "nc*_inq_var_fill failed, ierr = %d", ierr));
        return ierr;
    }

    /* Broadcast results to all tasks. Ignore NULL parameters. */
    if (no_fill)
        if ((mpierr = MPI_Bcast(no_fill, 1, MPI_INT, ios->ioroot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);
    if (fill_valuep)
        if ((mpierr = MPI_Bcast(fill_valuep, type_size, MPI_CHAR, ios->ioroot, ios->my_comm)))
            check_mpi(NULL, file, mpierr, __FILE__, __LINE__);

    return PIO_NOERR;
}

/**
 * Get the value of an attribute of any type, with no type conversion.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute to get
 * @param ip a pointer that will get the attribute value.
 * @return PIO_NOERR for success, error code otherwise.
 * @ingroup PIO_get_att
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_get_att(int ncid, int varid, const char *name, void *ip)
{
    iosystem_desc_t *ios;  /* Pointer to io system information. */
    file_desc_t *file;     /* Pointer to file information. */
    int ierr = PIO_NOERR;              /* Return code from function calls. */
    nc_type atttype;       /* The type of the attribute. */

    /* Find the info about this file. */
    if ((ierr = pio_get_file(ncid, &file)))
    {
        const char *aname = (name) ? name : "UNKNOWN";
        return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                        "Getting attribute %s associated with variable (varid=%d) failed on file (ncid=%d). Unable to query internal structure associated with the file id", aname, varid, ncid);
    }
    ios = file->iosystem;

    /* User must provide a name and destination pointer. */
    if (!name || !ip || strlen(name) > PIO_MAX_NAME)
    {
        const char *aname = (name) ? name : "UNKNOWN";
        const char *err_msg_ip = "Invalid (NULL) pointer to buffer to store attribute value";
        const char *err_msg_name = (!name) ? "Invalid (NULL) pointer to attribute name" : "The length of attribute name exceeds PIO_MAX_NAME";
        return pio_err(ios, file, PIO_EINVAL, __FILE__, __LINE__,
                        "Getting attribute %s associated with variable %s(varid=%d) failed on file %s (ncid=%d). %s", aname, pio_get_vname_from_file(file, varid), varid, pio_get_fname_from_file(file), ncid, (!ip) ? err_msg_ip : err_msg_name);
    }

    LOG((1, "PIOc_get_att ncid %d varid %d name %s", ncid, varid, name));

    /* Get the type of the attribute. */
    ierr = PIOc_inq_att(ncid, varid, name, &atttype, NULL);
    if(ierr != PIO_NOERR){
        LOG((1, "PIOc_inq_att failed, ierr = %d", ierr));
        return ierr;
    }
    LOG((2, "atttype = %d", atttype));

    return PIOc_get_att_tc(ncid, varid, name, atttype, ip);
}

/**
 * @ingroup PIO_put_att
 * Write a netCDF attribute of any type.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute.
 * @param xtype the nc_type of the attribute.
 * @param len the length of the attribute array.
 * @param op a pointer with the attribute data.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_put_att(int ncid, int varid, const char *name, nc_type xtype,
                 PIO_Offset len, const void *op)
{
    return PIOc_put_att_tc(ncid, varid, name, xtype, len, xtype, op);
}

/**
 * @ingroup PIO_get_att
 * Get the value of an 64-bit floating point array attribute.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute to get
 * @param ip a pointer that will get the attribute value.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_get_att_double(int ncid, int varid, const char *name, double *ip)
{
    return PIOc_get_att_tc(ncid, varid, name, PIO_DOUBLE, (void *)ip);
}

/**
 * @ingroup PIO_get_att
 * Get the value of an 8-bit unsigned char array attribute.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute to get
 * @param ip a pointer that will get the attribute value.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_get_att_uchar(int ncid, int varid, const char *name, unsigned char *ip)
{
    return PIOc_get_att_tc(ncid, varid, name, PIO_UBYTE, (void *)ip);
}

/**
 * @ingroup PIO_get_att
 * Get the value of an 16-bit unsigned integer array attribute.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute to get
 * @param ip a pointer that will get the attribute value.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_get_att_ushort(int ncid, int varid, const char *name, unsigned short *ip)
{
    return PIOc_get_att_tc(ncid, varid, name, PIO_USHORT, (void *)ip);
}

/**
 * Get the value of an 32-bit unsigned integer array attribute.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute to get
 * @param ip a pointer that will get the attribute value.
 * @return PIO_NOERR for success, error code otherwise.
 * @ingroup PIO_get_att
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_get_att_uint(int ncid, int varid, const char *name, unsigned int *ip)
{
    return PIOc_get_att_tc(ncid, varid, name, PIO_UINT, (void *)ip);
}

/**
 * @ingroup PIO_get_att
 * Get the value of an 32-bit ingeger array attribute.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute to get
 * @param ip a pointer that will get the attribute value.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_get_att_long(int ncid, int varid, const char *name, long *ip)
{
    return PIOc_get_att_tc(ncid, varid, name, PIO_LONG_INTERNAL, (void *)ip);
}

/**
 * Get the value of an text attribute. There is no type conversion
 * with this call. If the attribute is not of type NC_CHAR, then an
 * error will be returned.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute to get
 * @param ip a pointer that will get the attribute value.
 * @return PIO_NOERR for success, error code otherwise.
 * @ingroup PIO_get_att
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_get_att_text(int ncid, int varid, const char *name, char *ip)
{
    return PIOc_get_att_tc(ncid, varid, name, PIO_CHAR, (void *)ip);
}

/**
 * @ingroup PIO_get_att
 * Get the value of an 8-bit signed char array attribute.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute to get
 * @param ip a pointer that will get the attribute value.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_get_att_schar(int ncid, int varid, const char *name, signed char *ip)
{
    return PIOc_get_att_tc(ncid, varid, name, PIO_BYTE, (void *)ip);
}

/**
 * @ingroup PIO_get_att
 * Get the value of an 64-bit unsigned integer array attribute.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute to get
 * @param ip a pointer that will get the attribute value.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_get_att_ulonglong(int ncid, int varid, const char *name, unsigned long long *ip)
{
    return PIOc_get_att_tc(ncid, varid, name, PIO_UINT64, (void *)ip);
}

/**
 * @ingroup PIO_get_att
 * Get the value of an 16-bit integer array attribute.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute to get
 * @param ip a pointer that will get the attribute value.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_get_att_short(int ncid, int varid, const char *name, short *ip)
{
    return PIOc_get_att_tc(ncid, varid, name, PIO_SHORT, (void *)ip);
}

/**
 * @ingroup PIO_get_att
 * Get the value of an 32-bit integer array attribute.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute to get
 * @param ip a pointer that will get the attribute value.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_get_att_int(int ncid, int varid, const char *name, int *ip)
{
    return PIOc_get_att_tc(ncid, varid, name, PIO_INT, (void *)ip);
}

/**
 * @ingroup PIO_get_att
 * Get the value of an 64-bit integer array attribute.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute to get
 * @param ip a pointer that will get the attribute value.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_get_att_longlong(int ncid, int varid, const char *name, long long *ip)
{
    return PIOc_get_att_tc(ncid, varid, name, PIO_INT64, (void *)ip);
}

/**
 * @ingroup PIO_get_att
 * Get the value of an 32-bit floating point array attribute.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute to get
 * @param ip a pointer that will get the attribute value.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_get_att_float(int ncid, int varid, const char *name, float *ip)
{
    return PIOc_get_att_tc(ncid, varid, name, PIO_FLOAT, (void *)ip);
}

/**
 * @ingroup PIO_put_att
 * Write a netCDF attribute array of 8-bit signed chars.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute.
 * @param xtype the nc_type of the attribute.
 * @param len the length of the attribute array.
 * @param op a pointer with the attribute data.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_put_att_schar(int ncid, int varid, const char *name, nc_type xtype,
                       PIO_Offset len, const signed char *op)
{
    return PIOc_put_att_tc(ncid, varid, name, xtype, len, PIO_BYTE, op);
}

/**
 * @ingroup PIO_put_att
 * Write a netCDF attribute array of 32-bit signed integers.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute.
 * @param xtype the nc_type of the attribute.
 * @param len the length of the attribute array.
 * @param op a pointer with the attribute data.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_put_att_long(int ncid, int varid, const char *name, nc_type xtype,
                      PIO_Offset len, const long *op)
{
    return PIOc_put_att_tc(ncid, varid, name, xtype, len, PIO_LONG_INTERNAL, op);
}

/**
 * @ingroup PIO_put_att
 * Write a netCDF attribute array of 32-bit signed integers.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute.
 * @param xtype the nc_type of the attribute.
 * @param len the length of the attribute array.
 * @param op a pointer with the attribute data.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_put_att_int(int ncid, int varid, const char *name, nc_type xtype,
                     PIO_Offset len, const int *op)
{
    return PIOc_put_att_tc(ncid, varid, name, xtype, len, PIO_INT, op);
}

/**
 * @ingroup PIO_put_att
 * Write a netCDF attribute array of 8-bit unsigned chars.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute.
 * @param xtype the nc_type of the attribute.
 * @param len the length of the attribute array.
 * @param op a pointer with the attribute data.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_put_att_uchar(int ncid, int varid, const char *name, nc_type xtype,
                       PIO_Offset len, const unsigned char *op)
{
    return PIOc_put_att_tc(ncid, varid, name, xtype, len, PIO_UBYTE, op);
}

/**
 * @ingroup PIO_put_att
 * Write a netCDF attribute array of 64-bit signed integers.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute.
 * @param xtype the nc_type of the attribute.
 * @param len the length of the attribute array.
 * @param op a pointer with the attribute data.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_put_att_longlong(int ncid, int varid, const char *name, nc_type xtype,
                          PIO_Offset len, const long long *op)
{
    return PIOc_put_att_tc(ncid, varid, name, xtype, len, PIO_INT64, op);
}

/**
 * @ingroup PIO_put_att
 * Write a netCDF attribute array of 32-bit unsigned integers.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute.
 * @param xtype the nc_type of the attribute.
 * @param len the length of the attribute array.
 * @param op a pointer with the attribute data.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_put_att_uint(int ncid, int varid, const char *name, nc_type xtype,
                      PIO_Offset len, const unsigned int *op)
{
    return PIOc_put_att_tc(ncid, varid, name, xtype, len, PIO_UINT, op);
}

/**
 * @ingroup PIO_put_att
 * Write a netCDF attribute array of 32-bit floating points.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute.
 * @param xtype the nc_type of the attribute.
 * @param len the length of the attribute array.
 * @param op a pointer with the attribute data.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_put_att_float(int ncid, int varid, const char *name, nc_type xtype,
                       PIO_Offset len, const float *op)
{
    return PIOc_put_att_tc(ncid, varid, name, xtype, len, PIO_FLOAT, op);
}

/**
 * @ingroup PIO_put_att
 * Write a netCDF attribute array of 64-bit unsigned integers.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute.
 * @param xtype the nc_type of the attribute.
 * @param len the length of the attribute array.
 * @param op a pointer with the attribute data.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_put_att_ulonglong(int ncid, int varid, const char *name, nc_type xtype,
                           PIO_Offset len, const unsigned long long *op)
{
    return PIOc_put_att_tc(ncid, varid, name, xtype, len, PIO_UINT64, op);
}

/**
 * @ingroup PIO_put_att
 * Write a netCDF attribute array of 16-bit unsigned integers.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute.
 * @param xtype the nc_type of the attribute.
 * @param len the length of the attribute array.
 * @param op a pointer with the attribute data.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_put_att_ushort(int ncid, int varid, const char *name, nc_type xtype,
                        PIO_Offset len, const unsigned short *op)
{
    return PIOc_put_att_tc(ncid, varid, name, xtype, len, PIO_USHORT, op);
}

/**
 * @ingroup PIO_put_att
 * Write a netCDF text attribute.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute.
 * @param xtype the nc_type of the attribute.
 * @param len the length of the attribute array.
 * @param op a pointer with the attribute data.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_put_att_text(int ncid, int varid, const char *name,
                      PIO_Offset len, const char *op)
{
    return PIOc_put_att_tc(ncid, varid, name, NC_CHAR, len, NC_CHAR, op);
}

/**
 * @ingroup PIO_put_att
 * Write a netCDF attribute array of 16-bit integers.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute.
 * @param xtype the nc_type of the attribute.
 * @param len the length of the attribute array.
 * @param op a pointer with the attribute data.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_put_att_short(int ncid, int varid, const char *name, nc_type xtype,
                       PIO_Offset len, const short *op)
{
    return PIOc_put_att_tc(ncid, varid, name, xtype, len, PIO_SHORT, op);
}

/**
 * @ingroup PIO_put_att
 * Write a netCDF attribute array of 64-bit floating points.
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param ncid the ncid of the open file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param varid the variable ID.
 * @param name the name of the attribute.
 * @param xtype the nc_type of the attribute.
 * @param len the length of the attribute array.
 * @param op a pointer with the attribute data.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jim Edwards, Ed Hartnett
 */
int PIOc_put_att_double(int ncid, int varid, const char *name, nc_type xtype,
                        PIO_Offset len, const double *op)
{
    return PIOc_put_att_tc(ncid, varid, name, xtype, len, PIO_DOUBLE, op);
}

/**
 * @ingroup PIO_copy_att
 * Copy an attribute from one file to another
 *
 * This routine is called collectively by all tasks in the communicator
 * ios.union_comm.
 *
 * @param incid the ncid of the input file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param ivarid the ID of the variable with the attribute
 * in the input file.
 * @param name the name of the attribute to be copied
 * @param oncid the ncid of the output file, obtained from
 * PIOc_openfile() or PIOc_createfile().
 * @param ovarid the ID of the variable to copy the attribute
 * to in the output file.
 * @return PIO_NOERR for success, error code otherwise.
 * @author Jayesh Krishna
 */
int PIOc_copy_att(int incid, int ivarid, const char *name,
                  int oncid, int ovarid)
{
  int msg = PIO_MSG_COPY_ATT;
  iosystem_desc_t *ios = NULL;
  file_desc_t *ifile = NULL, *ofile = NULL;
  int mpierr = MPI_SUCCESS;
  int ierr = PIO_NOERR;

  /* Find file corresponding to the input file id */
  if((ierr = pio_get_file(incid, &ifile))){
    const char *aname = (name) ? name : "UNKNOWN";
    return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                    "Copying attribute (%s) associated with variable (varid=%d) failed on file (ncid=%d). Unable to query internal structure associated with the input file id", aname, ivarid, incid);
  }
  ios = ifile->iosystem;
  assert(ios);

  /* User must provide name shorter than PIO_MAX_NAME +1. */
  if(!name || strlen(name) > PIO_MAX_NAME){
    const char *aname = (name) ? name : "UNKNOWN";
    const char *err_msg = (!name) ? "The pointer to attribute name is NULL" : "The length of attribute name exceeds PIO_MAX_NAME";
    return pio_err(ios, ifile, PIO_EINVAL, __FILE__, __LINE__,
                    "Copying attribute, %s, associated with variable %s (varid=%d) failed on file %s (ncid=%d). %s", aname, pio_get_vname_from_file(ifile, ivarid), ivarid, pio_get_fname_from_file(ifile), incid, err_msg);
  }

  /* Find file corresponding to the output file id */
  if((ierr = pio_get_file(oncid, &ofile))){
    return pio_err(NULL, NULL, ierr, __FILE__, __LINE__,
                    "Copying attribute (%s) associated with variable (varid=%d) failed on file (ncid=%d). Unable to query internal structure associated with the output file id", name, ovarid, oncid);
  }

  /* Currently we only support copying attributes between files on
   * the same iosystem
   */
  assert(ofile->iosystem);
  if(ofile->iosystem->iosysid != ifile->iosystem->iosysid){
    return pio_err(ios, NULL, PIO_EINVAL, __FILE__, __LINE__,
                    "Copying attribute, %s, associated with variable %s (varid=%d) from file %s (ncid=%d, iosystem id = %d) to %s (ncid=%d, iosystem id =%d) failed. The two files operate on different iosystems, we currently do not support copying attributes between files operating on two different iosystems", name, pio_get_vname_from_file(ifile, ivarid), ivarid, pio_get_fname_from_file(ifile), incid, ifile->iosystem->iosysid, pio_get_fname_from_file(ofile), oncid, ofile->iosystem->iosysid);
  }
  if(ofile->iotype != ifile->iotype){
    return pio_err(ios, NULL, PIO_EINVAL, __FILE__, __LINE__,
                    "Copying attribute, %s, associated with variable %s (varid=%d) from file %s (ncid=%d, iosystem id = %d, iotype=%s) to %s (ncid=%d, iosystem id =%d, iotype=%s) failed. The iotypes of the two files are different, we currently do not support copying attributes between files with different iotypes", name, pio_get_vname_from_file(ifile, ivarid), ivarid, pio_get_fname_from_file(ifile), incid, ifile->iosystem->iosysid, pio_iotype_to_string(ifile->iotype), pio_get_fname_from_file(ofile), oncid, ofile->iosystem->iosysid, pio_iotype_to_string(ofile->iotype));
  }
  LOG((1, "PIOc_copy_att incid = %d ivarid = %d name = %s, oncid = %d, ovarid = %d", incid, ivarid, name, oncid, ovarid));

  /* If async is in use, and this is not an IO task, bcast the parameters. */
  if(ios->async){
    int namelen = strlen(name) + 1;
    PIO_SEND_ASYNC_MSG(ios, msg, &ierr, incid, ivarid, namelen, name, oncid, ovarid);
    if(ierr != PIO_NOERR){
      return pio_err(ios, NULL, ierr, __FILE__, __LINE__,
                      "Copying attribute, %s, associated with variable %s (varid=%d) from file %s (ncid=%d) to %s (ncid=%d, varid=%d) failed. Unable to send asynchronous message, PIO_MSG_COPY_ATT, on iosystem (iosysid=%d)", name, pio_get_vname_from_file(ifile, ivarid), ivarid, pio_get_fname_from_file(ifile), incid, pio_get_fname_from_file(ofile), oncid, ovarid, ios->iosysid);
    }
  }

  switch(ifile->iotype){
#ifdef _PNETCDF
    case PIO_IOTYPE_PNETCDF:
          if(ios->ioproc){
            ierr = ncmpi_copy_att(ifile->fh, ivarid, name,
                    ofile->fh, ovarid);
          }
          ierr = check_netcdf(ios, ifile, ierr, __FILE__, __LINE__);
          break;
#endif
#ifdef _NETCDF
    case PIO_IOTYPE_NETCDF:
    case PIO_IOTYPE_NETCDF4C:
    case PIO_IOTYPE_NETCDF4P:
          if(ios->ioproc && ifile->do_io){
            ierr = nc_copy_att(ifile->fh, ivarid, name,
                    ofile->fh, ovarid);
          }
          ierr = check_netcdf(ios, ifile, ierr, __FILE__, __LINE__);
          break;
#endif
    default:
          {
            /* Get the attribute from the input file and put it in the
             * output file
             */
            nc_type att_type;
            PIO_Offset att_len = 0, type_sz = 0;
            ierr = PIOc_inq_att(ifile->fh, ivarid, name, &att_type, &att_len);
            if(ierr != PIO_NOERR){
              ierr = pio_err(ios, ifile, ierr, __FILE__, __LINE__,
                      "Copying attribute, %s, associated with variable %s (varid=%d) in file %s (ncid=%d) to file %s (ncid=%d) failed. Inquiring attribute type and length in file %s (ncid=%d) failed", name, pio_get_vname_from_file(ifile, ivarid), ivarid, pio_get_fname_from_file(ifile), incid, pio_get_fname_from_file(ofile), oncid, pio_get_fname_from_file(ifile), incid);
              break;
            }
            ierr = PIOc_inq_type(ifile->fh, att_type, NULL, &type_sz);
            if(ierr != PIO_NOERR){
              ierr = pio_err(ios, ifile, ierr, __FILE__, __LINE__,
                      "Copying attribute, %s, associated with variable %s (varid=%d) in file %s (ncid=%d) to file %s (ncid=%d) failed. Inquiring attribute type size (attribute type = %x) failed", name, pio_get_vname_from_file(ifile, ivarid), ivarid, pio_get_fname_from_file(ifile), incid, pio_get_fname_from_file(ofile), oncid, att_type);
              break;
            }
            void *pbuf = malloc(type_sz * att_len);
            if(!pbuf){
              ierr = pio_err(ios, ifile, PIO_ENOMEM, __FILE__, __LINE__,
                      "Copying attribute, %s, associated with variable %s (varid=%d) in file %s (ncid=%d) to file %s (ncid=%d) failed. Unable to allocate %lld bytes to temporarily cache the attribute for copying", name, pio_get_vname_from_file(ifile, ivarid), ivarid, pio_get_fname_from_file(ifile), incid, pio_get_fname_from_file(ofile), oncid, (long long int) (type_sz * att_len));
              break;
            }
            ierr = PIOc_get_att(ifile->fh, ivarid, name, pbuf);
            if(ierr != PIO_NOERR){
              ierr = pio_err(ios, ifile, ierr, __FILE__, __LINE__,
                      "Copying attribute, %s, associated with variable %s (varid=%d) in file %s (ncid=%d) to file %s (ncid=%d) failed. Getting attribute from file %s (ncid=%d) failed", name, pio_get_vname_from_file(ifile, ivarid), ivarid, pio_get_fname_from_file(ifile), incid, pio_get_fname_from_file(ofile), oncid, pio_get_fname_from_file(ifile), incid);
              break;
            }
            ierr = PIOc_put_att(ofile->fh, ovarid, name, att_type, att_len, pbuf);
            if(ierr != PIO_NOERR){
              ierr = pio_err(ios, ifile, ierr, __FILE__, __LINE__,
                      "Copying attribute, %s, associated with variable %s (varid=%d) in file %s (ncid=%d) to file %s (ncid=%d) failed. Putting/Writing attribute to file %s (ncid=%d, varid=%d) failed", name, pio_get_vname_from_file(ifile, ivarid), ivarid, pio_get_fname_from_file(ifile), incid, pio_get_fname_from_file(ofile), oncid, pio_get_fname_from_file(ofile), oncid, ovarid);
              break;
            }
            free(pbuf);
            break;
          }
  } /* switch(file->iotype) */

  if(ierr != PIO_NOERR){
    return pio_err(ios, ifile, ierr, __FILE__, __LINE__,
            "Copying attribute, %s, associated with variable %s (varid=%d) in file %s (ncid=%d) to file %s (ncid=%d) failed with iotype = %s (%d)", name, pio_get_vname_from_file(ifile, ivarid), ivarid, pio_get_fname_from_file(ifile), incid, pio_get_fname_from_file(ofile), oncid, pio_iotype_to_string(ifile->iotype), ifile->iotype);
  }

  return PIO_NOERR;
}
