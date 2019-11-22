#include "mpi.h"
#include "pio.h"
#include "pio_internal.h"
#include "pio_tests.h"


#define LOG_RANK0(rank, ...)                     \
            do{                                   \
                if(rank == 0)                     \
                {                                 \
                    fprintf(stderr, __VA_ARGS__); \
                }                                 \
            }while(0);

static const int FAIL = -1;

/* Update vars in file with dummy reqs
 * file : pointer to file descriptor struct associated with file
 * nvars : Number of vars to update
 * disp : Number of vars to skip (displacement from the 1st var)
 * stride : The stride to use to decide which vars to update
 * nreqs : Number of requests to add to each variable
 * request_sizes : Array of request sizes
 * nrequest_sizes : Size of array of request sizes
 */
int update_file_varlist(file_desc_t *file,
                      int nvars, int disp, int stride,
                      int nreqs,
                      PIO_Offset *request_sizes, int nrequest_sizes)
{
  int ret = PIO_NOERR;
  const int DUMMY_REQ_START = 101;
  assert(file &&
    (nvars > 0) && (disp >= 0) && (stride > 0) &&
    (nreqs > 0) && (request_sizes) && (nrequest_sizes > 0));

  for(int v = 0, i = disp;
      (v < nvars) && (i < PIO_MAX_VARS); v++, i += stride){
    file->varlist[i].varid = i;
    snprintf(file->varlist[i].vname, PIO_MAX_NAME, "test_var_%d", i);
    if(file->varlist[i].request){
      assert(file->varlist[i].nreqs > 0);
      free(file->varlist[i].request);
      assert(file->varlist[i].request_sz != NULL);
      free(file->varlist[i].request_sz);
    }
    file->varlist[i].request = (int *)malloc(nreqs * sizeof(int));
    file->varlist[i].request_sz =
      (PIO_Offset *)malloc(nreqs * sizeof(PIO_Offset));
    assert(file->varlist[i].request && file->varlist[i].request_sz);

    for(int j = 0, reqid = DUMMY_REQ_START, k = 0;
        j < nreqs; j++, reqid++, k++){
      if(k == nrequest_sizes){
        k = 0;
      }
      file->varlist[i].request[j] = reqid;
      file->varlist[i].request_sz[j] = request_sizes[k];
    }
    file->varlist[i].nreqs = nreqs;
  }

  return ret;
}

/* Init file->varlist */
int init_file_varlist(file_desc_t *file)
{
  int ret = PIO_NOERR;
  assert(file);

  for(int i = 0; i < PIO_MAX_VARS; i++){
    file->varlist[i].varid = 0;
    file->varlist[i].vname[0] = '\0';
    file->varlist[i].vdesc[0] = '\0';
    file->varlist[i].rec_var = 0;
    file->varlist[i].record = 0;
    file->varlist[i].request = NULL;
    file->varlist[i].request_sz = NULL;
    file->varlist[i].nreqs = 0;
    file->varlist[i].fillvalue = NULL;
    file->varlist[i].pio_type = PIO_INT;
    file->varlist[i].type_size = sizeof(int);
    file->varlist[i].vrsize = 0;
    file->varlist[i].rb_pend = 0;
    file->varlist[i].wb_pend = 0;
    file->varlist[i].use_fill = 0;
    file->varlist[i].fillbuf = NULL;
  }
  return ret;
}

/* Free file->varlist, file is not freed */
void free_file_varlist(file_desc_t *file)
{
  assert(file);
  for(int i = 0; i < PIO_MAX_VARS; i++){
    if(file->varlist[i].nreqs > 0){
      free(file->varlist[i].request);
      free(file->varlist[i].request_sz);
    }
  }
}

/* Re-initialize file->varlist : free current varlist and init */
int reinit_file_varlist(file_desc_t *file)
{
  int ret = PIO_NOERR;
  free_file_varlist(file);

  ret = init_file_varlist(file);

  return ret;
}

/* Set up iosystem and file structs for a test */
int test_setup(MPI_Comm comm, int rank, int sz,
      iosystem_desc_t **pios, file_desc_t **pfile)
{
  int ret = PIO_NOERR;
  const int TEST_IOSYSID = 101;
  const int ROOT_RANK = 0;

  assert((comm != MPI_COMM_NULL) && (rank >= 0) && (sz > 0) && pios && pfile);

  *pios = calloc(1, sizeof(iosystem_desc_t));
  *pfile = calloc(1, sizeof(file_desc_t));

  iosystem_desc_t *ios = *pios;
  file_desc_t *file = *pfile;

  assert(ios && file);
  
  /* Initialize I/O system
   * - All tasks are I/O tasks
   * - I/O root is rank 0
   */
  ios->iosysid = TEST_IOSYSID;
  ios->union_comm = comm;
  ios->io_comm = comm;
  ios->comp_comm = comm;
  ios->intercomm = MPI_COMM_NULL;
  ios->my_comm = comm;
  ios->compgroup = MPI_GROUP_NULL;
  ios->iogroup = MPI_GROUP_NULL;
  ios->num_iotasks = sz;
  ios->num_comptasks = sz;
  ios->num_uniontasks = sz;
  ios->union_rank = rank;
  ios->comp_rank = rank;
  ios->io_rank = rank;
  ios->iomaster = (rank == ROOT_RANK) ? MPI_ROOT : 0;
  ios->compmaster = (rank == ROOT_RANK) ? MPI_ROOT : 0;
  ios->ioroot = ROOT_RANK;
  ios->comproot = ROOT_RANK;
  /* We don't need the I/O / compute process ranks for this test */
  ios->ioranks = NULL;
  ios->compranks = NULL;
  ios->error_handler = PIO_RETURN_ERROR;
  ios->default_rearranger = PIO_REARR_BOX;
  ios->async = false;
  ios->ioproc = true;
  ios->compproc = true;
  ios->info = MPI_INFO_NULL;
  ios->async_ios_msg_info.seq_num = 0;
  ios->async_ios_msg_info.prev_msg = 0;
  ios->comp_idx = 0;
  /* We don't need the rearranger options set for this test */
  ios->next = NULL;


  /* Initialize file structure with some dummy requests */
  file->iosystem = ios;
  file->fh = 0;
  strncpy(file->fname, "test_file_req_blocks.nc", PIO_MAX_NAME);
  file->pio_ncid = 0;
  file->iotype = PIO_IOTYPE_PNETCDF;

  ret = init_file_varlist(file);
  assert(ret == PIO_NOERR);

  file->num_unlim_dimids = 0;
  file->unlim_dimids= NULL;
  file->mode = PIO_WRITE;

  /* Write multibuffer is not used by this test */
  file->rb_pend = 0;
  file->wb_pend = 0;
  for(int i = 0; i < PIO_IODESC_MAX_IDS; i++){
    file->iobuf[i] = NULL;
  }
  file->next = NULL;
  file->do_io = true;

  return ret;
}

/* Teardown/finalize iosystem and file structs */
int test_teardown(iosystem_desc_t **pios, file_desc_t **pfile)
{
  int ret = PIO_NOERR;
  assert(pios && pfile);

  iosystem_desc_t *ios = *pios;
  file_desc_t *file = *pfile;

  if(ios){
    free(ios);
    *pios = NULL;
  }

  if(file){
    free_file_varlist(file);
    free(file);
    *pfile = NULL;
  }

  return ret;
}

/* Unit tests with simple pending requests in a file */
int test_simple_file_req_blocks(MPI_Comm comm, int rank, int sz)
{
  int ret = PIO_NOERR;
  iosystem_desc_t *iosys = NULL;
  file_desc_t *file = NULL;

  ret = test_setup(comm, rank, sz, &iosys, &file);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Initializing/setup of test failed for test_simple_file_req_blocks, ret = %d\n", ret);
    return ret;
  }

  const PIO_Offset MAX_REQ_SZ = 10;

  PIO_Offset one_req_sz[1];
  PIO_Offset three_req_sz[3];
  int *reqs = NULL;
  int nreqs = 0;
  int nvars_with_reqs = 0;
  int last_var_with_req = 0;
  int *req_block_ranges = NULL;
  int ereq_one_block_ranges[1 * 2];
  int ereq_two_block_ranges[2 * 2];
  int nreq_blocks = 0;
  int enreq_blocks = 0;
  /*******************  Test 1 *************************/
  /* A single variable with 1 request within req limit */
  one_req_sz[0] = MAX_REQ_SZ - 1;
  ret = update_file_varlist(file, 1, 0, 1, 1, one_req_sz, 1);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Updating file varlist failed for test_simple_file_req_blocks, 1 var with 1 req within req limit, ret = %d\n", ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  ret = set_file_req_block_size_limit(file, MAX_REQ_SZ);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Setting file request block size limit (to %llu bytes) failed : 1 var with 1 req : test_simple_file_req_blocks, ret = %d\n", (unsigned long long) MAX_REQ_SZ, ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  reqs = NULL;
  nreqs = 0;
  nvars_with_reqs = 0;
  last_var_with_req = 0;
  req_block_ranges = NULL;
  nreq_blocks = 0;
  enreq_blocks = 1;
  ereq_one_block_ranges[0] = 0;
  ereq_one_block_ranges[1] = 0;
  ret = get_file_req_blocks(file,
                            &reqs, &nreqs,
                            &nvars_with_reqs, &last_var_with_req,
                            &req_block_ranges, &nreq_blocks);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Getting file request block ranges failed : 1 var with 1 req : test_simple_file_req_blocks, ret = %d\n", ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  if(nreq_blocks != enreq_blocks){
    LOG_RANK0(rank, "Error while calculating block ranges (expected = %d, returned=%d) : 1 var with 1 req : test_simple_file_req_blocks, ret = %d\n", enreq_blocks, nreq_blocks, ret);
    free(reqs);
    free(req_block_ranges);
    test_teardown(&iosys, &file);
    return PIO_EINTERNAL;
  }

  assert(nreq_blocks == 1);
  for(int i = 0; i < 2 * nreq_blocks; i++){
    if(req_block_ranges[i] != ereq_one_block_ranges[i]){
      LOG_RANK0(rank, "Error: Incorrect block range returned, block range index %d = %d (expected %d)\n", i, req_block_ranges[i], ereq_one_block_ranges[i]);
      LOG_RANK0(rank, "Block ranges returned : \n");
      for(int j = 0; j < nreq_blocks; j++){
        LOG_RANK0(rank, "[%d, %d], ",
          req_block_ranges[j], req_block_ranges[j + nreq_blocks]);
      }
      LOG_RANK0(rank, "\n");
      LOG_RANK0(rank, "Block ranges expected : \n");
      for(int j = 0; j < nreq_blocks; j++){
        LOG_RANK0(rank, "[%d, %d], ",
          ereq_one_block_ranges[j], ereq_one_block_ranges[j + nreq_blocks]);
      }
      LOG_RANK0(rank, "\n");

      free(reqs);
      free(req_block_ranges);
      test_teardown(&iosys, &file);
      return PIO_EINTERNAL;
    }
  }

  free(reqs);
  free(req_block_ranges);

  /*******************  Test 2 *************************/
  /* Two variables with 1 request each within req limit */
  one_req_sz[0] = MAX_REQ_SZ / 2;

  ret = update_file_varlist(file, 2, 0, 1, 1, one_req_sz, 1);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Updating file varlist failed for test_simple_file_req_blocks, 2 vars with 1 req within req limit, ret = %d\n", ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  ret = set_file_req_block_size_limit(file, MAX_REQ_SZ);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Setting file request block size limit (to %llu bytes) failed : 2 vars with 1 req each : test_simple_file_req_blocks, ret = %d\n", (unsigned long long) MAX_REQ_SZ, ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  reqs = NULL;
  nreqs = 0;
  nvars_with_reqs = 0;
  last_var_with_req = 0;
  req_block_ranges = NULL;
  nreq_blocks = 0;
  enreq_blocks = 1;
  ereq_one_block_ranges[0] = 0;
  ereq_one_block_ranges[1] = 1;
  ret = get_file_req_blocks(file,
                            &reqs, &nreqs,
                            &nvars_with_reqs, &last_var_with_req,
                            &req_block_ranges, &nreq_blocks);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Getting file request block ranges failed : 2 vars wtih 1 req each : test_simple_file_req_blocks, ret = %d\n", ret);
    return ret;
  }

  if(nreq_blocks != enreq_blocks){
    LOG_RANK0(rank, "Error while calculating block ranges (expected = %d, returned=%d) : 2 vars with 1 req each : test_simple_file_req_blocks, ret = %d\n", enreq_blocks, nreq_blocks, ret);
    free(reqs);
    free(req_block_ranges);
    test_teardown(&iosys, &file);
    return PIO_EINTERNAL;
  }

  assert(nreq_blocks == 1);
  for(int i = 0; i < 2 * nreq_blocks; i++){
    if(req_block_ranges[i] != ereq_one_block_ranges[i]){
      LOG_RANK0(rank, "Error: Incorrect block range returned, block range index %d = %d (expected %d)\n", i, req_block_ranges[i], ereq_one_block_ranges[i]);
      LOG_RANK0(rank, "Block ranges returned : \n");
      for(int j = 0; j < nreq_blocks; j++){
        LOG_RANK0(rank, "[%d, %d], ",
          req_block_ranges[j], req_block_ranges[j + nreq_blocks]);
      }
      LOG_RANK0(rank, "\n");
      LOG_RANK0(rank, "Block ranges expected : \n");
      for(int j = 0; j < nreq_blocks; j++){
        LOG_RANK0(rank, "[%d, %d], ",
          ereq_one_block_ranges[j], ereq_one_block_ranges[j + nreq_blocks]);
      }
      LOG_RANK0(rank, "\n");

      free(reqs);
      free(req_block_ranges);
      test_teardown(&iosys, &file);
      return PIO_EINTERNAL;
    }
  }

  free(reqs);
  free(req_block_ranges);

  /*******************  Test 3 *************************/
  /* Two variables with 3 requests each within req limit */
  three_req_sz[0] = MAX_REQ_SZ / 3;
  three_req_sz[1] = MAX_REQ_SZ / 3;
  three_req_sz[2] = MAX_REQ_SZ / 3;

  ret = update_file_varlist(file, 2, 0, 1, 3, three_req_sz, 3);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Updating file varlist failed for test_simple_file_req_blocks, 2 vars with 3 reqs each, within req limit, ret = %d\n", ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  ret = set_file_req_block_size_limit(file, MAX_REQ_SZ);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Setting file request block size limit (to %llu bytes) failed : 2 vars with 3 reqs each : test_simple_file_req_blocks, ret = %d\n", (unsigned long long) MAX_REQ_SZ, ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  reqs = NULL;
  nreqs = 0;
  nvars_with_reqs = 0;
  last_var_with_req = 0;
  req_block_ranges = NULL;
  nreq_blocks = 0;
  enreq_blocks = 2;
  /* Expected ranges [0,2], [3,5] */
  ereq_two_block_ranges[0] = 0;
  ereq_two_block_ranges[2] = 2;
  ereq_two_block_ranges[1] = 3;
  ereq_two_block_ranges[3] = 5;
  ret = get_file_req_blocks(file,
                            &reqs, &nreqs,
                            &nvars_with_reqs, &last_var_with_req,
                            &req_block_ranges, &nreq_blocks);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Getting file request block ranges failed : 2 vars with 3 reqs each : test_simple_file_req_blocks, ret = %d\n", ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  if(nreq_blocks != enreq_blocks){
    LOG_RANK0(rank, "Error while calculating block ranges (expected = %d, returned=%d) : 2 vars with 3 reqs each : test_simple_file_req_blocks, ret = %d\n", enreq_blocks, nreq_blocks, ret);
    free(reqs);
    free(req_block_ranges);
    test_teardown(&iosys, &file);
    return PIO_EINTERNAL;
  }

  assert(nreq_blocks == 2);
  for(int i = 0; i < 2 * nreq_blocks; i++){
    if(req_block_ranges[i] != ereq_two_block_ranges[i]){
      LOG_RANK0(rank, "Error: Incorrect block range returned, block range index %d = %d (expected %d)\n", i, req_block_ranges[i], ereq_two_block_ranges[i]);
      LOG_RANK0(rank, "Block ranges returned : \n");
      for(int j = 0; j < nreq_blocks; j++){
        LOG_RANK0(rank, "[%d, %d], ",
          req_block_ranges[j], req_block_ranges[j + nreq_blocks]);
      }
      LOG_RANK0(rank, "\n");
      LOG_RANK0(rank, "Block ranges expected : \n");
      for(int j = 0; j < nreq_blocks; j++){
        LOG_RANK0(rank, "[%d, %d], ",
          ereq_two_block_ranges[j], ereq_two_block_ranges[j + nreq_blocks]);
      }
      LOG_RANK0(rank, "\n");

      free(reqs);
      free(req_block_ranges);
      test_teardown(&iosys, &file);
      return PIO_EINTERNAL;
    }
  }

  free(reqs);
  free(req_block_ranges);

  ret = test_teardown(&iosys, &file);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Finalizing/teardown of test failed for test_simple_file_req_blocks, ret = %d\n", ret);
    return ret;
  }

  return ret;
}

/* Unit test with misc file pending request patterns : This
 * test includes slightly  more complex pending request patterns
 */
int test_misc_file_req_blocks(MPI_Comm comm, int rank, int sz)
{
  int ret = PIO_NOERR;
  iosystem_desc_t *iosys = NULL;
  file_desc_t *file = NULL;

  ret = test_setup(comm, rank, sz, &iosys, &file);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Initializing/setup of test failed for test_misc_file_req_blocks, ret = %d\n", ret);
    return ret;
  }

  const PIO_Offset MAX_REQ_SZ = 20;

  PIO_Offset two_req_sz[2];
  PIO_Offset three_req_sz[3];
  int *reqs = NULL;
  int nreqs = 0;
  int nvars_with_reqs = 0;
  int last_var_with_req = 0;
  int *req_block_ranges = NULL;
  int nreq_blocks = 0;
  int enreq_blocks = 0;
  int ereq_one_block_ranges[1 * 2];
  int ereq_two_block_ranges[2 * 2];
  int ereq_three_block_ranges[3 * 2];
  int ereq_four_block_ranges[4 * 2];

  /*******************  Test 1 *************************/
  /* Two variables, with stride 2, with 3 requests each within req limit */
  three_req_sz[0] = MAX_REQ_SZ / 3;
  three_req_sz[1] = MAX_REQ_SZ / 3;
  three_req_sz[2] = MAX_REQ_SZ / 3;

  ret = update_file_varlist(file, 2, 0, 2, 3, three_req_sz, 3);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Updating file varlist failed for test_misc_file_req_blocks, 2 vars stride 2 with 3 reqs each, within req limit, ret = %d\n", ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  ret = set_file_req_block_size_limit(file, MAX_REQ_SZ);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Setting file request block size limit (to %llu bytes) failed : 2 vars stride 2 with 3 reqs each : test_misc_file_req_blocks, ret = %d\n", (unsigned long long) MAX_REQ_SZ, ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  reqs = NULL;
  nreqs = 0;
  nvars_with_reqs = 0;
  last_var_with_req = 0;
  req_block_ranges = NULL;
  nreq_blocks = 0;
  enreq_blocks = 2;
  /* Expected ranges [0, 2], [3, 5] */
  ereq_two_block_ranges[0] = 0;
  ereq_two_block_ranges[2] = 2;
  ereq_two_block_ranges[1] = 3;
  ereq_two_block_ranges[3] = 5;
  ret = get_file_req_blocks(file,
                            &reqs, &nreqs,
                            &nvars_with_reqs, &last_var_with_req,
                            &req_block_ranges, &nreq_blocks);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Getting file request block ranges failed : 2 vars stride 2 with 3 reqs each : test_misc_file_req_blocks, ret = %d\n", ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  if(nreq_blocks != enreq_blocks){
    LOG_RANK0(rank, "Error while calculating block ranges (expected = %d, returned=%d) : 2 vars stride 2 with 3 reqs each : test_misc_file_req_blocks, ret = %d\n", enreq_blocks, nreq_blocks, ret);
    free(reqs);
    free(req_block_ranges);
    test_teardown(&iosys, &file);
    return PIO_EINTERNAL;
  }

  assert(nreq_blocks == 2);
  for(int i = 0; i < 2 * nreq_blocks; i++){
    if(req_block_ranges[i] != ereq_two_block_ranges[i]){
      LOG_RANK0(rank, "Error: Incorrect block range returned, block range index %d = %d (expected %d)\n", i, req_block_ranges[i], ereq_two_block_ranges[i]);
      LOG_RANK0(rank, "Block ranges returned : \n");
      for(int j = 0; j < nreq_blocks; j++){
        LOG_RANK0(rank, "[%d, %d], ",
          req_block_ranges[j], req_block_ranges[j + nreq_blocks]);
      }
      LOG_RANK0(rank, "\n");
      LOG_RANK0(rank, "Block ranges expected : \n");
      for(int j = 0; j < nreq_blocks; j++){
        LOG_RANK0(rank, "[%d, %d], ",
          ereq_two_block_ranges[j], ereq_two_block_ranges[j + nreq_blocks]);
      }
      LOG_RANK0(rank, "\n");

      free(reqs);
      free(req_block_ranges);
      test_teardown(&iosys, &file);
      return PIO_EINTERNAL;
    }
  }

  free(reqs);
  free(req_block_ranges);

  /*******************  Test 2 *************************/
  /* Three variables, with stride 2, displacement 1 and
   * with 3 requests each within req limit
   */
  three_req_sz[0] = MAX_REQ_SZ / 3;
  three_req_sz[1] = MAX_REQ_SZ / 3;
  three_req_sz[2] = MAX_REQ_SZ / 3;

  ret = reinit_file_varlist(file);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Re-initializing file varlist failed for test_misc_file_req_blocks, 2 vars disp 1 stride 2 with 3 reqs each within req limit, ret = %d\n", ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  ret = update_file_varlist(file, 3, 1, 2, 3, three_req_sz, 3);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Updating file varlist failed for test_misc_file_req_blocks, 3 vars disp 1 stride 2 with 3 reqs each, within req limit, ret = %d\n", ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  ret = set_file_req_block_size_limit(file, MAX_REQ_SZ);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Setting file request block size limit (to %llu bytes) failed : 3 vars disp 1 stride 2 with 3 reqs each : test_misc_file_req_blocks, ret = %d\n", (unsigned long long) MAX_REQ_SZ, ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  reqs = NULL;
  nreqs = 0;
  nvars_with_reqs = 0;
  last_var_with_req = 0;
  req_block_ranges = NULL;
  nreq_blocks = 0;
  enreq_blocks = 3;
  /* Expected ranges [0,2], [3,5], [6,8] */
  ereq_three_block_ranges[0] = 0;
  ereq_three_block_ranges[3] = 2;
  ereq_three_block_ranges[1] = 3;
  ereq_three_block_ranges[4] = 5;
  ereq_three_block_ranges[2] = 6;
  ereq_three_block_ranges[5] = 8;
  ret = get_file_req_blocks(file,
                            &reqs, &nreqs,
                            &nvars_with_reqs, &last_var_with_req,
                            &req_block_ranges, &nreq_blocks);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Getting file request block ranges failed : 3 vars disp 1 stride 2 with 3 reqs each : test_misc_file_req_blocks, ret = %d\n", ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  if(nreq_blocks != enreq_blocks){
    LOG_RANK0(rank, "Error while calculating block ranges (expected = %d, returned=%d) : 3 vars disp 1 stride 2 with 3 reqs each : test_misc_file_req_blocks, ret = %d\n", enreq_blocks, nreq_blocks, ret);
    free(reqs);
    free(req_block_ranges);
    test_teardown(&iosys, &file);
    return PIO_EINTERNAL;
  }

  assert(nreq_blocks == 3);
  for(int i = 0; i < 2 * nreq_blocks; i++){
    if(req_block_ranges[i] != ereq_three_block_ranges[i]){
      LOG_RANK0(rank, "Error: Incorrect block range returned, block range index %d = %d (expected %d)\n", i, req_block_ranges[i], ereq_three_block_ranges[i]);
      LOG_RANK0(rank, "Block ranges returned : \n");
      for(int j = 0; j < nreq_blocks; j++){
        LOG_RANK0(rank, "[%d, %d], ",
          req_block_ranges[j], req_block_ranges[j + nreq_blocks]);
      }
      LOG_RANK0(rank, "\n");
      LOG_RANK0(rank, "Block ranges expected : \n");
      for(int j = 0; j < nreq_blocks; j++){
        LOG_RANK0(rank, "[%d, %d], ",
          ereq_three_block_ranges[j], ereq_three_block_ranges[j + nreq_blocks]);
      }
      LOG_RANK0(rank, "\n");

      free(reqs);
      free(req_block_ranges);
      test_teardown(&iosys, &file);
      return PIO_EINTERNAL;
    }
  }

  free(reqs);
  free(req_block_ranges);

  /*******************  Test 3 *************************/
  /* Three variables, with stride 2, displacement 1 and
   * with 3 requests each within req limit
   * MAX_REQ_SZ/2 variables with 2 reqs of size 1 each, stride 2
   * MAX_REQ_SZ/4 variables with 2 reqs of size 2 each, stride 3
   * MAX_REQ_SZ/8 variables with 2 reqs of size 4 each, stride 4
   * MAX_REQ_SZ/16 variables with 2 reqs of size 8 each, stride 5
   */
  ret = reinit_file_varlist(file);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Re-initializing file varlist failed for test_misc_file_req_blocks, 2 vars disp 1 stride 2 with 3 reqs each within req limit, ret = %d\n", ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  PIO_Offset req_sz = 1;
  int disp = 0;
  enreq_blocks = 4;
  for(int nvars = MAX_REQ_SZ/2, stride = 2, ereq_block_idx = 0, enreqs = 0;
      nvars > 0;
      nvars /= 2, req_sz *= 2, stride += 1, ereq_block_idx++){
    two_req_sz[0] = req_sz;
    two_req_sz[1] = req_sz;

    ret = update_file_varlist(file, nvars, disp, stride, 2, two_req_sz, 2);
    if(ret != PIO_NOERR){
      LOG_RANK0(rank, "Updating file varlist failed for test_misc_file_req_blocks, exp inc vars with 2 reqs each, within req limit, ret = %d\n", ret);
      test_teardown(&iosys, &file);
      return ret;
    }

    disp += (nvars * stride);

    assert((enreq_blocks == 4) && (ereq_block_idx < 4));
    int nreqs_in_one_block = min(max(1, MAX_REQ_SZ / req_sz), nvars * 2);
    
    ereq_four_block_ranges[ereq_block_idx] = enreqs;
    ereq_four_block_ranges[ereq_block_idx + enreq_blocks] =
      enreqs + nreqs_in_one_block - 1;
    enreqs += nreqs_in_one_block;
  }

  ret = set_file_req_block_size_limit(file, MAX_REQ_SZ);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Setting file request block size limit (to %llu bytes) failed : exp inc vars with 2 reqs each : test_misc_file_req_blocks, ret = %d\n", (unsigned long long) MAX_REQ_SZ, ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  reqs = NULL;
  nreqs = 0;
  nvars_with_reqs = 0;
  last_var_with_req = 0;
  req_block_ranges = NULL;
  nreq_blocks = 0;
  ret = get_file_req_blocks(file,
                            &reqs, &nreqs,
                            &nvars_with_reqs, &last_var_with_req,
                            &req_block_ranges, &nreq_blocks);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Getting file request block ranges failed : exp inc vars with 2 reqs each : test_misc_file_req_blocks, ret = %d\n", ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  if(nreq_blocks != enreq_blocks){
    LOG_RANK0(rank, "Error while calculating block ranges (expected = %d, returned=%d) : exp inc vars with 2 reqs each : test_misc_file_req_blocks, ret = %d\n", enreq_blocks, nreq_blocks, ret);
    free(reqs);
    free(req_block_ranges);
    test_teardown(&iosys, &file);
    return PIO_EINTERNAL;
  }

  assert(nreq_blocks == 4);
  for(int i = 0; i < 2 * nreq_blocks; i++){
    if(req_block_ranges[i] != ereq_four_block_ranges[i]){
      LOG_RANK0(rank, "Error: Incorrect block range returned, block range index %d = %d (expected %d)\n", i, req_block_ranges[i], ereq_four_block_ranges[i]);
      LOG_RANK0(rank, "Block ranges returned : \n");
      for(int j = 0; j < nreq_blocks; j++){
        LOG_RANK0(rank, "[%d, %d], ",
          req_block_ranges[j], req_block_ranges[j + nreq_blocks]);
      }
      LOG_RANK0(rank, "\n");
      LOG_RANK0(rank, "Block ranges expected : \n");
      for(int j = 0; j < nreq_blocks; j++){
        LOG_RANK0(rank, "[%d, %d], ",
          ereq_four_block_ranges[j], ereq_four_block_ranges[j + nreq_blocks]);
      }
      LOG_RANK0(rank, "\n");

      free(reqs);
      free(req_block_ranges);
      test_teardown(&iosys, &file);
      return PIO_EINTERNAL;
    }
  }

  free(reqs);
  free(req_block_ranges);

  /*******************  Test 4 *************************/
  /* N variables, with rank based stride, displacement 1 and
   * with 3 requests each within req limit
   * MAX_REQ_SZ/2 variables with 2 reqs of size rank each, stride 2
   * MAX_REQ_SZ/4 variables with 2 reqs of size rank each, stride 3
   * MAX_REQ_SZ/8 variables with 2 reqs of size rank each, stride 4
   * MAX_REQ_SZ/16 variables with 2 reqs of size rank each, stride 5
   */
  ret = reinit_file_varlist(file);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Re-initializing file varlist failed for test_misc_file_req_blocks, 2 vars disp 1 stride 2 with 3 reqs each within req limit, ret = %d\n", ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  PIO_Offset rank_req_sz = (rank+1 <= MAX_REQ_SZ/2) ? rank+1 : MAX_REQ_SZ/2;
  PIO_Offset max_rank_req_sz = 0;
  for(int i = 0; i < sz; i++){
    int irank_req_sz = (i+1 <= MAX_REQ_SZ/2) ? i+1 : MAX_REQ_SZ/2;
    if(irank_req_sz > max_rank_req_sz){
      max_rank_req_sz = irank_req_sz;
    }
  }
  disp = 0;
  enreq_blocks = 1;
  PIO_Offset req_sum = 0;
  for(int nvars = MAX_REQ_SZ/2, stride = 2; nvars > 0;
      nvars /= 2, rank_req_sz *= 2, max_rank_req_sz *= 2, stride += 1){
    two_req_sz[0] = rank_req_sz;
    two_req_sz[1] = rank_req_sz;

    for(int j = 0; j < nvars * 2; j++){
      req_sum += max_rank_req_sz;
      if(req_sum > MAX_REQ_SZ){
        req_sum = max_rank_req_sz;
        enreq_blocks++;
      }
    }

    ret = update_file_varlist(file, nvars, disp, stride, 2, two_req_sz, 2);
    if(ret != PIO_NOERR){
      LOG_RANK0(rank, "Updating file varlist failed for test_misc_file_req_blocks, exp inc vars with 2 reqs with size based on rank, within req limit, ret = %d\n", ret);
      test_teardown(&iosys, &file);
      return ret;
    }

    disp += (nvars * stride);
  }

  ret = set_file_req_block_size_limit(file, MAX_REQ_SZ);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Setting file request block size limit (to %llu bytes) failed : exp inc vars with 2 reqs with size based on rank : test_misc_file_req_blocks, ret = %d\n", (unsigned long long) MAX_REQ_SZ, ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  reqs = NULL;
  nreqs = 0;
  nvars_with_reqs = 0;
  last_var_with_req = 0;
  req_block_ranges = NULL;
  nreq_blocks = 0;
  ret = get_file_req_blocks(file,
                            &reqs, &nreqs,
                            &nvars_with_reqs, &last_var_with_req,
                            &req_block_ranges, &nreq_blocks);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Getting file request block ranges failed : exp inc vars with 2 reqs with size based on rank : test_misc_file_req_blocks, ret = %d\n", ret);
    test_teardown(&iosys, &file);
    return ret;
  }

  if(nreq_blocks != enreq_blocks){
    LOG_RANK0(rank, "Error while calculating block ranges (expected = %d, returned=%d) : exp inc vars with 2 reqs with size based on rank : test_misc_file_req_blocks, ret = %d\n", enreq_blocks, nreq_blocks, ret);
    free(reqs);
    free(req_block_ranges);
    test_teardown(&iosys, &file);
    return PIO_EINTERNAL;
  }

  /* FIXME: Check the value of the block ranges */

  /*
  LOG_RANK0(rank, "Block ranges returned : \n");
  for(int i = 0; i < nreq_blocks; i++){
    LOG_RANK0(rank, "[%d, %d], ",
      req_block_ranges[i], req_block_ranges[i + nreq_blocks]);
  }
  LOG_RANK0(rank, "\n");
  */

  free(reqs);
  free(req_block_ranges);

  ret = test_teardown(&iosys, &file);
  if(ret != PIO_NOERR){
    LOG_RANK0(rank, "Finalizing/teardown of test failed for test_misc_file_req_blocks, ret = %d\n", ret);
    return ret;
  }

  return ret;
}

int test_invalid_file_req_blocks(MPI_Comm comm, int rank, int sz)
{
  int ret = PIO_NOERR;

  return ret;
}

int test_driver(MPI_Comm comm, int wrank, int wsz, int *num_errors)
{
    int nerrs = 0, ret = PIO_NOERR, tmp_ret = PIO_NOERR, mpierr = MPI_SUCCESS;
    assert((comm != MPI_COMM_NULL) && (wrank >= 0) && (wsz > 0) && num_errors);
    
    /* Test simple file request blocks */
    tmp_ret = test_simple_file_req_blocks(comm, wrank, wsz);
    mpierr = MPI_Reduce(&tmp_ret, &ret, 1, MPI_INT, MPI_MIN, 0, comm);
    assert(mpierr == MPI_SUCCESS);
    if(ret != PIO_NOERR){
        LOG_RANK0(wrank, "test_simple_file_req_blocks() FAILED, ret = %d\n", ret);
        nerrs++;
    }
    else{
        LOG_RANK0(wrank, "test_simple_file_req_blocks() PASSED\n");
    }

    tmp_ret = test_misc_file_req_blocks(comm, wrank, wsz);
    mpierr = MPI_Reduce(&tmp_ret, &ret, 1, MPI_INT, MPI_MIN, 0, comm);
    assert(mpierr == MPI_SUCCESS);
    if(ret != PIO_NOERR){
        LOG_RANK0(wrank, "test_misc_file_req_blocks() FAILED, ret = %d\n", ret);
        nerrs++;
    }
    else{
        LOG_RANK0(wrank, "test_misc_file_req_blocks() PASSED\n");
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
