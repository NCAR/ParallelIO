#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <array>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <unistd.h>

extern "C"{
#include "pio_config.h"
#include "pio.h"
#include "pio_internal.h"
#include "spio_io_summary.h"
}

#include "spio_io_summary.hpp"
#include "spio_serializer.hpp"

namespace PIO_Util{
  namespace IO_Summary_Util{
    namespace GVars{
      /* File I/O statistics cache:
       * This cache stores per file I/O statistics per I/O system
       * std::map<IOID, std::map<FILENAME, FILE_SSTATS> >
       */
      /* FIXME: Since iosystem and other structs are currently used
       * in C we cannot store this cache there. We need to get rid of this global cache.
       */
      std::map<int, std::map<std::string, IO_summary_stats_t> > file_sstats_cache;
    }
  } // namespace IO_Summary_Util
} // namespace PIO_Util

/* min/max definitions for PIO_Offset type */
static inline PIO_Offset spio_min(PIO_Offset a, PIO_Offset b)
{
  return (a < b) ? a : b;
}

static inline PIO_Offset spio_max(PIO_Offset a, PIO_Offset b)
{
  return (a > b) ? a : b;
}

static inline double spio_min(double a, double b)
{
  return (a < b) ? a : b;
}

static inline double spio_max(double a, double b)
{
  return (a > b) ? a : b;
}

/* Cache file I/O statistics
 * This function is called on a file close to cache the statistics for that
 * file
 */
static void cache_file_stats(int iosysid, const std::string &filename,
  const PIO_Util::IO_Summary_Util::IO_summary_stats_t &file_sstats)
{
  assert(filename.size() > 0);
  if(PIO_Util::IO_Summary_Util::GVars::file_sstats_cache.count(iosysid) != 0){
    if(PIO_Util::IO_Summary_Util::GVars::file_sstats_cache[iosysid].count(filename) == 0){
      /* First time this file stats being cached for an iosysid with other
       * cached file stats
       */
      PIO_Util::IO_Summary_Util::GVars::file_sstats_cache[iosysid][filename] = file_sstats;
    }
    else{
      /* Stats for this file has been cached for this iosysid before.
       * e.g. File being opened multiple times on the same I/O system
       */
      PIO_Util::IO_Summary_Util::IO_summary_stats_t sstats =
        PIO_Util::IO_Summary_Util::GVars::file_sstats_cache[iosysid][filename];
      sstats.rb_total += file_sstats.rb_total;
      sstats.rb_min += file_sstats.rb_min;
      sstats.rb_max += file_sstats.rb_max;
      sstats.wb_total += file_sstats.wb_total;
      sstats.wb_min += file_sstats.wb_min;
      sstats.wb_max += file_sstats.wb_max;
      sstats.rtime_min += file_sstats.rtime_min;
      sstats.rtime_max += file_sstats.rtime_max;
      sstats.wtime_min += file_sstats.wtime_min;
      sstats.wtime_max += file_sstats.wtime_max;
      sstats.ttime_min += file_sstats.ttime_min;
      sstats.ttime_max += file_sstats.ttime_max;
      PIO_Util::IO_Summary_Util::GVars::file_sstats_cache[iosysid][filename] = sstats;
    }
  }
  else{
    /* First file stats being cached for iosysid */
    std::map<std::string, PIO_Util::IO_Summary_Util::IO_summary_stats_t> file_sstats_map;
    file_sstats_map[filename] = file_sstats;
    PIO_Util::IO_Summary_Util::GVars::file_sstats_cache[iosysid] = file_sstats_map;
  }
}

/* Get all the I/O statistics for all files in an I/O system */
static void get_file_stats(int iosysid, std::vector<std::string> &filenames,
  std::vector<PIO_Util::IO_Summary_Util::IO_summary_stats_t> &file_stats)
{
  if(PIO_Util::IO_Summary_Util::GVars::file_sstats_cache.count(iosysid) == 0){
    /* No file stats cached for this I/O system id */
    return;
  }
  for(std::map<std::string, PIO_Util::IO_Summary_Util::IO_summary_stats_t>::const_iterator
        citer = PIO_Util::IO_Summary_Util::GVars::file_sstats_cache[iosysid].cbegin();
        citer != PIO_Util::IO_Summary_Util::GVars::file_sstats_cache[iosysid].cend();
        ++citer){
    filenames.push_back(citer->first);
    file_stats.push_back(citer->second);
  }
}

/* Convert IO_summary_stats_t struct to string */
std::string PIO_Util::IO_Summary_Util::io_summary_stats2str(const IO_summary_stats_t &io_sstats)
{
  std::stringstream ostr;
  ostr << "Read bytes (total) : " << bytes2hr(io_sstats.rb_total) << "\n";
  ostr << "Read bytes (min) : " << bytes2hr(io_sstats.rb_min) << "\n";
  ostr << "Read bytes (max) : " << bytes2hr(io_sstats.rb_max) << "\n";
  ostr << "Write bytes (total) : " << bytes2hr(io_sstats.wb_total) << "\n";
  ostr << "Write bytes (min) : " << bytes2hr(io_sstats.wb_min) << "\n";
  ostr << "Write bytes (max) : " << bytes2hr(io_sstats.wb_max) << "\n";
  ostr << "Read time in secs (min) : " << io_sstats.rtime_min << "\n";
  ostr << "Read time in secs (max) : " << io_sstats.rtime_max << "\n";
  ostr << "Write time in secs (min) : " << io_sstats.wtime_min << "\n";
  ostr << "Write time in secs (max) : " << io_sstats.wtime_max << "\n";
  ostr << "Total time in secs (min) : " << io_sstats.ttime_min << "\n";
  ostr << "Total time in secs (max) : " << io_sstats.ttime_max << "\n";

  return ostr.str();
}

/* Convert bytes to human readable format
 * e.g. nb = 1024 (1024 bytes) would be converted to 1 KB
 */
std::string PIO_Util::IO_Summary_Util::bytes2hr(PIO_Offset nb)
{
  const std::vector<std::pair<std::size_t, std::string> > predef_fmts =
    { {1024 * 1024 * 1024, "GB"}, {1024 * 1024, "MB"}, {1024, "KB"} };

  std::stringstream ostr;
  for(std::vector<std::pair<std::size_t, std::string> >::const_iterator
        citer = predef_fmts.cbegin(); citer != predef_fmts.cend(); ++citer){
    if((nb / (*citer).first) > 0){
      ostr << ((double ) nb)/(*citer).first << " " << (*citer).second;
      return ostr.str();
    }
  }

  ostr << nb << " bytes";
  return ostr.str();
}

/* Definitions for the functions IO_summary_stats2mpi class, the class is used to convert
 * the struct to an MPI datatype
 */
PIO_Util::IO_Summary_Util::IO_summary_stats2mpi::IO_summary_stats2mpi() : dt_(MPI_DATATYPE_NULL)
{
  int mpi_errno = MPI_SUCCESS;
  /* Corresponds to defn of IO_summary_stats */
  std::array<MPI_Datatype, NUM_IO_SUMMARY_STATS_MEMBERS> types = {MPI_OFFSET, MPI_OFFSET,
    MPI_OFFSET, MPI_OFFSET, MPI_OFFSET, MPI_OFFSET, MPI_DOUBLE, MPI_DOUBLE,
    MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE};

  std::array<MPI_Aint, NUM_IO_SUMMARY_STATS_MEMBERS> disps;
  std::array<int, NUM_IO_SUMMARY_STATS_MEMBERS> blocklens = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

#ifndef MPI_SERIAL
  get_io_summary_stats_address_disps(disps);

  mpi_errno = MPI_Type_create_struct(NUM_IO_SUMMARY_STATS_MEMBERS, blocklens.data(),
                disps.data(), types.data(), &dt_);
  if(mpi_errno != MPI_SUCCESS){
    throw std::runtime_error("Creating MPI datatype for I/O summary stats struct failed");
  }

  mpi_errno = MPI_Type_commit(&dt_);
  if(mpi_errno != MPI_SUCCESS){
    throw std::runtime_error("Committing MPI datatype for I/O summary stats struct failed");
  }
#endif
}

MPI_Datatype PIO_Util::IO_Summary_Util::IO_summary_stats2mpi::get_mpi_datatype(void ) const
{
  return dt_;
}

PIO_Util::IO_Summary_Util::IO_summary_stats2mpi::~IO_summary_stats2mpi()
{
  if(dt_ != MPI_DATATYPE_NULL){
    MPI_Type_free(&dt_);
  }
}

void PIO_Util::IO_Summary_Util::IO_summary_stats2mpi::get_io_summary_stats_address_disps(
  std::array<MPI_Aint, NUM_IO_SUMMARY_STATS_MEMBERS> &disps) const
{
  int mpi_errno = MPI_SUCCESS;
  IO_summary_stats io_sstats;

#ifndef MPI_SERIAL
  mpi_errno = MPI_Get_address(&io_sstats, &disps[0]);
  if(mpi_errno != MPI_SUCCESS){
    throw std::runtime_error("Getting address for I/O summary stat struct members failed");
  }

  mpi_errno = MPI_Get_address(&(io_sstats.rb_min), &disps[1]);
  if(mpi_errno != MPI_SUCCESS){
    throw std::runtime_error("Getting address for I/O summary stat struct members failed");
  }

  mpi_errno = MPI_Get_address(&(io_sstats.rb_max), &disps[2]);
  if(mpi_errno != MPI_SUCCESS){
    throw std::runtime_error("Getting address for I/O summary stat struct members failed");
  }

  mpi_errno = MPI_Get_address(&(io_sstats.wb_total), &disps[3]);
  if(mpi_errno != MPI_SUCCESS){
    throw std::runtime_error("Getting address for I/O summary stat struct members failed");
  }

  mpi_errno = MPI_Get_address(&(io_sstats.wb_min), &disps[4]);
  if(mpi_errno != MPI_SUCCESS){
    throw std::runtime_error("Getting address for I/O summary stat struct members failed");
  }

  mpi_errno = MPI_Get_address(&(io_sstats.wb_max), &disps[5]);
  if(mpi_errno != MPI_SUCCESS){
    throw std::runtime_error("Getting address for I/O summary stat struct members failed");
  }

  mpi_errno = MPI_Get_address(&(io_sstats.rtime_min), &disps[6]);
  if(mpi_errno != MPI_SUCCESS){
    throw std::runtime_error("Getting address for I/O summary stat struct members failed");
  }

  mpi_errno = MPI_Get_address(&(io_sstats.rtime_max), &disps[7]);
  if(mpi_errno != MPI_SUCCESS){
    throw std::runtime_error("Getting address for I/O summary stat struct members failed");
  }

  mpi_errno = MPI_Get_address(&(io_sstats.wtime_min), &disps[8]);
  if(mpi_errno != MPI_SUCCESS){
    throw std::runtime_error("Getting address for I/O summary stat struct members failed");
  }

  mpi_errno = MPI_Get_address(&(io_sstats.wtime_max), &disps[9]);
  if(mpi_errno != MPI_SUCCESS){
    throw std::runtime_error("Getting address for I/O summary stat struct members failed");
  }

  mpi_errno = MPI_Get_address(&(io_sstats.ttime_min), &disps[10]);
  if(mpi_errno != MPI_SUCCESS){
    throw std::runtime_error("Getting address for I/O summary stat struct members failed");
  }

  mpi_errno = MPI_Get_address(&(io_sstats.ttime_max), &disps[11]);
  if(mpi_errno != MPI_SUCCESS){
    throw std::runtime_error("Getting address for I/O summary stat struct members failed");
  }

  assert(disps.size() > 0);
  MPI_Aint base_addr = disps[0];
  for(std::size_t i = 0; i < disps.size(); i++){
    disps[i] -= base_addr;
  }
#else
  throw std::runtime_error("Getting address for I/O summary stat struct members is not supported for MPI serial");
#endif
}

extern "C"{
/* User defined MPI reduce function to consolidate I/O performance statistics */
void reduce_io_summary_stats(PIO_Util::IO_Summary_Util::IO_summary_stats_t *in_arr,
  PIO_Util::IO_Summary_Util::IO_summary_stats_t *inout_arr,
  int *nelems, MPI_Datatype *pdt);
} /* extern "C" */

void reduce_io_summary_stats(PIO_Util::IO_Summary_Util::IO_summary_stats_t *in_arr,
  PIO_Util::IO_Summary_Util::IO_summary_stats_t *inout_arr,
  int *nelems, MPI_Datatype *pdt)
{
  assert(in_arr && inout_arr && nelems && pdt);

  for(int i = 0; i < *nelems; i++, in_arr++, inout_arr++){
    inout_arr->rb_total += in_arr->rb_total;
    inout_arr->rb_min = spio_min(inout_arr->rb_min, in_arr->rb_min);
    inout_arr->rb_max = spio_max(inout_arr->rb_max, in_arr->rb_max);
    inout_arr->wb_total += in_arr->wb_total;
    inout_arr->wb_min = spio_min(inout_arr->wb_min, in_arr->wb_min);
    inout_arr->wb_max = spio_max(inout_arr->wb_max, in_arr->wb_max);

    inout_arr->rtime_min = spio_min(inout_arr->rtime_min, in_arr->rtime_min);
    inout_arr->rtime_max = spio_max(inout_arr->rtime_max, in_arr->rtime_max);
    inout_arr->wtime_min = spio_min(inout_arr->wtime_min, in_arr->wtime_min);
    inout_arr->wtime_max = spio_max(inout_arr->wtime_max, in_arr->wtime_max);
    inout_arr->ttime_min = spio_min(inout_arr->ttime_min, in_arr->ttime_min);
    inout_arr->ttime_max = spio_max(inout_arr->ttime_max, in_arr->ttime_max);
  }
}

/* Guess the I/O system (E3SM component) name from the file names
 * used by the I/O system
 */
static std::string file_names_to_ios_name(iosystem_desc_t *ios,
        std::vector<std::string> &file_names)
{
  assert(ios);

  std::string ios_name(ios->sname);
  /* Output file name hints */
  std::vector<std::pair<std::string, std::string> > e3sm_comp_ohints =
    {
      {".cpl.","CPL"},
      {".eam.","EAM"},
      {".elm.","ELM"},
      {".mosart.","MOSART"},
      {".mpaso.","MPASO"},
      {".mpasli.","MPASLI"},
      {".mpassi.","MPASSI"}
    };
  /* Input file name hints */
  std::vector<std::pair<std::string, std::string> > e3sm_comp_ihints =
    {
      {"/inputdata/cpl","CPL"},
      {"/inputdata/atm","EAM"},
      {"/inputdata/clm","ELM"},
      {"/inputdata/lnd","ELM"},
      {"/inputdata/rof/mosart","MOSART"},
      {"/inputdata/rof","RTM"},
      {"/inputdata/ocn/mpas-o","MPASO"},
      {"/inputdata/ocn","OCN"},
      {"/inputdata/glc/mpasli","MPASLI"},
      {"/inputdata/glc","GLC"},
      {"/inputdata/ice/mpas-cice","MPASSI"},
      {"/inputdata/ice","CICE"},
      {"/inputdata/wav","WAV"}
    };
  /* FIXME: We currently have no hints for IAC & ESP components */

  for(std::vector<std::string>::const_iterator fiter = file_names.cbegin();
      fiter != file_names.cend(); ++fiter){
    /* First check output files for hints */
    for(std::vector<std::pair<std::string, std::string> >::const_iterator
          hiter = e3sm_comp_ohints.cbegin(); hiter != e3sm_comp_ohints.cend(); ++hiter){
      if((*fiter).find(hiter->first) != std::string::npos){
        return hiter->second;
      }
    }

    /* Check input files for hints */
    for(std::vector<std::pair<std::string, std::string> >::const_iterator
          hiter = e3sm_comp_ihints.cbegin(); hiter != e3sm_comp_ihints.cend(); ++hiter){
      if((*fiter).find(hiter->first) != std::string::npos){
        return hiter->second;
      }
    }
  }

  return ios_name;
}

/* Cache or write I/O system I/O performance statistics */
static int cache_or_print_stats(iosystem_desc_t *ios, int root_proc,
  PIO_Util::IO_Summary_Util::IO_summary_stats_t &iosys_gio_sstats,
  std::vector<std::string> &file_names,
  std::vector<PIO_Util::IO_Summary_Util::IO_summary_stats_t> &file_gio_sstats)
{
  /* Static cache of overall I/O statistics for the application */
  static PIO_Util::IO_Summary_Util::IO_summary_stats_t cached_overall_gio_sstats;
  /* Static cache of I/O statistics for I/O systems */
  static std::vector<PIO_Util::IO_Summary_Util::IO_summary_stats_t> cached_ios_gio_sstats;
  /* Static cache of I/O system names, these have a one to noe correspondence with the
   * I/O statistics cache above, cached_ios_gio_sstats
   */
  static std::vector<std::string> cached_ios_names;
  /* Static cache of I/O statistics for files */
  static std::vector<std::vector<PIO_Util::IO_Summary_Util::IO_summary_stats_t> >
          cached_file_gio_sstats;
  /* Static cache of file names, these have a one to noe correspondence with the
   * I/O statistics cache above, cached_file_gio_sstats
   */
  static std::vector<std::vector<std::string> > cached_file_names;
  int niosys = 0;
  int ierr;

  assert(ios);

  /* Cache only in the root process */
  if(ios->union_rank == root_proc){
    cached_overall_gio_sstats.rb_total += iosys_gio_sstats.rb_total;
    /* FIXME: Since we are guessing the values below from Component I/O stats,
     * they are upper bounds, not accurate. We need separate timers to capture
     * overall I/O perf
     */
    cached_overall_gio_sstats.rb_min += iosys_gio_sstats.rb_min;
    cached_overall_gio_sstats.rb_max += iosys_gio_sstats.rb_max;
    cached_overall_gio_sstats.wb_total += iosys_gio_sstats.wb_total;
    cached_overall_gio_sstats.wb_min += iosys_gio_sstats.wb_min;
    cached_overall_gio_sstats.wb_max += iosys_gio_sstats.wb_max;
    cached_overall_gio_sstats.rtime_min += iosys_gio_sstats.rtime_min;
    cached_overall_gio_sstats.rtime_max += iosys_gio_sstats.rtime_max;
    cached_overall_gio_sstats.wtime_min += iosys_gio_sstats.wtime_min;
    cached_overall_gio_sstats.wtime_max += iosys_gio_sstats.wtime_max;
    cached_overall_gio_sstats.ttime_min += iosys_gio_sstats.ttime_min;
    cached_overall_gio_sstats.ttime_max += iosys_gio_sstats.ttime_max;

    cached_ios_gio_sstats.push_back(iosys_gio_sstats);
    cached_ios_names.push_back(file_names_to_ios_name(ios, file_names));
    cached_file_gio_sstats.push_back(file_gio_sstats);
    cached_file_names.push_back(file_names);
  }

  ierr = pio_num_iosystem(&niosys);
  if(ierr != PIO_NOERR){
    return pio_err(ios, NULL, PIO_EINTERNAL, __FILE__, __LINE__,
                    "Unable to query the number of active iosystems, ret = %d", ierr);
  }

  /* If this I/O system is the last one, print the I/O stats if this process has any 
   * cached stats
   */
  if((niosys == 1) && (cached_ios_gio_sstats.size() > 0)){
    const std::string model_name = "Scorpio";
    const std::size_t ONE_MB = 1024 * 1024;
    long long int pid = static_cast<long long int>(getpid());
    /* The summary file name is : "io_perf_summary_<PID>.[txt|json]" */
    std::string sfname = std::string("io_perf_summary_") + std::to_string(pid);
    const std::string sfname_txt_suffix(".txt");
    const std::string sfname_json_suffix(".json");

    assert(cached_ios_gio_sstats.size() == cached_ios_names.size());

    /* Create TEXT and JSON serializers */
    std::unique_ptr<PIO_Util::SPIO_serializer> spio_ser =
      PIO_Util::Serializer_Utils::create_serializer(PIO_Util::Serializer_type::TEXT_SERIALIZER,
        sfname + sfname_txt_suffix);
    std::unique_ptr<PIO_Util::SPIO_serializer> spio_json_ser =
      PIO_Util::Serializer_Utils::create_serializer(PIO_Util::Serializer_type::JSON_SERIALIZER,
        sfname + sfname_json_suffix);
    std::vector<std::pair<std::string, std::string> > vals;
    int id = spio_ser->serialize("ScorpioIOSummaryStatistics", vals);
    int json_id = spio_json_ser->serialize("ScorpioIOSummaryStatistics", vals);

    /* Add Overall I/O performance statistics */
    std::vector<std::pair<std::string, std::string> > overall_comp_vals;
    PIO_Util::Serializer_Utils::serialize_pack("name", model_name, overall_comp_vals);
    PIO_Util::Serializer_Utils::serialize_pack("avg_wtput(MB/s)",
      (cached_overall_gio_sstats.wtime_max > 0.0) ?
      (cached_overall_gio_sstats.wb_total / (ONE_MB * cached_overall_gio_sstats.wtime_max)) : 0.0,
      overall_comp_vals);

    PIO_Util::Serializer_Utils::serialize_pack("avg_rtput(MB/s)",
      (cached_overall_gio_sstats.rtime_max > 0.0) ?
      (cached_overall_gio_sstats.rb_total / (ONE_MB * cached_overall_gio_sstats.rtime_max)) : 0.0,
      overall_comp_vals);

    PIO_Util::Serializer_Utils::serialize_pack("tot_wb(bytes)",
      cached_overall_gio_sstats.wb_total, overall_comp_vals);

    PIO_Util::Serializer_Utils::serialize_pack("tot_rb(bytes)",
      cached_overall_gio_sstats.rb_total, overall_comp_vals);

    PIO_Util::Serializer_Utils::serialize_pack("tot_wtime(s)",
      cached_overall_gio_sstats.wtime_max, overall_comp_vals);

    PIO_Util::Serializer_Utils::serialize_pack("tot_rtime(s)",
      cached_overall_gio_sstats.rtime_max, overall_comp_vals);

    PIO_Util::Serializer_Utils::serialize_pack("tot_time(s)",
      cached_overall_gio_sstats.ttime_max, overall_comp_vals);

    spio_ser->serialize(id, "OverallIOStatistics", overall_comp_vals);
    spio_json_ser->serialize(json_id, "OverallIOStatistics", overall_comp_vals);

    /* Add Model Component I/O performance statistics */
    std::vector<std::vector<std::pair<std::string, std::string> > > comp_vvals;
    std::vector<int> comp_vvals_ids, json_comp_vvals_ids;

    for(std::size_t i = 0; i < cached_ios_gio_sstats.size(); i++){
      std::vector<std::pair<std::string, std::string> > comp_vals;
      /*
      LOG((1, "I/O stats recv (component = %s):\n%s",
            cached_ios_names[i].c_str(),
            PIO_Util::IO_Summary_Util::io_summary_stats2str(cached_ios_gio_sstats[i]).c_str()));
      std::cout << "Model Component I/O Statistics (" << cached_ios_names[i].c_str() << ")\n";
      std::cout << "Average I/O write throughput = "
        << PIO_Util::IO_Summary_Util::bytes2hr((cached_ios_gio_sstats[i].wtime_max > 0.0) ?
              (cached_ios_gio_sstats[i].wb_total / cached_ios_gio_sstats[i].wtime_max) : 0)
        << "/s \n";
      std::cout << "Average I/O read thoughput = "
        << PIO_Util::IO_Summary_Util::bytes2hr((cached_ios_gio_sstats[i].rtime_max > 0.0) ?
            (cached_ios_gio_sstats[i].rb_total / cached_ios_gio_sstats[i].rtime_max) : 0)
        << "/s \n";
      */


      PIO_Util::Serializer_Utils::serialize_pack("name", cached_ios_names[i], comp_vals);
      PIO_Util::Serializer_Utils::serialize_pack("avg_wtput(MB/s)",
        (cached_ios_gio_sstats[i].wtime_max > 0.0) ?
        (cached_ios_gio_sstats[i].wb_total / (ONE_MB * cached_ios_gio_sstats[i].wtime_max)) : 0.0,
        comp_vals);

      PIO_Util::Serializer_Utils::serialize_pack("avg_rtput(MB/s)",
        (cached_ios_gio_sstats[i].rtime_max > 0.0) ?
        (cached_ios_gio_sstats[i].rb_total / (ONE_MB * cached_ios_gio_sstats[i].rtime_max)) : 0.0,
        comp_vals);

      PIO_Util::Serializer_Utils::serialize_pack("tot_wb(bytes)",
        cached_ios_gio_sstats[i].wb_total, comp_vals);

      PIO_Util::Serializer_Utils::serialize_pack("tot_rb(bytes)",
        cached_ios_gio_sstats[i].rb_total, comp_vals);

      PIO_Util::Serializer_Utils::serialize_pack("tot_wtime(s)",
        cached_ios_gio_sstats[i].wtime_max, comp_vals);

      PIO_Util::Serializer_Utils::serialize_pack("tot_rtime(s)",
        cached_ios_gio_sstats[i].rtime_max, comp_vals);

      PIO_Util::Serializer_Utils::serialize_pack("tot_time(s)",
        cached_ios_gio_sstats[i].ttime_max, comp_vals);

      comp_vvals.push_back(comp_vals);
    }
    spio_ser->serialize(id, "ModelComponentIOStatistics", comp_vvals, comp_vvals_ids);
    spio_json_ser->serialize(json_id, "ModelComponentIOStatistics", comp_vvals, json_comp_vvals_ids);

    /* Add File I/O statistics */
    std::vector<std::vector<std::pair<std::string, std::string> > > file_vvals;
    std::vector<int> file_vvals_ids, json_file_vvals_ids;
    assert(cached_file_gio_sstats.size() == cached_ios_gio_sstats.size());
    for(std::size_t i = 0; i < cached_file_gio_sstats.size(); i++){
      for(std::size_t j = 0; j < cached_file_gio_sstats[i].size(); j++){
        std::vector<std::pair<std::string, std::string> > file_vals;
        /*
        LOG((1, "I/O stats recv (component = %s, file = %s):\n%s",
              cached_ios_names[i].c_str(),
              cached_file_names[i][j].c_str(),
              PIO_Util::IO_Summary_Util::io_summary_stats2str(cached_file_gio_sstats[i][j]).c_str()));
        std::cout << "File I/O Statistics (" << cached_file_names[i][j].c_str() << ")\n";
        std::cout << "Average I/O write throughput = "
          << PIO_Util::IO_Summary_Util::bytes2hr((cached_file_gio_sstats[i][j].wtime_max > 0.0) ?
                (cached_file_gio_sstats[i][j].wb_total / cached_file_gio_sstats[i][j].wtime_max) : 0)
          << "/s \n";
        std::cout << "Average I/O read thoughput = "
          << PIO_Util::IO_Summary_Util::bytes2hr((cached_file_gio_sstats[i][j].rtime_max > 0.0) ?
              (cached_file_gio_sstats[i][j].rb_total / cached_file_gio_sstats[i][j].rtime_max) : 0)
          << "/s \n";
        */

        const std::size_t ONE_MB = 1024 * 1024;

        PIO_Util::Serializer_Utils::serialize_pack("name", cached_file_names[i][j], file_vals);
        PIO_Util::Serializer_Utils::serialize_pack("avg_wtput(MB/s)",
          (cached_file_gio_sstats[i][j].wtime_max > 0.0) ?
          (cached_file_gio_sstats[i][j].wb_total / (ONE_MB * cached_file_gio_sstats[i][j].wtime_max)) : 0.0,
          file_vals);

        PIO_Util::Serializer_Utils::serialize_pack("avg_rtput(MB/s)",
          (cached_file_gio_sstats[i][j].rtime_max > 0.0) ?
          (cached_file_gio_sstats[i][j].rb_total / (ONE_MB * cached_file_gio_sstats[i][j].rtime_max)) : 0.0,
          file_vals);

        PIO_Util::Serializer_Utils::serialize_pack("tot_wb(bytes)",
          cached_file_gio_sstats[i][j].wb_total, file_vals);

        PIO_Util::Serializer_Utils::serialize_pack("tot_rb(bytes)",
          cached_file_gio_sstats[i][j].rb_total, file_vals);

        PIO_Util::Serializer_Utils::serialize_pack("tot_wtime(s)",
          cached_file_gio_sstats[i][j].wtime_max, file_vals);

        PIO_Util::Serializer_Utils::serialize_pack("tot_rtime(s)",
          cached_file_gio_sstats[i][j].rtime_max, file_vals);

        PIO_Util::Serializer_Utils::serialize_pack("tot_time(s)",
          cached_file_gio_sstats[i][j].ttime_max, file_vals);

        file_vvals.push_back(file_vals);
      }
    }
    spio_ser->serialize(id, "FileIOStatistics", file_vvals, file_vvals_ids);
    spio_json_ser->serialize(json_id, "FileIOStatistics", file_vvals, json_file_vvals_ids);

    /* Sync/flush I/O performance statistics to the files */
    spio_ser->sync();
    spio_json_ser->sync();
  }

  return PIO_NOERR;
}

/* Write I/O performance summary
 * This function is called when an I/O system is finalized. The performance data is
 * cached if there are any I/O systems pending to be finalized. When the last I/O
 * system is finalized the I/O performance statistics are written to the output files
 */
int spio_write_io_summary(iosystem_desc_t *ios)
{
  int ierr = 0;
  const std::vector<std::string> wr_timers = {
    ios->io_fstats->wr_timer_name
  };

  /* FIXME: We need better timers to distinguish between
   * read/write modes while opening a file
   */
  const std::vector<std::string> rd_timers = {
    ios->io_fstats->rd_timer_name
  };

  const std::vector<std::string> tot_timers = {
    ios->io_fstats->tot_timer_name
  };

#ifndef SPIO_IO_STATS
  return PIO_NOERR;
#endif

  assert(ios);

  /* For async I/O only collect statistics from the I/O processes */
  if(ios->async && !(ios->ioproc)){
    return PIO_NOERR;
  }

  /* Get timings for this I/O system from the timers */
  double total_wr_time = 0, total_rd_time = 0, total_tot_time = 0;
  for(std::vector<std::string>::const_iterator citer = wr_timers.cbegin();
        citer != wr_timers.cend(); ++citer){
    total_wr_time += spio_ltimer_get_wtime((*citer).c_str());
  }

  for(std::vector<std::string>::const_iterator citer = rd_timers.cbegin();
        citer != rd_timers.cend(); ++citer){
    total_rd_time += spio_ltimer_get_wtime((*citer).c_str());
  }

  for(std::vector<std::string>::const_iterator citer = tot_timers.cbegin();
        citer != tot_timers.cend(); ++citer){
    total_tot_time += spio_ltimer_get_wtime((*citer).c_str());
  }

  /*
  LOG((1, "Total read time = %f s, write time = %f s, total time = %f s", total_rd_time, total_wr_time, total_tot_time));
  LOG((1, "Total bytes read = %lld, bytes written = %lld",
        (unsigned long long) ios->io_fstats->rb, (unsigned long long) ios->io_fstats->wb));
  */

  PIO_Util::IO_Summary_Util::IO_summary_stats_t io_sstats;
  PIO_Util::IO_Summary_Util::IO_summary_stats2mpi io_sstats2mpi;

  io_sstats.rb_total = ios->io_fstats->rb;
  io_sstats.rb_min = io_sstats.rb_total;
  io_sstats.rb_max = io_sstats.rb_total;
  io_sstats.wb_total = ios->io_fstats->wb;
  io_sstats.wb_min = io_sstats.wb_total;
  io_sstats.wb_max = io_sstats.wb_total;

  io_sstats.rtime_min = total_rd_time;
  io_sstats.rtime_max = total_rd_time;
  io_sstats.wtime_min = total_wr_time;
  io_sstats.wtime_max = total_wr_time;
  io_sstats.ttime_min = total_tot_time;
  io_sstats.ttime_max = total_tot_time;

  /*
  LOG((1, "I/O stats sent :\n%s",
        PIO_Util::IO_Summary_Util::io_summary_stats2str(io_sstats).c_str()));
  */

  std::vector<std::string> filenames;
  std::vector<PIO_Util::IO_Summary_Util::IO_summary_stats_t> tmp_sstats;

  /* Get the I/O statistics of files belonging to this I/O system */
  get_file_stats(ios->iosysid, filenames, tmp_sstats);

  //PIO_Util::IO_Summary_Util::IO_summary_stats_t gio_sstats = {0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0};
  /* Add the component I/O stats to the end of file I/O stats */
  const std::size_t nelems_to_red = filenames.size() + 1;
  std::vector<PIO_Util::IO_Summary_Util::IO_summary_stats_t> gio_sstats(nelems_to_red);

  tmp_sstats.push_back(io_sstats);

  const int ROOT_PROC = 0;
#ifndef MPI_SERIAL
  MPI_Op op;
  ierr = MPI_Op_create((MPI_User_function *)reduce_io_summary_stats, true, &op);
  if(ierr != PIO_NOERR){
    LOG((1, "MPI_Op_create for reducing I/O memory stats failed"));
    return pio_err(ios, NULL, PIO_EINTERNAL, __FILE__, __LINE__,
            "Creating MPI reduction operation for reducing I/O summary statistics failed for I/O system (iosysid=%d)",
            ios->iosysid);
  }

  /* Consolidate I/O performance statistics for the files, belonging to this I/O system, and the I/O system */
  MPI_Comm comm = (ios->async) ? (ios->io_comm) : (ios->union_comm);
  ierr = MPI_Reduce(tmp_sstats.data(), gio_sstats.data(), nelems_to_red, io_sstats2mpi.get_mpi_datatype(),
                      op, ROOT_PROC, comm);
  if(ierr != PIO_NOERR){
    LOG((1, "Reducing I/O memory statistics failed"));
    MPI_Op_free(&op);
    return pio_err(ios, NULL, PIO_EINTERNAL, __FILE__, __LINE__,
            "MPI reduction operation for reducing I/O summary statistics failed for I/O system (iosysid=%d)",
            ios->iosysid);
  }

  ierr = MPI_Op_free(&op);
  if(ierr != PIO_NOERR){
    LOG((1, "MPI_Op_free for function for reducing I/O memory stats failed"));
    return pio_err(ios, NULL, PIO_EINTERNAL, __FILE__, __LINE__,
            "Freeing MPI reduction operation for reducing I/O summary statistics failed for I/O system (iosysid=%d)",
            ios->iosysid);
  }
#else
  /* For MPI serial library, i.e., one MPI process, copy file I/O stats and component I/O stats to global I/O stats */
  assert(gio_sstats.size() == tmp_sstats.size());
  for(std::size_t i = 0 ; i < tmp_sstats.size(); i++){
    gio_sstats[i] = tmp_sstats[i];
  }
#endif

  /* Retrieve the global I/O performance statistics for the I/O system */
  PIO_Util::IO_Summary_Util::IO_summary_stats_t iosys_gio_stats = gio_sstats.back();
  gio_sstats.pop_back();

  ierr = cache_or_print_stats(ios, ROOT_PROC, iosys_gio_stats, filenames, gio_sstats);
  if(ierr != PIO_NOERR){
    return pio_err(ios, NULL, PIO_EINTERNAL, __FILE__, __LINE__,
            "Caching/printing I/O statistics failed (iosysid=%d)", ios->iosysid);
  }

  return PIO_NOERR;
}

/* Write File I/O performance summary
 * This function is called when a file is closed. The I/O performance statistics collected here is cached
 * until the associated I/O system is finalized.
 */
int spio_write_file_io_summary(file_desc_t *file)
{
  const std::vector<std::string> wr_timers = {
    file->io_fstats->wr_timer_name
  };

  /* FIXME: We need better timers to distinguish between
   * read/write modes while opening a file
   */
  const std::vector<std::string> rd_timers = {
    file->io_fstats->rd_timer_name
  };

  const std::vector<std::string> tot_timers = {
    file->io_fstats->tot_timer_name
  };

#ifndef SPIO_IO_STATS
  return PIO_NOERR;
#endif

  assert(file);

  /* Get timings for this file from the timers */
  double total_wr_time = 0, total_rd_time = 0,  total_tot_time = 0;
  for(std::vector<std::string>::const_iterator citer = wr_timers.cbegin();
        citer != wr_timers.cend(); ++citer){
    total_wr_time += spio_ltimer_get_wtime((*citer).c_str());
  }

  for(std::vector<std::string>::const_iterator citer = rd_timers.cbegin();
        citer != rd_timers.cend(); ++citer){
    total_rd_time += spio_ltimer_get_wtime((*citer).c_str());
  }

  for(std::vector<std::string>::const_iterator citer = tot_timers.cbegin();
        citer != tot_timers.cend(); ++citer){
    total_tot_time += spio_ltimer_get_wtime((*citer).c_str());
  }

  LOG((1, "Total read time = %f s, write time = %f s, total time = %f s", total_rd_time, total_wr_time, total_tot_time));
  LOG((1, "Total bytes read = %lld, bytes written = %lld",
        (unsigned long long) file->io_fstats->rb, (unsigned long long) file->io_fstats->wb));

  PIO_Util::IO_Summary_Util::IO_summary_stats_t io_sstats;
  PIO_Util::IO_Summary_Util::IO_summary_stats2mpi io_sstats2mpi;

  io_sstats.rb_total = file->io_fstats->rb;
  io_sstats.rb_min = io_sstats.rb_total;
  io_sstats.rb_max = io_sstats.rb_total;
  io_sstats.wb_total = file->io_fstats->wb;
  io_sstats.wb_min = io_sstats.wb_total;
  io_sstats.wb_max = io_sstats.wb_total;

  io_sstats.rtime_min = total_rd_time;
  io_sstats.rtime_max = total_rd_time;
  io_sstats.wtime_min = total_wr_time;
  io_sstats.wtime_max = total_wr_time;
  io_sstats.ttime_min = total_tot_time;
  io_sstats.ttime_max = total_tot_time;

  LOG((1, "File I/O stats sent :\n%s",
        PIO_Util::IO_Summary_Util::io_summary_stats2str(io_sstats).c_str()));

  assert(file->iosystem);
  /* Cache the file I/O statistics, until the I/O system is finalized */
  cache_file_stats(file->iosystem->iosysid, file->fname, io_sstats);

  return PIO_NOERR;
}
