// todo

#ifndef HTTP_REQUEST_PARSER_H_
#define HTTP_REQUEST_PARSER_H_

#include <memory>
#include <unordered_set>

#include "tinywebserver/network/http/const.h"
#include "tinywebserver/network/http/parser.h"
#include "tinywebserver/network/http/request.h"
#include "tinywebserver/utils/buffer.h"

namespace http {

class RequestParser : public Parser {
 public:
  enum class State {
    ERROR_READ_FD,
    ERROR_REQUEST_LINE,
    ERROR_HEADER,
    ERROR_NO_EMPTY_LINE,
    ERROR_BODY_LENGTH,
    INIT,
    PARSING_REQUEST_LINE,
    PARSING_REQUEST_HEADER,
    PARSING_EMPTY_LINE,
    BEFORE_PARSING_REQUST_BODY,
    PARSING_REQUEST_BODY,
    COMPLETE,
  };

  /**
   * @brief Determine whether it is a error state
   */
  static bool is_error_state(State state) {
    std::unordered_set<State> m = {
        State::ERROR_READ_FD,     State::ERROR_REQUEST_LINE,
        State::ERROR_HEADER,      State::ERROR_NO_EMPTY_LINE,
        State::ERROR_BODY_LENGTH,
    };
    return m.count(state);
  }

  /**
   * @brief consume data from Linux file descriptor
   * @param fd socket file descriptor, should be set with O_NONBLOCK
   * @param is_et whether fd is in the edge triger mode.
   * @return The state of parser and Request object. If parsing is not complete,
   * the Requst object will be nullptr
   */
  std::pair<State, std::unique_ptr<Request>> consume_from_fd(int fd,
                                                             bool is_et = true);
  /**
   * @brief Clear the state of parser.
   */
  void clear() {
    buf_.clear();
    state_ = State::INIT;
    obj_ = nullptr;
    req_body_size_ = 0;
  }

 protected:
  /**
   * @brief Parse the http request line.
   * @return return false if something errors occur.
   */
  static bool parse_request_line(std::string_view line, Request& obj);

 protected:
  Buffer buf_;
  State state_ = State::INIT;

  std::unique_ptr<Request> obj_;

  /**
   * @brief The Content-Length in the header
   */
  size_t req_body_size_ = 0;
};

}  // namespace http

#endif