
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <unistd.h> // usleep
#include <mpi.h>
#include "pio.h"
#include "adios_read.h"
#ifdef TIMING
#include <gptl.h>
#endif

using namespace std;


MPI_Comm comm = MPI_COMM_WORLD;
int mpirank;
int nproc;


/** The ID for the parallel I/O system. It is set by
 * PIOc_Init_Intracomm(). It references an internal structure
 * containing the general IO subsystem data and MPI
 * structure. It is passed to PIOc_finalize() to free
 * associated resources, after all I/O, but before
 * MPI_Finalize is called. */
int iosysid;

/** The ncid of the netCDF file created in this example. */
int ncid;

/* Number of processes that wrote the BP file (read from the file) */
int n_bp_writers;

/* Timer functions to separate time for read (ADIOS) and write (PIO) */
double time_read, time_write;
double time_temp_read, time_temp_write;
void TimerInitialize() { time_read = 0.0; time_write = 0.0; }
#define TimerStart(x) { time_temp_##x = MPI_Wtime(); }
#define TimerStop(x) { time_##x += (MPI_Wtime() - time_temp_##x); }
void TimerReport(MPI_Comm comm)
{
    int nproc, rank;
    double tr_sum, tr_max;
    double tw_sum, tw_max;
    MPI_Comm_size(comm, &nproc);
    MPI_Comm_rank(comm, &rank);
    MPI_Reduce(&time_read, &tr_max, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
    MPI_Reduce(&time_read, &tr_sum, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
    MPI_Reduce(&time_write, &tw_max, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
    MPI_Reduce(&time_write, &tw_sum, 1, MPI_DOUBLE, MPI_SUM, 0, comm);

    if (!rank)
    {
        cout << "Timing information:     Max     Sum of all\n";
        cout.precision(2);
        cout << "ADIOS read time   = " << std::fixed << std::setw(8) << tr_max << "s " << std::setw(8) << tr_sum << "s\n";
        cout << "PIO  write time   = " << std::fixed << std::setw(8) << tw_max << "s " << std::setw(8) << tw_sum << "s\n";
    }
}
void TimerFinalize() {}

struct Dimension {
    int dimid;
    PIO_Offset dimvalue;
};

using DimensionMap = std::map<std::string,Dimension>;

struct Variable {
    int nc_varid;
    bool is_timed;
};

using VariableMap = std::map<std::string,Variable>;


void InitPIO()
{
    if (PIOc_Init_Intracomm(comm, nproc, 1,
            0, PIO_REARR_SUBSET, &iosysid))
        throw std::runtime_error("PIO initialization failed\n");
}

void FlushStdout(MPI_Comm comm)
{
    cout << std::flush;
    usleep((useconds_t)100);
    MPI_Barrier(comm);
}

std::vector<int> AssignWriteRanks(int n_bp_writers)
{
    if (!mpirank) cout << "The BP file was written by " << n_bp_writers << " processes\n";
    int nwb = n_bp_writers / nproc; // number of blocks to process
    int start_wb; // starting wb (in [0..nwb-1])
    if (mpirank < n_bp_writers % nproc)
        nwb++;

    if (mpirank < n_bp_writers % nproc)
        start_wb = mpirank * nwb;
    else
        start_wb = mpirank * nwb + n_bp_writers % nproc;
    cout << "Process " << mpirank << " start block = " << start_wb <<
            " number of blocks = " << nwb << std::endl;
    FlushStdout(comm);
    std::vector<int> blocks(nwb);
    for (int i=0; i<nwb; ++i)
        blocks[i]=start_wb+i;
    return blocks;
}

void ProcessGlobalFillmode(ADIOS_FILE *infile, int ncid)
{
    cout << "Process Global Fillmode: \n";
    int asize;
    void *fillmode;
    ADIOS_DATATYPES atype;
    adios_get_attr(infile, "/__pio__/fillmode", &atype, &asize, &fillmode);
    cout << "    set fillmode: " << *(int*)fillmode << std::endl;
    PIOc_set_fill(ncid, *(int*)fillmode, NULL);
    free(fillmode);
}

void ProcessVarAttributes(ADIOS_FILE *infile, int adios_varid, std::string varname, int ncid, int nc_varid)
{
    ADIOS_VARINFO *vi = adios_inq_var(infile, infile->var_namelist[adios_varid]);
    for (int i=0; i < vi->nattrs; i++)
    {
        cout << "    Attribute: " << infile->attr_namelist[vi->attr_ids[i]] << std::endl;
        int asize;
        char *adata;
        ADIOS_DATATYPES atype;
        adios_get_attr(infile, infile->attr_namelist[vi->attr_ids[i]], &atype, &asize, (void**)&adata);
        nc_type piotype = PIOc_get_nctype_from_adios_type(atype);
        char *attname = infile->attr_namelist[vi->attr_ids[i]]+varname.length()+1;
        cout << "        define PIO attribute: " << attname << ""
             << "  type=" << piotype << std::endl;
        int len = 1;
        if (atype == adios_string)
            len = strlen(adata);
        PIOc_put_att(ncid, nc_varid, attname, piotype, len, adata);
        free(adata);
    }
    adios_free_varinfo(vi);
}

void ProcessGlobalAttributes(ADIOS_FILE *infile, int ncid)
{
    cout << "Process Global Attributes: \n";
    for (int i=0; i < infile->nattrs; i++)
    {
        string a = infile->attr_namelist[i];
        if (a.find("pio_global/") != string::npos)
        {
            cout << "    Attribute: " << infile->attr_namelist[i] << std::endl;
            int asize;
            char *adata;
            ADIOS_DATATYPES atype;
            adios_get_attr(infile, infile->attr_namelist[i], &atype, &asize, (void**)&adata);
            nc_type piotype = PIOc_get_nctype_from_adios_type(atype);
            char *attname = infile->attr_namelist[i]+strlen("pio_global/");
            cout << "        define PIO attribute: " << attname << ""
                    << "  type=" << piotype << std::endl;
            int len = 1;
            if (atype == adios_string)
                len = strlen(adata);
            PIOc_put_att(ncid, PIO_GLOBAL, attname, piotype, len, adata);
            free(adata);
        }
    }
}

std::map<std::string,int> ProcessDecompositions(ADIOS_FILE * infile, int ncid, std::vector<int>& wblocks)
{
    std::map<std::string,int> decomp_map;
    for (int i = 0; i < infile->nvars; i++)
    {
        string v = infile->var_namelist[i];
        if (v.find("/__pio__/decomp/") != string::npos)
        {
            /* Read all decomposition blocks assigned to this process,
             * create one big array from them and
             * create a single big decomposition with PIO
             */
            string decompname = v.substr(16);
            if (!mpirank) cout << "Process decomposition " << decompname << endl;

            /* Sum the sizes of blocks assigned to this process */
            TimerStart(read);
            ADIOS_VARINFO *vi = adios_inq_var(infile, infile->var_namelist[i]);
            adios_inq_var_blockinfo(infile, vi);

            uint64_t nelems = 0;
            for (auto wb : wblocks)
            {
                nelems += vi->blockinfo[wb].count[0];
            }
            std::vector<PIO_Offset> d(nelems);
            uint64_t offset = 0;
            for (auto wb : wblocks)
            {
                cout << "    rank " << mpirank << ": read decomp wb = " << wb <<
                        " start = " << offset <<
                        " elems = " << vi->blockinfo[wb].count[0] << endl;
                ADIOS_SELECTION *wbsel = adios_selection_writeblock(wb);
                int ret = adios_schedule_read(infile, wbsel, infile->var_namelist[i], 0, 1,
                        d.data()+offset);
                adios_perform_reads(infile, 1);
                offset += vi->blockinfo[wb].count[0];
            }
            adios_free_varinfo(vi);
            string attname = string(infile->var_namelist[i]) + "/piotype";
            int asize;
            int *piotype;
            ADIOS_DATATYPES atype;
            adios_get_attr(infile, attname.c_str(), &atype, &asize, (void**)&piotype);

            attname = string(infile->var_namelist[i]) + "/ndims";
            int *decomp_ndims;
            adios_get_attr(infile, attname.c_str(), &atype, &asize, (void**)&decomp_ndims);

            int *decomp_dims;
            attname = string(infile->var_namelist[i]) + "/dimlen";
            adios_get_attr(infile, attname.c_str(), &atype, &asize, (void**)&decomp_dims);
            TimerStop(read);

            TimerStart(write);
            int ioid;
            PIOc_InitDecomp(iosysid, *piotype, *decomp_ndims, decomp_dims, (PIO_Offset)nelems,
                           d.data(), &ioid, NULL, NULL, NULL);
            TimerStop(write);

            free(piotype);
            free(decomp_ndims);
            free(decomp_dims);
            decomp_map[decompname] = ioid;
        }
        FlushStdout(comm);
    }
    return decomp_map;
}

DimensionMap ProcessDimensions(ADIOS_FILE * infile, int ncid)
{
    DimensionMap dimensions_map;
    for (int i = 0; i < infile->nvars; i++)
    {
        string v = infile->var_namelist[i];
        if (v.find("/__pio__/dim/") != string::npos)
        {
            /* For each dimension stored, define a dimension variable with PIO */
            string dimname = v.substr(13);
            if (!mpirank) cout << "Process dimension " << dimname << endl;

            unsigned long long dimval;
            TimerStart(read);
            int ret = adios_schedule_read(infile, NULL, infile->var_namelist[i], 0, 1, &dimval);
            adios_perform_reads(infile, 1);
            TimerStop(read);
            int dimid;
            TimerStart(write);
            PIOc_def_dim(ncid, dimname.c_str(), (PIO_Offset)dimval, &dimid);
            TimerStop(write);
            dimensions_map[dimname] = Dimension{dimid,(PIO_Offset)dimval};
        }
        FlushStdout(comm);
    }
    return dimensions_map;
}

VariableMap ProcessVariableDefinitions(ADIOS_FILE * infile, int ncid, DimensionMap& dimension_map)
{
    VariableMap vars_map;
    for (int i = 0; i < infile->nvars; i++)
    {
        string v = infile->var_namelist[i];
        if (v.find("/__") == string::npos)
        {
            /* For each variable written define it with PIO */
            if (!mpirank) cout << "Process variable " << v << endl;

            TimerStart(read);
            string attname = string(infile->var_namelist[i]) + "/__pio__/nctype";
            int asize;
            int *nctype;
            ADIOS_DATATYPES atype;
            adios_get_attr(infile, attname.c_str(), &atype, &asize, (void**)&nctype);

            attname = string(infile->var_namelist[i]) + "/__pio__/ndims";
            int *ndims;
            adios_get_attr(infile, attname.c_str(), &atype, &asize, (void**)&ndims);

            char **dimnames = NULL;
            int dimids[MAX_NC_DIMS];
            bool timed = false;
            if (*ndims)
            {
                attname = string(infile->var_namelist[i]) + "/__pio__/dims";
                adios_get_attr(infile, attname.c_str(), &atype, &asize, (void**)&dimnames);

                for (int d=0; d < *ndims; d++)
                {
                    //cout << "Dim " << d << " = " <<  dimnames[d] << endl;
                    dimids[d] = dimension_map[dimnames[d]].dimid;
                    if (dimension_map[dimnames[d]].dimvalue == PIO_UNLIMITED)
                    {
                        timed = true;
                    }
                }
            }
            TimerStop(read);

            TimerStart(write);
            int varid;
            PIOc_def_var(ncid, v.c_str(), *nctype, *ndims, dimids, &varid);
            TimerStop(write);
            vars_map[v] = Variable{varid,timed};

            if (!mpirank)
                ProcessVarAttributes(infile, i, v, ncid, varid);

            free(nctype);
            free(ndims);
            free(dimnames);
        }
        //else cout << "Skip var " << v << std::endl;
        FlushStdout(comm);
    }
    return vars_map;
}

int ConvertVariablePutVar(ADIOS_FILE * infile, int adios_varid, int ncid, Variable var)
{
    TimerStart(read);
    int ret = 0;
    ADIOS_VARINFO *vi = adios_inq_var(infile, infile->var_namelist[adios_varid]);

    if (vi->ndim == 0)
    {
        /* Scalar variable */
        TimerStart(write);
        int ret = PIOc_put_var(ncid, var.nc_varid, vi->value);
        if (ret != PIO_NOERR)
            cout << "ERROR in PIOc_put_var(), code = " << ret
            << " at " << __func__ << ":" << __LINE__ << endl;
        TimerStop(write);
    }
    else
    {
        /* An N-dimensional array that needs no rearrangement.
         * put_vara() needs all processes participate */
        TimerStart(read);
        /* We do 1-dim decomposition only, even if some process will read 0 bytes */
        uint64_t mydims[vi->ndim];
        uint64_t offsets[vi->ndim];
        mydims[0] = vi->dims[0] / nproc;
        if (mpirank < vi->dims[0] % nproc)
        {
            ++mydims[0];
            offsets[0] = mydims[0] * mpirank;
        }
        else
        {
            offsets[0] = mydims[0] * mpirank + vi->dims[0] % nproc;
        }
        uint64_t nelems = mydims[0];
        for (int d=1; d < vi->ndim; ++d)
        {
            mydims[d] = vi->dims[d];
            offsets[d] = 0;
            nelems *= vi->dims[d];
        }
        ADIOS_SELECTION * box = adios_selection_boundingbox(vi->ndim, offsets, mydims);
        cout << "    rank " << mpirank << ": read var with 1D decomposition: "
             << " offset[0] = " << offsets[0]
             << " count[0] = " << mydims[0]
             << " elems = " << nelems << endl;

        size_t mysize = (size_t)nelems * adios_type_size(vi->type, NULL);
        char * buf = (char *)malloc(mysize);

        adios_schedule_read(infile, box, infile->var_namelist[adios_varid], 0, 1, buf);
        adios_perform_reads(infile, 1);
        adios_selection_delete(box);
        TimerStop(read);

        TimerStart(write);
        PIO_Offset start[MAX_NC_DIMS], count[MAX_NC_DIMS];
        for (int d=0; d < vi->ndim; ++d)
        {
            start[d] = (PIO_Offset) offsets[d];
            count[d] = (PIO_Offset) mydims[d];
        }
        ret = PIOc_put_vara(ncid, var.nc_varid, start, count, buf);
        if (ret != PIO_NOERR)
            cout << "rank " << mpirank << ":ERROR in PIOc_put_vara(), code = " << ret
                 << " at " << __func__ << ":" << __LINE__ << endl;
        TimerStop(write);
        free(buf);
    }
    adios_free_varinfo(vi);
    return ret;
}

int ConvertVariableTimedPutVar(ADIOS_FILE * infile, int adios_varid, int ncid, Variable var, int nblocks_per_step)
{
    TimerStart(read);
    int ret = 0;
    ADIOS_VARINFO *vi = adios_inq_var(infile, infile->var_namelist[adios_varid]);

    if (vi->ndim == 0)
    {
        /* Scalar variable over time */
        /* Written by only one process, so steps = number of blocks in file */
        int nsteps = vi->nblocks[0];
        TimerStart(read);
        adios_inq_var_stat(infile, vi, 0, 1);
        TimerStart(read);
        PIO_Offset start[1], count[1];
        for (int ts=0; ts < nsteps; ++ts)
        {
            TimerStart(write);
            start[0] = ts;
            count[0] = 1;
            //cout << "DBG: " << infile->var_namelist[adios_varid] << " step " << ts
            //     << " value = " << *(int*) vi->statistics->blocks->mins[ts] << endl;
            int ret = PIOc_put_vara(ncid, var.nc_varid, start, count, vi->statistics->blocks->mins[ts]);
            if (ret != PIO_NOERR)
                cout << "ERROR in PIOc_put_var(), code = " << ret
                << " at " << __func__ << ":" << __LINE__ << endl;
            TimerStop(write);
        }
    }
    else
    {
        /* calculate how many records/steps we have for this variable */
        int nsteps = 1;
        if (var.is_timed)
        {
            nsteps = vi->nblocks[0] / nblocks_per_step;
        }
        if (vi->nblocks[0] != nsteps * nblocks_per_step)
        {
            cout << "rank " << mpirank << ":ERROR in processing variable '" << infile->var_namelist[adios_varid]
                 << "'. Number of blocks = " << vi->nblocks[0]
                 << " does not equal the number of steps * number of writers = "
                 << nsteps << " * " << nblocks_per_step << " = " << nsteps*nblocks_per_step
                 << endl;
        }

        /* Is this a local array written by each process, or a truly distributed global array */
        TimerStart(read);
        adios_inq_var_blockinfo(infile, vi);
        TimerStart(read);
        bool local_array = true;
        for (int d = 0; d < vi->ndim; d++)
        {
            if (vi->blockinfo[0].count[d] != vi->dims[d])
            {
                local_array = false;
                break;
            }
        }

        if (local_array)
        {
            /* Just read the arrays written by rank 0 (on every process here) and
             * write it collectively.
             */
            for (int ts=0; ts < nsteps; ++ts)
            {
                TimerStart(read);
                int elemsize = adios_type_size(vi->type,NULL);
                uint64_t nelems = 1;
                for (int d = 0; d < vi->ndim; d++)
                {
                    nelems *= vi->dims[d];
                }
                std::vector<char> d(nelems * elemsize);
                ADIOS_SELECTION *wbsel = adios_selection_writeblock(ts);
                int ret = adios_schedule_read(infile, wbsel, infile->var_namelist[adios_varid],
                        0, 1, d.data());
                adios_perform_reads(infile, 1);
                TimerStop(read);

                TimerStart(write);
                PIO_Offset start[vi->ndim+1], count[vi->ndim+1];
                start[0] = ts;
                count[0] = 1;
                for (int d = 0; d < vi->ndim; d++)
                {
                    start[d+1] = 0;
                    count[d+1] = vi->dims[d];
                }
                if ((ret = PIOc_put_vara(ncid, var.nc_varid, start, count, d.data())))
                if (ret != PIO_NOERR)
                    cout << "ERROR in PIOc_put_var(), code = " << ret
                    << " at " << __func__ << ":" << __LINE__ << endl;
                TimerStop(write);
            }
        }
        else
        {
            cout << "ERROR: put_vara of arrays over time is not supported yet. "
                    << "Variable \"" << infile->var_namelist[adios_varid] << "\" is a "
                    << vi->ndim << "D array including the unlimited dimension"
                    << endl;
        }
    }
    adios_free_varinfo(vi);
    return ret;
}

int ConvertVariableDarray(ADIOS_FILE * infile, int adios_varid, int ncid, Variable var,
        std::vector<int>& wblocks, std::map<std::string,int>& decomp_map, int nblocks_per_step)
{
    int ret = 0;
    string attname = string(infile->var_namelist[adios_varid]) + "/__pio__/decomp";
    int asize;
    ADIOS_DATATYPES atype;
    char *decompname;
    adios_get_attr(infile, attname.c_str(), &atype, &asize, (void**)&decompname);
    int decompid = decomp_map[decompname];
    free(decompname);
    //attname = string(infile->var_namelist[adios_varid]) + "/__pio__/timed";
    //char *is_timed_val;
    //adios_get_attr(infile, attname.c_str(), &atype, &asize, (void**)&is_timed_val);
    //char is_timed = *is_timed_val;
    //free(is_timed_val);

    ADIOS_VARINFO *vi = adios_inq_var(infile, infile->var_namelist[adios_varid]);
    adios_inq_var_blockinfo(infile, vi);

    /* calculate how many records/steps we have for this variable */
    int nsteps = 1;
    if (var.is_timed)
    {
        nsteps = vi->nblocks[0] / nblocks_per_step;
    }
    if (vi->nblocks[0] != nsteps * nblocks_per_step)
    {
        cout << "rank " << mpirank << ":ERROR in processing darray '" << infile->var_namelist[adios_varid]
             << "'. Number of blocks = " << vi->nblocks[0]
             << " does not equal the number of steps * number of writers = "
             << nsteps << " * " << nblocks_per_step << " = " << nsteps*nblocks_per_step
             << endl;
    }


    for (int ts = 0; ts < nsteps; ++ts)
    {
        /* Sum the sizes of blocks assigned to this process */
        uint64_t nelems = 0;
        for (auto wb : wblocks)
        {
            if (wb < vi->nblocks[0])
                nelems += vi->blockinfo[wb*nsteps+ts].count[0];
        }
        int elemsize = adios_type_size(vi->type,NULL);
        std::vector<char> d(nelems * elemsize);
        uint64_t offset = 0;
        for (auto wb : wblocks)
        {
            int blockid = wb*nsteps+ts;
            if (blockid < vi->nblocks[0])
            {
                cout << "    rank " << mpirank << ": read var = " << blockid <<
                        " start byte = " << offset <<
                        " elems = " << vi->blockinfo[blockid].count[0] << endl;
                ADIOS_SELECTION *wbsel = adios_selection_writeblock(blockid);
                int ret = adios_schedule_read(infile, wbsel, infile->var_namelist[adios_varid], 0, 1,
                        d.data()+offset);
                adios_perform_reads(infile, 1);
                offset += vi->blockinfo[blockid].count[0] * elemsize;
            }
        }
        TimerStop(read);

        TimerStart(write);
        if (wblocks[0] < nblocks_per_step)
        {
            if (var.is_timed)
                PIOc_setframe(ncid, var.nc_varid, ts);
            ret = PIOc_write_darray(ncid, var.nc_varid, decompid, (PIO_Offset)nelems,
                    d.data(), NULL);
        }
        TimerStop(write);
    }
    adios_free_varinfo(vi);

    return ret;
}

void ConvertBPFile(string infilename, string outfilename, int pio_iotype)
{
    TimerStart(read);
    ADIOS_FILE * infile = adios_read_open_file(infilename.c_str(), ADIOS_READ_METHOD_BP, comm);
    TimerStop(read);
    if (!infile)
        throw std::runtime_error("ADIOS: "+ string(adios_errmsg()) + "\n");

    try {
        ncid = -1;
        TimerStart(read);
        int ret = adios_schedule_read(infile, NULL, "/__pio__/info/nproc", 0, 1, &n_bp_writers);
        if (ret)
            throw std::runtime_error("Invalid BP file: missing '/__pio__/info/nproc' variable\n");
        adios_perform_reads(infile, 1);
        TimerStop(read);

        /* Number of BP file writers != number of processes here */
        std::vector<int> wblocks; // from-to writeblocks
        wblocks = AssignWriteRanks(n_bp_writers);

        /* First process decompositions */
        std::map<std::string,int> decomp_map = ProcessDecompositions(infile, ncid, wblocks);

        /* Create output file */
        TimerStart(write);
        ret = PIOc_createfile(iosysid, &ncid, &pio_iotype, outfilename.c_str(), PIO_CLOBBER);
        TimerStop(write);
        if (ret)
            throw std::runtime_error("Could not create output file " + outfilename + "\n");

        /* Process the global fillmode */
        ProcessGlobalFillmode(infile, ncid);

        /* Next process dimensions */
        DimensionMap dimension_map = ProcessDimensions(infile, ncid);

        /* For each variable, define a variable with PIO */
        VariableMap vars_map = ProcessVariableDefinitions(infile, ncid, dimension_map);

        /* Process the global attributes */
        ProcessGlobalAttributes(infile, ncid);

        PIOc_enddef(ncid);


        /* For each variable,
         * read in the data with ADIOS then write it out with PIO */
        for (int i = 0; i < infile->nvars; i++)
        {
            string v = infile->var_namelist[i];
            if (v.find("/__") == string::npos)
            {
                /* For each variable, read with ADIOS then write with PIO */
                if (!mpirank) cout << "Convert variable " << v << endl;
                Variable& var = vars_map[v];

                TimerStart(read);
                string attname = string(infile->var_namelist[i]) + "/__pio__/ncop";
                int asize;
                char *ncop;
                ADIOS_DATATYPES atype;
                adios_get_attr(infile, attname.c_str(), &atype, &asize, (void**)&ncop);
                TimerStop(read);

                std::string op(ncop);
                if (op == "put_var")
                {
                    if (var.is_timed)
                    {
                        ConvertVariableTimedPutVar(infile, i, ncid, var, n_bp_writers);
                    }
                    else
                    {
                        ConvertVariablePutVar(infile, i, ncid, var);
                    }
                }
                else if (op == "darray")
                {
                    /* Variable was written with pio_write_darray() with a decomposition */
                    ConvertVariableDarray(infile, i, ncid, var, wblocks, decomp_map, n_bp_writers);
                }
                else
                {
                    if (!mpirank)
                        cout << "  WARNING: unknown operation " << op << ". Will not process this variable\n";
                }
                free(ncop);
            }
            FlushStdout(comm);
        }
        TimerStart(write);
        ret = PIOc_sync(ncid);
        ret = PIOc_closefile(ncid);
        TimerStop(write);
        TimerStart(read);
        adios_read_close(infile);
        TimerStop(read);
    }
    catch (std::exception &e)
    {
        if (ncid > -1)
            PIOc_closefile(ncid);
        adios_read_close(infile);
        throw e;
    }
}

void usage(string prgname)
{
    if (!mpirank) {
        cout << "Usage: " << prgname << " bp_file  nc_file  pio_io_type\n";
        cout << "   bp file   :  data produced by PIO with ADIOS format\n";
        cout << "   nc file   :  output file name after conversion\n";
        cout << "   pio format:  output PIO_IO_TYPE. Supported parameters:\n";
        cout << "                pnetcdf  netcdf  netcdf4c  netcdf4p   or:\n";
        cout << "                   1       2        3         4\n";
    }
}

enum PIO_IOTYPE GetIOType(string t)
{
    enum PIO_IOTYPE iotype = PIO_IOTYPE_NETCDF;
    if (t == "pnetcdf" || t == "PNETCDF" || t == "1")
    {
        iotype = PIO_IOTYPE_PNETCDF;
    }
    else if (t == "netcdf" || t == "NETCDF" || t == "2")
    {
        iotype = PIO_IOTYPE_NETCDF;
    }
    else if (t == "netcdf4c" || t == "NETCDF4C" || t == "3")
    {
        iotype = PIO_IOTYPE_NETCDF4C;
    }
    else if (t == "netcdf4p" || t == "NETCDF4P" || t == "4")
    {
        iotype = PIO_IOTYPE_NETCDF4P;
    }
    else
    {
        throw invalid_argument("Invalid conversion type given: " + t + "\n");
    }
    return iotype;
}

int main (int argc, char *argv[])
{
    int ret = 0;
    MPI_Init(&argc, &argv);
    MPI_Comm_set_errhandler(comm, MPI_ERRORS_RETURN);
    MPI_Comm_rank(comm, &mpirank);
    MPI_Comm_size(comm, &nproc);

    if (argc < 4) {
        usage(argv[0]);
        return 1;
    }
    
    string infilename = argv[1];
    string outfilename = argv[2];

    #ifdef TIMING
    /* Initialize the GPTL timing library. */
    if ((ret = GPTLinitialize ()))
        return ret;
#endif

    TimerInitialize();

    try {
        enum PIO_IOTYPE pio_iotype = GetIOType(argv[3]);
        InitPIO();
        ConvertBPFile(infilename, outfilename, pio_iotype);
        PIOc_finalize(iosysid);
        TimerReport(comm);
    }
    catch (std::invalid_argument &e)
    {
        std::cout << e.what() << std::endl;
        usage(argv[0]);
        return 2;
    }
    catch (std::exception &e)
    {
        std::cout << e.what() << std::endl;
        return 3;
    }

    MPI_Finalize();
    TimerFinalize();
#ifdef TIMING
    /* Finalize the GPTL timing library. */
    if ((ret = GPTLfinalize ()))
        return ret;
#endif
    return ret;
}
