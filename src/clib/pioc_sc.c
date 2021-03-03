/**
 * @file
 * Compute start and count arrays for the box rearranger
 *
 * @author Jim Edwards
 * @date  2014
 */
#include <pio_config.h>
#include <pio.h>
#include <pio_internal.h>

/** The default target blocksize in bytes for each io task when the box
 * rearranger is used. */
#define DEFAULT_BLOCKSIZE 1024

/** The target blocksize for each io task when the box rearranger is
 * used. */
int blocksize = DEFAULT_BLOCKSIZE;

/**
 * Recursive Standard C Function: Greatest Common Divisor.
 *
 * @param a
 * @param b
 * @returns greatest common divisor.
 */
int gcd(int a, int b )
{
    if (a == 0)
        return b;
    return gcd(b % a, a);
}

/**
 * Recursive Standard C Function: Greatest Common Divisor for 64 bit
 * ints.
 *
 * @param a
 * @param b
 * @returns greates common divisor.
 */
long long lgcd(long long a, long long b)
{
    if (a == 0)
        return b;
    return lgcd(b % a, a);
}

/**
 * Return the gcd of elements in an int array.
 *
 * @param nain length of the array
 * @param ain an array of length nain
 * @returns greatest common divisor.
 */
int gcd_array(int nain, int *ain)
{
    int i;
    int bsize = 1;

    for (i = 0; i < nain; i++)
        if (ain[i] <= 1)
            return bsize;

    bsize = ain[0];
    i = 1;
    while (i < nain && bsize > 1)
    {
        bsize = gcd(bsize, ain[i]);
        i++;
    }

    return bsize;
}

/**
 * Return the greatest common devisor of array ain as int_64.
 *
 * @param nain number of elements in ain.
 * @param ain array of length nain.
 * @returns GCD of elements in ain.
 */
long long lgcd_array(int nain, long long *ain)
{
    int i;
    long long bsize = 1;

    for (i = 0; i < nain; i++)
        if(ain[i] <= 1)
            return bsize;

    bsize = ain[0];
    i = 1;
    while (i < nain && bsize > 1)
    {
        bsize = gcd(bsize, ain[i]);
        i++;
    }

    return bsize;
}

/**
 * Compute one element (dimension) of start and count arrays. This
 * function is used by CalcStartandCount().
 *
 * @param gdim global size of one dimension.
 * @param ioprocs number of io tasks.
 * @param rank IO rank of this task.
 * @param start pointer to PIO_Offset that will get the start value.
 * @param count pointer to PIO_Offset that will get the count value.
 */
void compute_one_dim(int gdim, int ioprocs, int rank, PIO_Offset *start,
                     PIO_Offset *count)
{
    int irank;     /* The IO rank for this task. */
    int remainder;
    int adds;
    PIO_Offset lstart, lcount;

    /* Check inputs. */
    pioassert(gdim >= 0 && ioprocs > 0 && rank >= 0 && start && count,
              "invalid input", __FILE__, __LINE__);

    /* Determin which IO task to use. */
    irank = rank % ioprocs;

    /* Each IO task will have its share of the global dim. */
    lcount = (long int)(gdim / ioprocs);

    /* Find the start for this task. */
    lstart = (long int)(lcount * irank);

    /* Is there anything left over? */
    remainder = gdim - lcount * ioprocs;

    /* Distribute left over data to some IO tasks. */
    if (remainder >= ioprocs - irank)
    {
        lcount++;
        if ((adds = irank + remainder - ioprocs) > 0)
            lstart += adds;
    }

    /* Return results to caller. */
    *start = lstart;
    *count = lcount;
}

/**
 * Look for the largest block of data for io which can be expressed in
 * terms of start and count (account for gaps).
 *
 * @param arrlen
 * @param arr_in
 * @returns the size of the block
 */
PIO_Offset GCDblocksize_gaps(int arrlen, const PIO_Offset *arr_in)
{
    int numblks = 0;  /* Number of blocks. */
    int numtimes = 0; /* Number of times adjacent arr_in elements differ by != 1. */
    int numgaps = 0;  /* Number of gaps. */
    int j;  /* Loop counter. */
    int ii; /* Loop counter. */
    int n;
    PIO_Offset bsize;     /* Size of the block. */
    PIO_Offset bsizeg;    /* Size of gap block. */
    PIO_Offset blklensum; /* Sum of all block lengths. */
    PIO_Offset *del_arr = NULL; /* Array of deltas between adjacent elements in arr_in. */

    /* Check inputs. */
    pioassert(arrlen > 0 && arr_in, "invalid input", __FILE__, __LINE__);

    if (arrlen > 1)
    {
        if (!(del_arr = malloc((arrlen - 1) * sizeof(PIO_Offset))))
        {
            return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                            "Internal error while calculating GCD block size. Out of memory allocating %lld bytes for internal array", (unsigned long long) ((arrlen - 1) * sizeof(PIO_Offset)));
        }
    }

    /* Count the number of contiguous blocks in arr_in. If any if
       these blocks is of size 1, we are done and can return.
       Otherwise numtimes is the number of blocks. */
    for (int i = 0; i < arrlen - 1; i++)
    {
        del_arr[i] = arr_in[i + 1] - arr_in[i];
        if (del_arr[i] != 1)
        {
            numtimes++;
            if ( i > 0 && del_arr[i - 1] > 1)
            {
                free(del_arr);
                del_arr = NULL;
                return(1);
            }
        }
    }

    /* If numtimes is 0 the all of the data in arr_in is contiguous
     * and numblks=1. Not sure why I have three different variables
     * here, seems like n,numblks and numtimes could be combined. */
    numblks = numtimes + 1;
    if (numtimes == 0)
        n = numblks;
    else
        n = numtimes;

    /* If numblks==1 then the result is arrlen and you can return. */
    bsize = (PIO_Offset)arrlen;
    if (numblks > 1)
    {
        PIO_Offset blk_len[numblks];
        PIO_Offset gaps[numtimes];

        /* If numblks > 1 then numtimes must be > 0 and this if block
         * isn't needed. */
        if (numtimes > 0)
        {
            ii = 0;
            for (int i = 0; i < arrlen - 1; i++)
                if (del_arr[i] > 1)
                    gaps[ii++] = del_arr[i] - 1;
            numgaps = ii;
        }

        /* If numblks > 1 then arrlen must be > 1 */
        PIO_Offset *loc_arr = calloc(arrlen - 1, sizeof(PIO_Offset));
        if (!loc_arr)
        {
            return pio_err(NULL, NULL, PIO_ENOMEM, __FILE__, __LINE__,
                            "Internal error while calculating GCD block size. Out of memory allocating %lld bytes for internal array", (unsigned long long) ((arrlen -1) * sizeof(PIO_Offset)));
        }

        j = 0;
        /* If numblks > 1 then n must be <= (arrlen - 1) */
        for (int i = 0; i < n; i++)
            loc_arr[i] = 1;

        for (int i = 0; i < arrlen - 1; i++)
            if(del_arr[i] != 1)
                loc_arr[j++] = i;

        /* This is handled differently from the Fortran version in PIO1,
         * since array index is 1-based in Fortran and 0-based in C.
         * Original Fortran code: blk_len(1) = loc_arr(1)
         * Converted C code (incorrect): blk_len[0] = loc_arr[0];
         * Converted C code (correct): blk_len[0] = loc_arr[0] + 1;
         * For example, if loc_arr[0] is 2, the first block actually
         * has 3 elements with indices 0, 1 and 2. */
        blk_len[0] = loc_arr[0] + 1;
        blklensum = blk_len[0];
        /* If numblks > 1 then numblks must be <= arrlen */
        for(int i = 1; i < numblks - 1; i++)
        {
            blk_len[i] = loc_arr[i] - loc_arr[i - 1];
            blklensum += blk_len[i];
        }
        free(loc_arr);
        loc_arr = NULL;
        blk_len[numblks - 1] = arrlen - blklensum;

        /* Get the GCD in blk_len array. */
        bsize = lgcd_array(numblks, blk_len);

        /* I don't recall why i needed these next two blocks, I
         * remember struggling to get this right in all cases and I'm
         * afraid that the end result is that bsize is almost always
         * 1. */
        if (numgaps > 0)
        {
            bsizeg = lgcd_array(numgaps, gaps);
            bsize = lgcd(bsize, bsizeg);
        }

        /* ??? */
        if (arr_in[0] > 0)
            bsize = lgcd(bsize, arr_in[0]);
    }

    free(del_arr);
    del_arr = NULL;

    return bsize;
}

/**
 * Look for the largest block of data for io which can be expressed in
 * terms of start and count (ignore gaps).
 *
 * @param arrlen
 * @param arr_in
 * @returns the size of the block
 */
PIO_Offset GCDblocksize(int arrlen, const PIO_Offset *arr_in)
{
    /* Check inputs. */
    pioassert(arrlen > 0 && arr_in && arr_in[0] >= 0, "invalid input", __FILE__, __LINE__);

    /* If theres is only one contiguous block with length 1,
     * the result must be 1 and we can return. */
    if (arrlen == 1)
        return 1;

    /* We can use the array length as the initial value. 
     * Suppose we have n contiguous blocks with lengths
     * b1, b2, ..., bn, then gcd(b1, b2, ..., bn) =
     * gcd(b1 + b2 + ... + bn, b1, b2, ..., bn) =
     * gcd(arrlen, b1, b2, ..., bn) */
    PIO_Offset bsize = arrlen;

    /* The minimum length of a block is 1. */
    PIO_Offset blk_len = 1;

    for (int i = 0; i < arrlen - 1; i++)
    {
        pioassert(arr_in[i + 1] >= 0, "invalid input", __FILE__, __LINE__);

        if ((arr_in[i + 1] - arr_in[i]) == 1)
        {
            /* Still in a contiguous block. */
            blk_len++;
        }
        else
        {
            /* The end of a block has been reached. */
            if (blk_len == 1)
                return 1;

            bsize = lgcd(bsize, blk_len);
            if (bsize == 1)
              return 1;

            /* Continue to find next block. */
            blk_len = 1;
        }
    }

    /* Handle the last block. */
    bsize = lgcd(bsize, blk_len);

    return bsize;
}

/**
 * Compute start and count values for each io task. This is used in
 * PIOc_InitDecomp() for the box rearranger only.
 *
 * @param pio_type the PIO data type used in this decomposition.
 * @param ndims the number of dimensions in the variable, not
 * including the unlimited dimension.
 * @param gdims an array of global size of each dimension.
 * @param num_io_procs the number of IO tasks.
 * @param myiorank rank of this task in IO communicator.
 * @param start array of length ndims with data start values.
 * @param count array of length ndims with data count values.
 * @param num_aiotasks the number of IO tasks actually used
 * @returns 0 for success, error code otherwise.
 */
int CalcStartandCount(int pio_type, int ndims, const int *gdims, int num_io_procs,
                      int myiorank, PIO_Offset *start, PIO_Offset *count, int *num_aiotasks)
{
    int base_size; /* Size in bytes of base data type. */
    long int total_data_size; /* Total data size in bytes. */
    int use_io_procs; /* Number of IO tasks that will be actually used. */
    int i;
    int tiorank;
    int ioprocs;
    int tioprocs;
    int ret;

    /* Check inputs. */
    pioassert(pio_type > 0 && ndims > 0 && gdims && num_io_procs > 0 && start && count,
              "invalid input", __FILE__, __LINE__);
    LOG((1, "CalcStartandCount pio_type = %d ndims = %d num_io_procs = %d myiorank = %d",
         pio_type, ndims, num_io_procs, myiorank));

    /* Determine the size of the data type. */
    if ((ret = find_mpi_type(pio_type, NULL, &base_size)))
    {
        return pio_err(NULL, NULL, ret, __FILE__, __LINE__,
                        "Internal error while calculating start/count for I/O decomposition. Finding MPI type corresponding to PIO type (%d) failed", pio_type);
    }

    /* Find the total size of the data. */
    total_data_size = base_size;
    for (i = 0; i < ndims; i++)
        total_data_size *= (long int)gdims[i];

    /* Reduce the number of ioprocs that are needed so that we have at least
     * blocksize data (on avearge) on each iotask. */
    use_io_procs = max(1, min((int)(total_data_size / blocksize), num_io_procs));

    /* The partition algorithm below requires that use_io_procs is continuously
     * divisible by each outer dimension length, until the quotient is less than
     * or equal to an inner dimension length to terminate at that dimension.
     *
     * For decomposition D_1 x D_2 x ... X D_n, use_io_procs does not exceed the
     * product (assume that blocksize > base_size). Reduce use_io_procs as little
     * as possible, such that we have:
     * use_io_procs = D_1 X D_2 x ... X D_s x d, where 0 <= s < n and d <= D_(s+1)
     *
     * On D_1, D_2, ..., D_s, each IO task has count fixed as 1 and the partition
     * proecess continues on next dimension.
     *
     * On D_(s+1), each IO task has count at least D_(s+1) / d (left over data
     * distributed to some tasks) and the partition process ends. */
    int gdims_partial_product = 1;
    for (i = 0; i < ndims; i++)
    {
        if (gdims_partial_product * (long int)gdims[i] < (long int)use_io_procs)
            gdims_partial_product *= gdims[i];
        else
            break;
    }
    assert(gdims_partial_product >= 1 && gdims_partial_product <= use_io_procs);
    use_io_procs -= (use_io_procs % gdims_partial_product);

    /* On IO tasks, compute values for start/count arrays.
     * On non-IO tasks, set start/count to zero. */
    if (myiorank < use_io_procs)
    {
        /* Set default start/count */
        for (i = 0; i < ndims; i++)
        {
            start[i] = 0;
            count[i] = gdims[i];
        }

        if (use_io_procs > 1)
        {
            ioprocs = use_io_procs;
            tiorank = myiorank;
            for (i = 0; i < ndims; i++)
            {
                if (gdims[i] >= ioprocs)
                {
                    compute_one_dim(gdims[i], ioprocs, tiorank, &start[i], &count[i]);
                    assert(start[i] + count[i] <= gdims[i]);
                    break; /* Terminate on this dimension */
                }
                else if (gdims[i] > 1)
                {
                    assert(ioprocs % gdims[i] == 0);
                    tioprocs = gdims[i];
                    tiorank = (myiorank * tioprocs) / ioprocs;
                    compute_one_dim(gdims[i], tioprocs, tiorank, &start[i], &count[i]);
                    ioprocs = ioprocs / tioprocs;
                    tiorank  = myiorank % ioprocs;
                }
            }
        }
    }
    else
    {
        for (i = 0; i < ndims; i++)
        {
            start[i] = 0;
            count[i] = 0;
        }
    }

    /* Return the number of IO procs used to the caller. */
    *num_aiotasks = use_io_procs;
    
    return PIO_NOERR;
}
