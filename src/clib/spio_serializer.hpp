#ifndef __SPIO_SERIALIZER_HPP__
#define __SPIO_SERIALIZER_HPP__

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <map>
#include "spio_tree.hpp"

namespace PIO_Util{

class SPIO_serializer{
  public:
    SPIO_serializer(const std::string &pname) : pname_(pname) {}
    std::string get_name(void ) const { return pname_; }
    void set_name(const std::string &name) { pname_ = name; }
    virtual int serialize(const std::string &name,
                          const std::vector<std::pair<std::string, std::string> > &vals) = 0;
    virtual int serialize(int parent_id, const std::string &name,
                          const std::vector<std::pair<std::string, std::string> > &vals) = 0;
    virtual void serialize(const std::string &name,
                          const std::vector<std::vector<std::pair<std::string, std::string> > > &vvals,
                          std::vector<int> &val_ids) = 0;
    virtual void serialize(int parent_id, const std::string &name,
                          const std::vector<std::vector<std::pair<std::string, std::string> > > &vvals,
                          std::vector<int> &val_ids) = 0;
    virtual void sync(void ) = 0;
    virtual std::string get_serialized_data(void ) = 0;
    virtual ~SPIO_serializer() {};
  protected:
    std::string pname_;
};

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
    std::string sdata_;
    const int START_ID_SPACES = 0;
    const int INC_SPACES = 2;
    std::map<int, int> id2spaces_;

    struct Text_serializer_val{
      std::string name;
      std::vector<std::pair<std::string, std::string> > vals;
    };

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
    std::map<int, int> id2spaces_;

    enum class Json_agg_type{
      ARRAY,
      ARRAY_ELEMENT,
      OBJECT
    };

    struct Json_serializer_val{
      Json_agg_type type;
      std::string name;
      std::vector<std::pair<std::string, std::string> > vals;
    };

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
        const char AGG_SEP = ',';
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
  std::string to_string(const Serializer_type &type);

  std::unique_ptr<SPIO_serializer> create_serializer(const Serializer_type &type,
                                    const std::string &persistent_name);

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
