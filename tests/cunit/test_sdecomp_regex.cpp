#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cassert>

#include "pio_sdecomps_regex.hpp"
extern "C"{
#include <pio_tests.h>
}

#define LOG_RANK0(rank, ...)                     \
            do{                                   \
                if(rank == 0)                     \
                {                                 \
                    fprintf(stderr, __VA_ARGS__); \
                }                                 \
            }while(0);

static const int FAIL = -1;

/* Test creatng the regular expression class */
int test_create_sdecomp_regex(void )
{
    PIO_Util::PIO_save_decomp_regex test_regex("*");

    return PIO_NOERR;
}

/* Test creatng a match all regular expression */
int test_matchall_regex(int wrank)
{
    int ret = PIO_NOERR;
    PIO_Util::PIO_save_decomp_regex test_regex("*");
    std::vector<int> ioids = {-2, -1, 0, 1, 2, 3, 4, 99, 100, 1024, 4096};
    std::vector<std::string> vnames = {
                                          std::string("test_var1"),
                                          std::string("test_var2")
                                      };
    std::vector<std::string> fnames = {
                                          std::string("test_file1"),
                                          std::string("test_file2")
                                      };

    for(std::vector<int>::const_iterator id_iter = ioids.cbegin();
        id_iter != ioids.cend(); ++id_iter){
      for(std::vector<std::string>::const_iterator var_iter = vnames.cbegin();
          var_iter != vnames.cend(); ++var_iter){
        for(std::vector<std::string>::const_iterator file_iter = fnames.cbegin();
          file_iter != fnames.cend(); ++file_iter){
          bool is_match = test_regex.matches(*id_iter, *file_iter, *var_iter);
          if(!is_match){
            LOG_RANK0(wrank, "test_matchall_regex failed for : ioid=%d, fname=%s, vname=%s\n", *id_iter, (*file_iter).c_str(), (*var_iter).c_str());
            ret = PIO_EINTERNAL;
            break;
          }
        }
      }
    }

    return ret;
}

/* Test creatng a regular expression to match ids */
int test_idmatch_regex(int wrank)
{
    int ret = PIO_NOERR;
    const std::string ID_REG_PREFIX("ID=");
    const std::string QUOTE("\"");
    const std::string LEFT_BRACKET("(");
    const std::string RIGHT_BRACKET(")");
    const int MATCH_ID = 99;
    PIO_Util::PIO_save_decomp_regex 
      test_regex(ID_REG_PREFIX +
        QUOTE +
        std::to_string(MATCH_ID) +
        QUOTE);
    std::vector<int> ioids = {-2, -1, 0, 1, 2, 3, 4, MATCH_ID, 100, 1024, 4096};
    std::vector<std::string> vnames = {
                                          std::string("test_var1"),
                                          std::string("test_var2")
                                      };
    std::vector<std::string> fnames = {
                                          std::string("test_file1"),
                                          std::string("test_file2")
                                      };

    for(std::vector<int>::const_iterator id_iter = ioids.cbegin();
        id_iter != ioids.cend(); ++id_iter){
      for(std::vector<std::string>::const_iterator var_iter = vnames.cbegin();
          var_iter != vnames.cend(); ++var_iter){
        for(std::vector<std::string>::const_iterator file_iter = fnames.cbegin();
          file_iter != fnames.cend(); ++file_iter){
          bool is_match = test_regex.matches(*id_iter, *file_iter, *var_iter);
          if( ((!is_match) && (*id_iter == MATCH_ID)) ||
              (is_match && (*id_iter != MATCH_ID)) ){
            LOG_RANK0(wrank, "test_idmatch_regex failed for : ioid=%d, fname=%s, vname=%s\n", *id_iter, (*file_iter).c_str(), (*var_iter).c_str());
            ret = PIO_EINTERNAL;
            break;
          }
        }
      }
    }

    return ret;
}

/* Test creatng a regular expression to match vars */
int test_vmatch_regex(int wrank)
{
    int ret = PIO_NOERR;
    const std::string VAR_REG_PREFIX("VAR=");
    const std::string QUOTE("\"");
    std::vector<int> ioids = {-2, -1, 0, 1, 2, 3, 4, 99, 100, 1024, 4096};
    const std::string VNAME_TO_MATCH("test_var2");
    const std::string VNAME_REGEX(".*_var2");
    std::vector<std::string> vnames = {
                                          std::string("test_var1"),
                                          VNAME_TO_MATCH,
                                          std::string("test_var3"),
                                          std::string("test_var4")
                                      };
    PIO_Util::PIO_save_decomp_regex
      test_regex(VAR_REG_PREFIX +
        QUOTE +
        VNAME_REGEX +
        QUOTE);
    std::vector<std::string> fnames = {
                                          std::string("test_file1"),
                                          std::string("test_file2")
                                      };

    for(std::vector<int>::const_iterator id_iter = ioids.cbegin();
        id_iter != ioids.cend(); ++id_iter){
      for(std::vector<std::string>::const_iterator var_iter = vnames.cbegin();
          var_iter != vnames.cend(); ++var_iter){
        for(std::vector<std::string>::const_iterator file_iter = fnames.cbegin();
          file_iter != fnames.cend(); ++file_iter){
          bool is_match = test_regex.matches(*id_iter, *file_iter, *var_iter);
          if( ((!is_match) && (*var_iter == VNAME_TO_MATCH)) ||
              (is_match && (*var_iter != VNAME_TO_MATCH)) ){
            LOG_RANK0(wrank, "test_vmatch_regex failed for : ioid=%d, fname=%s, vname=%s\n", *id_iter, (*file_iter).c_str(), (*var_iter).c_str());
            ret = PIO_EINTERNAL;
            break;
          }
        }
      }
    }

    return ret;
}

/* Test creatng a regular expression to match files */
int test_fmatch_regex(int wrank)
{
    int ret = PIO_NOERR;
    const std::string FILE_REG_PREFIX("FILE=");
    const std::string QUOTE("\"");
    std::vector<int> ioids = {-2, -1, 0, 1, 2, 3, 4, 99, 100, 1024, 4096};
    const std::string FNAME_TO_MATCH("test_file1");
    const std::string FNAME_REGEX(".*_file1");
    std::vector<std::string> vnames = {
                                          std::string("test_var1"),
                                          std::string("test_var2"),
                                          std::string("test_var3")
                                      };
    PIO_Util::PIO_save_decomp_regex
      test_regex(FILE_REG_PREFIX +
        QUOTE +
        FNAME_REGEX +
        QUOTE);
    std::vector<std::string> fnames = {
                                          std::string("test_file1"),
                                          FNAME_TO_MATCH,
                                          std::string("test_file3")
                                      };

    for(std::vector<int>::const_iterator id_iter = ioids.cbegin();
        id_iter != ioids.cend(); ++id_iter){
      for(std::vector<std::string>::const_iterator var_iter = vnames.cbegin();
          var_iter != vnames.cend(); ++var_iter){
        for(std::vector<std::string>::const_iterator file_iter = fnames.cbegin();
          file_iter != fnames.cend(); ++file_iter){
          bool is_match = test_regex.matches(*id_iter, *file_iter, *var_iter);
          if( ((!is_match) && (*file_iter == FNAME_TO_MATCH)) ||
              (is_match && (*file_iter != FNAME_TO_MATCH)) ){
            LOG_RANK0(wrank, "test_fmatch_regex failed for : ioid=%d, fname=%s, vname=%s\n", *id_iter, (*file_iter).c_str(), (*var_iter).c_str());
            ret = PIO_EINTERNAL;
            break;
          }
        }
      }
    }

    return ret;
}

/* Test a regular expression to match a specfic ioid, variable and file name */
int test_match_regex(int wrank)
{
    int ret = PIO_NOERR;
    const std::string QUOTE("\"");
    const std::string LEFT_BRACKET("(");
    const std::string RIGHT_BRACKET(")");
    const std::string LOGICAL_AND("&&");

    const std::string ID_REG_PREFIX("ID=");
    const std::string VAR_REG_PREFIX("VAR=");
    const std::string FILE_REG_PREFIX("FILE=");

    const int ID_TO_MATCH = 99;
    const std::string VNAME_TO_MATCH("test_var2");
    const std::string VNAME_REGEX(".*_var2");
    const std::string FNAME_TO_MATCH("test_file3");
    const std::string FNAME_REGEX(".*_file3");

    std::vector<int> ioids = {-2, -1, 0, 1, 2, 3, 4, ID_TO_MATCH, 100, 1024, 4096};
    std::vector<std::string> vnames = {
                                          std::string("test_var1"),
                                          VNAME_TO_MATCH,
                                          std::string("test_var3")
                                      };
    PIO_Util::PIO_save_decomp_regex
      test_regex(
        LEFT_BRACKET +
          ID_REG_PREFIX + QUOTE + std::to_string(ID_TO_MATCH) + QUOTE +
        RIGHT_BRACKET +
        LOGICAL_AND +
        LEFT_BRACKET +
          FILE_REG_PREFIX + QUOTE + FNAME_REGEX + QUOTE +
        RIGHT_BRACKET +
        LOGICAL_AND +
        LEFT_BRACKET +
          VAR_REG_PREFIX + QUOTE + VNAME_REGEX + QUOTE +
        RIGHT_BRACKET
        );
    std::vector<std::string> fnames = {
                                          std::string("test_file1"),
                                          std::string("test_file2"),
                                          FNAME_TO_MATCH,
                                          std::string("test_file4")
                                      };

    for(std::vector<int>::const_iterator id_iter = ioids.cbegin();
        id_iter != ioids.cend(); ++id_iter){
      for(std::vector<std::string>::const_iterator var_iter = vnames.cbegin();
          var_iter != vnames.cend(); ++var_iter){
        for(std::vector<std::string>::const_iterator file_iter = fnames.cbegin();
          file_iter != fnames.cend(); ++file_iter){
          bool is_match = test_regex.matches(*id_iter, *file_iter, *var_iter);
          if( (
                (!is_match) &&
                (*id_iter == ID_TO_MATCH) &&
                (*var_iter == VNAME_TO_MATCH) &&
                (*file_iter == FNAME_TO_MATCH)
              ) ||
              (
                (is_match) &&
                ( (*id_iter != ID_TO_MATCH) ||
                  (*var_iter != VNAME_TO_MATCH) ||
                  (*file_iter != FNAME_TO_MATCH)
                )
              ) ){
            LOG_RANK0(wrank, "test_fmatch_regex failed for : ioid=%d, fname=%s, vname=%s\n", *id_iter, (*file_iter).c_str(), (*var_iter).c_str());
            ret = PIO_EINTERNAL;
            break;
          }
        }
      }
    }

    return ret;
}

/* Test a regular expression to match a specfic variable and file name */
int test_match_regex2(int wrank)
{
    int ret = PIO_NOERR;
    const std::string QUOTE("\"");
    const std::string LEFT_BRACKET("(");
    const std::string RIGHT_BRACKET(")");
    const std::string LOGICAL_AND("&&");
    const std::string LOGICAL_OR("||");

    const std::string ID_REG_PREFIX("ID=");
    const std::string VAR_REG_PREFIX("VAR=");
    const std::string FILE_REG_PREFIX("FILE=");

    const std::string V2_TO_MATCH("test_var2");
    const std::string V2_REGEX(".*_var2");
    const std::string F1_TO_MATCH("test_file1");
    const std::string F1_REGEX(".*_file1.*");

    const std::string V3_TO_MATCH("test_var3");
    const std::string V3_REGEX(".*_var3");
    const std::string F4_TO_MATCH("test_file4");
    const std::string F4_REGEX(".*_file4.*");

    std::vector<int> ioids = {-2, -1, 0, 1, 2, 3, 4, 99, 100, 1024, 4096};
    std::vector<std::string> vnames = {
                                          std::string("test_var1"),
                                          V2_TO_MATCH,
                                          V3_TO_MATCH,
                                          std::string("test_var4")
                                      };
    std::vector<std::string> fnames = {
                                          F1_TO_MATCH,
                                          std::string("test_file2"),
                                          std::string("test_file3"),
                                          F4_TO_MATCH
                                      };

    /* Match V2 in F1 and V3 in F4 */
    PIO_Util::PIO_save_decomp_regex
      test_regex(
        LEFT_BRACKET +
          LEFT_BRACKET +
            FILE_REG_PREFIX + QUOTE + F1_REGEX + QUOTE +
          RIGHT_BRACKET +
          LOGICAL_AND +
          LEFT_BRACKET +
            VAR_REG_PREFIX + QUOTE + V2_REGEX + QUOTE +
          RIGHT_BRACKET +
        RIGHT_BRACKET +
          LOGICAL_OR +
        LEFT_BRACKET +
          LEFT_BRACKET +
            FILE_REG_PREFIX + QUOTE + F4_REGEX + QUOTE +
          RIGHT_BRACKET +
          LOGICAL_AND +
          LEFT_BRACKET +
            VAR_REG_PREFIX + QUOTE + V3_REGEX + QUOTE +
          RIGHT_BRACKET +
        RIGHT_BRACKET
        );
    for(std::vector<int>::const_iterator id_iter = ioids.cbegin();
        id_iter != ioids.cend(); ++id_iter){
      for(std::vector<std::string>::const_iterator var_iter = vnames.cbegin();
          var_iter != vnames.cend(); ++var_iter){
        for(std::vector<std::string>::const_iterator file_iter = fnames.cbegin();
          file_iter != fnames.cend(); ++file_iter){
          bool is_match = test_regex.matches(*id_iter, *file_iter, *var_iter);
          bool exp_match = 
                          (
                            (*var_iter == V2_TO_MATCH) &&
                            (*file_iter == F1_TO_MATCH)
                          ) ||
                          (
                            (*var_iter == V3_TO_MATCH) &&
                            (*file_iter == F4_TO_MATCH)
                          );
          if((!is_match && exp_match) || (is_match && !exp_match)){
            LOG_RANK0(wrank, "test_fmatch_regex failed for : ioid=%d, fname=%s, vname=%s\n", *id_iter, (*file_iter).c_str(), (*var_iter).c_str());
            ret = PIO_EINTERNAL;
            break;
          }
        }
      }
    }

    return ret;
}

int test_driver(MPI_Comm comm, int wrank, int wsz, int *num_errors)
{
    int nerrs = 0, ret = PIO_NOERR;
    assert((comm != MPI_COMM_NULL) && (wrank >= 0) && (wsz > 0) && num_errors);
    
    /* Test creating a regular expression */
    try{
      ret = test_create_sdecomp_regex();
    }
    catch(...){
      ret = PIO_EINTERNAL;
    }
    if(ret != PIO_NOERR)
    {
        LOG_RANK0(wrank, "test_create_sdecomp_regex() FAILED, ret = %d\n", ret);
        nerrs++;
    }
    else{
        LOG_RANK0(wrank, "test_create_sdecomp_regex() PASSED\n");
    }

    /* Test creating a simple, match all, regular expression */
    try{
      ret = test_matchall_regex(wrank);
    }
    catch(...){
      ret = PIO_EINTERNAL;
    }
    if(ret != PIO_NOERR)
    {
        LOG_RANK0(wrank, "test_matchall_regex() FAILED, ret = %d\n", ret);
        nerrs++;
    }
    else{
        LOG_RANK0(wrank, "test_matchall_regex() PASSED\n");
    }

    /* Test creating a simple regular expression to match a specific ioid */
    try{
      ret = test_idmatch_regex(wrank);
    }
    catch(...){
      ret = PIO_EINTERNAL;
    }
    if(ret != PIO_NOERR)
    {
        LOG_RANK0(wrank, "test_idmatch_regex() FAILED, ret = %d\n", ret);
        nerrs++;
    }
    else{
        LOG_RANK0(wrank, "test_idmatch_regex() PASSED\n");
    }

    /* Test creating a regular expression to match a variable name */
    try{
      ret = test_vmatch_regex(wrank);
    }
    catch(...){
      ret = PIO_EINTERNAL;
    }
    if(ret != PIO_NOERR)
    {
        LOG_RANK0(wrank, "test_vmatch_regex() FAILED, ret = %d\n", ret);
        nerrs++;
    }
    else{
        LOG_RANK0(wrank, "test_vmatch_regex() PASSED\n");
    }

    /* Test creating a regular expression to match a file name */
    try{
      ret = test_fmatch_regex(wrank);
    }
    catch(...){
      ret = PIO_EINTERNAL;
    }
    if(ret != PIO_NOERR)
    {
        LOG_RANK0(wrank, "test_fmatch_regex() FAILED, ret = %d\n", ret);
        nerrs++;
    }
    else{
        LOG_RANK0(wrank, "test_fmatch_regex() PASSED\n");
    }

    /* Test creating a regular expression to match an ioid, variable and file name */
    try{
      ret = test_match_regex(wrank);
    }
    catch(...){
      ret = PIO_EINTERNAL;
    }
    if(ret != PIO_NOERR)
    {
        LOG_RANK0(wrank, "test_match_regex() FAILED, ret = %d\n", ret);
        nerrs++;
    }
    else{
        LOG_RANK0(wrank, "test_match_regex() PASSED\n");
    }

    /* Test creating a regular expression to match a variable and file name */
    try{
      ret = test_match_regex2(wrank);
    }
    catch(...){
      ret = PIO_EINTERNAL;
    }
    if(ret != PIO_NOERR)
    {
        LOG_RANK0(wrank, "test_match_regex2() FAILED, ret = %d\n", ret);
        nerrs++;
    }
    else{
        LOG_RANK0(wrank, "test_match_regex2() PASSED\n");
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
    if(ret != 0)
    {
        LOG_RANK0(wrank, "GPTLinitialize() FAILED, ret = %d\n", ret);
        return ret;
    }
#endif /* TIMING_INTERNAL */
#endif /* TIMING */

    ret = MPI_Init(&argc, &argv);
    if(ret != MPI_SUCCESS)
    {
        LOG_RANK0(wrank, "MPI_Init() FAILED, ret = %d\n", ret);
        return ret;
    }

    ret = MPI_Comm_rank(MPI_COMM_WORLD, &wrank);
    if(ret != MPI_SUCCESS)
    {
        LOG_RANK0(wrank, "MPI_Comm_rank() FAILED, ret = %d\n", ret);
        return ret;
    }
    ret = MPI_Comm_size(MPI_COMM_WORLD, &wsz);
    if(ret != MPI_SUCCESS)
    {
        LOG_RANK0(wrank, "MPI_Comm_rank() FAILED, ret = %d\n", ret);
        return ret;
    }

    num_errors = 0;
    ret = test_driver(MPI_COMM_WORLD, wrank, wsz, &num_errors);
    if(ret != 0)
    {
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
    if(ret != 0)
    {
        LOG_RANK0(wrank, "GPTLinitialize() FAILED, ret = %d\n", ret);
        return ret;
    }
#endif /* TIMING_INTERNAL */
#endif /* TIMING */

    if(num_errors != 0)
    {
        LOG_RANK0(wrank, "Total errors = %d\n", num_errors);
        return FAIL;
    }
    return 0;
}
