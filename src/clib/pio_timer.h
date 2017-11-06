#ifndef _PIO_TIMER_H_
#define _PIO_TIMER_H_

#include "config.h"
#include <stdbool.h>
#include "mpi.h"
#include "pio_internal.h"

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
    char name[PIO_MAX_NAME];
    char log_fname[PIO_MAX_NAME];

    double start_time;
    double stop_time;
    double total_time;
    MPI_Comm comm;

    mtimer_state_t state;
    bool is_async_event_in_progress;
} *mtimer_t;

typedef enum mtimer_type {
    PIO_MICRO_MPI_WTIME_ROOT = 0,
    PIO_MICRO_NUM_TIMERS
} mtimer_type_t;

/* FIXME: Do we need to return error codes here - or just have asserts */
int mtimer_init(mtimer_type_t type);
mtimer_t mtimer_create(const char *name, MPI_Comm comm, char *log_fname);
int mtimer_start(mtimer_t mt);
int mtimer_stop(mtimer_t mt, const char *log_msg);
int mtimer_destroy(mtimer_t *pmt);
int mtimer_finalize(void );
int mtimer_pause(mtimer_t mt, bool *was_running);
int mtimer_resume(mtimer_t mt);
int mtimer_reset(mtimer_t mt);

bool mtimer_is_valid(mtimer_t mt);
int mtimer_get_wtime(mtimer_t mt, double *wtime);
int mtimer_update(mtimer_t mt, double time);
int mtimer_flush(mtimer_t mt, const char *log_msg);
int mtimer_async_event_in_progress(mtimer_t mt, bool is_async_event_in_progress);
bool mtimer_has_async_event_in_progress(mtimer_t mt);
#endif
