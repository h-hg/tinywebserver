// todo
#include "tinywebserver/network/http/request_parser.h"

#include <unistd.h>

#include "tinywebserver/network/http/const.h"
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

std::pair<RequestParser::State, std::unique_ptr<Request>>
RequestParser::consume_from_fd(int fd, bool is_et) {
  // read data from file descriptor
  ssize_t total_read = 0;
  do {
    ssize_t n = 1024 * 5;
    buf_.ensure_writeable(n);
    int readn = ::read(fd, buf_.cur_write_ptr(), n);
    if (readn <= 0) {
      break;
    } else {
      buf_.update_write_ptr(readn);
      total_read += readn;
    }
  } while (is_et);

  if (total_read <= 0 && errno != EAGAIN) {
    return {State::ERROR_READ_FD, nullptr};
  }

  // parse http request from buf_
  for (bool stop = false; !stop;) {
    auto content = buf_.view();
    switch (state_) {
      case State::INIT: {
        if (obj_ == nullptr)
          obj_ = std::make_unique<Request>();
        else
          obj_->clear();
        // update the state_
        state_ = State::PARSING_REQUEST_LINE;
        break;
      }
      case State::PARSING_REQUEST_LINE: {
        // read line
        auto pos = content.find(CRLF);
        if (pos == std::string_view::npos) {
          return {state_, nullptr};
        }
        auto line = content.substr(0, pos);
        buf_.update_read_ptr(line.size() + 2);
        // parse request line
        if (!parse_request_line(line, *obj_)) {
          state_ = State::ERROR_REQUEST_LINE;
          return {state_, nullptr};
        }
        state_ = State::PARSING_REQUEST_HEADER;
        break;
      }
      case State::PARSING_REQUEST_HEADER: {
        // read line
        auto pos = content.find(CRLF);
        if (pos == std::string_view::npos) {
          return {state_, nullptr};
        }
        auto line = content.substr(0, pos);
        buf_.update_read_ptr(line.size() + 2);

        // parse header
        if (line.empty()) {
          // finish parsing empty line
          state_ = State::BEFORE_PARSING_REQUST_BODY;
          // set the request body size
          break;
        }
        if (!parse_header(line, obj_->header())) {
          state_ = State::ERROR_HEADER;
          return {state_, nullptr};
        }
        break;
      }
      case State::BEFORE_PARSING_REQUST_BODY: {
        auto &header = obj_->header();
        auto it = header.find(Header::CONTENT_LENGTH);
        if (it == header.end()) {
          state_ = State::ERROR_BODY_LENGTH;
          return {state_, nullptr};
        }
        req_body_size_ = std::stoul(it->second);
        state_ = State::PARSING_REQUEST_BODY;
        break;
      }
      // case State::PARSING_EMPTY_LINE:
      //  break;
      case State::PARSING_REQUEST_BODY: {
        auto &body = obj_->body();

        // read data to request body
        size_t readn =
            std::min(req_body_size_ - body.size(), buf_.readable_size());
        body.insert(body.end(), buf_.cur_read_ptr(),
                    buf_.cur_read_ptr() + readn);
        buf_.update_read_ptr(readn);

        if (body.size() < req_body_size_) break;
        if (body.size() == req_body_size_) {
          if (buf_.readable_size()) {
            state_ = State::ERROR_BODY_LENGTH;
            return {state_, nullptr};
          }
          state_ = State::COMPLETE;
          break;
        }
      }
      case State::COMPLETE: {
        state_ = State::INIT;
        return {State::COMPLETE, std::move(obj_)};
      }
      default:
        return {state_, nullptr};
    }
  }
  return {state_, state_ == State::COMPLETE ? std::move(obj_) : nullptr};
}

}  // namespace http