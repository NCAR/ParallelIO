#ifndef __SPIO_LTIMER_HPP__
#define __SPIO_LTIMER_HPP__

namespace PIO_Util{
  namespace SPIO_Ltimer_Utils{
    class SPIO_ltimer{
      public:
        SPIO_ltimer() : wtime_(0.0), lvl_(0) { start_ = MPI_Wtime(); }
        void start(void );
        void stop(void );
        double get_wtime(void ) const;
      private:
        double start_;
        double wtime_;
        int lvl_;
    };
  } // namespace SPIO_Ltimer_Utils
} // namespace PIO_Util


#endif /* __SPIO_LTIMER_HPP__ */
