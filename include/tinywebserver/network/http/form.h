#ifndef HTTP_FORM_H_
#define HTTP_FORM_H_

#include <string>
#include <string_view>
#include <unordered_map>

namespace http {

class Form : public std::unordered_map<std::string, std::string> {};

}  // namespace http

#endif