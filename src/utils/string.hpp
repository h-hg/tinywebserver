#ifndef UTILS_STRING_H_
#define UTILS_STRING_H_

#include <algorithm>
#include <cctype>
#include <string>

std::string& toupper(std::string& str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::toupper(c); });
}

std::string toupper(const std::string& str) {
  auto ret = str;
  return toupper(str);
}

std::string toupper(std::string&& str) { return toupper(str); }

std::string& tolower(std::string& str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::tolower(c); });
}

std::string tolower(const std::string& str) {
  auto ret = str;
  return tolower(str);
}

std::string tolower(std::string&& str) { return tolower(str); }

#endif