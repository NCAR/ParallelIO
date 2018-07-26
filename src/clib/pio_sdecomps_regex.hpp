#ifndef __PIO_SDECOMPS_REGEX_HPP__
#define __PIO_SDECOMPS_REGEX_HPP__

#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <cassert>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <stack>
extern "C"{
#include "config.h"
#include "pio.h"
#include "pio_internal.h"
#include "pio_sdecomps_regex.h"
}

namespace PIO_Util{
  namespace PIO_SDecomp_Util{
      class SDecomp_regex_op;
      class SDecomp_regex_token{
        public:
          virtual std::string to_string(void ) const = 0;
          virtual void convert_to_postfix(
            std::stack<const SDecomp_regex_op *> &tok_stack,
            std::vector<const SDecomp_regex_token *> &postfix_exp) const = 0;
          virtual void eval_postfix(
            std::stack<bool> &eval_stack,
            int ioid, const std::string &fname,
            const std::string &vname) const = 0;
          virtual ~SDecomp_regex_token() = 0;
      };

      void str_ltrim(std::string::const_iterator &begin,
        const std::string::const_iterator &end);

      void str_rtrim(const std::string::const_iterator &begin,
        std::string::const_iterator &end);

      void str_trim(std::string::const_iterator &begin,
        std::string::const_iterator &end);

      class SDecomp_regex_op : public SDecomp_regex_token{
        public:
          SDecomp_regex_op(const std::string &str);
          std::string to_string(void ) const;
          void convert_to_postfix(
            std::stack<const SDecomp_regex_op *> &tok_stack,
            std::vector<const SDecomp_regex_token *> &postfix_exp) const;
          void eval_postfix(
            std::stack<bool> &eval_stack,
            int ioid, const std::string &fname,
            const std::string &vname) const;
          static bool parse_and_create_token(
                  std::string::const_iterator &begin,
                  std::string::const_iterator &end,
                  std::vector<SDecomp_regex_token *> &pregex_tokens);
        private:
          typedef enum SDecomp_regex_op_type_{
            INVALID_OP = 0,
            LOGICAL_AND,
            LOGICAL_OR,
            LOGICAL_NOT,
            LEFT_BRACKET,
            RIGHT_BRACKET,
            NUM_OPS
          } SDecomp_regex_op_type;

          static SDecomp_regex_op_type to_type(const std::string &str);
          static std::string to_string(const SDecomp_regex_op_type &type);
          bool operator<(const SDecomp_regex_op &other) const;
          bool operator==(const SDecomp_regex_op_type &type) const;
          SDecomp_regex_op_type type_;
      };
      class SDecomp_regex_item : public SDecomp_regex_token{
        public:
          SDecomp_regex_item(const std::string &str);
          std::string to_string(void ) const;
          void convert_to_postfix(
            std::stack<const SDecomp_regex_op *> &tok_stack,
            std::vector<const SDecomp_regex_token *> &postfix_exp) const;
          void eval_postfix(
            std::stack<bool> &eval_stack,
            int ioid, const std::string &fname,
            const std::string &vname) const;
          static bool parse_and_create_token(
                  std::string::const_iterator &begin,
                  std::string::const_iterator &end,
                  std::vector<SDecomp_regex_token *> &pregex_tokens);
        private:
          typedef enum SDecomp_regex_item_type_{
            INVALID_REGEX = 0,
            ID_REGEX,
            VAR_REGEX,
            FILE_REGEX 
          } SDecomp_regex_item_type;
          const int INVALID_IOID = -1;

          static void find_tok(
            const std::string &tok_item_str,
            const std::string::const_iterator &begin,
            const std::string::const_iterator &end,
            std::string::const_iterator &tok_begin,
            std::string::const_iterator &tok_end);
          static SDecomp_regex_item_type to_type(const std::string &str);
          static std::string to_string(const SDecomp_regex_item_type &type);
          SDecomp_regex_item_type type_;
          std::regex rgx_;
          int ioid_;
      };
  } // namespace PIO_SDecomp_Util
  class PIO_save_decomp_regex{
    public:
      PIO_save_decomp_regex(const std::string &rgx);
      bool matches(int ioid, const std::string &fname, const std::string &vname) const;
      ~PIO_save_decomp_regex();
    private:
      void tokenize_sdecomp_regex(const std::string &rgx); 
      void convert_to_postfix(void );
      std::vector<PIO_SDecomp_Util::SDecomp_regex_token *> pregex_tokens_;
      std::vector<const PIO_SDecomp_Util::SDecomp_regex_token *> postfix_exp_;
  };
} // namespace PIO_Util

#endif // __PIO_SDECOMPS_REGEX_HPP__
