#ifndef HTTP_HANDLER_H_
#define HTTP_HANDLER_H_

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "tinywebserver/network/http/request.h"
#include "tinywebserver/network/http/response_writer.h"

namespace http {

using HTTPHandler = std::function<void(ResponseWriter &, const Request &)>;

/**
 * @ref ServeMux in net/http/server.go
 */
class HandlerManager {
 public:
  bool handle(const std::string &pattern, HTTPHandler &&handler);

  /**
   * @brief Get the HTTP handler by given pattern. There'is no need to delete
   * return pointer.
   * @return return nullptr when no handler is mathed.
   */
  HTTPHandler *match(const std::string &pattern) const;

 protected:
  /**
   * @brief pattern to http handler
   */
  std::unordered_map<std::string, std::unique_ptr<HTTPHandler>>
      pattern2handler_;

  /**
   * @brief Storing the HTTP handler whose pattern ends with '/', and it is
   * sorted by length of pattern from longest to shortest.
   */
  std::vector<std::pair<std::string, HTTPHandler *>> handlers_;
};

}  // namespace http

#endif
