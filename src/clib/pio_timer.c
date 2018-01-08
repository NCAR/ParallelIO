#include "config.h"
#include "pio_timer.h"
#include "pio_internal.h"

/* This structure stores information on a timer type
 * init -> The init function for the timer
 * finalize -> The finalize function for the timer
 * get_wtime -> Function to retrieve wallclock time from the timer
 * is_initialized -> Indicates if the timer type is initialized
 */
typedef struct internal_timer{
    int (*init) (void );
    int (*finalize) (void );
    double (*get_wtime) (void );
    bool is_initialized;
} internal_timer_t;

/* Note: entries here correspond to mtimer_type_t */
/* Array of available timer types - init'ed in mtimer_init() */
static internal_timer_t internal_timers[PIO_MICRO_NUM_TIMERS];
/* Number of timers created by the user */
static int pio_ntimers = 0;
/* Timer type chosen by the user - init'ed in mtimer_init() */
static mtimer_type_t pio_timer_type;

/* Flush timer on the root process */
static int mtimer_flush_root(mtimer_t mt, const char *log_msg, MPI_Comm comm)
{
    int rank;
    FILE *fp = NULL;

    assert(mt != NULL);
    MPI_Comm_rank(comm, &rank);
    /* We open and close the file for every flush because multiple
      *timers can potentially flush to the same log file
      */
    if(rank == 0)
    {
        fp = fopen(mt->log_fname, "a+");
        if(fp == NULL)
        {
            LOG((3, "ERROR: Opening the log file, %s, failed", mt->log_fname));
            return PIO_EINTERNAL;
        }
        fprintf(fp, "%s\n", log_msg);
        fclose(fp);
    }

    return PIO_NOERR;
}

/* Initialize the micro timing framework
 * type : Type of the timer (all timers used will be of this type)
 */
int mtimer_init(mtimer_type_t type)
{
    /* Note: entries in internal_timers correspond to mtimer_type_t */
    assert(type < PIO_MICRO_NUM_TIMERS);

    pio_timer_type = type;

    /* Add all supported internal timers */
    if(type == PIO_MICRO_MPI_WTIME_ROOT)
    {
        internal_timers[type] = (internal_timer_t ){ mpi_mtimer_init,
                                  mpi_mtimer_finalize,
                                  mpi_mtimer_get_wtime,
                                  true};
    }

    return PIO_NOERR;
}

/* Create a timer
 * name : Name of the timer
 * comm : MPI communicator where the timer runs
 * log_fname : File name for the timer logs
 */
mtimer_t mtimer_create(const char *name, MPI_Comm comm, char *log_fname)
{
    assert((name != NULL) && (log_fname != NULL));
    mtimer_t mt = (mtimer_t )malloc(sizeof(struct mtimer_info));
    assert(mt != NULL);
    strncpy(mt->name, name, PIO_MAX_NAME);
    strncpy(mt->log_fname, log_fname, PIO_MAX_NAME);
    mt->start_time = 0;
    mt->stop_time = 0;
    mt->total_time = 0;
    mt->comm = comm;

    mt->state = PIO_MICRO_TIMER_INIT;
    mt->is_async_event_in_progress = false;
    LOG((3, "Created timer : %s", name));

    pio_ntimers++;

    return mt;
}

/* Start a timer
 * mt - Micro timer handle
 * desc - Description of the timer
 * A start timer can be restarted multiple times.
 * When flushing the timer, the timer uses the latest desc (the
 * desc passed to the last mtimer_start() )
 */
int mtimer_start(mtimer_t mt)
{
    if(mt == NULL)
    {
        LOG((3, "ERROR: Micro timer start failed because either timer handle is invalid"));
        return PIO_EINVAL;
    }
    assert((mt->state == PIO_MICRO_TIMER_INIT) ||
            (mt->state == PIO_MICRO_TIMER_STOPPED));

    LOG((3, "Starting timer : %s", mt->name));
    mt->start_time = internal_timers[pio_timer_type].get_wtime();
    mt->state = PIO_MICRO_TIMER_RUNNING;
    return PIO_NOERR;
}

/* Stop a timer
 * mt - Micro timer handle
 * Stop a timer and flush timer info to the log file, if there
 * are no asynchronous events pending
 */
int mtimer_stop(mtimer_t mt, const char *log_msg)
{
    int ret = PIO_NOERR;
    double elapsed_time = 0;
    char tmp_log_msg[PIO_MAX_NAME];
    if(mt == NULL)
    {
        LOG((3, "ERROR: Micro timer failed to stop, the timer handle is invalid"));
        return PIO_EINVAL;
    }

    LOG((1, "Stopping timer : %s (log msg = %s, state = %d)", mt->name, log_msg, mt->state));

    assert(mt->state == PIO_MICRO_TIMER_RUNNING);
    mt->stop_time = internal_timers[pio_timer_type].get_wtime();
    elapsed_time = mt->stop_time - mt->start_time;
    mt->start_time = mt->stop_time = 0;
    if(elapsed_time < 0)
    {
        /* Internal timer clock is not monotonic */
        LOG((3, "Internal timer is not monotonic, elapsed time = %d", elapsed_time));
        elapsed_time = 0;
    }
    mt->total_time += elapsed_time;
    mt->state = PIO_MICRO_TIMER_STOPPED;

    /* Flush timer log message if no asynchronous events are pending */
    if(!mt->is_async_event_in_progress)
    {
        snprintf(tmp_log_msg, PIO_MAX_NAME, "%s %s time=%11.8f s", mt->name, (log_msg)?(log_msg):"", mt->total_time);
        if(pio_timer_type == PIO_MICRO_MPI_WTIME_ROOT)
        {
            ret = mtimer_flush_root(mt, tmp_log_msg, mt->comm);
            mt->total_time = 0;
        }
        else
        {
            LOG((3, "ERROR: The timer does not have a flush option"));
            ret = PIO_EINTERNAL;
        }
    }
    /* If asynchronous events are pending, the user needs to explicitly
      * 1) Set asynchronous events to 0 and wait for next timer the
      *     timer is stopped.
      * 2) Explicitly flush the timer after stopping/pausing a timer
      */
    return ret;
}

/* Destroy a timer
 * - A timer must be paused or stopped before destroying it.
 * - Destroying a timer does not flush the timer
 */
int mtimer_destroy(mtimer_t *pmt)
{
    if(pmt == NULL)
    {
        LOG((3, "ERROR: Destroying timer failed, pointer timer handle is invalid"));
        return PIO_EINVAL;
    }

    mtimer_t mt = *pmt;
    if(mt == NULL)
    {
        /* Don't penalize destorying already destroyed timers */
        /* LOG((3, "Trying to destroy an already destroyed timer")); */
        return PIO_NOERR;
    }

    LOG((3, "Destroying timer : %s", mt->name));
    mtimer_state_t state = mt->state;

    pio_ntimers--;
    free(mt);
    *pmt = NULL;

    if(state == PIO_MICRO_TIMER_RUNNING)
    {
        /* Timers should be stopped or paused before destroying */
        LOG((3, "ERROR: Trying to destroy a timer, that is not stopped/paused"));
        return PIO_EINTERNAL;
    }

    return PIO_NOERR;
}

/* Finalize the timer framework */
int mtimer_finalize(void )
{
    if(pio_ntimers != 0){
        LOG((3, "ERROR: Micro timer finalize failed, unflushed timers exist!"));
        return PIO_EINTERNAL;
    }
    return PIO_NOERR;
}

/* Pause a timer
 * mt - Handle to micro timer
 * pwas_running - pointer to bool that returns if the timer was running
 *  before it was paused (can be NULL)
 * A paused timer can be resumed or stopped
 * A timer is not flushed when paused (unlike stopping a timer)
 */
int mtimer_pause(mtimer_t mt, bool *pwas_running)
{
    double elapsed_time = 0;
    if(mt == NULL)
    {
        LOG((3, "ERROR: Micro timer pause failed, invalid handle"));
        return PIO_EINVAL;
    }

    LOG((3, "Pausing timer : %s", mt->name));
    if(pwas_running)
    {
        *pwas_running = (mt->state == PIO_MICRO_TIMER_RUNNING) ? true : false;
    }

    if(mt->state != PIO_MICRO_TIMER_RUNNING)
    {
        return PIO_NOERR;
    }

    mt->stop_time = internal_timers[pio_timer_type].get_wtime();
    elapsed_time = mt->stop_time - mt->start_time;
    if(elapsed_time < 0)
    {
        /* Internal timer clock is not monotonic */
        LOG((3, "Internal timer is not monotonic, elapsed time = %d", elapsed_time));
        elapsed_time = 0;
    }
    mt->total_time += elapsed_time;
    mt->start_time = 0;
    mt->stop_time = 0;

    mt->state = PIO_MICRO_TIMER_PAUSED;

    return PIO_NOERR;
}

/* Resume a timer
 * mt - Handle to micro timer
 * Only a paused timer can be resumed, once a timer is resumed it
 * is in the running state.
 */
int mtimer_resume(mtimer_t mt)
{
    if(mt == NULL)
    {
        LOG((3, "ERROR: Resuming timer failed, invalid handle"));
        return PIO_EINVAL;
    }

    LOG((3, "Resuming timer : %s", mt->name));
    assert(mt->state == PIO_MICRO_TIMER_PAUSED);
    mt->start_time = internal_timers[pio_timer_type].get_wtime();

    mt->state = PIO_MICRO_TIMER_RUNNING;
    return PIO_NOERR;
}

/* Reset a timer
 * mt - Handle to micro timer
 * Resetting timer resets the internal clocks and sets the
 * state to INIT
 */
int mtimer_reset(mtimer_t mt)
{
    if(mt == NULL)
    {
        LOG((3, "ERROR: Resetting timer failed, invalid handle"));
        return PIO_EINVAL;
    }
    LOG((3, "Resetting timer : %s", mt->name));
    mt->start_time = 0;
    mt->stop_time = 0;
    mt->total_time = 0;

    mt->state = PIO_MICRO_TIMER_STOPPED;
    mt->is_async_event_in_progress = false;

    return PIO_NOERR;
}

/* Check the validity of a timer
 * mt - Handle to micro timer
 * Returns true if the timer is valid, false otherwise
 */
bool mtimer_is_valid(mtimer_t mt)
{
    return (mt && (mt->state != PIO_MICRO_TIMER_INVALID));
}

/* Get time elapsed from the timer
 * mt - Handle to the micro timer
 * pwtime - Pointer to buffer to get the total time elapsed
 * Returns PIO_NOERR if successful, a pio error code otherwise
 */
int mtimer_get_wtime(mtimer_t mt, double *pwtime)
{
    if((mt == NULL) || (pwtime == NULL))
    {
        LOG((3, "ERROR: Resuming timer failed, invalid handle or NULL ptr as arg"));
        return PIO_EINVAL;
    }
    /* A timer needs to be paused or stopped before getting time */
    assert(mt->state != PIO_MICRO_TIMER_RUNNING);

    *pwtime = mt->total_time;

    return PIO_NOERR;
}

/* Update a timer
 * mt - Handle to micro timer
 * time - Update the timer with the time
 * The elapsed time for the timer is updated with the time
 * (elapsed time += time)
 */
int mtimer_update(mtimer_t mt, double time)
{
    if(mt == NULL)
    {
        LOG((3, "ERROR: Updating timer failed, invalid handle"));
        return PIO_EINVAL;
    }
    LOG((3, "Updating timer : %s (+ %f s)", mt->name, time));
    mt->total_time += time;
    return PIO_NOERR;
}

/* Flush a timer
 * mt - Handle to micro timer
 * The timer is only flushed if there are no pending asynchronous
 * events
 * A running timer cannot be flushed
 */
int mtimer_flush(mtimer_t mt, const char *log_msg)
{
    int ret = PIO_NOERR;
    char tmp_log_msg[PIO_MAX_NAME];
    if(mt == NULL)
    {
        LOG((3, "ERROR: Flushing timer failed, invalid handle"));
        return PIO_EINVAL;
    }
    LOG((3, "Flusing timer : %s", mt->name));
    if((mt->state == PIO_MICRO_TIMER_INVALID) ||
        (mt->state == PIO_MICRO_TIMER_INIT) ||
        (mt->state == PIO_MICRO_TIMER_RUNNING))
    {
        return PIO_NOERR;
    }
    /* Flush if,
      * 1) There are no asynchronous events pending
      * 2) The timer is not already flushed
      */
    if(!mt->is_async_event_in_progress && (mt->total_time > 0))
    {
        snprintf(tmp_log_msg, PIO_MAX_NAME, "%s %s time=%11.8f s", mt->name, (log_msg)?(log_msg):"", mt->total_time);
        if(pio_timer_type == PIO_MICRO_MPI_WTIME_ROOT)
        {
            ret = mtimer_flush_root(mt, tmp_log_msg, mt->comm);
            mt->total_time = 0;
        }
        else
        {
            LOG((3, "ERROR: The timer does not have a flush option"));
            ret = PIO_EINTERNAL;
        }
    }
    return ret;
}

/* Set if timer has pending asynchronous events
 * mt - Handle to micro timer
 * is_async_event_in_progress - true if there are pending asynchronous events
 * on this timer, false otherwise
 * If there are asynchronous events pending on the event that this timer is
 * keeping track of, set is_async_event_in_progress to true. If there are
 * pending asynchronous events on a timer, stopping a timer does not flush it
 */
int mtimer_async_event_in_progress(mtimer_t mt, bool is_async_event_in_progress)
{
    if(mt == NULL)
    {
        LOG((3, "ERROR: Flushing timer failed, invalid handle"));
        return PIO_EINVAL;
    }
    LOG((3, "Setting num async events in timer (%s) : %s", mt->name,
          (is_async_event_in_progress)?"true":"false"));
    mt->is_async_event_in_progress = is_async_event_in_progress;
    return PIO_NOERR;
}

/* Check if there are pending asynchronous events on the timer
 * mt - Handle to the micro timer
 * Returns true if there are pending asynchronous events on the timer, false
 * otherwise
 */
bool mtimer_has_async_event_in_progress(mtimer_t mt)
{
    if(mt == NULL)
    {
        LOG((3, "ERROR: Querying an invalid timer"));
        return false;
    }
    return mt->is_async_event_in_progress;
}
