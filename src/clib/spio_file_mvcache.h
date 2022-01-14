#ifndef __SPIO_FILE_MVCACHE_H__
#define __SPIO_FILE_MVCACHE_H__

#include "pio_config.h"
#include "pio.h"
#include "pio_internal.h"

void spio_file_mvcache_init(file_desc_t *file);
void *spio_file_mvcache_get(file_desc_t *file, int ioid);
void *spio_file_mvcache_alloc(file_desc_t *file, int ioid, size_t sz);
void *spio_file_mvcache_realloc(file_desc_t *file, int ioid, size_t sz);
void spio_file_mvcache_free(file_desc_t *file, int ioid);
void spio_file_mvcache_clear(file_desc_t *file);
void spio_file_mvcache_finalize(file_desc_t *file);
#endif // __SPIO_FILE_MVCACHE_H__
