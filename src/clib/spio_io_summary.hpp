#ifndef __SPIO_IO_SUMMARY_HPP__
#define __SPIO_IO_SUMMARY_HPP__

namespace PIO_Util{

namespace IO_Summary_Util{
/*
 * Structure used to calculate the I/O performance
 * global statistics (across all processes)
 *
 * This is a C struct since we need to communicate
 * this information using MPI C APIs
 */
typedef struct IO_summary_stats{
  /* Total/min/max read/write bytes */
  PIO_Offset rb_total;
  PIO_Offset rb_min;
  PIO_Offset rb_max;
  PIO_Offset wb_total;
  PIO_Offset wb_min;
  PIO_Offset wb_max;

  /* min/max read/write/total times */
  double rtime_min;
  double rtime_max;
  double wtime_min;
  double wtime_max;
  double ttime_min;
  double ttime_max;
} IO_summary_stats_t;

/* Util to convert I/O summary stats struct to a string */
std::string io_summary_stats2str(const IO_summary_stats_t &io_sstats);
/* Util to convert bytes into a human readable format, e.g. 1.2 KB */
std::string bytes2hr(PIO_Offset nb);

/* Class to convert the IO_summary_stats struct above to an MPI
 * datatype
 */
class IO_summary_stats2mpi{
  public:
    IO_summary_stats2mpi();
    MPI_Datatype get_mpi_datatype(void ) const;
    ~IO_summary_stats2mpi();
  private:
    static const int NUM_IO_SUMMARY_STATS_MEMBERS = 12;
    void get_io_summary_stats_address_disps(
      std::array<MPI_Aint, NUM_IO_SUMMARY_STATS_MEMBERS> &disps) const;
    MPI_Datatype dt_;
};

} // namespace IO_Summary_Util

} // namespace PIO_Util

#endif /* __SPIO_IO_SUMMARY_HPP__ */

