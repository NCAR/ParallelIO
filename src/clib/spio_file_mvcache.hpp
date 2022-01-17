#ifndef __SPIO_FILE_MVCACHE_HPP__
#define __SPIO_FILE_MVCACHE_HPP__

#include <map>

extern "C"{
#include "pio_config.h"
#include "pio.h"
#include "pio_internal.h"
#include "spio_file_mvcache.h"
}

namespace SPIO_Util{
  namespace File_Util{
    /* Multi-variable cache used to cache data from multiple variables
     * while writing it out to the output file
     * - Each file uses a single MVCache to cache the output data.
     * - An MVCache caches data based on the I/O decomposition
     * - Each buffer in the MVCache contains data from multiple variables
     *   with the same I/O decomposition
     * e.g. Writing out variables v1, v2 with decomp d1 and v3, v4 with
     *      decomp d2 to the same file f1. The MVCache for f1 contains two
     *      elements,
     *      First element : Contains data from v1 and v2 (corresponding to d1)
     *      Second element : Contains data from v3 and v4 (corresponding to d2)
     * Also look at file_desc_t for information on how its used
     */
    class MVCache{
      public:
        MVCache() : valid_mvbufs_(0) {}
        /* Get multi-variable buffer associated with ioid */
        void *get(int ioid);
        /* Allocate a multi-variable buffer of size, sz, for ioid */
        void *alloc(int ioid, std::size_t sz);
        /* Reallocate a multi-variable buffer of size, sz, for ioid.
         * Assumes that a multi-variable buffer was already allocated
         * using alloc() for this ioid
         */
        void *realloc(int ioid, std::size_t sz);
        /* Free the multi-variable buffer associated with ioid */
        void free(int ioid);
        /* Clear the multi-variable cache
         * All multi-variable buffers associated with all ioids for this
         * cache is freed
         */
        void clear(void );

        /* Returns true if the multi-variable cache is empty (has no
         * valid, non-NULL, multi-variable buffers associated with any
         * ioid). Returns false otherwise
         */
        bool is_empty(void ) const { return (valid_mvbufs_ == 0); }
      private:
        /* The number of valid multi-variable buffers in this cache */
        int valid_mvbufs_;
        /* Internal map to associate the multi-variable buffer with ioid */
        std::map<int, void *> ioid2mvbuf_;
    };
  } // namespace File_Util
} // namespace SPIO_Util

#endif // __SPIO_FILE_MVCACHE_HPP__
