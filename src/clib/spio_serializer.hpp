#ifndef __SPIO_SERIALIZER_HPP__
#define __SPIO_SERIALIZER_HPP__

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <map>
#include "spio_tree.hpp"

namespace PIO_Util{

/* The serializer base class
 * Serializers can be used to serialize (name, value) pairs into text/json/xml files
 */
class SPIO_serializer{
  public:
    /* Create the serializer with a persistent name, pname */
    SPIO_serializer(const std::string &pname) : pname_(pname) {}
    /* Get the persistent name of the serializer */
    std::string get_name(void ) const { return pname_; }
    /* Set the persistent name of the serializer */
    void set_name(const std::string &name) { pname_ = name; }
    /* Serialize a vector of (name, value) pairs, vals, with a tag with name, name
     * The function returns the id of the tag being serialized
     */
    virtual int serialize(const std::string &name,
                          const std::vector<std::pair<std::string, std::string> > &vals) = 0;
    /* Serialize a vector of (name, value) pairs, vals, with a tag with name, name, inside another
     * tag with id parent_id
     * The function returns the id of the tag being serialized
     */
    virtual int serialize(int parent_id, const std::string &name,
                          const std::vector<std::pair<std::string, std::string> > &vals) = 0;
    /* Serialize an array of tags, each tag containing a vector of value pairs
     * The function returns the ids of the value_pairs (each element in the array) being serialized
     * in the val_ids array
     */
    virtual void serialize(const std::string &name,
                          const std::vector<std::vector<std::pair<std::string, std::string> > > &vvals,
                          std::vector<int> &val_ids) = 0;
    /* Serialize an array of tags, each tag containing a vector of value pairs inside another
     * tag with id parent_id
     * The function returns the ids of the value_pairs (each element in the array) being serialized
     * in the val_ids array
     */
    virtual void serialize(int parent_id, const std::string &name,
                          const std::vector<std::vector<std::pair<std::string, std::string> > > &vvals,
                          std::vector<int> &val_ids) = 0;
    /* Sync/flush the contents. In the case of the text serializer the data is written out to the file */
    virtual void sync(void ) = 0;
    /* Get the serialized data. The contents need to be synced by calling sync() before calling this func */
    virtual std::string get_serialized_data(void ) = 0;
    virtual ~SPIO_serializer() {};
  protected:
    std::string pname_;
};

/* Class to serialize (name, value) pairs and tags in to a text file */
class Text_serializer : public SPIO_serializer{
  public:
    Text_serializer(const std::string &fname) : SPIO_serializer(fname) {};
    int serialize(const std::string &name,
                  const std::vector<std::pair<std::string, std::string> > &vals) override;
    int serialize(int parent_id, const std::string &name,
                  const std::vector<std::pair<std::string, std::string> > &vals) override;
    void serialize(const std::string &name,
                  const std::vector<std::vector<std::pair<std::string, std::string> > > &vvals,
                  std::vector<int> &val_ids) override;
    void serialize(int parent_id, const std::string &name,
                  const std::vector<std::vector<std::pair<std::string, std::string> > > &vvals,
                  std::vector<int> &val_ids) override;
    void sync(void ) override;
    std::string get_serialized_data(void ) override;
    ~Text_serializer() {};
  private:
    /* Cache of the serialized data */
    std::string sdata_;
    const int START_ID_SPACES = 0;
    const int INC_SPACES = 2;
    /* Store the number of spaces to output for each tag/id */
    std::map<int, int> id2spaces_;

    /* The internal tree stores the tag name and the associated (name, value) pairs 
     * in this struct
     */
    struct Text_serializer_val{
      std::string name;
      std::vector<std::pair<std::string, std::string> > vals;
    };

    /* Visitor class used to serialize the contents of the internal tree to text */
    class Text_serializer_visitor : public SPIO_tree_visitor<Text_serializer_val>{
      public:
        Text_serializer_visitor(const std::map<int, int> &id2spaces,
          int inc_spaces) : id2spaces_(id2spaces), inc_spaces_(inc_spaces) {};
        void enter_node(Text_serializer_val &val, int val_id) override;
        void enter_node(Text_serializer_val &val, int val_id,
              Text_serializer_val &parent_val, int parent_id) override;
        std::string get_serialized_data(void ) { return sdata_; };
      private:
        const char SPACE = ' ';
        const char ID_SEP = ':';
        const char NEWLINE = '\n';
        std::string sdata_;
        std::map<int, int> id2spaces_;
        int inc_spaces_;
    };

    SPIO_tree<Text_serializer_val> dom_tree_;
};

/* Class to serialize (name, value) pairs and tags in to an XML file */
class XML_serializer : public SPIO_serializer{
  public:
    XML_serializer(const std::string &fname) : SPIO_serializer(fname) {};
    int serialize(const std::string &name,
                  const std::vector<std::pair<std::string, std::string> > &vals) override;
    int serialize(int parent_id, const std::string &name,
                  const std::vector<std::pair<std::string, std::string> > &vals) override;
    void serialize(const std::string &name,
                  const std::vector<std::vector<std::pair<std::string, std::string> > > &vvals,
                  std::vector<int> &val_ids) override;
    void serialize(int parent_id, const std::string &name,
                  const std::vector<std::vector<std::pair<std::string, std::string> > > &vvals,
                  std::vector<int> &val_ids) override;
    void sync(void ) override;
    std::string get_serialized_data(void ) override;
    ~XML_serializer() {};
  private:
    /* Cache of the serialized data */
    std::string sdata_;
    const int START_ID_SPACES = 0;
    const int INC_SPACES = 2;
    /* Store the number of spaces to output for each tag/id */
    std::map<int, int> id2spaces_;

    /* The internal tree stores the tag name and the associated (name, value) pair
     * in this struct
     */
    struct XML_serializer_val{
      std::string name;
      std::vector<std::pair<std::string, std::string> > vals;
    };

    /* Visitor class used to serialize the contents of the internal tree to text */
    class XML_serializer_visitor : public SPIO_tree_visitor<XML_serializer_val>{
      public:
        XML_serializer_visitor(const std::map<int, int> &id2spaces,
          int inc_spaces) : id2spaces_(id2spaces), inc_spaces_(inc_spaces) {};
        void enter_node(XML_serializer_val &val, int val_id) override;
        void enter_node(XML_serializer_val &val, int val_id,
              XML_serializer_val &parent_val, int parent_id) override;
        void exit_node(XML_serializer_val &val, int val_id) override;
        void exit_node(XML_serializer_val &val, int val_id,
              XML_serializer_val &parent_val, int parent_id) override;
        std::string get_serialized_data(void ) { return sdata_; };
      private:
        const char SPACE = ' ';
        const char NEWLINE = '\n';
        std::string sdata_;
        std::map<int, int> id2spaces_;
        int inc_spaces_;

        std::string get_start_tag(const std::string &tag_name);
        std::string get_end_tag(const std::string &tag_name);
        std::string get_unquoted_str(const std::string &str);
    };

    SPIO_tree<XML_serializer_val> dom_tree_;
};

/* Class to serialize (name, value) pairs and tags in to a json file */
class Json_serializer : public SPIO_serializer{
  public:
    Json_serializer(const std::string &fname) : SPIO_serializer(fname) {};
    int serialize(const std::string &name,
                  const std::vector<std::pair<std::string, std::string> > &vals) override;
    int serialize(int parent_id, const std::string &name,
                  const std::vector<std::pair<std::string, std::string> > &vals) override;
    void serialize(const std::string &name,
                  const std::vector<std::vector<std::pair<std::string, std::string> > > &vvals,
                  std::vector<int> &val_ids) override;
    void serialize(int parent_id, const std::string &name,
                  const std::vector<std::vector<std::pair<std::string, std::string> > > &vvals,
                  std::vector<int> &val_ids) override;
    void sync(void ) override;
    std::string get_serialized_data(void ) override;
    ~Json_serializer() {};
  private:
    std::string sdata_;
    const int START_ID_SPACES = 0;
    const int INC_SPACES = 2;
    /* Store the number of spaces to output for each tag/id */
    std::map<int, int> id2spaces_;

    /* We need to distinguish between array, an array element and object json types */
    enum class Json_agg_type{
      ARRAY,
      ARRAY_ELEMENT,
      OBJECT
    };

    /* The internal tree stores the tag name and the associated (name, value) pairs 
     * in this struct
     */
    struct Json_serializer_val{
      Json_agg_type type;
      std::string name;
      std::vector<std::pair<std::string, std::string> > vals;
    };

    /* Visitor class used to serialize the contents of the internal tree to json */
    class Json_serializer_visitor : public SPIO_tree_visitor<Json_serializer_val>{
      public:
        Json_serializer_visitor(const std::map<int, int> &id2spaces,
          int inc_spaces) : id2spaces_(id2spaces), inc_spaces_(inc_spaces) {};
        void begin(void ) override;
        void enter_node(Json_serializer_val &val, int val_id) override;
        void enter_node(Json_serializer_val &val, int val_id,
                        Json_serializer_val &parent_val, int parent_id) override;
        void on_node(Json_serializer_val &val, int val_id) override;
        void on_node(Json_serializer_val &val, int val_id,
              Json_serializer_val &parent_val, int parent_id) override;
        void exit_node(Json_serializer_val &val, int val_id) override;
        void exit_node(Json_serializer_val &val, int val_id,
                        Json_serializer_val &parent_val, int parent_id) override;
        void end(void ) override;
        std::string get_serialized_data(void ) { return sdata_; };
      private:
        const char SPACE = ' ';
        const char ID_SEP = ':';
        const char NEWLINE = '\n';
        const char ARRAY_START = '[';
        const char ARRAY_END = ']';
        const char OBJECT_START = '{';
        const char OBJECT_END = '}';
        const char ELEM_SEP = ',';
        std::string sdata_;
        std::map<int, int> id2spaces_;
        int inc_spaces_;
    };

    SPIO_tree<Json_serializer_val> dom_tree_;
};

enum class Serializer_type{
  JSON_SERIALIZER,
  XML_SERIALIZER,
  TEXT_SERIALIZER,
  MEM_SERIALIZER
};

namespace Serializer_Utils{
  /* Convert Serializer type to string */
  std::string to_string(const Serializer_type &type);

  /* Get the file name prefix. An empty string is returned if there is no
   * prefix required.
   * The text based serializers need to use this prefix to create all files
   *
   * e.g. For file based serializers if all the serialized files need to be moved
   * to a separate directory this function will return the name of the directory
   * used to consolidate the serialized files
   */
  std::string get_fname_prefix(void );

  /* Factory for serializers */
  std::unique_ptr<SPIO_serializer> create_serializer(const Serializer_type &type,
                                    const std::string &persistent_name);

  /* Utility function to pack (name, value) pairs of different types into
   * a vector of string pairs that is used by the serializer to serialize
   * the pairs.
   */
  template<typename T>
  void serialize_pack(const std::string &name, const T &val,
                      std::vector<std::pair<std::string, std::string> > &vals)
  {
    /* FIXME: C++14 has std::quoted() */
    std::string qname = "\"" + name + "\"";
    vals.push_back(std::make_pair(qname, std::to_string(val)));
  }

  void serialize_pack(const std::string &name, const std::string &val,
                      std::vector<std::pair<std::string, std::string> > &vals);

} // namespace Serializer_Utils

} // namespace PIO_Util
#endif /* __SPIO_SERIALIZER_HPP__ */
