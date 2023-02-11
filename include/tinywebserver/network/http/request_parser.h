// todo

#ifndef HTTP_REQUEST_PARSER_H_
#define HTTP_REQUEST_PARSER_H_

#include <unordered_set>

#include "tinywebserver/network/http/const.h"
#include "tinywebserver/network/http/parser.h"
#include "tinywebserver/network/http/request.h"
#include "tinywebserver/utils/buffer.h"

namespace http {

class RequestParser : public Parser {
 public:
  enum class State {
    ERROR_REQUEST_LINE,
    ERROR_HEADER,
    ERROR_NO_EMPTY_LINE,
    EROROR_BODY_LENGTH,
    INIT,
    PARSERING_REQUEST_LINE,
    PARSERING_REQUEST_HEADER,
    PARSERING_EMPTY_LINE,
    PARSERING_REQUEST_BODY,
    COMPLETE,
  };

  /**
   * @brief Determine whether it is a error state
   */
  bool is_error_state(State state) {
    std::unordered_set<State> m = {};
    return m.count(state);
  }

  /**
   * @brief Parse the http request line.
   * @return return false if something errors occur.
   */
  static bool parse_request_line(std::string_view line, Request &obj);

  /**
   * @brief consume data from Linux file descriptor
   * @param fd socket file descriptor
   * @param is_et whether fd is in the edge triger mode
   */
  State consume_from_fd(int fd, bool is_et = true) {}

  /**
   * @brief Clear the state of parser.
   */
  void clear() {
    buf_.clear();
    state_ = State::INIT;
  }

 protected:
  Buffer buf_;
  State state_ = State::INIT;
};

}  // namespace http

#endif