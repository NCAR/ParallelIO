#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>

extern "C"{
#include "pio_config.h"
#include "pio.h"
#include "pio_tests.h"
#include "spio_file_mvcache.h"
}
#include "spio_file_mvcache.hpp"

#define LOG_RANK0(rank, ...)                     \
            do{                                   \
                if(rank == 0)                     \
                {                                 \
                    fprintf(stderr, __VA_ARGS__); \
                }                                 \
            }while(0);

static const int FAIL = -1;

/* Test creating an MVCache with no multi-variable buffers */
int test_empty_mvcache(int wrank)
{
  SPIO_Util::File_Util::MVCache empty_mvcache;

  if(!empty_mvcache.is_empty()){
    LOG_RANK0(wrank, "test_empty_mvcache() failed : MVCache not empty when created for the first time, expected MVCache to be empty\n");
    return PIO_EINTERNAL;
  }

  return PIO_NOERR;
}

/* Test creating an MVCache with one multi-variable buffer */
int test_one_mvbuf_mvcache(int wrank)
{
  SPIO_Util::File_Util::MVCache mvcache;

  if(!mvcache.is_empty()){
    LOG_RANK0(wrank, "test_one_mvbuf_mvcache() failed : MVCache not empty when created for the first time, expected MVCache to be empty\n");
    return PIO_EINTERNAL;
  }

  const int ioid = 1;
  /* Use multi-variable buffers of 16 bytes each */
  const int mvbuf_sz = 16;
  void *mvbuf = mvcache.alloc(ioid, mvbuf_sz);
  if(!mvbuf){
    LOG_RANK0(wrank, "test_one_mvbuf_mvcache() failed : Could not allocate multi-variable buffer in the MVCache\n");
    return PIO_EINTERNAL;
  }

  mvcache.clear();

  if(!mvcache.is_empty()){
    LOG_RANK0(wrank, "test_one_mvbuf_mvcache() failed : MVCache not empty after clear(), expected MVCache to be empty\n");
    return PIO_EINTERNAL;
  }

  return PIO_NOERR;
}

/* Test reusing an MVCache with one multi-variable buffer */
int test_reuse_mvbuf_mvcache(int wrank)
{
  SPIO_Util::File_Util::MVCache mvcache;

  if(!mvcache.is_empty()){
    LOG_RANK0(wrank, "test_reuse_mvbuf_mvcache() failed : MVCache not empty when created for the first time, expected MVCache to be empty\n");
    return PIO_EINTERNAL;
  }

  const int ioid = 1;
  /* Use multi-variable buffers of 16 bytes each */
  const int mvbuf_sz = 16;
  void *mvbuf = mvcache.alloc(ioid, mvbuf_sz);
  if(!mvbuf){
    LOG_RANK0(wrank, "test_reuse_mvbuf_mvcache() failed : Could not allocate multi-variable buffer in the MVCache \n");
    return PIO_EINTERNAL;
  }

  /* Reallocate the multi-variable buffer to reuse it - typically used to expand the buffer */
  mvbuf = mvcache.realloc(ioid, mvbuf_sz);
  if(!mvbuf){
    LOG_RANK0(wrank, "test_reuse_mvbuf_mvcache() failed : Could not reallocate multi-variable buffer in the MVCache \n");
    return PIO_EINTERNAL;
  }

  mvcache.free(ioid);

  if(!mvcache.is_empty()){
    LOG_RANK0(wrank, "test_reuse_mvbuf_mvcache() failed : MVCache not empty after free(), expected MVCache to be empty\n");
    return PIO_EINTERNAL;
  }

  /* Since the multi-variable buffer is freed, alloc() it again to reuse it */
  mvbuf = mvcache.alloc(ioid, mvbuf_sz);
  if(!mvbuf){
    LOG_RANK0(wrank, "test_reuse_mvbuf_mvcache() failed : Could not allocate multi-variable buffer after freeing it in the MVCache \n");
    return PIO_EINTERNAL;
  }

  mvcache.clear();

  if(!mvcache.is_empty()){
    LOG_RANK0(wrank, "test_reuse_mvbuf_mvcache() failed : MVCache not empty after clear(), expected MVCache to be empty\n");
    return PIO_EINTERNAL;
  }

  return PIO_NOERR;
}

/* Test using an MVCache with multiple I/O decomposition ids */
int test_multi_ioid_mvcache(int wrank)
{
  SPIO_Util::File_Util::MVCache mvcache;

  if(!mvcache.is_empty()){
    LOG_RANK0(wrank, "test_multi_ioid_mvcache() failed : MVCache not empty when created for the first time, expected MVCache to be empty\n");
    return PIO_EINTERNAL;
  }

  const std::vector<int> ioids = {1, 2, 4, 8, 9, 10, 512, 1024};
  std::vector<void *> mvbufs;
  /* Use multi-variable buffers of 16 bytes each */
  const int mvbuf_sz = 16;

  /* Check that no multi-variable buffer is associated with the ioids */
  for(std::vector<int>::const_iterator citer = ioids.cbegin(); citer != ioids.cend(); ++citer){
    void *mvbuf = mvcache.get(*citer);
    if(mvbuf){
      LOG_RANK0(wrank, "test_multi_ioid_mvcache() failed : Unallocated Multi-variable buffer for ioid (%d) is not NULL, expected buffer to be NULL since its not allocated yet \n", *citer);
      return PIO_EINTERNAL;
    }
  }

  /* Allocate multi-variable buffer for the different ioids */
  for(std::vector<int>::const_iterator citer = ioids.cbegin(); citer != ioids.cend(); ++citer){
    void *mvbuf = mvcache.alloc(*citer, mvbuf_sz);
    if(!mvbuf){
      LOG_RANK0(wrank, "test_multi_ioid_mvcache() failed : Could not allocate multi-variable buffer in the MVCache \n");
      return PIO_EINTERNAL;
    }
    mvbufs.push_back(mvbuf);
  }

  /* Get multi-variable buffer for the different ioids */
  assert(mvbufs.size() == ioids.size());
  int i = 0;
  for(std::vector<int>::const_iterator citer = ioids.cbegin(); citer != ioids.cend(); ++citer){
    void *mvbuf = mvcache.get(*citer);
    if(!mvbuf){
      LOG_RANK0(wrank, "test_multi_ioid_mvcache() failed : Could not get multi-variable buffer associated with ioid (%d) in the MVCache \n", *citer);
      return PIO_EINTERNAL;
    }

    if(mvbuf != mvbufs[i]){
      LOG_RANK0(wrank, "test_multi_ioid_mvcache() failed : Multi-variable buffer associated with ioid (%d) retrieved using get (%p) is different from the one allocated using alloc (%p)\n", *citer, mvbuf, mvbufs[i]);
      return PIO_EINTERNAL;
    }
    i++;
  }

  /* Free the multi-variable buffers associated with the ioids */
  for(std::vector<int>::const_iterator citer = ioids.cbegin(); citer != ioids.cend(); ++citer){
    if(mvcache.is_empty()){
      LOG_RANK0(wrank, "test_multi_ioid_mvcache() failed : MVCache empty after freeing multi-variable cache for some ioids, expected MVCache to be not empty\n");
      return PIO_EINTERNAL;
    }
    mvcache.free(*citer);
  }

  if(!mvcache.is_empty()){
    LOG_RANK0(wrank, "test_multi_ioid_mvcache() failed : MVCache not empty after freeing multi-variable buffers for all ioids, expected MVCache to be empty\n");
    return PIO_EINTERNAL;
  }

  /* Clearing an empty MVCache should not fail */
  mvcache.clear();

  if(!mvcache.is_empty()){
    LOG_RANK0(wrank, "test_multi_ioid_mvcache() failed : MVCache not empty after clear(), expected MVCache to be empty\n");
    return PIO_EINTERNAL;
  }

  return PIO_NOERR;
}

/* Test the MVCache C interface */
int test_cint_mvcache(int wrank)
{
  file_desc_t file;

  std::memset(&file, 0, sizeof(file_desc_t));

  /* Initialize the mvcache associated with this dummy file */
  spio_file_mvcache_init(&file);

  const std::vector<int> ioids = {1, 2, 4, 8, 9, 10, 512, 1024};
  std::vector<void *> mvbufs;

  /* Use multi-variable buffers of (mvbuf_init_sz + x * mvbuf_sz_scale) bytes each */
  const int mvbuf_init_sz = 2;
  const int mvbuf_sz_scale = 2;

  /* Check that no multi-variable buffer is associated with the ioids */
  for(std::vector<int>::const_iterator citer = ioids.cbegin(); citer != ioids.cend(); ++citer){
    void *mvbuf = spio_file_mvcache_get(&file, *citer);
    if(mvbuf){
      LOG_RANK0(wrank, "test_cint_mvcache() failed : Unallocated Multi-variable buffer for ioid (%d) is not NULL, expected buffer to be NULL since its not allocated yet \n", *citer);
      return PIO_EINTERNAL;
    }
  }

  /* Allocate multi-variable buffer for the different ioids */
  int i = 0;
  for(std::vector<int>::const_iterator citer = ioids.cbegin(); citer != ioids.cend(); ++citer, i++){
    std::size_t mvbuf_sz = mvbuf_init_sz + i * mvbuf_sz_scale;
    void *mvbuf = spio_file_mvcache_alloc(&file, *citer, mvbuf_sz);
    if(!mvbuf){
      LOG_RANK0(wrank, "test_cint_mvcache() failed : Could not allocate multi-variable buffer (%llu bytes ) in the MVCache for ioid (%d)\n", static_cast<unsigned long long>(mvbuf_sz), *citer);
      return PIO_EINTERNAL;
    }
    mvbufs.push_back(mvbuf);
  }

  /* Get multi-variable buffer for the different ioids */
  assert(mvbufs.size() == ioids.size());
  i = 0;
  for(std::vector<int>::const_iterator citer = ioids.cbegin(); citer != ioids.cend(); ++citer, i++){
    void *mvbuf = spio_file_mvcache_get(&file, *citer);
    if(!mvbuf){
      LOG_RANK0(wrank, "test_cint_mvcache() failed : Could not get multi-variable buffer associated with ioid (%d) in the MVCache \n", *citer);
      return PIO_EINTERNAL;
    }

    if(mvbuf != mvbufs[i]){
      LOG_RANK0(wrank, "test_cint_mvcache() failed : Multi-variable buffer associated with ioid (%d) retrieved using get (%p) is different from the one allocated using alloc (%p)\n", *citer, mvbuf, mvbufs[i]);
      return PIO_EINTERNAL;
    }
  }

  /* Free the multi-variable buffers associated with the ioids */
  for(std::vector<int>::const_iterator citer = ioids.cbegin(); citer != ioids.cend(); ++citer){
    spio_file_mvcache_free(&file, *citer);
  }

  /* Clearing an empty MVCache should not fail */
  spio_file_mvcache_clear(&file);

  /* Finalize the mvcache associated with this dummy file */
  spio_file_mvcache_finalize(&file);

  return PIO_NOERR;
}

int test_driver(MPI_Comm comm, int wrank, int wsz, int *num_errors)
{
  int nerrs = 0, ret = PIO_NOERR;
  assert((comm != MPI_COMM_NULL) && (wrank >= 0) && (wsz > 0) && num_errors);
  
  /* Test creating an empty MVCache */
  try{
    ret = test_empty_mvcache(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_empty_mvcache() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_empty_mvcache() PASSED\n");
  }

  /* Test creating an MVCache with one multi-variable buffer */
  try{
    ret = test_one_mvbuf_mvcache(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_one_mvbuf_mvcache() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_one_mvbuf_mvcache() PASSED\n");
  }

  /* Test reusing an MVCache with one multi-variable buffer */
  try{
    ret = test_reuse_mvbuf_mvcache(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_reuse_mvbuf_mvcache() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_reuse_mvbuf_mvcache() PASSED\n");
  }

  /* Test using an MVCache with multiple I/O decomposition ids */
  try{
    ret = test_multi_ioid_mvcache(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_multi_ioid_mvcache() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_multi_ioid_mvcache() PASSED\n");
  }

  /* Test the C interface for MVCache with multiple I/O decomposition ids */
  try{
    ret = test_cint_mvcache(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_cint_mvcache() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_cint_mvcache() PASSED\n");
  }

  *num_errors += nerrs;
  return nerrs;
}

int main(int argc, char *argv[])
{
  int ret;
  int wrank, wsz;
  int num_errors;
#ifdef TIMING
#ifndef TIMING_INTERNAL
  ret = GPTLinitialize();
  if(ret != 0){
    LOG_RANK0(wrank, "GPTLinitialize() FAILED, ret = %d\n", ret);
    return ret;
  }
#endif /* TIMING_INTERNAL */
#endif /* TIMING */

  ret = MPI_Init(&argc, &argv);
  if(ret != MPI_SUCCESS){
    LOG_RANK0(wrank, "MPI_Init() FAILED, ret = %d\n", ret);
    return ret;
  }

  ret = MPI_Comm_rank(MPI_COMM_WORLD, &wrank);
  if(ret != MPI_SUCCESS){
    LOG_RANK0(wrank, "MPI_Comm_rank() FAILED, ret = %d\n", ret);
    return ret;
  }
  ret = MPI_Comm_size(MPI_COMM_WORLD, &wsz);
  if(ret != MPI_SUCCESS){
    LOG_RANK0(wrank, "MPI_Comm_rank() FAILED, ret = %d\n", ret);
    return ret;
  }

  num_errors = 0;
  ret = test_driver(MPI_COMM_WORLD, wrank, wsz, &num_errors);
  if(ret != 0){
    LOG_RANK0(wrank, "Test driver FAILED\n");
    return FAIL;
  }
  else{
    LOG_RANK0(wrank, "All tests PASSED\n");
  }

  MPI_Finalize();

#ifdef TIMING
#ifndef TIMING_INTERNAL
  ret = GPTLfinalize();
  if(ret != 0){
    LOG_RANK0(wrank, "GPTLinitialize() FAILED, ret = %d\n", ret);
    return ret;
  }
#endif /* TIMING_INTERNAL */
#endif /* TIMING */

  if(num_errors != 0){
    LOG_RANK0(wrank, "Total errors = %d\n", num_errors);
    return FAIL;
  }
  return 0;
}
