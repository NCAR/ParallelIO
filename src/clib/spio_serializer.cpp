#include "spio_serializer.hpp"
#include <memory>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>

/* Text Serializer function definitions */

int PIO_Util::Text_serializer::serialize(const std::string &name,
      const std::vector<std::pair<std::string, std::string> > &vals)
{
  /* Add the user data to the internal tree */
  Text_serializer_val sval = {name, vals};
  int val_id = dom_tree_.add(sval);

  /* Since this val has no parent, 0 spaces required for this tag */
  id2spaces_[val_id] = 0;

  return val_id;
}

int PIO_Util::Text_serializer::serialize(int parent_id,
      const std::string &name,
      const std::vector<std::pair<std::string, std::string> > &vals)
{
  /* Add the user data to the internal tree */
  Text_serializer_val sval = {name, vals};
  int val_id = dom_tree_.add(sval, parent_id);

  /* Number of spaces is INC_SPACES more than the parent tag */
  id2spaces_[val_id] = id2spaces_[parent_id] + INC_SPACES;

  return val_id;
}

void PIO_Util::Text_serializer::serialize(const std::string &name,
      const std::vector< std::vector<std::pair<std::string, std::string> > > &vvals,
      std::vector<int> &val_ids)
{
  for(std::vector<std::vector<std::pair<std::string, std::string> > >::const_iterator
      citer = vvals.cbegin(); citer != vvals.cend(); ++citer){
    val_ids.push_back(serialize(name, *citer));
  }
}

void PIO_Util::Text_serializer::serialize(int parent_id, const std::string &name,
      const std::vector< std::vector<std::pair<std::string, std::string> > > &vvals,
      std::vector<int> &val_ids)
{
  for(std::vector<std::vector<std::pair<std::string, std::string> > >::const_iterator
      citer = vvals.cbegin(); citer != vvals.cend(); ++citer){
    val_ids.push_back(serialize(parent_id, name, *citer));
  }
}

void PIO_Util::Text_serializer::sync(void )
{
  /* Create a text serializer */
  Text_serializer_visitor vis(id2spaces_, INC_SPACES);

  /* Traverse the tree using the text serializer. The text serializer
   * serializes the data in the tree to text and caches it
   */
  dom_tree_.dfs(vis);

  /* Get the cached serialized data from the text serializer */
  sdata_ = vis.get_serialized_data();

  /* Write the data out to the text file */
  std::ofstream fstr;
  std::string pname = PIO_Util::Serializer_Utils::get_fname_prefix() + pname_;
  fstr.open(pname.c_str(), std::ofstream::out | std::ofstream::trunc);
  fstr << sdata_.c_str();
  fstr.close();
}

std::string PIO_Util::Text_serializer::get_serialized_data(void )
{
  return sdata_;
}

/* Text serializer visitor functions */
void PIO_Util::Text_serializer::Text_serializer_visitor::enter_node(
  Text_serializer_val &val, int val_id)
{
  int id_nspaces = id2spaces_[val_id];
  std::string id_spaces(id_nspaces, SPACE);

  std::string qname = "\"" + val.name + "\"";
  sdata_ += id_spaces + qname + ID_SEP + NEWLINE;

  int val_nspaces = id_nspaces + inc_spaces_;
  std::string val_spaces(val_nspaces, SPACE);

  /* Serialize and cache the (name, value) pairs on the node */
  for(std::vector<std::pair<std::string, std::string> >::const_iterator citer = val.vals.cbegin();
      citer != val.vals.cend(); ++citer){
    sdata_ += val_spaces + (*citer).first + SPACE + ID_SEP + SPACE + (*citer).second + NEWLINE;
  }
}

void PIO_Util::Text_serializer::Text_serializer_visitor::enter_node(
  Text_serializer_val &val, int val_id,
  Text_serializer_val &parent_val, int parent_id)
{
  /* Text serializer does not use the parent info */
  Text_serializer_visitor::enter_node(val, val_id);
}

/* XML Serializer function definitions */

int PIO_Util::XML_serializer::serialize(const std::string &name,
      const std::vector<std::pair<std::string, std::string> > &vals)
{
  /* Add the user data to the internal tree */
  XML_serializer_val sval = {name, vals};
  int val_id = dom_tree_.add(sval);

  /* Since this val has no parent, 0 spaces required for this tag */
  id2spaces_[val_id] = 0;

  return val_id;
}

int PIO_Util::XML_serializer::serialize(int parent_id,
      const std::string &name,
      const std::vector<std::pair<std::string, std::string> > &vals)
{
  /* Add the user data to the internal tree */
  XML_serializer_val sval = {name, vals};
  int val_id = dom_tree_.add(sval, parent_id);

  /* Number of spaces is INC_SPACES more than the parent tag */
  id2spaces_[val_id] = id2spaces_[parent_id] + INC_SPACES;

  return val_id;
}

void PIO_Util::XML_serializer::serialize(const std::string &name,
      const std::vector< std::vector<std::pair<std::string, std::string> > > &vvals,
      std::vector<int> &val_ids)
{
  for(std::vector<std::vector<std::pair<std::string, std::string> > >::const_iterator
      citer = vvals.cbegin(); citer != vvals.cend(); ++citer){
    val_ids.push_back(serialize(name, *citer));
  }
}

void PIO_Util::XML_serializer::serialize(int parent_id, const std::string &name,
      const std::vector< std::vector<std::pair<std::string, std::string> > > &vvals,
      std::vector<int> &val_ids)
{
  for(std::vector<std::vector<std::pair<std::string, std::string> > >::const_iterator
      citer = vvals.cbegin(); citer != vvals.cend(); ++citer){
    val_ids.push_back(serialize(parent_id, name, *citer));
  }
}

void PIO_Util::XML_serializer::sync(void )
{
  /* Create an XML serializer */
  XML_serializer_visitor vis(id2spaces_, INC_SPACES);

  /* Traverse the tree using the XML serializer. The XML serializer
   * serializes the data in the tree to text and caches it
   */
  dom_tree_.dfs(vis);

  /* Get the cached serialized data from the XML serializer */
  sdata_ = vis.get_serialized_data();

  /* Write the data out to the XML file */
  std::ofstream fstr;
  std::string pname = PIO_Util::Serializer_Utils::get_fname_prefix() + pname_;
  fstr.open(pname.c_str(), std::ofstream::out | std::ofstream::trunc);
  fstr << sdata_.c_str();
  fstr.close();
}

std::string PIO_Util::XML_serializer::get_serialized_data(void )
{
  return sdata_;
}

/* XML serializer visitor functions */
void PIO_Util::XML_serializer::XML_serializer_visitor::enter_node(
  XML_serializer_val &val, int val_id)
{
  int id_nspaces = id2spaces_[val_id];
  std::string id_spaces(id_nspaces, SPACE);

  std::string tname = get_start_tag(val.name);
  sdata_ += id_spaces + tname + NEWLINE;

  int val_nspaces = id_nspaces + inc_spaces_;
  std::string val_spaces(val_nspaces, SPACE);

  /* Serialize and cache the (name, value) pairs on the node */
  for(std::vector<std::pair<std::string, std::string> >::const_iterator citer = val.vals.cbegin();
      citer != val.vals.cend(); ++citer){
    std::string stag_name = get_start_tag(citer->first);
    std::string etag_name = get_end_tag(citer->first);
    sdata_ += val_spaces + stag_name + SPACE + citer->second + SPACE + etag_name + NEWLINE;
  }
}

void PIO_Util::XML_serializer::XML_serializer_visitor::enter_node(
  XML_serializer_val &val, int val_id,
  XML_serializer_val &parent_val, int parent_id)
{
  /* XML serializer does not use the parent info */
  XML_serializer_visitor::enter_node(val, val_id);
}

void PIO_Util::XML_serializer::XML_serializer_visitor::exit_node(
  XML_serializer_val &val, int val_id)
{
  int id_nspaces = id2spaces_[val_id];
  std::string id_spaces(id_nspaces, SPACE);

  std::string tname = get_end_tag(val.name);
  sdata_ += id_spaces + tname + NEWLINE;
}

void PIO_Util::XML_serializer::XML_serializer_visitor::exit_node(
  XML_serializer_val &val, int val_id,
  XML_serializer_val &parent_val, int parent_id)
{
  /* XML serializer does not use the parent info */
  XML_serializer_visitor::exit_node(val, val_id);
}

/* XML tagify the tag names */
std::string PIO_Util::XML_serializer::XML_serializer_visitor::get_start_tag(
  const std::string &tag_name)
{
  return std::string("<") +
          get_unquoted_str(tag_name) +
          std::string(">");
}

std::string PIO_Util::XML_serializer::XML_serializer_visitor::get_end_tag(
  const std::string &tag_name)
{
  return std::string("</") +
          get_unquoted_str(tag_name) +
          std::string(">");
}

/* Remove double quotes around a string
 * The function only removes double quotes if its present at the beginning
 * and the end of a string
 * e.g.
 *  "helloworld" --> helloworld
 *  'helloworld' --> 'helloworld'
 *  helloworld --> helloworld
 *  hello"world" --> hello"world"
 */
std::string PIO_Util::XML_serializer::XML_serializer_visitor::get_unquoted_str(
  const std::string &str)
{
  const char DQUOTE = '"';
  if(str.size() >= 2){
    if((str[0] == DQUOTE) && (str[str.size() - 1] == DQUOTE)){
      return str.substr(1, str.size() - 2);
    }
  }
  return str;
}

/* JSON Serializer functions */
int PIO_Util::Json_serializer::serialize(const std::string &name,
      const std::vector<std::pair<std::string, std::string> > &vals)
{
  /* Add the user data to the internal tree */
  Json_serializer_val sval = {Json_agg_type::OBJECT, name, vals};
  int val_id = dom_tree_.add(sval);

  id2spaces_[val_id] = START_ID_SPACES + INC_SPACES;

  return val_id;
}

int PIO_Util::Json_serializer::serialize(int parent_id,
      const std::string &name,
      const std::vector<std::pair<std::string, std::string> > &vals)
{
  /* Add the user data to the internal tree */
  Json_serializer_val sval = {Json_agg_type::OBJECT, name, vals};
  int val_id = dom_tree_.add(sval, parent_id);

  /* Number of spaces is INC_SPACES more than the parent tag */
  id2spaces_[val_id] = id2spaces_[parent_id] + INC_SPACES;

  return val_id;
}

void PIO_Util::Json_serializer::serialize(const std::string &name,
      const std::vector< std::vector<std::pair<std::string, std::string> > > &vvals,
      std::vector<int> &val_ids)
{
  std::vector<std::pair<std::string, std::string> > vals;
  Json_serializer_val sval = {Json_agg_type::ARRAY, name, vals}; 

  /* Add the tag with name, name, as a node in the tree */
  int sval_id = dom_tree_.add(sval);
  id2spaces_[sval_id] = START_ID_SPACES + INC_SPACES;

  /* Since the values are part of an array, mark each as an ARRAY_ELEMENT and
   * add it as children of the tag node
   */
  for(std::vector<std::vector<std::pair<std::string, std::string> > >::const_iterator
      citer = vvals.cbegin(); citer != vvals.cend(); ++citer){
    //val_ids.push_back(serialize(sval_id, name, *citer));
    Json_serializer_val arr_val = {Json_agg_type::ARRAY_ELEMENT, name, *citer};
    int arr_val_id = dom_tree_.add(arr_val, sval_id);

    id2spaces_[arr_val_id] = id2spaces_[sval_id] + INC_SPACES;
    val_ids.push_back(arr_val_id);
  }
}

void PIO_Util::Json_serializer::serialize(int parent_id, const std::string &name,
      const std::vector< std::vector<std::pair<std::string, std::string> > > &vvals,
      std::vector<int> &val_ids)
{
  std::vector<std::pair<std::string, std::string> > vals;
  Json_serializer_val sval = {Json_agg_type::ARRAY, name, vals}; 

  /* Add the tag with name, name, as a node in the tree */
  int sval_id = dom_tree_.add(sval, parent_id);
  id2spaces_[sval_id] = id2spaces_[parent_id] + INC_SPACES;

  /* Since the values are part of an array, mark each as an ARRAY_ELEMENT and
   * add it as children of the tag node
   */
  for(std::vector<std::vector<std::pair<std::string, std::string> > >::const_iterator
      citer = vvals.cbegin(); citer != vvals.cend(); ++citer){
    //val_ids.push_back(serialize(sval_id, name, *citer));
    Json_serializer_val arr_val = {Json_agg_type::ARRAY_ELEMENT, name, *citer};
    int arr_val_id = dom_tree_.add(arr_val, sval_id);

    id2spaces_[arr_val_id] = id2spaces_[sval_id] + INC_SPACES;
    val_ids.push_back(arr_val_id);
  }
}

void PIO_Util::Json_serializer::sync(void )
{
  /* Create a JSON serializer */
  Json_serializer_visitor vis(id2spaces_, INC_SPACES);

  /* Traverse the tree using the JSON serializer. The serializer will
   * serialize the contents of the tree and cache it
   */
  dom_tree_.dfs(vis);

  /* Retrieve the cached serialized contents of the tree from the serializer */
  sdata_ = vis.get_serialized_data();

  /* Write the serialized data to the JSON file */
  std::ofstream fstr;
  std::string pname = PIO_Util::Serializer_Utils::get_fname_prefix() + pname_;
  fstr.open(pname.c_str(), std::ofstream::out | std::ofstream::trunc);
  fstr << sdata_.c_str();
  fstr.close();
}

std::string PIO_Util::Json_serializer::get_serialized_data(void )
{
  return sdata_;
}

/* Json serializer visitor functions */

void PIO_Util::Json_serializer::Json_serializer_visitor::begin(void)
{
  /* Start the root JSON object that encapsulates all objects in the JSON file */
  sdata_ += std::string(1, OBJECT_START) + std::string(1, NEWLINE);
}

void PIO_Util::Json_serializer::Json_serializer_visitor::enter_node(
  Json_serializer_val &val, int val_id)
{
  int id_nspaces = id2spaces_[val_id];
  std::string id_spaces(id_nspaces, SPACE);
    
  if(val.type == Json_agg_type::ARRAY_ELEMENT){
    /* Each array element is a JSON object */
    sdata_ += id_spaces + OBJECT_START + NEWLINE;
  }
  else{
    std::string qname = "\"" + val.name + "\"";
    /* Check if the tag corresponds to a JSON array or object */
    const char JSON_AGG_TYPE_START = (val.type == Json_agg_type::ARRAY) ?
                                      ARRAY_START : OBJECT_START;
    sdata_ += id_spaces + qname + ID_SEP + JSON_AGG_TYPE_START + NEWLINE;
  }

  int val_nspaces = id_nspaces + inc_spaces_;
  std::string val_spaces(val_nspaces, SPACE);

  /* Serialize all (name, value) pairs on this node */
  for(std::vector<std::pair<std::string, std::string> >::const_iterator citer = val.vals.cbegin();
      citer != val.vals.cend(); ++citer){
    sdata_ += val_spaces + (*citer).first + SPACE + ID_SEP + SPACE + (*citer).second;
    if(citer + 1 != val.vals.cend()){
      sdata_ += ELEM_SEP;
    }
    sdata_ += NEWLINE;
  }
}

void PIO_Util::Json_serializer::Json_serializer_visitor::enter_node(
  Json_serializer_val &val, int val_id,
  Json_serializer_val &parent_val, int parent_id)
{
  enter_node(val, val_id);
}

void PIO_Util::Json_serializer::Json_serializer_visitor::on_node(
  Json_serializer_val &val, int val_id)
{
  int id_nspaces = id2spaces_[val_id] + inc_spaces_;
  std::string id_spaces(id_nspaces, SPACE);

  /* Separate out the JSON objects in this aggregate object using ELEM_SEP */
  sdata_ += id_spaces + ELEM_SEP + NEWLINE;
}

void PIO_Util::Json_serializer::Json_serializer_visitor::on_node(
  Json_serializer_val &val, int val_id,
  Json_serializer_val &parent_val, int parent_id)
{
  on_node(val, val_id);
}

void PIO_Util::Json_serializer::Json_serializer_visitor::exit_node(
  Json_serializer_val &val, int val_id)
{
  int id_nspaces = id2spaces_[val_id] + inc_spaces_;
  std::string id_spaces(id_nspaces, SPACE);

  if(val.type == Json_agg_type::ARRAY_ELEMENT){
    /* Each array element is a json object, close/end the object */
    sdata_ += id_spaces + OBJECT_END + NEWLINE;
  }
  else{
    const char JSON_AGG_TYPE_END = (val.type == Json_agg_type::ARRAY) ?
                                      ARRAY_END : OBJECT_END;
    sdata_ += id_spaces + JSON_AGG_TYPE_END + NEWLINE;
  }
}

void PIO_Util::Json_serializer::Json_serializer_visitor::exit_node(
  Json_serializer_val &val, int val_id,
  Json_serializer_val &parent_val, int parent_id)
{
  exit_node(val, val_id);
}

void PIO_Util::Json_serializer::Json_serializer_visitor::end(void)
{
  /* Close out the root JSON object that contains all other objects in the file */
  sdata_ += std::string(1, OBJECT_END) + std::string(1, NEWLINE);
}

/* Misc Serializer utils */

/* Convert Serializer type to string */
std::string PIO_Util::Serializer_Utils::to_string(const PIO_Util::Serializer_type &type)
{
  if(type == Serializer_type::JSON_SERIALIZER){
    return "JSON_SERIALIZER";
  }
  else if(type == Serializer_type::XML_SERIALIZER){
    return "XML_SERIALIZER";
  }
  else if(type == Serializer_type::TEXT_SERIALIZER){
    return "TEXT_SERIALIZER";
  }
  else if(type == Serializer_type::MEM_SERIALIZER){
    return "MEM_SERIALIZER";
  }
  else{
    return "UNKNOWN";
  }
}

/* Get the prefix for file names used by file-based (Text, XML, Json) serializers */
std::string PIO_Util::Serializer_Utils::get_fname_prefix(void )
{
  const std::string default_timing_dir("spio_stats");
  const std::string dir_sep("/");
  std::string fname_prefix;

  struct stat sb;
  /* If there exists a directory named "spio_stats" use it as the file name prefix
   * So if there is a directory named "spio_stats" all text based serializers
   * will create/collect stats/files in this directory
   */
  if(stat(default_timing_dir.c_str(), &sb) == 0){
    if(S_ISDIR(sb.st_mode)){
      fname_prefix = default_timing_dir + dir_sep;
    }
  }

  return fname_prefix;
}

/* Create a serializer */
std::unique_ptr<PIO_Util::SPIO_serializer> PIO_Util::Serializer_Utils::create_serializer(
  const PIO_Util::Serializer_type &type,
  const std::string &persistent_name)
{
  if(type == Serializer_type::JSON_SERIALIZER){
    return std::unique_ptr<Json_serializer>(new Json_serializer(persistent_name));
  }
  else if(type == Serializer_type::XML_SERIALIZER){
    return std::unique_ptr<XML_serializer>(new XML_serializer(persistent_name));
  }
  else if(type == Serializer_type::TEXT_SERIALIZER){
    return std::unique_ptr<Text_serializer>(new Text_serializer(persistent_name));
  }
  else if(type == Serializer_type::MEM_SERIALIZER){
    throw std::runtime_error("In memory serializer is currently not supported");
  }
  throw std::runtime_error("Unsupported serializer type provided");
}

/* Util to pack (name,value) pairs where values are strings - to pass to serializer */
void PIO_Util::Serializer_Utils::serialize_pack(
  const std::string &name, const std::string &val,
  std::vector<std::pair<std::string, std::string> > &vals)
{
  // FIXME: C++14 has std::quoted()
  std::string qname = "\"" + name + "\"";
  std::string qval = "\"" + val + "\"";
  vals.push_back(std::make_pair(qname, qval));
}

