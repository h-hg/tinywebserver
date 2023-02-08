#ifndef UTILS_STRING_H_
#define UTILS_STRING_H_

#include <algorithm>
#include <cctype>
#include <string>

inline std::string& toupper(std::string& str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return str;
}

inline std::string toupper(const std::string& str) {
  auto ret = str;
  return toupper(str);
}

inline std::string toupper(std::string&& str) {
  auto ret = std::move(str);
  return toupper(ret);
}

inline std::string& tolower(std::string& str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return str;
}

inline std::string tolower(const std::string& str) {
  auto ret = str;
  return tolower(str);
}

inline std::string tolower(std::string&& str) {
  auto ret = std::move(str);
  return tolower(ret);
}

#endif