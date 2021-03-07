#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cassert>

extern "C"{
#include "pio_config.h"
#include "pio.h"
#include "pio_tests.h"
}
#include "spio_tree.hpp"

#define LOG_RANK0(rank, ...)                     \
            do{                                   \
                if(rank == 0)                     \
                {                                 \
                    fprintf(stderr, __VA_ARGS__); \
                }                                 \
            }while(0);

static const int FAIL = -1;

template<typename T>
class SPIO_tree_node_counter : public PIO_Util::SPIO_tree_visitor<T>{
  public:
    SPIO_tree_node_counter() : nnodes_(0) {}
    void enter_node(T &val, int val_id) { nnodes_++; }
    void enter_node(T &val, int val_id,
                    T &parent_val, int parent_id) { nnodes_++; }
    int count(void ) const { return nnodes_; }
  private:
    int nnodes_;
};

template<typename T>
class SPIO_tree_node_validator : public PIO_Util::SPIO_tree_visitor<T>{
  public:
    SPIO_tree_node_validator(const std::vector<T> &expected_vals)
      : evals_(expected_vals), evals_idx_(0), is_valid_(true) {}
    void enter_node(T &val, int val_id)
    {
      if(!is_valid_){
        return;
      }

      if(evals_idx_ >= evals_.size()){
        is_valid_ = false;
        return;
      }

      if(val != evals_[evals_idx_]){
        is_valid_ = false;
        return;
      }

      evals_idx_++;
    }
    void enter_node(T &val, int val_id,
                    T &parent_val, int parent_id) { enter_node(val, val_id); }
    bool is_valid(void ) const { return is_valid_; }
  private:
    std::vector<T> evals_;
    std::size_t evals_idx_;
    bool is_valid_;
};

/* Test creatng a NULL tree */
int test_null_tree(int wrank)
{
  PIO_Util::SPIO_tree<int> null_tree;
  SPIO_tree_node_counter<int> node_counter;

  null_tree.dfs(node_counter);

  int nnodes = node_counter.count();
  if(nnodes != 0){
    LOG_RANK0(wrank, "test_null_tree failed : Found %d nodes, expected 0 nodes\n", nnodes);
    return PIO_EINTERNAL;
  }

  return PIO_NOERR;
}

/* Test a tree with a single node */
int test_single_node_tree(int wrank)
{
  PIO_Util::SPIO_tree<int> snode_tree;
  SPIO_tree_node_counter<int> node_counter;
  int val = 1;

  snode_tree.add(val);

  snode_tree.dfs(node_counter);

  int nnodes = node_counter.count();
  if(nnodes != 1){
    LOG_RANK0(wrank, "test_single_node_tree failed : Found %d nodes, expected 1 nodes\n", nnodes);
    return PIO_EINTERNAL;
  }

  std::vector<int> expected_vals = { val };
  SPIO_tree_node_validator<int> node_validator(expected_vals);

  snode_tree.dfs(node_validator);

  bool is_valid = node_validator.is_valid();
  if(!is_valid){
    LOG_RANK0(wrank, "test_single_node_tree failed : Nodes were not found in the expected order\n");
    return PIO_EINTERNAL;
  }

  return PIO_NOERR;
}

/* Test a fat tree with one root and all other nodes as the children of the root */
int test_multi_node_fat_tree(int wrank, int nnodes)
{
  PIO_Util::SPIO_tree<int> mnode_tree;
  SPIO_tree_node_counter<int> node_counter;
  std::vector<int> expected_vals;
  int val = 1;

  assert(nnodes > 0);

  expected_vals.push_back(val);
  int root_id = mnode_tree.add(val++);
  for(int i = 0; i < nnodes - 1; i++){
    expected_vals.push_back(val);
    mnode_tree.add(val++, root_id);
  }

  mnode_tree.dfs(node_counter);

  int nnodes_in_tree = node_counter.count();
  if(nnodes_in_tree != nnodes){
    LOG_RANK0(wrank, "test_multi_node_fat_tree failed : Found %d nodes, expected %d nodes\n",
              nnodes_in_tree, nnodes);
    return PIO_EINTERNAL;
  }

  SPIO_tree_node_validator<int> node_validator(expected_vals);

  mnode_tree.dfs(node_validator);

  bool is_valid = node_validator.is_valid();
  if(!is_valid){
    LOG_RANK0(wrank, "test_multi_node_fat_tree failed : Nodes were not found in the expected order\n");
    return PIO_EINTERNAL;
  }

  return PIO_NOERR;
}

/* Test tree where all nodes are skewed left, essentially forming
 * a linked list of the nodes
 */
int test_multi_node_skewed_tree(int wrank, int nnodes)
{
  PIO_Util::SPIO_tree<int> mnode_tree;
  SPIO_tree_node_counter<int> node_counter;
  std::vector<int> expected_vals;
  int val = 1;

  assert(nnodes > 0);

  expected_vals.push_back(val);
  int parent_id = mnode_tree.add(val++);
  for(int i = 0; i < nnodes - 1; i++){
    expected_vals.push_back(val);
    parent_id = mnode_tree.add(val++, parent_id);
  }

  mnode_tree.dfs(node_counter);

  int nnodes_in_tree = node_counter.count();
  if(nnodes_in_tree != nnodes){
    LOG_RANK0(wrank, "test_multi_node_skewed_tree failed : Found %d nodes, expected %d nodes\n",
              nnodes_in_tree, nnodes);
    return PIO_EINTERNAL;
  }


  SPIO_tree_node_validator<int> node_validator(expected_vals);

  mnode_tree.dfs(node_validator);

  bool is_valid = node_validator.is_valid();
  if(!is_valid){
    LOG_RANK0(wrank, "test_multi_node_skewed_tree failed : Nodes were not found in the expected order\n");
    return PIO_EINTERNAL;
  }

  return PIO_NOERR;
}

/* Test a balanced ternary tree */
int test_multi_node_balanced_tree(int wrank, int nnodes)
{
  PIO_Util::SPIO_tree<int> mnode_tree;
  SPIO_tree_node_counter<int> node_counter;
  const int MAX_CHILDREN = 3;
  int val = 1;

  assert(nnodes > 0);

  std::vector<int> parent_ids;
  int root_id = mnode_tree.add(val++);
  parent_ids.push_back(root_id);
  std::size_t j = 0;
  for(int i = 1; i < nnodes; i++){
    assert(j < parent_ids.size());
    int node_id = mnode_tree.add(val++, parent_ids[j]);
    parent_ids.push_back(node_id);
    if(i % MAX_CHILDREN == 0){
      j++;
    }
  }

  mnode_tree.dfs(node_counter);

  int nnodes_in_tree = node_counter.count();
  if(nnodes_in_tree != nnodes){
    LOG_RANK0(wrank, "test_multi_node_balanced_tree failed : Found %d nodes, expected %d nodes\n",
              nnodes_in_tree, nnodes);
    return PIO_EINTERNAL;
  }

  return PIO_NOERR;
}

/* Test a tree with user defined structs as values stored in the nodes */
class User_node{
  public:
    User_node() : val_(0), name_("") {}
    User_node(int val, const std::string &name) : val_(val), name_(name) {}
    int get_val(void ) const { return val_; }
    std::string get_name(void ) const { return name_; }
  private:
    int val_;
    std::string name_;
};

int test_user_struct_tree(int wrank)
{
  PIO_Util::SPIO_tree<User_node> mnode_tree;
  SPIO_tree_node_counter<User_node> node_counter;
  const int NNODES = 13;
  const int MAX_CHILDREN = 3;
  std::string name_prefix("node");
  int val = 0;

  std::vector<int> parent_ids;
  int root_id = mnode_tree.add(User_node(val, name_prefix + std::to_string(val)));
  parent_ids.push_back(root_id);
  std::size_t j = 0;
  for(int i = 1; i < NNODES; i++){
    assert(j < parent_ids.size());
    int node_id = mnode_tree.add(User_node(i, name_prefix + std::to_string(i)), parent_ids[j]);
    parent_ids.push_back(node_id);
    if(i % MAX_CHILDREN == 0){
      j++;
    }
  }

  mnode_tree.dfs(node_counter);

  int nnodes_in_tree = node_counter.count();
  if(nnodes_in_tree != NNODES){
    LOG_RANK0(wrank, "test_multi_node_balanced_tree failed : Found %d nodes, expected %d nodes\n",
              nnodes_in_tree, NNODES);
    return PIO_EINTERNAL;
  }

  return PIO_NOERR;
}

int test_driver(MPI_Comm comm, int wrank, int wsz, int *num_errors)
{
  int nerrs = 0, ret = PIO_NOERR;
  assert((comm != MPI_COMM_NULL) && (wrank >= 0) && (wsz > 0) && num_errors);
  
  /* Test creating a tree with no nodes */
  try{
    ret = test_null_tree(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_null_tree() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_null_tree() PASSED\n");
  }

  /* Test creating a tree with one nodes */
  try{
    ret = test_single_node_tree(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_single_node_tree() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_single_node_tree() PASSED\n");
  }

  /* Test creating a tree with multiple nodes */
  const int MIN_NODES = 2;
  const int MAX_NODES = 14;
  for(int nnodes = MIN_NODES; nnodes < MAX_NODES; nnodes++){
    try{
      ret = test_multi_node_fat_tree(wrank, nnodes);
    }
    catch(...){
      ret = PIO_EINTERNAL;
    }
    if(ret != PIO_NOERR){
      LOG_RANK0(wrank, "test_multi_node_fat_tree(nnodes = %d) FAILED, ret = %d\n", nnodes, ret);
      nerrs++;
    }
    else{
      LOG_RANK0(wrank, "test_multi_node_fat_tree(nnodes = %d) PASSED\n", nnodes);
    }

    try{
      ret = test_multi_node_skewed_tree(wrank, nnodes);
    }
    catch(...){
      ret = PIO_EINTERNAL;
    }
    if(ret != PIO_NOERR){
      LOG_RANK0(wrank, "test_multi_node_skewed_tree(nnodes = %d) FAILED, ret = %d\n", nnodes, ret);
      nerrs++;
    }
    else{
      LOG_RANK0(wrank, "test_multi_node_skewed_tree(nnodes = %d) PASSED\n", nnodes);
    }

    try{
      ret = test_multi_node_balanced_tree(wrank, nnodes);
    }
    catch(...){
      ret = PIO_EINTERNAL;
    }
    if(ret != PIO_NOERR){
      LOG_RANK0(wrank, "test_multi_node_balanced_tree(nnodes = %d) FAILED, ret = %d\n", nnodes, ret);
      nerrs++;
    }
    else{
      LOG_RANK0(wrank, "test_multi_node_balanced_tree(nnodes = %d) PASSED\n", nnodes);
    }
  }

  /* Test creating a tree with each node containing a user struct */
  try{
    ret = test_user_struct_tree(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_user_struct_tree() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_user_struct_tree() PASSED\n");
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
