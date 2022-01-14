#ifndef __SPIO_FILE_MVCACHE_H__
#define __SPIO_FILE_MVCACHE_H__

#include "pio_config.h"
#include "pio.h"
#include "pio_internal.h"

/* C interface for accessing the Multi-Variable Cache */

/* Initialize the MVCache. The MVCache needs to init before using it */
void spio_file_mvcache_init(file_desc_t *file);
/* Get the MVCache associated with this ioid on this file */
void *spio_file_mvcache_get(file_desc_t *file, int ioid);
/* Allocate MVCache buffer for this ioid on this file */
void *spio_file_mvcache_alloc(file_desc_t *file, int ioid, size_t sz);
/* Reallocate MVCache buffer for this ioid on this file */
void *spio_file_mvcache_realloc(file_desc_t *file, int ioid, size_t sz);
/* Free MVCache buffer associated with this ioid on this file */
void spio_file_mvcache_free(file_desc_t *file, int ioid);
/* Clear all MVCache buffers associated with this file */
void spio_file_mvcache_clear(file_desc_t *file);
/* Finalize the MVCache */
void spio_file_mvcache_finalize(file_desc_t *file);
#endif // __SPIO_FILE_MVCACHE_H__
