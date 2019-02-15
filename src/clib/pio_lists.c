/**
 * @file
 * PIO list functions.
 */
#include <config.h>
#include <pio.h>
#include <pio_internal.h>
#ifdef PIO_MICRO_TIMING
#include "pio_timer.h"
#endif
#include <string.h>
#include <stdio.h>

static io_desc_t *pio_iodesc_list = NULL;
static io_desc_t *current_iodesc = NULL;
static iosystem_desc_t *pio_iosystem_list = NULL;
static file_desc_t *pio_file_list = NULL;
static file_desc_t *current_file = NULL;

/** 
 * Add a new entry to the global list of open files.
 *
 * This function guarantees that files (id of the
 * files) are unique across the comm provided
 *
 * @param file pointer to the file_desc_t struct for the new file.
 * @param comm MPI Communicator across which the files
 * need to be unique
 * @returns The id for the file added to the list
 */
#define PIO_FILE_START_ID 16
int pio_add_to_file_list(file_desc_t *file, MPI_Comm comm)
{
    /* Using an arbitrary start id for file ids helps
     * in debugging, to distinguish between ids assigned
     * to different structures in the code
     * Also note that NetCDF ids start at 4, PnetCDF ids
     * start at 0 and NetCDF4 ids start at 65xxx
     */
    static int pio_file_next_id = PIO_FILE_START_ID;
    file_desc_t *cfile;

    assert(file);

    if(comm != MPI_COMM_NULL)
    {
        int tmp_id = pio_file_next_id;
        int mpierr = MPI_Allreduce(&tmp_id, &pio_file_next_id, 1, MPI_INT, MPI_MAX, comm);
        assert(mpierr == MPI_SUCCESS);
    }
    file->pio_ncid = pio_file_next_id;
    pio_file_next_id++;
    /* This file will be at the end of the list, and have no next. */
    file->next = NULL;

    /* Get a pointer to the global list of files. */
    cfile = pio_file_list;

    /* Keep a global pointer to the current file. */
    current_file = file;

    /* If there is nothing in the list, then file will be the first
     * entry. Otherwise, move to end of the list. */
    if (!cfile)
        pio_file_list = file;
    else
    {
        while (cfile->next)
            cfile = cfile->next;
        cfile->next = file;
    }

    return file->pio_ncid;
}

/** 
 * Given ncid, find the file_desc_t data for an open file. The ncid
 * used is the interally generated pio_ncid.
 *
 * @param ncid the PIO assigned ncid of the open file.
 * @param cfile1 pointer to a pointer to a file_desc_t. The pointer
 * will get a copy of the pointer to the file info.
 *
 * @returns 0 for success, error code otherwise.
 * @author Ed Hartnett
 */
int pio_get_file(int ncid, file_desc_t **cfile1)
{
    file_desc_t *cfile = NULL;

    LOG((2, "pio_get_file ncid = %d", ncid));

    /* Caller must provide this. */
    if (!cfile1)
        return PIO_EINVAL;

    /* Find the file pointer. */
    if (current_file && current_file->pio_ncid == ncid)
        cfile = current_file;
    else
        for (cfile = pio_file_list; cfile; cfile = cfile->next)
            if (cfile->pio_ncid == ncid)
            {
                current_file = cfile;
                break;
            }

    /* If not found, return error. */
    if (!cfile)
        return PIO_EBADID;

    /* We depend on every file having a pointer to the iosystem. */
    if (!cfile->iosystem)
        return PIO_EINVAL;

    /* Let's just ensure we have a valid IO type. */
    pioassert(iotype_is_valid(cfile->iotype), "invalid IO type", __FILE__, __LINE__);

    /* Copy pointer to file info. */
    *cfile1 = cfile;

    return PIO_NOERR;
}

/** 
 * Delete a file from the list of open files.
 *
 * @param ncid ID of file to delete from list
 * @returns 0 for success, error code otherwise
 * @author Jim Edwards, Ed Hartnett
 */
int pio_delete_file_from_list(int ncid)
{
    file_desc_t *cfile, *pfile = NULL;
    int ret;

    /* Look through list of open files. */
    for (cfile = pio_file_list; cfile; cfile = cfile->next)
    {
        if (cfile->pio_ncid == ncid)
        {
            if (!pfile)
                pio_file_list = cfile->next;
            else
                pfile->next = cfile->next;

            if (current_file == cfile)
                current_file = pfile;

            /* Free any fill values that were allocated. */
            for (int v = 0; v < PIO_MAX_VARS; v++)
            {
                if (cfile->varlist[v].fillvalue)
                    free(cfile->varlist[v].fillvalue);
#ifdef PIO_MICRO_TIMING
                mtimer_destroy(&(cfile->varlist[v].rd_mtimer));
                mtimer_destroy(&(cfile->varlist[v].rd_rearr_mtimer));
                mtimer_destroy(&(cfile->varlist[v].wr_mtimer));
                mtimer_destroy(&(cfile->varlist[v].wr_rearr_mtimer));
#endif
            }

            /* Free the varlist entries for this file. */
            while (cfile->varlist2)
                if ((ret = delete_var_desc(cfile->varlist2->varid, &cfile->varlist2)))
                    return pio_err(NULL, cfile, ret, __FILE__, __LINE__);

            free(cfile->unlim_dimids);
            /* Free the memory used for this file. */
            free(cfile);
            
            return PIO_NOERR;
        }
        pfile = cfile;
    }

    /* No file was found. */
    return PIO_EBADID;
}

/** 
 * Delete iosystem info from list.
 *
 * @param piosysid the iosysid to delete
 * @returns 0 on success, error code otherwise
 * @author Jim Edwards
 */
int pio_delete_iosystem_from_list(int piosysid)
{
    iosystem_desc_t *ciosystem, *piosystem = NULL;

    LOG((1, "pio_delete_iosystem_from_list piosysid = %d", piosysid));

    for (ciosystem = pio_iosystem_list; ciosystem; ciosystem = ciosystem->next)
    {
        LOG((3, "ciosystem->iosysid = %d", ciosystem->iosysid));
        if (ciosystem->iosysid == piosysid)
        {
            if (piosystem == NULL)
                pio_iosystem_list = ciosystem->next;
            else
                piosystem->next = ciosystem->next;
            free(ciosystem);
            return PIO_NOERR;
        }
        piosystem = ciosystem;
    }
    return PIO_EBADID;
}

/**
 * Add iosystem info to a global list.
 * This function guarantees that iosystems (ioid of the
 * iosystems) are unique across the comm provided
 *
 * @param ios pointer to the iosystem_desc_t info to add.
 * @param comm MPI Communicator across which the iosystems
 * need to be unique
 * @returns the id of the newly added iosystem.
 */
#define PIO_IOSYSTEM_START_ID 2048
int pio_add_to_iosystem_list(iosystem_desc_t *ios, MPI_Comm comm)
{
    /* Using an arbitrary start id for iosystem ids helps
     * in debugging, to distinguish between ids assigned
     * to different structures in the code
     */
    static int pio_iosystem_next_ioid = PIO_IOSYSTEM_START_ID;
    iosystem_desc_t *cios;

    assert(ios);

    if(comm != MPI_COMM_NULL)
    {
        int tmp_id = pio_iosystem_next_ioid;
        int mpierr = MPI_Allreduce(&tmp_id, &pio_iosystem_next_ioid, 1, MPI_INT, MPI_MAX, comm);
        assert(mpierr == MPI_SUCCESS);
    }
    ios->iosysid = pio_iosystem_next_ioid;
    pio_iosystem_next_ioid += 1;

    ios->next = NULL;
    cios = pio_iosystem_list;
    if (!cios)
        pio_iosystem_list = ios;
    else
    {
        while (cios->next)
        {
            cios = cios->next;
        }
        cios->next = ios;
    }

    return ios->iosysid;
}

/** 
 * Get iosystem info from list.
 *
 * @param iosysid id of the iosystem
 * @returns pointer to iosystem_desc_t, or NULL if not found.
 * @author Jim Edwards
 */
iosystem_desc_t *pio_get_iosystem_from_id(int iosysid)
{
    iosystem_desc_t *ciosystem;

    LOG((2, "pio_get_iosystem_from_id iosysid = %d", iosysid));

    for (ciosystem = pio_iosystem_list; ciosystem; ciosystem = ciosystem->next)
        if (ciosystem->iosysid == iosysid)
            return ciosystem;

    return NULL;
}

/** 
 * Count the number of open iosystems.
 *
 * @param niosysid pointer that will get the number of open iosystems.
 * @returns 0 for success.
 * @author Jim Edwards
 */
int pio_num_iosystem(int *niosysid)
{
    int count = 0;

    /* Count the elements in the list. */
    for (iosystem_desc_t *c = pio_iosystem_list; c; c = c->next)
        count++;

    /* Return count to caller via pointer. */
    if (niosysid)
        *niosysid = count;

    return PIO_NOERR;
}

/** 
 * Add an iodesc to a global list.
 * This function guarantees that iodescs (id of the
 * iodescs) are unique across the comm provided
 *
 * @param io_desc_t pointer to data to add to list.
 * @param comm MPI Communicator across which the iosystems
 * need to be unique
 * @returns the ioid of the newly added iodesc.
 */
int pio_add_to_iodesc_list(io_desc_t *iodesc, MPI_Comm comm)
{
    /* Using an arbitrary start id for iodesc ids helps
     * in debugging, to distinguish between ids assigned
     * to different structures in the code
     */
    static int pio_iodesc_next_id = PIO_IODESC_START_ID;
    io_desc_t *ciodesc = pio_iodesc_list;

    if(comm != MPI_COMM_NULL)
    {
        int tmp_id = pio_iodesc_next_id;
        int mpierr = MPI_Allreduce(&tmp_id, &pio_iodesc_next_id, 1, MPI_INT, MPI_MAX, comm);
        assert(mpierr == MPI_SUCCESS);
    }
    iodesc->ioid = pio_iodesc_next_id;
    pio_iodesc_next_id++;
    iodesc->next = NULL;

    /* Add to the global list */
    if (pio_iodesc_list == NULL)
        pio_iodesc_list = iodesc;
    else
    {
        /* pio_desc_list has atleast one node */
        while(ciodesc->next)
        {
            ciodesc = ciodesc->next;
        }
        ciodesc->next = iodesc;
    }
    current_iodesc = iodesc;

    return iodesc->ioid;
}

/** 
 * Get an iodesc.
 *
 * @param ioid ID of iodesc to get.
 * @returns pointer to the iodesc struc.
 * @author Jim Edwards
 */
io_desc_t *pio_get_iodesc_from_id(int ioid)
{
    io_desc_t *ciodesc = NULL;

    /* Do we already have a pointer to it? */
    if (current_iodesc && current_iodesc->ioid == ioid)
        return current_iodesc;

    /* Find the decomposition in the list. */
    for (ciodesc = pio_iodesc_list; ciodesc; ciodesc = ciodesc->next)
        if (ciodesc->ioid == ioid)
        {
            current_iodesc = ciodesc;
            break;
        }

    return ciodesc;
}

/** 
 * Delete an iodesc.
 *
 * @param ioid ID of iodesc to delete.
 * @returns 0 on success, error code otherwise.
 * @author Jim Edwards
 */
int pio_delete_iodesc_from_list(int ioid)
{
    io_desc_t *ciodesc, *piodesc = NULL;

    for (ciodesc = pio_iodesc_list; ciodesc; ciodesc = ciodesc->next)
    {
        if (ciodesc->ioid == ioid)
        {
            if (piodesc == NULL)
                pio_iodesc_list = ciodesc->next;
            else
                piodesc->next = ciodesc->next;

            if (current_iodesc == ciodesc)
                current_iodesc = pio_iodesc_list;
            free(ciodesc);
            return PIO_NOERR;
        }
        piodesc = ciodesc;
    }
    return PIO_EBADID;
}

/** 
 * Add var_desc_t info to the list.
 *
 * @param varid the varid of the variable.
 * @param rec_var non-zero if this is a record var.
 * @param varlist pointer to list to add to.
 * @returns 0 for success, error code otherwise.
 * @author Ed Hartnett
 */
int add_to_varlist(int varid, int rec_var, var_desc_t **varlist)
{
    var_desc_t *var_desc;

    /* Check inputs. */
    pioassert(varid >= 0 && varlist, "invalid input", __FILE__, __LINE__);

    /* Allocate storage. */
    if (!(var_desc = calloc(1, sizeof(var_desc_t))))
        return PIO_ENOMEM;

    /* Set values. */
    var_desc->varid = varid;
    var_desc->rec_var = rec_var;

    /* Add to list. */
    if (*varlist)
    {
        var_desc_t *v;
        
        /* Move to end of list. */
        for (v = *varlist; v->next; v = v->next)
            ;
        v->next = var_desc;
    }
    else
        *varlist = var_desc;
    
    return PIO_NOERR;
}

/** 
 * Get a var_desc_t info for a variable.
 *
 * @param varid ID of variable to get var_desc_t of.
 * @param varlist pointer to list of var_desc_t.
 * @param var_desc pointer that gets pointer to var_desc_t struct.
 * @returns 0 for success, error code otherwise.
 * @author Ed Hartnett
 */
int get_var_desc(int varid, var_desc_t **varlist, var_desc_t **var_desc)
{
    var_desc_t *my_var;
    
    /* Check inputs. */
    pioassert(varid >= 0 && varlist && var_desc, "invalid input",
              __FILE__, __LINE__);
    LOG((2, "get_var_desc varid = %d", varid));

    /* Empty varlist. */
    LOG((3, "*varlist = %d", *varlist));
    if (!*varlist)
        return PIO_ENOTVAR;

    /* Find the var_desc_t for this varid. */
    for (my_var = *varlist; my_var->next; my_var = my_var->next)
    {
        LOG((2, "my_var->varid = %d", my_var->varid));
        if (my_var->varid == varid)
            break;
    }

    /* Did we find it? */
    if (my_var->varid != varid)
        return PIO_ENOTVAR;
    else
        *var_desc = my_var;
    
    return PIO_NOERR;
}

/** 
 * Delete var_desc_t info for a variable.
 *
 * @param varid ID of variable to delete.
 * @param varlist pointer to list of var_desc_t.
 * @returns 0 on success, error code otherwise.
 * @author Ed Hartnett
 */
int delete_var_desc(int varid, var_desc_t **varlist)
{
    var_desc_t *v;
    var_desc_t *prev = NULL;

    /* Check inputs. */
    pioassert(varid >= 0 && varlist, "invalid input", __FILE__, __LINE__);

    /* Null list means no variables to delete. */
    if (!*varlist)
        return PIO_ENOTVAR;

    /* Find the var_desc_t for this varid. */
    for (v = *varlist; v->next; v = v->next)
    {
        LOG((3, "v->varid = %d", v->varid));
        if (v->varid == varid)
            break;
        prev = v;
    }

    /* Did we find it? */
    if (v->varid != varid)
    {
        LOG((3, "return notvar error"));
        return PIO_ENOTVAR;
    }

    /* Adjust next pointer. */
    if (prev)
        prev->next = v->next;
    else
        *varlist = v->next;

    /* Free memory. */
    free(v);
    
    return PIO_NOERR;
}
