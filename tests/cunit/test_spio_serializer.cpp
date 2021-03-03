#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include <utility>
#include <cassert>
#include <cctype>

extern "C"{
#include "pio_config.h"
#include "pio.h"
#include "pio_tests.h"
}
#include "spio_serializer.hpp"

#define LOG_RANK0(rank, ...)                     \
            do{                                   \
                if(rank == 0)                     \
                {                                 \
                    fprintf(stderr, __VA_ARGS__); \
                }                                 \
            }while(0);

static const int FAIL = -1;

namespace Utils{

static std::string &rem_blank_str(std::string &str)
{
  str.erase(std::remove_if(str.begin(), str.end(),
              [](unsigned char x)
                {return std::isblank(x);}),
            str.end());

  return str;
}

static std::string quoted_str(const std::string &str)
{
  return (std::string("\"") + str + std::string("\""));
}

} // namespace Utils

/* Test serializing simple types */
int test_simple_types(int wrank)
{
  /* Add a string, int and double */
  std::string name_tag("name");
  std::string name("helloworld");
  std::string ival_tag("ival");
  int ival = 3;
  std::string dval_tag("dval");
  double dval = 3.14;
  const std::string ID_SEP(":");
  const std::string NEWLINE("\n");

  std::string ser_tag("SerializedVals");

  /*
   * ============ Expected Serialized Text ===========
   * "SerializedVals":
   *  "name" : "helloworld"
   *  "ival" : 3
   *  "dval" : 3.14
   */
  std::string exp_ser_txt =
    Utils::quoted_str(ser_tag) + ID_SEP + NEWLINE +
    Utils::quoted_str(name_tag) + ID_SEP + Utils::quoted_str(name) + NEWLINE +
    Utils::quoted_str(ival_tag) + ID_SEP + std::to_string(ival) + NEWLINE +
    Utils::quoted_str(dval_tag) + ID_SEP + std::to_string(dval) + NEWLINE;

  /* Pack the user values into the vector of vals passed to the serializer */
  std::vector<std::pair<std::string, std::string> > vals;
  PIO_Util::Serializer_Utils::serialize_pack(name_tag, name, vals);
  PIO_Util::Serializer_Utils::serialize_pack(ival_tag, ival, vals);
  PIO_Util::Serializer_Utils::serialize_pack(dval_tag, dval, vals);

  /* Create a text serializer */
  std::unique_ptr<PIO_Util::SPIO_serializer> spio_text_ser =
    PIO_Util::Serializer_Utils::create_serializer(
      PIO_Util::Serializer_type::TEXT_SERIALIZER, "test_simple_types.txt");

  /* Serialize the vals, sync and retrieve the serialized data */
  spio_text_ser->serialize(ser_tag, vals);
  spio_text_ser->sync();
  std::string serialized_txt = spio_text_ser->get_serialized_data();

  if(Utils::rem_blank_str(serialized_txt) != Utils::rem_blank_str(exp_ser_txt)){
    LOG_RANK0(wrank, "test_simple_types() FAILED\n");
    LOG_RANK0(wrank, "Serialized text : \n");
    LOG_RANK0(wrank, "%s\n", serialized_txt.c_str());
    LOG_RANK0(wrank, "Expected serialized text : \n");
    LOG_RANK0(wrank, "%s\n", exp_ser_txt.c_str());

    return PIO_EINTERNAL;
  }

  LOG_RANK0(wrank, "Testing TEXT serializer PASSED\n");

  /*
   * ============ Expected Serialized JSON ===========
   *{
   * "SerializedVals":{
   *    "name" : "helloworld"
   *    "ival" : 3
   *    "dval" : 3.14
   *  }
   *}
   */
  const std::string JSON_OBJECT_START("{");
  const std::string JSON_OBJECT_END("}");
  std::string exp_ser_json =
    JSON_OBJECT_START + NEWLINE +
      Utils::quoted_str(ser_tag) + ID_SEP + JSON_OBJECT_START + NEWLINE +
        Utils::quoted_str(name_tag) + ID_SEP + Utils::quoted_str(name) + NEWLINE +
        Utils::quoted_str(ival_tag) + ID_SEP + std::to_string(ival) + NEWLINE +
        Utils::quoted_str(dval_tag) + ID_SEP + std::to_string(dval) + NEWLINE +
      JSON_OBJECT_END + NEWLINE +
    JSON_OBJECT_END + NEWLINE;

  /* Create a JSON serializer */
  std::unique_ptr<PIO_Util::SPIO_serializer> spio_json_ser =
    PIO_Util::Serializer_Utils::create_serializer(
      PIO_Util::Serializer_type::JSON_SERIALIZER, "test_simple_types.json");

  /* Serialize the vals, sync and retrieve the serialized data */
  spio_json_ser->serialize(ser_tag, vals);
  spio_json_ser->sync();
  std::string serialized_json = spio_json_ser->get_serialized_data();

  if(Utils::rem_blank_str(serialized_json) != Utils::rem_blank_str(exp_ser_json)){
    LOG_RANK0(wrank, "test_simple_types() FAILED\n");
    LOG_RANK0(wrank, "Serialized JSON : \n");
    LOG_RANK0(wrank, "%s\n", serialized_json.c_str());
    LOG_RANK0(wrank, "Expected serialized JSON : \n");
    LOG_RANK0(wrank, "%s\n", exp_ser_json.c_str());

    return PIO_EINTERNAL;
  }

  LOG_RANK0(wrank, "Testing JSON serializer PASSED\n");
  return PIO_NOERR;
}

/* Test serializing array types */
int test_array_types(int wrank)
{
  /* Add a string, int and double */
  std::vector<std::pair<std::string, std::string> > names = {
    {"name", "helloworld1"}, {"name", "helloworld2"}, {"name", "helloworld3"}
  };
  const std::string ID_SEP(":");
  const std::string NEWLINE("\n");

  std::string ser_tag("SerializedVals");

  /*
   * ============ Expected Serialized Text ===========
   * "SerializedVals":
   *  "name" : "helloworld1"
   * "SerializedVals":
   *  "name" : "helloworld2"
   * "SerializedVals":
   *  "name" : "helloworld3"
   */
  std::string exp_ser_txt;
  for(std::vector<std::pair<std::string, std::string> >::const_iterator
        citer = names.cbegin(); citer != names.cend(); ++citer){
    exp_ser_txt +=
      Utils::quoted_str(ser_tag) + ID_SEP + NEWLINE +
        Utils::quoted_str(citer->first) + ID_SEP
        + Utils::quoted_str(citer->second) + NEWLINE;
  }

  /* Pack the user values into the vector of vals passed to the serializer */
  std::vector<std::vector<std::pair<std::string, std::string> > > vvals;
  for(std::vector<std::pair<std::string, std::string> >::const_iterator
        citer = names.cbegin(); citer != names.cend(); ++citer){
    std::vector<std::pair<std::string, std::string> > vals;
    PIO_Util::Serializer_Utils::serialize_pack(citer->first, citer->second, vals);
    vvals.push_back(vals);
  }

  /* Create a text serializer */
  std::unique_ptr<PIO_Util::SPIO_serializer> spio_text_ser =
    PIO_Util::Serializer_Utils::create_serializer(
      PIO_Util::Serializer_type::TEXT_SERIALIZER, "test_array_types.txt");

  /* Serialize the vals, sync and retrieve the serialized data */
  std::vector<int> val_ids;
  spio_text_ser->serialize(ser_tag, vvals, val_ids);
  spio_text_ser->sync();
  std::string serialized_txt = spio_text_ser->get_serialized_data();

  if(Utils::rem_blank_str(serialized_txt) != Utils::rem_blank_str(exp_ser_txt)){
    LOG_RANK0(wrank, "test_array_types() FAILED\n");
    LOG_RANK0(wrank, "Serialized text : \n");
    LOG_RANK0(wrank, "%s\n", serialized_txt.c_str());
    LOG_RANK0(wrank, "Expected serialized text : \n");
    LOG_RANK0(wrank, "%s\n", exp_ser_txt.c_str());

    return PIO_EINTERNAL;
  }

  LOG_RANK0(wrank, "Testing TEXT serializer PASSED\n");

  /*
   * ============ Expected Serialized JSON ===========
   *{
   * "SerializedVals":[
   *    {
   *      "name" : "helloworld1"
   *    },
   *    {
   *      "name" : "helloworld2"
   *    },
   *    {
   *      "name" : "helloworld3"
   *    }
   * ]
   *}
   */
  const std::string JSON_OBJECT_START("{");
  const std::string JSON_ARRAY_START("[");
  const std::string JSON_OBJECT_END("}");
  const std::string JSON_ARRAY_END("]");
  const std::string JSON_ARRAY_ELEMENT_SEP(",");

  std::string exp_ser_json =
    JSON_OBJECT_START + NEWLINE +
      Utils::quoted_str(ser_tag) + ID_SEP + JSON_ARRAY_START + NEWLINE;

  for(std::vector<std::pair<std::string, std::string> >::const_iterator
        citer = names.cbegin(); citer != names.cend(); ++citer){
    exp_ser_json +=
        JSON_OBJECT_START + NEWLINE +
          Utils::quoted_str(citer->first) + ID_SEP + Utils::quoted_str(citer->second) + NEWLINE +
        JSON_OBJECT_END + NEWLINE;
    if(citer + 1 != names.cend()){
      exp_ser_json += JSON_ARRAY_ELEMENT_SEP + NEWLINE;
    }
  }

  exp_ser_json +=
      JSON_ARRAY_END + NEWLINE +
    JSON_OBJECT_END + NEWLINE;

  /* Create a JSON serializer */
  std::unique_ptr<PIO_Util::SPIO_serializer> spio_json_ser =
    PIO_Util::Serializer_Utils::create_serializer(
      PIO_Util::Serializer_type::JSON_SERIALIZER, "test_simple_types.json");

  /* Serialize the vals, sync and retrieve the serialized data */
  spio_json_ser->serialize(ser_tag, vvals, val_ids);
  spio_json_ser->sync();
  std::string serialized_json = spio_json_ser->get_serialized_data();

  if(Utils::rem_blank_str(serialized_json) != Utils::rem_blank_str(exp_ser_json)){
    LOG_RANK0(wrank, "test_array_types() FAILED\n");
    LOG_RANK0(wrank, "Serialized JSON : \n");
    LOG_RANK0(wrank, "%s\n", serialized_json.c_str());
    LOG_RANK0(wrank, "Expected serialized JSON : \n");
    LOG_RANK0(wrank, "%s\n", exp_ser_json.c_str());

    return PIO_EINTERNAL;
  }

  LOG_RANK0(wrank, "Testing JSON serializer PASSED\n");
  return PIO_NOERR;
}

/* Test serializing hierarchical/tiered data */
int test_tiered_data(int wrank)
{
  /* Add a string, int and double */
  std::string name_tag("name");
  std::string name("helloworld");
  std::string ival_tag("ival");
  int ival = 3;
  std::string dval_tag("dval");
  double dval = 3.14;
  const std::string ID_SEP(":");
  const std::string NEWLINE("\n");

  std::string ser_tag_tier1("SerializedValsT1");
  std::string ser_tag_tier2("SerializedValsT2");

  /*
   * ============ Expected Serialized Text ===========
   * "SerializedValsT1":
   *  "SerializedValsT2":
   *    "name" : "helloworld"
   *    "ival" : 3
   *    "dval" : 3.14
   */
  std::string exp_ser_txt =
    Utils::quoted_str(ser_tag_tier1) + ID_SEP + NEWLINE +
    Utils::quoted_str(ser_tag_tier2) + ID_SEP + NEWLINE +
    Utils::quoted_str(name_tag) + ID_SEP + Utils::quoted_str(name) + NEWLINE +
    Utils::quoted_str(ival_tag) + ID_SEP + std::to_string(ival) + NEWLINE +
    Utils::quoted_str(dval_tag) + ID_SEP + std::to_string(dval) + NEWLINE;

  /* Pack the user values into the vector of vals passed to the serializer */
  std::vector<std::pair<std::string, std::string> > empty_vals;
  std::vector<std::pair<std::string, std::string> > vals;
  PIO_Util::Serializer_Utils::serialize_pack(name_tag, name, vals);
  PIO_Util::Serializer_Utils::serialize_pack(ival_tag, ival, vals);
  PIO_Util::Serializer_Utils::serialize_pack(dval_tag, dval, vals);

  /* Create a text serializer */
  std::unique_ptr<PIO_Util::SPIO_serializer> spio_text_ser =
    PIO_Util::Serializer_Utils::create_serializer(
      PIO_Util::Serializer_type::TEXT_SERIALIZER, "test_htypes.txt");

  /* Serialize the vals, sync and retrieve the serialized data */
  int t1_id = spio_text_ser->serialize(ser_tag_tier1, empty_vals);
  spio_text_ser->serialize(t1_id, ser_tag_tier2, vals);
  spio_text_ser->sync();
  std::string serialized_txt = spio_text_ser->get_serialized_data();

  if(Utils::rem_blank_str(serialized_txt) != Utils::rem_blank_str(exp_ser_txt)){
    LOG_RANK0(wrank, "test_simple_types() FAILED\n");
    LOG_RANK0(wrank, "Serialized text : \n");
    LOG_RANK0(wrank, "%s\n", serialized_txt.c_str());
    LOG_RANK0(wrank, "Expected serialized text : \n");
    LOG_RANK0(wrank, "%s\n", exp_ser_txt.c_str());

    return PIO_EINTERNAL;
  }

  LOG_RANK0(wrank, "Testing TEXT serializer PASSED\n");

  /*
   * ============ Expected Serialized JSON ===========
   *{
   * "SerializedValsT1":{
   *  "SerializedValsT2":{
   *      "name" : "helloworld"
   *      "ival" : 3
   *      "dval" : 3.14
   *    }
   *  }
   *}
   */
  const std::string JSON_OBJECT_START("{");
  const std::string JSON_OBJECT_END("}");
  std::string exp_ser_json =
    JSON_OBJECT_START + NEWLINE +
      Utils::quoted_str(ser_tag_tier1) + ID_SEP + JSON_OBJECT_START + NEWLINE +
        Utils::quoted_str(ser_tag_tier2) + ID_SEP + JSON_OBJECT_START + NEWLINE +
          Utils::quoted_str(name_tag) + ID_SEP + Utils::quoted_str(name) + NEWLINE +
          Utils::quoted_str(ival_tag) + ID_SEP + std::to_string(ival) + NEWLINE +
          Utils::quoted_str(dval_tag) + ID_SEP + std::to_string(dval) + NEWLINE +
        JSON_OBJECT_END + NEWLINE +
      JSON_OBJECT_END + NEWLINE +
    JSON_OBJECT_END + NEWLINE;

  /* Create a JSON serializer */
  std::unique_ptr<PIO_Util::SPIO_serializer> spio_json_ser =
    PIO_Util::Serializer_Utils::create_serializer(
      PIO_Util::Serializer_type::JSON_SERIALIZER, "test_htypes.json");

  /* Serialize the vals, sync and retrieve the serialized data */
  t1_id = spio_json_ser->serialize(ser_tag_tier1, empty_vals);
  spio_json_ser->serialize(t1_id, ser_tag_tier2, vals);
  spio_json_ser->sync();
  std::string serialized_json = spio_json_ser->get_serialized_data();

  if(Utils::rem_blank_str(serialized_json) != Utils::rem_blank_str(exp_ser_json)){
    LOG_RANK0(wrank, "test_simple_types() FAILED\n");
    LOG_RANK0(wrank, "Serialized JSON : \n");
    LOG_RANK0(wrank, "%s\n", serialized_json.c_str());
    LOG_RANK0(wrank, "Expected serialized JSON : \n");
    LOG_RANK0(wrank, "%s\n", exp_ser_json.c_str());

    return PIO_EINTERNAL;
  }

  LOG_RANK0(wrank, "Testing JSON serializer PASSED\n");
  return PIO_NOERR;
}

int test_driver(MPI_Comm comm, int wrank, int wsz, int *num_errors)
{
  int nerrs = 0, ret = PIO_NOERR;
  assert((comm != MPI_COMM_NULL) && (wrank >= 0) && (wsz > 0) && num_errors);
  
  /* Test serializing simple types */
  try{
    ret = test_simple_types(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_simple_types() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_simple_types() PASSED\n");
  }

  /* Testing array types */
  try{
    ret = test_array_types(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_array_types() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_array_types() PASSED\n");
  }

  /* Testing hierarchical/tiered data */
  try{
    ret = test_tiered_data(wrank);
  }
  catch(...){
    ret = PIO_EINTERNAL;
  }
  if(ret != PIO_NOERR){
    LOG_RANK0(wrank, "test_tiered_data() FAILED, ret = %d\n", ret);
    nerrs++;
  }
  else{
    LOG_RANK0(wrank, "test_tiered_data() PASSED\n");
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
