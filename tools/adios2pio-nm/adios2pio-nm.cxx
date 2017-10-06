
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <stdexcept>
#include <unistd.h> // usleep
#include <mpi.h>
#include <sys/types.h>
#include <dirent.h>
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
    nc_type nctype;
};

using VariableMap = std::map<std::string,Variable>;

struct Decomposition {
    int ioid;
    int piotype;
};

using DecompositionMap = std::map<std::string,Decomposition>;

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

/* Set the currently encountered max number of steps if argument is given.
 * Return the max step currently
 */
int GlobalMaxSteps(int nsteps_in=0)
{
    static int nsteps_current = 1;
    if (nsteps_in > nsteps_current)
        nsteps_current = nsteps_in;
    return nsteps_current;
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

void ProcessGlobalFillmode(ADIOS_FILE **infile, int ncid)
{
    cout << "Process Global Fillmode: \n";
    int asize;
    void *fillmode;
    ADIOS_DATATYPES atype;
    adios_get_attr(infile[0], "/__pio__/fillmode", &atype, &asize, &fillmode);
    cout << "    set fillmode: " << *(int*)fillmode << std::endl;
    PIOc_set_fill(ncid, *(int*)fillmode, NULL);
    free(fillmode);
}

void ProcessVarAttributes(ADIOS_FILE **infile, int adios_varid, std::string varname, int ncid, int nc_varid)
{
    ADIOS_VARINFO *vi = adios_inq_var(infile[0], infile[0]->var_namelist[adios_varid]);
    for (int i=0; i < vi->nattrs; i++)
    {
        cout << "    Attribute: " << infile[0]->attr_namelist[vi->attr_ids[i]] << std::endl;
        int asize;
        char *adata;
        ADIOS_DATATYPES atype;
        adios_get_attr(infile[0], infile[0]->attr_namelist[vi->attr_ids[i]], &atype, &asize, (void**)&adata);
        nc_type piotype = PIOc_get_nctype_from_adios_type(atype);
        char *attname = infile[0]->attr_namelist[vi->attr_ids[i]]+varname.length()+1;
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

void ProcessGlobalAttributes(ADIOS_FILE **infile, int ncid)
{
    cout << "Process Global Attributes: \n";
    for (int i=0; i < infile[0]->nattrs; i++)
    {
        string a = infile[0]->attr_namelist[i];
        if (a.find("pio_global/") != string::npos)
        {
            cout << "    Attribute: " << infile[0]->attr_namelist[i] << std::endl;
            int asize;
            char *adata;
            ADIOS_DATATYPES atype;
            adios_get_attr(infile[0], infile[0]->attr_namelist[i], &atype, &asize, (void**)&adata);
            nc_type piotype = PIOc_get_nctype_from_adios_type(atype);
            char *attname = infile[0]->attr_namelist[i]+strlen("pio_global/");
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

Decomposition ProcessOneDecomposition(ADIOS_FILE **infile, int ncid, const char *varname, std::vector<int>& wblocks,
        int forced_type=NC_NAT)
{
    /* 
 	 * Read all decomposition blocks assigned to this process,
     * create one big array from them and create a single big 
     * decomposition with PIO
     */

    /* Sum the sizes of blocks assigned to this process */
    TimerStart(read);
    uint64_t nelems = 0;
    for (int i=1;i<=wblocks.size();i++) { // iterate over all the files assigned to this process
   		ADIOS_VARINFO *vb = adios_inq_var(infile[i], varname);
		if (vb) {
			adios_inq_var_blockinfo(infile[i], vb);

			// Assuming all time steps have the same number of writer blocks	
			for (int j=0;j<vb->nblocks[0];j++) 
				nelems += vb->blockinfo[j].count[0];
			adios_free_varinfo(vb);
		}
	}

    std::vector<PIO_Offset> d(nelems);
    uint64_t offset = 0;
    for (int i=1;i<=wblocks.size();i++) {
		ADIOS_VARINFO *vb = adios_inq_var(infile[i], varname);
		if (vb) {
       		adios_inq_var_blockinfo(infile[i], vb);
	   		// Assuming all time steps have the same number of writer blocks	
	   		for (int j=0;j<vb->nblocks[0];j++) {
       		 	cout << " rank " << mpirank << ": read decomp wb = " << j <<
       		         	" start = " << offset <<
       		         	" elems = " << vb->blockinfo[j].count[0] << endl;
				ADIOS_SELECTION *wbsel = adios_selection_writeblock(j);
       		 	int ret = adios_schedule_read(infile[i], wbsel, varname, 0, 1, d.data()+offset);
       		 	adios_perform_reads(infile[i], 1);
				offset += vb->blockinfo[j].count[0];
	   		}
	   		adios_free_varinfo(vb);
		}
	}

    string attname;
    int asize;
    int *piotype;
    ADIOS_DATATYPES atype;
    if (forced_type == NC_NAT) {
        attname = string(varname) + "/piotype";
        adios_get_attr(infile[0], attname.c_str(), &atype, &asize, (void**)&piotype);
    } else {
        piotype = (int*) malloc(sizeof(int));
        *piotype = forced_type;
    }
    attname = string(varname) + "/ndims";
    int *decomp_ndims;
    adios_get_attr(infile[0], attname.c_str(), &atype, &asize, (void**)&decomp_ndims);

    int *decomp_dims;
    attname = string(varname) + "/dimlen";
    adios_get_attr(infile[0], attname.c_str(), &atype, &asize, (void**)&decomp_dims);
    TimerStop(read);

    TimerStart(write);
    int ioid;
    PIOc_InitDecomp(iosysid, *piotype, *decomp_ndims, decomp_dims, (PIO_Offset)nelems,
            		d.data(), &ioid, NULL, NULL, NULL);
    TimerStop(write);

    free(piotype);
    free(decomp_ndims);
    free(decomp_dims);

    return Decomposition{ioid, *piotype};
}

DecompositionMap ProcessDecompositions(ADIOS_FILE **infile, int ncid, std::vector<int>& wblocks)
{
    DecompositionMap decomp_map;
    for (int i = 0; i < infile[0]->nvars; i++)
    {
        string v = infile[0]->var_namelist[i];
        if (v.find("/__pio__/decomp/") != string::npos)
        {
            string decompname = v.substr(16);
            if (!mpirank) cout << "Process decomposition " << decompname << endl;
            Decomposition d = ProcessOneDecomposition(infile, ncid, infile[0]->var_namelist[i], wblocks);
            decomp_map[decompname] = d;
        }
        FlushStdout(comm);
    }
    return decomp_map;
}

Decomposition GetNewDecomposition(DecompositionMap& decompmap, string decompname,
        ADIOS_FILE **infile, int ncid, std::vector<int>& wblocks, int nctype)
{
    stringstream ss;
    ss << decompname << "_" << nctype;
    string key = ss.str();
    auto it = decompmap.find(key);
    Decomposition d;
    if (it == decompmap.end())
    {
        string varname = "/__pio__/decomp/" + decompname;
        d = ProcessOneDecomposition(infile, ncid, varname.c_str(), wblocks, nctype);
        decompmap[key] = d;
    }
    else
    {
        d = it->second;
    }
    return d;
}

DimensionMap ProcessDimensions(ADIOS_FILE **infile, int ncid)
{
    DimensionMap dimensions_map;
    for (int i = 0; i < infile[0]->nvars; i++)
    {
        string v = infile[0]->var_namelist[i];
        if (v.find("/__pio__/dim/") != string::npos)
        {
            /* For each dimension stored, define a dimension variable with PIO */
            string dimname = v.substr(13);
            if (!mpirank) cout << "Process dimension " << dimname << endl;

            unsigned long long dimval;
            TimerStart(read);
            int ret = adios_schedule_read(infile[0], NULL, infile[0]->var_namelist[i], 0, 1, &dimval);
            adios_perform_reads(infile[0], 1);
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

VariableMap ProcessVariableDefinitions(ADIOS_FILE **infile, int ncid, DimensionMap& dimension_map)
{
    VariableMap vars_map;
    for (int i = 0; i < infile[0]->nvars; i++)
    {
        string v = infile[0]->var_namelist[i];
        if (v.find("/__") == string::npos)
        {
            /* For each variable written define it with PIO */
            if (!mpirank) cout << "Process variable " << v << endl;

            TimerStart(read);
            string attname = string(infile[0]->var_namelist[i]) + "/__pio__/nctype";
            int asize;
            int *nctype;
            ADIOS_DATATYPES atype;
            adios_get_attr(infile[0], attname.c_str(), &atype, &asize, (void**)&nctype);

            attname = string(infile[0]->var_namelist[i]) + "/__pio__/ndims";
            int *ndims;
            adios_get_attr(infile[0], attname.c_str(), &atype, &asize, (void**)&ndims);

            char **dimnames = NULL;
            int dimids[MAX_NC_DIMS];
            bool timed = false;
            if (*ndims)
            {
                attname = string(infile[0]->var_namelist[i]) + "/__pio__/dims";
                adios_get_attr(infile[0], attname.c_str(), &atype, &asize, (void**)&dimnames);

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
            vars_map[v] = Variable{varid,timed,*nctype};

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

int put_var(int ncid, int varid, int nctype, enum ADIOS_DATATYPES memtype, const void* buf)
{
    int ret = 0;
    switch(memtype)
    {
    case adios_byte:
        ret = PIOc_put_var_schar(ncid, varid, (const signed char*)buf);
        break;
    case adios_short:
        ret = PIOc_put_var_short(ncid, varid, (const signed short*)buf);
        break;
    case adios_integer:
        ret = PIOc_put_var_int(ncid, varid, (const signed int*)buf);
        break;
    case adios_real:
        ret = PIOc_put_var_float(ncid, varid, (const float *)buf);
        break;
    case adios_double:
        ret = PIOc_put_var_double(ncid, varid, (const double *)buf);
        break;
    case adios_unsigned_byte:
        ret = PIOc_put_var_uchar(ncid, varid, (const unsigned char *)buf);
        break;
    case adios_unsigned_short:
        ret = PIOc_put_var_ushort(ncid, varid, (const unsigned short *)buf);
        break;
    case adios_unsigned_integer:
        ret = PIOc_put_var_uint(ncid, varid, (const unsigned int *)buf);
        break;
    case adios_long:
        ret = PIOc_put_var_longlong(ncid, varid, (const signed long long *)buf);
        break;
    case adios_unsigned_long:
        ret = PIOc_put_var_ulonglong(ncid, varid, (const unsigned long long *)buf);
        break;
    case adios_string:
        ret = PIOc_put_var_text(ncid, varid, (const char *)buf);
        break;
    default:
        /* We can't do anything here, hope for the best, i.e. memtype equals to nctype */
        ret = PIOc_put_var(ncid, varid, buf);
        break;
    }
    return ret;
}

int put_vara(int ncid, int varid, int nctype, enum ADIOS_DATATYPES memtype, PIO_Offset *start, PIO_Offset *count, const void* buf)
{
    int ret = 0;
    switch(memtype)
    {
    case adios_byte:
        if (nctype == PIO_BYTE)
            ret = PIOc_put_vara_schar(ncid, varid, start, count, (const signed char*)buf);
        else
            ret = PIOc_put_vara_text(ncid, varid, start, count, (const char*)buf);
        break;
    case adios_short:
        ret = PIOc_put_vara_short(ncid, varid, start, count, (const signed short*)buf);
        break;
    case adios_integer:
        ret = PIOc_put_vara_int(ncid, varid, start, count, (const signed int*)buf);
        break;
    case adios_real:
        ret = PIOc_put_vara_float(ncid, varid, start, count, (const float *)buf);
        break;
    case adios_double:
        ret = PIOc_put_vara_double(ncid, varid, start, count, (const double *)buf);
        break;
    case adios_unsigned_byte:
        ret = PIOc_put_vara_uchar(ncid, varid, start, count, (const unsigned char *)buf);
        break;
    case adios_unsigned_short:
        ret = PIOc_put_vara_ushort(ncid, varid, start, count, (const unsigned short *)buf);
        break;
    case adios_unsigned_integer:
        ret = PIOc_put_vara_uint(ncid, varid, start, count, (const unsigned int *)buf);
        break;
    case adios_long:
        ret = PIOc_put_vara_longlong(ncid, varid, start, count, (const signed long long *)buf);
        break;
    case adios_unsigned_long:
        ret = PIOc_put_vara_ulonglong(ncid, varid, start, count, (const unsigned long long *)buf);
        break;
    case adios_string:
        ret = PIOc_put_vara_text(ncid, varid, start, count, (const char *)buf);
        break;
    default:
        /* We can't do anything here, hope for the best, i.e. memtype equals to nctype */
        ret = PIOc_put_vara(ncid, varid, start, count, buf);
        break;
    }
    return ret;
}

int ConvertVariablePutVar(ADIOS_FILE **infile, std::vector<int> wblocks, int adios_varid, int ncid, Variable& var)
{
    TimerStart(read);
    int ret = 0;
    ADIOS_VARINFO *vi = adios_inq_var(infile[0], infile[0]->var_namelist[adios_varid]);

    if (vi->ndim == 0)
    {
        /* Scalar variable */
        TimerStart(write);
		/* Read the variable from the files assigned to this processor */
		for (int i=0;i<=wblocks.size();i++) {
			ADIOS_VARINFO *vb = adios_inq_var(infile[i], infile[i]->var_namelist[adios_varid]);
            if (vb) {
        		int ret = put_var(ncid, var.nc_varid, var.nctype, vb->type, vb->value);
        		if (ret != PIO_NOERR)
            			cout << "ERROR in PIOc_put_var(), code = " << ret
            				<< " at " << __func__ << ":" << __LINE__ << endl;
				adios_free_varinfo(vb);
			}
		}
        TimerStop(write);
    }
    else
    {
        /* An N-dimensional array that needs no rearrangement.
         * put_vara() needs all processes participate */
        TimerStart(read);

		/* We are reading from multiple files.                       */
		/* Find the maximum number of blocks assigned to a process.  */
		int l_wbsize = 0;
		int g_wbsize = 0;
		for (int i=1;i<=wblocks.size();i++) {
			ADIOS_VARINFO *vb = adios_inq_var(infile[i], infile[i]->var_namelist[adios_varid]);
			if (vb) {
				l_wbsize += vb->nblocks[0];
				adios_free_varinfo(vb);
			}
		}
		MPI_Allreduce(&l_wbsize, &g_wbsize, 1, MPI_INT, MPI_MAX,comm);

		/* 
 		 * Now iterate over all the files and blocks. If a process has 
 		 * fewer blocks than g_wbsize, it will write out zero bytes.
 		 */
        PIO_Offset start[MAX_NC_DIMS], count[MAX_NC_DIMS];
		int k=0;
		size_t mysize = 0;
		char *buf = NULL;
		for (int i=1;i<=wblocks.size();i++) {
       		ADIOS_VARINFO *vb = adios_inq_var(infile[i], infile[i]->var_namelist[adios_varid]);
			if (vb) {
				adios_inq_var_blockinfo(infile[i], vb);
				for (int j=0;j<vb->nblocks[0];i++) {
					mysize = 0;
					for (int d=0;d<vb->ndim;d++) 
						mysize += (size_t)vb->blockinfo[j].count[d];
					mysize = mysize*adios_type_size(vb->type,NULL);
        			buf = (char *)malloc(mysize); 
					if (!buf) { 
						printf("ERROR: cannot allocate memory: %ld\n",mysize);
						return 1;
					}
					ADIOS_SELECTION *wbsel = adios_selection_writeblock(j);
       		 		int ret = adios_schedule_read(infile[i], wbsel, infile[i]->var_namelist[adios_varid], 0, 1, buf);
       		 		adios_perform_reads(infile[i], 1);

					for (int d=0;d<vb->ndim;d++) {
						start[d] = (PIO_Offset) vb->blockinfo[j].start[d];
            			count[d] = (PIO_Offset) vb->blockinfo[j].count[d];
					}
 					ret = put_vara(ncid, var.nc_varid, var.nctype, vb->type, start, count, buf);
        			if (ret != PIO_NOERR) {
            			cout << "rank " << mpirank << ":ERROR in PIOc_put_vara(), code = " << ret
                 			 << " at " << __func__ << ":" << __LINE__ << endl;
						return 1;
					}
        			adios_selection_delete(wbsel);
					free(buf);
					adios_free_varinfo(vb);
					k++;
				}
			}
		}
		mysize = 0;
		for (int d=0;d<vi->ndim;d++) {
       		start[d] = (PIO_Offset) 0;
    		count[d] = (PIO_Offset) 0;
        }
		buf = (char *)malloc(mysize+1);
		for (;k<g_wbsize;k++) {
			ret = put_vara(ncid, var.nc_varid, var.nctype, vi->type, start, count, buf);
        	if (ret != PIO_NOERR) {
           		cout << "rank " << mpirank << ":ERROR in PIOc_put_vara(), code = " << ret
           			 << " at " << __func__ << ":" << __LINE__ << endl;
				return 1;
			}
		}
        TimerStop(write);
		free(buf);
    }
    adios_free_varinfo(vi);
    return ret;
}

int ConvertVariableTimedPutVar(ADIOS_FILE **infile, std::vector<int> wblocks, int adios_varid, int ncid, Variable& var, int nblocks_per_step)
{
    TimerStart(read);
    int ret = 0;
    ADIOS_VARINFO *vi = adios_inq_var(infile[0], infile[0]->var_namelist[adios_varid]);

    if (vi->ndim == 0)
    {
        /* Scalar variable over time */
        /* Written by only one process, so steps = number of blocks in file */
        int nsteps = vi->nblocks[0];
        TimerStart(read);
        adios_inq_var_stat(infile[0], vi, 0, 1);
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

		/* compute the total number of blocks */
		int l_nblocks = 0;
		int g_nblocks = 0;
		for (int i=1;i<=wblocks.size();i++) {
    		ADIOS_VARINFO *vb = adios_inq_var(infile[i], infile[i]->var_namelist[adios_varid]);
			if (vb) {
				l_nblocks += vb->nblocks[0];
				adios_free_varinfo(vb);
			}
		}
		MPI_Allreduce(&l_nblocks,&g_nblocks,1,MPI_INT,MPI_SUM,comm); 
		
        if (var.is_timed)
        {
            nsteps = g_nblocks / nblocks_per_step;
        }
        if (g_nblocks != nsteps * nblocks_per_step)
        {
            cout << "rank " << mpirank << ":ERROR in processing variable '" << infile[0]->var_namelist[adios_varid]
                 << "'. Number of blocks = " << g_nblocks
                 << " does not equal the number of steps * number of writers = "
                 << nsteps << " * " << nblocks_per_step << " = " << nsteps*nblocks_per_step
                 << endl;
        }

        /* Is this a local array written by each process, or a truly distributed global array */
        TimerStart(read);
        adios_inq_var_blockinfo(infile[0], vi);
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
        if (var.nctype == PIO_CHAR && vi->ndim == 1)
        {
            /* Character array over time may have longer dimension declaration than the actual content */
            local_array = true;
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
                int ret = adios_schedule_read(infile[0], wbsel, infile[0]->var_namelist[adios_varid],
                        0, 1, d.data());
                adios_perform_reads(infile[0], 1);
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
                    << "Variable \"" << infile[0]->var_namelist[adios_varid] << "\" is a "
                    << vi->ndim << "D array including the unlimited dimension"
                    << endl;
        }
    }
    adios_free_varinfo(vi);
    return ret;
}

int ConvertVariableDarray(ADIOS_FILE **infile, int adios_varid, int ncid, Variable& var,
        std::vector<int>& wblocks, DecompositionMap& decomp_map, int nblocks_per_step)
{
    int ret = 0;

    string attname = string(infile[0]->var_namelist[adios_varid]) + "/__pio__/decomp";
    int asize;
    ADIOS_DATATYPES atype;
    char *decompname;
    adios_get_attr(infile[0], attname.c_str(), &atype, &asize, (void**)&decompname);
    Decomposition decomp = decomp_map[decompname];
    if (decomp.piotype != var.nctype)
    {
        /* 
 		 * Type conversion may happened at writing. Now we make a new decomposition
         * for this nctype
         */
        decomp = GetNewDecomposition(decomp_map, decompname, infile, ncid, wblocks, var.nctype);
    }
    free(decompname);

    ADIOS_VARINFO *vi = adios_inq_var(infile[0], infile[0]->var_namelist[adios_varid]);
    adios_inq_var_blockinfo(infile[0], vi);

    /* calculate how many records/steps we have for this variable */
    int nsteps = 1;
    int ts = 0; /* loop from ts to nsteps, below ts may become the last step */

	/* compute the total number of blocks */
	int l_nblocks = 0;
	int g_nblocks = 0;
	for (int i=1;i<=wblocks.size();i++) {
   		ADIOS_VARINFO *vb = adios_inq_var(infile[i], infile[i]->var_namelist[adios_varid]);
		if (vb) {
			l_nblocks += vb->nblocks[0];
			adios_free_varinfo(vb);
		}
	}
	MPI_Allreduce(&l_nblocks,&g_nblocks,1,MPI_INT,MPI_SUM,comm); 
		
    if (var.is_timed)
    {
        nsteps = g_nblocks / nblocks_per_step;
        if (g_nblocks != nsteps * nblocks_per_step)
        {
            cout << "rank " << mpirank << ":ERROR in processing darray '" << infile[0]->var_namelist[adios_varid]
                 << "'. Number of blocks = " << g_nblocks 
                 << " does not equal the number of steps * number of writers = "
                 << nsteps << " * " << nblocks_per_step << " = " << nsteps*nblocks_per_step
                 << endl;
        }
    }
    else
    {
        /* Silly apps may still write a non-timed variable every step, basically overwriting the variable.
         * But we have too many blocks in the adios file in such case and we need to deal with them
         */
        nsteps = g_nblocks / nblocks_per_step;
        int maxSteps = GlobalMaxSteps();
        if (g_nblocks != nsteps * nblocks_per_step)
        {
            cout << "rank " << mpirank << ":ERROR in processing darray '" << infile[0]->var_namelist[adios_varid]
                 << "' which has no unlimited dimension. Number of blocks = " << g_nblocks 
                 << " does not equal the number of steps * number of writers = "
                 << nsteps << " * " << nblocks_per_step << " = " << nsteps*nblocks_per_step
                 << endl;
        }
        else if (maxSteps != 1 && nsteps > maxSteps)
        {
            cout << "rank " << mpirank << ":ERROR in processing darray '" << infile[0]->var_namelist[adios_varid]
                 << "'. A variable without unlimited dimension was written multiple times."
                 << " The " << nsteps << " steps however does not equal to the number of steps "
                 << "of other variables that indeed have unlimited dimensions (" << maxSteps << ")."
                 << endl;
        }
        else if (nsteps > 1)
        {
            cout << "rank " << mpirank << ":WARNING in processing darray '" << infile[0]->var_namelist[adios_varid]
                 << "'. A variable without unlimited dimension was written " << nsteps << " times. "
                 << "We will write only the last occurence."
                 << endl;
        }
        ts = nsteps-1;
    }

	// TAHSIN -- THIS IS GETTING CONFUSING. NEED TO THINK ABOUT time steps. 
    for (; ts < nsteps; ++ts)
    {
        /* Sum the sizes of blocks assigned to this process */
		/* Compute the number of writers for each file from nsteps */
        uint64_t nelems = 0;
		int l_nwriters  = 0;
		for (int i=1;i<=wblocks.size();i++) {
        	ADIOS_VARINFO *vb = adios_inq_var(infile[i], infile[i]->var_namelist[adios_varid]);
        	if (vb) {
				adios_inq_var_blockinfo(infile[i], vb);
				l_nwriters = vb->nblocks[0]/nsteps;
				for (int j=0;j<l_nwriters;j++) {	
					if (j<vb->nblocks[0])
						nelems += vb->blockinfo[j*nsteps+ts].count[0];
				}
            	adios_free_varinfo(vb);
        	}
    	}

		/* Read local data for each file */
        int elemsize = adios_type_size(vi->type,NULL);
        std::vector<char> d(nelems * elemsize);
        uint64_t offset = 0;
		for (int i=1;i<=wblocks.size();i++) {
            ADIOS_VARINFO *vb = adios_inq_var(infile[i], infile[i]->var_namelist[adios_varid]);
            if (vb) {
                adios_inq_var_blockinfo(infile[i], vb);
                l_nwriters = vb->nblocks[0]/nsteps;
                for (int j=0;j<l_nwriters;j++) {
					int blockid = j*nsteps+ts;
                    if (blockid<vb->nblocks[0]) {
						cout << "    rank " << mpirank << ": read var = " << blockid <<
                        		" start byte = " << offset <<
                        		" elems = " << vb->blockinfo[blockid].count[0] << endl;
                		ADIOS_SELECTION *wbsel = adios_selection_writeblock(blockid);
                		int ret = adios_schedule_read(infile[i], wbsel, 
										infile[i]->var_namelist[adios_varid], 0, 1,
                        				d.data()+offset);
                		adios_perform_reads(infile[i], 1);
                		offset += vb->blockinfo[blockid].count[0] * elemsize;
					}
                }
                adios_free_varinfo(vb);
            }   
        }	
        TimerStop(read);

        TimerStart(write);
        if (wblocks[0] < nblocks_per_step)
        {
            if (var.is_timed)
                PIOc_setframe(ncid, var.nc_varid, ts);
            ret = PIOc_write_darray(ncid, var.nc_varid, decomp.ioid, (PIO_Offset)nelems,
                    d.data(), NULL);
        }
        TimerStop(write);
    }
    adios_free_varinfo(vi);

    return ret;
}

/*
 * Assumes a BP folder with name "infilename.dir" and 
 * all the files in the folder are bp files. It also 
 * assumes the file extensions are "infilename.bp.X" 
 * where X is 0 to N-1.
 */
int GetNumOfFiles(string infilename) 
{
	int file_count = 0;
	string foldername = infilename + ".dir/";
	DIR* dirp = opendir(foldername.c_str());
	if (!dirp) {
		fprintf(stderr, "Folder %s does not exist.\n",foldername.c_str());
		return -1;
	}
    	struct dirent * dp;
    	while ((dp = readdir(dirp)) != NULL) {
		if (dp->d_type==DT_REG) file_count++;
    	}
    	closedir(dirp);	
	return file_count;
}

void ConvertBPFile(string infilename, string outfilename, int pio_iotype)
{
	ADIOS_FILE **infile = NULL;
	int num_infiles = 0; 
	try {
		ncid = -1;

		/*
		 * This assumes we are running the program in the same folder where 
		 * the BP folder exists.
		 *
		 * TODO: split the basename and the path.
		 */

		/*
		 * Get the number of files in BP folder. 
		 * This operation assumes that the BP folder contains only the 
		 * BP files. 
		 */
		int n_bp_files = GetNumOfFiles(infilename);
		if (n_bp_files<0) 
			throw std::runtime_error("Input folder doesn't exist.\n");

		/* Number of BP file writers != number of converter processes here */
        std::vector<int> wblocks;
        wblocks = AssignWriteRanks(n_bp_files);
        printf("SIZE: %d\n",wblocks.size());
        for (auto nb: wblocks)
            printf("Myrank: %d File id: %d\n",mpirank,nb);

		num_infiles = wblocks.size()+1; //  infile[0] is opened by all processors
		infile = (ADIOS_FILE **)malloc(num_infiles*sizeof(ADIOS_FILE *));
		if (!infile) 
			throw std::runtime_error("Cannot allocate space for infile array.\n");
		
		string file0 = infilename + ".dir/" + infilename + ".0";
		infile[0] = adios_read_open_file(file0.c_str(), ADIOS_READ_METHOD_BP, comm);
		int ret = adios_schedule_read(infile[0], NULL, "/__pio__/info/nproc", 0, 1, &n_bp_writers);
		if (ret)
			throw std::runtime_error("Invalid BP file: missing '/__pio__/info/nproc' variable\n");
		adios_perform_reads(infile[0], 1);
		if (n_bp_writers!=n_bp_files) {
			std::cout  << "ERROR: #writers ("<< n_bp_writers 
					   << ") != #files (" << n_bp_files << std::endl;
			throw std::runtime_error("#writers has to be equal to #files.\n");
		} else {
			std::cout << "n_bp_writers: " << n_bp_writers 
					  << " n_bp_files: " << n_bp_files << std::endl;
		}

		/*
		 * Open the BP files. 
		 * infilename.bp.0 is opened by all the nodes. It contains all of the variables 
		 * and attributes. Each node then opens the files assigned to that node. 
		 */
		for (int i=1;i<=wblocks.size();i++) {
			string filei = infilename + ".dir/" + infilename + "." + to_string(wblocks[i-1]);
			infile[i] = adios_read_open_file(filei.c_str(), ADIOS_READ_METHOD_BP, MPI_COMM_SELF); 
			std::cout << "myrank " << mpirank << " file: " << filei << std::endl;
		}

		/* First process decompositions */
		DecompositionMap decomp_map = ProcessDecompositions(infile, ncid, wblocks);

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

		/* 
		 * For each variable,read in the data 
		 * with ADIOS then write it out with PIO 
		 */
		for (int i = 0; i < infile[0]->nvars; i++) {
			string v = infile[0]->var_namelist[i];
			if (v.find("/__") == string::npos)
			{
				/* For each variable, read with ADIOS then write with PIO */
				if (!mpirank) cout << "Convert variable " << v << endl;
				Variable& var = vars_map[v];

				TimerStart(read);
				string attname = string(infile[0]->var_namelist[i]) + "/__pio__/ncop";
				int asize;
				char *ncop;
				ADIOS_DATATYPES atype;
				adios_get_attr(infile[0], attname.c_str(), &atype, &asize, (void**)&ncop);
				TimerStop(read);

				std::string op(ncop);
				if (op == "put_var") {
					if (var.is_timed) {
						ConvertVariableTimedPutVar(infile, wblocks, i, ncid, var, n_bp_writers);
					} else {
						ConvertVariablePutVar(infile, wblocks, i, ncid, var);
					}
				} else if (op == "darray") {
					/* Variable was written with pio_write_darray() with a decomposition */
					ConvertVariableDarray(infile, i, ncid, var, wblocks, decomp_map, n_bp_writers);
				} else {
					if (!mpirank)
						cout << "  WARNING: unknown operation " << op << ". Will not process this variable\n";
				}
				free(ncop);
			}
			FlushStdout(comm);
			PIOc_sync(ncid); /* FIXME: flush after each variable until development is done. Remove for efficiency */
		}
		TimerStart(write);
		ret = PIOc_sync(ncid);
		ret = PIOc_closefile(ncid);
		TimerStop(write);
		TimerStart(read);
		for (int i=0;i<num_infiles;i++) 
			adios_read_close(infile[i]);
		TimerStop(read);

		return;
   } catch (std::exception &e) {
        if (ncid > -1)
            PIOc_closefile(ncid);
		if (infile) {
			for (int i=0;i<num_infiles;i++) 
				adios_read_close(infile[i]);
		}
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
    
    string infilename  = argv[1];
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
