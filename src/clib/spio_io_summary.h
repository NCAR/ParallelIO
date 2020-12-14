#ifndef __SPIO_IO_SUMMARY_H__
#define __SPIO_IO_SUMMARY_H__

#include "pio_config.h"
#include "pio.h"
#include "pio_internal.h"

typedef struct spio_io_fstats_summary{
  /* Number of bytes read */
  PIO_Offset rb;
  /* Number of bytes written */
  PIO_Offset wb;
} spio_io_fstats_summary_t;

int spio_write_io_summary(iosystem_desc_t *ios);
#endif /* __SPIO_IO_SUMMARY_H__ */
