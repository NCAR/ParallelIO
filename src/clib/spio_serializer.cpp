#include "spio_serializer.hpp"
#include <memory>
#include <stdexcept>
#include <iostream>
#include <fstream>

/* Text Serializer function definitions */

PIO_Util::Text_serializer::Text_serializer(const std::string &fname):
  SPIO_serializer(fname), next_id_(START_ID + 1)
{
  /* Zero spaces for start id */
  id2spaces_[START_ID] = START_ID_SPACES; 
}

int PIO_Util::Text_serializer::serialize(const std::string &name,
      const std::vector<std::pair<std::string, std::string> > &vals)
{
  int parent_id = START_ID;
  return serialize(parent_id, name, vals);
}

int PIO_Util::Text_serializer::serialize(int parent_id,
      const std::string &name,
      const std::vector<std::pair<std::string, std::string> > &vals)
{
  const char SPACE = ' ';
  const char ID_SEP = ':';
  const char NEWLINE = '\n';
  int val_id = next_id_++;

  int id_nspaces = id2spaces_.at(parent_id) + INC_SPACES;
  std::string id_spaces(id_nspaces, SPACE);

  id2spaces_[val_id] = id_nspaces;

  /* FIXME: C++14, Use std::quoted() */
  std::string qname = "\"" + name + "\"";
  sdata_ += id_spaces + qname + ID_SEP + NEWLINE;

  int val_nspaces = id_nspaces + INC_SPACES;
  std::string val_spaces(val_nspaces, SPACE);

  for(std::vector<std::pair<std::string, std::string> >::const_iterator citer = vals.cbegin();
      citer != vals.cend(); ++citer){
    sdata_ += val_spaces + (*citer).first + SPACE + ID_SEP + SPACE + (*citer).second + NEWLINE;
  }

  return val_id;
}

void PIO_Util::Text_serializer::sync(void )
{
  //std::cout << sdata_.c_str() << "\n";
  std::ofstream fstr;
  fstr.open(pname_.c_str(), std::ofstream::out | std::ofstream::trunc);
  fstr << sdata_.c_str();
  fstr.close();
}

std::string PIO_Util::Text_serializer::get_serialized_data(void )
{
  return sdata_;
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

/* Create a serializer */
std::unique_ptr<PIO_Util::SPIO_serializer> PIO_Util::Serializer_Utils::create_serializer(
  const PIO_Util::Serializer_type &type,
  const std::string &persistent_name)
{
  if(type == Serializer_type::JSON_SERIALIZER){
    throw std::runtime_error("JSON serializer is currently not supported");
  }
  else if(type == Serializer_type::XML_SERIALIZER){
    throw std::runtime_error("XML serializer is currently not supported");
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

