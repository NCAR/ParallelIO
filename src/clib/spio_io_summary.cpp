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
#include <ctime>

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
      /* FIXME: Since iosystem and other structs are currently used
       * in C we cannot store this cache there. We need to get rid of this global cache.
       * std::map<IOID, std::map<FILENAME, FILE_SSTATS> >
       */
      std::map<int, std::map<std::string, IO_summary_stats_t> > file_sstats_cache;
    }
  } // namespace IO_Summary_Util
} // namespace PIO_Util

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
      sstats.rb_min = spio_min(sstats.rb_min, file_sstats.rb_min);
      sstats.rb_max = spio_max(sstats.rb_max, file_sstats.rb_max);
      sstats.wb_total += file_sstats.wb_total;
      sstats.wb_min = spio_min(sstats.wb_min, file_sstats.wb_min);
      sstats.wb_max = spio_max(sstats.wb_max, file_sstats.wb_max);
      sstats.rtime_min = spio_min(sstats.rtime_min, file_sstats.rtime_min);
      sstats.rtime_max = spio_max(sstats.rtime_max, file_sstats.rtime_max);
      sstats.wtime_min = spio_min(sstats.wtime_min, file_sstats.wtime_min);
      sstats.wtime_max = spio_max(sstats.wtime_max, file_sstats.wtime_max);
    }
  }
  else{
    /* First file stats being cached for iosysid */
    std::map<std::string, PIO_Util::IO_Summary_Util::IO_summary_stats_t> file_sstats_map;
    file_sstats_map[filename] = file_sstats;
    PIO_Util::IO_Summary_Util::GVars::file_sstats_cache[iosysid] = file_sstats_map;
  }
}

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

  return ostr.str();
}

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

PIO_Util::IO_Summary_Util::IO_summary_stats2mpi::IO_summary_stats2mpi() : dt_(MPI_DATATYPE_NULL)
{
  int mpi_errno = MPI_SUCCESS;
  /* Corresponds to defn of IO_summary_stats */
  std::array<MPI_Datatype, NUM_IO_SUMMARY_STATS_MEMBERS> types = {MPI_OFFSET, MPI_OFFSET,
    MPI_OFFSET, MPI_OFFSET, MPI_OFFSET, MPI_OFFSET, MPI_DOUBLE, MPI_DOUBLE,
    MPI_DOUBLE, MPI_DOUBLE};

  std::array<MPI_Aint, NUM_IO_SUMMARY_STATS_MEMBERS> disps;
  std::array<int, NUM_IO_SUMMARY_STATS_MEMBERS> blocklens = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

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

  assert(disps.size() > 0);
  MPI_Aint base_addr = disps[0];
  for(std::size_t i = 0; i < disps.size(); i++){
    disps[i] -= base_addr;
  }
}

extern "C"{
void red_io_summary_stats(PIO_Util::IO_Summary_Util::IO_summary_stats_t *in_arr,
  PIO_Util::IO_Summary_Util::IO_summary_stats_t *inout_arr,
  int *nelems, MPI_Datatype *pdt);
} /* extern "C" */

void red_io_summary_stats(PIO_Util::IO_Summary_Util::IO_summary_stats_t *in_arr,
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
  }
}

static int cache_or_print_stats(iosystem_desc_t *ios, int root_proc,
  PIO_Util::IO_Summary_Util::IO_summary_stats_t &iosys_gio_sstats,
  std::vector<std::string> &file_names,
  std::vector<PIO_Util::IO_Summary_Util::IO_summary_stats_t> &file_gio_sstats)
{
  static PIO_Util::IO_Summary_Util::IO_summary_stats_t cached_overall_gio_sstats;
  static std::vector<PIO_Util::IO_Summary_Util::IO_summary_stats_t> cached_ios_gio_sstats;
  static std::vector<std::string> cached_ios_names;
  static std::vector<std::vector<PIO_Util::IO_Summary_Util::IO_summary_stats_t> >
          cached_file_gio_sstats;
  static std::vector<std::vector<std::string> > cached_file_names;
  int niosys = 0;
  int ierr;

  assert(ios);

  /* Cache only in the root process */
  if(ios->union_rank == root_proc){
    cached_overall_gio_sstats.rb_total += iosys_gio_sstats.rb_total;
    cached_overall_gio_sstats.rb_min = spio_min(cached_overall_gio_sstats.rb_min, iosys_gio_sstats.rb_min);
    cached_overall_gio_sstats.rb_max = spio_max(cached_overall_gio_sstats.rb_max, iosys_gio_sstats.rb_max);
    cached_overall_gio_sstats.wb_total += iosys_gio_sstats.wb_total;
    cached_overall_gio_sstats.wb_min = spio_min(cached_overall_gio_sstats.wb_min, iosys_gio_sstats.wb_min);
    cached_overall_gio_sstats.wb_max = spio_max(cached_overall_gio_sstats.wb_max, iosys_gio_sstats.wb_max);
    cached_overall_gio_sstats.rtime_min = spio_min(cached_overall_gio_sstats.rtime_min, iosys_gio_sstats.rtime_min);
    cached_overall_gio_sstats.rtime_max = spio_max(cached_overall_gio_sstats.rtime_max, iosys_gio_sstats.rtime_max);
    cached_overall_gio_sstats.wtime_min = spio_min(cached_overall_gio_sstats.wtime_min, iosys_gio_sstats.wtime_min);
    cached_overall_gio_sstats.wtime_max = spio_max(cached_overall_gio_sstats.wtime_max, iosys_gio_sstats.wtime_max);

    cached_ios_gio_sstats.push_back(iosys_gio_sstats);
    cached_ios_names.push_back(ios->sname);
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
  if(niosys == 1){
    const std::string model_name = "Scorpio";
    const std::size_t ONE_MB = 1024 * 1024;
    std::srand(std::time(nullptr));
    std::string sfname = std::string("io_perf_summary_") + std::to_string(std::rand());
    const std::string sfname_txt_suffix(".txt");
    const std::string sfname_json_suffix(".json");

    assert(cached_ios_gio_sstats.size() == cached_ios_names.size());
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
    PIO_Util::Serializer_Utils::serialize_pack("avg_wtput",
      (cached_overall_gio_sstats.wtime_max > 0.0) ?
      (cached_overall_gio_sstats.wb_total / (ONE_MB * cached_overall_gio_sstats.wtime_max)) : 0.0,
      overall_comp_vals);

    PIO_Util::Serializer_Utils::serialize_pack("avg_rtput",
      (cached_overall_gio_sstats.rtime_max > 0.0) ?
      (cached_overall_gio_sstats.rb_total / (ONE_MB * cached_overall_gio_sstats.rtime_max)) : 0.0,
      overall_comp_vals);

    PIO_Util::Serializer_Utils::serialize_pack("tot_wb",
      cached_overall_gio_sstats.wb_total, overall_comp_vals);

    PIO_Util::Serializer_Utils::serialize_pack("tot_rb",
      cached_overall_gio_sstats.rb_total, overall_comp_vals);

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
      PIO_Util::Serializer_Utils::serialize_pack("avg_wtput",
        (cached_ios_gio_sstats[i].wtime_max > 0.0) ?
        (cached_ios_gio_sstats[i].wb_total / (ONE_MB * cached_ios_gio_sstats[i].wtime_max)) : 0.0,
        comp_vals);

      PIO_Util::Serializer_Utils::serialize_pack("avg_rtput",
        (cached_ios_gio_sstats[i].rtime_max > 0.0) ?
        (cached_ios_gio_sstats[i].rb_total / (ONE_MB * cached_ios_gio_sstats[i].rtime_max)) : 0.0,
        comp_vals);

      PIO_Util::Serializer_Utils::serialize_pack("tot_wb",
        cached_ios_gio_sstats[i].wb_total, comp_vals);

      PIO_Util::Serializer_Utils::serialize_pack("tot_rb",
        cached_ios_gio_sstats[i].rb_total, comp_vals);

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
        PIO_Util::Serializer_Utils::serialize_pack("avg_wtput",
          (cached_file_gio_sstats[i][j].wtime_max > 0.0) ?
          (cached_file_gio_sstats[i][j].wb_total / (ONE_MB * cached_file_gio_sstats[i][j].wtime_max)) : 0.0,
          file_vals);

        PIO_Util::Serializer_Utils::serialize_pack("avg_rtput",
          (cached_file_gio_sstats[i][j].rtime_max > 0.0) ?
          (cached_file_gio_sstats[i][j].rb_total / (ONE_MB * cached_file_gio_sstats[i][j].rtime_max)) : 0.0,
          file_vals);

        PIO_Util::Serializer_Utils::serialize_pack("tot_wb",
          cached_file_gio_sstats[i][j].wb_total, file_vals);

        PIO_Util::Serializer_Utils::serialize_pack("tot_rb",
          cached_file_gio_sstats[i][j].rb_total, file_vals);

        file_vvals.push_back(file_vals);
      }
    }
    spio_ser->serialize(id, "FileIOStatistics", file_vvals, file_vvals_ids);
    spio_json_ser->serialize(json_id, "FileIOStatistics", file_vvals, json_file_vvals_ids);
    spio_ser->sync();
    spio_json_ser->sync();
  }

  return PIO_NOERR;
}

/* Write I/O performance summary */
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

#ifndef TIMING
  return PIO_NOERR;
#endif

  assert(ios);

  /* For async I/O only collect statistics from the I/O processes */
  if(ios->async && !(ios->ioproc)){
    return PIO_NOERR;
  }

  const int THREAD_ID = 0;
  double total_wr_time = 0, total_rd_time = 0;
  for(std::vector<std::string>::const_iterator citer = wr_timers.cbegin();
        citer != wr_timers.cend(); ++citer){
    double wtime = 0;
    ierr = GPTLget_wallclock((*citer).c_str(), THREAD_ID, &wtime);
    if(ierr == 0){
      total_wr_time += wtime;
    }
    else{
      LOG((1,"Unable to get timer value for timer (%s)", (*citer).c_str()));
    }
  }

  for(std::vector<std::string>::const_iterator citer = rd_timers.cbegin();
        citer != rd_timers.cend(); ++citer){
    double wtime = 0;
    ierr = GPTLget_wallclock((*citer).c_str(), THREAD_ID, &wtime);
    if(ierr == 0){
      total_rd_time += wtime;
    }
    else{
      LOG((1, "Unable to get timer value for timer (%s)", (*citer).c_str()));
    }
  }

  /*
  LOG((1, "Total read time = %f s, write time = %f s", total_rd_time, total_wr_time));
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

  /*
  LOG((1, "I/O stats sent :\n%s",
        PIO_Util::IO_Summary_Util::io_summary_stats2str(io_sstats).c_str()));
  */

  /* Get the I/O statistics of files belonging to this I/O system */
  std::vector<std::string> filenames;
  std::vector<PIO_Util::IO_Summary_Util::IO_summary_stats_t> tmp_sstats;

  get_file_stats(ios->iosysid, filenames, tmp_sstats);

  MPI_Op op;
  ierr = MPI_Op_create((MPI_User_function *)red_io_summary_stats, true, &op);
  if(ierr != PIO_NOERR){
    LOG((1, "MPI_Op_create for reducing I/O memory stats failed"));
    return pio_err(ios, NULL, PIO_EINTERNAL, __FILE__, __LINE__,
            "Creating MPI reduction operation for reducing I/O summary statistics failed for I/O system (iosysid=%d)",
            ios->iosysid);
  }

  //PIO_Util::IO_Summary_Util::IO_summary_stats_t gio_sstats = {0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0};
  /* Add the component I/O stats to the end of file I/O stats*/
  const std::size_t nelems_to_red = filenames.size() + 1;
  std::vector<PIO_Util::IO_Summary_Util::IO_summary_stats_t> gio_sstats(nelems_to_red);

  tmp_sstats.push_back(io_sstats);

  const int ROOT_PROC = 0;
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

  PIO_Util::IO_Summary_Util::IO_summary_stats_t iosys_gio_stats = gio_sstats.back();
  gio_sstats.pop_back();

  ierr = cache_or_print_stats(ios, ROOT_PROC, iosys_gio_stats, filenames, gio_sstats);
  if(ierr != PIO_NOERR){
    return pio_err(ios, NULL, PIO_EINTERNAL, __FILE__, __LINE__,
            "Caching/printing I/O statistics failed (iosysid=%d)", ios->iosysid);
  }

  return PIO_NOERR;
}

/* Write File I/O performance summary */
int spio_write_file_io_summary(file_desc_t *file)
{
  int ierr = 0;
  const std::vector<std::string> wr_timers = {
    file->io_fstats->wr_timer_name
  };

  /* FIXME: We need better timers to distinguish between
   * read/write modes while opening a file
   */
  const std::vector<std::string> rd_timers = {
    file->io_fstats->rd_timer_name
  };

#ifndef TIMING
  return PIO_NOERR;
#endif

  assert(file);

  const int THREAD_ID = 0;
  double total_wr_time = 0, total_rd_time = 0;
  for(std::vector<std::string>::const_iterator citer = wr_timers.cbegin();
        citer != wr_timers.cend(); ++citer){
    double wtime = 0;
    ierr = GPTLget_wallclock((*citer).c_str(), THREAD_ID, &wtime);
    if(ierr == 0){
      total_wr_time += wtime;
    }
    else{
      LOG((1,"Unable to get timer value for timer (%s)", (*citer).c_str()));
    }
  }

  for(std::vector<std::string>::const_iterator citer = rd_timers.cbegin();
        citer != rd_timers.cend(); ++citer){
    double wtime = 0;
    ierr = GPTLget_wallclock((*citer).c_str(), THREAD_ID, &wtime);
    if(ierr == 0){
      total_rd_time += wtime;
    }
    else{
      LOG((1, "Unable to get timer value for timer (%s)", (*citer).c_str()));
    }
  }

  LOG((1, "Total read time = %f s, write time = %f s", total_rd_time, total_wr_time));
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

  LOG((1, "File I/O stats sent :\n%s",
        PIO_Util::IO_Summary_Util::io_summary_stats2str(io_sstats).c_str()));

  assert(file->iosystem);
  cache_file_stats(file->iosystem->iosysid, file->fname, io_sstats);

  return PIO_NOERR;
}
