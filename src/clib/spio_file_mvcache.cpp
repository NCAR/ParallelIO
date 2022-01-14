#include <iostream>
#include <map>
#include <stdexcept>
#include <cassert>

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

    void *MVCache::get(int ioid)
    {
      try{
        return ioid2mvbuf_.at(ioid);
      }
      catch(std::out_of_range &e){
        /* Multi-variable cache has not been allocated for ioid */
        return NULL;
      }
    }
    
    void *MVCache::alloc(int ioid, std::size_t sz)
    {
      void *buf = bget(sz);
      ioid2mvbuf_[ioid] = buf;
      valid_mvbufs_++;
      return buf;
    }

    void *MVCache::realloc(int ioid, std::size_t sz)
    {
      void *buf = ioid2mvbuf_.at(ioid);
      buf = bgetr(buf, sz);
      ioid2mvbuf_[ioid] = buf;
      /* Note: Number of valid mvbufs remains the same */
      return buf;
    }

    void MVCache::free(int ioid)
    {
      brel(ioid2mvbuf_.at(ioid));
      ioid2mvbuf_[ioid] = NULL;
      valid_mvbufs_--;
    }

    void MVCache::clear(void )
    {
      /* First free all valid multi-variable buffers in this cache */
      for(std::map<int, void *>::iterator iter = ioid2mvbuf_.begin();
            iter != ioid2mvbuf_.end(); ++iter){
        if(iter->second){
          brel(iter->second);
          valid_mvbufs_--;
        }
      }

      assert(this->is_empty());
      /* Clear the internal map */
      ioid2mvbuf_.clear();
    }
  } // namespace File_Util
} // namespace SPIO_Util

/* C interfaces for accessing the Multi-variable cache */
void spio_file_mvcache_init(file_desc_t *file)
{
  assert(file != NULL);
  file->mvcache = static_cast<void *>(new SPIO_Util::File_Util::MVCache());
}

void *spio_file_mvcache_get(file_desc_t *file, int ioid)
{
  assert((file != NULL) && (file->mvcache != NULL) && (ioid >= 0));
  return (static_cast<SPIO_Util::File_Util::MVCache *>(file->mvcache))->get(ioid);
}

void *spio_file_mvcache_alloc(file_desc_t *file, int ioid, std::size_t sz)
{
  assert((file != NULL) && (file->mvcache != NULL) && (ioid >= 0) && (sz > 0));
  return (static_cast<SPIO_Util::File_Util::MVCache *>(file->mvcache))->alloc(ioid, sz);
}

void *spio_file_mvcache_realloc(file_desc_t *file, int ioid, std::size_t sz)
{
  assert((file != NULL) && (file->mvcache != NULL) && (ioid >= 0) && (sz > 0));
  return (static_cast<SPIO_Util::File_Util::MVCache *>(file->mvcache))->realloc(ioid, sz);
}

void spio_file_mvcache_free(file_desc_t *file, int ioid)
{
  assert((file != NULL) && (file->mvcache != NULL) && (ioid >= 0));
  (static_cast<SPIO_Util::File_Util::MVCache *>(file->mvcache))->free(ioid);
}

void spio_file_mvcache_clear(file_desc_t *file)
{
  assert((file != NULL) && (file->mvcache != NULL));
  (static_cast<SPIO_Util::File_Util::MVCache *>(file->mvcache))->clear();
}

void spio_file_mvcache_finalize(file_desc_t *file)
{
  if(file && file->mvcache){
    SPIO_Util::File_Util::MVCache *mvcache = static_cast<SPIO_Util::File_Util::MVCache *>(file->mvcache);
    /* An MVCache needs to be "clear"ed before finalize. Also, an MVCache can be cleared multiple times
     * but once finalized needs to be re"init"ed before using it
     */
    assert(mvcache->is_empty());
    delete mvcache;
    file->mvcache = NULL;
  }
}
