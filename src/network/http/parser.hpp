#ifndef HTTP_PARSER_H_
#define HTTP_PARSER_H_

#include <string_view>

#include "./form.hpp"
#include "./header.hpp"

namespace http {

class Parser {
 public:
  static bool parse_header(std::string_view line, Header &obj);

  static Form parse_form(std::string_view data);

 protected:
  /**
   * @brief Convert a hexadecimal char to decimal int
   * @example 'a' -> 10, 'A' -> 10, '0' -> 0
   */
  static int hex2dec(char ch);

  /**
   * @brief Parse form element
   */
  static std::string parse_form_elem(std::string_view data);
};

}  // namespace http
#endif