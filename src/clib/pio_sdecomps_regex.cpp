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
#include "pio_sdecomps_regex.hpp"

namespace PIO_Util{
  namespace PIO_SDecomp_Util{

      SDecomp_regex_token::~SDecomp_regex_token()
      {
      }

      void str_ltrim(std::string::const_iterator &begin,
        const std::string::const_iterator &end)
      {
        /* Find the first non-space character from the beginning and
         * set "begin" to point to that value
         */
        begin = std::find_if(begin, end, [](int ch) { return !std::isspace(ch); });
      }

      void str_rtrim(const std::string::const_iterator &begin,
        std::string::const_iterator &end)
      {
        /* Traverse from the end using a reverse iterator and find
         * the first non-space character, convert the reverse iterator
         * back to the forward iterator and set "end" to point to that
         * iterator
         */
        end = std::find_if( std::reverse_iterator<std::string::const_iterator>(end),
                            std::reverse_iterator<std::string::const_iterator>(begin),
                            [](int ch) { return !std::isspace(ch); } ).base();
      }

      void str_trim(std::string::const_iterator &begin,
        std::string::const_iterator &end)
      {
        str_ltrim(begin, end);
        str_rtrim(begin, end);
      }

      SDecomp_regex_op::SDecomp_regex_op(const std::string &str):type_(INVALID_OP)
      {
        std::string::const_iterator begin = str.cbegin();
        std::string::const_iterator end = str.cend();

        str_trim(begin, end);
        std::string op_str(begin, end);
        type_ = to_type(op_str);
      }

      /* Add current operator to postfix_exp based on information in the
       * tok_stack, the stack of postfix operators
       */
      void SDecomp_regex_op::convert_to_postfix(
              std::stack<const SDecomp_regex_op *> &tok_stack,
              std::vector<const SDecomp_regex_token *> &postfix_exp) const
      {
        const SDecomp_regex_op *tok = this;
        if(*tok == LEFT_BRACKET){
          tok_stack.push(tok);
        }
        else if(*tok == RIGHT_BRACKET){
          assert(!tok_stack.empty());
          while(!tok_stack.empty()){
            const SDecomp_regex_op *top_elem = tok_stack.top();
            if(*top_elem == LEFT_BRACKET){
              tok_stack.pop();
              break;
            }
            else{
              postfix_exp.push_back(top_elem);
              tok_stack.pop();
            }
          }
        }
        else{
          while(!tok_stack.empty()){
            const SDecomp_regex_op *top_elem = tok_stack.top();
            if(*tok < *top_elem){
              postfix_exp.push_back(top_elem);
              tok_stack.pop();
            }
            else{
              break;
            }
          }
          tok_stack.push(tok);
        }
      }

      /* Evaluate the current operator using values in the eval
       * stack and user-specified ioid, file/variable names
       */
      void SDecomp_regex_op::eval_postfix(
            std::stack<bool> &eval_stack,
            int ioid, const std::string &fname,
            const std::string &vname) const
      {
        const SDecomp_regex_op *tok = this;
        if(*tok == LOGICAL_NOT){
          assert(!eval_stack.empty());
          bool val = eval_stack.top();
          eval_stack.pop();
          eval_stack.push(!val);
        }
        else if(*tok == LOGICAL_OR){
          assert(!eval_stack.empty());
          bool val1 = eval_stack.top();
          eval_stack.pop();
          assert(!eval_stack.empty());
          bool val2 = eval_stack.top();
          eval_stack.pop();
          eval_stack.push(val1 || val2);
        }
        else if(*tok == LOGICAL_AND){
          assert(!eval_stack.empty());
          bool val1 = eval_stack.top();
          eval_stack.pop();
          assert(!eval_stack.empty());
          bool val2 = eval_stack.top();
          eval_stack.pop();
          eval_stack.push(val1 && val2);
        }
      }

      bool SDecomp_regex_op::parse_and_create_token(
                    std::string::const_iterator &begin,
                    std::string::const_iterator &end,
                    std::vector<SDecomp_regex_token *> &pregex_tokens)
      {
        bool found_token = false;
        std::string::const_iterator iter = begin;

        /* Trim any spaces at the beginning and end */
        str_trim(iter, end);

        std::string tok;
        if(iter != end){
          /* Check for single character ops */
          std::string sc_op(iter, iter+1);
          if(sc_op == to_string(RIGHT_BRACKET)){
            found_token = true;
            tok = sc_op;
            ++iter;
          }
          else if(sc_op == to_string(LEFT_BRACKET)){
            found_token = true;
            tok = sc_op;
            ++iter;
          }
          else if(sc_op == to_string(LOGICAL_NOT)){
            found_token = true;
            tok = sc_op;
            ++iter;
          }
          else{
            /* Check for double character ops */
            if((iter + 1) != end){
              std::string dc_op(iter, iter+2);
              if(dc_op == to_string(LOGICAL_OR)){
                found_token = true;
                tok = dc_op;
                ++iter;
                ++iter;
              }
              else if(dc_op == to_string(LOGICAL_AND)){
                found_token = true;
                tok = dc_op;
                ++iter;
                ++iter;
              }
            }
          }
        }

        if(found_token){
          assert(tok.size() > 0);
          pregex_tokens.push_back(new SDecomp_regex_op(tok));
        }

        begin = iter;
        return found_token;
      }

      SDecomp_regex_op::SDecomp_regex_op_type 
        SDecomp_regex_op::to_type(const std::string &str)
      {
        std::string::const_iterator begin = str.cbegin();
        std::string::const_iterator end = str.cend();

        str_trim(begin, end);
        std::string op_str(begin, end);
        if(op_str == to_string(LOGICAL_AND)){
          return LOGICAL_AND;
        }
        else if(op_str == to_string(LOGICAL_OR)){
          return LOGICAL_OR;
        }
        else if(op_str == to_string(LOGICAL_NOT)){
          return LOGICAL_NOT;
        }
        else if(op_str == to_string(LEFT_BRACKET)){
          return LEFT_BRACKET;
        }
        else if(op_str == to_string(RIGHT_BRACKET)){
          return RIGHT_BRACKET;
        }
        else{
          /* FIXME: Use PIO exceptions */
          std::string error_str = std::string("Error creating regex op from string : ")
                                    + str;
          throw std::runtime_error(error_str);
        }
      }

      std::string SDecomp_regex_op::to_string(
        const SDecomp_regex_op::SDecomp_regex_op_type &type)
      {
        if(type == LOGICAL_AND){
          return std::string("&&");
        }
        else if(type == LOGICAL_OR){
          return std::string("||");
        }
        else if(type == LOGICAL_NOT){
          return std::string("!");
        }
        else if(type == LEFT_BRACKET){
          return std::string("(");
        }
        else if(type == RIGHT_BRACKET){
          return std::string(")");
        }
        else{
          assert(0);
        }
      }

      std::string SDecomp_regex_op::to_string(void ) const
      {
        return to_string(type_);
      }

      bool SDecomp_regex_op::operator<(const SDecomp_regex_op &other) const
      {
        static const int INVALID_OP_PRIORITY = 0;
        static const int LOGICAL_AND_PRIORITY = 1;
        static const int LOGICAL_OR_PRIORITY = 1;
        static const int LOGICAL_NOT_PRIORITY = 2;
        static const int LEFT_BRACKET_PRIORITY = 3;
        static const int RIGHT_BRACKET_PRIORITY = 3;
        static const int OP_PRIORITY[NUM_OPS] = { INVALID_OP_PRIORITY,
                                                  LOGICAL_AND_PRIORITY,
                                                  LOGICAL_OR_PRIORITY,
                                                  LOGICAL_NOT_PRIORITY,
                                                  LEFT_BRACKET_PRIORITY,
                                                  RIGHT_BRACKET_PRIORITY};
        return (OP_PRIORITY[type_] < OP_PRIORITY[other.type_]);
      }

      bool SDecomp_regex_op::operator==(
        const SDecomp_regex_op::SDecomp_regex_op_type &type) const
      {
        return (type_ == type);
      }

      SDecomp_regex_item::SDecomp_regex_item(const std::string &str):type_(INVALID_REGEX), ioid_(INVALID_IOID)
      {
        /* The strings for creating items are in the following form
         * REGEX_TYPE=REGEX_STRING
         * e.g. ID="101", ID="1.*", VAR="T.*", FILE=".*cam.*"
         */
        std::regex item_rgx("[[:space:]]*(.+)[[:space:]]*=[[:space:]]*[\"](.+)[\"][[:space:]]*");
        std::smatch match;
        if(std::regex_search(str, match, item_rgx) && (match.size() == 3)){
          std::string item_type_str = match.str(1);
          std::string item_rgx = match.str(2);
          type_ = to_type(item_type_str);
          rgx_.assign(item_rgx.c_str());
        }
        else{
          /* FIXME: Use PIO exceptions */
          std::string error_str = std::string("Error parsing item string : ") + str;
          throw std::runtime_error(error_str);
        }
      }

      void SDecomp_regex_item::convert_to_postfix(
              std::stack<const SDecomp_regex_op *> &tok_stack,
              std::vector<const SDecomp_regex_token *> &postfix_exp) const
      {
        postfix_exp.push_back(this);
      }

      void SDecomp_regex_item::eval_postfix(
            std::stack<bool> &eval_stack,
            int ioid, const std::string &fname,
            const std::string &vname) const
      {
        if(type_ == ID_REGEX){
          eval_stack.push(std::regex_match(std::to_string(ioid), rgx_));
        }
        else if(type_ == FILE_REGEX){
          eval_stack.push(std::regex_match(fname, rgx_));
        }
        else if(type_ == VAR_REGEX){
          eval_stack.push(std::regex_match(vname, rgx_));
        }
      }

      bool SDecomp_regex_item::parse_and_create_token(
                    std::string::const_iterator &begin,
                    std::string::const_iterator &end,
                    std::vector<SDecomp_regex_token *> &pregex_tokens)
      {
        bool found_token = false;
        const std::string EQUALS("=");

        /* Trim spaces at the beginning of the string */
        str_ltrim(begin, end);

        std::size_t dist = std::distance(begin, end);
        std::string id_str = to_string(ID_REGEX);
        std::string var_str = to_string(VAR_REGEX);
        std::string file_str = to_string(FILE_REGEX);

        std::string tok_item_str;
        std::string::const_iterator tok_begin = begin;
        std::string::const_iterator tok_end = begin;

        /* Check if the regex is for an ID */
        if(id_str.size() <= dist){
          std::string tmp_str(begin, begin+id_str.size());
          if(tmp_str == id_str){
            tok_item_str = id_str;
            found_token = true;
          }
        }

        /* If token not found, check if the regex is for VAR */
        if(!found_token && (var_str.size() <= dist)){
          std::string tmp_str(begin, begin+var_str.size());
          if(tmp_str == var_str){
            tok_item_str = var_str;
            found_token = true;
          }
        }

        /* If token not found, check if the regex is for FILE */
        if(!found_token && (file_str.size() <= dist)){
          std::string tmp_str(begin, begin+file_str.size());
          if(tmp_str == file_str){
            tok_item_str = file_str;
            found_token = true;
          }
        }

        if(found_token){
          find_tok(tok_item_str, begin, end, tok_begin, tok_end);
          pregex_tokens.push_back(
            new SDecomp_regex_item(std::string(tok_begin, tok_end)) );
        }

        begin = tok_end;
        return found_token;
      }

      void SDecomp_regex_item::find_tok(
            const std::string &tok_item_str,
            const std::string::const_iterator &begin,
            const std::string::const_iterator &end,
            std::string::const_iterator &tok_begin,
            std::string::const_iterator &tok_end)
      {
        const std::string EQUALS("=");
        const std::string DOUBLE_QUOTE("\"");
        const std::string ESCAPE("\\");
        const std::string REGEX_CHAR_GROUP_BEGIN("[");
        const std::string REGEX_CHAR_GROUP_END("]");

        tok_begin = begin;
        tok_end = tok_begin;
        str_ltrim(tok_end, end);

        /* Strings are of form <ID String> = "<ID Reg expression>"
         * ID="101", ID="10.*"
         * VAR="T" , VAR="T.*"
         * FILE=".*cam.*"
         */

        /* ID string */
        assert(std::distance(begin, end) >= 0);
        assert(static_cast<size_t>(std::abs(std::distance(begin, end))) >= tok_item_str.size());
        tok_end += tok_item_str.size();
        str_ltrim(tok_end, end);

        /* Equals */
        assert(tok_end != end);
        assert(std::string(tok_end, tok_end+1) == EQUALS);
        ++tok_end;
        str_ltrim(tok_end, end);

        /* Starting double qupte of reg expression */
        assert(tok_end != end);
        assert(std::string(tok_end, tok_end+1) == DOUBLE_QUOTE);
        ++tok_end;

        /* Find the ending double quote of reg expression */
        bool found_token = false;
        bool escape_next_ch = false;
        bool in_regex_char_group = false;

        /* Note: we need to ignore all chars within a char group
         * expression or that are explicitly escaped
         */
        while(tok_end != end){
          std::string next_ch(tok_end, tok_end+1);
          if(escape_next_ch){
            ++tok_end;
            escape_next_ch = false;
          }
          else if(in_regex_char_group){
            if(next_ch == REGEX_CHAR_GROUP_END){
              in_regex_char_group = false;
            }
            ++tok_end;
          }
          else if(next_ch == ESCAPE){
            ++tok_end;
            escape_next_ch = true;
          }
          else if(next_ch == REGEX_CHAR_GROUP_BEGIN){
            ++tok_end;
            in_regex_char_group = true;
          }
          else if(next_ch == DOUBLE_QUOTE){
            ++tok_end;
            found_token = true;
            break;
          }
          else{
            ++tok_end;
          }
        }

        assert(found_token);
      }

      SDecomp_regex_item::SDecomp_regex_item_type
        SDecomp_regex_item::to_type(const std::string &str)
      {
        std::string::const_iterator begin = str.cbegin();
        std::string::const_iterator end = str.cend();

        str_trim(begin, end);
        std::string item_str(begin, end);
        SDecomp_regex_item::SDecomp_regex_item_type
          rgx_item_type = INVALID_REGEX;
        if(item_str == to_string(ID_REGEX)){
          rgx_item_type = ID_REGEX;
        }
        else if(item_str == to_string(VAR_REGEX)){
          rgx_item_type = VAR_REGEX;
        }
        else if(item_str == to_string(FILE_REGEX)){
          rgx_item_type = FILE_REGEX;
        }
        else{
          assert(0);
        }
        return rgx_item_type;
      }

      std::string SDecomp_regex_item::to_string(
        const SDecomp_regex_item::SDecomp_regex_item_type &type)
      {
        std::string rgx_item("INVALID");
        if(type == ID_REGEX){
          rgx_item = "ID";
        }
        else if(type == VAR_REGEX){
          rgx_item = "VAR";
        }
        else if(type == FILE_REGEX){
          rgx_item = "FILE";
        }
        else{
          assert(0);
        }
        return rgx_item;
      }

      std::string SDecomp_regex_item::to_string(void ) const
      {
        return to_string(type_);
      }

  } // namespace PIO_SDecomp_Util

  PIO_save_decomp_regex::PIO_save_decomp_regex(const std::string &str)
  {
    std::regex match_all_rgx("^[[:space:]]*[*][[:space:]]*$");
    std::smatch match;
    if(!std::regex_search(str, match, match_all_rgx)){
      /* Tokenize regex string into a list of SDecomp_regex_items and ops
       * that can be used for future evaluations (match variable and file
       * names)
       */
      tokenize_sdecomp_regex(str);
      /* Convert the tokenized string to a postfix expression
       * The postfix expression is saved for evaluating the 
       * regex
       */
      convert_to_postfix();
    }
    else{
      /* Do nothing
       * an empty container of SDecomo_regex_items implies "matches all"
       */
    }
  }

  bool PIO_save_decomp_regex::matches(int ioid, const std::string &fname,
        const std::string &vname) const
  {
    std::stack<bool> eval_stack;
    for(std::vector<const PIO_SDecomp_Util::SDecomp_regex_token *>::const_iterator citer=
          postfix_exp_.cbegin(); citer != postfix_exp_.cend(); ++citer){
      (*citer)->eval_postfix(eval_stack, ioid, fname, vname);
    }
    if(eval_stack.empty()){
      return true;
    }
    else{
      assert(eval_stack.size() == 1);
      return eval_stack.top();
    }
  }

  PIO_save_decomp_regex::~PIO_save_decomp_regex()
  {
    for(std::vector<PIO_SDecomp_Util::SDecomp_regex_token *>::iterator iter =
            pregex_tokens_.begin();
          iter != pregex_tokens_.end(); ++iter){
      delete(*iter);
    }
  }

  void PIO_save_decomp_regex::tokenize_sdecomp_regex(const std::string &str)
  {
    std::string::const_iterator begin = str.cbegin();
    std::string::const_iterator end = str.cend();
    while(begin != end){
      bool parsed_next_token = false;

      /* First try to parse string for an op */
      parsed_next_token =
        PIO_SDecomp_Util::SDecomp_regex_op::parse_and_create_token(begin, end,
          pregex_tokens_);

      if(!parsed_next_token){
        /* Next try to parse string for a regex item */
        parsed_next_token =
          PIO_SDecomp_Util::SDecomp_regex_item::parse_and_create_token(begin, end,
            pregex_tokens_);
      }

      /* Make sure that we parsed at least one token - we are making progress */
      assert(parsed_next_token);
    }
    /*
    for(std::vector<PIO_SDecomp_Util::SDecomp_regex_token *>::const_iterator citer =
          pregex_tokens_.cbegin(); citer != pregex_tokens_.cend(); ++citer){
      std::cout << (*citer)->to_string().c_str() << " ";
    }
    std::cout << "\n";
    */
  }

  void PIO_save_decomp_regex::convert_to_postfix(void )
  {
    std::stack<const PIO_SDecomp_Util::SDecomp_regex_op *> tok_stack;
    for(std::vector<PIO_SDecomp_Util::SDecomp_regex_token *>::const_iterator citer=
          pregex_tokens_.cbegin(); citer != pregex_tokens_.cend(); ++citer){
      (*citer)->convert_to_postfix(tok_stack, postfix_exp_);
    }
    while(!tok_stack.empty()){
      postfix_exp_.push_back(tok_stack.top());
      tok_stack.pop();
    }
    /*
    for(std::vector<const PIO_SDecomp_Util::SDecomp_regex_token *>::const_iterator citer =
          postfix_exp_.cbegin(); citer != postfix_exp_.cend(); ++citer){
      std::cout << (*citer)->to_string().c_str() << " ";
    }
    std::cout << "\n";
    */
  }
} // namespace PIO_Util

static PIO_Util::PIO_save_decomp_regex pio_sdecomp_regex(PIO_SAVE_DECOMPS_REGEX);

bool pio_save_decomps_regex_match(int ioid, const char *fname, const char *vname)
{
  bool ioid_is_valid = (ioid >= 0) ? true : false;
  if(!ioid_is_valid && !fname && !vname){
    return false;
  }
  std::string fname_str((fname) ? fname : "");
  std::string vname_str((vname) ? vname : "");
  return pio_sdecomp_regex.matches(ioid, fname_str, vname_str);
}
