#ifndef HTTP_RESPONSE_H_
#define HTTP_RESPONSE_H_

#include <string>
#include <vector>

#include "tinywebserver/network/http/header.h"

namespace http {

class Response {
 public:
  /**
   * @ref https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
   */
  enum StatusCode {
    INVALID_CODE = 0,
    // Informational responses
    CONTINUE = 100,
    SWITCHING_PROTOCOLS = 101,
    PROCESSING = 102,  // WebDAV
    EARLY_HINTS = 103,
    // Successful responses (
    OK = 200,
    CREATED = 201,
    ACCEPTED = 202,
    NON_AUTHORITATIVE_INFORMATION = 203,
    NO_CONTENT = 204,
    RESET_CONTENT = 205,
    PARTIAL_CONTENT = 206,
    MULTI_STATUS = 207,      // WebDAV
    ALREADY_REPORTED = 208,  // WebDAV
    IM_USED = 226,           // HTTP Delta encoding
    // Redirection messages
    MULTIPLE_CHOICES = 300,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    SEE_OTHER = 303,
    NOT_MODIFIED = 304,
    USE_PROXY = 305,
    TEMPORARY_REDIRECT = 307,
    PERMANENT_REDIRECT = 308,
    // Client error responses
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    PAYMENT_REQUIRED = 402,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    NOT_ACCEPTABLE = 406,
    PROXY_AUTHENTICATION_REQUIRED = 407,
    REQUEST_TIMEOUT = 408,
    CONFLICT = 409,
    GONE = 410,
    LENGTH_REQUIRED = 411,
    PRECONDITION_FAILED = 412,
    REQUEST_ENTITY_TOO_LARGE = 413,
    URI_TOO_LONG = 414,
    UNSUPPORTED_MEDIA_TYPE = 415,
    REQUESTED_RANGE_NOT_SATISFIABLE = 416,
    EXPECTATION_FAILED = 417,
    IM_A_TEAPOT = 418,
    Misdirected_Request = 421,
    UNPROCESSABLE_ENTITY = 422,  // WebDAV
    LOCKED = 423,                // WebDAV
    FAILED_DEPENDENCY = 424,     // WebDAV
    TOO_EARLY = 425,
    UPGRADE_REQUIRED = 426,
    PRECONDITION_REQUIRED = 428,
    TOO_MANY_REQUESTS = 429,
    REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
    UNAVAILABLE_FOR_LEGAL_REASONS = 451,
    // Server error responses
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503,
    GATEWAY_TIMEOUT = 504,
    VERSION_NOT_SUPPORTED = 505,
    VARIANT_ALSO_NEGOTIATES = 506,
    INSUFFICIENT_STORAGE = 507,  // WebDAV
    LOOP_DETECTED = 508,         // WebDAV
    NOT_EXTENDED = 510,
    NETWORK_AUTHENTICATION_REQUIRED = 511,
  };

  static const unordered_map<int, std::string> CodeToStatus{
      {StatusCode::OK : "OK"},
      {StatusCode::BAD_REQUEST : "BAD_REQUEST"},
      {StatusCode::FORBIDDEN : "FORBIDDEN"},
      {StatusCode::NOT_FOUND : "NOT_FOUND"}};

  std::string version() { return version_; }
  void set_version(const std::string& version) { version_ = version; }

  int status() { return status_; }
  void set_status(int status) { status_ = status; }

  std::string desc() { return desc_; }
  void set_desc(const std::string& desc) { desc_ = desc; }

  Header& header() { return header_; }
  const Header& header() const { return header_; }

  auto& body() { return body_; };
  const auto& body() const { return body_; }

 protected:
  std::string version_;
  int status_ = StatusCode::INVALID_CODE;
  std::string desc_;
  Header header_;
  std::vector<char> body_;
};

}  // namespace http

#endif