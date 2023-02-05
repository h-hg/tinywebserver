#include "./request.hpp"

#include "../../utils/string.hpp"
#include "./parser.hpp"

namespace http {

Request::Method Request::str2Method(const std::string &str) {
  static const std::unordered_map<std::string, Request::Method> m = {
      {"GET", Method::GET},         {"POST", Method::POST},
      {"HEAD", Method::HEAD},       {"PUT", Method::PUT},
      {"DELETE", Method::DELETE},   {"TRACE", Method::TRACE},
      {"CONNECT", Method::CONNECT},
  };
  auto s = toupper(str);
  if (auto it = m.find(s); it != m.end())
    return it->second;
  else
    return Method::UNKNOWN;
}

Form Request::parse_form() const {
  if (auto it = header_.find("Content-Type");
      it == header_.end() || it->second != "application/x-www-form-urlencoded")
    return {};
  else if (method_ == Method::POST) {
    if (body_.size() == 0) return {};
    return Parser::parse_form(std::string_view(body_.begin(), body_.end()));
  } else if (method_ == Method::GET) {
    auto pos = uri_.find_last_of('?');
    if (pos == std::string::npos) return {};
    return Parser::parse_form(
        std::string_view(uri_.begin() + pos + 1, uri_.end()));
  }
  return {};
}

bool Request::is_keepalive() const {
  if (auto it = header_.find("Connnection"); it == header_.end())
    return false;
  else if (it->second == "keep-alive" && version_ == "1.1")
    return true;
  return false;
}

}  // namespace http