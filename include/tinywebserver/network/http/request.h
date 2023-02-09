#ifndef HTTP_REQUEST_
#define HTTP_REQUEST_

#include <string>
#include <vector>

#include "tinywebserver/network/http/form.h"
#include "tinywebserver/network/http/header.h"

namespace http {

class RequestParser;

class Request {
  friend RequestParser;

 public:
  Request() = default;
  enum class Method {
    UNKNOWN,
    GET,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    CONNECT,
  };

  static Request::Method str2Method(const std::string& str);

  Method method() const { return method_; }
  void set_method(Method method) { method_ = method; }

  std::string protocol() const { return protocol_; }

  std::string version() const { return version_; }
  void set_version(const std::string& version) { version_ = version; }

  std::string uri() const { return uri_; }
  void set_uri(const std::string& uri) { uri_ = uri; }

  Header& header() { return header_; }
  const Header& header() const { return header_; }

  auto& body() { return body_; }
  const auto& body() const { return body_; }

  bool is_keepalive() const;

  Form parse_form() const;

 protected:
  Method method_ = Method::UNKNOWN;
  std::string uri_;
  std::string protocol_;
  std::string version_;
  Header header_;
  std::vector<char> body_;
};

}  // namespace http

#endif
