#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <array>
#include <cassert>
#include <stdexcept>
#include <algorithm>

extern "C"{
#include "pio_config.h"
#include "pio.h"
#include "pio_internal.h"
#include "spio_io_summary.h"
}

#include "spio_io_summary.hpp"

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

/* Write I/O performance summary */
int spio_write_io_summary(iosystem_desc_t *ios)
{
  int ierr = 0;
  static const std::vector<std::string> wr_timers = {
      "PIO:PIOc_createfile",
      "PIO:PIOc_put_att_tc",
      "PIO:PIOc_put_vars_tc",
      "PIO:PIOc_write_darray",
      "PIO:PIOc_sync",
      "PIO:PIOc_closefile_write_mode"
  };

  /* FIXME: We need better timers to distinguish between
   * read/write modes while opening a file
   */
  static const std::vector<std::string> rd_timers = {
      "PIO:PIOc_openfile",
      "PIO:PIOc_get_att_tc",
      "PIO:PIOc_get_vars_tc",
      "PIO:PIOc_read_darray"
  };

#ifndef TIMING
  return PIO_NOERR;
#endif

  assert(ios);

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
        (unsigned long long) ios->io_fstats->rb, (unsigned long long) ios->io_fstats->wb));

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

  LOG((1, "I/O stats sent :\n%s",
        PIO_Util::IO_Summary_Util::io_summary_stats2str(io_sstats).c_str()));

  MPI_Op op;
  ierr = MPI_Op_create((MPI_User_function *)red_io_summary_stats, true, &op);
  if(ierr != PIO_NOERR){
    LOG((1, "MPI_Op_create for reducing I/O memory stats failed"));
    return pio_err(ios, NULL, PIO_EINTERNAL, __FILE__, __LINE__,
            "Creating MPI reduction operation for reducing I/O summary statistics failed for I/O system (iosysid=%d)",
            ios->iosysid);
  }

  PIO_Util::IO_Summary_Util::IO_summary_stats_t gio_sstats = {0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0};
  const int ROOT_PROC = 0;
  ierr = MPI_Reduce(&io_sstats, &gio_sstats, 1, io_sstats2mpi.get_mpi_datatype(), op, ROOT_PROC, ios->union_comm);
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

  if(ios->union_rank == ROOT_PROC){
    LOG((1, "I/O stats recv :\n%s",
          PIO_Util::IO_Summary_Util::io_summary_stats2str(gio_sstats).c_str()));
    assert(ios->io_fstats);
    std::cout << "Average I/O write thoughput = "
      << PIO_Util::IO_Summary_Util::bytes2hr((gio_sstats.wtime_max > 0.0) ? (gio_sstats.wb_total / gio_sstats.wtime_max) : 0)
      << "/s \n";
    std::cout << "Average I/O read thoughput = "
      << PIO_Util::IO_Summary_Util::bytes2hr((gio_sstats.rtime_max > 0.0) ? (gio_sstats.rb_total / gio_sstats.rtime_max) : 0)
      << "/s \n";
  }

  return PIO_NOERR;
}

