#ifndef _PIO_TIMER_H_
#define _PIO_TIMER_H_

#include "config.h"
#include <stdbool.h>
#include "mpi.h"
#include "pio_internal.h"

/* Timer states */
typedef enum mtimer_state{
  PIO_MICRO_TIMER_INVALID=0,
  PIO_MICRO_TIMER_INIT,
  PIO_MICRO_TIMER_RUNNING,
  PIO_MICRO_TIMER_PAUSED,
  PIO_MICRO_TIMER_STOPPED
} mtimer_state_t;

/* Micro timer info */
typedef struct mtimer_info
{
    /* Timer name */
    char name[PIO_MAX_NAME];
    /* Time log file name */
    char log_fname[PIO_MAX_NAME];

    /* Start/stop/total times recorded */
    double start_time;
    double stop_time;
    double total_time;
    /* Comm that this timer operates on */
    MPI_Comm comm;

    /* State of the timer */
    mtimer_state_t state;
    /* True if the event being timed has an async
     * event pending. False otherwise
     */
    bool is_async_event_in_progress;
} *mtimer_t;

/* Timer types available,
 * PIO_MICRO_MPI_WTIME_ROOT - Uses MPI_Wtime() to measure time,
 *  outputs timer logs from the root MPI process
 */
typedef enum mtimer_type {
    PIO_MICRO_MPI_WTIME_ROOT = 0,
    PIO_MICRO_NUM_TIMERS
} mtimer_type_t;

/* FIXME: Do we need to return error codes here - or just have asserts */
/* Init timer framework - needs to be called once before using timers */
int mtimer_init(mtimer_type_t type);
/* Create/Start/Stop/Destroy a timer */
mtimer_t mtimer_create(const char *name, MPI_Comm comm, char *log_fname);
int mtimer_start(mtimer_t mt);
int mtimer_stop(mtimer_t mt, const char *log_msg);
int mtimer_destroy(mtimer_t *pmt);
/* Finalize the timer framework */
int mtimer_finalize(void );
/* Pause/resume/reset a timer */
int mtimer_pause(mtimer_t mt, bool *was_running);
int mtimer_resume(mtimer_t mt);
int mtimer_reset(mtimer_t mt);

/* Returns true if a timer is valid, false otherwise */
bool mtimer_is_valid(mtimer_t mt);
/* Get wallclock time from a timer */
int mtimer_get_wtime(mtimer_t mt, double *wtime);
/* Manually update (add) a timer with user-specified time */
int mtimer_update(mtimer_t mt, double time);
/* Manually flush a timer - write timing info to logs */
int mtimer_flush(mtimer_t mt, const char *log_msg);
/* Specify if an async event is pending on the event being timed */
int mtimer_async_event_in_progress(mtimer_t mt, bool is_async_event_in_progress);
/* Returns true if an async event is pending on the event being timed,
 * returns false otherwise
 */
bool mtimer_has_async_event_in_progress(mtimer_t mt);
#endif
