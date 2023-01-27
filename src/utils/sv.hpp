#ifndef SV_H_
#define SV_H_

#include <regex>
#include <string_view>
using svmatch = std::match_results<std::string_view::const_iterator>;
using svsub_match = std::sub_match<std::string_view::const_iterator>;
using svregex_iterator = std::regex_iterator<std::string_view::const_iterator>;
using svregex_token_iterator =
    std::regex_token_iterator<std::string_view::const_iterator>;

inline std::string_view ltrim(std::string_view str) {
  return {std::find_if(str.begin(), str.end(),
                       [](auto ch) { return !std::isspace(ch); }),
          str.end()};
}

inline std::string_view rtrim(std::string_view str) {
  auto riter = std::find_if(str.rbegin(), str.rend(),
                            [](auto ch) { return !std::isspace(ch); });
  return {str.begin(), riter.base()};
}

inline std::string_view trim(std::string_view str) { return rtrim(ltrim(str)); }

#endif