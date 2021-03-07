#ifndef __SPIO_IO_SUMMARY_H__
#define __SPIO_IO_SUMMARY_H__

#include "pio_config.h"
#include "pio.h"
#include "pio_internal.h"

#define GPTL_TIMER_MAX_NAME 63
/* The struct below is used to cache I/O statistics
 * for I/O systems and files
 */
typedef struct spio_io_fstats_summary{
  /* Timer names for capturing write/read/total times */
  char wr_timer_name[GPTL_TIMER_MAX_NAME + 1];
  char rd_timer_name[GPTL_TIMER_MAX_NAME + 1];
  char tot_timer_name[GPTL_TIMER_MAX_NAME + 1];

  /* Number of bytes read */
  PIO_Offset rb;
  /* Number of bytes written */
  PIO_Offset wb;
} spio_io_fstats_summary_t;

/* Write the I/O performance summary. The function is
 * called when an I/O system is finalized
 */
int spio_write_io_summary(iosystem_desc_t *ios);
/* Write/cache the file I/O performance statistics.
 * The function is called when a file is closed
 */
int spio_write_file_io_summary(file_desc_t *file);
#endif /* __SPIO_IO_SUMMARY_H__ */
