#ifndef HTTP_HEADER_H_
#define HTTP_HEADER_H_

#include <string>
#include <unordered_map>

namespace http {

class Header : public std::unordered_map<std::string, std::string> {
 public:
  static const std::string HOST;
  static const std::string CONTENT_LENGTH;
  static const std::string ACCEPT_ENCODING;
  static const std::string CONNECTION;
  static const std::string TRANSFER_ENCODING;
};  // namespace http

}  // namespace http

#endif