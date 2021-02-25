#ifndef __SPIO_SERIALIZER_HPP__
#define __SPIO_SERIALIZER_HPP__

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <map>

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
    virtual void sync(void ) = 0;
    virtual std::string get_serialized_data(void ) = 0;
    virtual ~SPIO_serializer() {};
  protected:
    std::string pname_;
};

class Text_serializer : public SPIO_serializer{
  public:
    Text_serializer(const std::string &fname);
    int serialize(const std::string &name,
                  const std::vector<std::pair<std::string, std::string> > &vals) override;
    int serialize(int parent_id, const std::string &name,
                  const std::vector<std::pair<std::string, std::string> > &vals) override;
    void sync(void ) override;
    std::string get_serialized_data(void ) override;
    ~Text_serializer() {};
  private:
    const int START_ID = 0;
    int next_id_;
    std::string sdata_;
    const int START_ID_SPACES = 0;
    const int INC_SPACES = 2;
    std::map<int, int> id2spaces_;
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
