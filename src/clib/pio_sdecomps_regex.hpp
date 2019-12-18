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
#include "pio_config.h"
#include "pio.h"
#include "pio_internal.h"
#include "pio_sdecomps_regex.h"
}

namespace PIO_Util{
  /* Internal util classes and functions used for parsing and evaluating
   * regular expression used to save I/O decompositions
   */
  namespace PIO_SDecomp_Util{
      class SDecomp_regex_op;
      /* Abstract class to represent all tokens in the regular expression
       * The user regex consists of 
       * Regex operators and regex items (discussed below)
       */
      class SDecomp_regex_token{
        public:
          /* Convert token to string */
          virtual std::string to_string(void ) const = 0;
          /* Convert infix tokenized expression to postfix, the args include
           * a stack for op tokens and the current postfix expression
           */
          virtual void convert_to_postfix(
            std::stack<const SDecomp_regex_op *> &tok_stack,
            std::vector<const SDecomp_regex_token *> &postfix_exp) const = 0;
          /* Evaluate the postfix expression of regex tokens */
          virtual void eval_postfix(
            std::stack<bool> &eval_stack,
            int ioid, const std::string &fname,
            const std::string &vname) const = 0;
          virtual ~SDecomp_regex_token() = 0;
      };

      /* Trim spaces on the left end of a string */
      void str_ltrim(std::string::const_iterator &begin,
        const std::string::const_iterator &end);

      /* Trim spaces on the right end of a string */
      void str_rtrim(const std::string::const_iterator &begin,
        std::string::const_iterator &end);

      /* Trim spaces on the left and right ends of a string */
      void str_trim(std::string::const_iterator &begin,
        std::string::const_iterator &end);

      /* A regex operator/op token
       * Operators supported are
       * logical not : "!" in user regex
       * logical or : "||" in user regex
       * logical and : "&&" in user regex
       */
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
          /* Util function to parse a string and possibly find a 
           * regex operator token
           * returns true if a regex operator is found, the begin
           * and end of the string are modified accordingly
           * returns false if a regex operator is not found
           */
          static bool parse_and_create_token(
                  std::string::const_iterator &begin,
                  std::string::const_iterator &end,
                  std::vector<SDecomp_regex_token *> &pregex_tokens);
        private:
          /* Types of regex operator tokens */
          typedef enum SDecomp_regex_op_type_{
            INVALID_OP = 0,
            LOGICAL_AND,
            LOGICAL_OR,
            LOGICAL_NOT,
            LEFT_BRACKET,
            RIGHT_BRACKET,
            NUM_OPS
          } SDecomp_regex_op_type;

          /* Convert a regex operator string to type */
          static SDecomp_regex_op_type to_type(const std::string &str);
          /* Convert a regex op type to string */
          static std::string to_string(const SDecomp_regex_op_type &type);
          /* Comparison operators for regex operators */
          bool operator<(const SDecomp_regex_op &other) const;
          bool operator==(const SDecomp_regex_op_type &type) const;
          SDecomp_regex_op_type type_;
      };
      /* Regex item token
       * Each regex item consits of an item identifier and a regex to identify
       * the item match
       * The types of regex items supported are
       * ID type : ID="REGEX_TO_MATCH_ID"
       * FILE type : FILE="REGEX_TO_MATCH_FILE_NAME"
       * VAR type : VAR="REGEX_TO_MATCH_VARIABLE_NAME"
       * */
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
          /* Util function to parse a string and possibly find a 
           * regex item token
           * returns true if a regex item is found, the begin
           * and end of the string are modified accordingly
           * returns false if a regex item is not found
           */
          static bool parse_and_create_token(
                  std::string::const_iterator &begin,
                  std::string::const_iterator &end,
                  std::vector<SDecomp_regex_token *> &pregex_tokens);
        private:
          /* Regex token item types */
          typedef enum SDecomp_regex_item_type_{
            INVALID_REGEX = 0,
            ID_REGEX,
            VAR_REGEX,
            FILE_REGEX 
          } SDecomp_regex_item_type;
          const int INVALID_IOID = -1;

          /* Find token item identified by "item_str" that identifies
           * the token item
           * On return the tok_begin and tok_end points to the part of
           * the string that corresponds to the token
           * The begin and end are modified (to point to next regex token)
           */
          static void find_tok(
            const std::string &tok_item_str,
            const std::string::const_iterator &begin,
            const std::string::const_iterator &end,
            std::string::const_iterator &tok_begin,
            std::string::const_iterator &tok_end);
          /* Convert string identifying a token item type to item type */
          static SDecomp_regex_item_type to_type(const std::string &str);
          /* Convert token item type to string */
          static std::string to_string(const SDecomp_regex_item_type &type);
          SDecomp_regex_item_type type_;
          std::regex rgx_;
          int ioid_;
      };
  } // namespace PIO_SDecomp_Util
  /* Class that saves the user-specified regex to save I/O decompositions */
  class PIO_save_decomp_regex{
    public:
      /* Create the class from user-specified regex string */
      PIO_save_decomp_regex(const std::string &rgx);
      /* Returns true if the user-specified ioid, file and variable name matches
       * the regular expression (specified in the constructor)
       * returns false otherwise
       */
      bool matches(int ioid, const std::string &fname, const std::string &vname) const;
      ~PIO_save_decomp_regex();
    private:
      /* Tokenize user specified regex string into regex tokens, saved in
       * pregex_tokens_
       */
      void tokenize_sdecomp_regex(const std::string &rgx); 
      /* Convert regex tokens saved in pregex_tokens from infix expression to
       * postfix
       */
      void convert_to_postfix(void );
      /* Vector of pointers to regex tokens created from user regular expression */
      std::vector<PIO_SDecomp_Util::SDecomp_regex_token *> pregex_tokens_;
      /* Vector of pointers to regex tokens in postfix form, used to evaluate
       * the regular expression
       */
      std::vector<const PIO_SDecomp_Util::SDecomp_regex_token *> postfix_exp_;
  };
} // namespace PIO_Util

#endif // __PIO_SDECOMPS_REGEX_HPP__
