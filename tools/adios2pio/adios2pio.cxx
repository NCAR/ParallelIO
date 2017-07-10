#include <iostream>
#include <string>
#include <vector>
#include <mpi.h>
#include "pio.h"
#include "adios_read.h"
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

void InitPIO()
{
    if (PIOc_Init_Intracomm(comm, nproc, 1,
            0, PIO_REARR_SUBSET, &iosysid))
        throw std::runtime_error("PIO initialization failed\n");
}

std::vector<int> AssignWriteRanks(int n_bp_writers)
{
    if (!mpirank) cout << "The BP file was written by " << to_string(n_bp_writers) << " processes\n";
    int nwb = n_bp_writers / nproc; // number of blocks to process
    int start_wb; // starting wb (in [0..nwb-1])
    if (mpirank < n_bp_writers % nproc)
        nwb++;

    if (mpirank < n_bp_writers % nproc)
        start_wb = mpirank * nwb;
    else
        start_wb = mpirank * nwb + n_bp_writers % nproc;
    cout << "Process " << to_string(mpirank) << " start block = " << to_string(start_wb) <<
            " number of blocks = " << to_string(nwb) << std::endl;
    MPI_Barrier(comm);
    std::vector<int> blocks(nwb);
    for (int i=0; i<nwb; ++i)
        blocks[i]=start_wb+i;
    return blocks;
}

void ConvertBPFile(string infilename, string outfilename, int pio_iotype)
{
    ADIOS_FILE * infile = adios_read_open_file(infilename.c_str(), ADIOS_READ_METHOD_BP, comm);
    if (!infile)
        throw std::runtime_error("ADIOS: "+ string(adios_errmsg()) + "\n");

    try {
        ncid = -1;
        int ret = adios_schedule_read(infile, NULL, "info/nproc", 0, 1, &n_bp_writers);
        if (ret)
            throw std::runtime_error("Invalid BP file: missing 'info/nproc' variable\n");
        adios_perform_reads(infile, 1);

        /* Number of BP file writers != number of processes here */
        std::vector<int> wblocks; // from-to writeblocks
        wblocks = AssignWriteRanks(n_bp_writers);


        /* First process decompositions */
        std::map<std::string,int> decomp_map;
        for (int i = 0; i < infile->nvars; i++)
        {
            string v = infile->var_namelist[i];
            if (v.find("decomp/") != string::npos)
            {
                /* Read all decomposition blocks assigned to this process,
                 * create one big array from them and
                 * create a single big decomposition with PIO
                 */
                string decompname = v.substr(7);
                if (!mpirank) cout << "Process decomposition " << decompname << endl;

                /* Sum the sizes of blocks assigned to this process */
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
                    cout << "rank " << to_string(mpirank) << ": read decomp wb = " << to_string(wb) <<
                            " start = " << to_string(offset) <<
                            " elems = " << to_string(vi->blockinfo[wb].count[0]) << endl;
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
                int ioid;
                PIOc_InitDecomp(iosysid, *piotype, *decomp_ndims, decomp_dims, (PIO_Offset)nelems,
                               d.data(), &ioid, NULL, NULL, NULL);

                free(piotype);
                free(decomp_ndims);
                free(decomp_dims);
                decomp_map[decompname] = ioid;
            }
            MPI_Barrier(comm);
        }

        /* Create output file */
        ret = PIOc_createfile(iosysid, &ncid, &pio_iotype, outfilename.c_str(), PIO_CLOBBER);
        if (ret)
            throw std::runtime_error("Could not create output file " + outfilename + "\n");

        /* Next process dimensions */
        std::map<std::string,int> dimensions_map;
        for (int i = 0; i < infile->nvars; i++)
        {
            string v = infile->var_namelist[i];
            if (v.find("dim/") != string::npos)
            {
                /* For each dimension stored, define a dimension variable with PIO */
                string dimname = v.substr(4);
                if (!mpirank) cout << "Process dimension " << dimname << endl;

                unsigned long long dimval;
                int ret = adios_schedule_read(infile, NULL, infile->var_namelist[i], 0, 1, &dimval);
                adios_perform_reads(infile, 1);
                int dimid;
                PIOc_def_dim(ncid, dimname.c_str(), (PIO_Offset)dimval, &dimid);
                dimensions_map[dimname] = dimid;

            }
            MPI_Barrier(comm);
        }

        /* For each variable, define a variable with PIO */
        std::map<string, int> vars_map;
        for (int i = 0; i < infile->nvars; i++)
        {
            string v = infile->var_namelist[i];
            if (v.find("darray/") != string::npos)
            {
                /* For each variable written by pio_write_darray, read with ADIOS then write with PIO */
                string varname = v.substr(7);
                if (!mpirank) cout << "Process darray variable " << varname << endl;

                string attname = string(infile->var_namelist[i]) + "/nctype";
                int asize;
                int *nctype;
                ADIOS_DATATYPES atype;
                adios_get_attr(infile, attname.c_str(), &atype, &asize, (void**)&nctype);

                attname = string(infile->var_namelist[i]) + "/ndims";
                int *ndims;
                adios_get_attr(infile, attname.c_str(), &atype, &asize, (void**)&ndims);

                char **dimnames;
                attname = string(infile->var_namelist[i]) + "/dims";
                adios_get_attr(infile, attname.c_str(), &atype, &asize, (void**)&dimnames);

                int dimids[*ndims];
                for (int i=0; i < *ndims; i++)
                {
                    //cout << "Dim " << to_string(i) << " = " <<  dimnames[i] << endl;
                    dimids[i] = dimensions_map[dimnames[i]];
                }

                int varid;
                PIOc_def_var(ncid, varname.c_str(), *nctype, *ndims, dimids, &varid);
                vars_map[varname] = varid;

                free(nctype);
                free(ndims);
                free(dimnames);
            }
            MPI_Barrier(comm);
        }

        PIOc_enddef(ncid);


        /* For each variable,
         * read in the data with ADIOS then write it out with PIO */
        for (int i = 0; i < infile->nvars; i++)
        {
            string v = infile->var_namelist[i];
            if (v.find("darray/") != string::npos)
            {
                /* For each variable written by pio_write_darray, read with ADIOS then write with PIO */
                string varname = v.substr(7);
                if (!mpirank) cout << "Convert darray variable " << varname << endl;

                string attname = string(infile->var_namelist[i]) + "/decomp";
                int asize;
                char *decompname;
                ADIOS_DATATYPES atype;
                adios_get_attr(infile, attname.c_str(), &atype, &asize, (void**)&decompname);
                int decompid = decomp_map[decompname];
                free(decompname);

                /* Sum the sizes of blocks assigned to this process */
                ADIOS_VARINFO *vi = adios_inq_var(infile, infile->var_namelist[i]);
                adios_inq_var_blockinfo(infile, vi);
                uint64_t nelems = 0;
                for (auto wb : wblocks)
                {
                    nelems += vi->blockinfo[wb].count[0];
                }
                int elemsize = adios_type_size(vi->type,NULL);
                std::vector<char> d(nelems * elemsize);
                uint64_t offset = 0;
                for (auto wb : wblocks)
                {
                    cout << "rank " << to_string(mpirank) << ": read var = " << to_string(wb) <<
                            " start byte = " << to_string(offset) <<
                            " elems = " << to_string(vi->blockinfo[wb].count[0]) << endl;
                    ADIOS_SELECTION *wbsel = adios_selection_writeblock(wb);
                    int ret = adios_schedule_read(infile, wbsel, infile->var_namelist[i], 0, 1,
                            d.data()+offset);
                    adios_perform_reads(infile, 1);
                    offset += vi->blockinfo[wb].count[0] * elemsize;
                }

                ret = PIOc_write_darray(ncid, vars_map[varname], decompid, (PIO_Offset)nelems,
                                        d.data(), NULL);

                adios_free_varinfo(vi);
            }
            MPI_Barrier(comm);
        }
        ret = PIOc_sync(ncid);
        ret = PIOc_closefile(ncid);
        adios_read_close(infile);
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
    try {
        enum PIO_IOTYPE pio_iotype = GetIOType(argv[3]);
        InitPIO();
        ConvertBPFile(infilename, outfilename, pio_iotype);
        PIOc_finalize(iosysid);
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
    return ret;
}
