#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cassert>

extern "C"{
#include "pio_config.h"
#include "pio.h"
#include "pio_tests.h"
#include "spio_ltimer.h"
}
#include "spio_ltimer.hpp"

#define LOG_RANK0(rank, ...)                     \
            do{                                   \
                if(rank == 0)                     \
                {                                 \
                    fprintf(stderr, __VA_ARGS__); \
                }                                 \
            }while(0);

static const int FAIL = -1;

/* Test starting and stopping the SPIO ltimer */
int test_spio_ltimer(int wrank)
{
  const int SLEEP_TIME_IN_SECS = 1;
  PIO_Util::SPIO_Ltimer_Utils::SPIO_ltimer timer;

  if(timer.get_wtime() != 0){
    LOG_RANK0(wrank, "test_spio_ltimer() failed, a new timer has non-zero wallclock time\n");
    return PIO_EINTERNAL;
  }

  timer.start();
  std::this_thread::sleep_for(std::chrono::seconds(SLEEP_TIME_IN_SECS));
  timer.stop();

  double etime = timer.get_wtime();
  if(etime <= 0.0){
    LOG_RANK0(wrank,
      "test_spio_ltimer() failed. The timer did not record a valid wallclock time (%f s)\n",
      etime);
    return PIO_EINTERNAL;
  }

  return PIO_NOERR;
}

/* Test a recursive timer */
int test_spio_recursive_ltimer(int wrank)
{
  const int SLEEP_TIME_IN_SECS = 1;
  const int MAX_RECURSIVE_DEPTH = 3;
  static int cur_depth = 0;
  static PIO_Util::SPIO_Ltimer_Utils::SPIO_ltimer timer;
  int ret = PIO_NOERR;

  if(cur_depth > MAX_RECURSIVE_DEPTH){
    return PIO_NOERR;
  }

  timer.start();
  std::this_thread::sleep_for(std::chrono::seconds(SLEEP_TIME_IN_SECS));
  cur_depth++;
  ret = test_spio_recursive_ltimer(wrank);
  cur_depth--;
  if(ret != PIO_NOERR){
    return ret;
  }
  timer.stop();

  if(cur_depth == 0){
    double etime = timer.get_wtime();
    if(etime <= 0.0){
      LOG_RANK0(wrank,
        "test_spio_recursive_ltimer() failed. The timer did not record a valid wallclock time (%f s)\n",
        etime);
      return PIO_EINTERNAL;
    }
  }

  return PIO_NOERR;
}

/* Test timer APIs */
int test_timer_api(int wrank)
{
  const int SLEEP_TIME_IN_SECS = 1;
  const std::string tname("test_timer1");

  spio_ltimer_start(tname.c_str());
  std::this_thread::sleep_for(std::chrono::seconds(SLEEP_TIME_IN_SECS));
  spio_ltimer_stop(tname.c_str());

  double etime = spio_ltimer_get_wtime(tname.c_str());
  if(etime <= 0.0){
    LOG_RANK0(wrank,
      "test_timer_api() failed. The timer did not record a valid wallclock time (%f s)\n",
      etime);
    return PIO_EINTERNAL;
  }

  return PIO_NOERR;
}

/* Test timer APIs with multiple timers */
int test_many_timers_api(int wrank)
{
  const int SLEEP_TIME_IN_SECS = 1;
  const std::string tname1("test_timer1");
  const std::string tname2("test_timer2");
  const std::string tname3("test_timer3");

  spio_ltimer_start(tname1.c_str());
  spio_ltimer_start(tname3.c_str());
  spio_ltimer_start(tname2.c_str());
  std::this_thread::sleep_for(std::chrono::seconds(SLEEP_TIME_IN_SECS));
  spio_ltimer_stop(tname1.c_str());
  spio_ltimer_stop(tname2.c_str());
  spio_ltimer_stop(tname3.c_str());

  double etime1 = spio_ltimer_get_wtime(tname1.c_str());
  if(etime1 <= 0.0){
    LOG_RANK0(wrank,
      "test_many_timers_api() failed. The timer did not record a valid wallclock time (%f s)\n",
      etime1);
    return PIO_EINTERNAL;
  }

  double etime2 = spio_ltimer_get_wtime(tname2.c_str());
  if(etime2 <= 0.0){
    LOG_RANK0(wrank,
      "test_many_timers_api() failed. The timer did not record a valid wallclock time (%f s)\n",
      etime2);
    return PIO_EINTERNAL;
  }

  double etime3 = spio_ltimer_get_wtime(tname3.c_str());
  if(etime3 <= 0.0){
    LOG_RANK0(wrank,
      "test_many_timers_api() failed. The timer did not record a valid wallclock time (%f s)\n",
      etime3);
    return PIO_EINTERNAL;
  }

  return PIO_NOERR;
}

/* Test many recursive timers */
int test_many_recursive_timers_api(int wrank)
{
  const int SLEEP_TIME_IN_SECS = 1;
  const int MAX_RECURSIVE_DEPTH = 3;
  static int cur_depth = 0;
  const std::string tname1("test_timer1");
  const std::string tname2("test_timer2");
  const std::string tname3("test_timer3");
  int ret = PIO_NOERR;

  if(cur_depth > MAX_RECURSIVE_DEPTH){
    return PIO_NOERR;
  }

  spio_ltimer_start(tname1.c_str());
  spio_ltimer_start(tname3.c_str());
  spio_ltimer_start(tname2.c_str());
  std::this_thread::sleep_for(std::chrono::seconds(SLEEP_TIME_IN_SECS));
  cur_depth++;
  ret = test_many_recursive_timers_api(wrank);
  cur_depth--;
  if(ret != PIO_NOERR){
    return ret;
  }
  spio_ltimer_stop(tname1.c_str());
  spio_ltimer_stop(tname2.c_str());
  spio_ltimer_stop(tname3.c_str());

  if(cur_depth == 0){
    double etime1 = spio_ltimer_get_wtime(tname1.c_str());
    if(etime1 <= 0.0){
      LOG_RANK0(wrank,
        "test_many_recursive_timers_api() failed. The timer did not record a valid wallclock time (%f s)\n",
        etime1);
      return PIO_EINTERNAL;
    }

    double etime2 = spio_ltimer_get_wtime(tname2.c_str());
    if(etime2 <= 0.0){
      LOG_RANK0(wrank,
        "test_many_recursive_timers_api() failed. The timer did not record a valid wallclock time (%f s)\n",
        etime2);
      return PIO_EINTERNAL;
    }

    double etime3 = spio_ltimer_get_wtime(tname3.c_str());
    if(etime3 <= 0.0){
      LOG_RANK0(wrank,
        "test_many_recursive_timers_api() failed. The timer did not record a valid wallclock time (%f s)\n",
        etime3);
      return PIO_EINTERNAL;
    }
  }

  return PIO_NOERR;
}

int test_driver(MPI_Comm comm, int wrank, int wsz, int *num_errors)
{
  int nerrs = 0, ret = PIO_NOERR;
  assert((comm != MPI_COMM_NULL) && (wrank >= 0) && (wsz > 0) && num_errors);
  
  /* Test the timer class */
  try{
    ret = test_spio_ltimer(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_spio_ltimer() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_spio_ltimer() PASSED\n");
  }

  /* Test a recursive timer */
  try{
    ret = test_spio_recursive_ltimer(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_spio_recursive_ltimer() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_spio_recursive_ltimer() PASSED\n");
  }

  /* Test timer API */
  try{
    ret = test_timer_api(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_timer_api() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_timer_api() PASSED\n");
  }

  /* Test many timers using the timer API */
  try{
    ret = test_many_timers_api(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_many_timers_api() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_many_timers_api() PASSED\n");
  }

  /* Test many recursive timers using the timer API */
  try{
    ret = test_many_recursive_timers_api(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_many_recursive_timers_api() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_many_recursive_timers_api() PASSED\n");
  }

  *num_errors += nerrs;
  return nerrs;
}

int main(int argc, char *argv[])
{
  int ret;
  int wrank, wsz;
  int num_errors;
#ifdef TIMING
#ifndef TIMING_INTERNAL
  ret = GPTLinitialize();
  if(ret != 0){
    LOG_RANK0(wrank, "GPTLinitialize() FAILED, ret = %d\n", ret);
    return ret;
  }
#endif /* TIMING_INTERNAL */
#endif /* TIMING */

  ret = MPI_Init(&argc, &argv);
  if(ret != MPI_SUCCESS){
    LOG_RANK0(wrank, "MPI_Init() FAILED, ret = %d\n", ret);
    return ret;
  }

  ret = MPI_Comm_rank(MPI_COMM_WORLD, &wrank);
  if(ret != MPI_SUCCESS){
    LOG_RANK0(wrank, "MPI_Comm_rank() FAILED, ret = %d\n", ret);
    return ret;
  }
  ret = MPI_Comm_size(MPI_COMM_WORLD, &wsz);
  if(ret != MPI_SUCCESS){
    LOG_RANK0(wrank, "MPI_Comm_rank() FAILED, ret = %d\n", ret);
    return ret;
  }

  num_errors = 0;
  ret = test_driver(MPI_COMM_WORLD, wrank, wsz, &num_errors);
  if(ret != 0){
    LOG_RANK0(wrank, "Test driver FAILED\n");
    return FAIL;
  }
  else{
    LOG_RANK0(wrank, "All tests PASSED\n");
  }

  MPI_Finalize();

#ifdef TIMING
#ifndef TIMING_INTERNAL
  ret = GPTLfinalize();
  if(ret != 0){
    LOG_RANK0(wrank, "GPTLinitialize() FAILED, ret = %d\n", ret);
    return ret;
  }
#endif /* TIMING_INTERNAL */
#endif /* TIMING */

  if(num_errors != 0){
    LOG_RANK0(wrank, "Total errors = %d\n", num_errors);
    return FAIL;
  }
  return 0;
}
