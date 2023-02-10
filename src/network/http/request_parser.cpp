// todo

#include "tinywebserver/network/http/request_parser.h"

#include "tinywebserver/utils/sv.h"

namespace http {

bool RequestParser::parse_request_line(std::string_view line, Request &obj) {
  std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
  if (svmatch sub_match;
      !std::regex_match(line.begin(), line.end(), sub_match, patten)) {
    return false;
  } else {
    obj.method_ = Request::str2Method(
        std::string(sub_match[1].first, sub_match[1].second));
    obj.uri_ = std::string(sub_match[2].first, sub_match[2].second);
    obj.version_ = std::string(sub_match[3].first, sub_match[3].second);
    if (obj.method_ == Request::Method::UNKNOWN) return false;
    return true;
  }
};

}  // namespace http