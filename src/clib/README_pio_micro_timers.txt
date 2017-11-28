PIO Micro timers
-----------------
This document briefly describes the micro timers available in PIO. Please take a look at pio_timer.h and pio_timer.c to see the interface to these timers. This document only describes the typical usage of these timers in the PIO code.

Available timers
------------------
The following timers are available,

* PIO_MICRO_MPI_WTIME_ROOT - Micro timer that uses MPI_Wtime() to measure time and
  reports only from the root process.

Timer usage
------------
The micro timers have interfaces to,

* Initialize/finalize the timer framework
* Create/Destroy, start/stop, pause/resume, update and reset a timer
* time asynchronous events
* Add custom log messages to each timed event (for reads and writes in PIO we
  use this feature to output information about the variable)

The timing logs are written to a log file.

A simple synchronous timer
-----------------------------

mtimer_t mt;

// Initialize the timer framework
ret = mtimer_init(PIO_MICRO_MPI_WTIME_ROOT);
assert(ret == PIO_NOERR);

// Create a simple timer, timer logs are written out to "temp_log.txt"
mt = mtimer_create("temp_timer", MPI_COMM_WORLD, "temp_log.txt");
assert(mt != NULL);

// Start the timer
ret = mtimer_start(mt)
assert(ret == PIO_NOERR);

// Stop the timer. The elapsed time is written out to log file, temp_log.txt
ret = mtimer_stop(mt, NULL)
assert(ret == PIO_NOERR);

// Destroy/delete the timer
ret = mtimer_destroy(&mt);
assert(ret == PIO_NOERR);

// Finalize the timer framework
ret = mtimer_finalize();
assert(ret == PIO_NOERR);

An asynchronous timer
------------------------

mtimer_t mt;

// Initialize the timer framework
ret = mtimer_init(PIO_MICRO_MPI_WTIME_ROOT);
assert(ret == PIO_NOERR);

// Create a simple timer, timer logs are written out to "temp_log.txt"
mt = mtimer_create("temp_timer", MPI_COMM_WORLD, "temp_log.txt");
assert(mt != NULL);

// Start the timer
ret = mtimer_start(mt)
assert(ret == PIO_NOERR);

// Asynchronous event started, will have to wait for completion in future
// e.g. a write is buffered, a non-blocking write is initiated etc
ret = async_event_start(...)
assert(ret == PIO_NOERR);

// Indicate that an asynchronous event is in progress
ret = mtimer_async_event_in_progress(mt, true);
assert(ret == PIO_NOERR);

// Stop the timer. The timing logs are NOT flushed to the log file
// since there are pending asynchronous events associated with the
// event
ret = mtimer_stop(mt, NULL)
assert(ret == PIO_NOERR);

// A temporary timer to measure the wait time for the asynchronous
// event
mtimer_t wait_mt;
wait_mt = mtimer_create("wait_timer", MPI_COMM_WORLD, "temp_log.txt");
assert(wait_mt != NULL);

ret = mtimer_start(wait_mt);
assert(ret == PIO_NOERR);

// Wait for the asynchronous event to complete
// e.g. : Waiting on pending MPI request associated with a non-blocking
// write
ret = wait_for_async_event_to_complete(...)
assert(ret == PIO_NOERR);

// Pausing a timer does not flush the timing information from this
// temporary timer
ret = mtimer_pause(wait_mt);
assert(ret == PIO_NOERR);

double wait_time;
ret = mtimer_get_wtime(wait_mt, &wait_time);
assert(ret == PIO_NOERR);

// Update/add original timer with time spent on waiting
ret = mtimer_update(mt, wait_time)
assert(ret == PIO_NOERR);

// Async event is completed, no more pending events
ret = mtimer_async_event_in_progress(mt, false);
assert(ret == PIO_NOERR);

// Explicitly flush the timer info to the timing log file
ret = mtimer_flush(mt, "Explicit flush of mt :");
assert(ret == PIO_NOERR);

ret = mtimer_destroy(&tmp_mt);
assert(ret == PIO_NOERR);

// Destroy/delete the timer
ret = mtimer_destroy(&mt);
assert(ret == PIO_NOERR);

// Finalize the timer framework
ret = mtimer_finalize();
assert(ret == PIO_NOERR);




