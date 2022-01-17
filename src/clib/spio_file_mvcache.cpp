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

#include "spio_file_mvcache.hpp"

void *SPIO_Util::File_Util::MVCache::get(int ioid)
{
  try{
    return ioid2mvbuf_.at(ioid);
  }
  catch(std::out_of_range &e){
    /* Multi-variable cache has not been allocated for ioid */
    return NULL;
  }
}

void *SPIO_Util::File_Util::MVCache::alloc(int ioid, std::size_t sz)
{
  void *buf = bget(sz);
  ioid2mvbuf_[ioid] = buf;
  valid_mvbufs_++;
  return buf;
}

void *SPIO_Util::File_Util::MVCache::realloc(int ioid, std::size_t sz)
{
  void *buf = ioid2mvbuf_.at(ioid);
  buf = bgetr(buf, sz);
  ioid2mvbuf_[ioid] = buf;
  /* Note: Number of valid mvbufs remains the same */
  return buf;
}

void SPIO_Util::File_Util::MVCache::free(int ioid)
{
  brel(ioid2mvbuf_.at(ioid));
  ioid2mvbuf_[ioid] = NULL;
  valid_mvbufs_--;
}

void SPIO_Util::File_Util::MVCache::clear(void )
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
