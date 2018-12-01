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
#include "pio_internal.h"
#include "adios_read.h"
#include "adios2pio-nm-lib.h"
#include "adios2pio-nm-lib-c.h"

using namespace std;

/* debug output */
static int debug_out = 0;

/* static MPI_Comm comm = MPI_COMM_WORLD; */
/*
static int mpirank;
static int nproc;
*/

/** The ID for the parallel I/O system. It is set by
 * PIOc_Init_Intracomm(). It references an internal structure
 * containing the general IO subsystem data and MPI
 * structure. It is passed to PIOc_finalize() to free
 * associated resources, after all I/O, but before
 * MPI_Finalize is called. */
/* static int iosysid; */

/** The ncid of the netCDF file created in this example. */
/* static int ncid; */

/* Number of processes that wrote the BP file (read from the file) */
/* static int n_bp_writers; */

/* Timer functions to separate time for read (ADIOS) and write (PIO) */
#ifdef ADIOS_TIMING
static double time_read, time_write;
static double time_temp_read, time_temp_write;
void TimerInitialize_nm() { time_read = 0.0; time_write = 0.0; }
#define TimerStart(x) { time_temp_##x = MPI_Wtime(); }
#define TimerStop(x) { time_##x += (MPI_Wtime() - time_temp_##x); }
void TimerReport_nm(MPI_Comm comm)
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

    if (!rank && debug_out)
    {
        cout << "Timing information:     Max     Sum of all\n";
        cout.precision(2);
        cout << "ADIOS read time   = " << std::fixed << std::setw(8) << tr_max << "s " << std::setw(8) << tr_sum << "s\n";
        cout << "PIO  write time   = " << std::fixed << std::setw(8) << tw_max << "s " << std::setw(8) << tw_sum << "s\n";
    }
}
void TimerFinalize_nm() {}
#else
void TimerInitialize_nm() {}
#define TimerStart(x) {}
#define TimerStop(x) {}
void TimerReport_nm(MPI_Comm comm) {}
void TimerFinalize_nm() {}
#endif 

void SetDebugOutput(int val) { debug_out = val; }

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

int InitPIO(MPI_Comm comm, int mpirank, int nproc)
{
	int iosysid; 

    if (PIOc_Init_Intracomm(comm, nproc, 1, 0, PIO_REARR_SUBSET, &iosysid))
        throw std::runtime_error("PIO initialization failed\n");

	return iosysid;
}

void FlushStdout_nm(MPI_Comm comm)
{
    cout << std::flush;
    usleep((useconds_t)100);
    MPI_Barrier(comm);
}

/* Set the currently encountered max number of steps if argument is given.
 * Return the max step currently
 */
int GlobalMaxSteps_nm(int nsteps_in=0)
{
    static int nsteps_current = 1;
    if (nsteps_in > nsteps_current)
        nsteps_current = nsteps_in;
    return nsteps_current;
}

std::vector<int> AssignWriteRanks(int n_bp_writers, MPI_Comm comm, int mpirank, int nproc)
{
    if (!mpirank && debug_out) cout << "The BP file was written by " << n_bp_writers << " processes\n";
    int nwb = n_bp_writers / nproc; // number of blocks to process
    int start_wb; // starting wb (in [0..nwb-1])
    if (mpirank < n_bp_writers % nproc)
        nwb++;

    if (mpirank < n_bp_writers % nproc)
        start_wb = mpirank * nwb;
    else
        start_wb = mpirank * nwb + n_bp_writers % nproc;
	if (debug_out)
    	cout << "Process " << mpirank << " start block = " << start_wb <<
            	" number of blocks = " << nwb << std::endl;
    FlushStdout_nm(comm);
    std::vector<int> blocks(nwb);
    for (int i=0; i<nwb; ++i)
        blocks[i]=start_wb+i;
    return blocks;
}

void ProcessGlobalFillmode(ADIOS_FILE **infile, int ncid)
{
    int  asize;
    void *fillmode;
    ADIOS_DATATYPES atype;
    adios_get_attr(infile[0], "/__pio__/fillmode", &atype, &asize, &fillmode);
    PIOc_set_fill(ncid, *(int*)fillmode, NULL);
    free(fillmode);
}

void ProcessVarAttributes(ADIOS_FILE **infile, int adios_varid, std::string varname, int ncid, int nc_varid)
{
    ADIOS_VARINFO *vi = adios_inq_var(infile[0], infile[0]->var_namelist[adios_varid]);
    for (int i=0; i < vi->nattrs; i++)
    {
		if (debug_out)
        	cout << "    Attribute: " << infile[0]->attr_namelist[vi->attr_ids[i]] << std::endl;
        int asize;
        char *adata;
        ADIOS_DATATYPES atype;
        adios_get_attr(infile[0], infile[0]->attr_namelist[vi->attr_ids[i]], &atype, &asize, (void**)&adata);
        nc_type piotype = PIOc_get_nctype_from_adios_type(atype);
        char *attname = infile[0]->attr_namelist[vi->attr_ids[i]]+varname.length()+1;
		if (debug_out)
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


void ProcessGlobalAttributes(ADIOS_FILE **infile, int ncid, DimensionMap& dimension_map, VariableMap& vars_map)
{
	if (debug_out) cout << "Process Global Attributes: \n";

	std::string delimiter = "/";
	std::map<std::string,char> processed_attrs;
	std::map<std::string,int>  var_att_map;

    for (int i=0; i < infile[0]->nattrs; i++)
    {
        string a = infile[0]->attr_namelist[i];
        if (a.find("pio_global/") != string::npos)
        {
            if (debug_out) cout << "    Attribute: " << infile[0]->attr_namelist[i] << std::endl;
            int asize;
            char *adata = NULL;
            ADIOS_DATATYPES atype;
            adios_get_attr(infile[0], infile[0]->attr_namelist[i], &atype, &asize, (void**)&adata);
            nc_type piotype = PIOc_get_nctype_from_adios_type(atype);
            char *attname = infile[0]->attr_namelist[i]+strlen("pio_global/");
            if (debug_out) cout << "        define PIO attribute: " << attname << ""
                    			<< "  type=" << piotype << std::endl;
            int len = 1;
            if (atype == adios_string)
                len = strlen(adata);
            PIOc_put_att(ncid, PIO_GLOBAL, attname, piotype, len, adata);
            if (adata) free(adata);
        } else {
			std::string token = a.substr(0, a.find(delimiter));
			if (token!="" && vars_map.find(token)==vars_map.end()) 
			{ 
				if (var_att_map.find(token)==var_att_map.end()) 
				{
					// first encounter 
           			string attname = token + "/__pio__/nctype";
					processed_attrs[attname] = 1;
           			int asize;
           			int *nctype = NULL;
           			ADIOS_DATATYPES atype;
           			adios_get_attr(infile[0], attname.c_str(), &atype, &asize, (void**)&nctype);

            		attname = token + "/__pio__/ndims";
					processed_attrs[attname] = 1;
            		int *ndims = NULL;
            		adios_get_attr(infile[0], attname.c_str(), &atype, &asize, (void**)&ndims);

            		char **dimnames = NULL;
            		int dimids[MAX_NC_DIMS];
            		if (*ndims)
            		{
              	 		attname = token + "/__pio__/dims";
						processed_attrs[attname] = 1;
						adios_get_attr(infile[0], attname.c_str(), &atype, &asize, (void**)&dimnames);

               			for (int d=0; d < *ndims; d++)
                   			dimids[d] = dimension_map[dimnames[d]].dimid;
               		}
            		int varid;
            		PIOc_def_var(ncid, token.c_str(), *nctype, *ndims, dimids, &varid);
	           		var_att_map[token] = varid; 

            		if (nctype) free(nctype);
            		if (ndims) free(ndims);
            		if (dimnames) free(dimnames);
				} else {
					if (processed_attrs.find(a)==processed_attrs.end()) {
						processed_attrs[a] = 1;
						int asize;
           				char *adata = NULL;
           				ADIOS_DATATYPES atype;
           				adios_get_attr(infile[0], a.c_str(), &atype, &asize, (void**)&adata);
						nc_type piotype = PIOc_get_nctype_from_adios_type(atype);
        				char *attname = ((char*)a.c_str())+token.length()+1;;
        				int len = 1;
        				if (atype == adios_string) len = strlen(adata);
        				PIOc_put_att(ncid, var_att_map[token], attname, piotype, len, adata);
        				if (adata) free(adata);
					}
				}
			}
		}
    }
}

Decomposition ProcessOneDecomposition(ADIOS_FILE **infile, int ncid, const char *varname, std::vector<int>& wfiles, int iosysid, int mpirank,
		int nproc,
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
    for (int i=1;i<=wfiles.size();i++) { // iterate over all the files assigned to this process
   		ADIOS_VARINFO *vb = adios_inq_var(infile[i], varname);
		if (vb) {
			adios_inq_var_blockinfo(infile[i], vb);

			// Assuming all time steps have the same number of writer blocks	
			for (int j=0;j<vb->nblocks[0];j++) 
				nelems += vb->blockinfo[j].count[0];
			adios_free_varinfo(vb);
		}
	}

	/* allocate +1 to prevent d.data() from returning NULL. Otherwise, read/write operations fail */
	/* nelems may be 0, when some processes do not have any data */
   	std::vector<PIO_Offset> d(nelems+1);  
   	uint64_t offset = 0;
   	for (int i=1;i<=wfiles.size();i++) {
		ADIOS_VARINFO *vb = adios_inq_var(infile[i], varname);
		if (vb) {
   			adios_inq_var_blockinfo(infile[i], vb);
   			// Assuming all time steps have the same number of writer blocks	
   			for (int j=0;j<vb->nblocks[0];j++) {
   		 		if (debug_out) cout << " rank " << mpirank << ": read decomp wb = " << j <<
   		       		  				" start = " << offset <<
   		       		  				" elems = " << vb->blockinfo[j].count[0] << endl;
				ADIOS_SELECTION *wbsel = adios_selection_writeblock(j);
   		 		int ret = adios_schedule_read(infile[i], wbsel, varname, 0, 1, d.data()+offset);
   		 		adios_perform_reads(infile[i], 1);
				offset += vb->blockinfo[j].count[0];
				adios_selection_delete(wbsel);
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

	int decomp_piotype = *piotype;

   	free(piotype);
   	free(decomp_ndims);
   	free(decomp_dims);

    return Decomposition{ioid, decomp_piotype};
}

DecompositionMap ProcessDecompositions(ADIOS_FILE **infile, int ncid, std::vector<int>& wfiles, int iosysid, MPI_Comm comm, int mpirank, int nproc)
{
    DecompositionMap decomp_map;
    for (int i = 0; i < infile[0]->nvars; i++)
    {
        string v = infile[0]->var_namelist[i];
        if (v.find("/__pio__/decomp/") != string::npos)
        {
            string decompname = v.substr(16);
            if (!mpirank && debug_out) cout << "Process decomposition " << decompname << endl;
            Decomposition d = ProcessOneDecomposition(infile, ncid, infile[0]->var_namelist[i], wfiles, iosysid, mpirank, nproc);
            decomp_map[decompname] = d;
        }
        FlushStdout_nm(comm);
    }
    return decomp_map;
}

Decomposition GetNewDecomposition(DecompositionMap& decompmap, string decompname,
        ADIOS_FILE **infile, int ncid, std::vector<int>& wfiles, int nctype, int iosysid, int mpirank, int nproc)
{
	char ss[256];
	sprintf(ss,"%s_%d",decompname.c_str(),nctype);
	string key(ss);

    auto it = decompmap.find(key);
    Decomposition d;
    if (it == decompmap.end())
    {
        string varname = "/__pio__/decomp/" + decompname;
        d = ProcessOneDecomposition(infile, ncid, varname.c_str(), wfiles, iosysid, mpirank, nproc, nctype);
        decompmap[key] = d;
    }
    else
    {
        d = it->second;
    }
    return d;
}

DimensionMap ProcessDimensions(ADIOS_FILE **infile, int ncid, MPI_Comm comm, int mpirank, int nproc)
{
    DimensionMap dimensions_map;
    for (int i = 0; i < infile[0]->nvars; i++)
    {
        string v = infile[0]->var_namelist[i];
        if (v.find("/__pio__/dim/") != string::npos)
        {
            /* For each dimension stored, define a dimension variable with PIO */
            string dimname = v.substr(13);
            if (!mpirank && debug_out) cout << "Process dimension " << dimname << endl;

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
        FlushStdout_nm(comm);
    }
    return dimensions_map;
}

VariableMap ProcessVariableDefinitions(ADIOS_FILE **infile, int ncid, DimensionMap& dimension_map, MPI_Comm comm, int mpirank, int nproc)
{
    VariableMap vars_map;
    for (int i = 0; i < infile[0]->nvars; i++)
    {
        string v = infile[0]->var_namelist[i];
        if (!mpirank && debug_out) cout << "BEFORE Process variable " << v << endl;
        if (v.find("/__") == string::npos)
        {
            /* For each variable written define it with PIO */
            if (!mpirank && debug_out) cout << "Process variable " << v << endl;

			if (v.find("decomp_id/")==string::npos && 
				v.find("frame_id/")==string::npos && 
				v.find("fillval_id/")==string::npos) {

            	TimerStart(read);
            	string attname = string(infile[0]->var_namelist[i]) + "/__pio__/nctype";
            	int asize;
            	int *nctype = NULL;
            	ADIOS_DATATYPES atype;
            	adios_get_attr(infile[0], attname.c_str(), &atype, &asize, (void**)&nctype);

	            attname = string(infile[0]->var_namelist[i]) + "/__pio__/ndims";
	            int *ndims = NULL;
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

	            if (nctype) free(nctype);
	            if (ndims) free(ndims);
	            if (dimnames) free(dimnames);
			}
        }
        FlushStdout_nm(comm);
    }
    return vars_map;
}

int put_var_nm(int ncid, int varid, int nctype, enum ADIOS_DATATYPES memtype, const void* buf)
{
    int ret = 0;
    switch(memtype)
    {
    case adios_byte:
		if (nctype==PIO_CHAR)
			ret = PIOc_put_var_text(ncid, varid, (const char*)buf);
		else  
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

int put_vara_nm(int ncid, int varid, int nctype, enum ADIOS_DATATYPES memtype, PIO_Offset *start, PIO_Offset *count, const void* buf)
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

int ConvertVariablePutVar(ADIOS_FILE **infile, std::vector<int> wfiles, int adios_varid, int ncid, Variable& var, int mpirank, int nproc)
{
    TimerStart(read);
    int ret = 0;
	char *varname     = infile[0]->var_namelist[adios_varid];
    ADIOS_VARINFO *vi = adios_inq_var(infile[0], varname);

    if (vi->ndim == 0)
    {
        /* Scalar variable */
        TimerStart(write);
        int ret = put_var_nm(ncid, var.nc_varid, var.nctype, vi->type, vi->value);
        if (ret != PIO_NOERR)
                cout << "ERROR in PIOc_put_var(), code = " << ret
                     << " at " << __func__ << ":" << __LINE__ << endl;
        TimerStop(write);
    }
    else
    {
        /* An N-dimensional array that needs no rearrangement.
         * put_vara_nm() needs all processes participate */
        TimerStart(read);
		
		/* ACME writes this array from I/O processor 0 */
        PIO_Offset start[MAX_NC_DIMS], count[MAX_NC_DIMS];
		// PIOc_put_var may have been called multiple times with different start,count values 
		// for a variable. We need to convert the output from each of those calls.
   		ADIOS_VARINFO *vb = adios_inq_var(infile[0], varname);
		if (vb) {
			adios_inq_var_blockinfo(infile[0], vb);
			if (mpirank==0) {
				size_t mysize = 1;
				char   *buf   = NULL;
				for (int ii=0;ii<vb->sum_nblocks;ii++) {
					mysize = 1;
					for (int d=0;d<vb->ndim;d++) 
						mysize *= (size_t)vb->blockinfo[ii].count[d];
					mysize = mysize*adios_type_size(vb->type,NULL);
   	   				if ((buf = (char *)malloc(mysize))==NULL) {
						printf("ERROR: cannot allocate memory: %ld\n",mysize);
						return 1;
					}
					ADIOS_SELECTION *wbsel = adios_selection_writeblock(ii);
   	   				int ret = adios_schedule_read(infile[0], wbsel, varname, 0, 1, buf);
   	   				adios_perform_reads(infile[0], 1);
					for (int d=0;d<vb->ndim;d++) {
						start[d] = (PIO_Offset) vb->blockinfo[ii].start[d];
       					count[d] = (PIO_Offset) vb->blockinfo[ii].count[d];
					}
 					ret = put_vara_nm(ncid, var.nc_varid, var.nctype, vb->type, start, count, buf);
       				if (ret != PIO_NOERR) {
       					cout << "rank " << mpirank << ":ERROR in PIOc_put_vara(), code = " << ret
       						 << " at " << __func__ << ":" << __LINE__ << endl;
						return 1;
					}
					adios_selection_delete(wbsel);
					if (buf) free(buf);
				}
			} else {
				char   temp_buf;
				for (int ii=0;ii<vb->sum_nblocks;ii++) {
					for (int d=0;d<vb->ndim;d++) {
						start[d] = (PIO_Offset) 0;
       					count[d] = (PIO_Offset) 0;
					}
 					ret = put_vara_nm(ncid, var.nc_varid, var.nctype, vb->type, start, count, &temp_buf);
       				if (ret != PIO_NOERR) {
       					cout << "rank " << mpirank << ":ERROR in PIOc_put_vara(), code = " << ret
       						 << " at " << __func__ << ":" << __LINE__ << endl;
						return 1;
					}
				}
			}
   			adios_free_varinfo(vb);
		}

#if 0
		if (mpirank==0) {
			size_t mysize = 1;
			char   *buf   = NULL;
			adios_inq_var_blockinfo(infile[0], vi);
			for (int d=0;d<vi->ndim;d++) 
				mysize *= (size_t)vi->blockinfo[0].count[d];
			mysize = mysize*adios_type_size(vi->type,NULL);
   	    	if ((buf = (char *)malloc(mysize))==NULL) {
				printf("ERROR: cannot allocate memory: %ld\n",mysize);
				return 1;
			}
			ADIOS_SELECTION *wbsel = adios_selection_writeblock(0);
   	    	int ret = adios_schedule_read(infile[0], wbsel, varname, 0, 1, buf);
   	    	adios_perform_reads(infile[0], 1);
	
			for (int d=0;d<vi->ndim;d++) {
				start[d] = (PIO_Offset) vi->blockinfo[0].start[d];
   	    		count[d] = (PIO_Offset) vi->blockinfo[0].count[d];
			}
 			ret = put_vara_nm(ncid, var.nc_varid, var.nctype, vi->type, start, count, buf);
   	    	if (ret != PIO_NOERR) {
   	    		cout << "rank " << mpirank << ":ERROR in PIOc_put_vara(), code = " << ret
   	    			 << " at " << __func__ << ":" << __LINE__ << endl;
				return 1;
			}
			adios_selection_delete(wbsel);
			if (buf) free(buf);
		} else {
			char temp_buf;
			for (int d=0;d<vi->ndim;d++) {
				start[d] = (PIO_Offset) 0;
   	    		count[d] = (PIO_Offset) 0;
			}
			ret = put_vara_nm(ncid, var.nc_varid, var.nctype, vi->type, start, count, &temp_buf);
   	    	if (ret != PIO_NOERR) {
   	    		cout << "rank " << mpirank << ":ERROR in PIOc_put_vara(), code = " << ret
   	    			 << " at " << __func__ << ":" << __LINE__ << endl;
				return 1;
			}
		}
#endif 
        TimerStop(write);
    }

    adios_free_varinfo(vi);
    return ret;
}

int ConvertVariableTimedPutVar(ADIOS_FILE **infile, std::vector<int> wfiles, int adios_varid, int ncid, Variable& var, int nblocks_per_step,
							  MPI_Comm comm, int mpirank, int nproc)
{
    TimerStart(read);
    int ret = 0;
	char *varname = infile[0]->var_namelist[adios_varid];
    ADIOS_VARINFO *vi = adios_inq_var(infile[0], varname);

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
		for (int i=1;i<=wfiles.size();i++) {
    		ADIOS_VARINFO *vb = adios_inq_var(infile[i], varname);
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
            if (debug_out) cout << "rank " << mpirank << ":ERROR in processing variable '" << varname 
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
                int ret = adios_schedule_read(infile[0], wbsel, varname, 0, 1, d.data());
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
				adios_selection_delete(wbsel);
            }
        }
        else
        {
			/* PIOc_put_vara_ writes out from processor 0       */
			/* Read in infile[0] and output using PIOc_put_vara */
			if (mpirank==0) {
				for (int ts=0; ts < nsteps; ++ts)
           		{
               		TimerStart(read);
               		int elemsize = adios_type_size(vi->type,NULL);
               		uint64_t nelems = 1;
               		for (int d = 0; d < vi->ndim; d++) {
               	    	nelems *= vi->dims[d];
               		}
               		std::vector<char> d(nelems * elemsize);
               		ADIOS_SELECTION *wbsel = adios_selection_writeblock(ts);
               		int ret = adios_schedule_read(infile[0], wbsel, varname, 0, 1, d.data());
               		adios_perform_reads(infile[0], 1);
               		TimerStop(read);
	
   	            	TimerStart(write);
   	            	PIO_Offset start[vi->ndim+1], count[vi->ndim+1];
   	            	start[0] = ts;
   	            	count[0] = 1;
   	            	for (int d = 0; d < vi->ndim; d++) {
						start[d+1] = 0;
   	                	count[d+1] = vi->dims[d];
   	            	}
	
   	            	if ((ret = PIOc_put_vara(ncid, var.nc_varid, start, count, d.data())))
   	            	if (ret != PIO_NOERR)
   	                	cout << "ERROR in PIOc_put_var(), code = " << ret
   	                	<< " at " << __func__ << ":" << __LINE__ << endl;
   	            	TimerStop(write);
					adios_selection_delete(wbsel);
   	        	}
			} else {
				for (int ts=0; ts < nsteps; ++ts)
           		{
               		TimerStart(read);
               		int elemsize = adios_type_size(vi->type,NULL);
               		uint64_t nelems = 1;
               		std::vector<char> d(nelems * elemsize);

   	            	PIO_Offset start[vi->ndim+1], count[vi->ndim+1];
   	            	start[0] = 0;
   	            	count[0] = 0;
   	            	for (int d = 0; d < vi->ndim; d++) {
						start[d+1] = 0;
   	                	count[d+1] = 0; 
   	            	}
	
   	            	if ((ret = PIOc_put_vara(ncid, var.nc_varid, start, count, d.data())))
   	            	if (ret != PIO_NOERR)
   	                	cout << "ERROR in PIOc_put_var(), code = " << ret
   	                	<< " at " << __func__ << ":" << __LINE__ << endl;
   	        	}
			}
        }
    }
    adios_free_varinfo(vi);
    return ret;
}

int ConvertVariableDarray(ADIOS_FILE **infile, int adios_varid, int ncid, Variable& var,
        std::vector<int>& wfiles, DecompositionMap& decomp_map, int nblocks_per_step, int iosysid,
		MPI_Comm comm, int mpirank, int nproc)
{
    int ret = 0;

	char *varname = infile[0]->var_namelist[adios_varid];
    ADIOS_VARINFO *vi = adios_inq_var(infile[0], varname);
    adios_inq_var_blockinfo(infile[0], vi);

    /* calculate how many records/steps we have for this variable */
    int nsteps = 1;
    int ts = 0; /* loop from ts to nsteps, below ts may become the last step */

	/* compute the total number of blocks */
	int l_nblocks = 0;
	int g_nblocks = 0;
	for (int i=1;i<=wfiles.size();i++) {
   		ADIOS_VARINFO *vb = adios_inq_var(infile[i], varname);
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
            if (debug_out) cout << "rank " << mpirank << ":ERROR in processing darray '" << varname 
                 << "'. Number of blocks = " << g_nblocks 
                 << " does not equal the number of steps * number of writers = "
                 << nsteps << " * " << nblocks_per_step << " = " << nsteps*nblocks_per_step
                 << endl;
        }
    }
    else
    {
        /* Apps may still write a non-timed variable every step, basically overwriting the variable.
         * But we have too many blocks in the adios file in such case and we need to deal with them
         */
        nsteps = g_nblocks / nblocks_per_step;
        int maxSteps = GlobalMaxSteps_nm();
        if (g_nblocks != nsteps * nblocks_per_step)
        {
            if (debug_out) cout << "rank " << mpirank << ":ERROR in processing darray '" << varname 
                 << "' which has no unlimited dimension. Number of blocks = " << g_nblocks 
                 << " does not equal the number of steps * number of writers = "
                 << nsteps << " * " << nblocks_per_step << " = " << nsteps*nblocks_per_step
                 << endl;
        }
        else if (maxSteps != 1 && nsteps > maxSteps)
        {
            if (debug_out) cout << "rank " << mpirank << ":ERROR in processing darray '" << varname 
                 << "'. A variable without unlimited dimension was written multiple times."
                 << " The " << nsteps << " steps however does not equal to the number of steps "
                 << "of other variables that indeed have unlimited dimensions (" << maxSteps << ")."
                 << endl;
        }
        else if (nsteps > 1)
        {
            if (debug_out) cout << "rank " << mpirank << ":WARNING in processing darray '" << varname 
                 << "'. A variable without unlimited dimension was written " << nsteps << " times. "
                 << "We will write only the last occurence."
                 << endl;
        }
        ts = nsteps-1;
    }

	/* different decompositions at different frames */
	char decomp_varname[128];
	char frame_varname[128];
	char fillval_varname[128];
	char decompname[64];
	sprintf(decomp_varname,"decomp_id/%s",varname);
	sprintf(frame_varname,"frame_id/%s",varname);
	sprintf(fillval_varname,"fillval_id/%s",varname);
	int  decomp_id, frame_id, fillval_exist; 
	char fillval_id[16];

	// TAHSIN -- THIS IS GETTING CONFUSING. NEED TO THINK ABOUT time steps. 
    for (; ts < nsteps; ++ts)
    {
        /* Sum the sizes of blocks assigned to this process */
		/* Compute the number of writers for each file from nsteps */
        uint64_t nelems = 0;
		int l_nwriters  = 0;
		for (int i=1;i<=wfiles.size();i++) {
        	ADIOS_VARINFO *vb = adios_inq_var(infile[i], varname);
        	if (vb) {
				adios_inq_var_blockinfo(infile[i], vb);
				l_nwriters = vb->nblocks[0]/nsteps;
				for (int j=0;j<l_nwriters;j++) {	
					int blockid = j*nsteps+ts;
					if (blockid<vb->nblocks[0])
						nelems += vb->blockinfo[blockid].count[0];
				}
            	adios_free_varinfo(vb);
        	}
    	}

		/* Read local data for each file */
        int elemsize = adios_type_size(vi->type,NULL);
		/* allocate +1 to prevent d.data() from returning NULL. Otherwise, read/write operations fail */
		/* nelems may be 0, when some processes do not have any data */
        std::vector<char> d((nelems+1) * elemsize);
        uint64_t offset = 0;
		for (int i=1;i<=wfiles.size();i++) {
            ADIOS_VARINFO *vb = adios_inq_var(infile[i], varname);
            if (vb) {
                adios_inq_var_blockinfo(infile[i], vb);
                l_nwriters = vb->nblocks[0]/nsteps;
                for (int j=0;j<l_nwriters;j++) {
					int blockid = j*nsteps+ts;
                    if (blockid<vb->nblocks[0]) {
                		ADIOS_SELECTION *wbsel = adios_selection_writeblock(blockid);
                		int ret = adios_schedule_read(infile[i], wbsel, 
										varname, 0, 1,
                        				d.data()+offset);

						/* different decompositions at different frames */
                		ret = adios_schedule_read(infile[i], wbsel, 
										decomp_varname, 0, 1, &decomp_id);
                		ret = adios_schedule_read(infile[i], wbsel, 
										frame_varname, 0, 1, &frame_id);
                		adios_perform_reads(infile[i], 1);
						
						if (decomp_id>0) {
                			ret = adios_schedule_read(infile[i], wbsel, 
											fillval_varname, 0, 1, fillval_id);
                			adios_perform_reads(infile[i], 1);
							fillval_exist = 1;
						} else {
							decomp_id = -decomp_id;
							fillval_exist = 0;
						}
                		offset += vb->blockinfo[blockid].count[0] * elemsize;
        				adios_selection_delete(wbsel);
					}
                }
                adios_free_varinfo(vb);
            }   
        }	
        TimerStop(read);

        TimerStart(write);
		sprintf(decompname,"%d",decomp_id);
    	Decomposition decomp = decomp_map[decompname];
    	if (decomp.piotype != var.nctype) {
       		/* Type conversion may happened at writing. Now we make a new decomposition for this nctype */
        	decomp = GetNewDecomposition(decomp_map, decompname, infile, ncid, wfiles, var.nctype, iosysid, mpirank, nproc);
		}
		if (frame_id<0) frame_id = 0;
        if (wfiles[0] < nblocks_per_step)
        {
			/* different decompositions at different frames */	
            if (var.is_timed)
                PIOc_setframe(ncid, var.nc_varid, frame_id);
			if (fillval_exist) {
            	ret = PIOc_write_darray(ncid, var.nc_varid, decomp.ioid, (PIO_Offset)nelems,
						d.data(), fillval_id); 
			} else {
            	ret = PIOc_write_darray(ncid, var.nc_varid, decomp.ioid, (PIO_Offset)nelems,
						d.data(), NULL); 
			}
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

std::string ExtractFilename(std::string pathname)
{
	size_t pos = pathname.find_last_of("/\\");
	if (pos==std::string::npos) {
		return pathname;
	} else {
		return pathname.substr(pos+1);
	}
}

std::string ExtractPathname(std::string pathname)
{
	size_t pos = pathname.find_last_of("/\\");
	if (pos==std::string::npos) {
		return "./";
	} else {
		return pathname.substr(0,pos);
	}
}

void ConvertBPFile(string infilepath, string outfilename, int pio_iotype, int iosysid, MPI_Comm comm, int mpirank, int nproc)
{
	ADIOS_FILE **infile = NULL;
	int num_infiles = 0; 
	int ncid = -1;
	int n_bp_writers;

	try {

		int save_imax = pio_get_imax();

		/*
		 * This assumes we are running the program in the same folder where 
		 * the BP folder exists.
		 *
		 */
		std::string foldername   = ExtractPathname(infilepath);
		std::string basefilename = ExtractFilename(infilepath);

		/*
		 * Get the number of files in BP folder. 
		 * This operation assumes that the BP folder contains only the 
		 * BP files. 
		 */
		int n_bp_files = GetNumOfFiles(infilepath);
		if (n_bp_files<0) 
			throw std::runtime_error("Input folder doesn't exist.\n");

		if (nproc>n_bp_files) {
			if (debug_out) std::cout << "ERROR: nproc (" << nproc << ") is greater than #files (" << n_bp_files << ")" << std::endl;
			throw std::runtime_error("Use fewer processors.\n");
		}

		/* Number of BP file writers != number of converter processes here */
        std::vector<int> wfiles;
        wfiles = AssignWriteRanks(n_bp_files,comm, mpirank, nproc);
		if (debug_out) {
        	for (auto nb: wfiles)
            	printf("Myrank: %d File id: %d\n",mpirank,nb);
		}

		num_infiles = wfiles.size()+1; //  infile[0] is opened by all processors
		infile = (ADIOS_FILE **)malloc(num_infiles*sizeof(ADIOS_FILE *));
		if (!infile) 
			throw std::runtime_error("Cannot allocate space for infile array.\n");
		
		string file0 = infilepath + ".dir/" + basefilename + ".0";
		if (debug_out) printf("FILE0: %s\n",file0.c_str()); fflush(stdout);
		infile[0] = adios_read_open_file(file0.c_str(), ADIOS_READ_METHOD_BP, comm);
		if (infile[0]==NULL) { 
			printf("ERROR: file open returned an error.\n");
			fflush(stdout);
		}
		int ret = adios_schedule_read(infile[0], NULL, "/__pio__/info/nproc", 0, 1, &n_bp_writers);
		if (ret)
			throw std::runtime_error("Invalid BP file: missing '/__pio__/info/nproc' variable\n");
		adios_perform_reads(infile[0], 1);
		if (n_bp_writers!=n_bp_files) {
			if (debug_out) std::cout  << "WARNING: #writers (" 
					<< n_bp_writers << ") != #files (" 
					<< n_bp_files << ")" << std::endl;
		} else {
			if (debug_out) std::cout << "n_bp_writers: " << n_bp_writers 
					  << " n_bp_files: " << n_bp_files << std::endl;
		}

		/*
		 * Open the BP files. 
		 * basefilename.bp.0 is opened by all the nodes. It contains all of the variables 
		 * and attributes. Each node then opens the files assigned to that node. 
		 */
		for (int i=1;i<=wfiles.size();i++) {
			// ostringstream ss; ss << wfiles[i-1];
			// string fileid_str = ss.str(); 
			char ss[64];
			sprintf(ss,"%d",wfiles[i-1]);
			string fileid_str(ss);

			string filei = infilepath + ".dir/" + basefilename + "." + fileid_str;
			infile[i] = adios_read_open_file(filei.c_str(), ADIOS_READ_METHOD_BP, MPI_COMM_SELF); 
			if (debug_out) std::cout << "myrank " << mpirank << " file: " << filei << std::endl;
		}

		/* First process decompositions */
		DecompositionMap decomp_map = ProcessDecompositions(infile, ncid, wfiles,iosysid,comm, mpirank, nproc);

		/* Create output file */
		TimerStart(write);
		/* 
			Use NC_64BIT_DATA instead of PIO_64BIT_OFFSET. Some output files will have variables 
			that require more than 4GB storage. 
		*/
		ret = PIOc_createfile(iosysid, &ncid, &pio_iotype, outfilename.c_str(), NC_64BIT_DATA); 
		TimerStop(write);
		if (ret)
			throw std::runtime_error("Could not create output file " + outfilename + "\n");

		/* Process the global fillmode */
		ProcessGlobalFillmode(infile, ncid);

		/* Next process dimensions */
		DimensionMap dimension_map = ProcessDimensions(infile, ncid,comm, mpirank, nproc);

		/* For each variable, define a variable with PIO */
		VariableMap vars_map = ProcessVariableDefinitions(infile, ncid, dimension_map, comm, mpirank, nproc);

		/* Process the global attributes */
		ProcessGlobalAttributes(infile, ncid, dimension_map, vars_map);

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
				if (!mpirank && debug_out) cout << "Convert variable: " << v << endl;

				if (v.find("decomp_id/")==string::npos && 
					v.find("frame_id/")==string::npos &&
					v.find("fillval_id/")==string::npos) {
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
							if (debug_out) printf("ConvertVariableTimedPutVar: %d\n",mpirank); fflush(stdout);
							ConvertVariableTimedPutVar(infile, wfiles, i, ncid, var, n_bp_writers, comm, mpirank, nproc);
						} else {
							if (debug_out) printf("ConvertVariablePutVar: %d\n",mpirank); fflush(stdout);
							ConvertVariablePutVar(infile, wfiles, i, ncid, var, mpirank, nproc);
						}
					} else if (op == "darray") {
						/* Variable was written with pio_write_darray() with a decomposition */
						if (debug_out) printf("ConvertVariableDarray: %d\n",mpirank); fflush(stdout);
						ConvertVariableDarray(infile, i, ncid, var, wfiles, decomp_map, n_bp_writers,iosysid, comm, mpirank, nproc);
					} else {
						if (!mpirank && debug_out)
							cout << "  WARNING: unknown operation " << op << ". Will not process this variable\n";
					}
					free(ncop);
				}
			}
			FlushStdout_nm(comm);
			PIOc_sync(ncid); /* FIXME: flush after each variable until development is done. Remove for efficiency */
		}
		TimerStart(write);
	
 		for (std::map<std::string,Decomposition>::iterator it=decomp_map.begin(); it!=decomp_map.end(); ++it) {
		     Decomposition d = it->second;
		     int err_code = PIOc_freedecomp(iosysid, d.ioid);
		     if (err_code!=0) {
		           printf("ERROR: PIOc_freedecomp: %d\n",err_code);
		             fflush(stdout);
		     }
		}
	
		pio_set_imax(save_imax);
	
		ret = PIOc_sync(ncid);
		ret = PIOc_closefile(ncid);
		TimerStop(write);
		TimerStart(read);

		MPI_Barrier(comm);
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

void usage_nm(string prgname)
{
        cout << "Usage: " << prgname << " bp_file  nc_file  pio_io_type\n";
        cout << "   bp file   :  data produced by PIO with ADIOS format\n";
        cout << "   nc file   :  output file name after conversion\n";
        cout << "   pio format:  output PIO_IO_TYPE. Supported parameters:\n";
        cout << "                pnetcdf  netcdf  netcdf4c  netcdf4p   or:\n";
        cout << "                   1       2        3         4\n";
}

enum PIO_IOTYPE GetIOType_nm(string t)
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

int ConvertBPToNC(string infilepath, string outfilename, string piotype, MPI_Comm comm_in)
{
	int ret = 0;
	int iosysid = 0;

	MPI_Comm comm   = comm_in;
	MPI_Comm w_comm = comm_in;
	int mpirank, w_mpirank;
	int nproc, w_nproc;

    MPI_Comm_set_errhandler(w_comm, MPI_ERRORS_RETURN);
    MPI_Comm_rank(w_comm, &w_mpirank);
    MPI_Comm_size(w_comm, &w_nproc);

	/* 
 	 * Check if the number of nodes is less than or equal to the number of BP files.
 	 * If not, create a new comm.
 	 *
 	 */
	int num_files = GetNumOfFiles(infilepath); 
	int io_proc   = 0;
	if (num_files<w_nproc) {
		fprintf(stderr, "Warning: #files: %d < #procs: %d\n",num_files,w_nproc); 
		fflush(stderr);
		if (w_mpirank<num_files) /* I/O nodes */
			io_proc = 1; 
		MPI_Comm_split(w_comm,io_proc,w_mpirank,&comm);
    	MPI_Comm_rank(comm, &mpirank);
    	MPI_Comm_size(comm, &nproc);
	} else {
		io_proc = 1;
		comm    = w_comm;
		mpirank = w_mpirank;
		nproc   = w_nproc;
	}
	
    TimerInitialize_nm();

    try {
		if (io_proc) {
        	enum PIO_IOTYPE pio_iotype = GetIOType_nm(piotype);
        	iosysid = InitPIO(comm,mpirank,nproc);
        	ConvertBPFile(infilepath, outfilename, pio_iotype, iosysid,comm, mpirank, nproc);
        	PIOc_finalize(iosysid);
        	TimerReport_nm(comm);
		}
		MPI_Barrier(w_comm);
    } catch (std::invalid_argument &e) {
        std::cout << e.what() << std::endl;
        return 2;
    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
        return 3;
    }

    TimerFinalize_nm();
    return ret;
}

#ifdef __cplusplus
extern "C" {
#endif

int C_API_ConvertBPToNC(const char *infilepath, const char *outfilename, const char *piotype, MPI_Comm comm_in)
{
    return ConvertBPToNC(string(infilepath), string(outfilename), string(piotype), comm_in);
}

#ifdef __cplusplus
}
#endif
