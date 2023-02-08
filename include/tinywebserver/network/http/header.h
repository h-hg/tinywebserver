#ifndef HTTP_HEADER_H_
#define HTTP_HEADER_H_

#include <string>
#include <unordered_map>

namespace http {

class Header : public std::unordered_map<std::string, std::string> {
 public:
  inline static const std::string HOST = "Host";
  inline static const std::string CONTENT_LENGTH = "Content-Length";
  inline static const std::string ACCEPT_ENCODING = "Accept-Encoding";
  inline static const std::string CONNECTION = "Connection";
  inline static const std::string TRANSFER_ENCODING = "Transfer-Encoding";
};

}  // namespace http

#endif