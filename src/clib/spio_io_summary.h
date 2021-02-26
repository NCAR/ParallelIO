#ifndef __SPIO_IO_SUMMARY_H__
#define __SPIO_IO_SUMMARY_H__

#include "pio_config.h"
#include "pio.h"
#include "pio_internal.h"

typedef struct spio_io_fstats_summary{
  char wr_timer_name[PIO_MAX_NAME + 1];
  char rd_timer_name[PIO_MAX_NAME + 1];
  char tot_timer_name[PIO_MAX_NAME + 1];

  /* Number of bytes read */
  PIO_Offset rb;
  /* Number of bytes written */
  PIO_Offset wb;
} spio_io_fstats_summary_t;

int spio_write_io_summary(iosystem_desc_t *ios);
int spio_write_file_io_summary(file_desc_t *file);
#endif /* __SPIO_IO_SUMMARY_H__ */
