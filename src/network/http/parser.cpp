
#include "tinywebserver/network/http/parser.h"

#include <regex>
#include <string>

#include "tinywebserver/utils/sv.h"

namespace http {

int Parser::hex2dec(char ch) {
  if ('A' <= ch && ch <= 'F')
    return ch - 'A' + 10;
  else if ('a' <= ch && ch <= 'f')
    return ch - 'a' + 10;
  else
    return ch - '0';
}

std::string Parser::parse_form_elem(std::string_view data) {
  std::string ret;
  for (int i = 0; i < data.size();) {
    if (data[i] == '%') {
      ret.push_back(hex2dec(data[i + 1]) * 16 + hex2dec(data[i + 2]));
      // skip the hexadecimal char
      i += 3;
    } else if (data[i] == '+') {
      ret.push_back(' ');
      ++i;
    } else {
      ret.push_back(data[i++]);
    }
  }
  return ret;
}

Form Parser::parse_form(std::string_view data) {
  /**
   * exmaple:
   *  - origin:
   *    - key1=a b+\
   *    - key2=cc
   *  - send: key1=a+b%5C%3D&key2=cc
   *    - %5C = 0x5C = 92 = '\'
   *    - %3D = 0f3D = 61 = '='
   */
  Form ret;
  for (std::string_view line; getline(data, line, '&'), !data.empty();) {
    auto pos = line.find('=');
    if (pos == std::string_view::npos) return {};
    ret.emplace(parse_form_elem(line.substr(0, pos)),
                parse_form_elem(line.substr(pos + 1)));
  }
  return ret;
}

bool Parser::parse_header(std::string_view line, Header &obj) {
  std::regex pattern("^([^:]*): ?(.*)$");
  if (svmatch sub_match;
      !std::regex_match(line.begin(), line.end(), sub_match, pattern)) {
    return false;
  } else {
    obj.emplace(std::string(sub_match[1].first, sub_match[1].second),
                std::string(sub_match[2].first, sub_match[2].second));
    return true;
  }
}

}  // namespace http