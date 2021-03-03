#include <iostream>
#include <string>
#include <map>
#include <cassert>

extern "C"{
#include "pio_config.h"
#include "pio.h"
#include "pio_internal.h"
#include "spio_ltimer.h"
}
#include "spio_ltimer.hpp"

/* Global timer cache */
static std::map<std::string, PIO_Util::SPIO_Ltimer_Utils::SPIO_ltimer> gtimers;

/* Timer class function definitions */
void PIO_Util::SPIO_Ltimer_Utils::SPIO_ltimer::start(void )
{
  if(lvl_ == 0){
    start_ = MPI_Wtime();
  }
  lvl_++;
}

void PIO_Util::SPIO_Ltimer_Utils::SPIO_ltimer::stop(void )
{
  assert(lvl_ > 0);

  lvl_--;
  if(lvl_ != 0){
    /* Don't record time until we reach the last start+stop call */
    return;
  }

  wtime_ += MPI_Wtime() - start_;
  start_ = 0.0;
}

double PIO_Util::SPIO_Ltimer_Utils::SPIO_ltimer::get_wtime(void ) const
{
  assert(lvl_ == 0);

  return wtime_;
}

/* Timer APIs */
void spio_ltimer_start(const char *timer_name)
{
  gtimers[timer_name].start();
}

void spio_ltimer_stop(const char *timer_name)
{
  gtimers[timer_name].stop();
}

double spio_ltimer_get_wtime(const char *timer_name)
{
  /* Note : If the timer is not present we return 0 */
  return gtimers[timer_name].get_wtime();
}
